#include "CoachEngine.h"
#include "../../Common/types/Constants.h"
#include "../../Common/types/LogHelper.h"
#include "../../Common/name/NameInferrer.h"
#include "../../Messenger/core/MessengerType.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <vector>
#include <cmath>

namespace mixcoach {

// Forward declarations for free functions defined later in this file
TrackRole getTrackRoleForTrackType(TrackType type) noexcept;
TrackType getTrackTypeForRole(TrackRole role) noexcept;

CoachEngine::CoachEngine(PhaseManager& phaseManager, SharedData& sharedData, AudioAnalyzer& audioAnalyzer)
    : phaseManager_(phaseManager)
    , sharedData_(sharedData)
    , audioAnalyzer_(audioAnalyzer)
    , trackFeedCore_(std::make_unique<TrackFeedCore>())
{
    // Inicializar estados de pista
    for (auto& state : trackStates_) {
        state = TrackAnalysisState{};
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  HELPERS
// ═══════════════════════════════════════════════════════════════════════════

TrackTelemetry CoachEngine::getLatestTelemetry(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots)
        return TrackTelemetry{};
    auto result = sharedData_.getTrackAudioResult(slotIndex);
    if (result.timestampUs <= 0)
        return TrackTelemetry{};
    TrackTelemetry telem;
    result.timestampUs = result.timestampUs;
    telem.peakLeft = result.peakLeft;
    telem.peakRight = result.peakRight;
    telem.rmsLeft = result.rmsLeft;
    telem.rmsRight = result.rmsRight;
    telem.correlation = result.correlation;
    telem.crestFactor = result.crestPerBand[0];
    telem.lufsMomentary = -100.0f;
    telem.lufsShortTerm = -100.0f;
    for (int b = 0; b < 30; ++b)
        telem.bandEnergies[b] = result.bandEnergies[b];
    return telem;
}

void CoachEngine::respondWith(const juce::String& text, MentorMessage::Type type)
{
    MentorMessage msg;
    msg.type      = type;
    msg.text      = text.toStdString();
    msg.timestamp = juce::Time::getMillisecondCounter() * 1000;
    msg.context   = "MixCoach";
    sharedData_.pushMessage(msg);
    LogHelper::writeToLog("[CoachEngine] Mensaje: " + text.substring(0, 80));
}

void CoachEngine::respondWithContext(const juce::String& text,
                                     const juce::String& context,
                                     MentorMessage::Type type)
{
    MentorMessage msg;
    msg.type      = type;
    msg.text      = text.toStdString();
    msg.timestamp = juce::Time::getMillisecondCounter() * 1000;
    msg.context   = context.toStdString();
    sharedData_.pushMessage(msg);
    LogHelper::writeToLog("[CoachEngine] Mensaje (" + context + "): "
                          + text.substring(0, 80));
}

// ═══════════════════════════════════════════════════════════════════════════
//  RESET — Reinicia estados de pista para empezar de nuevo la fase
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::resetTrackStates()
{
    for (auto& state : trackStates_) {
        state = TrackAnalysisState{};
    }
    lastPeakWarningUs_ = 0;
    lastCrestWarningUs_ = 0;
    lastPhaseWarningUs_ = 0;
    lastHeadroomWarningUs_ = 0;
    lastTonalWarningUs_ = 0;
    lastDynamicWarningUs_ = 0;
    lastLoudnessWarningUs_ = 0;
    lastMaskingWarningUs_ = 0;
    // Resetear orden de senial (V13 Silence + Load Order)
    for (auto& ts : firstSignalTimestampsUs_) ts = 0;
    signalOrderCount_ = 0;
    for (auto& applied : signalOrderApplied_) applied = false;
    for (auto& confirmed : signalOrderConfirmed_) confirmed = false;
    LogHelper::writeToLog("[CoachEngine] Track states and cooldowns reset");
}

// ═══════════════════════════════════════════════════════════════════════════
//  ANÁLISIS PERIÓDICO — Llama a los análisis según la fase actual
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::periodicAnalysis()
{
    auto now = juce::Time::getMillisecondCounter() * 1000;

    // Throttle: solo cada ~8s
    if (now - lastPeriodicAnalysisUs_ < kAnalysisIntervalUs)
        return;
    lastPeriodicAnalysisUs_ = now;

    auto& registry = sharedData_.getSlotRegistry();
    if (registry.activeCount() == 0)
        return;

        LogHelper::writeToLog("[CoachEngine] Análisis periódico iniciado ("
                          + juce::String(registry.activeCount()) + " pistas activas)");

    // ═══ SPRINT 5: Sincronizar TrackFeedCore con datos frescos ══════════
    syncTrackFeedCore();

    // ═══ SPRINT 6A: Per-track gain analysis ═══════════════
    auto allGainAdvice = analyzeAllTracksGain();
    {
        int offTargetCount = 0;
        for (const auto& adv : allGainAdvice)
            if (adv.status == TrackGainAdvice::Status::OffTarget)
                offTargetCount++;

        if (offTargetCount > 0
            && now - lastGainAdviceUs_ >= kGainAdviceCooldownUs)
        {
            lastGainAdviceUs_ = now;
            juce::String examples;
            int shown = 0;
            for (const auto& adv : allGainAdvice)
            {
                if (adv.status == TrackGainAdvice::Status::OffTarget
                    && shown < 3)
                {
                    if (shown > 0) examples += "\n";
                    examples += adv.message;
                    shown++;
                }
            }
            if (offTargetCount == 1)
            {
                respondWith(examples, MentorMessage::Type::Tip);
            }
            else
            {
                respondWith(
                    "\xF0\x9F\x93\x8A **" + juce::String(offTargetCount)
                    + " pistas fuera de rango \xF3\xBE\x90\xA2ptimo:**\n"
                    + examples + "\n\xF0\x9F\x92\xA1 Ajusta el fader de gain de cada pista.",
                    MentorMessage::Type::Tip);
            }
        }
    }
    // ═══ FIN Sprint 6A ═══════════════════════

    // ═══ SPRINT 6B: Per-track dynamics analysis ══════════
    {
        auto allDynAdvice = analyzeAllTracksDynamics();
        int offTargetDyn = 0;
        for (const auto& adv : allDynAdvice)
            if (adv.status == TrackDynamicsAdvice::Status::OffTarget)
                offTargetDyn++;

        if (offTargetDyn > 0
            && now - lastDynamicsAdviceUs_ >= kDynamicsAdviceCooldownUs)
        {
            lastDynamicsAdviceUs_ = now;
            juce::String dynExamples;
            int shownDyn = 0;
            for (const auto& adv : allDynAdvice)
            {
                if (adv.status == TrackDynamicsAdvice::Status::OffTarget
                    && shownDyn < 3)
                {
                    if (shownDyn > 0) dynExamples += "\n";
                    dynExamples += adv.message;
                    shownDyn++;
                }
            }
            if (offTargetDyn == 1)
            {
                respondWith(dynExamples, MentorMessage::Type::Tip);
            }
            else
            {
                respondWith(
                    "\xF0\x9F\x93\x8A **" + juce::String(offTargetDyn)
                    + " pistas con problemas de din\xC3\xA1mica:**\n"
                    + dynExamples
                    + "\n\xF0\x9F\x92\xA1 Revisa los compresores de cada pista.",
                    MentorMessage::Type::Tip);
            }
        }
    }
    // ═══ FIN Sprint 6B ═══════════════════════
    // ═════════════════════
    //  SPRINT 8: Per-track phase analysis
    // ═════════════════════
    {
        auto allPhaseAdvice = analyzeAllTracksPhase();
        int offTargetPhase = 0;
        for (const auto& adv : allPhaseAdvice)
            if (adv.status == TrackPhaseAdvice::Status::OffTarget)
                offTargetPhase++;

        if (offTargetPhase > 0
            && now - lastPhaseAdviceUs_ >= kPhaseAdviceCooldownUs)
        {
            lastPhaseAdviceUs_ = now;
            juce::String examples;
            int shown = 0;
            for (const auto& adv : allPhaseAdvice)
            {
                if (adv.status == TrackPhaseAdvice::Status::OffTarget && shown < 3)
                {
                    if (shown > 0) examples += "\n";
                    examples += adv.message;
                    shown++;
                }
            }
            if (offTargetPhase == 1)
                respondWith(examples, MentorMessage::Type::Warning);
            else
                respondWith(
                    "\xf0\x9f\x94\xae **" + juce::String(offTargetPhase)
                    + " pistas con problemas de fase:**\n"
                    + examples
                    + "\n\xf0\x9f\x92\xa1 Revisa la correlacion de cada pista.",
                    MentorMessage::Type::Warning);
        }
    }

    // Siempre ejecutar análisis global independientemente de la fase
    analyzeSpectralMaskingReal();

    analyzeOverallMixReal();

    // ═══ Actualizar datos del ReferenceMatchPanel (comparación mix vs referencia) ═══
    // Esto permite que las barras espectrales y LUFS del ReferenceMatchPanel se
    // actualicen en vivo mientras el usuario ajusta la mezcla.
    if (referenceFingerprint_.valid)
        computeAndSendMatchData();

    // ═══ SPRINT 1: Continuous role inference during Organización phase ═══
    // inferTrackRoles() is idempotent (skips slots that already have a role),
    // so calling it every 8s is safe and catches new tracks automatically.
    // Pattern mirrors the referenceDrivenMode_ block below.
    if (isMixMode() && phaseManager_.getCurrentPhase() == MentorPhase::Organizacion)
    {
        inferTrackRoles();
    }

    // ═══ REFERENCE-DRIVEN MODE: Computar match progreso y enviar análisis ═══
    // Cuando el modo está activo y hay referencia cargada, computamos el % de match
    // contra la referencia cada ciclo de análisis y detectamos mejora/empeoramiento.
    if (referenceDrivenMode_ && referenceFingerprint_.valid)
    {
        auto progress = computeReferenceMatchProgress();

        // ─── Mensaje de bienvenida al activar REF MODE por primera vez ────
        if (!referenceDrivenWelcomeSent_)
        {
            referenceDrivenWelcomeSent_ = true;
            juce::String welcomeMsg;
            welcomeMsg += "\xf0\x9f\x8e\xaf **Modo Referencia activado** \xe2\x80\x94 ahora toda la mezcla se mide contra ";
            welcomeMsg += getReferenceName();
            welcomeMsg += ".\n";
            welcomeMsg += "Te ir\xc3\xa9 contando c\xc3\xb3mo vamos: qu\xc3\xa9 \xc3\xa1reas est\xc3\xa1n cerca y cu\xc3\xa1les necesitan ajuste.\n";
            welcomeMsg += "\xf0\x9f\x92\xa1 Escucha la referencia, ajusta tu mezcla, y yo te dir\xc3\xa9 cuando se parezcan.";

        }

        if (now - lastReferenceDrivenAnalysisUs_ >= kReferenceDrivenAnalysisIntervalUs)
        {
            lastReferenceDrivenAnalysisUs_ = now;

            // Detectar cambios significativos en el match (1ra condición)
            bool significantChange = std::abs(progress.delta) >= 0.05f
                && now - lastReferenceGapImprovedUs_ >= kReferenceDrivenProgressCooldownUs;

            // O mensaje periódico incluso sin cambios (2da condición)
            bool periodicStatus = (now - lastReferenceDrivenStatusUs_ >= kReferenceDrivenStatusIntervalUs)
                && progress.currentMatch > 0.01f;

            if (significantChange)
            {
                sendReferenceDrivenAnalysis();

                if (progress.delta > 0)
                    lastReferenceGapImprovedUs_ = now;
                else
                    lastReferenceGapWorsenedUs_ = now;

                lastReferenceDrivenStatusUs_ = now; // reset periodic timer
            }
            else if (periodicStatus)
            {
                lastReferenceDrivenStatusUs_ = now;
                sendReferenceDrivenAnalysis();
            }
        }
    }

    // Análisis específico según la fase actual    // Análisis específico según la fase actual
    auto phase = phaseManager_.getCurrentPhase();
    switch (phase) {
        case MentorPhase::GainStaging:
            analyzeGainStagingReal();
            break;
        case MentorPhase::Organizacion:
            analyzeOrganisationReal();
            break;
        case MentorPhase::Balance:
            analyzeTonalBalanceReal();
            break;
        case MentorPhase::Compresion:
            analyzeDynamicsReal();
            break;
        case MentorPhase::Espacio:
            analyzePhaseReal();
            break;
        default:
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  GAIN STAGING — Picos, clipping, headroom

// ═══════════════════════════════════════════════════════════════════════════
//  SILENCE + LOAD ORDER INFERENCE V13
// ═══════════════════════════════════════════════════════════════════════════

static constexpr TrackRole kSignalOrderRoles[] = {
    TrackRole::Kick,          // Position 0
    TrackRole::Snare,         // Position 1
    TrackRole::HiHat,         // Position 2
    TrackRole::VozPrincipal,  // Position 3 — vocal suele entrar temprano
    TrackRole::BassSub,       // Position 4
    TrackRole::Clap,          // Position 5
    TrackRole::SynthPad,      // Position 6
    TrackRole::Percussion,    // Position 7
    TrackRole::FxRiser,       // Position 8
    TrackRole::FxAmbience,    // Position 9
};

static constexpr float kSignalOrderConfidences[] = {
    0.60f, 0.50f, 0.45f, 0.55f,
    0.40f, 0.45f, 0.35f, 0.40f,
    0.35f, 0.30f,
};

static constexpr int kSignalOrderRolesCount =
    sizeof(kSignalOrderRoles) / sizeof(kSignalOrderRoles[0]);

// ═══════════════════════════════════════════════════════════════════════════
//  detectFirstSignal
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::detectFirstSignal()
{
    int64_t now = juce::Time::getMillisecondCounter() * 1000;
    auto& registry = sharedData_.getSlotRegistry();

    registry.forEachActive([&](const SlotInfo& info) {
        int idx = info.slotIndex;
        if (idx < 0 || idx >= SlotRegistry::kMaxSlots)
            return;
        if (firstSignalTimestampsUs_[idx] > 0)
            return;

        auto telem = getLatestTelemetry(idx);
        if (telem.timestamp == 0)
            return;

        float peakDb = juce::jmax(telem.peakLeft, telem.peakRight);
        if (peakDb > kSignalThresholdDb && signalOrderCount_ < kMaxSignalOrder)
        {
            firstSignalTimestampsUs_[idx] = now;
            signalOrderIndices_[signalOrderCount_] = idx;
            signalOrderCount_++;

            LogHelper::writeToLog(juce::String("[SignalOrder] Slot ") + juce::String(idx)
                                  + " primera senial en posicion "
                                  + juce::String(signalOrderCount_ - 1)
                                  + " (peak=" + juce::String(peakDb, 1) + " dB)");
        }

        // --- SPRINT 6A: Role-aware gain analysis ---
        // Post-process con analyzeTrackGain() usando targets
        // por rol (ExpectedProfile.peakTargetDb) en vez de thresholds fijos.
        // Empuja eventos y actualiza health para que aparezcan en el banner UI.
        auto gainAdvice = analyzeTrackGain(idx);
        if (gainAdvice.isActionable())
        {
            TrackEvent ev;
            ev.trackId = idx;
            ev.timestampUs = telem.timestamp;
            ev.value = gainAdvice.currentPeak;
            ev.threshold = gainAdvice.peakTarget;
            ev.deviation = gainAdvice.peakDeviation;
            ev.context = "gain";

            // Cooldown: only push if enough time has passed
            int64_t nowGc = telem.timestamp > 0 ? telem.timestamp : juce::Time::getMillisecondCounter() * 1000;
            if (!trackFeedCore_->checkAndSetGainCooldown(idx, nowGc, kGainAdviceCooldownUs))
                return;

            if (gainAdvice.currentPeak > -0.5f)
            {
                ev.type = TrackEventType::ClippingDetected;
                ev.severity = 0.9f;
            }
            else if (gainAdvice.currentPeak > -6.0f)
            {
                ev.type = TrackEventType::LevelSpike;
                ev.severity = 0.6f;
            }
            else if (gainAdvice.currentPeak < -30.0f)
            {
                ev.type = TrackEventType::LowSignal;
                ev.severity = 0.4f;
            }
            else
            {
                ev.type = TrackEventType::Info;
                ev.severity = 0.3f;
            }
            ev.message = gainAdvice.message;

            trackFeedCore_->pushTrackEvent(idx, ev);

            // Actualizar health segun el advice rol-aware
            if (gainAdvice.currentPeak > -0.5f)
                trackFeedCore_->updateTrackHealth(idx, TrackHealth::ClippingRisk);
            else if (gainAdvice.currentPeak < -30.0f)
                trackFeedCore_->updateTrackHealth(idx, TrackHealth::LowSignal);
        }

        // --- SPRINT 6C: Role-aware tonal analysis ---
        // Post-process con analyzeTrackTonal() usando targets
        // por rol (ExpectedProfile.spectralOffset) en vez de thresholds fijos.
        // Empuja eventos y actualiza health para que aparezcan en el banner UI.
        auto tonalAdvice = analyzeTrackTonal(idx);
        if (tonalAdvice.isActionable())
        {
            TrackEvent ev;
            ev.trackId = idx;
            ev.timestampUs = telem.timestamp;
            ev.value = tonalAdvice.worstDeviation;
            ev.threshold = TrackTonalAdvice::kToleranceDb;
            ev.deviation = tonalAdvice.worstDeviation;
            ev.context = "tonal";

            // Cooldown: only push if enough time has passed
            int64_t nowTc = telem.timestamp > 0 ? telem.timestamp : juce::Time::getMillisecondCounter() * 1000;
            if (!trackFeedCore_->checkAndSetTonalCooldown(idx, nowTc, kTonalAdviceCooldownUs))
                return;

            if (tonalAdvice.status == TrackTonalAdvice::Status::OffTarget)
            {
                ev.type = TrackEventType::SpectralImbalance;
                ev.severity = 0.7f;
            }
            else
            {
                ev.type = TrackEventType::SpectralImbalance;
                ev.severity = 0.3f;
            }
            ev.message = tonalAdvice.message;

            trackFeedCore_->pushTrackEvent(idx, ev);

            // Actualizar health segun el advice rol-aware
            if (tonalAdvice.status == TrackTonalAdvice::Status::OffTarget)
                trackFeedCore_->updateTrackHealth(idx, TrackHealth::NeedsEQ);
        }
        // --- SPRINT 8: Role-aware phase analysis ---
        auto phaseAdvice = analyzeTrackPhase(idx);
        if (phaseAdvice.isActionable())
        {
            TrackEvent ev;
            ev.trackId = idx;
            ev.timestampUs = telem.timestamp;
            ev.value = phaseAdvice.currentCorrelation;
            ev.threshold = 0.3f;
            ev.deviation = phaseAdvice.correlationDeviation;
            ev.context = "phase";

            int64_t nowPh = telem.timestamp > 0 ? telem.timestamp : juce::Time::getMillisecondCounter() * 1000;
            if (!trackFeedCore_->checkAndSetPhaseCooldown(idx, nowPh, kPhaseAdviceCooldownUs))
                return;

            if (phaseAdvice.status == TrackPhaseAdvice::Status::OffTarget)
            {
                ev.type = TrackEventType::PhaseIssue;
                ev.severity = 0.8f;
            }
            else
            {
                ev.type = TrackEventType::PhaseIssue;
                ev.severity = 0.4f;
            }
            ev.message = phaseAdvice.message;

            trackFeedCore_->pushTrackEvent(idx, ev);

            if (phaseAdvice.status == TrackPhaseAdvice::Status::OffTarget)
                trackFeedCore_->updateTrackHealth(idx, TrackHealth::PhaseIssue);
        }

    });
}

// ═══════════════════════════════════════════════════════════════════════════
//  inferBySilenceOrder
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::inferBySilenceOrder()
{
    if (signalOrderCount_ == 0)
        return;

    for (int pos = 0; pos < signalOrderCount_ && pos < kSignalOrderRolesCount; ++pos)
    {
        int idx = signalOrderIndices_[pos];
        if (idx < 0 || idx >= SlotRegistry::kMaxSlots)
            continue;
        if (trackRoles_[idx] != TrackRole::Unknown)
            continue;

        auto info = sharedData_.getSlotRegistry().getSlotInfo(idx);
        juce::String trackName = juce::String(info.trackName).trim();
        if (trackName.isNotEmpty()
            && !trackName.startsWithIgnoreCase("Pista")
            && !trackName.startsWithIgnoreCase("Track")
            && !trackName.startsWithIgnoreCase("Channel"))
        {
            continue;
        }

        TrackRole inferredRole = kSignalOrderRoles[pos];
        trackRoles_[idx] = inferredRole;
        trackRoleWasInferred_[idx] = true;
        signalOrderApplied_[idx] = true;

        LogHelper::writeToLog(juce::String("[SignalOrder] Slot ") + juce::String(idx)
                              + " inferido como " + juce::String(getRoleName(inferredRole))
                              + " (posicion " + juce::String(pos) + ")");

        auto& registry2 = sharedData_.getSlotRegistry();
        TrackType mappedType = getTrackTypeForRole(inferredRole);
        registry2.updateSlotTrackType(idx, (int)mappedType);
    }
}



void CoachEngine::respondWithLLM(const juce::String& text)
{
    // Enviar como mensaje premium (no sistema) para que el chat UI
    // lo renderice como burbuja completa en vez de compacto dimmed.
    respondWithPremium(text, MentorMessage::Type::Info);
}

void CoachEngine::analyzeGainStagingReal()
{
    auto now = juce::Time::getMillisecondCounter() * 1000;
    auto& registry = sharedData_.getSlotRegistry();

    float maxGlobalPeak = -100.0f;
    int clippingCount = 0;
    int lowSignalCount = 0;

    registry.forEachActive([&](const SlotInfo& info) {
        auto telem = getLatestTelemetry(info.slotIndex);
        if (telem.timestamp == 0) return;

        float peakDb = juce::jmax(telem.peakLeft, telem.peakRight);
        maxGlobalPeak = juce::jmax(maxGlobalPeak, peakDb);

        auto& state = trackStates_[info.slotIndex];
        juce::String trackName = juce::String(info.trackName).trim();
        if (trackName.isEmpty()) trackName = "Pista " + juce::String(info.slotIndex + 1);

        // ─── SPRINT 6A: Obtener target del rol via analyzeTrackGain ───────
        auto gainAdvice = analyzeTrackGain(info.slotIndex);
        float peakTarget = gainAdvice.peakTarget;
        bool hasTarget = (gainAdvice.role != TrackRole::Unknown
                         && gainAdvice.role != TrackRole::Master);

        // ─── Clipping detection (peak > -0.5dB) con target del rol ───────
        if (peakDb > -0.5f) {
            clippingCount++;
            if (!state.wasClipping) {
                state.wasClipping = true;
                if (now - lastPeakWarningUs_ > kWarningCooldownUs) {
                    lastPeakWarningUs_ = now;
                    juce::String msg;
                    msg += "🔴 **" + trackName + "** está recortando a **"
                        + juce::String(peakDb, 1) + " dB";
                    if (hasTarget)
                        msg += " (target " + juce::String(peakTarget, 1) + " dBFS)";
                    msg += ". Reduce el gain inmediatamente.";
                    respondWithContext(msg, trackName, MentorMessage::Type::Warning);
                }
            }
        }
        // ─── Pre-clipping warning (peak > -3dB) con delta al target ──────
        else if (peakDb > -3.0f && peakDb > state.lastPeakDb) {
            if (now - state.lastWarningUs > kTrackCooldownUs) {
                state.lastWarningUs = now;
                juce::String msg;
                msg += "⚠️ **" + trackName + "** está a **" + juce::String(peakDb, 1)
                    + " dBFS";
                if (hasTarget) {
                    float delta = peakDb - peakTarget;
                    msg += " (target " + juce::String(peakTarget, 1) + " dBFS). Baja ~"
                        + juce::String(std::abs(delta), 1) + " dB";
                } else {
                    msg += "**. Considera reducir el gain 3-6 dB";
                }
                msg += " para llegar a rango óptimo.";
                respondWithContext(msg, trackName, MentorMessage::Type::Tip);
            }
        }
        // ─── Señal muy baja ─────────────────────────────────────────────
        else if (peakDb < -30.0f) {
            lowSignalCount++;
            if (!state.wasLowSignal && registry.activeCount() > 1) {
                state.wasLowSignal = true;
                juce::String msg;
                msg += "🔇 **" + trackName + "** tiene señal muy baja (" + juce::String(peakDb, 1) + " dB)";
                if (hasTarget)
                    msg += ", target " + juce::String(peakTarget, 1) + " dBFS";
                msg += ". ¿Subiste el fader?";
                respondWithContext(msg, trackName, MentorMessage::Type::Info);
            }
        } else {
            state.wasClipping = false;
            state.wasLowSignal = false;
        }

        // ─── Crest factor analysis ────────────────────────────────────────
        if (telem.crestFactor > 0.0f && telem.rmsLeft > -40.0f) {
            state.lastCrestFactor = telem.crestFactor;
        }

        state.lastPeakDb = peakDb;
        state.lastRmsDb  = telem.rmsLeft;
    });

    // ─── Resumen global de headroom ───────────────────────────────────────
    if (maxGlobalPeak > -6.0f && maxGlobalPeak < -0.5f) {
        if (now - lastHeadroomWarningUs_ > kWarningCooldownUs) {
            lastHeadroomWarningUs_ = now;
            float headroom = -maxGlobalPeak;
            respondWith(
                "📊 **Headroom: " + juce::String(headroom, 1) + " dB** — "
                "la pista con más nivel alcanza **" + juce::String(maxGlobalPeak, 1)
                + " dB**. El rango ideal es -6 dB a -3 dB de pico en el master.",
                MentorMessage::Type::Tip);
        }
    } else if (maxGlobalPeak < -18.0f && registry.activeCount() >= 3) {
        if (now - lastHeadroomWarningUs_ > kWarningCooldownUs) {
            lastHeadroomWarningUs_ = now;
            respondWith(
                "📊 Las pistas están muy bajas (pico máximo: **" + juce::String(maxGlobalPeak, 1)
                + " dB**). Sube los faders de gain hasta que el master marque "
                "entre -12 dB y -6 dB.",
                MentorMessage::Type::Info);
        }
    }

    // Log de diagnóstico
    if (clippingCount > 0 || lowSignalCount > 0) {
        LogHelper::writeToLog("[CoachEngine] GainStaging: "
                              + juce::String(clippingCount) + " clipping, "
                              + juce::String(lowSignalCount) + " baja señal, "
                              + "pico global=" + juce::String(maxGlobalPeak, 1) + " dB");
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  ORGANIZACIÓN — Conteo de pistas, buses, colores
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::analyzeOrganisationReal()
{
    auto& registry = sharedData_.getSlotRegistry();
    int active = registry.activeCount();

    if (active == 0) return;

    int bussedCount = 0;
    registry.forEachActive([&](const SlotInfo& info) {
        if (info.bus != BusType::None)
            bussedCount++;
    });

    int unnamedCount = 0;
    registry.forEachActive([&](const SlotInfo& info) {
        juce::String name = juce::String(info.trackName).trim();
        if (name.isEmpty() || name.startsWith("Pista"))
            unnamedCount++;
    });

    if (unnamedCount > 0) {
        respondWith(
            "📝 **" + juce::String(unnamedCount) + " pista(s)** sin nombre. "
            "Nombrar cada pista ayuda a mantener la mezcla organizada.",
            MentorMessage::Type::Info);
    }

    if (active >= 3 && bussedCount < active / 2) {
        respondWith(
            "🔗 Solo **" + juce::String(bussedCount) + "/" + juce::String(active)
            + "** pistas tienen bus asignado. Agrupar por familias "
            "(batería, bajo, voces) facilita el procesamiento por grupos.",
            MentorMessage::Type::Tip);
    }

    LogHelper::writeToLog("[CoachEngine] Organización: "
                          + juce::String(active) + " activas, "
                          + juce::String(bussedCount) + " con bus, "
                          + juce::String(unnamedCount) + " sin nombre");
}

// ═══════════════════════════════════════════════════════════════════════════
//  BALANCE TONAL — Espectro, comparación de bandas
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::analyzeTonalBalanceReal()
{
    auto now = juce::Time::getMillisecondCounter() * 1000;
    auto& registry = sharedData_.getSlotRegistry();

    bool hasSpectrumData = false;
    float avgSubBass   = -100.0f; // bins 0-1:   ~20-47Hz    (deep sub)
    float avgLowBass   = -100.0f; // bins 1-3:   ~47-141Hz   (sub-bass)
    float avgBass      = -100.0f; // bins 3-6:   ~141-281Hz  (bass fundamental)
    float avgLowMids   = -100.0f; // bins 6-13:  ~281-609Hz  (low mids)
    float avgMids      = -100.0f; // bins 13-25: ~609-1172Hz (mids)
    float avgHighMids  = -100.0f; // bins 25-53: ~1.17-2.48kHz
    float avgPresence  = -100.0f; // bins 53-106:~2.48-4.97kHz
    float avgHighs     = -100.0f; // bins 106-213:~4.97-9.98kHz
    float avgAir       = -100.0f; // bins 213-426:~9.98-19.97kHz
    int trackCount = 0;

    registry.forEachActive([&](const SlotInfo& info) {
        auto telem = getLatestTelemetry(info.slotIndex);
        if (telem.timestamp == 0) return;

        // Verificar si hay datos espectrales válidos
        float spectrumSum = 0.0f;
        for (int fi = 0; fi < kNumSpectrumBins; ++fi)
            spectrumSum += telem.spectrum[fi];

        if (spectrumSum < 0.01f) return; // Sin datos espectrales recientes
        hasSpectrumData = true;
        trackCount++;

        // Promedio por bandas
        auto bandAvg = [&](int start, int end) -> float {
            float s = 0.0f;
            for (int fi = start; fi < end && fi < kNumSpectrumBins; ++fi)
                s += telem.spectrum[fi];
            int n = juce::jmin(end - start, kNumSpectrumBins - start);
            return (n > 0) ? (s / n) : -100.0f;
        };

        float subBass  = bandAvg(0, 1);
        float lowBass  = bandAvg(1, 3);
        float bass     = bandAvg(3, 6);
        float lowMids  = bandAvg(6, 13);
        float mids     = bandAvg(13, 25);
        float highMids = bandAvg(25, 53);
        float presence = bandAvg(53, 106);
        float highs    = bandAvg(106, 213);
        float air      = bandAvg(213, 426);

        auto accumMax = [](float& acc, float v) {
            if (v > acc) acc = v;
        };

        accumMax(avgSubBass,  subBass);
        accumMax(avgLowBass,  lowBass);
        accumMax(avgBass,     bass);
        accumMax(avgLowMids,  lowMids);
        accumMax(avgMids,     mids);
        accumMax(avgHighMids, highMids);
        accumMax(avgPresence, presence);
        accumMax(avgHighs,    highs);
        accumMax(avgAir,      air);
    });

    if (!hasSpectrumData || trackCount == 0)
        return;

    // ─── Detectar desbalances espectrales ─────────────────────────────────
    // Convertir a dBFS (los bins están normalizados 0..1, convertir a dB)
    auto toDb = [](float v) -> float {
        return (v > 0.001f) ? juce::Decibels::gainToDecibels(v) : -60.0f;
    };

    float subBassDb  = toDb(avgSubBass);
    float lowBassDb  = toDb(avgLowBass);
    float bassDb     = toDb(avgBass);
    float lowMidsDb  = toDb(avgLowMids);
    float midsDb     = toDb(avgMids);
    float highMidsDb = toDb(avgHighMids);
    float presenceDb = toDb(avgPresence);
    float highsDb    = toDb(avgHighs);
    float airDb      = toDb(avgAir);

    // Log de diagnóstico
    LogHelper::writeToLog("[CoachEngine] Espectro (dBFS): sub="
                          + juce::String(subBassDb, 1) + " lbass="
                          + juce::String(lowBassDb, 1) + " bass="
                          + juce::String(bassDb, 1) + " lmids="
                          + juce::String(lowMidsDb, 1) + " mids="
                          + juce::String(midsDb, 1) + " hmids="
                          + juce::String(highMidsDb, 1) + " pres="
                          + juce::String(presenceDb, 1) + " highs="
                          + juce::String(highsDb, 1) + " air="
                          + juce::String(airDb, 1));

    // Chequeos tonales
    if (now - lastTonalWarningUs_ > kWarningCooldownUs) {
        bool warned = false;

        // ═══ Demasiados graves (sub-bass mucho más alto que mids) ══════════
        if ((subBassDb > -20.0f || lowBassDb > -15.0f)
            && subBassDb > midsDb + 10.0f) {
            warned = true;
            respondWith(
                "🎛️ **Exceso de graves**: el sub-bass ("
                + juce::String(subBassDb, 1) + " dBFS) domina sobre los medios ("
                + juce::String(midsDb, 1) + " dBFS). Prueba un HPF en el bajo "
                "alrededor de 40-60 Hz o reduce el nivel del sub-bass.",
                MentorMessage::Type::Tip);
        }
        // ═══ Faltan graves (bajo mucho más bajo que mids) ══════════════════
        else if (bassDb < -35.0f && lowBassDb < -30.0f && midsDb > -25.0f) {
            warned = true;
            respondWith(
                "🎛️ **Faltan graves**: el rango de bajos ("
                + juce::String(bassDb, 1) + " dBFS) es muy bajo comparado con los medios ("
                + juce::String(midsDb, 1) + " dBFS). "
                "Revisa que el bajo y el bombo tengan presencia en el mezclador.",
                MentorMessage::Type::Info);
        }
        // ═══ Mezcla opaca (poca presencia en highs) ════════════════════════
        else if (presenceDb < -35.0f && highsDb < -40.0f && airDb < -45.0f && midsDb > -25.0f) {
            warned = true;
            respondWith(
                "🎛️ **Mezcla opaca**: hay poca energía en frecuencias altas ("
                + juce::String(highsDb, 1) + " dBFS). "
                "Prueba un realce suave de EQ shelving en 8-12 kHz o agrega "
                "aire con un excitador armónico.",
                MentorMessage::Type::Tip);
        }
        // ═══ Demasiados agudos (presencia domina) ══════════════════════════
        else if (presenceDb > -15.0f && presenceDb > midsDb + 8.0f) {
            warned = true;
            respondWith(
                "🎛️ **Exceso de agudos**: la presencia ("
                + juce::String(presenceDb, 1) + " dBFS) domina la mezcla. "
                "Prueba un filtro low-pass suave en 12-14 kHz o reduce "
                "el nivel de las pistas agudas.",
                MentorMessage::Type::Tip);
        }
        // ═══ Nuevo: Exceso de aire (demasiada energía >10kHz) ══════════════
        else if (airDb > -15.0f && airDb > highMidsDb + 6.0f && presenceDb > -20.0f) {
            warned = true;
            respondWith(
                "🎛️ **Exceso de aire**: la banda de 10-20 kHz ("
                + juce::String(airDb, 1) + " dBFS) es muy prominente. "
                "Reduce el shelving de alta frecuencia o aplica un low-pass "
                "suave en 16-18 kHz para evitar fatiga auditiva.",
                MentorMessage::Type::Tip);
        }

        if (warned)
            lastTonalWarningUs_ = now;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  DINÁMICA — Crest factor, LUFS, loudness range
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::analyzeDynamicsReal()
{
    auto now = juce::Time::getMillisecondCounter() * 1000;
    auto& registry = sharedData_.getSlotRegistry();

    float avgCrestFactor = 0.0f;
    int crestCount = 0;
    int lowCrestCount = 0;
    int highCrestCount = 0;
    float minLufsMomentary = -100.0f;
    float maxLufsMomentary = -100.0f;

    registry.forEachActive([&](const SlotInfo& info) {
        auto telem = getLatestTelemetry(info.slotIndex);
        if (telem.timestamp == 0) return;

        auto& state = trackStates_[info.slotIndex];
        juce::String trackName = juce::String(info.trackName).trim();
        if (trackName.isEmpty()) trackName = "Pista " + juce::String(info.slotIndex + 1);

        // ─── Crest Factor ─────────────────────────────────────────────────
        if (telem.crestFactor > 0.0f && telem.rmsLeft > -40.0f) {
            avgCrestFactor += telem.crestFactor;
            crestCount++;

            state.lastCrestFactor = telem.crestFactor;

            if (telem.crestFactor < 6.0f) {
                lowCrestCount++;
                if (now - lastCrestWarningUs_ > kWarningCooldownUs
                    && now - state.lastWarningUs > kTrackCooldownUs) {
                    state.lastWarningUs = now;
                    lastCrestWarningUs_ = now;
                    respondWithContext(
                        "⚡ **" + trackName + "** tiene poca dinámica (crest factor: "
                        + juce::String(telem.crestFactor, 1) + " dB). "
                        "Si está comprimida, prueba con un ratio más bajo (2:1) "
                        "o sube el threshold 2-3 dB.",
                        trackName,
                        MentorMessage::Type::Tip);
                }
            } else if (telem.crestFactor > 24.0f) {
                highCrestCount++;
                if (now - lastCrestWarningUs_ > kWarningCooldownUs
                    && now - state.lastWarningUs > kTrackCooldownUs) {
                    state.lastWarningUs = now;
                    lastCrestWarningUs_ = now;
                    respondWithContext(
                        "⚡ **" + trackName + "** tiene mucha dinámica (crest factor: "
                        + juce::String(telem.crestFactor, 1) + " dB). "
                        "Un compresor con ratio 4:1 y attack rápido (~10ms) "
                        "puede ayudar a controlar los picos.",
                        trackName,
                        MentorMessage::Type::Tip);
                }
            }
        }

        // ─── LUFS ─────────────────────────────────────────────────────────
        if (telem.lufsMomentary > -80.0f) {
            if (telem.lufsMomentary < minLufsMomentary || minLufsMomentary == -100.0f)
                minLufsMomentary = telem.lufsMomentary;
            if (telem.lufsMomentary > maxLufsMomentary || maxLufsMomentary == -100.0f)
                maxLufsMomentary = telem.lufsMomentary;
        }

        state.lastLufsShort = telem.lufsShortTerm;
        state.lastRmsDb = telem.rmsLeft;
    });

    // ─── Reporte de dinámica global ───────────────────────────────────────
    if (crestCount > 0) {
        float globalCrest = avgCrestFactor / crestCount;

        if (now - lastDynamicWarningUs_ > kWarningCooldownUs) {
            if (globalCrest < 8.0f && lowCrestCount > crestCount / 2) {
                lastDynamicWarningUs_ = now;
                respondWith(
                    "📈 **Mezcla comprimida**: crest factor promedio de "
                    + juce::String(globalCrest, 1) + " dB ("
                    + juce::String(lowCrestCount) + "/" + juce::String(crestCount)
                    + " pistas con poca dinámica). "
                    "Revisa los compresores — podrías estar sobre-comprimiendo.",
                    MentorMessage::Type::Warning);
            } else if (globalCrest > 18.0f && highCrestCount > crestCount / 3) {
                lastDynamicWarningUs_ = now;
                respondWith(
                    "📈 **Mezcla muy dinámica**: crest factor promedio de "
                    + juce::String(globalCrest, 1) + " dB. "
                    "Considera compresores suaves en las pistas más dinámicas "
                    "para nivelar la mezcla.",
                    MentorMessage::Type::Info);
            }
        }

        // ─── Reporte de LUFS (si hay datos) ──────────────────────────────
        if (maxLufsMomentary > -30.0f && now - lastLoudnessWarningUs_ > kWarningCooldownUs) {
            lastLoudnessWarningUs_ = now;
            float range = maxLufsMomentary - minLufsMomentary;
            respondWith(
                "🔊 **Rango de loudness**: " + juce::String(range, 1)
                + " LU (de " + juce::String(minLufsMomentary, 1)
                + " a " + juce::String(maxLufsMomentary, 1)
                + " LUFS). Para mezcla balanceada, busca que las pistas "
                "tengan loudness similar, con máximo 8-10 LU de diferencia.",
                MentorMessage::Type::Info);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  FASE / ESPACIAL — Correlación, panoramas, fase estéreo
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::analyzePhaseReal()
{
    auto now = juce::Time::getMillisecondCounter() * 1000;
    auto& registry = sharedData_.getSlotRegistry();

    int phaseIssueTracks = 0;
    juce::String phaseTracks;

    registry.forEachActive([&](const SlotInfo& info) {
        auto telem = getLatestTelemetry(info.slotIndex);
        if (telem.timestamp == 0) return;

        juce::String trackName = juce::String(info.trackName).trim();
        if (trackName.isEmpty()) trackName = "Pista " + juce::String(info.slotIndex + 1);

        // ─── Correlación (mono compatibility check) ───────────────────────
        auto& state = trackStates_[info.slotIndex];
        float prevCorrelation = state.lastCorrelation;
        state.lastCorrelation = telem.correlation;

        if (telem.correlation < 0.3f && telem.rmsLeft > -30.0f) {
            phaseIssueTracks++;
            if (!phaseTracks.isEmpty()) phaseTracks += ", ";
            phaseTracks += trackName;

            // Solo advertir si empeora significativamente (respecto al valor ANTERIOR)
            if ((telem.correlation < prevCorrelation - 0.2f || telem.correlation < 0.0f)
                && now - lastPhaseWarningUs_ > kWarningCooldownUs) {
                lastPhaseWarningUs_ = now;
                juce::String msg;
                if (telem.correlation < 0.0f) {
                    msg = "🔮 **" + trackName + "** tiene correlación negativa ("
                        + juce::String(telem.correlation, 2) + ") — "
                        "las fases están invertidas. Revisa los micrófonos "
                        "o el procesado estéreo.";
                } else {
                    msg = "🔮 **" + trackName + "** tiene baja correlación ("
                        + juce::String(telem.correlation, 2) + "). "
                        "Si la pista suena hueca al monitorear en mono, "
                        "revisa el panorama o efectos estéreo.";
                }
                respondWithContext(msg, trackName, MentorMessage::Type::Warning);
            }
        }
    });

    // Resumen global de fase
    if (phaseIssueTracks >= 3) {
        respondWith(
            "🔮 Se detectaron problemas de fase en **" + juce::String(phaseIssueTracks)
            + " pistas**. Revisa la correlación en el panel de análisis "
            "y considera usar correladores de fase.",
            MentorMessage::Type::Info);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  ANÁLISIS GLOBAL — Resumen del estado general de la mezcla
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::analyzeOverallMixReal()
{
    auto& registry = sharedData_.getSlotRegistry();
    int active = registry.activeCount();

    if (active == 0) return;

    // Recolectar métricas globales
    int clippingCount = 0;
    int nearClipCount = 0;
    int noSignalCount = 0;
    int negativePhaseCount = 0;
    float masterPeakEstimate = -100.0f;
    float masterRmsSum = 0.0f;
    int rmsCount = 0;

    registry.forEachActive([&](const SlotInfo& info) {
        auto telem = getLatestTelemetry(info.slotIndex);
        if (telem.timestamp == 0) return;

        float peak = juce::jmax(telem.peakLeft, telem.peakRight);
        masterPeakEstimate = juce::jmax(masterPeakEstimate, peak);

        if (peak > -0.5f) clippingCount++;
        else if (peak > -3.0f) nearClipCount++;

        if (peak < -60.0f) noSignalCount++;

        if (telem.correlation < 0.0f) negativePhaseCount++;

        if (telem.rmsLeft > -60.0f) {
            masterRmsSum += (telem.rmsLeft + telem.rmsRight) * 0.5f;
            rmsCount++;
        }
    });

    // Log de diagnóstico periódico
    LogHelper::writeToLog("[CoachEngine] OverallMix: "
                          + juce::String(active) + " tracks, "
                          + juce::String(clippingCount) + " clipping, "
                          + juce::String(nearClipCount) + " near-clip, "
                          + juce::String(noSignalCount) + " silent, "
                          + "peak=" + juce::String(masterPeakEstimate, 1) + " dB");
}

// ═══════════════════════════════════════════════════════════════════════════
//  MANEJO DE MENSAJES DEL USUARIO
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::handleUserMessage(const juce::String& message)
{
    auto lower = message.toLowerCase();

    // Comandos de sistema
    if (lower.startsWith("/")) {
        executeCommand(message);
        return;
    }

    // ═══ RUTEO AL LLM (lenguaje natural) ═══════════════════════════════
    // Si el LLM está habilitado y hay callback de streaming, enviamos
    // el mensaje al LLM para que responda con lenguaje natural.
    //
    // El flujo es:
    //   1. LLM streaming callback → onToken (token por token) + onComplete
    //   2. El streaming callback llama a startStreamingMessage()/appendStreamingToken()
    //      en el chat UI via los callbacks setStreamingCallbacks()
    //   3. Si el LLM responde, se renderiza como burbuja premium
    //   4. Si el LLM no está disponible (timeout, error), cae a respuestas por reglas
    //
    // Setup dialogue (FASE 0) usa sus propios mensajes por reglas — no se rutea al LLM.
    if (llmEnabled_
        && setupStep_ == SetupStep::Complete
        && llmStreamingCallback_)
    {
        // Primero disparar el streaming callback
        bool accepted = llmStreamingCallback_(
            message,
            [this](const juce::String& token)
            {
                // Cada token → entregar a la UI via streaming callbacks
                if (streamTokenCb_)
                    streamTokenCb_(token);
            },
            [this](const juce::String& response)
            {
                // Streaming completado — notificar fin
                if (streamEndedCb_)
                    streamEndedCb_();

                // Log de la respuesta
                LogHelper::writeToLog("[CoachEngine] LLM streaming completado ("
                                      + juce::String(response.length()) + " chars)");
            });

        if (accepted)
        {
            // Notificar inicio de streaming a la UI (crea burbuja vacía)
            if (streamStartedCb_)
                streamStartedCb_();

            // Marcar interacción del usuario (resetea timer de inactividad)
            setUserInteracted();
            return;
        }
        // Si el callback retornó false, caemos a respuestas por reglas
    }

    // ═══ Fallback: LLM sin streaming (no preferido, pero funcional) ═════
    if (llmEnabled_
        && setupStep_ == SetupStep::Complete
        && llmResponseCallback_)
    {
        bool accepted = llmResponseCallback_(
            message,
            [this](const juce::String& response)
            {
                // La respuesta completa se entrega como burbuja premium
                respondWithLLM(response);
            });

        if (accepted)
        {
            setUserInteracted();
            return;
        }
    }

    // ─── FALLBACK: Respuesta contextual según la fase (por reglas) ─────
    auto phase = phaseManager_.getCurrentPhase();

    // Si hay telemetría y pistas activas, responder con datos reales
    auto& registry = sharedData_.getSlotRegistry();
    bool hasTelemetry = (registry.activeCount() > 0);

    switch (phase) {
        

        case MentorPhase::GainStaging:
            if (hasTelemetry) {
                analyzeGainStagingReal();
                respondWith(
                    "📊 Puedes preguntar \"cómo están los niveles\" o "
                    "\"hay clipping?\" para un análisis detallado.",
                    MentorMessage::Type::Info);
            } else {
                respondWith(
                    "Aún no detecto pistas activas. Asegúrate de tener "
                    "Messengers cargados en tus pistas.",
                    MentorMessage::Type::Info);
            }
            break;

        case MentorPhase::Organizacion:
            if (hasTelemetry) {
                analyzeOrganisationReal();
            } else {
                respondWith(
                    "En fase de Organización. Recomiendo:\n"
                    "1. Nombra cada pista descriptivamente\n"
                    "2. Asigna colores por familia\n"
                    "3. Agrupa en buses virtuales",
                    MentorMessage::Type::Tip);
            }
            break;

        case MentorPhase::Balance:
            if (hasTelemetry) {
                analyzeTonalBalanceReal();
            } else {
                respondWith(
                    "En fase de Balance Tonal. Revisa el espectro y "
                    "compáralo con referencias de tu género.",
                    MentorMessage::Type::Info);
            }
            break;

        case MentorPhase::Compresion:
            if (hasTelemetry) {
                analyzeDynamicsReal();
            } else {
                respondWith(
                    "En fase de Dinámica. Considera compresores en buses "
                    "y limitador en el master (solo 1-2 dB).",
                    MentorMessage::Type::Tip);
            }
            break;

        case MentorPhase::Espacio:
            if (hasTelemetry) {
                analyzePhaseReal();
            }
            respondWith(
                "En fase de Espacialidad. Trabaja panoramas, reverbs, "
                "y efectos de profundidad.",
                MentorMessage::Type::Tip);
            break;
    }

    // Verificar logros
    auto activeCount = registry.activeCount();
    if (activeCount >= 1)  phaseManager_.unlockAchievement(Achievement::FirstTrack);
    if (activeCount >= 5)  phaseManager_.unlockAchievement(Achievement::FiveTracks);
    if (activeCount >= 10) phaseManager_.unlockAchievement(Achievement::TenTracks);
}

// ═══════════════════════════════════════════════════════════════════════════
//  TIP PROACTIVO — Genera un tip basado en la fase y datos actuales
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::generateProactiveTip()
{
    auto& registry = sharedData_.getSlotRegistry();
    int active = registry.activeCount();

    if (active == 0) {
        respondWith(
            "💡 Aún no hay pistas activas. Carga Messengers en tus pistas "
            "para empezar a recibir análisis en tiempo real.",
            MentorMessage::Type::Tip);
        return;
    }

    // Si hay datos, dar tip basado en la fase y métricas reales
    auto phase = phaseManager_.getCurrentPhase();
    switch (phase) {
        case MentorPhase::GainStaging:
            analyzeGainStagingReal();
            if (juce::Time::getMillisecondCounter() * 1000 - lastPeakWarningUs_
                > kWarningCooldownUs) {
                respondWith(
                    "💡 Tip rápido: revisa que el fader de ganancia de cada pista "
                    "permita picos de -18 dB a -12 dB en el submix antes de "
                    "tocar el fader de volumen.",
                    MentorMessage::Type::Tip);
            }
            break;

        case MentorPhase::Organizacion:
            analyzeOrganisationReal();
            break;

        case MentorPhase::Balance:
            respondWith(
                "💡 Para evaluar el balance tonal, revisa el Analyzer "
                "(pestaña 2). Busca una curva suave de menos de 3 dB/octava "
                "de diferencia entre bandas adyacentes.",
                MentorMessage::Type::Tip);
            analyzeTonalBalanceReal();
            break;

        case MentorPhase::Compresion:
            analyzeDynamicsReal();
            break;

        case MentorPhase::Espacio:
            analyzePhaseReal();
            break;

        default:
            respondWith(
                "💡 Escribe /help para ver comandos disponibles o pregúntame "
                "\"cómo va la mezcla?\" para un análisis completo.",
                MentorMessage::Type::Tip);
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  PROGRESO Y LOGROS
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::checkProgress()
{
    auto phase = phaseManager_.getCurrentPhase();
    auto progress = phaseManager_.getPhaseProgress(phase);
    auto& registry = sharedData_.getSlotRegistry();

    respondWith(
        juce::String("📈 **Progreso** en fase '")
        + phaseManager_.phaseDescription(phase) + "': "
        + juce::String(static_cast<int>(progress * 100.0f)) + "%\n"
        + "Pistas activas: " + juce::String(registry.activeCount()) + "\n"
        + "Logros: " + juce::String(phaseManager_.getAchievementCount()),
        MentorMessage::Type::Info);

    if (phaseManager_.isPhaseComplete(phase)) {
        respondWith(
            "🎉 ¡Has completado esta fase! Escribe **/next** para avanzar "
            "a la siguiente.",
            MentorMessage::Type::Achievement);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  ANUNCIO DE NUEVA PISTA (desde Messenger)
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::announceNewTrack(int slotIndex, const juce::String& trackName,
                                   const juce::Colour& colour)
{
    juce::ignoreUnused(slotIndex, colour);

    auto& registry = sharedData_.getSlotRegistry();
    auto telem = getLatestTelemetry(slotIndex);

    juce::String msg = "🎛️ **Nuevo Messenger detectado:** " + trackName.trim() + "\n\n";

    // Si aún no hay telemetría, mostrar mensaje genérico
    if (telem.timestamp == 0) {
        msg += "Esperando datos de telemetría... (aparecerán en unos segundos)\n";
    } else {
        msg += "Niveles actuales:\n";
        msg += "• Peak: **" + juce::String(telem.peakLeft, 1) + " dB**\n";
        msg += "• RMS: **" + juce::String(telem.rmsLeft, 1) + " dB**\n";
    }

    if (telem.crestFactor > 0.0f)
        msg += "• Crest factor: **" + juce::String(telem.crestFactor, 1) + " dB**\n";

    if (telem.correlation < 1.0f)
        msg += "• Correlación: **" + juce::String(telem.correlation, 2) + "**\n";

    if (registry.activeCount() >= 3) {
        msg += "\n📊 Ya tienes **" + juce::String(registry.activeCount())
               + " pistas** activas — la mezcla empieza a tomar forma.";
    }

    respondWith(msg, MentorMessage::Type::Info);

    // Si estamos en Welcome, avanzar automáticamente a GainStaging
    if (phaseManager_.getCurrentPhase() == MentorPhase::Organizacion) {
        respondWith(
            "🚀 ¡Excelente! Como ya tienes pistas, pasamos directo a "
            "**Gain Staging** para ajustar niveles.",
            MentorMessage::Type::Tip);
        phaseManager_.advanceToNextPhase();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  ENMASCARAMIENTO ESPECTRAL — Pares de pistas que compiten en frecuencia
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::analyzeSpectralMaskingReal()
{
    auto now = juce::Time::getMillisecondCounter() * 1000;
    auto& registry = sharedData_.getSlotRegistry();

    if (registry.activeCount() < 2)
        return;

    // ─── Recolectar espectro de todas las pistas activas ──────────────────
    struct TrackSpec {
        int slotIndex;
        juce::String name;
        float spectrum[kNumSpectrumBins];
        float rmsDb;
    };

    std::vector<TrackSpec> tracks;
    registry.forEachActive([&](const SlotInfo& info) {
        auto telem = getLatestTelemetry(info.slotIndex);
        if (telem.timestamp == 0) return;

        float sum = 0.0f;
        for (int fi = 0; fi < kNumSpectrumBins; ++fi)
            sum += telem.spectrum[fi];
        if (sum < 0.01f || telem.rmsLeft < -50.0f) return;

        TrackSpec ts;
        ts.slotIndex = info.slotIndex;
        ts.name = juce::String(info.trackName).trim();
        if (ts.name.isEmpty())
            ts.name = "Pista " + juce::String(info.slotIndex + 1);
        for (int fi = 0; fi < kNumSpectrumBins; ++fi)
            ts.spectrum[fi] = telem.spectrum[fi];
        ts.rmsDb = telem.rmsLeft;
        tracks.push_back(ts);
    });

    if (tracks.size() < 2) return;

    // ─── Bandas críticas (escala Bark simplificada para 1024-FFT @ 48kHz) ──
    // Cada bin del FFT = 48000/1024 = ~46.875 Hz
    struct MaskBand {
        const char* name;
        int binStart; // inclusive
        int binEnd;   // exclusive
    };

    const MaskBand bands[] = {
        { "sub-graves (20-70 Hz)",       0,   1 },
        { "graves bajos (70-150 Hz)",    1,   3 },
        { "graves (150-300 Hz)",         3,   6 },
        { "medios bajos (300-600 Hz)",   6,  13 },
        { "medios (600-1.2 kHz)",       13,  26 },
        { "medios altos (1.2-2.5 kHz)", 26,  53 },
        { "presencia (2.5-5 kHz)",      53, 106 },
        { "presencia alta (5-10 kHz)", 106, 213 },
        { "agudos (10-16 kHz)",        213, 341 },
        { "aire (16-20 kHz)",          341, 426 },
    };
    constexpr int kNumBands = sizeof(bands) / sizeof(bands[0]);

    // ─── Comparación pairwise ─────────────────────────────────────────────
    struct MaskPair {
        int idxA, idxB;
        float overlapScore;
        int worstBand;
        float energyA, energyB;
        juce::String bandName;
    };

    std::vector<MaskPair> pairs;

    auto toDb = [](float v) -> float {
        return (v > 0.001f) ? juce::Decibels::gainToDecibels(v) : -80.0f;
    };

    for (size_t i = 0; i < tracks.size(); ++i) {
        for (size_t j = i + 1; j < tracks.size(); ++j) {
            float totalOverlap = 0.0f;
            int worstBand = -1;
            float worstDiff = 0.0f;
            float worstEnergyA = 0.0f, worstEnergyB = 0.0f;
            juce::String worstBandName;

            for (int b = 0; b < kNumBands; ++b) {
                // Energía promedio de cada pista en esta banda
                float energyA = 0.0f, energyB = 0.0f;
                int binStart = bands[b].binStart;
                int binEnd = juce::jmin(bands[b].binEnd, kNumSpectrumBins);
                int count = binEnd - binStart;

                for (int bi = binStart; bi < binEnd; ++bi) {
                    energyA += tracks[i].spectrum[bi];
                    energyB += tracks[j].spectrum[bi];
                }
                if (count > 0) {
                    energyA /= (float)count;
                    energyB /= (float)count;
                }

                // Ambas deben tener energía significativa (> -40 dBFS)
                if (energyA > 0.01f && energyB > 0.01f) {
                    float bandOverlap = juce::jmin(energyA, energyB)
                                      * juce::jmax(energyA, energyB) * 10.0f;
                    totalOverlap += bandOverlap;

                    float diffDb = std::fabs(toDb(energyA) - toDb(energyB));
                    if (diffDb > worstDiff) {
                        worstDiff = diffDb;
                        worstBand = b;
                        worstEnergyA = energyA;
                        worstEnergyB = energyB;
                        worstBandName = bands[b].name;
                    }
                }
            }

            // Umbral: overlap suficiente para considerar enmascaramiento
            if (totalOverlap > 0.03f && worstBand >= 0) {
                MaskPair mp;
                mp.idxA = (int)i;
                mp.idxB = (int)j;
                mp.overlapScore = totalOverlap;
                mp.worstBand = worstBand;
                mp.energyA = worstEnergyA;
                mp.energyB = worstEnergyB;
                mp.bandName = worstBandName;
                pairs.push_back(mp);
            }
        }
    }

    if (pairs.empty())
        return;

    // ─── Ordenar por overlap (más crítico primero) ────────────────────────
    std::sort(pairs.begin(), pairs.end(), [](const MaskPair& a, const MaskPair& b) {
        return a.overlapScore > b.overlapScore;
    });

    // ─── Reportar top maskings ────────────────────────────────────────────
    if (now - lastMaskingWarningUs_ > kWarningCooldownUs) {
        lastMaskingWarningUs_ = now;

        int numToReport = juce::jmin((int)pairs.size(), 3);
        for (int p = 0; p < numToReport; ++p) {
            auto& mp = pairs[p];
            auto& trackA = tracks[mp.idxA];
            auto& trackB = tracks[mp.idxB];

            float aDb = toDb(mp.energyA);
            float bDb = toDb(mp.energyB);
            juce::String context = trackA.name + " / " + trackB.name;
            juce::String advice;

            if (aDb > bDb + 6.0f) {
                // A enmascara a B
                advice = "🔊 **" + trackA.name + "** enmascara a **" + trackB.name
                       + "** en " + mp.bandName + "\n"
                       + "    " + trackA.name + ": **" + juce::String(aDb, 1) + " dBFS**, "
                       + trackB.name + ": **" + juce::String(bDb, 1) + " dBFS**\n"
                       + "    💡 Prueba: reducir " + juce::String(3) + " dB en " + mp.bandName
                       + " de " + trackB.name + ", o aplica sidechain dinámico.";
            } else if (bDb > aDb + 6.0f) {
                advice = "🔊 **" + trackB.name + "** enmascara a **" + trackA.name
                       + "** en " + mp.bandName + "\n"
                       + "    " + trackB.name + ": **" + juce::String(bDb, 1) + " dBFS**, "
                       + trackA.name + ": **" + juce::String(aDb, 1) + " dBFS**\n"
                       + "    💡 Prueba: reducir " + juce::String(3) + " dB en " + mp.bandName
                       + " de " + trackA.name + ", o ajusta panoramas para separarlas.";
            } else {
                advice = "🔊 **" + trackA.name + "** y **" + trackB.name
                       + "** compiten en " + mp.bandName + "\n"
                       + "    " + trackA.name + ": **" + juce::String(aDb, 1) + " dBFS**, "
                       + trackB.name + ": **" + juce::String(bDb, 1) + " dBFS**\n"
                       + "    💡 Considera EQ carving (" + mp.bandName + ") "
                       + "o panoramas opuestos para separarlas.";
            }

            respondWithContext(advice, context, MentorMessage::Type::Tip);
        }

        if ((int)pairs.size() > numToReport) {
            respondWith(
                "🔊 **" + juce::String((int)pairs.size() - numToReport)
                + " par(es) adicional(es)** con enmascaramiento. "
                "Usa analizador espectral (pestaña 2) para identificar conflictos.",
                MentorMessage::Type::Info);
        }
    }

    // Log de diagnóstico
    LogHelper::writeToLog("[CoachEngine] SpectralMasking: "
                          + juce::String((int)pairs.size()) + " pares, "
                          + juce::String((int)tracks.size()) + " tracks");
}

// ═══════════════════════════════════════════════════════════════════════════
//  LEGACY (mantenidos por compatibilidad con llamadas existentes)
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::analyzeGainStaging()
{
    analyzeGainStagingReal();
}

void CoachEngine::analyzeOrganisation()
{
    analyzeOrganisationReal();
}

void CoachEngine::analyzeTonalBalance()
{
    analyzeTonalBalanceReal();
}

void CoachEngine::analyzeDynamics()
{
    analyzeDynamicsReal();
}

void CoachEngine::analyzeSpatial()
{
    analyzePhaseReal();
}

// ═══════════════════════════════════════════════════════════════════════════
//  TrackType ↔ TrackRole mapping (Feedback Loop V9)
// ═══════════════════════════════════════════════════════════════════════════
TrackRole getTrackRoleForTrackType(TrackType type) noexcept
{
    switch (type) {
        // ─── Drums ──────────────────────────────────────────────────────
        case TrackType::Kick:          return TrackRole::Kick;
        case TrackType::ReggaetonKick: return TrackRole::ReggaetonKick;
        case TrackType::Snare:         return TrackRole::Snare;
        case TrackType::HiHat:         return TrackRole::HiHat;
        case TrackType::Tom:           return TrackRole::Tom;
        case TrackType::Percussion:    return TrackRole::Percussion;
        case TrackType::Overheads:     return TrackRole::DrumBus;
        case TrackType::Room:          return TrackRole::DrumRoom;
        // ─── Bass ───────────────────────────────────────────────────────
        case TrackType::BassDI:        return TrackRole::BassFinger;
        case TrackType::BassMic:       return TrackRole::BassPick;
        case TrackType::Bass808:       return TrackRole::Bass808;
        case TrackType::Sub:           return TrackRole::BassSub;
        // ─── Melody ─────────────────────────────────────────────────────
        case TrackType::Piano:         return TrackRole::KeysPiano;
        case TrackType::Guitar:        return TrackRole::GuitarElectric;
        case TrackType::SynthLead:     return TrackRole::SynthLead;
        case TrackType::SynthPad:      return TrackRole::SynthPad;
        case TrackType::Strings:       return TrackRole::Strings;
        // ─── Vocals ─────────────────────────────────────────────────────
        case TrackType::LeadVocal:     return TrackRole::VozPrincipal;
        case TrackType::DoubleVocal:   return TrackRole::VozFondo;
        case TrackType::Adlibs:        return TrackRole::Adlibs;
        case TrackType::Chorus:        return TrackRole::VozFondo;
        // ─── FX ─────────────────────────────────────────────────────────
        case TrackType::Risers:        return TrackRole::FxRiser;
        case TrackType::Impacts:       return TrackRole::FxImpact;
        case TrackType::Ambience:      return TrackRole::FxAmbience;
        // ─── None ───────────────────────────────────────────────────────
        default:                       return TrackRole::Unknown;
    }
}

TrackType getTrackTypeForRole(TrackRole role) noexcept
{
    switch (role) {
        // ─── 1:1 mappings ───────────────────────────────────────────────
        case TrackRole::Kick:          return TrackType::Kick;
        case TrackRole::ReggaetonKick: return TrackType::ReggaetonKick;
        case TrackRole::Snare:         return TrackType::Snare;
        case TrackRole::HiHat:         return TrackType::HiHat;
        case TrackRole::Tom:           return TrackType::Tom;
        case TrackRole::Bass808:       return TrackType::Bass808;
        case TrackRole::BassSub:       return TrackType::Sub;
        case TrackRole::SynthLead:     return TrackType::SynthLead;
        case TrackRole::SynthPad:      return TrackType::SynthPad;
        case TrackRole::Strings:       return TrackType::Strings;
        case TrackRole::VozPrincipal:  return TrackType::LeadVocal;
        case TrackRole::Adlibs:        return TrackType::Adlibs;
        case TrackRole::FxRiser:       return TrackType::Risers;
        case TrackRole::FxImpact:      return TrackType::Impacts;
        case TrackRole::FxAmbience:    return TrackType::Ambience;
        // ─── N:1 — many roles -> same TrackType ─────────────────────────
        case TrackRole::Kick808:       return TrackType::Kick;
        case TrackRole::SnareTrap:     return TrackType::Snare;
        case TrackRole::HiHatOpen:     return TrackType::HiHat;
        case TrackRole::TomFloor:      return TrackType::Tom;
        case TrackRole::Ride:
        case TrackRole::Crash:
        case TrackRole::Clap:
        case TrackRole::Percussion:    return TrackType::Percussion;
        case TrackRole::DrumBus:       return TrackType::Overheads;
        case TrackRole::DrumRoom:      return TrackType::Room;
        case TrackRole::BassFinger:    return TrackType::BassDI;
        case TrackRole::BassPick:      return TrackType::BassMic;
        case TrackRole::BassSynth:
        case TrackRole::BassBus:       return TrackType::BassDI;
        case TrackRole::GuitarAcoustic:
        case TrackRole::GuitarElectric:
        case TrackRole::GuitarRhythm:
        case TrackRole::GuitarLead:
        case TrackRole::GuitarBus:     return TrackType::Guitar;
        case TrackRole::KeysPiano:
        case TrackRole::KeysElectric:
        case TrackRole::KeysOrgan:
        case TrackRole::KeysBus:
        case TrackRole::MelodyBus:     return TrackType::Piano;
        case TrackRole::SynthPluck:    return TrackType::SynthLead;
        case TrackRole::VozFondo:      return TrackType::DoubleVocal;
        case TrackRole::VozDouble:     return TrackType::DoubleVocal;
        case TrackRole::VozBus:        return TrackType::LeadVocal;
        case TrackRole::FxNoise:       return TrackType::Ambience;
        // ─── Unknown / None ─────────────────────────────────────────────
        case TrackRole::Master:
        case TrackRole::Winds:
        default:                       return TrackType::None;
    }
}


// ═══════════════════════════════════════════════════════════════════════════
//  INFERENCIA DE ROL POR NOMBRE — Lookup Table optimizada
// ═══════════════════════════════════════════════════════════════════════════

CoachEngine::NameInferenceResult CoachEngine::inferTrackRoleFromName(
    const juce::String& trackName) noexcept
{
    NameInferenceResult result;

    auto kw = NameInferrer::detectKeywords(trackName);
    if (kw.isEmpty)
        return result;

        // Rules evaluated in priority order -- first match wins
    // Replaces lambda-based lookup table for MSVC compatibility
    auto& k = kw;

    // KICK
    if (k.has808Kick) { result = { TrackRole::Kick808, 0.95f, true }; return result; }
    if (k.hasKick)    { result = { TrackRole::Kick, 0.90f, true }; return result; }

    // 808 BASS
    if (k.has808Bass || (k.has808 && k.hasSub)) { result = { TrackRole::Bass808, 0.90f, true }; return result; }
    if (k.has808 && !k.hasDrum)                 { result = { TrackRole::Bass808, 0.75f, true }; return result; }

    // HIHAT
    if (k.hasOpenHH) { result = { TrackRole::HiHatOpen, 0.90f, true }; return result; }
    if (k.hasHiHat)  { result = { TrackRole::HiHat, 0.90f, true }; return result; }

    // SNARE
    if (k.hasSnare && k.hasTrap) { result = { TrackRole::SnareTrap, 0.90f, true }; return result; }
    if (k.hasSnare)              { result = { TrackRole::Snare, 0.90f, true }; return result; }
    if (k.hasRim)                { result = { TrackRole::Snare, 0.80f, true }; return result; }

    // TOM
    if (k.hasTom && (k.normalizedName.contains("floor") || k.normalizedName.contains("piso") || k.normalizedName.contains("suelo")))
                                 { result = { TrackRole::TomFloor, 0.90f, true }; return result; }
    if (k.hasTom)                { result = { TrackRole::Tom, 0.85f, true }; return result; }

    // REGGAETON / CLAP
    if (k.hasReggaeton && k.hasKick) { result = { TrackRole::ReggaetonKick, 0.85f, true }; return result; }
    if (k.hasReggaeton && k.hasDrum) { result = { TrackRole::ReggaetonKick, 0.70f, true }; return result; }
    if (k.hasClap)                   { result = { TrackRole::Clap, 0.90f, true }; return result; }

    // RIDE / CRASH
    if (k.hasRide)  { result = { TrackRole::Ride, 0.85f, true }; return result; }
    if (k.hasCrash) { result = { TrackRole::Crash, 0.85f, true }; return result; }

    // BASS / SUB
    if (k.hasSub && k.hasBass)         { result = { TrackRole::BassSub, 0.85f, true }; return result; }
    if (k.hasFinger && k.hasBass)      { result = { TrackRole::BassFinger, 0.85f, true }; return result; }
    if (k.hasPick && k.hasBass)        { result = { TrackRole::BassPick, 0.85f, true }; return result; }
    if (k.hasSynth && k.hasBass)       { result = { TrackRole::BassSynth, 0.80f, true }; return result; }

    // GUITAR
    if (k.hasAcoustic && k.hasGuitar)  { result = { TrackRole::GuitarAcoustic, 0.90f, true }; return result; }
    if (k.hasLead && k.hasGuitar)      { result = { TrackRole::GuitarLead, 0.85f, true }; return result; }
    if (k.hasRhythm && k.hasGuitar)    { result = { TrackRole::GuitarRhythm, 0.85f, true }; return result; }
    if (k.hasGuitar)                   { result = { TrackRole::GuitarElectric, 0.75f, true }; return result; }

    // VOCALS
    if (k.hasVozPrincipal) { result = { TrackRole::VozPrincipal, 0.90f, true }; return result; }
    if (k.hasVozFondo)     { result = { TrackRole::VozFondo, 0.85f, true }; return result; }
    if (k.hasAdlib)        { result = { TrackRole::Adlibs, 0.80f, true }; return result; }
    if (k.hasVocal)        { result = { TrackRole::VozPrincipal, 0.70f, true }; return result; }

    // SYNTHS & KEYS
    if (k.hasPad)              { result = { TrackRole::SynthPad, 0.85f, true }; return result; }
    if (k.hasPluck)            { result = { TrackRole::SynthPluck, 0.85f, true }; return result; }
    if (k.hasLead && k.hasSynth) { result = { TrackRole::SynthLead, 0.85f, true }; return result; }
    if (k.hasOrgan)            { result = { TrackRole::KeysOrgan, 0.85f, true }; return result; }
    if (k.hasPiano)            { result = { TrackRole::KeysPiano, 0.85f, true }; return result; }
    if (k.hasSynth)            { result = { TrackRole::SynthLead, 0.70f, true }; return result; }

    // STRINGS / BRASS
    if (k.hasStrings) { result = { TrackRole::Strings, 0.85f, true }; return result; }
    if (k.hasBrass)   { result = { TrackRole::Brass, 0.85f, true }; return result; }

    // PERCUSSION
    if (k.hasPerc)    { result = { TrackRole::Percussion, 0.75f, true }; return result; }

    // FX / RISER / AMBIENT
    if (k.hasRiser)   { result = { TrackRole::FxRiser, 0.85f, true }; return result; }
    if (k.hasAmbient) { result = { TrackRole::FxAmbience, 0.80f, true }; return result; }
    if (k.hasNoise)   { result = { TrackRole::FxNoise, 0.80f, true }; return result; }
    if (k.hasFx)      { result = { TrackRole::FxAmbience, 0.65f, true }; return result; }

    // DRUM BUS / MASTER / BUS
    if (k.hasDrum && k.hasBus) { result = { TrackRole::DrumBus, 0.90f, true }; return result; }
    if (k.hasBus && k.hasBass) { result = { TrackRole::BassBus, 0.80f, true }; return result; }
    if (k.hasMaster)           { result = { TrackRole::Master, 0.95f, true }; return result; }

    // EXTENDED PATTERNS
    if (k.hasWinds)               { result = { TrackRole::Winds, 0.75f, true }; return result; }
    if (k.hasViolin)              { result = { TrackRole::Strings, 0.65f, true }; return result; }
    if (k.hasSample)              { result = { TrackRole::FxAmbience, 0.45f, true }; return result; }
    if (k.hasIntro)               { result = { TrackRole::FxRiser, 0.60f, true }; return result; }
    if (k.hasFill)                { result = { TrackRole::Percussion, 0.55f, true }; return result; }
    if (k.hasMelody || k.hasArp)  { result = { TrackRole::SynthLead, 0.50f, true }; return result; }
    if (k.hasClick)               { result = { TrackRole::Percussion, 0.50f, true }; return result; }
    // Drum room mic ("Room OH", "Drum Room") → DrumRoom, no ambient FX
    if (k.hasRoomMic)             { result = { TrackRole::DrumRoom, 0.85f, true }; return result; }
    // "Room" genérico sin contexto de batería → ambient pad
    if (k.hasRoom)                { result = { TrackRole::FxAmbience, 0.60f, true }; return result; }

    // No rule matched
    result.role = TrackRole::Unknown;
    result.confidence = 0.0f;
    result.fromName = false;
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
//  inferTrackRoleCombined
// ═══════════════════════════════════════════════════════════════════════════

CoachEngine::NameInferenceResult CoachEngine::inferTrackRoleCombined(
    const juce::String& trackName,
    const TrackSpectralProfile& spectral) noexcept
{
    auto nameResult = inferTrackRoleFromName(trackName);
    if (nameResult.isValid() && nameResult.confidence >= 0.70f)
        return nameResult;

    if (spectral.hasData()) {
        TrackRole spectralRole = SpectralProfiler::inferTrackRole(spectral);
        if (spectralRole != TrackRole::Unknown) {
            if (nameResult.isValid() && nameResult.confidence >= 0.40f
                && nameResult.role == spectralRole) {
                NameInferenceResult combined;
                combined.role = spectralRole;
                combined.confidence = 0.80f;
                combined.fromName = nameResult.fromName;
                return combined;
            }
            NameInferenceResult spectralResult;
            spectralResult.role = spectralRole;
            spectralResult.confidence = 0.65f;
            spectralResult.fromName = false;
            return spectralResult;
        }
    }

    if (nameResult.isValid())
        return nameResult;
    return nameResult;
}


// ═══════════════════════════════════════════════════════════════════════════
//  inferTrackRoles — Itera slots activos, infiere roles combinados
// ═══════════════════════════════════════════════════════════════════════════
void CoachEngine::inferTrackRoles()
{
    auto& registry = sharedData_.getSlotRegistry();
    int rolesInf = 0, nameInf = 0, spectralInf = 0, explicitInf = 0;

    // ─── Signal-order inference (V13): detecta primeras pistas con señal
    //     y asigna roles por orden típico (kick, snare, hihat, voz, bass...).
    //     Solo aplica a pistas con nombres genéricos ("Pista X", "Track X").
    //     Antes era código muerto — ahora se activa cada 8s desde
    //     periodicAnalysis() durante la fase Organización.
    detectFirstSignal();
    inferBySilenceOrder();

    registry.forEachActive([&](const SlotInfo& info) {
        int idx = info.slotIndex;
        if (idx < 0 || idx >= SlotRegistry::kMaxSlots)
            return;
        TrackRole cur = trackRoles_[idx];
        if (cur != TrackRole::Unknown && cur != TrackRole::Master)
            return;

        juce::String trackName = juce::String(info.trackName).trim();

        // 1. TrackType explicito del Messenger
        if (info.trackType >= 0 && info.trackType != (int)TrackType::None) {
            TrackRole explicitRole = getTrackRoleForTrackType((TrackType)info.trackType);
            if (explicitRole != TrackRole::Unknown && explicitRole != TrackRole::Master) {
                trackRoles_[idx] = explicitRole;
                trackRoleWasInferred_[idx] = true;
                trackRoleConfirmed_[idx] = false;  // Sprint 1: inferred, not yet confirmed
                rolesInf++; explicitInf++;
                return;
            }
        }

        // 2. Inferencia combinada nombre + espectral
        TrackSpectralProfile spectral;
        auto telem = getLatestTelemetry(idx);
        if (telem.timestamp > 0)
            spectral = SpectralProfiler::computeProfile(telem);

        auto combined = inferTrackRoleCombined(trackName, spectral);
        if (combined.isValid()) {
            trackRoles_[idx] = combined.role;
            trackRoleWasInferred_[idx] = true;
            trackRoleConfirmed_[idx] = false;  // Sprint 1: inferred, not yet confirmed
            rolesInf++;
            if (combined.fromName) nameInf++;
            else spectralInf++;

            if (combined.fromName && combined.confidence >= 0.7f) {
                TrackType mappedType = getTrackTypeForRole(combined.role);
                if (mappedType != TrackType::None)
                    registry.updateSlotTrackType(idx, (int)mappedType);
            }
        }
    });

    if (rolesInf > 0) {
        LogHelper::writeToLog(juce::String("[IdentityLayer] Roles inferidos: ") + juce::String(rolesInf));
        showIdentitySummary();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  showIdentitySummary — FASE 2: Muestra resumen completo de identidad
//  Formato estilo PRODUCT_VISION: cuenta pistas, muestra tracking por
//  instrumento con status ✅/⚡/⚠️, resumen por familias, y call-to-action.
// ═══════════════════════════════════════════════════════════════════════════
void CoachEngine::showIdentitySummary()
{
    auto& registry = sharedData_.getSlotRegistry();
    int d=0,b=0,g=0,k=0,v=0,fx=0,m=0,u=0;
    int totalActive=0, identified=0, inferred=0, unknown=0;
    juce::String identifiedLines, unknownLines;

    registry.forEachActive([&](const SlotInfo& info) {
        int idx = info.slotIndex;
        if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;
        auto role = trackRoles_[idx];
        auto cat = getRoleCategory(role);
        juce::String name = juce::String(info.trackName).trim();
        if (name.isEmpty()) name = "Pista " + juce::String(idx+1);

        switch (cat) {
            case RoleCategory::Drums:   d++; break;
            case RoleCategory::Bass:    b++; break;
            case RoleCategory::Guitars: g++; break;
            case RoleCategory::Keys:    k++; break;
            case RoleCategory::Vocals:  v++; break;
            case RoleCategory::FX:      fx++; break;
            case RoleCategory::Melody:  m++; break;
            default:                    u++; break;
        }

        totalActive++;

        if (role != TrackRole::Unknown && role != TrackRole::Master) {
            identified++;
            if (trackRoleWasInferred_[idx] && !trackRoleConfirmed_[idx])
                inferred++;

            // Status emoji: ✅ confirmed, ⚡ inferred pending
            juce::String statusEmoji = isRoleConfirmed(idx) ? "\xE2\x9C\x85" : "\xE2\x9A\xA1";

            // Bus icon + name
            juce::String busStr;
            if (info.bus >= BusType::Drums && info.bus <= BusType::Melody)
                busStr = getBusIcon(info.bus) + " " + juce::String(busNames[static_cast<int>(info.bus)]);
            else
                busStr = "Sin bus";

            identifiedLines += "  " + statusEmoji + " " + name + " \xE2\x86\x92 "
                             + juce::String(getRoleName(role))
                             + " (" + busStr + ")\n";
        } else if (role == TrackRole::Unknown) {
            unknown++;
            unknownLines += "  \xE2\x9A\xA0\xEF\xB8\x8F \xE2\x80\x9C" + name
                          + "\xE2\x80\x9D \xE2\x86\x92 \xC2\xBFQu\xC3\xA9 instrumento es?\n";
        }
    });

    juce::String msg;
    msg += "\xF0\x9F\x93\xA1 **MESSENGER ACTIVO \xE2\x80\x94 PISTAS IDENTIFICADAS**\n\n";
    msg += "\xF0\x9F\x8E\xA7 **Total: " + juce::String(totalActive) + " pistas detectadas**\n\n";

    if (identified > 0) {
        msg += "\xE2\x9C\x85 **Identificadas (" + juce::String(identified) + ")**\n";
        msg += identifiedLines;
        msg += "\n";
    }

    if (unknown > 0) {
        msg += "\xE2\x9A\xA0\xEF\xB8\x8F **Sin identificar (" + juce::String(unknown) + ")**\n";
        msg += unknownLines;
        msg += "\n";
    }

    // Category summary (same as original)
    msg += "\xF0\x9F\x93\x8A **Resumen por familias:**\n";
    juce::String famLine;
    if (d > 0)  famLine += "\xF0\x9F\xA5\x81 Bater\xC3\xAD" "a: "     + juce::String(d) + " | ";
    if (b > 0)  famLine += "\xF0\x9F\x8E\xB8 Bajo: "               + juce::String(b) + " | ";
    if (g > 0)  famLine += "\xF0\x9F\x8E\xB8 Guitarras: "          + juce::String(g) + " | ";
    if (k > 0)  famLine += "\xF0\x9F\x8E\xB9 Teclados: "           + juce::String(k) + " | ";
    if (v > 0)  famLine += "\xF0\x9F\x8E\xA4 Voces: "             + juce::String(v) + " | ";
    if (fx > 0) famLine += "\xF0\x9F\x9B\x9B\xEF\xB8\x8F FX: "    + juce::String(fx) + " | ";
    if (m > 0)  famLine += "\xF0\x9F\x8E\xB5 Mel\xC3\xB3" "dicos: "   + juce::String(m) + " | ";
    if (u > 0)  famLine += "\xF0\x9F\x93\xA1 Otros: "              + juce::String(u) + " | ";
    if (famLine.isNotEmpty())
        msg += "  " + famLine.substring(0, famLine.length() - 3) + "\n";

    // Call to action según el estado
    if (inferred > 0 && unknown == 0) {
        msg += "\n\xE2\x9A\xA1 **" + juce::String(inferred)
             + " pista(s) pendiente(s) de confirmaci\xC3\xB3n.**\n";
        msg += "   Usa el bot\xC3\xB3n **Confirmar todo** o haz clic en \xE2\x9A\xA1 para confirmar individualmente.";
    } else if (unknown > 0) {
        msg += "\n\xF0\x9F\x92\xA1 **Asigna un rol a cada pista sin identificar**\n";
        msg += "   desde el panel Messenger para completar la identidad de la sesi\xC3\xB3n.";
    } else if (identified > 0 && unknown == 0 && inferred == 0) {
        msg += "\n\xE2\x9C\x85 **Todas las pistas est\xC3\xA1n identificadas y confirmadas.** \xC2\xA1Listo para avanzar!";
    }

    respondWith(msg, MentorMessage::Type::Info);
}

// ════════════════════════════════════════════════════════════════════════════
//  Sprint 1: confirmAllInferredRoles — Confirma todos los pendientes y
//  avanza de fase Organización → GainStaging.
//  V2: Muestra resumen detallado con cada rol confirmado + call-to-action.
// ════════════════════════════════════════════════════════════════════════════
int CoachEngine::confirmAllInferredRoles() noexcept
{
    auto& registry = sharedData_.getSlotRegistry();
    int confirmedCount = 0;
    registry.forEachActive([&](const SlotInfo& info) {
        int idx = info.slotIndex;
        if (idx < 0 || idx >= SlotRegistry::kMaxSlots)
            return;
        if (isInferredRole(idx))
        {
            confirmRole(idx);
            confirmedCount++;
        }
        
    });

    if (confirmedCount > 0)
    {
        LogHelper::writeToLog(juce::String("[Sprint1] Roles confirmados: ")
                              + juce::String(confirmedCount));

        // Construir mensaje detallado con cada rol confirmado
        juce::String detailLines;
        registry.forEachActive([&](const SlotInfo& info) {
            int idx = info.slotIndex;
            if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;
            auto role = trackRoles_[idx];
            if (role == TrackRole::Unknown || role == TrackRole::Master) return;

            juce::String name = juce::String(info.trackName).trim();
            if (name.isEmpty()) name = "Pista " + juce::String(idx+1);

            juce::String busStr;
            if (info.bus >= BusType::Drums && info.bus <= BusType::Melody)
                busStr = " (" + getBusIcon(info.bus) + " " + juce::String(busNames[static_cast<int>(info.bus)]) + ")";

            detailLines += "  \xE2\x9C\x85 " + name + " \xE2\x86\x92 " + juce::String(getRoleName(role)) + busStr + "\n";
        });

        // Avanzar de fase: Organización → GainStaging
        auto currentPhase = phaseManager_.getCurrentPhase();
        if (currentPhase == MentorPhase::Organizacion)
        {
            phaseManager_.advanceToNextPhase();

            juce::String msg;
            msg += "\xE2\x9C\x85 **" + juce::String(confirmedCount) + " roles confirmados!**\n\n";
            msg += "Resumen de identidad de la sesi\u00f3n:\n";
            msg += detailLines;
            msg += "\n\xF0\x9F\x9A\x00 \u00a1Todos los roles confirmados! Avanzamos a **"
                   + juce::String(phaseNames[static_cast<int>(phaseManager_.getCurrentPhase())])
                   + "**.";

            respondWith(msg, MentorMessage::Type::Info);

            // Guía de la nueva fase
            sendPhaseGuidance(phaseManager_.getCurrentPhase());

            LogHelper::writeToLog("[Sprint1] Fase avanzada: Organizacion -> "
                                  + juce::String(phaseNames[static_cast<int>(phaseManager_.getCurrentPhase())]));
        }
        else
        {
            juce::String msg;
            msg += "\xE2\x9C\x85 **" + juce::String(confirmedCount) + " roles confirmados**\n\n";
            msg += detailLines;
            msg += "\n\xE2\x9C\x85 Sesi\u00f3n completamente identificada.";
            respondWith(msg, MentorMessage::Type::Info);
        }
    }
    else
    {
        // No pending roles to confirm - check if there are unknown tracks
        int unknownCount = 0;
        registry.forEachActive([&](const SlotInfo& info) {
            int idx = info.slotIndex;
            if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;
            if (trackRoles_[idx] == TrackRole::Unknown)
                unknownCount++;
        });

        if (unknownCount > 0)
        {
            respondWith("\xE2\x9A\xA0\xEF\xB8\x8F No hay roles pendientes de confirmaci\u00f3n, pero **"
                        + juce::String(unknownCount) + " pista(s)** a\u00fan no tienen rol asignado.\n"
                        "Selecciona un rol desde la lista en el panel Messenger.",
                        MentorMessage::Type::Info);
        }
        else
        {
            respondWith("\xE2\x9C\x85 Todos los roles ya est\u00e1n confirmados.",
                        MentorMessage::Type::Info);
        }
    }

    return confirmedCount;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Sprint 1: getIdentityProgress — Cuenta pistas identificadas vs confirmadas
// ═══════════════════════════════════════════════════════════════════════════
IdentityProgress CoachEngine::getIdentityProgress() const noexcept
{
    IdentityProgress prog;
    const auto& registry = sharedData_.getSlotRegistry();

    registry.forEachActive([&](const SlotInfo& info) {
        int idx = info.slotIndex;
        if (idx < 0 || idx >= SlotRegistry::kMaxSlots)
            return;

        prog.totalActive++;
        auto role = trackRoles_[idx];
        if (role != TrackRole::Unknown && role != TrackRole::Master)
        {
            prog.identified++;
            if (isRoleConfirmed(idx))
                prog.confirmed++;
            else
                prog.pendingInferred++;
        }
    });

    return prog;
}

// ═══════════════════════════════════════════════════════════════════════════
//  setTrackRoleWithLearning
// ═══════════════════════════════════════════════════════════════════════════
void CoachEngine::setTrackRoleWithLearning(int slotIndex, TrackRole role) noexcept
{
    if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots)
        return;

    signalOrderConfirmed_[slotIndex] = true;

    TrackRole oldRole = trackRoles_[slotIndex];
    if (oldRole == role)
        return;

    if (trackRoleWasInferred_[slotIndex])
    {
        bool inferredFromName = false;
        auto telem = getLatestTelemetry(slotIndex);
        TrackSpectralProfile spectral;
        if (telem.timestamp > 0)
            spectral = SpectralProfiler::computeProfile(telem);

        correctionLearner_.recordCorrection(oldRole, role, inferredFromName,
            lastInferenceKeywords_[slotIndex], spectral);

        LogHelper::writeToLog(juce::String("[CoachEngine] Aprendiendo correccion: Slot ")
            + juce::String(slotIndex) + " " + juce::String(getRoleName(oldRole))
            + " -> " + juce::String(getRoleName(role)));
    }

    trackRoles_[slotIndex] = role;
    trackRoleWasInferred_[slotIndex] = false;
    trackRoleConfirmed_[slotIndex] = true;  // Sprint 1: manual role = confirmed
    lastInferenceKeywords_[slotIndex].clear();

    TrackType mappedType = getTrackTypeForRole(role);
    if (mappedType != TrackType::None) {
        auto& registry = sharedData_.getSlotRegistry();
        registry.updateSlotTrackType(slotIndex, (int)mappedType);
    }
}



// ═══════════════════════════════════════════════════════════════════════════
//  REFERENCE-DRIVEN MODE — Computar match % y enviar análisis consolidado
// ═══════════════════════════════════════════════════════════════════════════

ReferenceProgress CoachEngine::computeReferenceMatchProgress()
{
    ReferenceProgress progress;
    progress.hasReference = referenceMetadata_.valid();
    progress.hasAudio = referenceFingerprint_.valid;

    if (!progress.hasAudio)
    {
        referenceProgress_ = progress;
        return progress;
    }

    // ─── Construir DifferenceProfile completo ────────────────────────────
    auto gaps = getReferenceGaps();
    auto dp = buildDifferenceProfile();

    if (!dp.valid)
    {
        referenceProgress_ = progress;
        return progress;
    }

    // ─── Computar match % desde DifferenceProfile ────────────────────────
    // Combinamos 3 métricas:
    //   1. DeltaScore (0.0-1.0) desde DifferenceProfile (combina LUFS + Crest + Spectral)
    //   2. Inverse gap severity: cuantos gaps NO son críticos/warning
    //   3. Inverse LUFS gap: qué tan lejos estamos en loudness

    float deltaScore = dp.deltaScore;  // 0.0-1.0 desde DifferenceProfile

    // Inverse gap severity: si hay muchos gaps críticos, el match baja
    float gapScore = 1.0f;
    if (!gaps.empty())
    {
        int penaltyCount = 0;
        for (const auto& g : gaps)
        {
            if (g.severity == GapSeverity::Critical) penaltyCount += 3;
            else if (g.severity == GapSeverity::Warning) penaltyCount += 1;
        }
        gapScore = 1.0f - std::min(1.0f, static_cast<float>(penaltyCount) / 15.0f);
    }

    // Inverse LUFS gap: qué tan lejos está el integrated LUFS
    float lufsGap = std::abs(dp.deltaLUFS);
    float lufsScore = 1.0f - std::min(1.0f, lufsGap / 6.0f); // 6 LUFS = 0 score

    // ─── Match final: weighted average ────────────────────────────────────
    // DeltaScore pesa más porque ya combina múltiples métricas
    progress.currentMatch = deltaScore * 0.50f + gapScore * 0.30f + lufsScore * 0.20f;
    progress.currentMatch = juce::jlimit(0.0f, 1.0f, progress.currentMatch);

    // ─── Gaps counts ────────────────────────────────────────────────────
    progress.totalGaps = static_cast<int>(gaps.size());
    progress.criticalGaps = 0;
    progress.warningGaps = 0;
    for (const auto& g : gaps)
    {
        if (g.severity == GapSeverity::Critical) progress.criticalGaps++;
        else if (g.severity == GapSeverity::Warning) progress.warningGaps++;
    }

    // ─── Delta vs medición anterior ─────────────────────────────────────
    progress.previousMatch = previousReferenceMatch_;
    progress.delta = progress.currentMatch - progress.previousMatch;

    // ─── Resolved gaps: comparar con medición anterior ────────────────────
    // Si totalGaps bajó vs la última vez, esos se consideran "resolved"
    if (!previousReferenceGaps_.empty())
    {
        int prevTotal = static_cast<int>(previousReferenceGaps_.size());
        progress.resolvedGaps = std::max(0, prevTotal - progress.totalGaps);
    }

    // ─── Actualizar historial ────────────────────────────────────────────
    previousReferenceMatch_ = progress.currentMatch;
    previousReferenceGaps_ = gaps;

    if (progressHistoryCount_ < kMaxProgressHistory)
        progressHistoryCount_++;
    progressHistory_[progressHistoryIndex_] = progress.currentMatch;
    progressHistoryIndex_ = (progressHistoryIndex_ + 1) % kMaxProgressHistory;

    // ─── Cache ───────────────────────────────────────────────────────────
    referenceProgress_ = progress;

    LogHelper::writeToLog("[CoachEngine] Reference-Driven Progress: "
                          + juce::String(static_cast<int>(progress.currentMatch * 100.0f)) + "%"
                          + " (delta " + juce::String(progress.delta * 100.0f, 1) + "%"
                          + " | gaps " + juce::String(progress.criticalGaps) + "c/"
                          + juce::String(progress.warningGaps) + "w/"
                          + juce::String(progress.resolvedGaps) + "r)");

    return progress;
}

// ═══════════════════════════════════════════════════════════════════════════
//  resetReferenceProgress — Resets all reference-driven tracking data
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::resetReferenceProgress() noexcept
{
    referenceProgress_ = ReferenceProgress{};
    previousReferenceMatch_ = 0.0f;
    previousReferenceGaps_.clear();
    progressHistoryCount_ = 0;
    progressHistoryIndex_ = 0;
    for (auto& v : progressHistory_)
        v = 0.0f;
    lastReferenceDrivenAnalysisUs_ = 0;
    lastReferenceGapImprovedUs_ = 0;
    lastReferenceGapWorsenedUs_ = 0;
    LogHelper::writeToLog("[CoachEngine] Reference progress reset");
}

// ═══════════════════════════════════════════════════════════════════════════
//  sendReferenceDrivenAnalysis — Envía un mensaje consolidado al LLM o al chat
//  cuando el match contra la referencia cambia significativamente.
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::sendReferenceDrivenAnalysis()
{
    if (!referenceFingerprint_.valid)
        return;

    // Si hay LLM streaming, enviar el análisis como prompt al LLM
    // para que genere una respuesta natural estilo ingeniero.
    if (llmEnabled_ && llmStreamingCallback_ && setupStep_ == SetupStep::Complete)
    {
        juce::String refName = getReferenceName();
        auto& plan = planManager_;

        juce::String prompt;
        prompt += "=== REFERENCE-DRIVEN MODE ===\n";
        prompt += "El Reference-Driven Mode esta ACTIVO. La referencia es el norte absoluto.\n";
        prompt += "Referencia: " + refName + "\n\n";

        // Progreso actual
        prompt += referenceProgress_.toLLMContext();
        prompt += "\n";

        // Gaps activos (top 3)
        auto gaps = getReferenceGaps();
        if (!gaps.empty())
        {
            prompt += "Gaps actuales contra referencia:\n";
            int maxShow = std::min(static_cast<int>(gaps.size()), 5);
            for (int i = 0; i < maxShow; ++i)
                prompt += "  " + gaps[i].toLLMContextGap() + "\n";
            prompt += "\n";
        }

        // Siguiente paso del plan
        if (plan.hasPlan())
        {
            const auto* currentStep = plan.getCurrentStep();
            if (currentStep != nullptr)
            {
                prompt += "Siguiente paso en el plan:\n";
                prompt += "  " + currentStep->toTextSummary() + "\n";
            }
            else if (plan.getProgress() >= 0.95f)
            {
                prompt += "¡Todos los pasos del plan completados! La mezcla esta alineada con la referencia.\n";
            }
        }

        prompt += "\nGenera un comentario natural como ingeniero sobre el progreso contra la referencia.\n";
        prompt += "Si el match mejoro, celebralo brevemente. Si empeoro, avisa con calma.\n";
        prompt += "Di el numero exacto: \"Estamos al " + juce::String(static_cast<int>(referenceProgress_.currentMatch * 100.0f)) + "% del match con la referencia\"\n";
        prompt += "NO estructures la respuesta. NO digas \"Claro!\" ni \"Por supuesto!\".\n";
        prompt += "Habla como ingeniero en el estudio. Maximo 3 oraciones.\n";

        // Enviar al LLM streaming
        bool accepted = llmStreamingCallback_(
            prompt,
            [this](const juce::String& token)
            {
                if (streamTokenCb_)
                    streamTokenCb_(token);
            },
            [this](const juce::String& response)
            {
                if (streamEndedCb_)
                    streamEndedCb_();
                LogHelper::writeToLog("[CoachEngine] Reference-Driven Analysis enviada al chat");
            });

        if (accepted)
        {
            if (streamStartedCb_)
                streamStartedCb_();
            return;
        }
    }

        // ───     // ─── Fallback: mensaje CUALITATIVO al chat si no hay LLM ───────────────────
    // Genera un mensaje conversacional usando los gaps y la tendencia,
    // en vez de mostrar números crudos.
    auto gaps = getReferenceGaps();
    juce::String msg;

    // First line: general state
    int matchPct = static_cast<int>(referenceProgress_.currentMatch * 100.0f);
    if (matchPct >= 80)
        msg += "🎯 **Muy cerca de la referencia** — el balance general está muy alineado.\n";
    else if (matchPct >= 60)
        msg += "🎯 **Buen camino** — la mezcla se está pareciendo a la referencia.\n";
    else if (matchPct >= 40)
        msg += "🎯 **Avanzando** — varias áreas ya coinciden con la referencia.\n";
    else if (matchPct >= 20)
        msg += "🎯 **Empezando** — estamos en las primeras etapas de alineación.\n";
    else
        msg += "🎯 **Recién empezamos** — iremos ajustando poco a poco.\n";

    // Second line: trend
    if (referenceProgress_.delta > 0.03f)
        msg += "📈 La mezcla **está mejorando** contra la referencia. Sigue así.\n";
    else if (referenceProgress_.delta < -0.03f)
        msg += "📉 La mezcla **se está alejando** un poco — revisa los últimos cambios.\n";
    else
        msg += "➡ El match se mantiene estable con respecto al ciclo anterior.\n";

    // Third section: describe the 2 most significant gaps
    if (!gaps.empty())
    {
        // Filter critical/warning gaps, sort by priority
        std::vector<const DomainGap*> significantGaps;
        for (const auto& g : gaps)
        {
            if (g.severity == GapSeverity::Critical || g.severity == GapSeverity::Warning)
                significantGaps.push_back(&g);
        }
        std::sort(significantGaps.begin(), significantGaps.end(),
            [](const DomainGap* a, const DomainGap* b) {
                return a->priority < b->priority;
            });

        if (!significantGaps.empty())
        {
            int maxShow = std::min(static_cast<int>(significantGaps.size()), 2);
            msg += "\n📋 **Áreas con diferencia:**\n";
            for (int i = 0; i < maxShow; ++i)
            {
                const auto& g = *significantGaps[i];
                msg += "  • ";
                msg += g.metric;
                if (g.severity == GapSeverity::Critical)
                    msg += " (requiere atención)";
                else
                    msg += " (casi listo)";
                msg += "\n";
            }
            if ((int)significantGaps.size() > maxShow)
            {
                msg += "  • y ";
                msg += juce::String((int)significantGaps.size() - maxShow);
                msg += " más...\n";
            }
        }
    }

    // Fourth line: resolved gaps (achievement)
    if (referenceProgress_.resolvedGaps > 0)
    {
        msg += "\n✅ ";
        msg += juce::String(referenceProgress_.resolvedGaps);
        msg += " gap(s) resuelto(s) desde la última vez. ¡Buen trabajo!\n";
    }

    // Fifth line: next plan step
    if (planManager_.hasPlan())
    {
        const auto* step = planManager_.getCurrentStep();
        if (step != nullptr)
        {
            msg += "\n💡 Próximo paso: ";
            msg += step->description;
        }
    }

    respondWithPremium(msg, MentorMessage::Type::Info);
}

// ═══════════════════════════════════════════════════════════════════════════
// 
// ════════════════════════════════════════════════════════════════════
//  SPRINT 5: syncTrackFeedCore — Sincroniza datos de todas las pistas
//  activas desde SharedData hacia TrackFeedCore en cada ciclo de
//  periodicAnalysis(), generando eventos de salud y atención.
// ════════════════════════════════════════════════════════════════════
void CoachEngine::syncTrackFeedCore()
{
    auto& registry = sharedData_.getSlotRegistry();

    registry.forEachActive([&](const SlotInfo& info) {
        int idx = info.slotIndex;
        if (idx < 0 || idx >= SlotRegistry::kMaxSlots)
            return;

        auto result = sharedData_.getTrackAudioResult(idx);
        if (result.timestampUs <= 0)
            return;

        TrackRole role = trackRoles_[idx];
        trackFeedCore_->updateTrackState(idx, result, info, role);

        // --- SPRINT 6B: Role-aware dynamics analysis ---
        // Post-process con analyzeTrackDynamics() usando targets
        // por rol (ExpectedProfile.crestTargetDb) en vez de thresholds fijos.
        // Empuja eventos y actualiza health para que aparezcan en el banner UI.
        auto dynAdvice = analyzeTrackDynamics(idx);
        if (dynAdvice.isActionable())
        {
            TrackEvent ev;
            ev.trackId = idx;
            ev.timestampUs = result.timestampUs;
            ev.value = dynAdvice.currentCrest;
            ev.threshold = dynAdvice.crestTarget;
            ev.deviation = dynAdvice.crestDeviation;
            ev.context = "dynamics";

            // Cooldown: only push if enough time has passed
            int64_t nowTc = result.timestampUs > 0 ? result.timestampUs : juce::Time::getMillisecondCounter() * 1000;
            if (!trackFeedCore_->checkAndSetCrestCooldown(idx, nowTc, kDynamicsAdviceCooldownUs))
                return;

            if (dynAdvice.isOvercompressed())
            {
                ev.type = TrackEventType::CrestTooLow;
                ev.severity = 0.7f;
            }
            else if (dynAdvice.isTooDynamic())
            {
                ev.type = TrackEventType::CrestTooHigh;
                ev.severity = 0.5f;
            }
            else
            {
                ev.type = TrackEventType::Info;
                ev.severity = 0.3f;
            }
            ev.message = dynAdvice.message;

            trackFeedCore_->pushTrackEvent(idx, ev);

            // Actualizar health segun el advice rol-aware
            if (dynAdvice.isOvercompressed())
                trackFeedCore_->updateTrackHealth(idx, TrackHealth::Overcompressed);
            else if (dynAdvice.isTooDynamic())
                trackFeedCore_->updateTrackHealth(idx, TrackHealth::NeedsCompression);
        }

        // --- SPRINT 6A: Role-aware gain analysis ---
        // Post-process con analyzeTrackGain() usando targets
        // por rol (ExpectedProfile.peakTargetDb) en vez de thresholds fijos.
        // Empuja eventos y actualiza health para que aparezcan en el banner UI.
        auto gainAdvice = analyzeTrackGain(idx);
        if (gainAdvice.isActionable())
        {
            TrackEvent ev;
            ev.trackId = idx;
            ev.timestampUs = result.timestampUs;
            ev.value = gainAdvice.currentPeak;
            ev.threshold = gainAdvice.peakTarget;
            ev.deviation = gainAdvice.peakDeviation;
            ev.context = "gain";

            // Cooldown: only push if enough time has passed
            int64_t nowGc = result.timestampUs > 0 ? result.timestampUs : juce::Time::getMillisecondCounter() * 1000;
            if (!trackFeedCore_->checkAndSetGainCooldown(idx, nowGc, kGainAdviceCooldownUs))
                return;

            if (gainAdvice.currentPeak > -0.5f)
            {
                ev.type = TrackEventType::ClippingDetected;
                ev.severity = 0.9f;
            }
            else if (gainAdvice.currentPeak > -6.0f)
            {
                ev.type = TrackEventType::LevelSpike;
                ev.severity = 0.6f;
            }
            else if (gainAdvice.currentPeak < -30.0f)
            {
                ev.type = TrackEventType::LowSignal;
                ev.severity = 0.4f;
            }
            else
            {
                ev.type = TrackEventType::Info;
                ev.severity = 0.3f;
            }
            ev.message = gainAdvice.message;

            trackFeedCore_->pushTrackEvent(idx, ev);

            // Actualizar health segun el advice rol-aware
            if (gainAdvice.currentPeak > -0.5f)
                trackFeedCore_->updateTrackHealth(idx, TrackHealth::ClippingRisk);
            else if (gainAdvice.currentPeak < -30.0f)
                trackFeedCore_->updateTrackHealth(idx, TrackHealth::LowSignal);
        }

        // --- SPRINT 8: Role-aware phase analysis ---
        auto phaseAdvice = analyzeTrackPhase(idx);
        if (phaseAdvice.isActionable())
        {
            TrackEvent ev;
            ev.trackId = idx;
            ev.timestampUs = result.timestampUs;
            ev.value = phaseAdvice.currentCorrelation;
            ev.threshold = 0.3f;
            ev.deviation = phaseAdvice.correlationDeviation;
            ev.context = "phase";

            int64_t nowPh = result.timestampUs > 0 ? result.timestampUs : juce::Time::getMillisecondCounter() * 1000;
            if (!trackFeedCore_->checkAndSetPhaseCooldown(idx, nowPh, kPhaseAdviceCooldownUs))
                return;

            if (phaseAdvice.status == TrackPhaseAdvice::Status::OffTarget)
            {
                ev.type = TrackEventType::PhaseIssue;
                ev.severity = 0.8f;
            }
            else
            {
                ev.type = TrackEventType::PhaseIssue;
                ev.severity = 0.4f;
            }
            ev.message = phaseAdvice.message;

            trackFeedCore_->pushTrackEvent(idx, ev);

            if (phaseAdvice.status == TrackPhaseAdvice::Status::OffTarget)
                trackFeedCore_->updateTrackHealth(idx, TrackHealth::PhaseIssue);
        }

    });
}


// ════════════════════════════════════════════════════════════════════


// ══════════════════════════════════════════════════════════════════════════════════════
//  SPRINT 6A: analyzeTrackGain — Analiza ganancia de una pista vs su rol
//  Usa ExpectedProfile de TrackRole.h. Sin IA — reglas C++, 0 tokens.
// ══════════════════════════════════════════════════════════════════════════════════════
CoachEngine::TrackGainAdvice CoachEngine::analyzeTrackGain(int slotIndex)
{
    TrackGainAdvice advice;
    advice.slotIndex = slotIndex;

    if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots)
    {
        advice.status = TrackGainAdvice::Status::UnknownRole;
        return advice;
    }

    // Get slot info
    auto info = sharedData_.getSlotRegistry().getSlotInfo(slotIndex);
    advice.trackName = juce::String(info.trackName).trim();
    if (advice.trackName.isEmpty())
        advice.trackName = "Pista " + juce::String(slotIndex + 1);

    // Get role
    advice.role = trackRoles_[slotIndex];
    if (advice.role == TrackRole::Unknown || advice.role == TrackRole::Master)
    {
        advice.status = TrackGainAdvice::Status::UnknownRole;
        return advice;
    }

    // Get telemetry
    auto telem = getLatestTelemetry(slotIndex);
    if (telem.timestamp == 0)
    {
        advice.status = TrackGainAdvice::Status::NoSignal;
        return advice;
    }

    // Current values
    advice.currentPeak  = juce::jmax(telem.peakLeft, telem.peakRight);
    advice.currentRMS   = (telem.rmsLeft + telem.rmsRight) * 0.5f;
    advice.currentCrest = telem.crestFactor;
    advice.currentLUFS  = computePerTrackLUFS(
        sharedData_.getTrackAudioResult(slotIndex));

    // Target from role
    auto profile = getExpectedProfile(advice.role);
    advice.peakTarget    = profile.peakTargetDb;
    advice.crestTarget   = profile.crestTargetDb;
    advice.peakTolerance = profile.peakTolerance;

    // No signal check
    if (advice.currentPeak < -60.0f)
    {
        advice.status = TrackGainAdvice::Status::NoSignal;
        return advice;
    }

    // Deviation: positivo = track suena mas fuerte que el target
    advice.peakDeviation = advice.currentPeak - advice.peakTarget;

    // Status
    float tol = advice.peakTolerance;
    if (std::abs(advice.peakDeviation) <= tol)
        advice.status = TrackGainAdvice::Status::OnTarget;
    else if (std::abs(advice.peakDeviation) <= tol * 2.0f)
        advice.status = TrackGainAdvice::Status::NearTarget;
    else
        advice.status = TrackGainAdvice::Status::OffTarget;

    // Suggested delta: queremos acercarnos al target
    // Si peakDeviation > 0 (track mas fuerte), sugerimos bajar (delta negativo)
    // Si peakDeviation < 0 (track mas suave), sugerimos subir (delta positivo)
    advice.suggestedDeltaDb = juce::jlimit(-12.0f, 12.0f, -advice.peakDeviation);

    // Generate human-readable message
    switch (advice.status)
    {
        case TrackGainAdvice::Status::OnTarget:
            advice.message = "\u2705 " + advice.trackName
                + " \u2014 nivel en rango (" + juce::String(advice.currentPeak, 1)
                + " dBFS, target " + juce::String(advice.peakTarget, 1) + " dBFS)";
            break;
        case TrackGainAdvice::Status::NearTarget:
            if (advice.peakDeviation > 0.0f)
                advice.message = "\xF0\x9F\x9F\xA1 " + advice.trackName
                    + " \u2014 ligeramente alto (" + juce::String(advice.currentPeak, 1)
                    + " dBFS). Baja ~" + juce::String(std::abs(advice.suggestedDeltaDb), 1)
                    + " dB.";
            else
                advice.message = "\xF0\x9F\x9F\xA1 " + advice.trackName
                    + " \u2014 ligeramente bajo (" + juce::String(advice.currentPeak, 1)
                    + " dBFS). Sube ~" + juce::String(std::abs(advice.suggestedDeltaDb), 1)
                    + " dB.";
            break;
        case TrackGainAdvice::Status::OffTarget:
            if (advice.peakDeviation > 0.0f)
                advice.message = "\xF0\x9F\x94\xB4 " + advice.trackName
                    + " \u2014 demasiado alto (" + juce::String(advice.currentPeak, 1)
                    + " dBFS, target " + juce::String(advice.peakTarget, 1)
                    + " dBFS). Baja ~" + juce::String(std::abs(advice.suggestedDeltaDb), 1)
                    + " dB.";
            else
                advice.message = "\xF0\x9F\x94\xB4 " + advice.trackName
                    + " \u2014 demasiado bajo (" + juce::String(advice.currentPeak, 1)
                    + " dBFS, target " + juce::String(advice.peakTarget, 1)
                    + " dBFS). Sube ~" + juce::String(std::abs(advice.suggestedDeltaDb), 1)
                    + " dB.";
            break;
        case TrackGainAdvice::Status::NoSignal:
            advice.message = "\xF0\x9F\x94\x87 " + advice.trackName
                + " \u2014 sin sen\xC3\xB1""al detectable.";
            break;
        case TrackGainAdvice::Status::UnknownRole:
            advice.message = "\xE2\x9D\x93 " + advice.trackName
                + " \u2014 rol no especificado.";
            break;
    }

    return advice;
}

std::vector<CoachEngine::TrackGainAdvice> CoachEngine::analyzeAllTracksGain()
{
    std::vector<TrackGainAdvice> results;
    auto& registry = sharedData_.getSlotRegistry();

    registry.forEachActive([&](const SlotInfo& info) {
        auto advice = analyzeTrackGain(info.slotIndex);
        if (advice.isActionable())
            results.push_back(advice);
    });

    // Sort by severity: OffTarget first, then NearTarget
    std::sort(results.begin(), results.end(),
        [](const TrackGainAdvice& a, const TrackGainAdvice& b) {
            auto sev = [](TrackGainAdvice::Status s) -> int {
                return (s == TrackGainAdvice::Status::OffTarget) ? 0 : 1;
            };
            return sev(a.status) < sev(b.status);
        });

    return results;
}

// ═══════════════════════════════════════════════════════════════════════════
//  SPRINT 6B: analyzeTrackDynamics — Analiza dinámica de una pista vs su rol
//  Compara crestFactor contra ExpectedProfile.crestTargetDb ± crestTolerance.
//  Sin IA — reglas C++, 0 tokens.
// ═══════════════════════════════════════════════════════════════════════════
CoachEngine::TrackDynamicsAdvice CoachEngine::analyzeTrackDynamics(int slotIndex)
{
    TrackDynamicsAdvice advice;
    advice.slotIndex = slotIndex;

    if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots)
    {
        advice.status = TrackDynamicsAdvice::Status::UnknownRole;
        return advice;
    }

    // Get slot info
    auto info = sharedData_.getSlotRegistry().getSlotInfo(slotIndex);
    advice.trackName = juce::String(info.trackName).trim();
    if (advice.trackName.isEmpty())
        advice.trackName = "Pista " + juce::String(slotIndex + 1);

    // Get role
    advice.role = trackRoles_[slotIndex];
    if (advice.role == TrackRole::Unknown || advice.role == TrackRole::Master)
    {
        advice.status = TrackDynamicsAdvice::Status::UnknownRole;
        return advice;
    }

    // Get telemetry
    auto telem = getLatestTelemetry(slotIndex);
    if (telem.timestamp == 0 || telem.rmsLeft < -50.0f)
    {
        advice.status = TrackDynamicsAdvice::Status::NoSignal;
        return advice;
    }

    // Current values
    advice.currentCrest = telem.crestFactor;
    advice.currentPeak  = juce::jmax(telem.peakLeft, telem.peakRight);
    advice.currentRMS   = (telem.rmsLeft + telem.rmsRight) * 0.5f;

    // No viable crest data
    if (advice.currentCrest <= 0.0f)
    {
        advice.status = TrackDynamicsAdvice::Status::NoSignal;
        return advice;
    }

    // Target from role
    auto profile = getExpectedProfile(advice.role);
    advice.crestTarget    = profile.crestTargetDb;
    advice.crestTolerance = profile.crestTolerance;

    // Deviation: positivo = mas dinamico que el target
    advice.crestDeviation = advice.currentCrest - advice.crestTarget;

    // Status
    float tol = advice.crestTolerance;
    if (std::abs(advice.crestDeviation) <= tol)
    {
        advice.status = TrackDynamicsAdvice::Status::OnTarget;
        advice.subType = TrackDynamicsAdvice::SubType::None;
    }
    else if (std::abs(advice.crestDeviation) <= tol * 2.0f)
    {
        advice.status = TrackDynamicsAdvice::Status::NearTarget;
        advice.subType = (advice.crestDeviation < 0)
            ? TrackDynamicsAdvice::SubType::Overcompressed
            : TrackDynamicsAdvice::SubType::TooDynamic;
    }
    else
    {
        advice.status = TrackDynamicsAdvice::Status::OffTarget;
        advice.subType = (advice.crestDeviation < 0)
            ? TrackDynamicsAdvice::SubType::Overcompressed
            : TrackDynamicsAdvice::SubType::TooDynamic;
    }

    // Suggested action and message
    switch (advice.subType)
    {
        case TrackDynamicsAdvice::SubType::Overcompressed:
        {
            float deficit = advice.crestTarget - advice.currentCrest;  // cuantos dB le falta
            advice.suggestedAction = "Baja el ratio del compresor o sube el threshold "
                                     + juce::String(deficit * 0.5f, 1) + " dB.";
            advice.message = "\xF0\x9F\x94\xB4 " + advice.trackName
                + " \u2014 sobre-comprimido (crest " + juce::String(advice.currentCrest, 1)
                + " dB, target " + juce::String(advice.crestTarget, 1)
                + " dB). " + advice.suggestedAction;
            break;
        }
        case TrackDynamicsAdvice::SubType::TooDynamic:
        {
            advice.suggestedAction = "Prueba un compresor con ratio 4:1 y attack r\xC3\xA1pido (~10ms).";
            if (advice.currentCrest > advice.crestTarget + tol * 3.0f)
                advice.suggestedAction = "Aplica compresi\xC3\xB3n suave 2:1 con threshold en -20 dB.";
            advice.message = "\xF0\x9F\x94\xB4 " + advice.trackName
                + " \u2014 demasiado din\xC3\xA1mico (crest " + juce::String(advice.currentCrest, 1)
                + " dB, target " + juce::String(advice.crestTarget, 1)
                + " dB). " + advice.suggestedAction;
            break;
        }
        case TrackDynamicsAdvice::SubType::None:
        {
            if (advice.status == TrackDynamicsAdvice::Status::NearTarget)
            {
                advice.message = "\xF0\x9F\x9F\xA1 " + advice.trackName
                    + " \u2014 crest " + juce::String(advice.currentCrest, 1)
                    + " dB (target " + juce::String(advice.crestTarget, 1) + " dB). Cerca del l\u00EDmite.";
                advice.suggestedAction = "Monitorea, no requiere acci\xC3\xB3n inmediata.";
            }
            else
            {
                advice.message = "\u2705 " + advice.trackName
                    + " \u2014 crest " + juce::String(advice.currentCrest, 1)
                    + " dB en rango (target " + juce::String(advice.crestTarget, 1) + " dB).";
            }
            break;
        }
        default:
            break;
    }

    return advice;
}

// ═══════════════════════════════════════════════════════════════════════════
//  SPRINT 6C: analyzeTrackTonal — Analiza balance espectral de una pista vs su rol
//  Convierte 30 bandEnergies en 6 regiones espectrales y compara
//  contra ExpectedProfile.spectralOffset para detectar exceso/déficit.
//  Sin IA — reglas C++, 0 tokens.
// ═══════════════════════════════════════════════════════════════════════════
CoachEngine::TrackTonalAdvice CoachEngine::analyzeTrackTonal(int slotIndex)
{
    TrackTonalAdvice advice;
    advice.slotIndex = slotIndex;

    if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots)
    {
        advice.status = TrackTonalAdvice::Status::NoSignal;
        return advice;
    }

    // ─── Obtener info del slot ──────────────────────────────────────────
    auto& registry = sharedData_.getSlotRegistry();
    auto info = registry.getSlotInfo(slotIndex);
    if (info.slotIndex < 0)
    {
        advice.status = TrackTonalAdvice::Status::NoSignal;
        return advice;
    }

    juce::String trackName = juce::String(info.trackName).trim();
    if (trackName.isEmpty())
        trackName = "Pista " + juce::String(slotIndex + 1);
    advice.trackName = trackName;

    // ─── Obtener rol ────────────────────────────────────────────────────
    TrackRole role = trackRoles_[slotIndex];
    if (role == TrackRole::Unknown || role == TrackRole::Master)
    {
        advice.role = role;
        advice.status = TrackTonalAdvice::Status::UnknownRole;
        return advice;
    }
    advice.role = role;

    // ─── Obtener telemetría ─────────────────────────────────────────────
    auto telem = getLatestTelemetry(slotIndex);
    if (telem.timestamp == 0)
    {
        advice.status = TrackTonalAdvice::Status::NoSignal;
        return advice;
    }

    // Verificar que hay señal suficiente
    float peakDb = juce::jmax(telem.peakLeft, telem.peakRight);
    if (peakDb < -60.0f)
    {
        advice.currentPeak = peakDb;
        advice.status = TrackTonalAdvice::Status::NoSignal;
        return advice;
    }
    advice.currentPeak = peakDb;

    // ─── Obtener perfil esperado del rol ────────────────────────────────
    auto profile = getExpectedProfile(role);

    // ─── Mapear 30 bandEnergies a 6 regiones espectrales ──────────────────
    for (int region = 0; region < 6; ++region)
    {
        int bandStart = region * 5;
        int bandEnd = bandStart + 5;

        float sumEnergies = 0.0f;
        int validBands = 0;
        for (int b = bandStart; b < bandEnd && b < 30; ++b)
        {
            if (telem.bandEnergies[b] > -80.0f)
            {
                sumEnergies += telem.bandEnergies[b];
                validBands++;
            }
        }

        if (validBands > 0)
            advice.regionEnergy[region] = sumEnergies / (float)validBands;
        else
            advice.regionEnergy[region] = -100.0f;

        advice.regionExpected[region] = peakDb + profile.spectralOffset[region];

        if (advice.regionEnergy[region] > -80.0f)
            advice.regionDeviation[region] = advice.regionEnergy[region] - advice.regionExpected[region];
        else
            advice.regionDeviation[region] = 0.0f;
    }

    // ─── Encontrar la peor región (mayor desviación absoluta) ────────────
    float maxAbsDev = 0.0f;
    int worstRegion = -1;
    for (int r = 0; r < 6; ++r)
    {
        float absDev = std::abs(advice.regionDeviation[r]);
        if (absDev > maxAbsDev)
        {
            maxAbsDev = absDev;
            worstRegion = r;
        }
    }
    advice.worstRegion = worstRegion;
    advice.worstDeviation = (worstRegion >= 0) ? advice.regionDeviation[worstRegion] : 0.0f;

    // ─── Determinar status ────────────────────────────────────────────────
    if (worstRegion < 0)
    {
        advice.status = TrackTonalAdvice::Status::OnTarget;
        return advice;
    }

    float worstAbs = std::abs(advice.worstDeviation);
    if (worstAbs <= TrackTonalAdvice::kToleranceDb)
    {
        advice.status = TrackTonalAdvice::Status::OnTarget;
        advice.message = "\xF0\x9F\x9F\xA2 **" + trackName + "** \xE2\x80\x94 balance espectral en rango (" + juce::String(profile.name) + ").";
    }
    else if (worstAbs <= TrackTonalAdvice::kNearToleranceDb)
    {
        advice.status = TrackTonalAdvice::Status::NearTarget;
        advice.isExcess = (advice.worstDeviation > 0.0f);
        const char* regionName = TrackTonalAdvice::kRegionName(worstRegion);
        float deviation = advice.worstDeviation;
        juce::String action;
        if (advice.isExcess)
            action = "Exceso en **" + juce::String(regionName) + "** (" + juce::String(deviation, 1) + " dB sobre target). " + juce::String(TrackTonalAdvice::kExcessSuggestion(worstRegion));
        else
            action = "Falta en **" + juce::String(regionName) + "** (" + juce::String(-deviation, 1) + " dB bajo target). " + juce::String(TrackTonalAdvice::kDeficitSuggestion(worstRegion));

        advice.message = "\xF0\x9F\x9F\xA1 **" + trackName + "** \xE2\x80\x94 ligero desbalance espectral " + action;
    }
    else
    {
        advice.status = TrackTonalAdvice::Status::OffTarget;
        advice.isExcess = (advice.worstDeviation > 0.0f);
        const char* regionName = TrackTonalAdvice::kRegionName(worstRegion);
        const char* freqRange = TrackTonalAdvice::kRegionFreq(worstRegion);

        if (advice.isExcess)
        {
            advice.message = "\xF0\x9F\x94\xB4 **" + trackName + "** \xE2\x80\x94 exceso de energ\xC3\xAD""a en **"
                            + juce::String(regionName) + "** (" + juce::String(freqRange) + ", "
                            + juce::String(advice.regionEnergy[worstRegion], 1) + " dBFS, esperado "
                            + juce::String(advice.regionExpected[worstRegion], 1) + " dBFS). "
                            + "Prueba " + juce::String(TrackTonalAdvice::kExcessSuggestion(worstRegion)) + ".";
        }
        else
        {
            advice.message = "\xF0\x9F\x94\xB4 **" + trackName + "** \xE2\x80\x94 falta de energ\xC3\xAD""a en **"
                            + juce::String(regionName) + "** (" + juce::String(freqRange) + ", "
                            + juce::String(advice.regionEnergy[worstRegion], 1) + " dBFS, esperado "
                            + juce::String(advice.regionExpected[worstRegion], 1) + " dBFS). "
                            + "Prueba " + juce::String(TrackTonalAdvice::kDeficitSuggestion(worstRegion)) + ".";
        }
    }

    return advice;
}

std::vector<CoachEngine::TrackDynamicsAdvice> CoachEngine::analyzeAllTracksDynamics()
{
    std::vector<TrackDynamicsAdvice> results;
    auto& registry = sharedData_.getSlotRegistry();

    registry.forEachActive([&](const SlotInfo& info) {
        auto advice = analyzeTrackDynamics(info.slotIndex);
        if (advice.isActionable())
            results.push_back(advice);
    });

    // Sort by severity: OffTarget first (overcompressed antes que tooDynamic), then NearTarget
    std::sort(results.begin(), results.end(),
        [](const TrackDynamicsAdvice& a, const TrackDynamicsAdvice& b) {
            auto sev = [](const TrackDynamicsAdvice& adv) -> int {
                if (adv.status == TrackDynamicsAdvice::Status::OffTarget)
                    return adv.isOvercompressed() ? 0 : 1;
                if (adv.status == TrackDynamicsAdvice::Status::NearTarget)
                    return 2;
                return 3;
            };
            return sev(a) < sev(b);
        });

    return results;
}

// ═══════════════════════════════════════════════════════════════════════════
//  SPRINT 6C: analyzeAllTracksTonal — Analiza balance espectral de TODAS las pistas
//  Itera slots activos, filtra por rol conocido + señal, ordena por severidad.
// ═══════════════════════════════════════════════════════════════════════════
std::vector<CoachEngine::TrackTonalAdvice> CoachEngine::analyzeAllTracksTonal()
{
    std::vector<TrackTonalAdvice> results;
    auto& registry = sharedData_.getSlotRegistry();

    registry.forEachActive([&](const SlotInfo& info) {
        auto advice = analyzeTrackTonal(info.slotIndex);
        if (advice.isActionable())
            results.push_back(advice);
    });

    // Sort by severity: OffTarget first, then NearTarget
    std::sort(results.begin(), results.end(),
        [](const TrackTonalAdvice& a, const TrackTonalAdvice& b) {
            auto sev = [](const TrackTonalAdvice& adv) -> int {
                if (adv.status == TrackTonalAdvice::Status::OffTarget)
                    return adv.isExcess ? 0 : 1;
                if (adv.status == TrackTonalAdvice::Status::NearTarget)
                    return 2;
                return 3;
            };
            return sev(a) < sev(b);
        });

    return results;
}

// ═════════════════════
//  SPRINT 8: analyzeTrackPhase
// ═════════════════════
CoachEngine::TrackPhaseAdvice CoachEngine::analyzeTrackPhase(int slotIndex)
{
    TrackPhaseAdvice advice;
    advice.slotIndex = slotIndex;
    if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) return advice;
    auto& registry = sharedData_.getSlotRegistry();
    SlotInfo info = registry.getSlotInfo(slotIndex);
    if (!info.active) return advice;
    advice.trackName = juce::String(info.trackName).trim();
    if (advice.trackName.isEmpty())
        advice.trackName = "Track " + juce::String(slotIndex + 1);
    advice.role = trackRoles_[slotIndex];

    auto telem = getLatestTelemetry(slotIndex);
    if (telem.timestamp == 0)
    { advice.status = TrackPhaseAdvice::Status::NoSignal; return advice; }

    float corr = telem.correlation;
    float peak = juce::jmax(telem.peakLeft, telem.peakRight);
    advice.currentCorrelation = corr;
    advice.currentPeak = peak;
    if (peak < -40.0f)
    { advice.status = TrackPhaseAdvice::Status::NoSignal; return advice; }

    if (corr < 0.0f)
    {
        advice.status = TrackPhaseAdvice::Status::OffTarget;
        advice.correlationDeviation = corr - 0.0f;
        advice.message = "\xf0\x9f\x94\xae " + advice.trackName
            + " correlacion negativa (" + juce::String(corr, 2) + ")";
    }
    else if (corr < 0.3f)
    {
        advice.status = TrackPhaseAdvice::Status::NearTarget;
        advice.correlationDeviation = corr - 0.3f;
        advice.message = "\xf0\x9f\x94\xae " + advice.trackName
            + " correlacion baja (" + juce::String(corr, 2) + ")";
    }
    else
        advice.status = TrackPhaseAdvice::Status::OnTarget;

    return advice;
}

// ═════════════════════
//  SPRINT 8: analyzeAllTracksPhase
// ═════════════════════
std::vector<CoachEngine::TrackPhaseAdvice> CoachEngine::analyzeAllTracksPhase()
{
    std::vector<TrackPhaseAdvice> results;
    auto& registry = sharedData_.getSlotRegistry();
    registry.forEachActive([&](const SlotInfo& info) {
        int idx = info.slotIndex;
        if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;
        auto telem = getLatestTelemetry(idx);
        if (telem.timestamp == 0) return;
        float peak = juce::jmax(telem.peakLeft, telem.peakRight);
        if (peak < -40.0f) return;
        auto phase = analyzeTrackPhase(idx);
        if (phase.isActionable()) results.push_back(phase);
    });
    std::sort(results.begin(), results.end(),
        [](const TrackPhaseAdvice& a, const TrackPhaseAdvice& b) {
            if (a.status != b.status)
                return a.status == TrackPhaseAdvice::Status::OffTarget;
            return std::abs(a.correlationDeviation) > std::abs(b.correlationDeviation);
        });
    return results;
}


// ═══════════════════════════════════════════════════════════════════════════
//  STUB IMPLEMENTATIONS — Methods restored from loss due to git checkout
// ═══════════════════════════════════════════════════════════════════════════ — Methods restored from loss due to git checkout
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::saveSessionMapToJson(juce::DynamicObject& obj) const
{
    sessionMap_.toJson(obj);
}

void CoachEngine::loadSessionMapFromJson(const juce::DynamicObject& obj)
{
    sessionMap_ = SessionMap::fromJson(obj);
}

void CoachEngine::saveDifferenceProfileToJson(juce::DynamicObject& obj) const
{
    differenceProfile_.toJson(obj);
}

void CoachEngine::loadDifferenceProfileFromJson(const juce::DynamicObject& obj)
{
    differenceProfile_ = DifferenceProfile::fromJson(obj);
}

std::vector<AnalyzerInterpretation> CoachEngine::getCurrentInterpretations(
    const juce::String& genre) const
{
    // Get aggregate metrics from shared data and delegate to AnalyzerInterpreter
    auto& registry = sharedData_.getSlotRegistry();
    float correlation = 0.0f, crestDb = 0.0f, centroidRatio = 1.0f;
    float integratedLUFS = -100.0f, truePeakDBTP = -100.0f, lra = 0.0f;
    int count = 0;

    registry.forEachActive([&](const SlotInfo& info) {
        auto result = sharedData_.getTrackAudioResult(info.slotIndex);
        if (result.timestampUs <= 0) return;
        correlation += result.correlation;
        crestDb += result.crestPerBand[0];
        integratedLUFS = juce::jmax(integratedLUFS, result.getRmsCombined() + 2.0f);
        truePeakDBTP = juce::jmax(truePeakDBTP, result.getPeakCombined());
        count++;
    });

    if (count > 0) {
        correlation /= (float)count;
        crestDb /= (float)count;
    }

    return AnalyzerInterpreter::interpretAll(
        correlation, crestDb, centroidRatio,
        integratedLUFS, truePeakDBTP, lra, genre);
}

std::vector<SemanticDiff> CoachEngine::runSemanticAnalysis() const
{
    // Delegate to static SemanticComparator::compareAllTracks
    return SemanticComparator::compareAllTracks(
        sharedData_.getSlotRegistry(), sharedData_, trackRoles_);
}

float CoachEngine::computePerTrackLUFS(const TrackAudioResult& result) noexcept
{
    // Approximate LUFS from RMS + high-frequency boost
    float rmsDb = result.getRmsCombined();
    if (rmsDb < -90.0f)
        return -100.0f;
    // Simple K-weighting approximation: boost high bands
    float highEnergy = 0.0f;
    int highCount = 0;
    for (int b = 20; b < 30 && b < kNumSpectralBands; ++b) {
        if (result.bandEnergies[b] > -80.0f) {
            highEnergy += result.bandEnergies[b];
            highCount++;
        }
    }
    float highBoost = (highCount > 0) ? (highEnergy / highCount) : 0.0f;
    // LUFS ≈ RMS + up to 2.5dB boost from high frequencies
    return rmsDb + juce::jmin(highBoost * 0.1f, 2.5f);
}


// ═══════════════════════════════════════════════════════════════════════════
//  STUB IMPLEMENTATIONS — Methods lost in git checkout, restored minimally
// ═══════════════════════════════════════════════════════════════════════════

juce::String CoachEngine::getBusIcon(BusType bus) noexcept
{
    switch (bus) {
        case BusType::Drums:     return juce::String("🥁");
        case BusType::Bass:      return juce::String("🎸");
        case BusType::Guitars:   return juce::String("🎸");
        case BusType::Keys:      return juce::String("🎹");
        case BusType::Vocals:    return juce::String("🎤");
        case BusType::FX:        return juce::String("🎛️");
        default:                 return juce::String("📡");
    }
}

CoachEngine::CentroidInfo CoachEngine::getCentroidInfo(const juce::String& genre) const
{
    CentroidInfo info;
    info.genre = genre;
    info.expectedHz = expectedCentroidForGenre(genre);
    // Aggregate from active tracks
    auto& registry = sharedData_.getSlotRegistry();
    float totalEnergy = 0.0f, weightedFreq = 0.0f;
    int count = 0;
    registry.forEachActive([&](const SlotInfo& slot) {
        auto result = sharedData_.getTrackAudioResult(slot.slotIndex);
        if (result.timestampUs <= 0) return;
        float energy = result.getRmsCombined() + 100.0f; // offset to avoid negative weights
        if (energy <= 0.0f) return;
        // rough centroid from band energies (weighted average of band indices)
        float sum = 0.0f, weighted = 0.0f;
        for (int b = 0; b < 30; ++b) {
            float e = result.bandEnergies[b] + 100.0f;
            if (e > 0.0f) { sum += e; weighted += e * (float)(b * 200); }
        }
        if (sum > 0.0f) {
            weightedFreq += weighted / sum;
            totalEnergy += energy;
            count++;
        }
    });
    if (count > 0 && totalEnergy > 0.0f)
        info.actualHz = weightedFreq / count;
    return info;
}

void CoachEngine::fastTrackAnalysis()
{
    auto now = juce::Time::getMillisecondCounter() * 1000;
    if (now - lastFastAnalysisUs_ < kFastAnalysisIntervalUs)
        return;
    lastFastAnalysisUs_ = now;

    auto& registry = sharedData_.getSlotRegistry();
    if (registry.activeCount() == 0) return;

    // Quick peak/RMS/correlation check per track
    registry.forEachActive([&](const SlotInfo& info) {
        auto telem = getLatestTelemetry(info.slotIndex);
        if (telem.timestamp == 0) return;
        auto& state = trackStates_[info.slotIndex];
        state.lastPeakDb = juce::jmax(telem.peakLeft, telem.peakRight);
        state.lastRmsDb = telem.rmsLeft;
        state.lastCorrelation = telem.correlation;
    });
}

void CoachEngine::checkAndSendProactiveTip()
{
    auto now = juce::Time::getMillisecondCounter() * 1000;
    if (now - lastProactiveTipTimeUs_ < kProactiveTipIntervalUs)
        return;
    lastProactiveTipTimeUs_ = now;

    auto& registry = sharedData_.getSlotRegistry();
    if (registry.activeCount() == 0) return;

    // Delegate to the existing proactive tip generator
    generateProactiveTip();
}

const CoachEngine::GenreTargetProfile& CoachEngine::getGenreProfile(const juce::String& genre)
{
    // Static default profile
    static const GenreTargetProfile defaultProfile = {
        -14.0f,     // targetIntegratedLUFS
        2.0f,       // lufsTolerance
        12.0f,      // targetCrestFactor
        4.0f,       // crestTolerance
        -6.0f,      // targetHeadroomDb
        "Default mix profile",
        0.0f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f
    };
    juce::ignoreUnused(genre);
    return defaultProfile;
}

void CoachEngine::respondWithPremium(const juce::String& text, MentorMessage::Type type)
{
    // Same as respondWith() but intended for conversational bubbles
    // The chat UI will render it as isSystem=false
    respondWith(text, type);
}

void CoachEngine::respondWithCorrectionFeedback(const juce::String& text)
{
    // Wrapper that sends correction feedback as premium message
    respondWithPremium(text, MentorMessage::Type::Info);
}

std::vector<juce::String> CoachEngine::getDynamicSuggestions() const
{
    std::vector<juce::String> suggestions;
    auto& registry = sharedData_.getSlotRegistry();
    int active = registry.activeCount();

    if (active == 0) {
        suggestions.push_back("¿Cómo empiezo?");
        return suggestions;
    }

    suggestions.push_back("¿Cómo va la mezcla?");
    suggestions.push_back("Revisa niveles");

    auto phase = phaseManager_.getCurrentPhase();
    switch (phase) {
        case MentorPhase::Organizacion:
            suggestions.push_back("Nombra las pistas");
            suggestions.push_back("Asigna buses");
            break;
        case MentorPhase::GainStaging:
            suggestions.push_back("¿Hay clipping?");
            suggestions.push_back("Headroom suficiente");
            break;
        case MentorPhase::Balance:
            suggestions.push_back("¿Balance tonal?");
            suggestions.push_back("Espectro ok?");
            break;
        case MentorPhase::Compresion:
            suggestions.push_back("¿Demasiada compresión?");
            suggestions.push_back("Rango dinámico");
            break;
        case MentorPhase::Espacio:
            suggestions.push_back("¿Problemas de fase?");
            suggestions.push_back("Ancho estéreo");
            break;
        default:
            break;
    }

    if (hasReference())
        suggestions.push_back("¿Cómo sueno vs referencia?");
    if (active >= 3)
        suggestions.push_back("Enmascaramiento?");

    return suggestions;
}

DifferenceProfile CoachEngine::buildDifferenceProfile() const
{
    DifferenceProfile profile;
    profile.timestampUs = juce::Time::getMillisecondCounter() * 1000;
    if (referenceFingerprint_.valid) {
        profile.referenceName = referenceMetadata_.name;
        profile.refIntegratedLUFS = referenceFingerprint_.lufsIntegrated;
        profile.refShortTermLUFS = referenceFingerprint_.lufsShortTerm;
        profile.refMomentaryLUFS = referenceFingerprint_.lufsMomentary;
        profile.refCrestFactor = referenceFingerprint_.crestFactor;
        profile.refCorrelation = referenceFingerprint_.correlation;
        profile.refTruePeakDBTP = referenceFingerprint_.truePeakDBTP;
        // Map 30-band to 6-region
        for (int r = 0; r < 6; ++r) {
            int start = r * 5;
            float sum = 0.0f;
            for (int b = start; b < start + 5 && b < 30; ++b)
                sum += referenceFingerprint_.bandEnergies[b];
            profile.refRegionEnergy[r] = sum / 5.0f;
        }
    }
    return profile;
}

juce::String CoachEngine::buildSessionMapText() const
{
    // Delegate to the SessionMap::toText() via buildSessionMap()
    auto map = buildSessionMap();
    return map.toText();
}

float CoachEngine::expectedCentroidForGenre(const juce::String& genre) noexcept
{
    juce::ignoreUnused(genre);
    // Default centroid ~800Hz (generic mix balance)
    return 800.0f;
}

SessionMap CoachEngine::buildSessionMap() const
{
    SessionMap map;
    auto& registry = sharedData_.getSlotRegistry();
    map.timestampUs = juce::Time::getMillisecondCounter() * 1000;
    map.totalTracks = registry.activeCount();
    // Build minimal category list
    auto categoryName = [](RoleCategory cat) -> const char* {
        switch (cat) {
            case RoleCategory::Drums:   return "BATERIA";
            case RoleCategory::Bass:    return "BAJO";
            case RoleCategory::Guitars: return "GUITARRAS";
            case RoleCategory::Keys:    return "TECLADOS";
            case RoleCategory::Vocals:  return "VOCES";
            case RoleCategory::FX:      return "EFECTOS";
            case RoleCategory::Melody:  return "MELODICOS";
            default:                    return "OTROS";
        }
    };
    auto categoryEmoji = [](RoleCategory cat) -> const char* {
        switch (cat) {
            case RoleCategory::Drums:   return "🥁";
            case RoleCategory::Bass:    return "🎸";
            case RoleCategory::Guitars: return "🎸";
            case RoleCategory::Keys:    return "🎹";
            case RoleCategory::Vocals:  return "🎤";
            case RoleCategory::FX:      return "🎛️";
            case RoleCategory::Melody:  return "🎵";
            default:                    return "📡";
        }
    };
    std::vector<RoleCategory> categories = {
        RoleCategory::Drums, RoleCategory::Bass, RoleCategory::Guitars,
        RoleCategory::Keys, RoleCategory::Vocals, RoleCategory::FX,
        RoleCategory::Melody
    };
    for (auto cat : categories) {
        SessionMapCategory mapCat;
        mapCat.name = juce::String(categoryName(cat));
        mapCat.emoji = juce::String(categoryEmoji(cat));
        registry.forEachActive([&](const SlotInfo& info) {
            int idx = info.slotIndex;
            if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;
            if (getRoleCategory(trackRoles_[idx]) != cat) return;
            SessionMapEntry entry;
            entry.trackName = juce::String(info.trackName).trim();
            entry.roleName = juce::String(getRoleName(trackRoles_[idx]));
            entry.slotIndex = idx;
            entry.busType = (int)info.bus;
            entry.trackType = info.trackType;
            entry.confidence = trackRoleWasInferred_[idx] ? 0.7f : 1.0f;
            auto telem = getLatestTelemetry(idx);
            entry.hasSignal = (telem.timestamp > 0);
            if (entry.hasSignal)
                entry.peakDb = juce::jmax(telem.peakLeft, telem.peakRight);
            mapCat.tracks.push_back(entry);
        });
        if (!mapCat.tracks.empty())
            map.categories.push_back(mapCat);
    }
    return map;
}


// ═══════════════════════════════════════════════════════════════════════════
//  Sprint 2: onMapConfirmed — Called when user confirms the mix map.
//  Updates OrganizacionMetrics with routingValidated=true and triggers
//  evaluateAndAutoAdvance(). If phase advances, sends guidance message.
// ═══════════════════════════════════════════════════════════════════════════
void CoachEngine::onMapConfirmed()
{
    auto& registry = sharedData_.getSlotRegistry();
    int bussedCount = 0;
    int totalActive = 0;
    int identifiedCount = 0;

    registry.forEachActive([&](const SlotInfo& info) {
        totalActive++;
        if (info.bus != BusType::None)
            bussedCount++;
        int idx = info.slotIndex;
        if (idx >= 0 && idx < SlotRegistry::kMaxSlots
            && trackRoles_[idx] != TrackRole::Unknown
            && trackRoles_[idx] != TrackRole::Master)
            identifiedCount++;
    });

    // Build OrganizacionMetrics with routingValidated=true
    OrganizacionMetrics metrics;
    metrics.totalTracks      = totalActive;
    metrics.bussedTracks     = bussedCount;
    metrics.identifiedTracks = identifiedCount;
    metrics.routingValidated = true;
    metrics.hasData          = totalActive > 0;

    // Push metrics to PhaseManager
    phaseManager_.setOrganizacionMetrics(metrics);

    LogHelper::writeToLog("[Sprint2] Mapa confirmado: "
        + juce::String(bussedCount) + "/" + juce::String(totalActive)
        + " bussed, " + juce::String(identifiedCount) + " identified");

    // Check if phase auto-advances
    if (phaseManager_.getCurrentPhase() == MentorPhase::Organizacion
        && phaseManager_.evaluateAndAutoAdvance())
    {
        auto newPhase = phaseManager_.getCurrentPhase();
        juce::String msg;

        msg += "\xF0\x9F\x97\xBA **Organizaci\xC3\xB3n completa!** Avanzamos a **"
             + juce::String(phaseNames[static_cast<int>(newPhase)])
             + "**.\n\n";

        msg += "\xF0\x9F\x93\x8B Ahora ajustemos niveles para tener headroom saludable.";
        respondWith(msg, MentorMessage::Type::Achievement);

        // Send phase guidance for the new phase
        sendPhaseGuidance(newPhase);

        LogHelper::writeToLog("[Sprint2] Fase avanzada: Organizacion -> "
                              + juce::String(phaseNames[static_cast<int>(newPhase)]));
    }
    else
    {
        // Phase not complete yet \u2014 acknowledge the map confirmation
        respondWith("\xE2\x9C\x85 **Mapa de mezcla confirmado.** "
                    + juce::String(bussedCount) + "/" + juce::String(totalActive)
                    + " pistas con bus asignado.",
                    MentorMessage::Type::Info);
    }
}




} // namespace mixcoach
