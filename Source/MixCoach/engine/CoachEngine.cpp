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

    CoachEngine::CoachEngine(PhaseManager& phaseManager, SharedData& sharedData, AudioAnalyzer& audioAnalyzer) :
        phaseManager_(phaseManager),
        sharedData_(sharedData),
        audioAnalyzer_(audioAnalyzer),
        trackFeedCore_(std::make_unique<TrackFeedCore>())
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
        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) return TrackTelemetry{};
        auto result = sharedData_.getTrackAudioResult(slotIndex);
        if (result.timestampUs <= 0) return TrackTelemetry{};
        TrackTelemetry telem;
        result.timestampUs  = result.timestampUs;
        telem.peakLeft      = result.peakLeft;
        telem.peakRight     = result.peakRight;
        telem.rmsLeft       = result.rmsLeft;
        telem.rmsRight      = result.rmsRight;
        telem.correlation   = result.correlation;
        telem.crestFactor   = result.crestPerBand[0];
        telem.lufsMomentary = -100.0f;
        telem.lufsShortTerm = -100.0f;
        for (int b = 0; b < 30; ++b) telem.bandEnergies[b] = result.bandEnergies[b];
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

    void
    CoachEngine::respondWithContext(const juce::String& text, const juce::String& context, MentorMessage::Type type)
    {
        MentorMessage msg;
        msg.type      = type;
        msg.text      = text.toStdString();
        msg.timestamp = juce::Time::getMillisecondCounter() * 1000;
        msg.context   = context.toStdString();
        sharedData_.pushMessage(msg);
        LogHelper::writeToLog("[CoachEngine] Mensaje (" + context + "): " + text.substring(0, 80));
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  RESET — Reinicia estados de pista para empezar de nuevo la fase
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::resetTrackStates()
    {
        for (auto& state : trackStates_) {
            state = TrackAnalysisState{};
        }
        lastPeakWarningUs_     = 0;
        lastCrestWarningUs_    = 0;
        lastPhaseWarningUs_    = 0;
        lastHeadroomWarningUs_ = 0;
        lastTonalWarningUs_    = 0;
        lastDynamicWarningUs_  = 0;
        lastLoudnessWarningUs_ = 0;
        lastMaskingWarningUs_  = 0;
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
        if (now - lastPeriodicAnalysisUs_ < kAnalysisIntervalUs) return;
        lastPeriodicAnalysisUs_ = now;

        auto& registry = sharedData_.getSlotRegistry();

        // ═══ V8: Solo mode detection — si hay pistas en solo, las no-solistas se silencian
        soloActive_ = false;
        registry.forEachActive([&](const SlotInfo& si) {
            if (si.soloed) soloActive_ = true;
        });
        // ═══ V8: Notificar estado de mute/solo en el log
        {
            int mutedCount = 0, soloedCount = 0;
            juce::String mutedList, soloedList;
            registry.forEachActive([&](const SlotInfo& info) {
                if (info.muted) {
                    mutedCount++;
                    juce::String name = juce::String(info.trackName).trim();
                    if (name.isEmpty()) name = "Pista " + juce::String(info.slotIndex + 1);
                    if (!mutedList.isEmpty()) mutedList += ", ";
                    mutedList += name;
                }
                if (info.soloed) {
                    soloedCount++;
                    juce::String name = juce::String(info.trackName).trim();
                    if (name.isEmpty()) name = "Pista " + juce::String(info.slotIndex + 1);
                    if (!soloedList.isEmpty()) soloedList += ", ";
                    soloedList += name;
                }
            });
            if (mutedCount > 0 || soloedCount > 0) {
                juce::String muteMsg;
                if (mutedCount == 1) muteMsg += "\xFO\x9F\x94\x87 **" + mutedList + "** muteada";
                else if (mutedCount > 1)
                    muteMsg += "\xFO\x9F\x94\x87 **" + juce::String(mutedCount) + " pistas** muteadas";
                if (soloedCount > 0) {
                    if (mutedCount > 0) muteMsg += " | ";
                    if (soloedCount == 1) muteMsg += "\xFO\x9F\x94\x8A **" + soloedList + "** en solo";
                    else
                        muteMsg += "\xFO\x9F\x94\x8A **" + juce::String(soloedCount) + " pistas** en solo";
                }
                LogHelper::writeToLog("[CoachEngine-V8] " + muteMsg);
                if (soloedCount > 0)
                    LogHelper::writeToLog("[CoachEngine-V8] " + juce::String(soloedCount)
                                          + " pista(s) en solo — las demas se excluyen del analisis");
            }
        }


        if (registry.activeCount() == 0) return;

        LogHelper::writeToLog("[CoachEngine] Análisis periódico iniciado (" + juce::String(registry.activeCount())
                              + " pistas activas)");

        // ═══ SPRINT 5: Sincronizar TrackFeedCore con datos frescos ══════════
        syncTrackFeedCore();

        // ═══ SPRINT 7: Per-track consolidated analysis ═══════════════
        analyzeAllTracks();

        // ═══ SPRINT 6A: Per-track gain analysis ═══════════════
        auto allGainAdvice = analyzeAllTracksGain();
        {
            int offTargetCount = 0;
            for (const auto& adv : allGainAdvice)
                if (adv.status == TrackGainAdvice::Status::OffTarget) offTargetCount++;

            if (offTargetCount > 0 && now - lastGainAdviceUs_ >= kGainAdviceCooldownUs) {
                lastGainAdviceUs_ = now;
                juce::String examples;
                int shown = 0;
                for (const auto& adv : allGainAdvice) {
                    if (adv.status == TrackGainAdvice::Status::OffTarget && shown < 3) {
                        if (shown > 0) examples += "\n";
                        examples += adv.message;
                        shown++;
                    }
                }
                if (offTargetCount == 1) {
                    respondWith(examples, MentorMessage::Type::Tip);
                }
                else {
                    respondWith("\xF0\x9F\x93\x8A **" + juce::String(offTargetCount)
                                    + " pistas fuera de rango \xF3\xBE\x90\xA2ptimo:**\n" + examples
                                    + "\n\xF0\x9F\x92\xA1 Ajusta el fader de gain de cada pista.",
                                MentorMessage::Type::Tip);
                }
            }
        }
        // ═══ FIN Sprint 6A ═══════════════════════

        // ═══ SPRINT 6B: Per-track dynamics analysis ══════════
        {
            auto allDynAdvice = analyzeAllTracksDynamics();
            int offTargetDyn  = 0;
            for (const auto& adv : allDynAdvice)
                if (adv.status == TrackDynamicsAdvice::Status::OffTarget) offTargetDyn++;

            if (offTargetDyn > 0 && now - lastDynamicsAdviceUs_ >= kDynamicsAdviceCooldownUs) {
                lastDynamicsAdviceUs_ = now;
                juce::String dynExamples;
                int shownDyn = 0;
                for (const auto& adv : allDynAdvice) {
                    if (adv.status == TrackDynamicsAdvice::Status::OffTarget && shownDyn < 3) {
                        if (shownDyn > 0) dynExamples += "\n";
                        dynExamples += adv.message;
                        shownDyn++;
                    }
                }
                if (offTargetDyn == 1) {
                    respondWith(dynExamples, MentorMessage::Type::Tip);
                }
                else {
                    respondWith("\xF0\x9F\x93\x8A **" + juce::String(offTargetDyn)
                                    + " pistas con problemas de din\xC3\xA1mica:**\n" + dynExamples
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
            int offTargetPhase  = 0;
            for (const auto& adv : allPhaseAdvice)
                if (adv.status == TrackPhaseAdvice::Status::OffTarget) offTargetPhase++;

            if (offTargetPhase > 0 && now - lastPhaseAdviceUs_ >= kPhaseAdviceCooldownUs) {
                lastPhaseAdviceUs_ = now;
                juce::String examples;
                int shown = 0;
                for (const auto& adv : allPhaseAdvice) {
                    if (adv.status == TrackPhaseAdvice::Status::OffTarget && shown < 3) {
                        if (shown > 0) examples += "\n";
                        examples += adv.message;
                        shown++;
                    }
                }
                if (offTargetPhase == 1) respondWith(examples, MentorMessage::Type::Warning);
                else
                    respondWith("\xf0\x9f\x94\xae **" + juce::String(offTargetPhase)
                                    + " pistas con problemas de fase:**\n" + examples
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
        if (referenceFingerprint_.valid) computeAndSendMatchData();

        // ═══ SPRINT 1: Continuous role inference during Organización phase ═══
        // inferTrackRoles() is idempotent (skips slots that already have a role),
        // so calling it every 8s is safe and catches new tracks automatically.
        // Pattern mirrors the referenceDrivenMode_ block below.
        if (isMixMode() && phaseManager_.getCurrentPhase() == MentorPhase::Organizacion) {
            inferTrackRoles();
        }

        // ═══ REFERENCE-DRIVEN MODE: Computar match progreso y enviar análisis ═══
        // Cuando el modo está activo y hay referencia cargada, computamos el % de match
        // contra la referencia cada ciclo de análisis y detectamos mejora/empeoramiento.
        if (referenceDrivenMode_ && referenceFingerprint_.valid) {
            auto progress = computeReferenceMatchProgress();

            // ─── Mensaje de bienvenida al activar REF MODE por primera vez ────
            if (!referenceDrivenWelcomeSent_) {
                referenceDrivenWelcomeSent_ = true;
                juce::String welcomeMsg;
                welcomeMsg +=
                    "\xf0\x9f\x8e\xaf **Modo Referencia activado** \xe2\x80\x94 ahora toda la mezcla se mide contra ";
                welcomeMsg += getReferenceName();
                welcomeMsg += ".\n";
                welcomeMsg +=
                    "Te ir\xc3\xa9 contando c\xc3\xb3mo vamos: qu\xc3\xa9 \xc3\xa1reas est\xc3\xa1n cerca y "
                    "cu\xc3\xa1les necesitan ajuste.\n";
                welcomeMsg +=
                    "\xf0\x9f\x92\xa1 Escucha la referencia, ajusta tu mezcla, y yo te dir\xc3\xa9 cuando se parezcan.";
            }

            if (now - lastReferenceDrivenAnalysisUs_ >= kReferenceDrivenAnalysisIntervalUs) {
                lastReferenceDrivenAnalysisUs_ = now;

                // Detectar cambios significativos en el match (1ra condición)
                bool significantChange = std::abs(progress.delta) >= 0.05f
                                         && now - lastReferenceGapImprovedUs_ >= kReferenceDrivenProgressCooldownUs;

                // O mensaje periódico incluso sin cambios (2da condición)
                bool periodicStatus = (now - lastReferenceDrivenStatusUs_ >= kReferenceDrivenStatusIntervalUs)
                                      && progress.currentMatch > 0.01f;

                if (significantChange) {
                    sendReferenceDrivenAnalysis();

                    if (progress.delta > 0) lastReferenceGapImprovedUs_ = now;
                    else
                        lastReferenceGapWorsenedUs_ = now;

                    lastReferenceDrivenStatusUs_ = now; // reset periodic timer
                }
                else if (periodicStatus) {
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

    //  REFERENCE-DRIVEN MODE — Computar match % y enviar análisis consolidado
    // ═══════════════════════════════════════════════════════════════════════════

    ReferenceProgress CoachEngine::computeReferenceMatchProgress()
    {
        ReferenceProgress progress;
        progress.hasReference = referenceMetadata_.valid();
        progress.hasAudio     = referenceFingerprint_.valid;

        if (!progress.hasAudio) {
            referenceProgress_ = progress;
            return progress;
        }

        // ─── Construir DifferenceProfile completo ────────────────────────────
        auto gaps = getReferenceGaps();
        auto dp   = buildDifferenceProfile();

        if (!dp.valid) {
            referenceProgress_ = progress;
            return progress;
        }

        // ─── Computar match % desde DifferenceProfile ────────────────────────
        // Combinamos 3 métricas:
        //   1. DeltaScore (0.0-1.0) desde DifferenceProfile (combina LUFS + Crest + Spectral)
        //   2. Inverse gap severity: cuantos gaps NO son críticos/warning
        //   3. Inverse LUFS gap: qué tan lejos estamos en loudness

        float deltaScore = dp.deltaScore; // 0.0-1.0 desde DifferenceProfile

        // Inverse gap severity: si hay muchos gaps críticos, el match baja
        float gapScore = 1.0f;
        if (!gaps.empty()) {
            int penaltyCount = 0;
            for (const auto& g : gaps) {
                if (g.severity == GapSeverity::Critical) penaltyCount += 3;
                else if (g.severity == GapSeverity::Warning)
                    penaltyCount += 1;
            }
            gapScore = 1.0f - std::min(1.0f, static_cast<float>(penaltyCount) / 15.0f);
        }

        // Inverse LUFS gap: qué tan lejos está el integrated LUFS
        float lufsGap   = std::abs(dp.deltaLUFS);
        float lufsScore = 1.0f - std::min(1.0f, lufsGap / 6.0f); // 6 LUFS = 0 score

        // ─── Match final: weighted average ────────────────────────────────────
        // DeltaScore pesa más porque ya combina múltiples métricas
        progress.currentMatch = deltaScore * 0.50f + gapScore * 0.30f + lufsScore * 0.20f;
        progress.currentMatch = juce::jlimit(0.0f, 1.0f, progress.currentMatch);

        // ─── Gaps counts ────────────────────────────────────────────────────
        progress.totalGaps    = static_cast<int>(gaps.size());
        progress.criticalGaps = 0;
        progress.warningGaps  = 0;
        for (const auto& g : gaps) {
            if (g.severity == GapSeverity::Critical) progress.criticalGaps++;
            else if (g.severity == GapSeverity::Warning)
                progress.warningGaps++;
        }

        // ─── Delta vs medición anterior ─────────────────────────────────────
        progress.previousMatch = previousReferenceMatch_;
        progress.delta         = progress.currentMatch - progress.previousMatch;

        // ─── Resolved gaps: comparar con medición anterior ────────────────────
        // Si totalGaps bajó vs la última vez, esos se consideran "resolved"
        if (!previousReferenceGaps_.empty()) {
            int prevTotal         = static_cast<int>(previousReferenceGaps_.size());
            progress.resolvedGaps = std::max(0, prevTotal - progress.totalGaps);
        }

        // ─── Actualizar historial ────────────────────────────────────────────
        previousReferenceMatch_ = progress.currentMatch;
        previousReferenceGaps_  = gaps;

        if (progressHistoryCount_ < kMaxProgressHistory) progressHistoryCount_++;
        progressHistory_[progressHistoryIndex_] = progress.currentMatch;
        progressHistoryIndex_                   = (progressHistoryIndex_ + 1) % kMaxProgressHistory;

        // ─── Cache ───────────────────────────────────────────────────────────
        referenceProgress_ = progress;

        LogHelper::writeToLog("[CoachEngine] Reference-Driven Progress: "
                              + juce::String(static_cast<int>(progress.currentMatch * 100.0f)) + "%" + " (delta "
                              + juce::String(progress.delta * 100.0f, 1) + "%" + " | gaps "
                              + juce::String(progress.criticalGaps) + "c/" + juce::String(progress.warningGaps) + "w/"
                              + juce::String(progress.resolvedGaps) + "r)");

        return progress;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  resetReferenceProgress — Resets all reference-driven tracking data
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::resetReferenceProgress() noexcept
    {
        referenceProgress_      = ReferenceProgress{};
        previousReferenceMatch_ = 0.0f;
        previousReferenceGaps_.clear();
        progressHistoryCount_ = 0;
        progressHistoryIndex_ = 0;
        for (auto& v : progressHistory_) v = 0.0f;
        lastReferenceDrivenAnalysisUs_ = 0;
        lastReferenceGapImprovedUs_    = 0;
        lastReferenceGapWorsenedUs_    = 0;
        LogHelper::writeToLog("[CoachEngine] Reference progress reset");
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  sendReferenceDrivenAnalysis — Envía un mensaje consolidado al LLM o al chat
    //  cuando el match contra la referencia cambia significativamente.
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::sendReferenceDrivenAnalysis()
    {
        if (!referenceFingerprint_.valid) return;

        // Si hay LLM streaming, enviar el análisis como prompt al LLM
        // para que genere una respuesta natural estilo ingeniero.
        if (llmEnabled_ && llmStreamingCallback_ && setupStep_ == SetupStep::Complete) {
            juce::String refName = getReferenceName();
            auto& plan           = planManager_;

            juce::String prompt;
            prompt += "=== REFERENCE-DRIVEN MODE ===\n";
            prompt += "El Reference-Driven Mode esta ACTIVO. La referencia es el norte absoluto.\n";
            prompt += "Referencia: " + refName + "\n\n";

            // Progreso actual
            prompt += referenceProgress_.toLLMContext();
            prompt += "\n";

            // Gaps activos (top 3)
            auto gaps = getReferenceGaps();
            if (!gaps.empty()) {
                prompt += "Gaps actuales contra referencia:\n";
                int maxShow = std::min(static_cast<int>(gaps.size()), 5);
                for (int i = 0; i < maxShow; ++i) prompt += "  " + gaps[i].toLLMContextGap() + "\n";
                prompt += "\n";
            }

            // Siguiente paso del plan
            if (plan.hasPlan()) {
                const auto* currentStep = plan.getCurrentStep();
                if (currentStep != nullptr) {
                    prompt += "Siguiente paso en el plan:\n";
                    prompt += "  " + currentStep->toTextSummary() + "\n";
                }
                else if (plan.getProgress() >= 0.95f) {
                    prompt += "¡Todos los pasos del plan completados! La mezcla esta alineada con la referencia.\n";
                }
            }

            prompt += "\nGenera un comentario natural como ingeniero sobre el progreso contra la referencia.\n";
            prompt += "Si el match mejoro, celebralo brevemente. Si empeoro, avisa con calma.\n";
            prompt += "Di el numero exacto: \"Estamos al "
                      + juce::String(static_cast<int>(referenceProgress_.currentMatch * 100.0f))
                      + "% del match con la referencia\"\n";
            prompt += "NO estructures la respuesta. NO digas \"Claro!\" ni \"Por supuesto!\".\n";
            prompt += "Habla como ingeniero en el estudio. Maximo 3 oraciones.\n";

            // Enviar al LLM streaming
            bool accepted = llmStreamingCallback_(
                prompt,
                [this](const juce::String& token) {
                    if (streamTokenCb_) streamTokenCb_(token);
                },
                [this](const juce::String& response) {
                    if (streamEndedCb_) streamEndedCb_();
                    LogHelper::writeToLog("[CoachEngine] Reference-Driven Analysis enviada al chat");
                });

            if (accepted) {
                if (streamStartedCb_) streamStartedCb_();
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
        if (matchPct >= 80) msg += "🎯 **Muy cerca de la referencia** — el balance general está muy alineado.\n";
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
        if (!gaps.empty()) {
            // Filter critical/warning gaps, sort by priority
            std::vector<const DomainGap*> significantGaps;
            for (const auto& g : gaps) {
                if (g.severity == GapSeverity::Critical || g.severity == GapSeverity::Warning)
                    significantGaps.push_back(&g);
            }
            std::sort(significantGaps.begin(), significantGaps.end(), [](const DomainGap* a, const DomainGap* b) {
                return a->priority < b->priority;
            });

            if (!significantGaps.empty()) {
                int maxShow = std::min(static_cast<int>(significantGaps.size()), 2);
                msg += "\n📋 **Áreas con diferencia:**\n";
                for (int i = 0; i < maxShow; ++i) {
                    const auto& g = *significantGaps[i];
                    msg += "  • ";
                    msg += g.metric;
                    if (g.severity == GapSeverity::Critical) msg += " (requiere atención)";
                    else
                        msg += " (casi listo)";
                    msg += "\n";
                }
                if ((int)significantGaps.size() > maxShow) {
                    msg += "  • y ";
                    msg += juce::String((int)significantGaps.size() - maxShow);
                    msg += " más...\n";
                }
            }
        }

        // Fourth line: resolved gaps (achievement)
        if (referenceProgress_.resolvedGaps > 0) {
            msg += "\n✅ ";
            msg += juce::String(referenceProgress_.resolvedGaps);
            msg += " gap(s) resuelto(s) desde la última vez. ¡Buen trabajo!\n";
        }

        // Fifth line: next plan step
        if (planManager_.hasPlan()) {
            const auto* step = planManager_.getCurrentStep();
            if (step != nullptr) {
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
            if (info.muted || (soloActive_ && !info.soloed)) return;
            int idx = info.slotIndex;
            if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;

            auto result = sharedData_.getTrackAudioResult(idx);
            if (result.timestampUs <= 0) return;

            TrackRole role = trackRoles_[idx];
            trackFeedCore_->updateTrackState(idx, result, info, role);

            // --- SPRINT 6B: Role-aware dynamics analysis ---
            // Post-process con analyzeTrackDynamics() usando targets
            // por rol (ExpectedProfile.crestTargetDb) en vez de thresholds fijos.
            // Empuja eventos y actualiza health para que aparezcan en el banner UI.
            auto dynAdvice = analyzeTrackDynamics(idx);
            if (dynAdvice.isActionable()) {
                TrackEvent ev;
                ev.trackId     = idx;
                ev.timestampUs = result.timestampUs;
                ev.value       = dynAdvice.currentCrest;
                ev.threshold   = dynAdvice.crestTarget;
                ev.deviation   = dynAdvice.crestDeviation;
                ev.context     = "dynamics";

                // Cooldown: only push if enough time has passed
                int64_t nowTc = result.timestampUs > 0 ? result.timestampUs
                                                       : juce::Time::getMillisecondCounter() * 1000;
                if (!trackFeedCore_->checkAndSetCrestCooldown(idx, nowTc, kDynamicsAdviceCooldownUs)) return;

                if (dynAdvice.isOvercompressed()) {
                    ev.type     = TrackEventType::CrestTooLow;
                    ev.severity = 0.7f;
                }
                else if (dynAdvice.isTooDynamic()) {
                    ev.type     = TrackEventType::CrestTooHigh;
                    ev.severity = 0.5f;
                }
                else {
                    ev.type     = TrackEventType::Info;
                    ev.severity = 0.3f;
                }
                ev.message = dynAdvice.message;

                trackFeedCore_->pushTrackEvent(idx, ev);

                // Actualizar health segun el advice rol-aware
                if (dynAdvice.isOvercompressed()) trackFeedCore_->updateTrackHealth(idx, TrackHealth::Overcompressed);
                else if (dynAdvice.isTooDynamic())
                    trackFeedCore_->updateTrackHealth(idx, TrackHealth::NeedsCompression);
            }

            // --- SPRINT 6A: Role-aware gain analysis ---
            // Post-process con analyzeTrackGain() usando targets
            // por rol (ExpectedProfile.peakTargetDb) en vez de thresholds fijos.
            // Empuja eventos y actualiza health para que aparezcan en el banner UI.
            auto gainAdvice = analyzeTrackGain(idx);
            if (gainAdvice.isActionable()) {
                TrackEvent ev;
                ev.trackId     = idx;
                ev.timestampUs = result.timestampUs;
                ev.value       = gainAdvice.currentPeak;
                ev.threshold   = gainAdvice.peakTarget;
                ev.deviation   = gainAdvice.peakDeviation;
                ev.context     = "gain";

                // Cooldown: only push if enough time has passed
                int64_t nowGc = result.timestampUs > 0 ? result.timestampUs
                                                       : juce::Time::getMillisecondCounter() * 1000;
                if (!trackFeedCore_->checkAndSetGainCooldown(idx, nowGc, kGainAdviceCooldownUs)) return;

                if (gainAdvice.currentPeak > -0.5f) {
                    ev.type     = TrackEventType::ClippingDetected;
                    ev.severity = 0.9f;
                }
                else if (gainAdvice.currentPeak > -6.0f) {
                    ev.type     = TrackEventType::LevelSpike;
                    ev.severity = 0.6f;
                }
                else if (gainAdvice.currentPeak < -30.0f) {
                    ev.type     = TrackEventType::LowSignal;
                    ev.severity = 0.4f;
                }
                else {
                    ev.type     = TrackEventType::Info;
                    ev.severity = 0.3f;
                }
                ev.message = gainAdvice.message;

                trackFeedCore_->pushTrackEvent(idx, ev);

                // Actualizar health segun el advice rol-aware
                if (gainAdvice.currentPeak > -0.5f) trackFeedCore_->updateTrackHealth(idx, TrackHealth::ClippingRisk);
                else if (gainAdvice.currentPeak < -30.0f)
                    trackFeedCore_->updateTrackHealth(idx, TrackHealth::LowSignal);
            }

            // --- SPRINT 8: Role-aware phase analysis ---
            auto phaseAdvice = analyzeTrackPhase(idx);
            if (phaseAdvice.isActionable()) {
                TrackEvent ev;
                ev.trackId     = idx;
                ev.timestampUs = result.timestampUs;
                ev.value       = phaseAdvice.currentCorrelation;
                ev.threshold   = 0.3f;
                ev.deviation   = phaseAdvice.correlationDeviation;
                ev.context     = "phase";

                int64_t nowPh = result.timestampUs > 0 ? result.timestampUs
                                                       : juce::Time::getMillisecondCounter() * 1000;
                if (!trackFeedCore_->checkAndSetPhaseCooldown(idx, nowPh, kPhaseAdviceCooldownUs)) return;

                if (phaseAdvice.status == TrackPhaseAdvice::Status::OffTarget) {
                    ev.type     = TrackEventType::PhaseIssue;
                    ev.severity = 0.8f;
                }
                else {
                    ev.type     = TrackEventType::PhaseIssue;
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

        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) {
            advice.status = TrackGainAdvice::Status::UnknownRole;
            return advice;
        }

        // Get slot info
        auto info        = sharedData_.getSlotRegistry().getSlotInfo(slotIndex);
        advice.trackName = juce::String(info.trackName).trim();
        if (advice.trackName.isEmpty()) advice.trackName = "Pista " + juce::String(slotIndex + 1);

        // Get role
        advice.role = trackRoles_[slotIndex];
        if (advice.role == TrackRole::Unknown || advice.role == TrackRole::Master) {
            advice.status = TrackGainAdvice::Status::UnknownRole;
            return advice;
        }

        // Get telemetry
        auto telem = getLatestTelemetry(slotIndex);
        if (telem.timestamp == 0) {
            advice.status = TrackGainAdvice::Status::NoSignal;
            return advice;
        }

        // Current values
        advice.currentPeak  = juce::jmax(telem.peakLeft, telem.peakRight);
        advice.currentRMS   = (telem.rmsLeft + telem.rmsRight) * 0.5f;
        advice.currentCrest = telem.crestFactor;
        advice.currentLUFS  = computePerTrackLUFS(sharedData_.getTrackAudioResult(slotIndex));

        // Target from role
        auto profile         = getExpectedProfile(advice.role);
        advice.peakTarget    = profile.peakTargetDb;
        advice.crestTarget   = profile.crestTargetDb;
        advice.peakTolerance = profile.peakTolerance;

        // No signal check
        if (advice.currentPeak < -60.0f) {
            advice.status = TrackGainAdvice::Status::NoSignal;
            return advice;
        }

        // Deviation: positivo = track suena mas fuerte que el target
        advice.peakDeviation = advice.currentPeak - advice.peakTarget;

        // Status
        float tol = advice.peakTolerance;
        if (std::abs(advice.peakDeviation) <= tol) advice.status = TrackGainAdvice::Status::OnTarget;
        else if (std::abs(advice.peakDeviation) <= tol * 2.0f)
            advice.status = TrackGainAdvice::Status::NearTarget;
        else
            advice.status = TrackGainAdvice::Status::OffTarget;

        // Suggested delta: queremos acercarnos al target
        // Si peakDeviation > 0 (track mas fuerte), sugerimos bajar (delta negativo)
        // Si peakDeviation < 0 (track mas suave), sugerimos subir (delta positivo)
        advice.suggestedDeltaDb = juce::jlimit(-12.0f, 12.0f, -advice.peakDeviation);

        // Generate human-readable message
        switch (advice.status) {
            case TrackGainAdvice::Status::OnTarget:
                advice.message = "\u2705 " + advice.trackName + " \u2014 nivel en rango ("
                                 + juce::String(advice.currentPeak, 1) + " dBFS, target "
                                 + juce::String(advice.peakTarget, 1) + " dBFS)";
                break;
            case TrackGainAdvice::Status::NearTarget:
                if (advice.peakDeviation > 0.0f)
                    advice.message = "\xF0\x9F\x9F\xA1 " + advice.trackName + " \u2014 ligeramente alto ("
                                     + juce::String(advice.currentPeak, 1) + " dBFS). Baja ~"
                                     + juce::String(std::abs(advice.suggestedDeltaDb), 1) + " dB.";
                else
                    advice.message = "\xF0\x9F\x9F\xA1 " + advice.trackName + " \u2014 ligeramente bajo ("
                                     + juce::String(advice.currentPeak, 1) + " dBFS). Sube ~"
                                     + juce::String(std::abs(advice.suggestedDeltaDb), 1) + " dB.";
                break;
            case TrackGainAdvice::Status::OffTarget:
                if (advice.peakDeviation > 0.0f)
                    advice.message = "\xF0\x9F\x94\xB4 " + advice.trackName + " \u2014 demasiado alto ("
                                     + juce::String(advice.currentPeak, 1) + " dBFS, target "
                                     + juce::String(advice.peakTarget, 1) + " dBFS). Baja ~"
                                     + juce::String(std::abs(advice.suggestedDeltaDb), 1) + " dB.";
                else
                    advice.message = "\xF0\x9F\x94\xB4 " + advice.trackName + " \u2014 demasiado bajo ("
                                     + juce::String(advice.currentPeak, 1) + " dBFS, target "
                                     + juce::String(advice.peakTarget, 1) + " dBFS). Sube ~"
                                     + juce::String(std::abs(advice.suggestedDeltaDb), 1) + " dB.";
                break;
            case TrackGainAdvice::Status::NoSignal:
                advice.message = "\xF0\x9F\x94\x87 " + advice.trackName
                + " \u2014 sin sen\xC3\xB1""al detectable.";
                break;
            case TrackGainAdvice::Status::UnknownRole:
                advice.message = "\xE2\x9D\x93 " + advice.trackName + " \u2014 rol no especificado.";
                break;
        }

        return advice;
    }

    std::vector<CoachEngine::TrackGainAdvice> CoachEngine::analyzeAllTracksGain()
    {
        std::vector<TrackGainAdvice> results;
        auto& registry = sharedData_.getSlotRegistry();

        registry.forEachActive([&](const SlotInfo& info) {
            if (info.muted || (soloActive_ && !info.soloed)) return;
            auto advice = analyzeTrackGain(info.slotIndex);
            if (advice.isActionable()) results.push_back(advice);
        });

        // Sort by severity: OffTarget first, then NearTarget
        std::sort(results.begin(), results.end(), [](const TrackGainAdvice& a, const TrackGainAdvice& b) {
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

        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) {
            advice.status = TrackDynamicsAdvice::Status::UnknownRole;
            return advice;
        }

        // Get slot info
        auto info        = sharedData_.getSlotRegistry().getSlotInfo(slotIndex);
        advice.trackName = juce::String(info.trackName).trim();
        if (advice.trackName.isEmpty()) advice.trackName = "Pista " + juce::String(slotIndex + 1);

        // Get role
        advice.role = trackRoles_[slotIndex];
        if (advice.role == TrackRole::Unknown || advice.role == TrackRole::Master) {
            advice.status = TrackDynamicsAdvice::Status::UnknownRole;
            return advice;
        }

        // Get telemetry
        auto telem = getLatestTelemetry(slotIndex);
        if (telem.timestamp == 0 || telem.rmsLeft < -50.0f) {
            advice.status = TrackDynamicsAdvice::Status::NoSignal;
            return advice;
        }

        // Current values
        advice.currentCrest = telem.crestFactor;
        advice.currentPeak  = juce::jmax(telem.peakLeft, telem.peakRight);
        advice.currentRMS   = (telem.rmsLeft + telem.rmsRight) * 0.5f;

        // No viable crest data
        if (advice.currentCrest <= 0.0f) {
            advice.status = TrackDynamicsAdvice::Status::NoSignal;
            return advice;
        }

        // Target from role
        auto profile          = getExpectedProfile(advice.role);
        advice.crestTarget    = profile.crestTargetDb;
        advice.crestTolerance = profile.crestTolerance;

        // Deviation: positivo = mas dinamico que el target
        advice.crestDeviation = advice.currentCrest - advice.crestTarget;

        // Status
        float tol = advice.crestTolerance;
        if (std::abs(advice.crestDeviation) <= tol) {
            advice.status  = TrackDynamicsAdvice::Status::OnTarget;
            advice.subType = TrackDynamicsAdvice::SubType::None;
        }
        else if (std::abs(advice.crestDeviation) <= tol * 2.0f) {
            advice.status  = TrackDynamicsAdvice::Status::NearTarget;
            advice.subType = (advice.crestDeviation < 0) ? TrackDynamicsAdvice::SubType::Overcompressed
                                                         : TrackDynamicsAdvice::SubType::TooDynamic;
        }
        else {
            advice.status  = TrackDynamicsAdvice::Status::OffTarget;
            advice.subType = (advice.crestDeviation < 0) ? TrackDynamicsAdvice::SubType::Overcompressed
                                                         : TrackDynamicsAdvice::SubType::TooDynamic;
        }

        // Suggested action and message
        switch (advice.subType) {
            case TrackDynamicsAdvice::SubType::Overcompressed: {
                float deficit = advice.crestTarget - advice.currentCrest; // cuantos dB le falta
                advice.suggestedAction =
                    "Baja el ratio del compresor o sube el threshold " + juce::String(deficit * 0.5f, 1) + " dB.";
                advice.message = "\xF0\x9F\x94\xB4 " + advice.trackName + " \u2014 sobre-comprimido (crest "
                                 + juce::String(advice.currentCrest, 1) + " dB, target "
                                 + juce::String(advice.crestTarget, 1) + " dB). " + advice.suggestedAction;
                break;
            }
            case TrackDynamicsAdvice::SubType::TooDynamic: {
                advice.suggestedAction = "Prueba un compresor con ratio 4:1 y attack r\xC3\xA1pido (~10ms).";
                if (advice.currentCrest > advice.crestTarget + tol * 3.0f)
                    advice.suggestedAction = "Aplica compresi\xC3\xB3n suave 2:1 con threshold en -20 dB.";
                advice.message = "\xF0\x9F\x94\xB4 " + advice.trackName + " \u2014 demasiado din\xC3\xA1mico (crest "
                                 + juce::String(advice.currentCrest, 1) + " dB, target "
                                 + juce::String(advice.crestTarget, 1) + " dB). " + advice.suggestedAction;
                break;
            }
            case TrackDynamicsAdvice::SubType::None: {
                if (advice.status == TrackDynamicsAdvice::Status::NearTarget) {
                    advice.message         = "\xF0\x9F\x9F\xA1 " + advice.trackName + " \u2014 crest "
                                             + juce::String(advice.currentCrest, 1) + " dB (target "
                                             + juce::String(advice.crestTarget, 1) + " dB). Cerca del l\u00EDmite.";
                    advice.suggestedAction = "Monitorea, no requiere acci\xC3\xB3n inmediata.";
                }
                else {
                    advice.message = "\u2705 " + advice.trackName + " \u2014 crest "
                                     + juce::String(advice.currentCrest, 1) + " dB en rango (target "
                                     + juce::String(advice.crestTarget, 1) + " dB).";
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

        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) {
            advice.status = TrackTonalAdvice::Status::NoSignal;
            return advice;
        }

        // ─── Obtener info del slot ──────────────────────────────────────────
        auto& registry = sharedData_.getSlotRegistry();
        auto info      = registry.getSlotInfo(slotIndex);
        if (info.slotIndex < 0) {
            advice.status = TrackTonalAdvice::Status::NoSignal;
            return advice;
        }

        juce::String trackName = juce::String(info.trackName).trim();
        if (trackName.isEmpty()) trackName = "Pista " + juce::String(slotIndex + 1);
        advice.trackName = trackName;

        // ─── Obtener rol ────────────────────────────────────────────────────
        TrackRole role = trackRoles_[slotIndex];
        if (role == TrackRole::Unknown || role == TrackRole::Master) {
            advice.role   = role;
            advice.status = TrackTonalAdvice::Status::UnknownRole;
            return advice;
        }
        advice.role = role;

        // ─── Obtener telemetría ─────────────────────────────────────────────
        auto telem = getLatestTelemetry(slotIndex);
        if (telem.timestamp == 0) {
            advice.status = TrackTonalAdvice::Status::NoSignal;
            return advice;
        }

        // Verificar que hay señal suficiente
        float peakDb = juce::jmax(telem.peakLeft, telem.peakRight);
        if (peakDb < -60.0f) {
            advice.currentPeak = peakDb;
            advice.status      = TrackTonalAdvice::Status::NoSignal;
            return advice;
        }
        advice.currentPeak = peakDb;

        // ─── Obtener perfil esperado del rol ────────────────────────────────
        auto profile = getExpectedProfile(role);

        // ─── Mapear 30 bandEnergies a 6 regiones espectrales ──────────────────
        for (int region = 0; region < 6; ++region) {
            int bandStart = region * 5;
            int bandEnd   = bandStart + 5;

            float sumEnergies = 0.0f;
            int validBands    = 0;
            for (int b = bandStart; b < bandEnd && b < 30; ++b) {
                if (telem.bandEnergies[b] > -80.0f) {
                    sumEnergies += telem.bandEnergies[b];
                    validBands++;
                }
            }

            if (validBands > 0) advice.regionEnergy[region] = sumEnergies / (float)validBands;
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
        for (int r = 0; r < 6; ++r) {
            float absDev = std::abs(advice.regionDeviation[r]);
            if (absDev > maxAbsDev) {
                maxAbsDev   = absDev;
                worstRegion = r;
            }
        }
        advice.worstRegion    = worstRegion;
        advice.worstDeviation = (worstRegion >= 0) ? advice.regionDeviation[worstRegion] : 0.0f;

        // ─── Determinar status ────────────────────────────────────────────────
        if (worstRegion < 0) {
            advice.status = TrackTonalAdvice::Status::OnTarget;
            return advice;
        }

        float worstAbs = std::abs(advice.worstDeviation);
        if (worstAbs <= TrackTonalAdvice::kToleranceDb) {
            advice.status  = TrackTonalAdvice::Status::OnTarget;
            advice.message = "\xF0\x9F\x9F\xA2 **" + trackName + "** \xE2\x80\x94 balance espectral en rango ("
                             + juce::String(profile.name) + ").";
        }
        else if (worstAbs <= TrackTonalAdvice::kNearToleranceDb) {
            advice.status          = TrackTonalAdvice::Status::NearTarget;
            advice.isExcess        = (advice.worstDeviation > 0.0f);
            const char* regionName = TrackTonalAdvice::kRegionName(worstRegion);
            float deviation        = advice.worstDeviation;
            juce::String action;
            if (advice.isExcess)
                action = "Exceso en **" + juce::String(regionName) + "** (" + juce::String(deviation, 1)
                         + " dB sobre target). " + juce::String(TrackTonalAdvice::kExcessSuggestion(worstRegion));
            else
                action = "Falta en **" + juce::String(regionName) + "** (" + juce::String(-deviation, 1)
                         + " dB bajo target). " + juce::String(TrackTonalAdvice::kDeficitSuggestion(worstRegion));

            advice.message =
                "\xF0\x9F\x9F\xA1 **" + trackName + "** \xE2\x80\x94 ligero desbalance espectral " + action;
        }
        else {
            advice.status          = TrackTonalAdvice::Status::OffTarget;
            advice.isExcess        = (advice.worstDeviation > 0.0f);
            const char* regionName = TrackTonalAdvice::kRegionName(worstRegion);
            const char* freqRange  = TrackTonalAdvice::kRegionFreq(worstRegion);

            if (advice.isExcess) {
                advice.message = "\xF0\x9F\x94\xB4 **" + trackName + "** \xE2\x80\x94 exceso de energ\xC3\xAD""a en **"
                            + juce::String(regionName) + "** (" + juce::String(freqRange) + ", "
                            + juce::String(advice.regionEnergy[worstRegion], 1) + " dBFS, esperado "
                            + juce::String(advice.regionExpected[worstRegion], 1) + " dBFS). "
                            + "Prueba " + juce::String(TrackTonalAdvice::kExcessSuggestion(worstRegion)) + ".";
            }
            else {
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
            if (info.muted || (soloActive_ && !info.soloed)) return;
            auto advice = analyzeTrackDynamics(info.slotIndex);
            if (advice.isActionable()) results.push_back(advice);
        });

        // Sort by severity: OffTarget first (overcompressed antes que tooDynamic), then NearTarget
        std::sort(results.begin(), results.end(), [](const TrackDynamicsAdvice& a, const TrackDynamicsAdvice& b) {
            auto sev = [](const TrackDynamicsAdvice& adv) -> int {
                if (adv.status == TrackDynamicsAdvice::Status::OffTarget) return adv.isOvercompressed() ? 0 : 1;
                if (adv.status == TrackDynamicsAdvice::Status::NearTarget) return 2;
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
            if (info.muted || (soloActive_ && !info.soloed)) return;
            auto advice = analyzeTrackTonal(info.slotIndex);
            if (advice.isActionable()) results.push_back(advice);
        });

        // Sort by severity: OffTarget first, then NearTarget
        std::sort(results.begin(), results.end(), [](const TrackTonalAdvice& a, const TrackTonalAdvice& b) {
            auto sev = [](const TrackTonalAdvice& adv) -> int {
                if (adv.status == TrackTonalAdvice::Status::OffTarget) return adv.isExcess ? 0 : 1;
                if (adv.status == TrackTonalAdvice::Status::NearTarget) return 2;
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
        SlotInfo info  = registry.getSlotInfo(slotIndex);
        if (!info.active) return advice;
        advice.trackName = juce::String(info.trackName).trim();
        if (advice.trackName.isEmpty()) advice.trackName = "Track " + juce::String(slotIndex + 1);
        advice.role = trackRoles_[slotIndex];

        auto telem = getLatestTelemetry(slotIndex);
        if (telem.timestamp == 0) {
            advice.status = TrackPhaseAdvice::Status::NoSignal;
            return advice;
        }

        float corr                = telem.correlation;
        float peak                = juce::jmax(telem.peakLeft, telem.peakRight);
        advice.currentCorrelation = corr;
        advice.currentPeak        = peak;
        if (peak < -40.0f) {
            advice.status = TrackPhaseAdvice::Status::NoSignal;
            return advice;
        }

        if (corr < 0.0f) {
            advice.status               = TrackPhaseAdvice::Status::OffTarget;
            advice.correlationDeviation = corr - 0.0f;
            advice.message =
                "\xf0\x9f\x94\xae " + advice.trackName + " correlacion negativa (" + juce::String(corr, 2) + ")";
        }
        else if (corr < 0.3f) {
            advice.status               = TrackPhaseAdvice::Status::NearTarget;
            advice.correlationDeviation = corr - 0.3f;
            advice.message =
                "\xf0\x9f\x94\xae " + advice.trackName + " correlacion baja (" + juce::String(corr, 2) + ")";
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
            if (info.muted || (soloActive_ && !info.soloed)) return;
            int idx = info.slotIndex;
            if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;
            auto telem = getLatestTelemetry(idx);
            if (telem.timestamp == 0) return;
            if (info.muted || (soloActive_ && !info.soloed)) return;
            float peak = juce::jmax(telem.peakLeft, telem.peakRight);
            if (peak < -40.0f) return;
            auto phase = analyzeTrackPhase(idx);
            if (phase.isActionable()) results.push_back(phase);
        });
        std::sort(results.begin(), results.end(), [](const TrackPhaseAdvice& a, const TrackPhaseAdvice& b) {
            if (a.status != b.status) return a.status == TrackPhaseAdvice::Status::OffTarget;
            return std::abs(a.correlationDeviation) > std::abs(b.correlationDeviation);
        });
        return results;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  STUB IMPLEMENTATIONS — Methods restored from loss due to git checkout
    // ═══════════════════════════════════════════════════════════════════════════ — Methods restored from loss due to
    // git checkout ═══════════════════════════════════════════════════════════════════════════

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

    std::vector<AnalyzerInterpretation> CoachEngine::getCurrentInterpretations(const juce::String& genre) const
    {
        // Get aggregate metrics from shared data and delegate to AnalyzerInterpreter
        auto& registry    = sharedData_.getSlotRegistry();
        float correlation = 0.0f, crestDb = 0.0f, centroidRatio = 1.0f;
        float integratedLUFS = -100.0f, truePeakDBTP = -100.0f, lra = 0.0f;
        int count = 0;

        registry.forEachActive([&](const SlotInfo& info) {
            auto result = sharedData_.getTrackAudioResult(info.slotIndex);
            if (result.timestampUs <= 0) return;
            correlation += result.correlation;
            crestDb += result.crestPerBand[0];
            integratedLUFS = juce::jmax(integratedLUFS, result.getRmsCombined() + 2.0f);
            truePeakDBTP   = juce::jmax(truePeakDBTP, result.getPeakCombined());
            count++;
        });

        if (count > 0) {
            correlation /= (float)count;
            crestDb /= (float)count;
        }

        return AnalyzerInterpreter::interpretAll(
            correlation, crestDb, centroidRatio, integratedLUFS, truePeakDBTP, lra, genre);
    }

    std::vector<SemanticDiff> CoachEngine::runSemanticAnalysis() const
    {
        // Delegate to static SemanticComparator::compareAllTracks
        return SemanticComparator::compareAllTracks(sharedData_.getSlotRegistry(), sharedData_, trackRoles_);
    }

    float CoachEngine::computePerTrackLUFS(const TrackAudioResult& result) noexcept
    {
        // Approximate LUFS from RMS + high-frequency boost
        float rmsDb = result.getRmsCombined();
        if (rmsDb < -90.0f) return -100.0f;
        // Simple K-weighting approximation: boost high bands
        float highEnergy = 0.0f;
        int highCount    = 0;
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
            case BusType::Drums:
                return juce::String("🥁");
            case BusType::Bass:
                return juce::String("🎸");
            case BusType::Guitars:
                return juce::String("🎸");
            case BusType::Keys:
                return juce::String("🎹");
            case BusType::Vocals:
                return juce::String("🎤");
            case BusType::FX:
                return juce::String("🎛️");
            default:
                return juce::String("📡");
        }
    }

    CoachEngine::CentroidInfo CoachEngine::getCentroidInfo(const juce::String& genre) const
    {
        CentroidInfo info;
        info.genre      = genre;
        info.expectedHz = expectedCentroidForGenre(genre);
        // Aggregate from active tracks
        auto& registry    = sharedData_.getSlotRegistry();
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
                if (e > 0.0f) {
                    sum += e;
                    weighted += e * (float)(b * 200);
                }
            }
            if (sum > 0.0f) {
                weightedFreq += weighted / sum;
                totalEnergy += energy;
                count++;
            }
        });
        if (count > 0 && totalEnergy > 0.0f) info.actualHz = weightedFreq / count;
        return info;
    }

    void CoachEngine::fastTrackAnalysis()
    {
        auto now = juce::Time::getMillisecondCounter() * 1000;
        if (now - lastFastAnalysisUs_ < kFastAnalysisIntervalUs) return;
        lastFastAnalysisUs_ = now;

        auto& registry = sharedData_.getSlotRegistry();
        if (registry.activeCount() == 0) return;

        // Quick peak/RMS/correlation check per track
        registry.forEachActive([&](const SlotInfo& info) {
            if (info.muted || (soloActive_ && !info.soloed)) return;
            auto telem = getLatestTelemetry(info.slotIndex);
            if (telem.timestamp == 0) return;
            if (info.muted || (soloActive_ && !info.soloed)) return;
            auto& state           = trackStates_[info.slotIndex];
            state.lastPeakDb      = juce::jmax(telem.peakLeft, telem.peakRight);
            state.lastRmsDb       = telem.rmsLeft;
            state.lastCorrelation = telem.correlation;
        });
    }

    void CoachEngine::checkAndSendProactiveTip()
    {
        auto now = juce::Time::getMillisecondCounter() * 1000;
        if (now - lastProactiveTipTimeUs_ < kProactiveTipIntervalUs) return;
        lastProactiveTipTimeUs_ = now;

        auto& registry = sharedData_.getSlotRegistry();
        if (registry.activeCount() == 0) return;

        // Delegate to the existing proactive tip generator
        generateProactiveTip();
    }

    const CoachEngine::GenreTargetProfile& CoachEngine::getGenreProfile(const juce::String& genre)
    {
        // Static default profile
        static const GenreTargetProfile defaultProfile = {-14.0f, // targetIntegratedLUFS
                                                          2.0f,   // lufsTolerance
                                                          12.0f,  // targetCrestFactor
                                                          4.0f,   // crestTolerance
                                                          -6.0f,  // targetHeadroomDb
                                                          "Default mix profile",
                                                          0.0f,
                                                          -0.5f,
                                                          0.0f,
                                                          0.0f,
                                                          0.0f,
                                                          0.0f};
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
        int active     = registry.activeCount();

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

        if (hasReference()) suggestions.push_back("¿Cómo sueno vs referencia?");
        if (active >= 3) suggestions.push_back("Enmascaramiento?");

        return suggestions;
    }

    DifferenceProfile CoachEngine::buildDifferenceProfile() const
    {
        DifferenceProfile profile;
        profile.timestampUs = juce::Time::getMillisecondCounter() * 1000;
        if (referenceFingerprint_.valid) {
            profile.referenceName     = referenceMetadata_.name;
            profile.refIntegratedLUFS = referenceFingerprint_.lufsIntegrated;
            profile.refShortTermLUFS  = referenceFingerprint_.lufsShortTerm;
            profile.refMomentaryLUFS  = referenceFingerprint_.lufsMomentary;
            profile.refCrestFactor    = referenceFingerprint_.crestFactor;
            profile.refCorrelation    = referenceFingerprint_.correlation;
            profile.refTruePeakDBTP   = referenceFingerprint_.truePeakDBTP;
            // Map 30-band to 6-region
            for (int r = 0; r < 6; ++r) {
                int start = r * 5;
                float sum = 0.0f;
                for (int b = start; b < start + 5 && b < 30; ++b) sum += referenceFingerprint_.bandEnergies[b];
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
        auto& registry  = sharedData_.getSlotRegistry();
        map.timestampUs = juce::Time::getMillisecondCounter() * 1000;
        map.totalTracks = registry.activeCount();
        // Build minimal category list
        auto categoryName = [](RoleCategory cat) -> const char* {
            switch (cat) {
                case RoleCategory::Drums:
                    return "BATERIA";
                case RoleCategory::Bass:
                    return "BAJO";
                case RoleCategory::Guitars:
                    return "GUITARRAS";
                case RoleCategory::Keys:
                    return "TECLADOS";
                case RoleCategory::Vocals:
                    return "VOCES";
                case RoleCategory::FX:
                    return "EFECTOS";
                case RoleCategory::Melody:
                    return "MELODICOS";
                default:
                    return "OTROS";
            }
        };
        auto categoryEmoji = [](RoleCategory cat) -> const char* {
            switch (cat) {
                case RoleCategory::Drums:
                    return "🥁";
                case RoleCategory::Bass:
                    return "🎸";
                case RoleCategory::Guitars:
                    return "🎸";
                case RoleCategory::Keys:
                    return "🎹";
                case RoleCategory::Vocals:
                    return "🎤";
                case RoleCategory::FX:
                    return "🎛️";
                case RoleCategory::Melody:
                    return "🎵";
                default:
                    return "📡";
            }
        };
        std::vector<RoleCategory> categories = {RoleCategory::Drums,
                                                RoleCategory::Bass,
                                                RoleCategory::Guitars,
                                                RoleCategory::Keys,
                                                RoleCategory::Vocals,
                                                RoleCategory::FX,
                                                RoleCategory::Melody};
        for (auto cat : categories) {
            SessionMapCategory mapCat;
            mapCat.name  = juce::String(categoryName(cat));
            mapCat.emoji = juce::String(categoryEmoji(cat));
            registry.forEachActive([&](const SlotInfo& info) {
                if (info.muted || (soloActive_ && !info.soloed)) return;
                int idx = info.slotIndex;
                if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;
                if (getRoleCategory(trackRoles_[idx]) != cat) return;
                SessionMapEntry entry;
                entry.trackName  = juce::String(info.trackName).trim();
                entry.roleName   = juce::String(getRoleName(trackRoles_[idx]));
                entry.slotIndex  = idx;
                entry.busType    = (int)info.bus;
                entry.trackType  = info.trackType;
                entry.confidence = trackRoleWasInferred_[idx] ? 0.7f : 1.0f;
                auto telem       = getLatestTelemetry(idx);
                entry.hasSignal  = (telem.timestamp > 0);
                if (entry.hasSignal) entry.peakDb = juce::jmax(telem.peakLeft, telem.peakRight);
                mapCat.tracks.push_back(entry);
            });
            if (!mapCat.tracks.empty()) map.categories.push_back(mapCat);
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
        auto& registry      = sharedData_.getSlotRegistry();
        int bussedCount     = 0;
        int totalActive     = 0;
        int identifiedCount = 0;

        registry.forEachActive([&](const SlotInfo& info) {
            totalActive++;
            if (info.bus != BusType::None) bussedCount++;
            int idx = info.slotIndex;
            if (idx >= 0 && idx < SlotRegistry::kMaxSlots && trackRoles_[idx] != TrackRole::Unknown
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

        LogHelper::writeToLog("[Sprint2] Mapa confirmado: " + juce::String(bussedCount) + "/"
                              + juce::String(totalActive) + " bussed, " + juce::String(identifiedCount)
                              + " identified");

        // Check if phase auto-advances
        if (phaseManager_.getCurrentPhase() == MentorPhase::Organizacion && phaseManager_.evaluateAndAutoAdvance()) {
            auto newPhase = phaseManager_.getCurrentPhase();
            juce::String msg;

            msg += "\xF0\x9F\x97\xBA **Organizaci\xC3\xB3n completa!** Avanzamos a **"
                   + juce::String(phaseNames[static_cast<int>(newPhase)]) + "**.\n\n";

            msg += "\xF0\x9F\x93\x8B Ahora ajustemos niveles para tener headroom saludable.";
            respondWith(msg, MentorMessage::Type::Achievement);

            // Send phase guidance for the new phase
            sendPhaseGuidance(newPhase);

            LogHelper::writeToLog("[Sprint2] Fase avanzada: Organizacion -> "
                                  + juce::String(phaseNames[static_cast<int>(newPhase)]));
        }
        else {
            // Phase not complete yet \u2014 acknowledge the map confirmation
            respondWith("\xE2\x9C\x85 **Mapa de mezcla confirmado.** " + juce::String(bussedCount) + "/"
                            + juce::String(totalActive) + " pistas con bus asignado.",
                        MentorMessage::Type::Info);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  TrackAdvice::computeConsolidated — Consolida Gain + Dynamics + Tonal + Phase
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::TrackAdvice::computeConsolidated() noexcept
    {
        // Mapeo de status a severidad (0 = OnTarget, 3 = OffTarget)
        auto severityLevel = [](const auto& advice) -> int {
            using S = typename std::decay_t<decltype(advice)>::Status;
            S s     = advice.status;
            if (s == S::OffTarget) return 3;
            if (s == S::NearTarget) return 2;
            if (s == S::OnTarget) return 1;
            return 0;
        };

        struct DomainEntry
        {
            int severity;
            Domain domain;
            const juce::String* message;
        };

        DomainEntry entries[4] = {{severityLevel(gain), Domain::Gain, &gain.message},
                                  {severityLevel(dynamics), Domain::Dynamics, &dynamics.message},
                                  {severityLevel(tonal), Domain::Tonal, &tonal.message},
                                  {severityLevel(phase), Domain::Phase, &phase.message}};

        int worstSev = 0;
        worstDomain  = Domain::None;
        worstMessage.clear();

        for (const auto& entry : entries) {
            if (entry.severity > worstSev) {
                worstSev    = entry.severity;
                worstDomain = entry.domain;
                if (entry.message != nullptr) worstMessage = *entry.message;
            }
        }

        switch (worstSev) {
            case 3:
                status               = Status::OffTarget;
                consolidatedSeverity = 0.8f;
                break;
            case 2:
                status               = Status::NearTarget;
                consolidatedSeverity = 0.4f;
                break;
            case 1:
                status               = Status::OnTarget;
                consolidatedSeverity = 0.1f;
                break;
            case 0:
                status               = Status::NoSignal;
                consolidatedSeverity = 0.0f;
                break;
            default:
                status               = Status::UnknownRole;
                consolidatedSeverity = 0.0f;
                break;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  SPRINT 7: analyzeAllTracks — Analiza TODAS las pistas y consolida
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::analyzeAllTracks()
    {
        for (auto& ta : trackAdvices_) ta = TrackAdvice{};

        auto& registry = sharedData_.getSlotRegistry();

        registry.forEachActive([&](const SlotInfo& info) {
            int idx = info.slotIndex;
            if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;

            if (info.muted || (soloActive_ && !info.soloed)) return;

            TrackAdvice& ta = trackAdvices_[idx];
            ta.slotIndex    = idx;
            ta.trackName    = juce::String(info.trackName).trim();
            if (ta.trackName.isEmpty()) ta.trackName = "Track " + juce::String(idx + 1);
            ta.role = trackRoles_[idx];

            ta.gain     = analyzeTrackGain(idx);
            ta.dynamics = analyzeTrackDynamics(idx);
            ta.tonal    = analyzeTrackTonal(idx);
            ta.phase    = analyzeTrackPhase(idx);

            ta.computeConsolidated();
        });

        LogHelper::writeToLog("[Sprint7] analyzeAllTracks() completado — " + juce::String(registry.activeCount())
                              + " pistas analizadas");
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  SPRINT 7: getTrackAdvice — Retorna el TrackAdvice cacheado para un slot
    // ═══════════════════════════════════════════════════════════════════════════

    CoachEngine::TrackAdvice CoachEngine::getTrackAdvice(int slotIndex) const noexcept
    {
        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) return TrackAdvice{};

        return trackAdvices_[slotIndex];
    }

} // namespace mixcoach
