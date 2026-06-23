#include "PluginEditor.h"
#include "../../Common/types/LogHelper.h"

extern void earlyCrashLog(const char* point, const char* msg);

namespace mixcoach {

// Forward declarations from PluginEditor.cpp
void pushDiagnosticBridge(CoachEngine& coach, DiagnosticBridge& bridge);

// ═══════════════════════════════════════════════════════════════════════════
//  safeTimerLogic — Helper separado para evitar MSVC C2712 en timerCallback
// ═══════════════════════════════════════════════════════════════════════════
// timerCallback() tiene el __try/__except pero no puede tener objetos C++
// con destructores (lambdas, std::vector, etc.). Este helper tiene TODA
// la lógica del timer, SIN __try/__except.
///
// Se pasa por referencia para que safeTimerLogic pueda acceder a los
// miembros de MixCoachAudioProcessorEditor sin restricciones.
static void safeTimerLogic(MixCoachAudioProcessorEditor& editor,
                            juce::TabbedComponent* tabbedComp,
                            SharedData* sharedData,
                            MixCoachAudioProcessor& processor,
                            int timerTick,
                            std::atomic<bool>& bgHasNewResults,
                            std::atomic<int>& bgForceSyncResult,
                            std::atomic<int>& bgBackupResult,
                            std::atomic<bool>& s_initialFullSyncDone,
                            bool& initialSyncDone,
                            juce::CriticalSection& bgLock,
                            int headerActiveTab,
                            int& lastActiveSlotCount,
                            std::array<bool, SlotRegistry::kMaxSlots>& s_announcedSlots)
{
    juce::ignoreUnused(lastActiveSlotCount);
    juce::ignoreUnused(s_announcedSlots);

    // ═══ TRY/CATCH: Capturar excepciones C++ (std::bad_alloc, etc.) ═════
    // El __try/__except en timerCallback() captura SEH (Access Violations)
    // pero NO captura C++ exceptions. Este try/catch interno captura
    // ambas: C++ exceptions aquí, SEH en el __try/__except del caller.
    try
    {

    // ─── Step 2: init ligero (solo si UI no está construida aún) ─────────
    if (!editor.isFullUIBuilt() || tabbedComp == nullptr) {
        editor.initSharedData();
    }

    if (sharedData == nullptr || !tabbedComp) {
        return;
    }

    // ═══ Guard: shared memory mapping lost — bail out before touching SlotRegistry ═══
    // Si sharedData existe pero su mapeo de memoria compartida no está disponible,
    // cualquier acceso al SlotRegistry causaría un SEH (Access Violation).
    // Esto pasa cuando Messenger se descarga o FL Studio reinicia su sandbox.
    // La verificación temprana aquí evita el SEH antes de que safeTimerLogic
    // intente acceder a registry, slot, etc.
    if (!sharedData->isAvailable()) {
        return;
    }

    // ─── Bienvenida automática al iniciar sesión ────────────────────────
    {
        auto* coach = processor.getCoachEngine();
        if (coach != nullptr && coach->getSetupStep() == CoachEngine::SetupStep::NotStarted)
        {
            coach->startSetupDialogue();
        }
    }

    // ─── Step 3: Procesar resultados del background worker ────────────────
    if (bgHasNewResults.exchange(false))
    {
        int syncFound = bgForceSyncResult.exchange(0);
        int backupFound = bgBackupResult.exchange(0);

        if (syncFound > 0 || backupFound > 0)
        {
            if (!s_initialFullSyncDone.load())
            {
                s_initialFullSyncDone.store(true);
            }

            if (syncFound > 0 && !initialSyncDone)
                initialSyncDone = true;
            if (backupFound > 0 && !initialSyncDone)
                initialSyncDone = true;
        }

        if (syncFound > 0 || backupFound > 0)
        {
            if (tabbedComp && bgLock.tryEnter())
            {
                double sr = processor.getSampleRate();
                auto& registry = sharedData->getSlotRegistry();
                ((MainTabbedComponent*)tabbedComp)->updateAllPanels(registry, *sharedData, sr);
                bgLock.exit();
            }
            editor.detectNewMessengers();
            tabbedComp->repaint();
        }
    }

    // ─── Step 4: UI updates ──────────────────────────────────────────────
    if (tabbedComp && sharedData && sharedData->isAvailable())
    {
        auto& registry = sharedData->getSlotRegistry();
        auto* mainTabbed = (MainTabbedComponent*)tabbedComp;

        // ─── Fast update ────────────────────────────────────────────────
        mainTabbed->smoothMeters();
        mainTabbed->updateMasterMeters(processor.getAudioAnalyzer());

        if (headerActiveTab == 1)
            mainTabbed->smoothAnalyzersPanel(60.0);

        // ─── Poll + Refresh adaptativo ──────────────────────────────────
        {
            int activeForAdapt = registry.activeCount();
            const bool heavyTick = (activeForAdapt > 50)
                ? ((timerTick & 1) == 0)
                : true;

            if (bgLock.tryEnter())
            {
                if (heavyTick && headerActiveTab == 1)
                    mainTabbed->getAnalyzersPanel().fastUpdateMeters();

                if (heavyTick) {
                    mainTabbed->getCoachPanel().refreshMessengerTelemetry(registry, *sharedData);

                    // ═══ TrackFeed: actualizar mensajes por track desde CoachEngine ═══
                    auto* coach = processor.getCoachEngine();
                    if (coach != nullptr)
                        mainTabbed->getCoachPanel().updateCoachAdvice(*coach);
                }

                bgLock.exit();
            }
        }

        // ─── Slow update ────────────────────────────────────────────────
        bool didFullUpdate = false;
        if ((timerTick > 1 && timerTick <= 20) && bgLock.tryEnter())
        {
            double sr = processor.getSampleRate();
            mainTabbed->updateAllPanels(registry, *sharedData, sr);
            bgLock.exit();
            editor.detectNewMessengers();
            didFullUpdate = true;
        }

        if (!didFullUpdate)
        {
            int slowUpdateRate = (timerTick < 300) ? 4 : 16;
            if (timerTick % slowUpdateRate == 0 && bgLock.tryEnter())
            {
                double sr = processor.getSampleRate();
                mainTabbed->updateAllPanels(registry, *sharedData, sr);
                bgLock.exit();
                editor.detectNewMessengers();
            }
        }

        // ─── FAST LAYER: Track analysis cada ~60 ticks (~1s a 60fps) ─────
        if (timerTick % 60 == 0)
        {
            auto* coach = processor.getCoachEngine();
            if (coach != nullptr)
                coach->fastTrackAnalysis();
        }

        // ─── Reference position polling ─────────────────────────────────
        if (timerTick % 4 == 0)
        {
            auto& refPlayer = processor.getRefPlayer();
            if (refPlayer.isPlaying())
            {
                double pos = refPlayer.getPosition();
                double len = refPlayer.getLength();
                if (tabbedComp)
                {
                    auto& refPanel = mainTabbed->getCoachPanel().getRefPanel();
                    refPanel.updatePlaybackPosition(pos, len);
                }
            }
        }

        // ─── periodicAnalysis y secciones (cada 300 ticks) ──────────────
        if (timerTick % 300 == 0)
        {
            auto* phaseMgr = processor.getPhaseManager();
            auto* coach = processor.getCoachEngine();
            auto* adapter = processor.getAiCoachAdapter();
            if (phaseMgr != nullptr && coach != nullptr && mainTabbed)
            {
                auto& coachPanel = mainTabbed->getCoachPanel();

                MentorPhase phase = phaseMgr->getCurrentPhase();
                juce::String phaseName = getPhaseName(phase);

                juce::String genre = coach->getSetupGenre();
                if (genre.isEmpty()) genre = "SIN CONFIGURAR";

                float targetLufs = -14.0f;
                auto& profile = CoachEngine::getGenreProfile(genre);
                targetLufs = profile.targetIntegratedLUFS;

                double sr = processor.getSampleRate();
                juce::String srStr = juce::String(sr / 1000.0, 1) + " kHz";

                int expLevel = (adapter != nullptr)
                    ? static_cast<int>(adapter->getExperienceLevel())
                    : 1;

                coachPanel.updateFooterInfo(
                    phaseName,
                    genre,
                    juce::String((int)targetLufs) + " LUFS",
                    srStr,
                    expLevel);
            }
        }

        // ─── periodicAnalysis y secciones (cada 300 ticks) ──────────────
        if (timerTick % 300 == 0) {
            auto* coach = processor.getCoachEngine();
            if (coach != nullptr) {
                if (tabbedComp)
                {
                    auto* safePanel = &mainTabbed->getCoachPanel().getRefPanel();
                    coach->setMatchDataCallback([safePanel](const DifferenceProfile& data) {
                        if (safePanel != nullptr)
                            safePanel->updateMatchData(data);
                    });
                }

                // ═══ Mover analisis de referencia al background worker ═══
                if (coach->hasPendingReference()) {
                    juce::String refPath = coach->consumePendingReferencePath();
                    if (refPath.isNotEmpty()) {
                        editor.signalBgRefAnalysis(refPath);
                        LogHelper::writeToLog("[MixCoachEditor] Referencia delegada al background worker: " + refPath);
                    }
                }

                coach->periodicAnalysis();

                // ═══ Push Reference-Driven progress data to panel ═══════════
                {
                    auto& refPanel = mainTabbed->getCoachPanel().getRefPanel();
                    if (coach->isReferenceDrivenMode()) {
                        const auto& prog = coach->getReferenceProgress();
                        refPanel.setReferenceProgress(prog.currentMatch, prog.delta);
                    }
                }

                // ═══ Push diagnostics to DiagnosticBridge for visual overlay ═══
                pushDiagnosticBridge(*coach, processor.getDiagnosticBridge());

                // ═══ Push centroid info to spectrograph overlay ═══
                {
                    juce::String centGenre;
                    if (coach->hasReference())
                        centGenre = coach->getReferenceGenre();
                    if (centGenre.isEmpty())
                        centGenre = coach->getSetupGenre();

                    auto ceInfo = coach->getCentroidInfo(centGenre);
                    if (ceInfo.valid())
                    {
                        SpectrographComponent::CentroidInfo specInfo;
                        specInfo.actualHz   = ceInfo.actualHz;
                        specInfo.expectedHz = ceInfo.expectedHz;
                        specInfo.genre      = ceInfo.genre;
                        mainTabbed->getAnalyzersPanel().setCentroidInfo(specInfo);
                    }
                }

                // ═══ Dynamic suggestion chips (cada ~5s) ═══
                {
                    auto dynamicSugs = coach->getDynamicSuggestions();
                    if (!dynamicSugs.empty())
                        mainTabbed->getCoachPanel().setSuggestions(dynamicSugs);
                }

                if (timerTick % 600 == 0)
                    coach->checkAndSendProactiveTip();
                else
                    coach->generateProactiveTip();
            }

            auto* adapter = processor.getAiCoachAdapter();
            if (adapter != nullptr)
                adapter->autoSave();

            if (coach != nullptr && tabbedComp)
            {
                auto& refPanel = mainTabbed->getCoachPanel().getRefPanel();
                const auto& sections = coach->getReferenceSections();

                std::vector<ReferencePanelComponent::SectionInfo> sectionInfos;
                sectionInfos.reserve(sections.size());

                ReferencePanelComponent::SectionInfo fullInfo;
                fullInfo.label = "Full";
                fullInfo.startSeconds = 0.0f;
                fullInfo.endSeconds = 0.0f;
                sectionInfos.push_back(fullInfo);

                for (const auto& sec : sections)
                {
                    ReferencePanelComponent::SectionInfo info;
                    info.label = sec.label;
                    info.startSeconds = sec.startSeconds;
                    info.endSeconds = sec.endSeconds;
                    sectionInfos.push_back(info);
                }

                int activeIdx = coach->getActiveSection() + 1;
                if (!sections.empty() && coach->getActiveSection() < 0)
                    activeIdx = 0;

                refPanel.setSectionData(sectionInfos, activeIdx);
            }
        }
    }        // ─── Step 5: Log periódico ──────────────────────────────────────────
    if (timerTick % 300 == 0)
    {
        int activeSlots = 0;
        if (sharedData && bgLock.tryEnter()) {
            activeSlots = sharedData->getSlotRegistry().activeCount();
            bgLock.exit();
        }
    }

    } // try
    catch (const std::exception& e)
    {
        earlyCrashLog("TIMER_CPP", e.what());
        processor.getRefPlayer().sehSafeStop();
    }
    catch (...)
    {
        earlyCrashLog("TIMER_CPP", "unknown C++ exception");
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Timer callback — AHORA con __try/__except SEH protection
// ═══════════════════════════════════════════════════════════════════════════
void MixCoachAudioProcessorEditor::timerCallback()
{
    // ═══ Guardia antichoque (rápida, sin C++ objects) ═══════════════════
    if (editorBeingDestroyed_)
        return;

    // ═══ Advance header animations (simple float ops, safe before __try) ═══
    headerTabAnim_.advance(60.0);

    const int timerTick = ++timerTickCount_;

    // ─── Startup delay ──────────────────────────────────────────────────
    {
        constexpr int kStartupDelay = 1;
        if (timerTick <= kStartupDelay)
            return;
    }

    // ═══ __try/__except: captura SEH en el message thread ═══════════════
    __try
    {
        safeTimerLogic(
            *this,
            tabbedComponent_.get(),
            sharedData_,
            processorRef_,
            timerTick,
            bgHasNewResults_,
            bgForceSyncResult_,
            bgBackupResult_,
            s_initialFullSyncDone_,
            initialSyncDone_,
            bgLock_,
            headerActiveTab_,
            lastActiveSlotCount_,
            s_announcedSlots_
        );
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        // ═══ Contador SEH consecutivo: evitar flood de 200+ entradas en el log ═══
        // Si el SEH es persistente (ej: shared memory inválida), el timer seguiría
        // crasheando en cada tick (~16ms), llenando el log con 60 entradas/segundo.
        // Tras 10 SEH consecutivos, detenemos el timer definitivamente.
        // USAMOS SOLO C PURO (sprintf_s) para evitar C++ objects con destructor
        // dentro del __except handler, que causa C2712.
        static int s_consecutiveSEH = 0;
        s_consecutiveSEH++;

        constexpr int kMaxConsecutiveSEH = 10;
        if (s_consecutiveSEH > kMaxConsecutiveSEH)
        {
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
