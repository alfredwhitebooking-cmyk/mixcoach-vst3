#include "TrackFeedCore.h"
#include "../../Common/types/LogHelper.h"
#include <algorithm>
#include <cmath>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  updateTrackState — Punto de entrada principal desde background worker
//  Recibe TrackAudioResult + SlotInfo + TrackRole y actualiza el estado
//  unificado de la pista, generando eventos si hay cambios significativos.
// ═══════════════════════════════════════════════════════════════════════════
void TrackFeedCore::updateTrackState(int slotIndex,
                                      const TrackAudioResult& result,
                                      const SlotInfo& info,
                                      TrackRole role)
{
    if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots)
        return;

    const juce::ScopedLock lock(lock_);

    // ─── 1. Leer estado ANTERIOR (para detectar cambios) ──────────────────
    TrackState prev = states_[slotIndex];

    // ─── 2. Construir estado NUEVO ────────────────────────────────────────
    TrackState current;
    current.slotIndex   = slotIndex;
    current.trackName   = juce::String(info.trackName).trim();
    current.colour      = info.colour;
    current.bus         = info.bus;
    current.role        = role;
    current.active      = info.active;

    // Audio raw
    current.peakLeft    = result.peakLeft;
    current.peakRight   = result.peakRight;
    current.rmsLeft     = result.rmsLeft;
    current.rmsRight    = result.rmsRight;
    current.correlation = result.correlation;
    current.timestampUs = result.timestampUs;

    // Espectro
    for (int b = 0; b < 30; ++b)
        current.bandEnergies[b] = result.bandEnergies[b];
    for (int b = 0; b < 6; ++b) {
        current.crestPerBand[b]        = result.crestPerBand[b];
        current.stereoWidthPerBand[b]  = result.stereoWidthPerBand[b];
        current.midEnergyPerBand[b]    = result.midEnergyPerBand[b];
        current.sideEnergyPerBand[b]   = result.sideEnergyPerBand[b];
    }
    current.transientRatio = result.transientRatio;
    current.attackTimeMs   = result.attackTimeMs;
    current.releaseTimeMs  = result.releaseTimeMs;
    current.sustainLevelDb = result.sustainLevelDb;

    // Derivado
    current.peakCombined   = result.getPeakCombined();
    current.rmsCombined    = result.getRmsCombined();
    current.crestFactor    = (current.rmsCombined > -80.0f)
                             ? current.peakCombined - current.rmsCombined : 0.0f;
    {
        float wSum = 0.0f;
        for (int b = 0; b < 6; ++b)
            wSum += current.stereoWidthPerBand[b];
        current.avgStereoWidth = wSum / 6.0f;
    }

    // Transicional (desde estado anterior)
    current.wasClipping  = prev.wasClipping;
    current.wasLowSignal = prev.wasLowSignal;
    current.prevCrest    = prev.crestFactor;
    current.prevPeak     = prev.peakCombined;

    // Cooldowns (preservar del estado anterior)
    current.lastClipWarningUs      = prev.lastClipWarningUs;
    current.lastLowSignalUs        = prev.lastLowSignalUs;
    current.lastCrestWarningUs     = prev.lastCrestWarningUs;
    current.lastPhaseWarningUs     = prev.lastPhaseWarningUs;
    current.lastSpectralWarningUs  = prev.lastSpectralWarningUs;
    current.lastStereoWarningUs    = prev.lastStereoWarningUs;
    current.lastTransientWarningUs = prev.lastTransientWarningUs;

    // ─── 3. Generar eventos según transiciones ────────────────────────────
    generateEvents(prev, current);

    // ═══ Actualizar flags de transición DESPUÉS de generar eventos ═══￿
    // Sin esto, wasClipping/wasLowSignal nunca reflejan el estado actual,
    // causando que eventos de clipping/low-signal se disparen en CADA ciclo.
    current.wasClipping  = current.isClipping();
    current.wasLowSignal = (current.peakCombined < kLowSignalThreshold);

    // ─── 4. Computar health y attentionScore ──────────────────────────────
    // Health se asigna según el evento más severo reciente
    if (!current.hasSignal()) {
        current.health = TrackHealth::Silent;
    } else if (current.isClipping()) {
        current.health = TrackHealth::ClippingRisk;
    } else if (current.correlation < 0.0f) {
        current.health = TrackHealth::StereoCollapse;
    } else if (current.crestFactor < kCrestOvercompressed && current.crestFactor > 0.0f) {
        current.health = TrackHealth::Overcompressed;
    } else if (current.crestFactor > kCrestTooDynamic) {
        current.health = TrackHealth::NeedsCompression;
    } else if (current.peakCombined < kLowSignalThreshold) {
        current.health = TrackHealth::LowSignal;
    } else {
        current.health = TrackHealth::Clean;
    }

    updateAttention(current);

    // ─── 5. Almacenar estado actualizado ──────────────────────────────────
    states_[slotIndex] = current;
    globalCacheDirty_ = true;
}

// ═══════════════════════════════════════════════════════════════════════════
//  generateEvents — Compara estado previo vs actual y genera eventos
//  Esta es la lógica CENTRAL que convierte datos en "opinión del sistema".
// ═══════════════════════════════════════════════════════════════════════════
void TrackFeedCore::generateEvents(TrackState& prev, const TrackState& current)
{
    auto now = current.timestampUs;
    if (now <= 0)
        now = juce::Time::getMillisecondCounter() * 1000;

    // ═══ 1. Track appeared (prev inactive, current active) ════════════════
    if (!prev.active && current.active && current.hasSignal()) {
        TrackEvent ev;
        ev.type = TrackEventType::TrackAppeared;
        ev.trackId = current.slotIndex;
        ev.severity = 0.2f;
        ev.timestampUs = now;
        ev.message = current.trackName + " detectada";
        pushEvent(current.slotIndex, ev);
    }

    if (!current.hasSignal())
        return;

    // ═══ 2. Clipping detection ════════════════════════════════════════════
    if (current.isClipping() && !prev.wasClipping) {
        if (canFireEvent(current, TrackEventType::ClippingDetected, now)) {
            TrackEvent ev;
            ev.type = TrackEventType::ClippingDetected;
            ev.trackId = current.slotIndex;
            ev.severity = 1.0f;
            ev.timestampUs = now;
            ev.message = "🔴 " + current.trackName + " está recortando a "
                         + juce::String(current.peakCombined, 1) + " dB";
            ev.value = current.peakCombined;
            ev.threshold = kClippingThreshold;
            ev.deviation = current.peakCombined + 0.5f;
            pushEvent(current.slotIndex, ev);
        }
    }
    // Clipping cleared
    else if (!current.isClipping() && prev.wasClipping) {
        TrackEvent ev;
        ev.type = TrackEventType::ClippingCleared;
        ev.trackId = current.slotIndex;
        ev.severity = 0.3f;
        ev.timestampUs = now;
        ev.message = "🟢 " + current.trackName + " ya no recorta ("
                     + juce::String(current.peakCombined, 1) + " dB)";
        pushEvent(current.slotIndex, ev);
    }

    // ═══ 3. Level spike (peak subió > 6 dB respecto al ciclo anterior) ═══
    if (prev.peakCombined > -80.0f
        && (current.peakCombined - prev.peakCombined) > 6.0f)
    {
        if (canFireEvent(current, TrackEventType::LevelSpike, now)) {
            TrackEvent ev;
            ev.type = TrackEventType::LevelSpike;
            ev.trackId = current.slotIndex;
            ev.severity = 0.5f;
            ev.timestampUs = now;
            ev.message = "⚡ " + current.trackName + " subió "
                         + juce::String(current.peakCombined - prev.peakCombined, 1)
                         + " dB (" + juce::String(current.peakCombined, 1) + " dB)";
            ev.value = current.peakCombined;
            ev.threshold = prev.peakCombined + 6.0f;
            ev.deviation = current.peakCombined - prev.peakCombined;
            pushEvent(current.slotIndex, ev);
        }
    }

    // ═══ 4. Low signal ════════════════════════════════════════════════════
    if (current.peakCombined < kLowSignalThreshold && !prev.wasLowSignal) {
        if (canFireEvent(current, TrackEventType::LowSignal, now)) {
            TrackEvent ev;
            ev.type = TrackEventType::LowSignal;
            ev.trackId = current.slotIndex;
            ev.severity = 0.4f;
            ev.timestampUs = now;
            ev.message = "🔇 " + current.trackName + " señal muy baja ("
                         + juce::String(current.peakCombined, 1) + " dB)";
            ev.value = current.peakCombined;
            ev.threshold = kLowSignalThreshold;
            pushEvent(current.slotIndex, ev);
        }
    }

    // ═══ 5. Crest factor issues ═══════════════════════════════════════════
    if (current.crestFactor > 0.0f) {
        // Sobre-comprimido (crest muy bajo)
        if (current.crestFactor < kCrestOvercompressed && prev.crestFactor >= kCrestOvercompressed) {
            if (canFireEvent(current, TrackEventType::CrestTooLow, now, 120 * 1000 * 1000)) {
                TrackEvent ev;
                ev.type = TrackEventType::CrestTooLow;
                ev.trackId = current.slotIndex;
                ev.severity = 0.7f;
                ev.timestampUs = now;
                ev.message = "⚡ " + current.trackName
                             + " crest bajo (" + juce::String(current.crestFactor, 1)
                             + " dB) — ¿sobre-comprimido?";
                ev.value = current.crestFactor;
                ev.threshold = kCrestOvercompressed;
                pushEvent(current.slotIndex, ev);
            }
        }
        // Muy dinámico (crest muy alto)
        else if (current.crestFactor > kCrestTooDynamic && prev.crestFactor <= kCrestTooDynamic) {
            if (canFireEvent(current, TrackEventType::CrestTooHigh, now, 120 * 1000 * 1000)) {
                TrackEvent ev;
                ev.type = TrackEventType::CrestTooHigh;
                ev.trackId = current.slotIndex;
                ev.severity = 0.5f;
                ev.timestampUs = now;
                ev.message = "⚡ " + current.trackName
                             + " crest alto (" + juce::String(current.crestFactor, 1)
                             + " dB) — necesita compresión";
                ev.value = current.crestFactor;
                ev.threshold = kCrestTooDynamic;
                pushEvent(current.slotIndex, ev);
            }
        }
    }

    // ═══ 6. Phase / correlation issues ════════════════════════════════════
    if (current.correlation < kPhaseIssueThreshold && prev.correlation >= kPhaseIssueThreshold) {
        if (canFireEvent(current, TrackEventType::PhaseIssue, now)) {
            TrackEvent ev;
            ev.type = TrackEventType::PhaseIssue;
            ev.trackId = current.slotIndex;
            ev.severity = 0.8f;
            ev.timestampUs = now;
            ev.message = "🔮 " + current.trackName + " correlación negativa ("
                         + juce::String(current.correlation, 2) + ") — fase invertida?";
            ev.value = current.correlation;
            ev.threshold = kPhaseIssueThreshold;
            pushEvent(current.slotIndex, ev);
        }
    }
    else if (current.correlation < kStereoCollapseThreshold && current.correlation >= kPhaseIssueThreshold
             && prev.correlation >= kStereoCollapseThreshold) {
        if (canFireEvent(current, TrackEventType::StereoCollapse, now)) {
            TrackEvent ev;
            ev.type = TrackEventType::StereoCollapse;
            ev.trackId = current.slotIndex;
            ev.severity = 0.6f;
            ev.timestampUs = now;
            ev.message = "🔮 " + current.trackName + " correlación baja ("
                         + juce::String(current.correlation, 2) + ")";
            ev.value = current.correlation;
            ev.threshold = kStereoCollapseThreshold;
            pushEvent(current.slotIndex, ev);
        }
    }

    // ═══ 7. Spectral imbalance ════════════════════════════════════════════
    // Detecta si alguna banda espectral domina excesivamente
    {
        float maxBand = -100.0f, minBand = 100.0f;
        for (int b = 0; b < 6; ++b) {
            if (current.crestPerBand[b] > maxBand) maxBand = current.crestPerBand[b];
            if (current.crestPerBand[b] < minBand) minBand = current.crestPerBand[b];
        }
        float spectralSpread = maxBand - minBand;
        if (spectralSpread > kSpectralSpreadThreshold) {
            if (canFireEvent(current, TrackEventType::SpectralImbalance, now, 120 * 1000 * 1000)) {
                TrackEvent ev;
                ev.type = TrackEventType::SpectralImbalance;
                ev.trackId = current.slotIndex;
                ev.severity = computeSeverity(spectralSpread, kSpectralSpreadThreshold, 20.0f);
                ev.timestampUs = now;
                ev.message = "🎛️ " + current.trackName
                             + " desbalance espectral (" + juce::String(spectralSpread, 1)
                             + " dB entre bandas)";
                ev.value = spectralSpread;
                ev.threshold = kSpectralSpreadThreshold;
                pushEvent(current.slotIndex, ev);
            }
        }
    }

    // ═══ 8. Good balance (cuando todo está en rango y antes había problemas) ═══
    if (prev.health != TrackHealth::Clean && current.health == TrackHealth::Clean)
    {
        TrackEvent ev;
        ev.type = TrackEventType::Improvement;
        ev.trackId = current.slotIndex;
        ev.severity = 0.2f;
        ev.timestampUs = now;
        ev.message = "✅ " + current.trackName + " — todo en rango";
        pushEvent(current.slotIndex, ev);
    }

    // ═══ 9. Transient detected ════════════════════════════════════════════
    if (current.transientRatio > kTransientThreshold && prev.transientRatio <= kTransientThreshold) {
        if (canFireEvent(current, TrackEventType::TransientDetected, now, 30 * 1000 * 1000)) {
            TrackEvent ev;
            ev.type = TrackEventType::TransientDetected;
            ev.trackId = current.slotIndex;
            ev.severity = 0.2f;
            ev.timestampUs = now;
            ev.message = current.trackName + " transiente fuerte (ratio "
                         + juce::String(current.transientRatio, 1) + ")";
            pushEvent(current.slotIndex, ev);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  updateAttention — Computa el attentionScore para una pista
//  Fórmula: loudnessImpact + spectralProblem + stereoIssue + roleImportance
//  Range: 0.0 (baja prioridad) a 1.0 (debe hablar ahora)
// ═══════════════════════════════════════════════════════════════════════════
void TrackFeedCore::updateAttention(TrackState& state)
{
    if (!state.hasSignal()) {
        state.attentionScore = 0.0f;
        return;
    }

    float score = 0.0f;

    // ─── 1. Loudness impact (0.0 - 0.4) ──────────────────────────────────
    float loudnessScore = 0.0f;
    if (state.isClipping())
        loudnessScore = 0.4f;          // Clipping = máxima urgencia
    else if (state.peakCombined > -3.0f)
        loudnessScore = 0.25f;         // Near-clip
    else if (state.peakCombined > -10.0f)
        loudnessScore = 0.1f;          // Nivel normal-alto
    else if (state.peakCombined > -18.0f)
        loudnessScore = 0.05f;         // Nivel normal
    else if (state.peakCombined < kLowSignalThreshold)
        loudnessScore = 0.2f;          // Señal muy baja (puede ser problema)
    score += loudnessScore * kWeightLoudness;

    // ─── 2. Spectral problem (0.0 - 0.3) ─────────────────────────────────
    float spectralScore = 0.0f;
    if (state.crestFactor < kCrestOvercompressed && state.crestFactor > 0.0f)
        spectralScore = 0.3f;          // Sobre-comprimido
    else if (state.crestFactor > kCrestTooDynamic)
        spectralScore = 0.15f;         // Muy dinámico
    else {
        // Detectar desbalance spectral: crestPerBand muy dispares
        float maxCrest = 0.0f, minCrest = 100.0f;
        for (int b = 0; b < 6; ++b) {
            if (state.crestPerBand[b] > maxCrest) maxCrest = state.crestPerBand[b];
            if (state.crestPerBand[b] < minCrest) minCrest = state.crestPerBand[b];
        }
        float spread = maxCrest - minCrest;
        if (spread > kSpectralSpreadThreshold)
            spectralScore = 0.2f;
        else if (spread > kSpectralSpreadMid)
            spectralScore = 0.1f;
    }
    score += spectralScore * kWeightSpectral;

    // ─── 3. Stereo / phase issue (0.0 - 0.2) ─────────────────────────────
    float stereoScore = 0.0f;
    if (state.correlation < kPhaseIssueThreshold)
        stereoScore = 0.2f;            // Fase invertida
    else if (state.correlation < kStereoCollapseThreshold)
        stereoScore = 0.1f;            // Baja correlación
    else if (state.avgStereoWidth > 0.8f)
        stereoScore = 0.05f;           // Muy ancho (mono compat?)
    score += stereoScore * kWeightStereo;

    // ─── 4. Role importance (0.0 - 0.1) ──────────────────────────────────
    float roleScore = 0.0f;
    switch (state.role) {
        case TrackRole::Kick:
        case TrackRole::Kick808:
        case TrackRole::Snare:
        case TrackRole::VozPrincipal:
        case TrackRole::BassSub:
        case TrackRole::Bass808:
            roleScore = 0.1f;           // Elementos fundamentales
            break;
        case TrackRole::HiHat:
        case TrackRole::BassFinger:
        case TrackRole::GuitarRhythm:
        case TrackRole::SynthLead:
            roleScore = 0.07f;          // Elementos importantes
            break;
        case TrackRole::FxRiser:
        case TrackRole::FxAmbience:
        case TrackRole::Adlibs:
            roleScore = 0.03f;          // Elementos de adorno
            break;
        case TrackRole::Unknown:
            roleScore = 0.05f;          // Rol no asignado = atención media
            break;
        default:
            roleScore = 0.05f;
            break;
    }
    score += roleScore * kWeightRole;

    // ─── 5. Reference gap bonus (0.0 - 0.15) ─────────────────────────────
    // Si hay gaps contra la referencia en regiones donde esta pista opera,
    // la pista recibe un bonus de atención para que el coach hable de ella.
    float referenceBonus = 0.0f;
    bool hasAnyGap = false;
    for (int r = 0; r < 6; ++r) {
        float gapDb = referenceGapProfile_[r];
        if (gapDb > 0.0f) {
            hasAnyGap = true;
            // ¿La pista tiene energía significativa en esta región?
            if (state.midEnergyPerBand[r] > -35.0f) {
                // Más bonus para gaps más grandes, escalado a ~0.025 por región
                float gapWeight = juce::jlimit(0.0f, 1.0f, gapDb / 10.0f);
                referenceBonus += gapWeight * 0.025f;
            }
        }
    }
    // Si hay gaps pero la pista no tiene energía en ninguna región con gap,
    // aplicar un bonus mínimo para que el coach al menos mencione el gap
    if (hasAnyGap && referenceBonus < 0.02f)
        referenceBonus = 0.02f;
    score += juce::jlimit(0.0f, 0.15f, referenceBonus);

    state.attentionScore = juce::jlimit(0.0f, 1.0f, score);
}

// ═══════════════════════════════════════════════════════════════════════════
//  updateTrackRole — Actualiza solo el rol (sin datos de audio)
// ═══════════════════════════════════════════════════════════════════════════
void TrackFeedCore::updateTrackRole(int slotIndex, TrackRole role)
{
    if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots)
        return;

    const juce::ScopedLock lock(lock_);

    TrackRole prevRole = states_[slotIndex].role;
    states_[slotIndex].role = role;

    if (prevRole != role) {
        TrackEvent ev;
        ev.type = TrackEventType::RoleAssigned;
        ev.trackId = slotIndex;
        ev.severity = 0.15f;
        ev.timestampUs = juce::Time::getMillisecondCounter() * 1000;
        ev.message = "🏷️ " + states_[slotIndex].trackName + " → "
                     + juce::String(getRoleName(role));
        pushEvent(slotIndex, ev);
        globalCacheDirty_ = true;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  markTrackDisappeared — Marca una pista como desconectada
// ═══════════════════════════════════════════════════════════════════════════
void TrackFeedCore::markTrackDisappeared(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots)
        return;

    const juce::ScopedLock lock(lock_);

    if (!states_[slotIndex].active)
        return;

    states_[slotIndex].active = false;
    states_[slotIndex].health = TrackHealth::Silent;

    TrackEvent ev;
    ev.type = TrackEventType::TrackDisappeared;
    ev.trackId = slotIndex;
    ev.severity = 0.3f;
    ev.timestampUs = juce::Time::getMillisecondCounter() * 1000;
    ev.message = states_[slotIndex].trackName + " se desconectó";
    pushEvent(slotIndex, ev);
    globalCacheDirty_ = true;
}

// ═══════════════════════════════════════════════════════════════════════════
//  clearAll — Reset completo
// ═══════════════════════════════════════════════════════════════════════════
void TrackFeedCore::clearAll()
{
    const juce::ScopedLock lock(lock_);
    for (auto& state : states_)
        state = TrackState{};
    for (auto& history : eventHistory_)
        history.clear();
    globalEvents_.clear();
    globalEventCache_.clear();
    globalCacheDirty_ = false;
}

// ═══════════════════════════════════════════════════════════════════════════
//  CONSULTAS
// ═══════════════════════════════════════════════════════════════════════════

TrackState TrackFeedCore::getTrackState(int slotIndex) const
{
    const juce::ScopedLock lock(lock_);
    if (slotIndex >= 0 && slotIndex < SlotRegistry::kMaxSlots)
        return states_[slotIndex];
    return TrackState{};
}

std::vector<TrackEvent> TrackFeedCore::getRecentEvents(int trackId, int maxEvents) const
{
    const juce::ScopedLock lock(lock_);
    if (trackId < 0 || trackId >= SlotRegistry::kMaxSlots)
        return {};

    const auto& history = eventHistory_[trackId];
    if (history.empty())
        return {};

    int64_t cutoff = (juce::Time::getMillisecondCounter() * 1000)
                     - TrackStateDefaults::kEventRetentionUs;

    std::vector<TrackEvent> result;
    result.reserve(std::min(maxEvents, (int)history.size()));

    // Iterar en reversa (más recientes primero)
    for (int i = (int)history.size() - 1; i >= 0 && (int)result.size() < maxEvents; --i) {
        if (history[i].timestampUs >= cutoff)
            result.push_back(history[i]);
    }

    return result;
}

std::vector<TrackEvent> TrackFeedCore::getGlobalEvents(int maxEvents) const
{
    const juce::ScopedLock lock(lock_);

    if (globalCacheDirty_) {
        const_cast<TrackFeedCore*>(this)->rebuildGlobalCache();
    }

    if ((int)globalEventCache_.size() > maxEvents)
        return std::vector<TrackEvent>(globalEventCache_.begin(),
                                        globalEventCache_.begin() + maxEvents);
    return globalEventCache_;
}

float TrackFeedCore::getAttentionScore(int trackId) const
{
    const juce::ScopedLock lock(lock_);
    if (trackId >= 0 && trackId < SlotRegistry::kMaxSlots)
        return states_[trackId].attentionScore;
    return 0.0f;
}

std::vector<int> TrackFeedCore::getActiveTracksByPriority() const
{
    const juce::ScopedLock lock(lock_);

    std::vector<std::pair<int, float>> scored;
    for (int i = 0; i < SlotRegistry::kMaxSlots; ++i) {
        if (states_[i].active && states_[i].hasSignal())
            scored.emplace_back(i, states_[i].attentionScore);
    }

    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<int> result;
    result.reserve(scored.size());
    for (const auto& s : scored)
        result.push_back(s.first);

    return result;
}

TrackFeedCore::HealthSummary TrackFeedCore::getHealthSummary() const
{
    const juce::ScopedLock lock(lock_);

    HealthSummary summary;
    for (const auto& state : states_) {
        if (!state.active) continue;
        switch (state.health) {
            case TrackHealth::Clean:            summary.clean++;    break;
            case TrackHealth::NeedsEQ:
            case TrackHealth::NeedsCompression:
            case TrackHealth::MaskingIssue:
            case TrackHealth::PhaseIssue:       summary.warning++;  break;
            case TrackHealth::Overcompressed:
            case TrackHealth::ClippingRisk:
            case TrackHealth::StereoCollapse:   summary.critical++; break;
            case TrackHealth::LowSignal:
            case TrackHealth::Silent:           summary.silent++;   break;
            default:                            summary.unknown++;  break;
        }
    }
    return summary;
}

// ═══════════════════════════════════════════════════════════════════════════
//  HELPERS INTERNOS
// ═══════════════════════════════════════════════════════════════════════════

void TrackFeedCore::pushEvent(int slotIndex, const TrackEvent& event)
{
    if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots)
        return;

    auto& history = eventHistory_[slotIndex];
    history.push_back(event);

    // Podar historial si excede el máximo
    if ((int)history.size() > TrackStateDefaults::kMaxEventsPerTrack)
        pruneEventHistory(slotIndex);

    // Log
    LogHelper::writeToLog("[TrackFeed] " + event.message);

    // Callback
    if (eventCallback_)
        eventCallback_(event);
}

// ═══════════════════════════════════════════════════════════════════════════
//  pushTrackEvent — Empuja un evento asociado a una pista (rol-aware)
//  Thread-safe wrapper alrededor de pushEvent() para uso externo.
// ═══════════════════════════════════════════════════════════════════════════
void TrackFeedCore::pushTrackEvent(int slotIndex, const TrackEvent& event)
{
    const juce::ScopedLock lock(lock_);
    pushEvent(slotIndex, event);
    globalCacheDirty_ = true;
}

// ═══════════════════════════════════════════════════════════════════════════
//  updateTrackHealth — Actualiza el health de una pista desde análisis externo
//  Permite que análisis rol-aware (Sprint 6B) corrijan el health computado
//  con thresholds fijos por updateTrackState().
// ═══════════════════════════════════════════════════════════════════════════
void TrackFeedCore::updateTrackHealth(int slotIndex, TrackHealth health)
{
    const juce::ScopedLock lock(lock_);
    if (slotIndex >= 0 && slotIndex < SlotRegistry::kMaxSlots)
    {
        states_[slotIndex].health = health;
        globalCacheDirty_ = true;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  checkAndSetGainCooldown — Check and update gain event cooldown
//  Uses lastClipWarningUs from TrackState to avoid duplicate events.
// ═══════════════════════════════════════════════════════════════════════════
bool TrackFeedCore::checkAndSetGainCooldown(int slotIndex, int64_t nowUs, int64_t cooldownUs)
{
    const juce::ScopedLock lock(lock_);
    if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots)
        return false;
    auto& state = states_[slotIndex];
    if ((nowUs - state.lastClipWarningUs) >= cooldownUs)
    {
        state.lastClipWarningUs = nowUs;
        return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
//  checkAndSetCrestCooldown — Check and update crest event cooldown
//  Uses lastCrestWarningUs from TrackState to avoid duplicate events.
// ═══════════════════════════════════════════════════════════════════════════
bool TrackFeedCore::checkAndSetCrestCooldown(int slotIndex, int64_t nowUs, int64_t cooldownUs)
{
    const juce::ScopedLock lock(lock_);
    if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots)
        return false;
    auto& state = states_[slotIndex];
    if ((nowUs - state.lastCrestWarningUs) >= cooldownUs)
    {
        state.lastCrestWarningUs = nowUs;
        return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
//  checkAndSetTonalCooldown — Check and update tonal event cooldown
//  Uses lastSpectralWarningUs from TrackState to avoid duplicate events.
// ═══════════════════════════════════════════════════════════════════════════
bool TrackFeedCore::checkAndSetTonalCooldown(int slotIndex, int64_t nowUs, int64_t cooldownUs)
{
    const juce::ScopedLock lock(lock_);
    if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots)
        return false;
    auto& state = states_[slotIndex];
    if ((nowUs - state.lastSpectralWarningUs) >= cooldownUs)
    {
        state.lastSpectralWarningUs = nowUs;
        return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
//  checkAndSetPhaseCooldown — Check and update phase event cooldown
//  Uses lastPhaseWarningUs from TrackState to avoid duplicate events.
// ═══════════════════════════════════════════════════════════════════════════
bool TrackFeedCore::checkAndSetPhaseCooldown(int slotIndex, int64_t nowUs, int64_t cooldownUs)
{
    const juce::ScopedLock lock(lock_);
    if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots)
        return false;
    auto& state = states_[slotIndex];
    if ((nowUs - state.lastPhaseWarningUs) >= cooldownUs)
    {
        state.lastPhaseWarningUs = nowUs;
        return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
//  pushGlobalEvent — Empuja un evento global (no asociado a una pista)
// ═══════════════════════════════════════════════════════════════════════════
void TrackFeedCore::pushGlobalEvent(const TrackEvent& event)
{
    EventCallback callbackCopy;

    {
        const juce::ScopedLock lock(lock_);

        globalEvents_.push_back(event);

        // Podar global events si exceden el máximo
        constexpr int kMaxGlobalEvents = 64;
        if ((int)globalEvents_.size() > kMaxGlobalEvents)
        {
            int toRemove = (int)globalEvents_.size() - kMaxGlobalEvents;
            globalEvents_.erase(globalEvents_.begin(), globalEvents_.begin() + toRemove);
        }

        globalCacheDirty_ = true;

        LogHelper::writeToLog("[TrackFeed-Global] " + event.message);

        // Copiar callback para invocarlo FUERA del lock
        callbackCopy = eventCallback_;
    } // ScopedLock release aquí

    // Callback fuera del lock para evitar deadlocks
    if (callbackCopy)
        callbackCopy(event);
}

void TrackFeedCore::pruneEventHistory(int slotIndex)
{
    auto& history = eventHistory_[slotIndex];
    if ((int)history.size() <= TrackStateDefaults::kMaxEventsPerTrack)
        return;

    // Eliminar los más viejos hasta tener kMaxEventsPerTrack
    int toRemove = (int)history.size() - TrackStateDefaults::kMaxEventsPerTrack;
    history.erase(history.begin(), history.begin() + toRemove);
}

void TrackFeedCore::rebuildGlobalCache()
{
    globalEventCache_.clear();

    int64_t cutoff = (juce::Time::getMillisecondCounter() * 1000)
                     - TrackStateDefaults::kEventRetentionUs;

    // Recolectar eventos de todas las pistas
    for (const auto& history : eventHistory_) {
        for (const auto& ev : history) {
            if (ev.timestampUs >= cutoff)
                globalEventCache_.push_back(ev);
        }
    }

    // Recolectar eventos globales (referencia, logros, etc.)
    for (const auto& ev : globalEvents_) {
        if (ev.timestampUs >= cutoff)
            globalEventCache_.push_back(ev);
    }

    // Ordenar por severidad descendente
    std::sort(globalEventCache_.begin(), globalEventCache_.end(),
              [](const TrackEvent& a, const TrackEvent& b) {
                  return a.severity > b.severity;
              });

    // Limitar tamaño
    if ((int)globalEventCache_.size() > TrackStateDefaults::kMaxGlobalEvents)
        globalEventCache_.resize(TrackStateDefaults::kMaxGlobalEvents);

    globalCacheDirty_ = false;
}

bool TrackFeedCore::canFireEvent(const TrackState& state,
                                  TrackEventType type,
                                  int64_t nowUs,
                                  int64_t cooldownUs) const
{
    // Buscar el cooldown correspondiente al tipo de evento
    int64_t lastEventUs = 0;

    switch (type) {
        case TrackEventType::ClippingDetected:  lastEventUs = state.lastClipWarningUs;     break;
        case TrackEventType::LowSignal:         lastEventUs = state.lastLowSignalUs;        break;
        case TrackEventType::CrestTooLow:
        case TrackEventType::CrestTooHigh:      lastEventUs = state.lastCrestWarningUs;    break;
        case TrackEventType::PhaseIssue:
        case TrackEventType::StereoCollapse:    lastEventUs = state.lastPhaseWarningUs;    break;
        case TrackEventType::SpectralImbalance: lastEventUs = state.lastSpectralWarningUs; break;
        case TrackEventType::TransientDetected: lastEventUs = state.lastTransientWarningUs; break;
        default:                                return true;  // Sin cooldown para eventos ligeros
    }

    return (nowUs - lastEventUs) >= cooldownUs;
}

float TrackFeedCore::computeSeverity(float value, float threshold, float maxDeviation)
{
    float deviation = value - threshold;
    if (deviation <= 0.0f)
        return 0.0f;

    float normalized = deviation / maxDeviation;
    return juce::jlimit(0.0f, 1.0f, normalized);
}

float TrackFeedCore::clipSeverity(float value) noexcept
{
    return juce::jlimit(0.0f, 1.0f, value);
}

// ═══════════════════════════════════════════════════════════════════════════
//  CONSULTAS ADICIONALES
// ═══════════════════════════════════════════════════════════════════════════

int TrackFeedCore::getActiveTrackCount() const
{
    const juce::ScopedLock lock(lock_);
    int count = 0;
    for (const auto& state : states_) {
        if (state.active)
            count++;
    }
    return count;
}

int TrackFeedCore::getTotalEventCount() const
{
    const juce::ScopedLock lock(lock_);
    int count = 0;
    for (const auto& history : eventHistory_)
        count += (int)history.size();
    return count;
}

} // namespace mixcoach
