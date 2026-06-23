#include "CoachEngine.h"
#include "../../Common/types/LogHelper.h"
#include "../../Common/types/Constants.h"
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  storeRecommendation — Almacena una recomendación activa para loop
    //  de corrección. No reemplaza recomendaciones activas recientes (<30s).
    // ═══════════════════════════════════════════════════════════════════════════
    void CoachEngine::storeRecommendation(int slotIndex,
                                          const juce::String& trackName,
                                          TrackRecommendation::Domain domain,
                                          const juce::String& action,
                                          float beforeValue,
                                          float expectedAfter,
                                          float delta,
                                          const juce::String& verifyMetric,
                                          float frequencyHz,
                                          int spectralBand)
    {
        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) return;

        auto& existing = recommendations_[slotIndex];
        auto now       = juce::Time::getMillisecondCounter() * 1000;

        // No reemplazar recomendaciones activas muy recientes (<60s)
        if (existing.slotIndex == slotIndex && existing.status == TrackRecommendation::Status::Pending
            && now - existing.timestampUs < 60 * 1000 * 1000) {
            return;
        }

        TrackRecommendation rec;
        rec.slotIndex     = slotIndex;
        rec.timestampUs   = now;
        rec.trackName     = trackName;
        rec.action        = action;
        rec.beforeValue   = beforeValue;
        rec.expectedAfter = expectedAfter;
        rec.delta         = delta;
        rec.domain        = domain;
        rec.verifyMetric  = verifyMetric;
        rec.frequencyHz   = frequencyHz;
        rec.spectralBand  = spectralBand;
        rec.verifyInitial = beforeValue;
        rec.verifyCurrent = beforeValue;
        rec.status        = TrackRecommendation::Status::Pending;
        rec.verifyRetries = 0;

        recommendations_[slotIndex] = rec;

        LogHelper::writeToLog("[CoachEngine] Recomendacion creada: slot=" + juce::String(slotIndex) + " \"" + trackName
                              + "\" " + action + " [" + verifyMetric + "] " + "(" + juce::String(beforeValue, 1)
                              + " dB \xE2\x86\x92 " + juce::String(expectedAfter, 1) + " dB)");
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getTrackRecommendation — Retorna ptr a recomendación activa (o nullptr)
    // ═══════════════════════════════════════════════════════════════════════════
    const TrackRecommendation* CoachEngine::getTrackRecommendation(int slotIndex) const
    {
        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) return nullptr;
        if (recommendations_[slotIndex].slotIndex != slotIndex) return nullptr;
        return &recommendations_[slotIndex];
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  storeRecommendationFromSemanticAnalysis — Convierte issues del análisis
    //  semántico en recomendaciones activas del loop de corrección.
    //
    //  Solo issues Critical/Warning generan recomendaciones. Se llama
    //  periódicamente desde periodicAnalysis() para mantener las
    //  recomendaciones sincronizadas con el estado actual de la mezcla.
    // ═══════════════════════════════════════════════════════════════════════════
    void CoachEngine::storeRecommendationFromSemanticAnalysis()
    {
        auto diffs = runSemanticAnalysis();
        if (diffs.empty()) return;

        auto& registry = sharedData_.getSlotRegistry();

        for (const auto& diff : diffs) {
            for (const auto& issue : diff.issues) {
                if (issue.severity != SemanticIssue::Severity::Critical
                    && issue.severity != SemanticIssue::Severity::Warning)
                    continue;

                // Determinar dominio según el contenido del issue
                TrackRecommendation::Domain domain = TrackRecommendation::Domain::Gain;
                juce::String verifyMetric          = "peak";

                auto msg = issue.message.toLowerCase();

                if (msg.contains("eq") || msg.contains("freq") || msg.contains("hz") || msg.contains("espectral")
                    || msg.contains("spectral") || msg.contains("tonal") || msg.contains("balance")) {
                    domain       = TrackRecommendation::Domain::Tonal;
                    verifyMetric = "band_" + juce::String(diff.spectralFocusBand);
                }
                else if (msg.contains("compres") || msg.contains("crest") || msg.contains("dinam")
                         || msg.contains("dynamics") || msg.contains("peak") || msg.contains("rms")) {
                    domain       = TrackRecommendation::Domain::Dynamics;
                    verifyMetric = "crest";
                }
                else if (msg.contains("pan") || msg.contains("estereo") || msg.contains("stereo")
                         || msg.contains("phase") || msg.contains("correlacion") || msg.contains("correlation")) {
                    domain       = TrackRecommendation::Domain::Spatial;
                    verifyMetric = "correlation";
                }

                // Encontrar slot por trackName
                int slotFound = -1;
                registry.forEachActive([&](const SlotInfo& info) {
                    if (slotFound >= 0) return;
                    juce::String name = juce::String(info.trackName).trim();
                    if (name == diff.trackName || info.slotIndex == diff.slotIndex) slotFound = info.slotIndex;
                });

                if (slotFound < 0) continue;

                // Obtener valor actual para la métrica
                auto telem = getLatestTelemetry(slotFound);
                if (telem.timestamp == 0) continue;

                float beforeValue = juce::jmax(telem.peakLeft, telem.peakRight);

                if (domain == TrackRecommendation::Domain::Tonal) {
                    int bandIdx = juce::jlimit(0, 29, diff.spectralFocusBand);
                    beforeValue = telem.spectrum[bandIdx];
                    if (beforeValue < -90.0f) beforeValue = -60.0f;
                }
                else if (domain == TrackRecommendation::Domain::Dynamics) {
                    beforeValue = telem.crestFactor;
                    if (beforeValue <= 0.0f) beforeValue = 10.0f; // Fallback genérico
                }
                else if (domain == TrackRecommendation::Domain::Spatial) {
                    beforeValue = telem.correlation;
                }

                // Delta estimado: inferir dirección del texto del issue
                float delta   = 3.0f;
                auto lowerMsg = msg;
                bool isReduction =
                    lowerMsg.contains("reduce") || lowerMsg.contains("baja") || lowerMsg.contains("cut")
                    || lowerMsg.contains("menos") || lowerMsg.contains("too much") || lowerMsg.contains("demasiado")
                    || lowerMsg.contains("exceso") || lowerMsg.contains("excess") || lowerMsg.contains("clipping")
                    || lowerMsg.contains("sobre-comp") || lowerMsg.contains("over-comp")
                    || lowerMsg.contains("overcomp") || lowerMsg.contains("reduce") || lowerMsg.contains("bajar");

                float expectedAfter = isReduction ? (beforeValue - delta) : (beforeValue + delta);

                // Clamp para dominios con rango acotado (Spatial: correlation [-1, 1])
                if (domain == TrackRecommendation::Domain::Spatial) {
                    expectedAfter = juce::jlimit(-1.0f, 1.0f, expectedAfter);
                    delta         = std::abs(expectedAfter - beforeValue);
                }

                storeRecommendation(slotFound,
                                    diff.trackName,
                                    domain,
                                    issue.recommendation.isEmpty() ? issue.message : issue.recommendation,
                                    beforeValue,
                                    expectedAfter,
                                    delta,
                                    verifyMetric,
                                    0.0f,
                                    diff.spectralFocusBand);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  verifyTrackCorrections — Núcleo del loop de corrección activo.
    //
    //  Flujo por cada recomendación Pending:
    //    1. Obtener valor actual de la métrica de verificación
    //    2. Calcular ratio de aplicación (qué fracción del target alcanzó)
    //    3. Clasificar:
    //       - Applied (80-120%):  ✅ feedback positivo
    //       - OverApplied (>120%): ⚠️ aviso de pasarse
    //       - UnderApplied (15-80%): 💪 ánimo con lo que falta
    //       - Ignored (<15% tras 3 retries): silencio
    //    4. Generar mensaje de seguimiento contextual vía respondWithContext()
    //
    //  Se llama desde periodicAnalysis() cada ~12s (kCorrectionVerifyUs).
    // ═══════════════════════════════════════════════════════════════════════════
    void CoachEngine::verifyTrackCorrections()
    {
        auto now       = juce::Time::getMillisecondCounter() * 1000;
        auto& registry = sharedData_.getSlotRegistry();

        for (int i = 0; i < SlotRegistry::kMaxSlots; ++i) {
            auto& rec = recommendations_[i];
            if (rec.slotIndex != i) continue;
            if (rec.status != TrackRecommendation::Status::Pending) continue;

            // Obtener telemetría actual
            auto telem = getLatestTelemetry(i);
            if (telem.timestamp == 0) continue;

            float currentValue = 0.0f;
            bool valid         = true;

            // ─── Leer métrica según verifyMetric ────────────────────────────
            if (rec.verifyMetric == "peak") currentValue = juce::jmax(telem.peakLeft, telem.peakRight);
            else if (rec.verifyMetric.startsWith("band_")) {
                int bandIdx = rec.verifyMetric.substring(5).getIntValue();
                if (bandIdx >= 0 && bandIdx < 30) currentValue = telem.spectrum[bandIdx];
                else
                    valid = false;
            }
            else if (rec.verifyMetric == "crest")
                currentValue = telem.crestFactor;
            else if (rec.verifyMetric == "lr_balance")
                currentValue = telem.peakLeft - telem.peakRight;
            else if (rec.verifyMetric == "correlation")
                currentValue = telem.correlation;
            else
                valid = false;

            if (!valid) continue;

            // Actualizar valor actual
            rec.verifyCurrent = currentValue;

            // ─── Calcular aplicación ────────────────────────────────────────
            float initialToTarget = rec.expectedAfter - rec.verifyInitial;
            float actualDelta     = currentValue - rec.verifyInitial;

            // Si el target es ~0, no hay nada que verificar
            if (std::abs(initialToTarget) < 0.1f) {
                rec.status = TrackRecommendation::Status::Superseded;
                continue;
            }

            float appliedRatio = actualDelta / initialToTarget;

            // ─── Clasificar usando adaptive thresholds ──────────────────────
            TrackRecommendation::Status newStatus = rec.status;
            juce::String feedbackMsg;
            bool statusFinalized = false; // ¿Es este el estado final de la recomendación?

            const auto& at = adaptiveThresholds_;

            if (std::abs(actualDelta) < std::abs(initialToTarget) * at.underApplyRatio
                && std::abs(initialToTarget) > 0.5f) {
                // ─── No aplicó o aplicó muy poco ────────────────────────────
                rec.verifyRetries++;

                if (rec.verifyRetries >= at.maxRetriesBeforeIgnore) {
                    newStatus       = TrackRecommendation::Status::Ignored;
                    statusFinalized = true;
                    LogHelper::writeToLog("[CoachEngine] Recomendacion IGNORADA: " + rec.trackName + " \""
                                          + rec.action.substring(0, 80) + "\"");
                }
                // Si aún tiene retries, esperar al próximo ciclo
            }
            else if (appliedRatio >= at.underApplyTarget && appliedRatio <= at.overApplyTarget) {
                // ─── ✅ Aplicada correctamente ──────────────────────────────
                newStatus           = TrackRecommendation::Status::Applied;
                statusFinalized     = true;
                float actualApplied = std::abs(actualDelta);

                feedbackMsg = "\xF0\x9F\x91\x80 Veo que ajustaste **" + rec.trackName + "** "
                        + juce::String(actualApplied, 1) + " dB — "
                        + "justo lo que suger\xC3\xAD. "
                        + "El balance se nota mejor. "
                        + "\xBF""Quieres seguir ajustando algo m\xC3\xA1s?";

                // Registrar en sesión
                if (trackChangeCallback_)
                    trackChangeCallback_(i, rec.trackName, rec.action, rec.verifyInitial, currentValue);

                LogHelper::writeToLog("[CoachEngine] Recomendacion APLICADA: " + rec.trackName + " \""
                                      + rec.action.substring(0, 60) + "\" (ratio=" + juce::String(appliedRatio, 2)
                                      + ", delta=" + juce::String(actualApplied, 1) + " dB)");
            }
            else if (appliedRatio > at.overApplyTarget) {
                // ─── ⚠ Sobre-aplicada (se pasó) ────────────────────────────
                newStatus = TrackRecommendation::Status::OverApplied;
                rec.verifyRetries++;

                float overBy = std::abs(actualDelta) - std::abs(initialToTarget);
                feedbackMsg  = "\xF0\x9F\x91\x80 Veo que en **" + rec.trackName + "** te pasaste "
                               + juce::String(overBy, 1) + " dB del target. " + "Suger\xC3\xAD "
                               + juce::String(std::abs(initialToTarget), 1) + " dB, ajustaste "
                               + juce::String(std::abs(actualDelta), 1) + " dB. Prueba compensar subiendo ~"
                               + juce::String(overBy * 0.5f, 1) + " dB.";

                if (rec.verifyRetries >= 2) {
                    statusFinalized = true;
                    newStatus       = TrackRecommendation::Status::Superseded;
                    feedbackMsg     = "";
                }

                LogHelper::writeToLog("[CoachEngine] Recomendacion SOBRE-APLICADA: " + rec.trackName + " (by "
                                      + juce::String(overBy, 1) + " dB)");
            }
            else if (appliedRatio >= at.goodStartRatio && appliedRatio < at.underApplyTarget) {
                // ─── 💪 Sub-aplicada ─────────────────────────────────────────
                newStatus = TrackRecommendation::Status::UnderApplied;
                rec.verifyRetries++;

                float remainingDb = std::abs(initialToTarget) - std::abs(actualDelta);
                feedbackMsg = "\xF0\x9F\x91\x80 Veo que empezaste a ajustar **" + rec.trackName + "** — "
                        + "bajaste ~" + juce::String(std::abs(actualDelta), 1)
                        + " dB. Siguen faltando ~" + juce::String(remainingDb, 1)
                        + " dB para llegar al target. "
                        + "\xC2\xA1""Vas bien, sigue unos dB m\xC3\xA1s!";

                if (rec.verifyRetries >= at.maxRetriesBeforeIgnore) {
                    statusFinalized = true;
                    newStatus       = TrackRecommendation::Status::Ignored;
                    feedbackMsg     = "";
                }

                LogHelper::writeToLog("[CoachEngine] Recomendacion SUB-APLICADA: " + rec.trackName
                                      + " (ratio=" + juce::String(appliedRatio, 2) + ", falta "
                                      + juce::String(remainingDb, 1) + " dB)");
            }

            // ─── Registrar en historial si se finalizó ──────────────────────
            if (statusFinalized) {
                CorrectionHistoryEntry entry;
                entry.timestampUs  = now;
                entry.slotIndex    = i;
                entry.trackName    = rec.trackName;
                entry.action       = rec.action;
                entry.domain       = rec.domain;
                entry.finalStatus  = newStatus;
                entry.beforeValue  = rec.verifyInitial;
                entry.afterValue   = currentValue;
                entry.appliedRatio = appliedRatio;

                if ((int)correctionHistory_.size() >= kMaxCorrectionHistory)
                    correctionHistory_.erase(correctionHistory_.begin());
                correctionHistory_.push_back(entry);

                // Recalcular thresholds adaptativos cada 10 correcciones
                if (correctionHistory_.size() % 10 == 0) recalcAdaptiveThresholds();
            }

            // ─── Enviar feedback al chat (burbuja premium!) ────────────────
            if (newStatus != rec.status && !feedbackMsg.isEmpty()) {
                rec.status          = newStatus;
                rec.feedbackMessage = feedbackMsg;
                rec.feedbackSent    = true;

                // Usar respondWithCorrectionFeedback() para burbuja premium visible
                respondWithCorrectionFeedback(feedbackMsg);

                // ═══ Follow-up: generar ajuste fino para OverApplied/UnderApplied ═══
                if (newStatus == TrackRecommendation::Status::OverApplied
                    || newStatus == TrackRecommendation::Status::UnderApplied) {
                    if (statusFinalized) generateFollowUp(i, rec);
                }
            }
            else if (newStatus != rec.status) {
                rec.status       = newStatus;
                rec.feedbackSent = true;

                // ═══ Follow-up para Superseded que fue OverApplied ═════════
                if (newStatus == TrackRecommendation::Status::Superseded && rec.feedbackMessage.isNotEmpty()) {
                    // Solo generar follow-up si se finalizó
                    if (statusFinalized) generateFollowUp(i, rec);
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getCorrectionHistory — Retorna el historial de correcciones
    //  (Definido fuera de línea porque correctionHistory_ se declara después que
    //   el método en el header, y MSVC no permite retornar referencia a un miembro
    //   no declarado en métodos inline.)
    // ═══════════════════════════════════════════════════════════════════════════
    const std::vector<CoachEngine::CorrectionHistoryEntry>& CoachEngine::getCorrectionHistory() const noexcept
    {
        return correctionHistory_;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  generateFollowUp — Genera recomendación de seguimiento post-verificación
    //
    //  Cuando una recomendación se verifica como OverApplied o UnderApplied,
    //  esta función genera una NUEVA recomendación con el delta corregido
    //  para que el usuario pueda ajustar fino sin tener que calcular
    //  mentalmente cuánto compensar.
    //
    //  Ejemplo:
    //    Rec original: "Baja Kick -6dB"     (delta = -6.0)
    //    Usuario bajó: -9.0 dB (OverApplied → delta real = -9.0)
    //    Follow-up:    "Sube Kick +3.0 dB para compensar el exceso"
    //                 (delta = -3.0, hacia el target original)
    //
    //  No genera follow-up si:
    //    • El usuario ignoró o descartó la recomendación
    //    • La diferencia entre lo aplicado y el target es < 1.0 dB
    //      (el usuario está lo suficientemente cerca)
    //    • Ya hay una recomendación Pending activa en el slot
    // ═══════════════════════════════════════════════════════════════════════════
    void CoachEngine::generateFollowUp(int slotIndex, const TrackRecommendation& oldRec)
    {
        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) return;

        // No generar follow-up si ya hay una rec Pending activa
        if (recommendations_[slotIndex].slotIndex == slotIndex
            && recommendations_[slotIndex].status == TrackRecommendation::Status::Pending)
            return;

        // No generar follow-up si ignoró o descartó
        if (oldRec.status == TrackRecommendation::Status::Ignored
            || oldRec.status == TrackRecommendation::Status::Superseded)
            return;

        // Calcular cuánto se desvió del target
        float actualDelta = oldRec.verifyCurrent - oldRec.verifyInitial;
        float targetDelta = oldRec.expectedAfter - oldRec.verifyInitial;

        // Si el target es ~0, no hay follow-up posible
        if (std::abs(targetDelta) < 0.1f) return;

        // Cuánto falta o sobra del target original
        float residualDelta = targetDelta - actualDelta;
        float residualDb    = std::abs(residualDelta);

        // No generar follow-up si el residual es < 1.0 dB (suficientemente cerca)
        if (residualDb < 1.0f) return;

        // ─── Construir follow-up según el dominio ────────────────────────────
        juce::String direction = (residualDelta > 0) ? "Sube " + juce::String(residualDb, 1) + " dB"
                                                     : "Baja " + juce::String(residualDb, 1) + " dB";

        juce::String followUpAction;
        switch (oldRec.domain) {
            case TrackRecommendation::Domain::Gain:
                followUpAction = direction + " en el fader de " + oldRec.trackName + " (te pasaste por "
                                 + juce::String(residualDb, 1) + " dB)";
                break;
            case TrackRecommendation::Domain::Tonal:
                followUpAction = direction + " en EQ de " + oldRec.trackName + " (ajuste fino, Q=1.8)";
                break;
            case TrackRecommendation::Domain::Dynamics:
                followUpAction = direction + " el threshold del compresor en " + oldRec.trackName + " (ajuste fino)";
                break;
            case TrackRecommendation::Domain::Spatial:
                followUpAction = direction + " el pan de " + oldRec.trackName + " (centrando un poco)";
                break;
            default:
                followUpAction = direction + " en " + oldRec.trackName;
                break;
        }

        // Calcular nuevo expectedAfter: el target original menos lo que ya aplicó
        float newExpectedAfter = oldRec.verifyInitial + targetDelta;
        // Pero partiendo del valor actual
        float newBeforeValue = oldRec.verifyCurrent;
        float newDelta       = residualDelta;

        // Registrar follow-up en el historial
        if (!correctionHistory_.empty()) correctionHistory_.back().hadFollowUp = true;

        // Almacenar como nueva recomendación (con delta corregido)
        storeRecommendation(slotIndex,
                            oldRec.trackName,
                            oldRec.domain,
                            followUpAction,
                            newBeforeValue,
                            newExpectedAfter,
                            newDelta,
                            oldRec.verifyMetric,
                            oldRec.frequencyHz,
                            oldRec.spectralBand);

        LogHelper::writeToLog("[CoachEngine] Follow-up generado: slot=" + juce::String(slotIndex)
                              + " residual=" + juce::String(residualDb, 1) + " dB ("
                              + (residualDelta > 0 ? "needs +" : "needs ") + juce::String(residualDb, 1) + ")");
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getSlotCorrectionStats — Estadísticas de corrección por slot
    // ═══════════════════════════════════════════════════════════════════════════
    CoachEngine::SlotCorrectionStats CoachEngine::getSlotCorrectionStats(int slotIndex) const noexcept
    {
        SlotCorrectionStats stats;
        for (const auto& entry : correctionHistory_) {
            if (entry.slotIndex != slotIndex) continue;

            stats.total++;
            switch (entry.finalStatus) {
                case TrackRecommendation::Status::Applied:
                    stats.applied++;
                    break;
                case TrackRecommendation::Status::OverApplied:
                    stats.overApplied++;
                    break;
                case TrackRecommendation::Status::UnderApplied:
                    stats.underApplied++;
                    break;
                case TrackRecommendation::Status::Ignored:
                    stats.ignored++;
                    break;
                default:
                    break;
            }
        }
        return stats;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getMostUrgentRecommendation — Recomendación Pending más urgente
    //  Prioridad: Clipping (peak > -0.5) > Phase < 0 > Crest extremo > Tonal > Gain
    // ═══════════════════════════════════════════════════════════════════════════
    const TrackRecommendation* CoachEngine::getMostUrgentRecommendation() const
    {
        const TrackRecommendation* best = nullptr;
        int bestPriority                = 999;

        for (int i = 0; i < SlotRegistry::kMaxSlots; ++i) {
            const auto& rec = recommendations_[i];
            if (rec.slotIndex != i || rec.status != TrackRecommendation::Status::Pending) continue;

            // Determinar prioridad
            int priority = 5; // Default: baja

            switch (rec.domain) {
                case TrackRecommendation::Domain::Gain: {
                    // Si el peak indica clipping, es prioridad máxima
                    auto telem = getLatestTelemetry(i);
                    float peak = juce::jmax(telem.peakLeft, telem.peakRight);
                    if (peak > -0.5f) priority = 0; // 🔴 CLIPPING
                    else if (peak > -3.0f)
                        priority = 2; // 🟡 Near-clipping
                    else
                        priority = 4; // Nivel normal
                    break;
                }
                case TrackRecommendation::Domain::Spatial:
                    priority = 1; // 🔮 Phase issues
                    break;
                case TrackRecommendation::Domain::Dynamics:
                    priority = 3; // ⚡ Compression/crest
                    break;
                case TrackRecommendation::Domain::Tonal:
                    priority = 4; // 🎛️ EQ
                    break;
            }

            if (priority < bestPriority) {
                bestPriority = priority;
                best         = &rec;
            }
        }

        return best;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  dismissRecommendation — Marca una recomendación como descartada por el
    //  usuario. Se llama desde comandos como /done, /skiprec.
    // ═══════════════════════════════════════════════════════════════════════════
    void CoachEngine::dismissRecommendation(int slotIndex)
    {
        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) return;

        auto& rec = recommendations_[slotIndex];
        if (rec.slotIndex != slotIndex) {
            respondWith("No hay recomendación activa para este track.", MentorMessage::Type::Info);
            return;
        }

        if (rec.status == TrackRecommendation::Status::Pending) {
            // Registrar en historial antes de descartar
            CorrectionHistoryEntry entry;
            entry.timestampUs  = juce::Time::getMillisecondCounter() * 1000;
            entry.slotIndex    = slotIndex;
            entry.trackName    = rec.trackName;
            entry.action       = rec.action;
            entry.domain       = rec.domain;
            entry.finalStatus  = TrackRecommendation::Status::Ignored;
            entry.beforeValue  = rec.beforeValue;
            entry.afterValue   = rec.verifyCurrent;
            entry.appliedRatio = 0.0f;

            if ((int)correctionHistory_.size() >= kMaxCorrectionHistory)
                correctionHistory_.erase(correctionHistory_.begin());
            correctionHistory_.push_back(entry);

            juce::String actionPreview = rec.action.substring(0, 60);
            juce::String msg = "\xE2\x8F\xAD **Recomendación descartada:** " + rec.trackName + " \"" + actionPreview
                               + "\"\n" + "Si cambias de opinión, siempre puedes preguntarme por ella.";
            respondWithContext(msg, rec.trackName, MentorMessage::Type::Info);

            LogHelper::writeToLog("[CoachEngine] Recomendacion descartada por usuario: slot="
                                  + juce::String(slotIndex));
        }

        rec = TrackRecommendation{};
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  recalcAdaptiveThresholds — Ajusta thresholds según comportamiento
    //
    //  Analiza el historial de correcciones y ajusta:
    //    • Si el usuario sobre-aplica con frecuencia (>50%):
    //      → Reduce expected delta en 30% para compensar
    //    • Si el usuario sub-aplica con frecuencia (>40%):
    //      → Aumenta expected delta en 20%
    //    • Si el usuario ignora con frecuencia (>60%):
    //      → Reduce maxRetriesBeforeIgnore a 1 para no insistir
    //    • Si el usuario aplica exacto consistentemente:
    //      → Thresholds más estrechos (underApply=0.90, overApply=1.10)
    // ═══════════════════════════════════════════════════════════════════════════
    void CoachEngine::recalcAdaptiveThresholds()
    {
        if (correctionHistory_.size() < 5) return; // No hay suficiente historial para ajustar

        int totalApplied = 0, totalOver = 0, totalUnder = 0, totalIgnored = 0;

        for (const auto& entry : correctionHistory_) {
            switch (entry.finalStatus) {
                case TrackRecommendation::Status::Applied:
                    totalApplied++;
                    break;
                case TrackRecommendation::Status::OverApplied:
                    totalOver++;
                    break;
                case TrackRecommendation::Status::UnderApplied:
                    totalUnder++;
                    break;
                case TrackRecommendation::Status::Ignored:
                    totalIgnored++;
                    break;
                default:
                    break;
            }
        }

        int total = totalApplied + totalOver + totalUnder + totalIgnored;
        if (total == 0) return;

        float overRatio   = (float)totalOver / total;
        float underRatio  = (float)totalUnder / total;
        float ignoreRatio = (float)totalIgnored / total;
        float exactRatio  = (float)totalApplied / total;

        LogHelper::writeToLog("[CoachEngine] Adaptive thresholds: applied=" + juce::String(totalApplied)
                              + " over=" + juce::String(totalOver) + " under=" + juce::String(totalUnder)
                              + " ignored=" + juce::String(totalIgnored) + " (total=" + juce::String(total) + ")");

        // ═══ Usuario sobre-aplica frecuentemente ═══════════════════════════
        if (overRatio > 0.5f) {
            // El usuario tiende a pasarse: reducimos el delta en 30%
            // Esto hace que las recomendaciones pidan menos ajuste del que realmente
            // necesitan, sabiendo que el usuario aplicará más de la cuenta.
            adaptiveThresholds_.underApplyTarget = 0.75f; // Más permisivo
            adaptiveThresholds_.overApplyTarget  = 1.35f; // Más permisivo
            LogHelper::writeToLog(
                "[CoachEngine] → Perfil sobre-aplicador detectado: "
                "thresholds relajados (under="
                + juce::String(adaptiveThresholds_.underApplyTarget, 2)
                + ", over=" + juce::String(adaptiveThresholds_.overApplyTarget, 2) + ")");
        }
        // ═══ Usuario aplica exacto y preciso ══════════════════════════════
        else if (exactRatio > 0.5f) {
            // El usuario es preciso: thresholds más estrechos
            adaptiveThresholds_.underApplyTarget = 0.90f;
            adaptiveThresholds_.overApplyTarget  = 1.10f;
            adaptiveThresholds_.underApplyRatio  = 0.20f;
            LogHelper::writeToLog(
                "[CoachEngine] → Perfil preciso detectado: "
                "thresholds estrechos (under="
                + juce::String(adaptiveThresholds_.underApplyTarget, 2)
                + ", over=" + juce::String(adaptiveThresholds_.overApplyTarget, 2) + ")");
        }
        // ═══ Usuario ignora frecuentemente ════════════════════════════════
        if (ignoreRatio > 0.6f) {
            adaptiveThresholds_.maxRetriesBeforeIgnore = 1; // Solo 1 intento
            LogHelper::writeToLog(
                "[CoachEngine] → Perfil que ignora detectado: "
                "maxRetries reducido a 1");
        }
        else if (ignoreRatio < 0.2f && exactRatio > 0.4f) {
            adaptiveThresholds_.maxRetriesBeforeIgnore = 4; // Más paciencia
            LogHelper::writeToLog(
                "[CoachEngine] → Usuario receptivo: "
                "maxRetries aumentado a 4");
        }

        // ═══ Usuario sub-aplica frecuentemente ════════════════════════════
        if (underRatio > 0.4f && overRatio < 0.3f) {
            adaptiveThresholds_.underApplyRatio = 0.10f; // Más permisivo con "poco avance"
            LogHelper::writeToLog(
                "[CoachEngine] → Perfil sub-aplicador detectado: "
                "underApplyRatio reducido a 0.10");
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  detectUnpromptedChanges — Detecta cambios en pistas SIN recomendación
    //  activa y envía observaciones conversacionales.
    //
    //  Mientras verifyTrackCorrections() verifica solo las pistas con
    //  recomendaciones Pending, este método escanea TODAS las pistas activas
    //  y detecta cambios de nivel no solicitados.
    //
    //  Reglas:
    //    • Solo tracks sin recomendación Pending
    //    • Cambio mínimo: 1.5 dB en peak (evita ruido de medición)
    //    • Cooldown: 120s por pista (kCorrectionCooldownUs)
    //    • Máximo 1 observación por ciclo de análisis
    //
    //  Se llama desde periodicAnalysis().
    // ═══════════════════════════════════════════════════════════════════════════

    // Cooldown específico para observaciones no solicitadas (2min)
    static constexpr int64_t kUnpromptedChangeCooldownUs = 120 * 1000 * 1000;

    void CoachEngine::detectUnpromptedChanges(int focusSlotIndex)
    {
        auto now        = juce::Time::getMillisecondCounter() * 1000;
        auto& registry  = sharedData_.getSlotRegistry();
        int activeCount = registry.activeCount();

        if (activeCount == 0) return;

        // Solo enviar UNA observación por ciclo para no saturar
        bool sentThisCycle = false;

        // ─── Helper lambda para evaluar un slot específico ────────────────
        auto checkSlot = [&](int idx) {
            if (sentThisCycle) return;
            if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;

            // Saltar si hay recomendación Pending (ya la maneja verifyTrackCorrections)
            const auto& rec = recommendations_[idx];
            if (rec.slotIndex == idx && rec.status == TrackRecommendation::Status::Pending) return;

            // Obtener telemetría actual
            auto telem = getLatestTelemetry(idx);
            if (telem.timestamp == 0) return;

            // Obtener nombre de la pista
            SlotInfo slotInfo      = registry.getSlotInfo(idx);
            juce::String trackName = juce::String(slotInfo.trackName).trim();
            if (trackName.isEmpty()) trackName = "Track " + juce::String(idx + 1);

            auto& state        = trackStates_[idx];
            float currentPeak  = juce::jmax(telem.peakLeft, telem.peakRight);
            float previousPeak = state.lastPeakDb;

            // Solo detectar si hay un valor previo válido
            if (previousPeak < -90.0f || previousPeak > 0.0f) {
                state.lastPeakDb = currentPeak;
                return;
            }

            float changeDb  = currentPeak - previousPeak;
            float absChange = std::abs(changeDb);

            // Ignorar cambios menores a 1.5 dB (ruido de medición)
            if (absChange < 1.5f) {
                state.lastPeakDb = currentPeak;
                return;
            }

            // Cooldown: no repetir observación para la misma pista
            if (now - state.lastWarningUs < kUnpromptedChangeCooldownUs) {
                state.lastPeakDb = currentPeak;
                return;
            }

            // Ignorar cambios extremadamente rápidos (posible salto del DAW, no del usuario)
            if (absChange > 12.0f) {
                state.lastPeakDb = currentPeak;
                return;
            }

            // ─── Construir mensaje conversacional dirigido ────────────────────
            juce::String direction;
            juce::String observation;

            if (changeDb < 0) {
                direction = "bajaste";
                observation = "\xF0\x9F\x91\x80 Veo que **" + trackName + "** baj\xC3\xB3 "
                        + juce::String(absChange, 1) + " dB. \xBF""Est\xC3\xA1s "
                        + "haciendo gain staging? Se escucha con m\xC3\xA1s headroom.";
            }
            else {
                direction = "subiste";
                observation = "\xF0\x9F\x91\x80 Not\xC3\xA9 que **" + trackName + "** subi\xC3\xB3 "
                        + juce::String(absChange, 1) + " dB. "
                        + "\xBF""Necesitas m\xC3\xA1s presencia de ese elemento?";
            }

            // Actualizar cooldown y enviar
            state.lastWarningUs = now;
            state.lastPeakDb    = currentPeak;

            respondWithCorrectionFeedback(observation);

            // Log
            LogHelper::writeToLog("[CoachEngine] Cambio detectado (no solicitado): " + trackName + " " + direction + " "
                                  + juce::String(absChange, 1) + " dB");

            sentThisCycle = true;
        };

        // ═══ Si focusSlotIndex está especificado, solo evaluar esa pista ═══
        if (focusSlotIndex >= 0) {
            checkSlot(focusSlotIndex);
        }
        // ═══ Si no, escanear todas las pistas activas (comportamiento original) ═══
        else {
            registry.forEachActive([&](const SlotInfo& info) { checkSlot(info.slotIndex); });
        }

        // Actualizar estados para el próximo ciclo incluso para tracks sin observación
        registry.forEachActive([&](const SlotInfo& info) {
            int idx = info.slotIndex;
            if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;

            auto telem = getLatestTelemetry(idx);
            if (telem.timestamp == 0) return;

            auto& state       = trackStates_[idx];
            float currentPeak = juce::jmax(telem.peakLeft, telem.peakRight);
            if (currentPeak > -90.0f && currentPeak < 0.0f) state.lastPeakDb = currentPeak;
        });
    }

} // namespace mixcoach
