#include "CoachEngine.h"
#include "PluginScanner.h"
#include "../../Common/types/Constants.h"
#include "../../Common/types/LogHelper.h"
#include "../../Common/name/NameInferrer.h"
#include "../../Messenger/core/MessengerType.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <vector>
#include <map>
#include <cmath>

namespace mixcoach {

    // Forward declarations for free functions defined later in this file
    TrackRole getTrackRoleForTrackType(TrackType type) noexcept;
    TrackType getTrackTypeForRole(TrackRole role) noexcept;

    CoachEngine::CoachEngine(PhaseManager& phaseManager, SharedData& sharedData, AudioAnalyzer& audioAnalyzer) :
        phaseManager_(phaseManager),
        sharedData_(sharedData),
        audioAnalyzer_(audioAnalyzer),
        trackFeedCore_(std::make_unique<TrackFeedCore>()),
        sessionProgression_()
    {
        // Cargar base de datos de plugins
        pluginSuggestionsProvider_.setDatabasePath("plugins/plugin_db.json");
        pluginSuggestionsProvider_.initialize();

        // ─── Escanear plugins VST3 instalados y conectar con PluginSuggestionsProvider ──
        {
            PluginScanner scanner;
            scanner.onPluginDetected = [this](const juce::String& pluginId) {
                pluginSuggestionsProvider_.addKnownPlugin(pluginId);
            };
            scanner.onUnknownPluginDetected = [](const juce::String& displayName) {
                LogHelper::writeToLog("[PluginScanner] Plugin desconocido detectado: " + displayName);
            };
            int count = scanner.scanAll();
            if (count > 0) {
                LogHelper::writeToLog("[CoachEngine] Plugins escaneados: "
                                      + juce::String(count) + " bundles VST3, "
                                      + juce::String((int)scanner.getDetectedPluginIds().size())
                                      + " conocidos, "
                                      + juce::String((int)scanner.getUnknownPlugins().size())
                                      + " desconocidos");
            }
        }

        // ─── ProgressTracker: milestone callback (celebración al cruzar umbrales) ─
        progressTracker_.setMilestoneCallback([this](float oldScore, float newScore, int milestoneIndex) {
            juce::String msg;
            switch (milestoneIndex) {
                case 0: // 25%
                    msg = "Empezamos a alinear la mezcla con la referencia. "
                          + juce::String((int)newScore) + "% de match.";
                    break;
                case 1: // 45%
                    msg = "Ya tenemos un **" + juce::String((int)newScore)
                          + "%** de match. La direccion es correcta.";
                    break;
                case 2: // 60%
                    msg = "**" + juce::String((int)newScore)
                          + "% de match**! Mas de la mitad del camino. Sigue ajustando.";
                    break;
                case 3: // 75%
                    msg = "Excelente! **" + juce::String((int)newScore)
                          + "% de match**. Tu mezcla ya suena muy cerca de la referencia.";
                    break;
                case 4: // 90%
                    msg = "**" + juce::String((int)newScore)
                          + "% de match**! Casi identica a la referencia. Tiempo de afinar detalles.";
                    break;
                default:
                    break;
            }
            if (msg.isNotEmpty())
                respondWithPremium(msg, MentorMessage::Type::Achievement);
        });

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
        telem.timestamp     = result.timestampUs;
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
        msg.text      = personalize(text).toStdString();
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
        msg.text      = personalize(text).toStdString();
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
        // Resetear contador de consistencia entre secciones (Automation 3.3)
        consistentTransitions_ = 0;

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

        // ═══ Sprint 4: Detectar cambios de fase para emitir DirectorEvent ═══
        MentorPhase previousPhase = phaseManager_.getCurrentPhase();

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


        // ═══ Empty Session Detection (5.1) — Guía cuando no hay pistas ═══
        // Si no hay pistas activas por >30s, el coach guía al usuario.
        // Se re-evalúa cada 30s hasta que aparezcan pistas.
        if (registry.activeCount() == 0) {
            if (now - lastEmptySessionWarningUs_ >= kEmptySessionCooldownUs) {
                lastEmptySessionWarningUs_ = now;
                sendEmptySessionGuide();
            }
            return;
        }

        LogHelper::writeToLog("[CoachEngine] Análisis periódico iniciado (" + juce::String(registry.activeCount())
                              + " pistas activas)");

        // ═══ SPRINT 5: Sincronizar TrackFeedCore con datos frescos ══════════
        syncTrackFeedCore();

        // ═══ SPRINT 7: Per-track consolidated analysis ═══════════════
        analyzeAllTracks();

        // ═══ CoachingStageManager: evaluar completitud de etapa actual ═══
        if (stageManager_.isInitialized()) {
            stageManager_.update(*this, audioAnalyzer_);
        }

        // ═══ SessionProgression: auto-detección de fase ════════════════
        // Detecta automáticamente si avanzamos a DeepAnalysis, GuidedCoaching,
        // Refinement, etc. basado en el estado del motor.
        if (sessionProgression_.updateFromEngine(*this, audioAnalyzer_)) {
            LogHelper::writeToLog("[CoachEngine] SessionProgression avanzó a fase: "
                                  + juce::String(SessionProgression::phaseShortName(sessionProgression_.currentPhase)));
        }

        // ═══ SPRINT 6A: Per-track gain analysis ═══════════════
        auto allGainAdvice = analyzeAllTracksGain();
        {
            int offTargetCount = 0;
            for (const auto& adv : allGainAdvice)
                if (adv.status == TrackGainAdvice::Status::OffTarget) offTargetCount++;                if (offTargetCount > 0 && now - lastGainAdviceUs_ >= kGainAdviceCooldownUs) {
                lastGainAdviceUs_ = now;
                juce::String examples;
                int shown = 0;
                int clipCount = 0;
                for (const auto& adv : allGainAdvice) {
                    if (adv.status == TrackGainAdvice::Status::OffTarget && shown < 3) {
                        if (shown > 0) examples += "\n";
                        // ═══ SPRINT: marcar CLIPPING si peak > -0.5 dBFS ═══════
                        if (adv.currentPeak > -0.5f) {
                            clipCount++;
                            juce::String clipMsg = "\xF0\x9F\x94\xB4 **" + adv.trackName
                                                   + "** est\xC3\xA1 recortando a **"
                                                   + juce::String(adv.currentPeak, 1) + " dB";
                            if (adv.peakTarget > -60.0f)
                                clipMsg += " (target " + juce::String(adv.peakTarget, 1) + " dBFS)";
                            clipMsg += ". Reduce el gain inmediatamente.";
                            examples += clipMsg;
                        } else {
                            examples += adv.message;
                        }
                        shown++;
                    }
                }
                if (offTargetCount == 1) {
                    respondWith(examples, clipCount > 0 ? MentorMessage::Type::Warning : MentorMessage::Type::Tip);
                }
                else {
                    juce::String header;
                    if (clipCount > 0)
                        header = "\xF0\x9F\x9A\xA8 **" + juce::String(clipCount)
                                 + (clipCount == 1 ? " pista" : " pistas")
                                 + " recortando y " + juce::String(offTargetCount - clipCount)
                                 + " fuera de rango \xF3\xBE\x90\xA2ptimo:**";
                    else
                        header = "[CHART] **" + juce::String(offTargetCount)
                                 + " pistas fuera de rango \xF3\xBE\x90\xA2ptimo:";
                    respondWith(header + "\n" + examples
                                    + "\n\xF0\x9F\x92\xA1 Ajusta el fader de gain de cada pista.",
                                MentorMessage::Type::Warning);
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
                    respondWith("[CHART] **" + juce::String(offTargetDyn)
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
        if (referenceFingerprint_.valid) {
            computeAndSendMatchData();

    // ─── ProgressTracker: snapshot cada 120s para timeline UI ────
    {
        auto dp = buildDifferenceProfile();
        if (dp.valid && progressTracker_.takeSnapshot(dp, now)) {
            // Si se tomó un snapshot nuevo, notificar a la UI via callback
            if (onTimelineUpdate_)
                onTimelineUpdate_(progressTracker_.getTimelinePoints(6));
        }
    }
        }

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
                analyzeSpaceReal();
                break;
            default:
                break;
        }

        // ═══ Sprint 4: Emitir eventos del Director para SceneManager ═══
        {
            // PhaseChanged: detectar si la fase avanzó durante este análisis
            MentorPhase currentPhase = phaseManager_.getCurrentPhase();
            if (currentPhase != previousPhase && directorEventCb_) {
                DirectorEvent phaseEv;
                phaseEv.type = DirectorEvent::Type::PhaseChanged;
                phaseEv.numericValue = static_cast<float>(currentPhase);
                phaseEv.payload = juce::String(phaseManager_.getPhaseName(currentPhase));
                phaseEv.timestamp = now;
                directorEventCb_(phaseEv);
            }

            // PriorityIssueDetected: informar sobre el issue de mayor prioridad
            // Usamos getMostUrgentRecommendation() como fuente de issue prioritario
            const TrackRecommendation* urgentRec = getMostUrgentRecommendation();
            if (urgentRec != nullptr && urgentRec->status == TrackRecommendation::Status::Pending && directorEventCb_) {
                float severity = 0.5f;
                if (urgentRec->domain == TrackRecommendation::Domain::Gain) severity = 0.8f;
                else if (urgentRec->domain == TrackRecommendation::Domain::Dynamics) severity = 0.7f;
                else if (urgentRec->domain == TrackRecommendation::Domain::Tonal) severity = 0.6f;

                DirectorEvent ev;
                ev.type = DirectorEvent::Type::PriorityIssueDetected;
                ev.numericValue = severity;
                ev.payload = urgentRec->action;
                ev.timestamp = now;
                directorEventCb_(ev);
            }

            // PhaseProgressUpdated: informar progreso de la fase actual
            float phaseProgress = phaseManager_.getPhaseProgress(currentPhase);
            if (directorEventCb_) {
                DirectorEvent progressEv;
                progressEv.type = DirectorEvent::Type::PhaseProgressUpdated;
                progressEv.numericValue = phaseProgress;
                progressEv.timestamp = now;
                directorEventCb_(progressEv);
            }
        }

        // ═══ FASE 2: Loop de Corrección — Verificar recomendaciones pendientes ═══
        // verifyTrackCorrections() compara métricas actuales vs esperadas para cada
        // recomendación Pending. Clasifica: Applied (✅), OverApplied (⚠️),
        // UnderApplied (💪), Ignored (⏭️). Genera feedback contextual en el chat.
        verifyTrackCorrections();

        // detectUnpromptedChanges() detecta cambios manuales en pistas SIN
        // recomendación activa (el usuario ajustó algo por su cuenta).
        // Máximo 1 observación por ciclo, cooldown 120s por pista.
        detectUnpromptedChanges();

        // ═══ Per-section metric collection (runs every cycle) ═══
        // Acumula métricas por pista para la sección musical actual en CADA ciclo
        // de periodicAnalysis(), no solo en transiciones. Esto permite calcular
        // promedios reales sobre la duración completa de cada sección.
        {
            auto currentSectionType = sectionDetector_.getCurrentSection().type;
            if (currentSectionType != SectionType::Unknown && currentSectionType == lastTrackedSection_) {
                registry.forEachActive([&](const SlotInfo& info) {
                    if (info.muted || info.slotIndex < 0 || info.slotIndex >= SlotRegistry::kMaxSlots)
                        return;
                    int idx = info.slotIndex;
                    auto result = sharedData_.getTrackAudioResult(idx);
                    if (result.timestampUs <= 0) return;

                    auto& metric = perSectionMetrics_[idx][static_cast<int>(currentSectionType)];
                    // Peak: max hold sobre toda la sección
                    metric.peakDb = std::max(metric.peakDb, juce::jmax(result.peakLeft, result.peakRight));
                    // RMS: promedio running sobre toda la sección
                    float currentRms = (result.rmsLeft + result.rmsRight) * 0.5f;
                    metric.rmsDb = (metric.rmsDb * static_cast<float>(metric.sampleCount) + currentRms)
                                   / static_cast<float>(metric.sampleCount + 1);
                    metric.crestDb = result.crestPerBand[0];
                    metric.correlation = result.correlation;
                    for (int b = 0; b < 6; ++b) {
                        float sum = 0.0f;
                        for (int sb = b * 5; sb < (b + 1) * 5 && sb < 30; ++sb)
                            sum += result.bandEnergies[sb];
                        metric.bandEnergy6[b] = sum / 5.0f;
                    }
                    metric.sampleCount++;
                });
            }
        }

        // ═══ Section Detection — detectar cambios de sección musical cada ~2s ═══
        {
            float rmsDb    = audioAnalyzer_.getMasterAnalysis().getRMS();
            float lufs     = audioAnalyzer_.getShortTermLUFS();
            float corr     = audioAnalyzer_.getMasterAnalysis().getCorrelation();
            float centroid = 0.0f;
            float songTime = static_cast<float>(now) / 1000000.0f;

            bool transition = sectionDetector_.analyzeFrame(rmsDb, lufs, corr, centroid, songTime, now);

            if (transition) {
                // ═══ Automation Suggestion (3.3): Transición detectada ═══
                // Las métricas de la sección anterior ya fueron acumuladas en el
                // bloque de recolección periódica (arriba). Aquí solo:
                // 1. Generamos sugerencia de automatización (compara promedios reales)
                // 2. Reseteamos el acumulador para la nueva sección
                // 3. Actualizamos lastTrackedSection_
                {
                    generateAutomationSuggestion(now);

                    // Resetear acumulador para la nueva sección
                    auto newSectionType = sectionDetector_.getCurrentSection().type;
                    if (newSectionType != lastTrackedSection_ && newSectionType != SectionType::Unknown) {
                        for (int s = 0; s < SlotRegistry::kMaxSlots; ++s) {
                            auto& metric = perSectionMetrics_[s][static_cast<int>(newSectionType)];
                            metric = PerSectionMetric{};
                        }
                        lastTrackedSection_ = newSectionType;
                    }
                }

                if (directorEventCb_) {
                    DirectorEvent ev;
                    ev.type = DirectorEvent::Type::PhaseChanged;
                    ev.payload = juce::String(sectionTypeName(sectionDetector_.getCurrentSection().type));
                    ev.numericValue = static_cast<float>(sectionDetector_.getCurrentSection().type);
                    ev.timestamp = now;
                    directorEventCb_(ev);
                }
            }
        }

        // ═══ Density Analysis — detectar congestión espectral del arreglo ═══
        {
            auto& registry = sharedData_.getSlotRegistry();
            auto result = densityAnalyzer_.analyze(
                registry.activeCount(),
                [&](int slotIdx) -> juce::String {
                    auto info = registry.getSlotInfo(slotIdx);
                    return juce::String(info.trackName).trim();
                },
                [&](int slotIdx) -> const float* {
                    return sharedData_.getTrackAudioResult(slotIdx).bandEnergies;
                },
                now);

            if (result.valid()) {
                juce::String msg = result.buildCongestionMessage();
                if (msg.isNotEmpty())
                    respondWith(msg, MentorMessage::Type::Tip);
            }
        }

        // ═══ Platform Target LUFS Warning — avisar si el master está lejos del target ═══
        // Cada 60s como máximo, avisa si el LUFS del master se desvía >2 LUFS del target.
        // Independiente de si hay referencia cargada — el target de plataforma aplica siempre.
        if (now - lastPlatformWarningUs_ >= 60 * 1000 * 1000) {
            float currentLUFS = audioAnalyzer_.getShortTermLUFS();
            float targetLUFS  = getDestinationLUFS();
            if (currentLUFS > -80.0f && std::abs(currentLUFS - targetLUFS) > 2.0f) {
                lastPlatformWarningUs_ = now;
                float diff = currentLUFS - targetLUFS;
                juce::String msg;
                msg += "[PLATFORM TARGET] Master at " + juce::String(currentLUFS, 1)
                       + " LUFS, target for " + juce::String(getPlatformTargetName())
                       + " is " + juce::String(targetLUFS, 0) + " LUFS.\n";
                msg += juce::String(std::abs(diff), 1) + " LUFS off target. "
                       + juce::String((diff > 0) ? "Reduce" : "Increase")
                       + " overall gain by approximately " + juce::String(std::abs(diff), 1) + " dB.";
                respondWith(msg, MentorMessage::Type::Tip);
            }
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
                    if (llmResponseCompleteCb_) llmResponseCompleteCb_(response);
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

        // Target from role (genre-aware)
        auto profile         = getExpectedProfile(advice.role, setupGenre_);
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
                advice.message = "[QUESTION] " + advice.trackName + " \u2014 rol no especificado.";
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

        // Target from role (genre-aware)
        auto profile          = getExpectedProfile(advice.role, setupGenre_);
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

        // ─── Obtener perfil esperado del rol (genre-aware) ────────────────
        auto profile = getExpectedProfile(role, setupGenre_);

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

    // ═══════════════════════════════════════════════════════════════════════════
    //  generateAutomationSuggestion — Sugerencias de automatización (3.3)
    //  Compara métricas por pista entre la sección anterior y la actual.
    //  Si la diferencia es >3dB en peak/RMS, sugiere automatizar el fader.
    //  Cooldown: 2 minutos entre sugerencias.
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::generateAutomationSuggestion(int64_t now)
    {
        // Cooldown: 2min entre sugerencias
        if (now - lastAutomationSuggestionUs_ < kAutomationSuggestionCooldownUs)
            return;

        auto currentSection = sectionDetector_.getCurrentSection();
        auto previousSection = sectionDetector_.getPreviousSection();

        // Necesitamos al menos 2 secciones diferentes para comparar
        if (currentSection.type == SectionType::Unknown
            || previousSection.type == SectionType::Unknown
            || currentSection.type == previousSection.type)
            return;

        auto& registry = sharedData_.getSlotRegistry();
        int prevSecIdx = static_cast<int>(previousSection.type);
        int currSecIdx = static_cast<int>(currentSection.type);

        // Buscar la pista con mayor variación entre secciones
        int bestSlot = -1;
        float maxDeltaDb = 0.0f;
        juce::String bestTrackName;
        bool isPeakDelta = true;
        float prevValue = 0.0f;
        float currValue = 0.0f;

        registry.forEachActive([&](const SlotInfo& info) {
            if (info.muted || info.slotIndex < 0 || info.slotIndex >= SlotRegistry::kMaxSlots)
                return;
            int idx = info.slotIndex;

            auto& prevMetric = perSectionMetrics_[idx][prevSecIdx];
            auto& currMetric = perSectionMetrics_[idx][currSecIdx];

            // Saltar si no hay suficientes muestras en alguna sección
            if (prevMetric.sampleCount < 2 || currMetric.sampleCount < 2)
                return;

            // Comparar peak
            float peakDelta = std::abs(currMetric.peakDb - prevMetric.peakDb);
            // Comparar RMS
            float rmsDelta = std::abs(currMetric.rmsDb - prevMetric.rmsDb);

            float delta = juce::jmax(peakDelta, rmsDelta);

            if (delta > maxDeltaDb && delta > 3.0f) {
                maxDeltaDb = delta;
                bestSlot = idx;
                bestTrackName = juce::String(info.trackName).trim();
                isPeakDelta = (peakDelta >= rmsDelta);
                prevValue = isPeakDelta ? prevMetric.peakDb : prevMetric.rmsDb;
                currValue = isPeakDelta ? currMetric.peakDb : currMetric.rmsDb;
            }
        });

        // ═══ Consistency Detection: si tras 3+ transiciones ninguna pista varía >3dB ═══
        // Envía mensaje de estabilidad con cooldown de 3min.
        // El contador se resetea cuando SÍ se genera una sugerencia (abajo).
        if (bestSlot < 0 || maxDeltaDb < 3.0f) {
            consistentTransitions_++;
            if (consistentTransitions_ >= 3
                && now - lastConsistencyMessageUs_ >= kConsistencyMessageCooldownUs) {
                lastConsistencyMessageUs_ = now;
                consistentTransitions_ = 0;
                juce::String stableMsg;
                stableMsg += "\xF0\x9F\x93\x8A **Tus pistas se mantienen estables entre secciones**\n\n";
                stableMsg += "No detecto variaciones significativas (>3dB) entre \""
                             + juce::String(sectionTypeName(previousSection.type))
                             + "\" y \"" + juce::String(sectionTypeName(currentSection.type))
                             + "\".\n\n";
                stableMsg += "Esto significa que **ajustes fijos son suficientes** "
                             "para toda la canci\xC3\xB3n — no necesitas automatizar faders\n"
                             "para compensar cambios de nivel entre secciones.";
                respondWith(stableMsg, MentorMessage::Type::Info);
                LogHelper::writeToLog("[CoachEngine] Consistency detected: "
                                      + juce::String(consistentTransitions_)
                                      + " transitions without >3dB variation. "
                                      + "Sent stability message.");
            }
            return;
        }

        // Reseteamos contador de consistencia al generar una sugerencia real
        consistentTransitions_ = 0;

        if (bestTrackName.isEmpty())
            bestTrackName = "Pista " + juce::String(bestSlot + 1);

        // ═══ Construir mensaje de sugerencia ═══════════════════════════════
        // Determinar qué sección es más fuerte
        bool louderInCurrent = (currValue > prevValue);
        const char* louderSectionName = louderInCurrent
            ? sectionTypeName(currentSection.type)
            : sectionTypeName(previousSection.type);
        const char* quieterSectionName = louderInCurrent
            ? sectionTypeName(previousSection.type)
            : sectionTypeName(currentSection.type);
        float deltaDb = std::abs(currValue - prevValue);

        // Tiempos para el mensaje (aproximados desde startTimeSec)
        float louderTimeSec = louderInCurrent
            ? currentSection.startTimeSec
            : previousSection.startTimeSec;
        float quieterTimeSec = louderInCurrent
            ? previousSection.startTimeSec
            : currentSection.startTimeSec;

        auto formatTime = [](float sec) -> juce::String {
            int min = (int)(sec / 60.0f);
            int seg = (int)sec % 60;
            return juce::String(min) + ":" + juce::String(seg).paddedLeft('0', 2);
        };

        juce::String metricLabel = isPeakDelta ? "nivel" : "RMS";

        juce::String msg;
        msg += "\xE2\x9C\xA8 **Sugerencia de automatizaci\xC3\xB3n**\n\n";
        msg += "**" + bestTrackName + "** tiene " + juce::String(deltaDb, 1)
               + " dB m\xC3\xA1s de " + metricLabel + " en el **"
               + juce::String(louderSectionName) + "** que en el **"
               + juce::String(quieterSectionName) + "**.\n\n";

        msg += "En vez de un ajuste fijo, prueba **automatizar el fader**:\n";
        msg += "  \xE2\x80\xA2 En **" + formatTime(louderTimeSec) + "** (" + juce::String(louderSectionName) + "): "
               + (louderInCurrent ? "baja" : "sube") + " ~" + juce::String(deltaDb, 1) + " dB\n";
        msg += "  \xE2\x80\xA2 En **" + formatTime(quieterTimeSec) + "** (" + juce::String(quieterSectionName) + "): "
               + (louderInCurrent ? "sube" : "baja") + " al nivel original\n\n";
        msg += "Esto mantendr\xC3\xA1 el balance correcto en cada secci\xC3\xB3n "
               "sin sacrificar din\xC3\xA1mica musical.";

        lastAutomationSuggestionUs_ = now;
        respondWith(msg, MentorMessage::Type::Tip);

        LogHelper::writeToLog("[CoachEngine] Automation suggestion: " + bestTrackName
                              + " varia " + juce::String(deltaDb, 1) + "dB entre "
                              + juce::String(sectionTypeName(previousSection.type)) + " → "
                              + juce::String(sectionTypeName(currentSection.type)));
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  sendEmptySessionGuide — Guía para proyectos sin pistas (5.1)
    //  Se dispara desde periodicAnalysis() cuando activeCount == 0 por >30s.
    //  Explica cómo configurar la sesión: insertar tracks, agregar Messenger,
    //  y ruteo. Se re-evalúa cada 30s hasta que aparezcan pistas.
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::sendEmptySessionGuide()
    {
        // ─── Mensaje 1: Guía inicial completa (solo la primera vez) ─────────
        // Si el setup no ha empezado, damos la bienvenida + guía completa.
        // Si ya está en setup, damos un recordatorio más corto.
        bool isFirstWarning = (lastEmptySessionWarningUs_ == 0);

        if (isFirstWarning || setupStep_ == SetupStep::NotStarted) {
            juce::String msg;
            msg += "\xF0\x9F\x94\x8D **No detecto pistas en tu proyecto.**\n\n";
            msg += "Para empezar a usar MixCoach, necesito escuchar tus pistas. "
                   "Sigue estos pasos:\n\n";
            msg += "**1. Crea canales de audio** en tu DAW\n";
            msg += "   \xE2\x80\xA2 Agrega pistas de audio para cada instrumento\n";
            msg += "   \xE2\x80\xA2 Aseg\xC3\xBArate de que tengan audio grabado o MIDI\n\n";
            msg += "**2. Inserta MixCoach Messenger** en cada canal\n";
            msg += "   \xE2\x80\xA2 Busca **Messenger** en tus plugins VST3\n";
            msg += "   \xE2\x80\xA2 Ins\xC3\xA9rtalo como \xFCltimo plugin en la cadena\n";
            msg += "   \xE2\x80\xA2 El Messenger enviar\xC3\xA1 el audio al Coach para an\xC3\xA1lisis\n\n";
            msg += "**3. Conecta el Master**\n";
            msg += "   \xE2\x80\xA2 MixCoach debe estar en el canal Master\n";
            msg += "   \xE2\x80\xA2 As\xC3\xAD puedo escuchar toda la mezcla\n\n";
            msg += "\xF0\x9F\x91\x89 Una vez que insertes los Messengers, "
                   "yo detectar\xC3\xA9 las pistas autom\xC3\xA1ticamente.";

            respondWith(msg, MentorMessage::Type::Info);

            LogHelper::writeToLog("[CoachEngine] Empty session guide enviada (primera vez)");
        } else {
            // ─── Mensaje 2+: Recordatorio corto (cooldown 30s) ────────────
            juce::String msg;
            msg += "\xF0\x9F\x94\x8D **Sigo sin detectar pistas.**\n\n";
            msg += "Recuerda:\n";
            msg += "   1. Crea canales de audio en tu proyecto\n";
            msg += "   2. Inserta **Messenger** (VST3) en cada canal\n";
            msg += "   3. MixCoach debe estar en el Master\n\n";
            msg += "Te avisar\xC3\xA9 cuando detecte las pistas.";

            respondWith(msg, MentorMessage::Type::Tip);

            LogHelper::writeToLog("[CoachEngine] Empty session recordatorio enviado");
        }
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
        juce::String g = genre.trim().toLowerCase();

        // ─── Trap: sub masivo, crest alto, headroom ajustado ────────────────
        if (g == "trap") {
            static const GenreTargetProfile profile = {
                -8.0f,  // targetIntegratedLUFS — más fuerte (competitivo)
                2.0f,   // lufsTolerance
                14.0f,  // targetCrestFactor — crest alto (dinámico)
                4.0f,   // crestTolerance
                -4.0f,  // targetHeadroomDb — headroom más ajustado
                "Trap: sub masivo, 808 dominante, agudos brillantes",
                -2.0f,  // subBassOffset — más sub
                -1.0f,  // bassOffset
                0.0f,   // lowMidOffset
                0.0f,   // highMidOffset
                2.0f,   // presenceOffset — presencia extra
                3.0f    // airOffset — aire brillante
            };
            return profile;
        }

        // ─── Pop: balanceado, vocal-forward, crest moderado ────────────────
        if (g == "pop") {
            static const GenreTargetProfile profile = {
                -10.0f, // targetIntegratedLUFS — estándar pop
                2.0f,   // lufsTolerance
                12.0f,  // targetCrestFactor — crest moderado
                4.0f,   // crestTolerance
                -6.0f,  // targetHeadroomDb — headroom estándar
                "Pop: vocal-forward, balance espectral equilibrado",
                0.0f,   // subBassOffset
                0.0f,   // bassOffset
                0.0f,   // lowMidOffset
                0.0f,   // highMidOffset
                1.0f,   // presenceOffset — presencia vocal
                1.0f    // airOffset — aire suave
            };
            return profile;
        }

        // ─── Rock: dinámico, guitarras presentes, crest natural ────────────
        if (g == "rock") {
            static const GenreTargetProfile profile = {
                -11.0f, // targetIntegratedLUFS — rango dinámico
                2.5f,   // lufsTolerance — más tolerancia (dinámica variable)
                14.0f,  // targetCrestFactor — crest alto (natural)
                5.0f,   // crestTolerance
                -6.0f,  // targetHeadroomDb
                "Rock: batería potente, guitarras presentes, dinámica natural",
                0.0f,   // subBassOffset
                1.0f,   // bassOffset — cuerpo extra
                2.0f,   // lowMidOffset — guitarras rítmicas
                3.0f,   // highMidOffset — guitarras líder
                1.0f,   // presenceOffset
                0.0f    // airOffset
            };
            return profile;
        }

        // ─── Reggaeton: bass dominante, bombo punchy, presencia vocal ─────
        if (g == "reggaeton" || g == "reggaeton/latin" || g == "latin" || g == "dembow") {
            static const GenreTargetProfile profile = {
                -8.0f,  // targetIntegratedLUFS — fuerte, como el género
                2.0f,   // lufsTolerance
                13.0f,  // targetCrestFactor
                4.0f,   // crestTolerance
                -4.0f,  // targetHeadroomDb
                "Reggaeton: 808 dominante, bombo punchy (mid-sub), voz clara",
                -3.0f,  // subBassOffset — sub pronunciado
                -1.0f,  // bassOffset
                0.0f,   // lowMidOffset
                0.0f,   // highMidOffset
                2.0f,   // presenceOffset — vocal presence
                1.0f    // airOffset
            };
            return profile;
        }

        // ─── Afrobeat: percusivo, cálido, bajos bailables ──────────────────
        if (g == "afrobeat" || g == "afrobeats" || g == "world") {
            static const GenreTargetProfile profile = {
                -10.0f, // targetIntegratedLUFS — balanceado
                2.5f,   // lufsTolerance
                14.0f,  // targetCrestFactor — crest alto (percusivo)
                5.0f,   // crestTolerance
                -6.0f,  // targetHeadroomDb
                "Afrobeat: percusivo, cálido, bajos con cuerpo",
                -1.0f,  // subBassOffset
                -1.0f,  // bassOffset — cuerpo extra
                1.0f,   // lowMidOffset
                1.0f,   // highMidOffset
                1.0f,   // presenceOffset — percusión brillante
                1.0f    // airOffset
            };
            return profile;
        }

        // ─── EDM: sub masivo, crest comprimido, presencia extrema ──────────
        if (g == "edm" || g == "electronic" || g == "house" || g == "techno" || g == "trance" || g == "dubstep") {
            static const GenreTargetProfile profile = {
                -9.0f,  // targetIntegratedLUFS — fuerte
                2.0f,   // lufsTolerance
                11.0f,  // targetCrestFactor — crest bajo (comprimido)
                4.0f,   // crestTolerance
                -4.0f,  // targetHeadroomDb — headroom ajustado
                "EDM: sub masivo, compresión fuerte, presencia extrema",
                -4.0f,  // subBassOffset — sub masivo
                -1.0f,  // bassOffset
                0.0f,   // lowMidOffset
                0.0f,   // highMidOffset
                2.0f,   // presenceOffset — presencia extrema
                3.0f    // airOffset — aire brillante
            };
            return profile;
        }

        // ─── Default: perfil genérico de mezcla ────────────────────────────
        {
            static const GenreTargetProfile defaultProfile = {
                -14.0f, // targetIntegratedLUFS
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
                0.0f
            };
            return defaultProfile;
        }
    }

    void CoachEngine::respondWithPremium(const juce::String& text, MentorMessage::Type type)
    {
        // Same as respondWith() but intended for conversational bubbles
        // The chat UI will render it as isSystem=false
        respondWith(text, type);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Acciones Directas desde TrackProblemCard — Día 3-4
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::recordManualApplication(int slotIndex, const juce::String& domain, const juce::String& trackName)
    {
        if (slotIndex < 0) return;

        // 1. Registrar en MixHistory
        MixHistoryEntry entry;
        entry.timestampUs = juce::Time::getMillisecondCounter() * 1000;
        entry.slotIndex   = slotIndex;
        entry.trackName   = trackName;
        entry.domain      = domain;
        entry.source      = MixHistoryEntry::Source::UserAction;

        // Mapear domain a description legible
        juce::String domainName;
        if (domain == "gain")        { domainName = "Gain Staging"; entry.description = "Ajuste manual de ganancia"; }
        else if (domain == "tonal")   { domainName = "EQ"; entry.description = "Ajuste manual de EQ"; }
        else if (domain == "dynamics") { domainName = "Compresión"; entry.description = "Ajuste manual de compresión"; }
        else if (domain == "spatial")  { domainName = "Espacio/Estéreo"; entry.description = "Ajuste manual de paneo/espacio"; }
        else                          { domainName = domain; entry.description = "Ajuste manual: " + domain; }

        pushMixHistory(entry);
        LogHelper::writeToLog("[AccionDirecta] recordManualApplication slot=" + juce::String(slotIndex)
                              + " domain=\"" + domain + "\" track=\"" + trackName + "\"");

        // 2. Responder como coach celebrando la acción
        juce::String msg = personalize("✅ **¡Registrado!** Buen ajuste en **" + domainName + "**");
        if (trackName.isNotEmpty())
            msg += " para **" + trackName + "**";
        msg += ". Sigue así, cada paso cuenta.";
        respondWithPremium(msg, MentorMessage::Type::Achievement);
    }

    void CoachEngine::skipProblem(int slotIndex, const juce::String& trackName)
    {
        if (slotIndex < 0) return;

        // 1. Registrar en MixHistory
        MixHistoryEntry entry;
        entry.timestampUs = juce::Time::getMillisecondCounter() * 1000;
        entry.slotIndex   = slotIndex;
        entry.trackName   = trackName;
        entry.domain      = "skip";
        entry.description = "Problema omitido por el usuario";
        entry.source      = MixHistoryEntry::Source::UserAction;
        pushMixHistory(entry);

        LogHelper::writeToLog("[AccionDirecta] skipProblem slot=" + juce::String(slotIndex)
                              + " track=\"" + trackName + "\"");

        // 2. Responder como coach validando la decisión
        juce::String msg = personalize("Entendido, no hay problema. Podemos retomarlo más adelante si quieres.");
        respondWithPremium(msg, MentorMessage::Type::Info);
    }

    void CoachEngine::explainProblem(int slotIndex, const juce::String& problemType, const juce::String& trackName)
    {
        if (slotIndex < 0 || problemType.isEmpty()) return;

        LogHelper::writeToLog("[AccionDirecta] explainProblem slot=" + juce::String(slotIndex)
                              + " type=\"" + problemType + "\" track=\"" + trackName + "\"");

        // Construir explicación técnica según el tipo de problema
        juce::String explanation;
        juce::String trackPrefix = trackName.isNotEmpty() ? "En **" + trackName + "**, " : "";

        if (problemType.containsIgnoreCase("clipping") || problemType.containsIgnoreCase("peak")) {
            explanation = trackPrefix + "el **clipping** ocurre cuando la señal supera el límite máximo "
                          "que el sistema puede manejar (0 dBFS). Esto produce distorsión digital "
                          "que suena como un 'crujido' desagradable, especialmente en los transitorios.\n\n"
                          "**Soluciones:**\n"
                          "  • Reduce el gain del canal\n"
                          "  • Usa un limitador suave en el master\n"
                          "  • Revisa los plugins que puedan estar saturando";
        } else if (problemType.containsIgnoreCase("masking") || problemType.containsIgnoreCase("enmascar")) {
            explanation = trackPrefix + "el **enmascaramiento** pasa cuando dos pistas compiten por la misma "
                          "frecuencia. Por ejemplo, si Kick y Bass tienen mucha energía en 60Hz, "
                          "no se distinguen y la mezcla suena embarrada.\n\n"
                          "**Soluciones:**\n"
                          "  • Corta frecuencias bajas de una de las dos pistas\n"
                          "  • Usa Sidechain EQ para hacer espacio\n"
                          "  • Prueba la técnica de 'Complementary EQ'";
        } else if (problemType.containsIgnoreCase("bass") || problemType.containsIgnoreCase("sub")) {
            explanation = trackPrefix + "cuando hablo de **demasiado subgrave**, me refiero a que las frecuencias "
                          "entre 20-60Hz están sobreelevadas. En la mayoría de sistemas "
                          "de escucha esto se traduce en una mezcla que suena 'retumbante' o imprecisa.\n\n"
                          "**Soluciones:**\n"
                          "  • Aplica un High-Pass Filter alrededor de 30-40Hz\n"
                          "  • Reduce 1-2dB en 50Hz con un EQ\n"
                          "  • Usa un analizador espectral para visualizarlo";
        } else if (problemType.containsIgnoreCase("gain") || problemType.containsIgnoreCase("nivel")) {
            explanation = trackPrefix + "el **nivel** de la pista está fuera del rango recomendado. "
                          "Un nivel adecuado es crucial para tener headroom suficiente "
                          "y evitar que el master se sature al sumar todas las pistas.\n\n"
                          "**Rango recomendado:**\n"
                          "  • Picos entre -18 dBFS y -10 dBFS\n"
                          "  • RMS alrededor de -24 dBFS a -18 dBFS\n"
                          "  • Deja al menos 6dB de headroom en el master";
        } else {
            explanation = trackPrefix + "este problema se refiere a: **" + problemType + "**.\n\n"
                          "En términos técnicos, significa que el análisis detectó "
                          "una desviación significativa respecto al perfil de referencia "
                          "o al rango recomendado para este tipo de pista.\n\n"
                          "Te sugiero que revises la sección de herramientas (Analyzers) "
                          "para ver la evidencia visual mientras ajustas.";
        }

        respondWithPremium(personalize(explanation), MentorMessage::Type::Info);
    }

    void CoachEngine::respondWithCorrectionFeedback(const juce::String& text)
    {
        MentorMessage msg;
        msg.type      = MentorMessage::Type::Info;
        msg.text      = personalize(text).toStdString();
        msg.timestamp = juce::Time::getMillisecondCounter() * 1000;
        msg.context   = "Correction";
        sharedData_.pushMessage(msg);
        LogHelper::writeToLog("[CoachEngine] CorrectionFeedback: " + text.substring(0, 80));
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  PERSONALIZE — Inserta el nombre del usuario cada ~5 mensajes
    // ═══════════════════════════════════════════════════════════════════════════

    juce::String CoachEngine::personalize(const juce::String& text)
    {
        if (!hasEngineerName()) return text;

        messageCountSinceLastNameUse_++;

        // Cada ~5 mensajes, insertar el nombre
        if (messageCountSinceLastNameUse_ < 5) return text;

        messageCountSinceLastNameUse_ = 0;

        juce::String result = text;

        // Si el texto tiene {name}, reemplazarlo
        if (result.contains("{name}")) {
            result = result.replace("{name}", engineerName_, false);
            return result;
        }

        // Si no tiene marcador, insertar el nombre al inicio
        // Pero solo si el texto no es muy corto (< 20 chars)
        if (result.length() >= 20) {
            result = engineerName_ + ", " + result.substring(0, 1).toLowerCase() + result.substring(1);
        }

        return result;
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

            msg += "[NOTES] Ahora ajustemos niveles para tener headroom saludable.";
            respondWith(msg, MentorMessage::Type::Achievement);

            // Send phase guidance for the new phase
            sendPhaseGuidance(newPhase);

            LogHelper::writeToLog("[Sprint2] Fase avanzada: Organizacion -> "
                                  + juce::String(phaseNames[static_cast<int>(newPhase)]));
        }
        else {
            // Phase not complete yet \u2014 acknowledge the map confirmation
            respondWith("[DONE] **Mapa de mezcla confirmado.** " + juce::String(bussedCount) + "/"
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


    // ═══════════════════════════════════════════════════════════════════════════
    //  REFERENCE-DRIVEN COACHING V2: computeBlendAlpha
    //  α = sigmoid(matchScore, k=8.0, midpoint=0.5)
    //  Determina cuánto peso darle a la referencia vs al perfil de género.
    //  • α ≈ 0.02 (match bajo) → confiar en perfil de género
    //  • α ≈ 0.50 (match=0.5) → 50/50 blend
    //  • α ≈ 0.98 (match alto) → confiar en la referencia
    // ═══════════════════════════════════════════════════════════════════════════

    float CoachEngine::computeBlendAlpha() const noexcept
    {
        // Si no hay referencia de audio, α = 0 (confiar 100% en perfil de género)
        if (!referenceFingerprint_.valid)
            return 0.0f;

        // Usar spectralSimilarity de la última comparación
        float matchScore = lastReferenceComparison_.spectralSimilarity;

        // Si no hay comparación previa, usar 0.3 como default conservador
        if (matchScore <= 0.0f)
            matchScore = 0.3f;

        // α = sigmoid(matchScore, k=8.0, midpoint=0.5)
        // k=8.0 da transición suave pero clara:
        //   match 0.3 → α ≈ 0.17 (mayormente género)
        //   match 0.5 → α ≈ 0.50 (mitad y mitad)
        //   match 0.7 → α ≈ 0.83 (mayormente referencia)
        return sigmoid(matchScore, 8.0f, 0.5f);
    }

// ═══════════════════════════════════════════════════════════════════════════
//  MIX HISTORY — Buffer circular unificado de 50 eventos con delta detection
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::pushMixHistory(const MixHistoryEntry& entry) noexcept
{
    mixHistory_[mixHistoryWriteIndex_] = entry;
    mixHistoryWriteIndex_ = (mixHistoryWriteIndex_ + 1) % kMaxMixHistory;
    if (mixHistoryCount_ < kMaxMixHistory)
        mixHistoryCount_++;
}

std::vector<CoachEngine::MixHistoryEntry> CoachEngine::getMixHistory(int maxCount) const noexcept
{
    std::vector<MixHistoryEntry> result;
    result.reserve(std::min(maxCount, mixHistoryCount_));

    int entriesToTake = std::min(maxCount, mixHistoryCount_);
    int startIdx = (mixHistoryWriteIndex_ - entriesToTake + kMaxMixHistory) % kMaxMixHistory;

    for (int i = 0; i < entriesToTake; ++i) {
        int idx = (startIdx + i) % kMaxMixHistory;
        result.push_back(mixHistory_[idx]);
    }

    // Reverse to get most recent first
    std::reverse(result.begin(), result.end());
    return result;
}

juce::String CoachEngine::MixHistoryEntry::toShortSummary() const
{
    const char* srcLabel = "?";
    switch (source) {
        case Source::UserAction:     srcLabel = "usuario"; break;
        case Source::Correction:     srcLabel = "correccion"; break;
        case Source::WorkflowDetect: srcLabel = "workflow"; break;
        case Source::ReferenceGap:   srcLabel = "referencia"; break;
        case Source::System:         srcLabel = "sistema"; break;
    }

    juce::String s;
    s += juce::String("[") + juce::String(srcLabel) + "] ";
    if (trackName.isNotEmpty())
        s += trackName + ": ";
    s += description;
    if (hasDelta()) {
        s += juce::String(" (");
        if (delta > 0) s += "+";
        s += juce::String(delta, 1) + ")";
    }
    return s;
}

juce::String CoachEngine::buildMixHistoryText(int maxEntries) const
{
    auto entries = getMixHistory(maxEntries);
    if (entries.empty())
        return "[MIX HISTORY]\n  (Sin historial de cambios)\n\n";

    juce::String s;
    s += "[MIX HISTORY]\n";

    // ─── Delta-aggregation: agrupar por (trackName + domain) ────────────
    // Map: "trackName:domain" → { count, totalDelta, lastDescription }
    struct DeltaGroup {
        int count = 0;
        float totalDelta = 0.0f;
        juce::String lastDescription;
    };
    std::map<juce::String, DeltaGroup> groups;

    for (const auto& e : entries) {
        juce::String key = e.trackName + ":" + e.domain;
        groups[key].count++;
        groups[key].totalDelta += e.delta;
        groups[key].lastDescription = e.description;
    }

    // ─── Mostrar resumen delta-aggregated ───────────────────────────────
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        const juce::String& key = it->first;
        const DeltaGroup& grp   = it->second;

        // Split key back into trackName:domain
        int colonPos = key.indexOf(":");
        juce::String track  = key.substring(0, colonPos);
        juce::String domain = key.substring(colonPos + 1);

        juce::String line;
        line += "  \u2022 ";
        line += (track.isNotEmpty() ? track : juce::String("(global)"));
        line += " [" + domain + "]";

        if (grp.count > 0) {
            line += ": " + juce::String(grp.count) + "x";
            if (std::abs(grp.totalDelta) > 0.01f) {
                line += " (net ";
                if (grp.totalDelta > 0) line += "+";
                line += juce::String(grp.totalDelta, 1) + ")";
            }
        }
        line += "\n";

        // Última descripción como detalle
        line += "         \u2192 ";
        line += grp.lastDescription;
        line += "\n";

        s += line;
    }

    s += "\n";
    return s;
}

void CoachEngine::clearMixHistory() noexcept
{
    for (auto& entry : mixHistory_)
        entry = MixHistoryEntry{};
    mixHistoryCount_ = 0;
    mixHistoryWriteIndex_ = 0;
}

// ============================================================
//  requestDiagnosticUpdate — Fires on-demand diagnostic update
//
//  Dispara el callback de actualizacion de diagnostico
//  inmediatamente, para que el DiagnosticBridge se actualice
//  con datos frescos (pushDiagnosticBridge en PluginEditor).
//  Util cuando el Coach (LLM) pide ver evidencia visual.
// ============================================================
void CoachEngine::requestDiagnosticUpdate()
{
    if (diagnosticUpdateCb_) {
        diagnosticUpdateCb_();
        LogHelper::writeToLog("[CoachEngine] On-demand diagnostic update fired");
    }
}



const DifferenceProfile& CoachEngine::getCachedDifferenceProfile() const noexcept
{
    return differenceProfile_;
}

RefinementProfile CoachEngine::getCachedRefinementProfile() const noexcept
{
    // Stub: differenceProfile_ has no refinementCache field yet
    juce::ignoreUnused(differenceProfile_);
    return RefinementProfile{};
}

ReferenceSummary CoachEngine::getReferenceSummary() const noexcept
{
    if (!referenceMetadata_.valid())
        return ReferenceSummary{};
    return ReferenceSummary::compute(referenceFingerprint_);
}

} // namespace mixcoach
