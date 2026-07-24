#include "PluginEditor.h"
#include "../../Common/types/LogHelper.h"

extern void earlyCrashLog(const char* point, const char* msg);

namespace mixcoach {

    // Forward declarations from PluginEditor.cpp
    void pushDiagnosticBridge(CoachEngine& coach, DiagnosticBridge& bridge);

    // ═══════════════════════════════════════════════════════════════════════════
    //  safeTimerLogic — Helper separado para evitar MSVC C2712 en timerCallback
    // ═══════════════════════════════════════════════════════════════════════════
    // INCREMENTO 1: Ya no usa bgHasNewResults/bgLock/stale sync atoms.
    // El bg service (MixCoachBgService en PluginProcessor) maneja su propio
    // estado. El timer solo lee snapshots y actualiza la UI.
    static void safeTimerLogic(MixCoachAudioProcessorEditor& editor,
                               NavigationShell* navShell,
                               SharedData* sharedData,
                               MixCoachAudioProcessor& processor,
                               int timerTick)
    {
        // ═══ TRY/CATCH: Capturar excepciones C++ (std::bad_alloc, etc.) ═════
        try {
            // ─── Step 1: init ligero (solo si UI no está construida aún) ─────────
            if (!editor.isFullUIBuilt() || navShell == nullptr) {
                editor.initSharedData();
            }

            if (sharedData == nullptr || !navShell) {
                return;
            }

            // ═══ Guard: shared memory mapping lost ═══
            if (!sharedData->isAvailable()) {
                return;
            }

            // ─── Startup welcome ────────────────────────────────────────────
            {
                auto* coach = processor.getCoachEngine();
                if (coach != nullptr && coach->getSetupStep() == CoachEngine::SetupStep::NotStarted) {
                    coach->startSetupDialogue();
                }
            }

            // ─── UI updates ─────────────────────────────────────────────────
            if (navShell && sharedData && sharedData->isAvailable()) {
                // ─── Fast update (60fps) ────────────────────────────────────
                navShell->smoothMeters();
                navShell->updateMasterMeters(processor.getAudioAnalyzer());

                // ─── Slow update (updateAllPanels + detectNewMessengers) ────
                // INCREMENTO 1: Ya no usamos bgHasNewResults_ para trigger.
                // El timer simplemente actualiza la UI cada N ticks.
                bool didFullUpdate = false;

                // Initial burst: first 20 ticks update aggressively
                if (timerTick > 1 && timerTick <= 20) {
                    double sr = processor.getSampleRate();
                    sharedData->getSlotRegistry(); // just check availability
                    navShell->updateAllPanels(sharedData->getSlotRegistry(), *sharedData, sr);
                    editor.detectNewMessengers();
                    didFullUpdate = true;
                }

                if (!didFullUpdate) {
                    int slowUpdateRate = (timerTick < 300) ? 4 : 16;
                    if (timerTick % slowUpdateRate == 0) {
                        double sr = processor.getSampleRate();
                        navShell->updateAllPanels(sharedData->getSlotRegistry(), *sharedData, sr);
                        editor.detectNewMessengers();
                    }
                }

                // ─── FAST LAYER: Track analysis cada ~60 ticks (~1s a 60fps) ─────
                if (timerTick % 60 == 0) {
                    auto* coach = processor.getCoachEngine();
                    if (coach != nullptr) coach->fastTrackAnalysis();
                }

                // ─── Reference position polling ─────────────────────────────────
                if (timerTick % 4 == 0) {
                    auto& refPlayer = processor.getRefPlayer();
                    if (refPlayer.isPlaying()) {
                        double pos = refPlayer.getPosition();
                        double len = refPlayer.getLength();
                        if (navShell) {
                            auto& refPanel = navShell->getCoachPanel().getRefPanel();
                            refPanel.updatePlaybackPosition(pos, len);
                        }
                    }
                }

                // ─── P1: Leer transporte del DAW (cada ~16ms — 60fps, caché para no spamear) ──
                // La info de transporte se lee cada 60 ticks (~1s) para evitar overhead
                static MixCoachAudioProcessor::TransportInfo s_cachedTransport;
                if (timerTick % 60 == 0) {
                    s_cachedTransport = processor.readTransportInfo();
                }

                // ─── Slow periodic (cada 300 ticks ~5s) ────────────────────────
                if (timerTick % 300 == 0) {
                    auto* phaseMgr = processor.getPhaseManager();
                    auto* coach    = processor.getCoachEngine();
                    auto* adapter  = processor.getAiCoachAdapter();
                    if (phaseMgr != nullptr && coach != nullptr && navShell) {
                        auto& coachPanel = navShell->getCoachPanel();

                        MentorPhase phase      = phaseMgr->getCurrentPhase();
                        juce::String phaseName = getPhaseName(phase);

                        juce::String genre = coach->getSetupGenre();
                        if (genre.isEmpty()) genre = "SIN CONFIGURAR";

                        float targetLufs = -14.0f;
                        auto& profile    = CoachEngine::getGenreProfile(genre);
                        targetLufs       = profile.targetIntegratedLUFS;

                        double sr          = processor.getSampleRate();
                        juce::String srStr = juce::String(sr / 1000.0, 1) + " kHz";

                        int expLevel = (adapter != nullptr) ? static_cast<int>(adapter->getExperienceLevel()) : 1;

                        coachPanel.updateFooterInfo(
                            phaseName, genre, juce::String((int)targetLufs) + " LUFS", srStr, expLevel);
                    }

                    // ─── Reference analysis delegation to bg service ───────
                    if (coach != nullptr) {
                        if (navShell) {
                            auto* safePanel = &navShell->getCoachPanel().getRefPanel();
                            coach->setMatchDataCallback([safePanel](const DifferenceProfile& data) {
                                if (safePanel != nullptr) safePanel->updateMatchData(data);
                            });
                        }

                        if (coach->hasPendingReference()) {
                            juce::String refPath = coach->consumePendingReferencePath();
                            if (refPath.isNotEmpty()) {
                                editor.signalBgRefAnalysis(refPath);
                                LogHelper::writeToLog("[MixCoachEditor] Referencia delegada al bg service: "
                                                      + refPath);
                            }
                        }

                        coach->periodicAnalysis();

                        // ═══ FASE 3: TrackProblemCards + advice agrupado por familia ═══
                        // updateCoachAdvice() construye grupos de problemas por bus,
                        // los postea como TrackProblemCards en el chat, y actualiza
                        // los badges de issues en el MixMap.
                        navShell->getCoachPanel().updateCoachAdvice(*coach);

                        // ─── Push Reference-Driven progress data ────────────
                        {
                            auto& refPanel = navShell->getCoachPanel().getRefPanel();
                            if (coach->isReferenceDrivenMode()) {
                                const auto& prog = coach->getReferenceProgress();
                                refPanel.setReferenceProgress(prog.currentMatch, prog.delta);
                            }
                        }

                        // ─── Push diagnostics to DiagnosticBridge ────────────
                        pushDiagnosticBridge(*coach, processor.getDiagnosticBridge());

                        // ─── Push centroid info to spectrograph overlay ──────
                        {
                            juce::String centGenre;
                            if (coach->hasReference()) centGenre = coach->getReferenceGenre();
                            if (centGenre.isEmpty()) centGenre = coach->getSetupGenre();

                            auto ceInfo = coach->getCentroidInfo(centGenre);
                            if (ceInfo.valid()) {
                                SpectrographComponent::CentroidInfo specInfo;
                                specInfo.actualHz   = ceInfo.actualHz;
                                specInfo.expectedHz = ceInfo.expectedHz;
                                specInfo.genre      = ceInfo.genre;
                                navShell->getAnalyzersPanel().setCentroidInfo(specInfo);
                            }
                        }

                        // ─── Dynamic suggestion chips (cada ~5s) ────────────
                        {
                            auto dynamicSugs = coach->getDynamicSuggestions();
                            if (!dynamicSugs.empty()) navShell->getCoachPanel().setSuggestions(dynamicSugs);
                        }

                        if (timerTick % 600 == 0) coach->checkAndSendProactiveTip();
                        else
                            coach->generateProactiveTip();
                    }

                    if (adapter != nullptr) adapter->autoSave();

                    if (coach != nullptr && navShell) {
                        auto& refPanel       = navShell->getCoachPanel().getRefPanel();
                        const auto& sections = coach->getReferenceSections();

                        std::vector<ReferencePanelComponent::SectionInfo> sectionInfos;
                        sectionInfos.reserve(sections.size());

                        ReferencePanelComponent::SectionInfo fullInfo;
                        fullInfo.label        = "Full";
                        fullInfo.startSeconds = 0.0f;
                        fullInfo.endSeconds   = 0.0f;
                        sectionInfos.push_back(fullInfo);

                        for (const auto& sec : sections) {
                            ReferencePanelComponent::SectionInfo info;
                            info.label        = sec.label;
                            info.startSeconds = sec.startSeconds;
                            info.endSeconds   = sec.endSeconds;
                            sectionInfos.push_back(info);
                        }

                        int activeIdx = coach->getActiveSection() + 1;
                        if (!sections.empty() && coach->getActiveSection() < 0) activeIdx = 0;

                        refPanel.setSectionData(sectionInfos, activeIdx);
                    }
                }
            }

        } // try
        catch (const std::exception& e) {
            earlyCrashLog("TIMER_CPP", e.what());
            processor.getRefPlayer().sehSafeStop();
        }
        catch (...) {
            earlyCrashLog("TIMER_CPP", "unknown C++ exception");
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Timer callback — AHORA con __try/__except SEH protection
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachAudioProcessorEditor::timerCallback()
    {
        // ═══ Guardia antichoque (rápida, sin C++ objects) ═══════════════════
        if (editorBeingDestroyed_) return;

        // ═══ Advance header animations (simple float ops, safe before __try) ═══
        headerTabAnim_.advance(60.0);

        const int timerTick = ++timerTickCount_;

        // ─── Startup delay ──────────────────────────────────────────────────
        {
            constexpr int kStartupDelay = 1;
            if (timerTick <= kStartupDelay) return;
        }

        // ═══ __try/__except: captura SEH en el message thread ═══════════════
        __try {
            // INCREMENTO 1: safeTimerLogic simplificado — ya no recibe
            // bgHasNewResults_, bgLock_, initialSyncDone_, etc.
            // El bg service (MixCoachBgService en el processor) maneja
            // todo eso internamente.
            safeTimerLogic(*this,
                           tabbedComponent_.get(),
                           sharedData_,
                           processorRef_,
                           timerTick);

            // ═══ LLM Status Polling — detecta cambios de conectividad cada ~3s (60 ticks a 20Hz) ═══
            static int llmPollTick = 0;
            if (++llmPollTick >= 60) {
                llmPollTick = 0;
                auto* coach   = processorRef_.getCoachEngine();
                auto* adapter = processorRef_.getAiCoachAdapter();
                if (coach != nullptr && adapter != nullptr) {
                    bool llmAvailable = adapter->isLlmAvailable();
                    bool llmEnabled   = adapter->isLlmEnabled();
                    CoachEngine::LlmStatus newStatus;
                    if (llmAvailable)
                        newStatus = CoachEngine::LlmStatus::Connected;
                    else if (llmEnabled)
                        newStatus = CoachEngine::LlmStatus::Fallback;
                    else
                        newStatus = CoachEngine::LlmStatus::Offline;

                    // setLlmStatus solo dispara el callback si el estado cambió
                    coach->setLlmStatus(newStatus);
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            // ═══ Contador SEH consecutivo: evitar flood de 200+ entradas en el log ═══
            // Si el SEH es persistente (ej: shared memory inválida), el timer seguiría
            // crasheando en cada tick (~16ms), llenando el log con 60 entradas/segundo.
            // Tras 10 SEH consecutivos, detenemos el timer definitivamente.
            // USAMOS SOLO C PURO (sprintf_s) para evitar C++ objects con destructor
            // dentro del __except handler, que causa C2712.
            static int s_consecutiveSEH = 0;
            s_consecutiveSEH++;

            constexpr int kMaxConsecutiveSEH = 10;
            if (s_consecutiveSEH > kMaxConsecutiveSEH) {
                // Una sola linea de aviso; luego silencio total
                if (s_consecutiveSEH == kMaxConsecutiveSEH + 1)
                    earlyCrashLog("TIMER", "SUPRIMIENDO timer tras 10 SEH consecutivos");
                return;
            }

            // SEH en message thread — parar reproducción, log, seguir vivos
            char countBuf[64];
            sprintf_s(countBuf, sizeof(countBuf), "SEH capturado en timer callback (#%d)", s_consecutiveSEH);
            earlyCrashLog("TIMER", countBuf);
            processorRef_.getRefPlayer().sehSafeStop();
        }
    }

} // namespace mixcoach
