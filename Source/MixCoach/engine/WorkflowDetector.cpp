#include "WorkflowDetector.h"
#include <cmath>
#include <algorithm>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  ImpactData — Resumen legible del impacto en el master
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String ImpactData::toImpactMessage() const
    {
        if (!valid) return {};

        juce::String msg;
        bool hasAny = false;

        // Correlation change (umbral: 0.10)
        float corrDiff = correlationAfter - correlationBefore;
        if (std::abs(corrDiff) >= 0.10f) {
            if (!hasAny) {
                msg += "[CHART] ";
                hasAny = true;
            }
            msg += "Correlaci\xC3\xB3n: " + juce::String(correlationBefore, 2) + " [RIGHT] "
                   + juce::String(correlationAfter, 2) + " (";
            if (corrDiff > 0) msg += "mejor\xC3\xB3";
            else
                msg += "empeor\xC3\xB3";
            msg += " " + juce::String(std::abs(corrDiff), 2) + ")";
        }

        // LUFS Short-Term change (umbral: 1.0 LU)
        if (lufsBefore > -80.0f && lufsAfter > -80.0f) {
            float lufsDiff = lufsAfter - lufsBefore;
            if (std::abs(lufsDiff) >= 1.0f) {
                if (!hasAny) {
                    msg += "[CHART] ";
                    hasAny = true;
                }
                else {
                    msg += " | ";
                }
                msg += "LUFS: " + juce::String(lufsBefore, 1) + " [RIGHT] " + juce::String(lufsAfter, 1) + " LU ("
                       + (lufsDiff > 0 ? "m\xC3\xA1s fuerte" : "m\xC3\xA1s suave") + ")";
            }
        }

        // Stereo width change (umbral: 0.08)
        float widthDiff = stereoWidthAfter - stereoWidthBefore;
        if (std::abs(widthDiff) >= 0.08f && stereoWidthBefore > 0.0f && stereoWidthAfter > 0.0f) {
            if (!hasAny) {
                msg += "[CHART] ";
                hasAny = true;
            }
            else {
                msg += " | ";
            }
            msg += "Ancho est\xC3\xA9reo: " + juce::String(stereoWidthBefore, 2) + " [RIGHT] "
                   + juce::String(stereoWidthAfter, 2) + " (" + (widthDiff > 0 ? "\xC3\xA1" : "se")
                   + "ampli\xC3\xB3" // split for readability
                   + ")";
        }

        // Crest factor change (umbral: 2.0 dB)
        if (crestBefore > 0.0f && crestAfter > 0.0f) {
            float crestDiff = crestAfter - crestBefore;
            if (std::abs(crestDiff) >= 2.0f) {
                if (!hasAny) {
                    msg += "[CHART] ";
                    hasAny = true;
                }
                else {
                    msg += " | ";
                }
                msg += "Crest: " + juce::String(crestBefore, 1) + " [RIGHT] " + juce::String(crestAfter, 1)
                       + " dB (" + (crestDiff > 0 ? "m\xC3\xA1s din\xC3\xA1mico" : "m\xC3\xA1s comprimido") + ")";
            }
        }

        // Centroid change (umbral: 0.10 ratio)
        if (centroidBefore > 0.0f && centroidAfter > 0.0f) {
            float centroidDiff = centroidAfter - centroidBefore;
            if (std::abs(centroidDiff) >= 0.10f) {
                if (!hasAny) {
                    msg += "[CHART] ";
                    hasAny = true;
                }
                else {
                    msg += " | ";
                }
                msg += "Centroide: " + juce::String(centroidBefore, 2) + " [RIGHT] "
                       + juce::String(centroidAfter, 2) + " ("
                       + (centroidDiff > 0 ? "espectro m\xC3\xA1s brillante" : "espectro m\xC3\xA1s oscuro") + ")";
            }
        }

        // True Peak change (umbral: 1.0 dB)
        if (truePeakBefore > -60.0f && truePeakAfter > -60.0f) {
            float tpDiff = truePeakAfter - truePeakBefore;
            if (std::abs(tpDiff) >= 1.0f) {
                if (!hasAny) {
                    msg += "[CHART] ";
                    hasAny = true;
                }
                else {
                    msg += " | ";
                }
                msg += "True Peak: " + juce::String(truePeakBefore, 1) + " [RIGHT] "
                       + juce::String(truePeakAfter, 1) + " dBTP";
                if (truePeakAfter > -1.0f && truePeakBefore > -1.0f) msg += " [WARN] CLIPPING!";
                else if (truePeakAfter < -6.0f && truePeakBefore >= -6.0f)
                    msg += " \xE2\x9C\xA8 headroom recuperado!";
            }
        }

        // LRA change (umbral: 2.0 LU)
        if (lraBefore > 0.0f && lraAfter > 0.0f) {
            float lraDiff = lraAfter - lraBefore;
            if (std::abs(lraDiff) >= 2.0f) {
                if (!hasAny) {
                    msg += "[CHART] ";
                    hasAny = true;
                }
                else {
                    msg += " | ";
                }
                msg += "Rango din\xC3\xA1mico: " + juce::String(lraBefore, 1) + " [RIGHT] "
                       + juce::String(lraAfter, 1) + " LU (" + (lraDiff > 0 ? "mayor variedad" : "m\xC3\xA1s uniforme")
                       + ")";
            }
        }

        if (hasAny) msg += "\n";

        return msg;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Ayudante: toChatMessage — Construye mensaje contextual listo para el chat
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String WorkflowEvent::toChatMessage(const juce::String& name) const
    {
        juce::String msg;

        switch (action) {
            case WorkflowAction::FaderUp:
                msg = "🎚️ **" + name + "** subiste el nivel **" + juce::String(magnitude, 1) + " dB** (de "
                      + juce::String(beforeValue, 1) + " a " + juce::String(afterValue, 1) + " dB). "
                      + "Buena jugada — revisa que no enmascare otras pistas en el bus.";
                break;

            case WorkflowAction::FaderDown:
                msg = "🎚️ **" + name + "** bajaste el nivel **" + juce::String(magnitude, 1) + " dB** (de "
                      + juce::String(beforeValue, 1) + " a " + juce::String(afterValue, 1) + " dB). "
                      + "El headroom del master debería mejorar.";
                break;

            case WorkflowAction::EQBoost:
                msg = "🎛️ Detecto un realce en **" + regionLabel + "** (" + juce::String(frequencyHint, 0)
                      + " Hz) en **" + name + "** de **" + juce::String(magnitude, 1) + " dB**. ";
                if (regionLabel == "Presence" || regionLabel == "Air")
                    msg += "Cuidado con la sibilancia — monitorea en 5-8 kHz.";
                else if (regionLabel == "Sub" || regionLabel == "Bass")
                    msg += "Revisa que no haya enmascaramiento con el bombo.";
                else
                    msg += "El cambio suena limpio — verifica el balance general.";
                break;

            case WorkflowAction::EQCut:
                msg = "🎛️ Detecto un corte en **" + regionLabel + "** (" + juce::String(frequencyHint, 0) + " Hz) en **"
                      + name + "** de **" + juce::String(magnitude, 1) + " dB**. "
                      + "Buen movimiento para limpiar el espectro.";
                break;

            case WorkflowAction::CompressionOn:
                msg = "⚡ **" + name + "** — el crest factor bajó **" + juce::String(magnitude, 1)
                      + " dB** (dinámica más controlada). " + "Prueba attack rápido (~10ms) y release medio (~80ms) "
                      + "si buscas un sonido más ajustado.";
                break;

            case WorkflowAction::CompressionOff:
                msg = "⚡ **" + name + "** — la dinámica aumentó **" + juce::String(magnitude, 1)
                      + " dB** (crest factor más alto). " + "La pista respira más natural ahora.";
                break;

            case WorkflowAction::PanLeft:
                msg = "🔄 **" + name + "** la moviste **" + juce::String(magnitude, 1)
                      + " dB** a la izquierda. El balance estéreo se está definiendo.";
                break;

            case WorkflowAction::PanRight:
                msg = "🔄 **" + name + "** la moviste **" + juce::String(magnitude, 1)
                      + " dB** a la derecha. El balance estéreo se está definiendo.";
                break;

            case WorkflowAction::StereoWiden:
                msg = "🌐 **" + name + "** — la imagen estéreo se ensanchó " + juce::String(magnitude, 2) + ". "
                      + "Verifica compatibilidad mono si usas ensanchadores.";
                break;

            case WorkflowAction::Muted:
                msg = "🔇 **" + name + "** silenciada. " + "A veces menos es más — buen criterio.";
                break;

            case WorkflowAction::Unmuted:
                msg = "🔊 **" + name + "** reactivada. Bienvenida de vuelta.";
                break;
        }

        // ═══ Añadir impacto en el master si está disponible ═══════════════════
        if (impact.valid) {
            juce::String impactMsg = impact.toImpactMessage();
            if (impactMsg.isNotEmpty()) {
                if (msg.isNotEmpty()) msg += "\n" + impactMsg;
                else
                    msg = impactMsg;
            }
        }

        return msg;
    }

    juce::String WorkflowEvent::toShortSummary() const
    {
        juce::String s;
        switch (action) {
            case WorkflowAction::FaderUp:
                s = "nivel +" + juce::String(magnitude, 1) + "dB";
                break;
            case WorkflowAction::FaderDown:
                s = "nivel " + juce::String(magnitude, 1) + "dB";
                break;
            case WorkflowAction::EQBoost:
                s = "EQ+" + regionLabel + " " + juce::String(magnitude, 1) + "dB";
                break;
            case WorkflowAction::EQCut:
                s = "EQ-" + regionLabel + " " + juce::String(magnitude, 1) + "dB";
                break;
            case WorkflowAction::CompressionOn:
                s = "compresi\xC3\xB3"
                    "n "
                    + juce::String(magnitude, 1) + "dB crest";
                break;
            case WorkflowAction::CompressionOff:
                s = "expansion " + juce::String(magnitude, 1) + "dB crest";
                break;
            case WorkflowAction::PanLeft:
                s = "pan L " + juce::String(magnitude, 1) + "dB";
                break;
            case WorkflowAction::PanRight:
                s = "pan R " + juce::String(magnitude, 1) + "dB";
                break;
            case WorkflowAction::StereoWiden:
                s = "stereo +" + juce::String(magnitude, 2);
                break;
            case WorkflowAction::Muted:
                s = "muted";
                break;
            case WorkflowAction::Unmuted:
                s = "unmuted";
                break;
            case WorkflowAction::TrackAdded:
                s = "added";
                break;
            case WorkflowAction::TrackRemoved:
                s = "removed";
                break;
            default:
                s = "";
                break;
        }

        // ═══ Incluir cambio de correlación si es significativo ════════════════
        if (impact.valid) {
            float corrDiff = impact.correlationAfter - impact.correlationBefore;
            if (std::abs(corrDiff) >= 0.10f) {
                s += " | corr " + juce::String(impact.correlationBefore, 2) + "[RIGHT]"
                     + juce::String(impact.correlationAfter, 2);
            }
            if (impact.lufsBefore > -80.0f && impact.lufsAfter > -80.0f
                && std::abs(impact.lufsAfter - impact.lufsBefore) >= 1.0f) {
                s += " | LUFS " + juce::String(impact.lufsBefore, 1) + "[RIGHT]"
                     + juce::String(impact.lufsAfter, 1);
            }
        }

        return s;
    }

    juce::String WorkflowEvent::toString() const
    {
        juce::String s = "[Workflow] " + trackName + " => ";
        s += toShortSummary();
        s += " (mag=" + juce::String(magnitude, 1) + " before=" + juce::String(beforeValue, 1)
             + " after=" + juce::String(afterValue, 1) + ")";
        return s;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateTracking — Almacena el snapshot actual de telemetría para una pista
    // ═══════════════════════════════════════════════════════════════════════════
    void WorkflowDetector::updateTracking(int slotIndex, const TrackAudioResult& result, const juce::String& trackName)
    {
        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) return;

        const juce::ScopedLock lock(lock_);

        auto& track = tracks_[slotIndex];

        // Mover baseline actual a "previous" (para comparación)
        track.previous = track.baseline;

        // Actualizar baseline con los nuevos datos
        auto& bl          = track.baseline;
        bl.peakCombined   = result.getPeakCombined();
        bl.rmsCombined    = result.getRmsCombined();
        bl.correlation    = result.correlation;
        bl.transientRatio = result.transientRatio;
        bl.attackTimeMs   = result.attackTimeMs;
        bl.releaseTimeMs  = result.releaseTimeMs;
        bl.sustainLevelDb = result.sustainLevelDb;

        // Crest factor = peak - RMS
        bl.crestFactor = (bl.rmsCombined > -80.0f) ? bl.peakCombined - bl.rmsCombined : 0.0f;

        // L/R balance
        bl.lrBalance = (result.peakLeft > -80.0f && result.peakRight > -80.0f) ? result.peakLeft - result.peakRight
                                                                               : 0.0f;

        // Stereo width promedio (6 bandas)
        {
            float sum = 0.0f;
            for (int b = 0; b < 6; ++b) sum += result.stereoWidthPerBand[b];
            bl.avgStereoWidth = sum / 6.0f;
        }

        // 30-band spectral energy
        for (int b = 0; b < 30; ++b) bl.bandEnergies[b] = result.bandEnergies[b];

        // 6-region profile
        computeRegionProfile(result.bandEnergies, bl.regionProfile);

        // Estado de actividad
        track.wasActive = true;
        track.trackName = trackName;

        bl.valid = true;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  detectChanges — Compara baseline vs previous y genera eventos
    // ═══════════════════════════════════════════════════════════════════════════
    std::vector<WorkflowEvent> WorkflowDetector::detectChanges(int64_t nowUs)
    {
        const juce::ScopedLock lock(lock_);

        std::vector<WorkflowEvent> newEvents;

        for (int i = 0; i < SlotRegistry::kMaxSlots; ++i) {
            auto& track = tracks_[i];

            // Solo procesar pistas con baseline válida y datos previos
            if (!track.baseline.valid || !track.previous.valid) continue;

            // Determinar estado de silencio desde datos crudos de baseline,
            // NO desde track.wasSilent (que se actualiza en updateTracking
            // antes de detectChanges, haciendo la comparación inválida)
            bool wasPrevSilent     = (track.previous.peakCombined < kSilenceThresholdDb);
            bool isCurrentlySilent = (track.baseline.peakCombined < kSilenceThresholdDb);

            // Silencio continuado: no detectar cambios
            if (wasPrevSilent && isCurrentlySilent) continue;

            detectFaderChange(track, i, nowUs, newEvents);
            detectEQChange(track, i, nowUs, newEvents);
            detectDynamicsChange(track, i, nowUs, newEvents);
            detectPanChange(track, i, nowUs, newEvents);
            detectMuteChange(track, i, nowUs, newEvents);
        }

        // ═══ Compute master impact for each new event ═══════════════════════
        for (auto& ev : newEvents) {
            ev.impact = computeImpact(masterBefore_, masterCurrent_);
        }

        // Almacenar eventos nuevos en el buffer circular
        for (const auto& ev : newEvents) {
            if ((int)recentEvents_.size() >= kMaxRecentEvents) recentEvents_.erase(recentEvents_.begin());
            recentEvents_.push_back(ev);
        }

        return newEvents;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  captureMasterSnapshot — Guarda el snapshots del master para detectar impacto
    //  masterBefore_ se actualiza con el valor anterior de masterCurrent_
    //  masterCurrent_ se actualiza con los nuevos datos
    // ═══════════════════════════════════════════════════════════════════════════
    void WorkflowDetector::captureMasterSnapshot(const MasterSnapshot& master)
    {
        const juce::ScopedLock lock(lock_);

        // Shift: el "antes" pasa a ser lo que era el "ahora" en el ciclo anterior
        masterBefore_ = masterCurrent_;

        // El "ahora" actual
        masterCurrent_       = master;
        masterCurrent_.valid = true;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  computeImpact — Compara dos snapshots del master y genera ImpactData
    // ═══════════════════════════════════════════════════════════════════════════
    ImpactData WorkflowDetector::computeImpact(const MasterSnapshot& before, const MasterSnapshot& after) const
    {
        ImpactData impact;

        // Sin datos previos válidos — no podemos calcular impacto
        if (!before.valid || !after.valid || before.timestampUs == 0) return impact;

        impact.valid = true;

        impact.correlationBefore = before.correlation;
        impact.correlationAfter  = after.correlation;

        impact.lufsBefore = before.lufsShortTerm;
        impact.lufsAfter  = after.lufsShortTerm;

        impact.stereoWidthBefore = before.avgStereoWidth;
        impact.stereoWidthAfter  = after.avgStereoWidth;

        impact.crestBefore = before.crestFactor;
        impact.crestAfter  = after.crestFactor;

        impact.centroidBefore = before.centroidRatio;
        impact.centroidAfter  = after.centroidRatio;

        impact.truePeakBefore = before.truePeakDBTP;
        impact.truePeakAfter  = after.truePeakDBTP;

        impact.lraBefore = before.lra;
        impact.lraAfter  = after.lra;

        return impact;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getRecentEvents — Para AiCoachAdapter
    // ═══════════════════════════════════════════════════════════════════════════
    std::vector<WorkflowEvent> WorkflowDetector::getRecentEvents(int maxEvents) const
    {
        const juce::ScopedLock lock(lock_);

        int64_t cutoff = (juce::Time::getMillisecondCounter() * 1000) - 300 * 1000 * 1000; // Últimos 5 minutos

        std::vector<WorkflowEvent> result;
        for (int i = (int)recentEvents_.size() - 1; i >= 0 && (int)result.size() < maxEvents; --i) {
            if (recentEvents_[i].timestampUs >= cutoff) result.push_back(recentEvents_[i]);
        }

        return result;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Reset
    // ═══════════════════════════════════════════════════════════════════════════
    void WorkflowDetector::resetTrack(int slotIndex)
    {
        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) return;
        const juce::ScopedLock lock(lock_);
        tracks_[slotIndex] = TrackState{};
    }

    void WorkflowDetector::resetAll()
    {
        const juce::ScopedLock lock(lock_);
        for (auto& track : tracks_) track = TrackState{};
        recentEvents_.clear();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  detectFaderChange — Nivel general cambió, espectro se mantiene
    // ═══════════════════════════════════════════════════════════════════════════
    void WorkflowDetector::detectFaderChange(TrackState& track,
                                             int slotIndex,
                                             int64_t nowUs,
                                             std::vector<WorkflowEvent>& events)
    {
        juce::ignoreUnused(slotIndex);

        // Cooldown
        if (nowUs - track.lastFaderEventUs < kFaderCooldownUs) return;

        auto& cur  = track.baseline;
        auto& prev = track.previous;

        // Ambos deben tener señal válida
        if (cur.peakCombined < -50.0f || prev.peakCombined < -50.0f) return;

        float peakDelta = cur.peakCombined - prev.peakCombined; // + = subió

        // Umbral mínimo
        if (std::abs(peakDelta) < kFaderThresholdDb) return;

        // VERIFICACIÓN: ¿el espectro se mantuvo similar?
        // Si el usuario solo movió el fader, el perfil espectral debe ser
        // muy similar (solo cambió la ganancia general, no la forma)
        float sim = spectralSimilarity(cur.bandEnergies, prev.bandEnergies);

        // Fader = cambio de nivel + espectro similar
        if (sim >= kSpectralSimilarity) {
            WorkflowEvent ev;
            ev.action        = (peakDelta > 0) ? WorkflowAction::FaderUp : WorkflowAction::FaderDown;
            ev.slotIndex     = slotIndex;
            ev.trackName     = track.trackName;
            ev.magnitude     = std::abs(peakDelta);
            ev.metricChanged = "Peak";
            ev.beforeValue   = prev.peakCombined;
            ev.afterValue    = cur.peakCombined;
            ev.timestampUs   = nowUs;
            events.push_back(ev);
            track.lastFaderEventUs = nowUs;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  detectEQChange — Espectro cambió, nivel se mantiene
    // ═══════════════════════════════════════════════════════════════════════════
    void WorkflowDetector::detectEQChange(TrackState& track,
                                          int slotIndex,
                                          int64_t nowUs,
                                          std::vector<WorkflowEvent>& events)
    {
        juce::ignoreUnused(slotIndex);

        // Cooldown
        if (nowUs - track.lastEQEventUs < kEQCooldownUs) return;

        auto& cur  = track.baseline;
        auto& prev = track.previous;

        // Ambos deben tener señal válida
        if (cur.peakCombined < -50.0f || prev.peakCombined < -50.0f) return;

        float peakDelta = std::abs(cur.peakCombined - prev.peakCombined);
        float rmsDelta  = std::abs(cur.rmsCombined - prev.rmsCombined);

        // EQ: nivel general se mantiene relativamente estable,
        // pero el espectro cambia significativamente
        if (peakDelta > kEQLevelStabilityDb || rmsDelta > kEQLevelStabilityDb) return;

        float sim = spectralSimilarity(cur.bandEnergies, prev.bandEnergies);

        // EQ = cambio espectral significativo + nivel estable
        if (sim >= kSpectralDissimilarity) return; // Todavía es similar — no es EQ

        // Encontrar la región más afectada
        float magnitudeDb = 0.0f;
        int worstReg      = findWorstRegion(prev.regionProfile, cur.regionProfile, magnitudeDb);

        if (magnitudeDb < kEQSpectralThreshold) return;

        WorkflowEvent ev;
        ev.slotIndex     = slotIndex;
        ev.trackName     = track.trackName;
        ev.magnitude     = magnitudeDb;
        ev.metricChanged = juce::String(regionName(worstReg));
        ev.beforeValue   = prev.regionProfile[worstReg];
        ev.afterValue    = cur.regionProfile[worstReg];
        ev.timestampUs   = nowUs;
        ev.regionLabel   = regionName(worstReg);
        ev.frequencyHint = regionCenterFreq(worstReg);

        // ¿Boost o cut?
        float diff = cur.regionProfile[worstReg] - prev.regionProfile[worstReg];
        ev.action  = (diff > 0) ? WorkflowAction::EQBoost : WorkflowAction::EQCut;

        events.push_back(ev);
        track.lastEQEventUs = nowUs;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  detectDynamicsChange — Crest factor / transient ratio cambia
    // ═══════════════════════════════════════════════════════════════════════════
    void WorkflowDetector::detectDynamicsChange(TrackState& track,
                                                int slotIndex,
                                                int64_t nowUs,
                                                std::vector<WorkflowEvent>& events)
    {
        juce::ignoreUnused(slotIndex);

        // Cooldown
        if (nowUs - track.lastDynamicsEventUs < kDynamicsCooldownUs) return;

        auto& cur  = track.baseline;
        auto& prev = track.previous;

        // Ambos deben tener crest válido
        if (cur.crestFactor <= 0.0f || prev.crestFactor <= 0.0f) return;
        if (cur.rmsCombined < -50.0f || prev.rmsCombined < -50.0f) return;

        float crestDelta = std::abs(cur.crestFactor - prev.crestFactor);

        if (crestDelta < kCrestThresholdDb) return;

        WorkflowEvent ev;
        ev.slotIndex     = slotIndex;
        ev.trackName     = track.trackName;
        ev.magnitude     = crestDelta;
        ev.metricChanged = "Crest Factor";
        ev.beforeValue   = prev.crestFactor;
        ev.afterValue    = cur.crestFactor;
        ev.timestampUs   = nowUs;

        // Crest bajó → compresión aplicada
        // Crest subió → compresión reducida o eliminada
        ev.action = (cur.crestFactor < prev.crestFactor) ? WorkflowAction::CompressionOn
                                                         : WorkflowAction::CompressionOff;

        events.push_back(ev);
        track.lastDynamicsEventUs = nowUs;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  detectPanChange — Balance L/R o stereo width cambia
    // ═══════════════════════════════════════════════════════════════════════════
    void WorkflowDetector::detectPanChange(TrackState& track,
                                           int slotIndex,
                                           int64_t nowUs,
                                           std::vector<WorkflowEvent>& events)
    {
        juce::ignoreUnused(slotIndex);

        // Cooldown
        if (nowUs - track.lastPanEventUs < kPanCooldownUs) return;

        auto& cur  = track.baseline;
        auto& prev = track.previous;

        // L/R balance change
        if (cur.lrBalance > -50.0f && prev.lrBalance > -50.0f) {
            float lrDelta = std::abs(cur.lrBalance - prev.lrBalance);

            if (lrDelta >= kPanThresholdDb) {
                WorkflowEvent ev;
                ev.slotIndex     = slotIndex;
                ev.trackName     = track.trackName;
                ev.magnitude     = lrDelta;
                ev.metricChanged = "L/R Balance";
                ev.beforeValue   = prev.lrBalance;
                ev.afterValue    = cur.lrBalance;
                ev.timestampUs   = nowUs;

                float diff = cur.lrBalance - prev.lrBalance;
                ev.action  = (diff > 0) ? WorkflowAction::PanLeft : WorkflowAction::PanRight;

                events.push_back(ev);
                track.lastPanEventUs = nowUs;
                return; // Priorizar pan sobre stereo width (no ambos)
            }
        }

        // Stereo width change
        if (cur.avgStereoWidth > 0.0f && prev.avgStereoWidth > 0.0f) {
            float widthDelta = std::abs(cur.avgStereoWidth - prev.avgStereoWidth);

            if (widthDelta >= kStereoThreshold) {
                WorkflowEvent ev;
                ev.action        = WorkflowAction::StereoWiden;
                ev.slotIndex     = slotIndex;
                ev.trackName     = track.trackName;
                ev.magnitude     = widthDelta;
                ev.metricChanged = "Stereo Width";
                ev.beforeValue   = prev.avgStereoWidth;
                ev.afterValue    = cur.avgStereoWidth;
                ev.timestampUs   = nowUs;
                events.push_back(ev);
                track.lastPanEventUs = nowUs;
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  detectMuteChange — Señal cae a silencio o se reactiva
    //  Usa track.previous.peakCombined para el estado anterior (NO track.wasSilent,
    //  que se sobreescribe en updateTracking antes de que detectChanges se ejecute).
    // ═══════════════════════════════════════════════════════════════════════════
    void WorkflowDetector::detectMuteChange(TrackState& track,
                                            int slotIndex,
                                            int64_t nowUs,
                                            std::vector<WorkflowEvent>& events)
    {
        juce::ignoreUnused(slotIndex);

        // Cooldown
        if (nowUs - track.lastMuteEventUs < kMuteCooldownUs) return;

        auto& cur = track.baseline;

        bool currentlySilent = (cur.peakCombined < kSilenceThresholdDb);
        bool wasPrevSilent   = (track.previous.peakCombined < kSilenceThresholdDb);

        // Mute: estaba con señal, ahora silencio
        if (currentlySilent && !wasPrevSilent && track.wasActive) {
            WorkflowEvent ev;
            ev.action      = WorkflowAction::Muted;
            ev.slotIndex   = slotIndex;
            ev.trackName   = track.trackName;
            ev.timestampUs = nowUs;
            events.push_back(ev);
            track.lastMuteEventUs = nowUs;
        }
        // Unmute: estaba en silencio, ahora tiene señal
        else if (!currentlySilent && wasPrevSilent) {
            WorkflowEvent ev;
            ev.action      = WorkflowAction::Unmuted;
            ev.slotIndex   = slotIndex;
            ev.trackName   = track.trackName;
            ev.timestampUs = nowUs;
            events.push_back(ev);
            track.lastMuteEventUs = nowUs;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Helpers espectrales
    // ═══════════════════════════════════════════════════════════════════════════

    float WorkflowDetector::spectralSimilarity(const float a[30], const float b[30])
    {
        // Similitud coseno entre dos vectores de 30 bandas
        double dot = 0.0, normA = 0.0, normB = 0.0;
        int activeBands = 0;

        for (int i = 0; i < 30; ++i) {
            // Solo bandas con energía significativa (> -60dB) para evitar
            // falsos positivos cuando ambos espectros están en silencio
            if (a[i] > -60.0f && b[i] > -60.0f) {
                double va = static_cast<double>(a[i] + 100.0); // Shift para evitar negativos
                double vb = static_cast<double>(b[i] + 100.0);
                dot += va * vb;
                normA += va * va;
                normB += vb * vb;
                ++activeBands;
            }
        }

        // Si hay muy pocas bandas activas (< 3), no podemos computar similitud
        // con fiabilidad. Retornar 1.0 (similar) para que el detector no clasifique
        // erróneamente silencio como "cambio espectral".
        if (activeBands < 3 || normA < 1e-10 || normB < 1e-10) return 1.0f;

        double sim = dot / (std::sqrt(normA) * std::sqrt(normB));
        return static_cast<float>(juce::jlimit(0.0, 1.0, sim));
    }

    void WorkflowDetector::computeRegionProfile(const float bandEnergies[30], float regionProfile[6])
    {
        // Regiones espectrales (6 agrupaciones de las 30 bandas):
        // Sub:    bandas 0-1   (0-86 Hz)
        // Bass:   bandas 2-4   (86-301 Hz)
        // LoMid:  bandas 5-9   (301-1076 Hz)
        // HiMid:  bandas 10-17 (1076-3532 Hz)
        // Pres:   bandas 18-24 (3532-8355 Hz)
        // Air:    bandas 25-29 (8355-16458 Hz)

        static const int kRegionBands[6][2] = {
            {0, 2},   // Sub
            {2, 5},   // Bass
            {5, 10},  // Low-Mid
            {10, 18}, // High-Mid
            {18, 25}, // Presence
            {25, 30}  // Air
        };

        for (int r = 0; r < 6; ++r) {
            double sum = 0.0;
            int count  = 0;
            for (int b = kRegionBands[r][0]; b < kRegionBands[r][1] && b < 30; ++b) {
                if (bandEnergies[b] > -90.0f) {
                    sum += static_cast<double>(bandEnergies[b]);
                    ++count;
                }
            }
            regionProfile[r] = (count > 0) ? static_cast<float>(sum / count) : -100.0f;
        }
    }

    int WorkflowDetector::findWorstRegion(const float baseline[6], const float current[6], float& magnitudeDb)
    {
        int worstIdx   = 0;
        float worstMag = 0.0f;

        for (int r = 0; r < 6; ++r) {
            if (baseline[r] < -80.0f || current[r] < -80.0f) continue;

            float diff = std::abs(current[r] - baseline[r]);
            if (diff > worstMag) {
                worstMag = diff;
                worstIdx = r;
            }
        }

        magnitudeDb = worstMag;
        return worstIdx;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Helpers de nombres de región
    // ═══════════════════════════════════════════════════════════════════════════
    const char* WorkflowDetector::regionName(int idx) noexcept
    {
        static const char* names[] = {"Sub", "Bass", "Low-Mid", "High-Mid", "Presence", "Air"};
        return (idx >= 0 && idx < 6) ? names[idx] : "?";
    }

    const char* WorkflowDetector::regionFreqRange(int idx) noexcept
    {
        static const char* ranges[] = {
            "0-86Hz", "86-301Hz", "301-1076Hz", "1076-3532Hz", "3532-8355Hz", "8355-16458Hz"};
        return (idx >= 0 && idx < 6) ? ranges[idx] : "?";
    }

    float WorkflowDetector::regionCenterFreq(int idx) noexcept
    {
        static const float freqs[] = {60.0f, 200.0f, 600.0f, 2000.0f, 5000.0f, 12000.0f};
        return (idx >= 0 && idx < 6) ? freqs[idx] : 0.0f;
    }

} // namespace mixcoach
