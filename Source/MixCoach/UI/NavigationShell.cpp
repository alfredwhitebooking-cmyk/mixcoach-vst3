#include "NavigationShell.h"
#include "FocusOverlay.h"
#include "MixMapComponent.h"
#include "GainStagingPanel.h"
#include "TrackProblemBuilder.h"
#include "SpacePanel.h"
#include "../engine/TrackGainAnalyzer.h"
#include "../engine/TrackDynamicsAnalyzer.h"
#include "EQPanel.h"
#include "../../Common/types/LogHelper.h"
#include "../core/PluginProcessor.h"
#include "../core/UrlDownloader.h"
#include "../engine/ReferenceProfile.h"
#include "../engine/CoachEngine.h"
#include "../engine/RefinementProfile.h"
#include "../engine/MixScore.h"
#include "../engine/GenreProfile.h"
#include "../engine/PluginScanner.h"
#include "../engine/PluginSuggestionsProvider.h"
#include "../engine/PanelRevealManager.h"
#include "../engine/ProblemAnalyzerMap.h"
#include "../engine/CoachingNarrativeDirector.h"
#include <algorithm>

namespace mixcoach {

// ═══ Compile-time guard contra desincronización con tests ═══════════════
// Los tests en TestNavigationAnimations.cpp duplican estos valores.
// Si cambian aquí, el static_assert falla en compilación.
static_assert(NavigationShell::SetupFadeAnim::kFrames == 18.0f);
static_assert(NavigationShell::SetupFadeAnim::kFadeInStart == 0.3f);

    // =======================================================================
    //  Constructor
    // =======================================================================
    NavigationShell::NavigationShell(MixCoachAudioProcessor& processor, SharedData& sharedData) :
        processorRef_(processor),
        sharedData_(sharedData),
        analyzersPanel_(std::make_unique<AnalyzersPanelComponent>(processor.getAudioAnalyzer())),
        progressScreen_(std::make_unique<ProgressScreen>()),
        reportPanel_(std::make_unique<EndOfSessionComponent>()),
        experienceManager_(*this, revealManager_),
        inMasterCheckDialog_{false}
    {
        coachPanel_ = std::make_unique<MixCoachPanel>();

        // ─── CoachingNarrativeDirector — después de coachPanel_ ═══════
        narrativeDirector_ = std::make_unique<CoachingNarrativeDirector>(*this, *coachPanel_, sharedData_);
        narrativeDirector_->onCycleComplete = [this]() {
            // Ciclo completado: disparar XP burst + confeti celebración
            // El system message "⚡ +45 XP · +12% progreso" ya se postea
            // desde CoachingNarrativeDirector::doCelebrateStep().
            // Aquí hacemos que la barra de progreso y el confeti hagan la animación.
            phaseProgressBar_.triggerXpBurst(45);
            backgroundEffects_.triggerConfetti(60);
            // Reproducir chime de celebración a través del plugin
            processorRef_.getCelebrationChime().trigger();
        };
        narrativeDirector_->onTierSelected = [this](int tierIndex, const PluginSuggestionGroup& option) {
            // El usuario seleccionó un tier de plugin
            if (!option.suggestions.empty()) {
                coachPanel_->addSystemMessage(
                    juce::String("[PLUGIN] Aplicando: ") + option.suggestions[0].pluginName);
            }
        };

        // ═══ FASE 5: Option cards clicables → verify loop directo ═══════
        // Cuando el usuario hace clic en una tarjeta de plugin inline,
        // dispara director.onOptionSelected() directamente sin round-trip al LLM.
        coachPanel_->onPluginCardClicked = [this](const juce::String& pluginName) {
            if (pluginName.isEmpty()) return;
            if (narrativeDirector_ && narrativeDirector_->isActive()) {
                narrativeDirector_->onOptionSelected(pluginName);
            }
        };
        narrativeDirector_->onStartVerification = [this](const CorrectionCardData& data) {
            // Iniciar verificación del cambio
            if (processorRef_.getCoachEngine()) {
                processorRef_.getCoachEngine()->requestDiagnosticUpdate();
            }
        };

        // ─── WelcomeComponent (STATE 0) ────────────────────────────────────
        welcomeComponent_ = std::make_unique<WelcomeComponent>();
        welcomeComponent_->onStart = [this](const juce::String& rawUserName) {
            // Sanitizar nombre de usuario: trim + length limit + character whitelist
            juce::String userName = rawUserName.trim();
            if (userName.length() > 40) userName = userName.substring(0, 40).trim();
            if (userName.isEmpty())   userName = "Ingeniero";
            // Eliminar caracteres potencialmente problemáticos
            userName = userName.retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz \xC3\xA1\xC3\xA9\xC3\xAD\xC3\xB3\xC3\xBA\xC3\xBC\xC3\xB1");
            if (userName.isEmpty())   userName = "Ingeniero";
            // ═══ P1→P2: Crossfade (150ms) entre WelcomeComponent y CoachPanel ═══
            // ═══ V4: Persistir nombre de usuario inmediatamente ═════════════════
            processorRef_.setSavedUserName(userName);

            // ═══ EventLog: nombre ingresado ═════════════════════════════════════
            if (auto* coach = processorRef_.getCoachEngine()) {
                coach->getEventLog().logEvent(SessionEvent::Type::SetupNameEntered,
                                              "Nombre: " + userName);
            }

            coachPanel_->setWelcomeMode(true);
            auto* coach = processorRef_.getCoachEngine();
            if (coach != nullptr) {
                coach->setEngineerName(userName);
                if (coach->getSetupStep() == CoachEngine::SetupStep::WaitingForName
                    || coach->getSetupStep() == CoachEngine::SetupStep::NotStarted) {
                    coach->detectAndSetEngineerName(userName);
                }
            }
            // ═══ CRITICO: Establecer estados alpha/visibility ANTES de cualquier cambio de escena ═══
            // Esto evita que resized() oculte welcomeComponent_ prematuramente
            welcomeComponent_->setAlpha(1.0f);
            welcomeComponent_->setVisible(true);
            coachPanel_->setAlpha(0.0f);
            coachPanel_->setVisible(true);
            resized();           // Layout correcto ANTES del crossfade
            repaint();

            // ═══ SceneManager: avanzar a ModeSelection ═══════════════════
            // processDirectorEvent llama a sceneManager_.processEvent() + applyScene()
            // applyScene() llama a setCoachRoomState(Intention) + postea coachMessage
            updateTabLockState();
            processDirectorEvent({DirectorEvent::Type::UserNameEntered});

            // ═══ Crossfade NOW: sobre layout ya estable ═══════════════════
            startCrossfade(welcomeComponent_.get(), coachPanel_.get());
            coachPanel_->setSuggestions({});
            coachPanel_->setShowSuggestionsOverride(false);
            // ═══ Auto-advance: robot saluda + delay 400ms antes de mostrar chips ═══
            // ═══ CHAT-CONTROLLED UX: modo como chips en QuickReplyBar, no tarjetas ═══
            setAvatarExpression(AvatarExpression::Happy);
            setAvatarWave(true);
            setAvatarNod(400);
            callAfterDelaySafe(400, [this]() {
                // ═══ Mode cards en vez de chips: dos tarjetas grandes centradas ═══
                coachPanel_->setShowModeCards(true);

                // ═══ Walkthrough (4.3): si es primera sesión, mostrar tutorial ═══
                callAfterDelaySafe(400, [this]() {
                    // BUG FIX: WalkthroughOverlay hace toFront(true) y setInterceptsMouseClicks(true,false)
                    // interceptando todos los clics a las mode cards. Solo mostrar walkthrough
                    // DESPUÉS de que el usuario seleccione un modo (no durante mode selection).
                    // El walkthrough se dispara desde onSuggestionClicked después de ModeSelected.
                    // auto* adapter = processorRef_.getAiCoachAdapter();
                    // if (adapter != nullptr && !adapter->getUserProfile().walkthroughCompleted) {
                    //     showWalkthrough();
                    // }
                });
            });
        };

        // ═══ Gap #3: Robot asiente con cada tarea del checklist ═══════════
        // Este callback se setea aquí, antes de que se muestre el checklist.
        // coachPanel_->getSessionPrepChecklist() está disponible porque
        // el SessionPrepChecklist es un miembro de CoachChatComponent creado
        // en su constructor. El callback se dispara desde SessionPrepChecklist::refresh()
        // cuando detecta una transición unchecked→checked.
        coachPanel_->getSessionPrepChecklist().onItemChecked = [this](int itemIndex) {
            juce::ignoreUnused(itemIndex);
            setAvatarNod(350);
        };

        // --- SessionPrep -> MixMapStage ---
        coachPanel_->onSessionPrepConfirmed = [this]() {
            if (getCoachRoomState() != CoachRoomState::SessionPrep)
                return;

            // EventLog
            if (auto* coachEv = processorRef_.getCoachEngine())
                coachEv->getEventLog().logEvent(SessionEvent::Type::SessionPrepped);

            // Robot asiente al completar checklist
            setAvatarExpression(AvatarExpression::Happy);
            setAvatarNod(600);

            auto* coach = processorRef_.getCoachEngine();
            if (coach == nullptr) return;

            // Helper: ir a MixMapStage directamente
            auto goToMixMap = [this]() {
                coachPanel_->setShowSessionPrepCard(false);
                // ═══ SceneManager: SessionPrepped → MixMapReview ════════════
                processDirectorEvent({DirectorEvent::Type::SessionPrepped});
                postUIEvent("\xf0\x9f\x97\xba", "Generando mapa de sesi\xc3\xb3n...");
                revealPanel(PanelId::MixMap);
                setAvatarPoint();
                coachPanel_->showPanelMixMap();
                startSetupTransition();
                coachPanel_->addSystemMessage(
                    "\xe2\x9c\x85 **Sesi\xc3\xb3n organizada!**\n\n"
                    "He mapeado todas tus pistas en el panel MIX MAP. "
                    "Revisa que el ruteo sea correcto y conf\xc3\xadrmalo para empezar a mezclar.");
                coachPanel_->setSuggestions({"Confirmar mapa"});
                coachPanel_->setShowSuggestionsOverride(true);
            };

            // Escanear pistas con rol Unknown
            std::vector<int> unknownSlots;
            auto& roles = coach->getTrackRoles();
            auto& registry = sharedData_.getSlotRegistry();

            for (int i = 0; i < SlotRegistry::kMaxSlots; ++i) {
                if (roles[i] == TrackRole::Unknown) {
                    auto info = registry.getSlotInfo(i);
                    if (info.active && juce::String(info.trackName).isNotEmpty()) {
                        unknownSlots.push_back(i);
                    }
                }
            }

            // Si no hay unknowns, ir directamente al MixMap con auto-advance
            if (unknownSlots.empty()) {
                // ═══ Auto-advance: delay 400ms antes de mostrar MixMap + fade-in ═══
                callAfterDelaySafe(400, [goToMixMap]() {
                    goToMixMap();
                    // startSetupTransition() se llama desde goToMixMap cuando el MixMap está visible
                });
                return;
            }

            // Preguntas interactivas para cada pista desconocida
            coachPanel_->setShowSessionPrepCard(false);

            static const std::vector<juce::String> kRoleOptions = {
                "\xf0\x9f\xa5\x81 Kick", "\xf0\x9f\xa5\x81 Snare",
                "\xf0\x9f\xa5\x81 HiHat", "\xf0\x9f\x94\x8a Bass",
                "\xf0\x9f\x8e\xa4 Voz",   "\xf0\x9f\x8e\xb8 Guitarra",
                "\xf0\x9f\x94\xb9 Synth",  "\xe2\x9e\xa1 Saltar"
            };

            auto roleFromString = [](const juce::String& s) -> TrackRole {
                juce::String lower = s.trim().toLowerCase();
                int sp = lower.indexOfChar(' ');
                if (sp > 0) lower = lower.substring(sp + 1).trim();

                if (lower == "kick")        return TrackRole::Kick;
                if (lower == "snare")       return TrackRole::Snare;
                if (lower == "hihat")       return TrackRole::HiHat;
                if (lower == "bass" || lower == "bajo")  return TrackRole::BassSub;
                if (lower == "voz" || lower == "vocal")  return TrackRole::VozPrincipal;
                if (lower == "guitarra" || lower == "guitar") return TrackRole::GuitarAcoustic;
                if (lower == "synth" || lower == "synthpad") return TrackRole::SynthPad;
                return TrackRole::Unknown;
            };

            struct AskState {
                std::vector<int> slots;
                int currentIdx = 0;
                std::function<void(const juce::String&)> prevCallback;
            };
            auto state = std::make_shared<AskState>();
            state->slots = unknownSlots;
            state->prevCallback = coachPanel_->getQuickReplyBar().onReplySelected;

            postUIEvent("\xf0\x9f\x94\x8d",
                        "Revisando identificaci\xc3\xb3n de pistas...");

            auto askNext = std::make_shared<std::function<void(const juce::String&)>>();
            *askNext = [this, coach, state, roleFromString, askNext, goToMixMap](const juce::String& reply) {
                // Aplicar rol seleccionado (excepto en llamada inicial con reply vacio)
                if (reply.isNotEmpty() && !reply.contains("Saltar")) {
                    TrackRole role = roleFromString(reply);
                    if (role != TrackRole::Unknown) {
                        int slotIdx = state->slots[state->currentIdx];
                        auto info = sharedData_.getSlotRegistry().getSlotInfo(slotIdx);
                        coach->setTrackRole(slotIdx, role);
                        coachPanel_->addSystemMessage(
                            "\xe2\x9c\x85 **" + juce::String(info.trackName)
                            + "** marcada como **" + reply + "**.");
                    }
                    state->currentIdx++;
                }

                // Si ya no quedan, ir al MixMap
                if (state->currentIdx >= (int)state->slots.size()) {
                    coachPanel_->getQuickReplyBar().onReplySelected = state->prevCallback;
                    coachPanel_->hideQuickReplies();
                    coachPanel_->addSystemMessage(
                        "🔍 **Identificación completada.**"
                        " Generando mapa de sesión...");
                    goToMixMap();
                    return;
                }

                // Preguntar por la siguiente pista no identificada
                int nextSlot = state->slots[state->currentIdx];
                auto nextInfo = sharedData_.getSlotRegistry().getSlotInfo(nextSlot);
                juce::String trackName(nextInfo.trackName);
                if (trackName.isEmpty())
                    trackName = "Pista " + juce::String(nextSlot + 1);

                coachPanel_->addSystemMessage(
                    "\xf0\x9f\xa4\x94 No identifico la pista **" + trackName + "**.\n\n"
                    "\xc2\xbfQu\xc3\xa9 instrumento es?");
                coachPanel_->showQuickReplies(kRoleOptions);
            };

            // Iniciar preguntas sobre la primera pista desconocida
            coachPanel_->getQuickReplyBar().onReplySelected = *askNext;
            (*askNext)("");
        };

        // ─── Toast label para notificaciones temporales ────────────────
        toastLabel_.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
        toastLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
        toastLabel_.setColour(juce::Label::backgroundColourId,
                              MixCoachTheme::bgPanel().withAlpha(0.85f));
        toastLabel_.setColour(juce::Label::outlineColourId,
                              MixCoachTheme::border().withAlpha(0.4f));
        toastLabel_.setJustificationType(juce::Justification::centred);
        toastLabel_.setAlpha(0.0f);
        // ═══ Back button — invisible hasta que se necesita en setup ──────
        backButtonLabel_.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
        backButtonLabel_.setText("←  Atr\xC3\xA1s", juce::dontSendNotification);
        backButtonLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted().withAlpha(0.6f));
        backButtonLabel_.setJustificationType(juce::Justification::centredLeft);
        backButtonLabel_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        backButtonLabel_.setVisible(false);
        backButtonLabel_.addMouseListener(this, false);
        addChildComponent(backButtonLabel_);

        // ─── Background effects (primero, detrás de todo) ────────────────
        addChildComponent(backgroundEffects_);
        backgroundEffects_.setVisible(true);

        addChildComponent(toastLabel_);

        addChildComponent(welcomeComponent_.get());

        // --- Sidebar navigation callback ---
        tabBar_.onTabSelected = [this](TabBarComponent::Tab tab) {
            cancelAutoReturn();
            if (tab == TabBarComponent::Coach) {
                autoReturnPermanentlyCancelled_ = false;
                autoReturnRestartCount_ = 0;
                LogHelper::writeToLog("[NavigationShell] Auto-return: reset permanente (usuario volvi\xC3\xB3 al Coach)");
            }
            switchContent(tab);
        };

        // --- TabBar starts INVISIBLE ---
        addChildComponent(tabBar_);
        focusOverlay_.onDismiss = [this]() { clearFocus(); };
        addChildComponent(focusOverlay_);
        addChildComponent(walkthroughOverlay_);

        // ─── Walkthrough callbacks ────────────────────────────────────────
        walkthroughOverlay_.onComplete = [this]() {
            // Marcar walkthrough como completado en UserProfile + persistir
            auto* adapter = processorRef_.getAiCoachAdapter();
            if (adapter != nullptr) {
                adapter->setWalkthroughCompleted(true);
                auto file = AiCoachAdapter::getDefaultProfileFile();
                adapter->saveUserProfile(file);
            }
            // Reanudar animaciones que se pausaron durante el walkthrough
            if (setupFadeAnim_.active) {
                startTimerHz(60);
            }
        };
        walkthroughOverlay_.onSkipped = [this]() {
            auto* adapter = processorRef_.getAiCoachAdapter();
            if (adapter != nullptr) {
                adapter->setWalkthroughCompleted(true);
                auto file = AiCoachAdapter::getDefaultProfileFile();
                adapter->saveUserProfile(file);
            }
            if (setupFadeAnim_.active) {
                startTimerHz(60);
            }
        };

        addChildComponent(coachPanel_.get());
        addChildComponent(analyzersPanel_.get());
        addChildComponent(progressScreen_.get());
        addChildComponent(reportPanel_.get());
        addChildComponent(phaseProgressBar_);

        // --- Wire reference panel callbacks ---
        auto cacheRefs = [this]() {
            auto& refPanel = coachPanel_->getRefPanel();
            processorRef_.cacheReferencePaths(refPanel.getFilePaths(), refPanel.getURLs());
        };

        coachPanel_->onReferenceFileAdded = [this, cacheRefs](const juce::String& path) {
            cacheRefs();
            // ═══ EventLog: referencia cargada ═════════════════════════════════
            if (auto* coach = processorRef_.getCoachEngine())
                coach->getEventLog().logEvent(SessionEvent::Type::ReferenceLoaded,
                                              "Ref: " + juce::File(path).getFileName());
            processDirectorEvent({DirectorEvent::Type::ReferenceLoaded});
            // ═══ NO ocultar card ni analizar automáticamente ══════════════════
            // La card ReferenceOnboardingCard permanece visible mostrando
            // el archivo cargado con el botón "Analizar referencia".
            // El usuario hace clic en "Analizar" → analyzeRefFunc se encarga
            // del análisis, progreso, ocultar card y avanzar flujo.
            if (getCoachRoomState() == CoachRoomState::ReferenceStage) {
                postUIEvent("\xF0\x9F\x93\x81", "Archivo cargado: " + juce::File(path).getFileName()
                            + "\nHaz clic en **Analizar referencia** para procesarlo.");
            }
        };

        coachPanel_->onReferenceURLAdded = [this](const juce::String& name, const juce::String& url) {
            // Guardar URL en el engine para referencia futura
            CoachEngine* eng = processorRef_.getCoachEngine();
            if (eng != nullptr) eng->setReferenceURL(name, url);
            // NO avanzar estado — esperar a que el usuario haga clic en "Analizar"
            // o que la descarga se complete. El avance se maneja en onStartReferenceAnalysis.
            postUIEvent("\xF0\x9F\x94\x97", "URL guardada: " + name
                        + "\nHaz clic en **Analizar referencia** para descargar y procesar.");
        };

        coachPanel_->onCheckReferenceCache = [this](const juce::String& path) -> bool {
            juce::File cacheDir =
                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                    .getChildFile("MixCoach_Logs/ReferenceCache");
            ReferenceProfileCache cache(cacheDir);
            return cache.has(ReferenceProfileCache::makeKey(path));
        };

        coachPanel_->onReferenceCleared = [this, cacheRefs]() {
            CoachEngine* eng = processorRef_.getCoachEngine();
            if (eng != nullptr) eng->clearReferences();
            cacheRefs();
        };

        // --- Play/Pause reference audio ---
        coachPanel_->onSeekReference = [this](double seconds) {
            processorRef_.getRefPlayer().setPosition(seconds);
        };

        coachPanel_->onPlayReference = [this](int refIndex) {
            auto& refs = coachPanel_->getRefPanel();
            if (refIndex >= 0 && refIndex < refs.getNumReferences()) {
                const auto& ref = refs.getReference(refIndex);
                if (ref.type == MixReference::Type::File) {
                    processorRef_.getRefPlayer().playFileAtPath(ref.path, refIndex);
                    coachPanel_->setRefPanelPlaybackState(refIndex, true);
                }
            }
        };

        // ─── Reference Onboarding Card → Analyze callback (async) ──────────
        // Reusable analyze function — used by both the component card AND the chat painted card
        auto analyzeRefFunc = [this, cacheRefs]() {
            CoachEngine* eng = processorRef_.getCoachEngine();
            auto& card = coachPanel_->getRefOnboardingCard();

            juce::String filePath = card.getFilePath();
            juce::String url = card.getURL();
            bool isURL = filePath.isEmpty() && url.isNotEmpty();
            if (eng == nullptr || (filePath.isEmpty() && !isURL)) return;

            // Mostrar la tarjeta de progreso
            coachPanel_->showReferenceAnalysisProgress(true);
            auto& progressCard = coachPanel_->getRefAnalysisProgressCard();

            // ═══ Gap #4: Mensajes secuenciales durante el análisis ═══════════
            progressCard.onStageChanged = [this](int stage) {
                const char* msg = "";
                switch (stage) {
                    case 0: msg = "Leyendo archivo de referencia..."; break;
                    case 1: msg = "Analizando espectro de frecuencias..."; break;
                    case 2: msg = "\xF0\x9F\x94\x8A Calculando LUFS y rango din\xC3\xA1mico..."; break;
                    case 3: msg = "Analisis completado"; break;
                    default: break;
                }
                if (msg[0] != '\0')
                    postUIEvent("·", juce::String(msg));
            };

            if (isURL) {
                // ═══ FLUJO URL: Descargar con yt-dlp, luego analizar ════════════
                if (!UrlDownloader::isYtDlpAvailable()) {
                    // yt-dlp no instalado: mostrar guía interactiva de instalación
                    coachPanel_->addSystemMessage(
                        "\xE2\x9D\x8C **yt-dlp no encontrado.**\n\n"
                        "Para descargar audio desde YouTube/Spotify necesito **yt-dlp**.\n\n"
                        "\xF0\x9F\x93\xA5 **Opci\xC3\xB3n 1: Descarga directa**\n"
                        "Descarga el ejecutable desde:\n"
                        "**https://github.com/yt-dlp/yt-dlp/releases**\n"
                        "Luego col\xC3\xB3" "calo en:\n"
                        "**Documentos/MixCoach/Tools/yt-dlp.exe**\n\n"
                        "**Opcion 2: Package manager**\n"
                        "Abre una terminal y ejecuta uno de estos comandos:\n"
                        "   \xE2\x80\xA2 **winget install yt-dlp** (Windows)\n"
                        "   \xE2\x80\xA2 **scoop install yt-dlp** (Scoop)\n"
                        "   \xE2\x80\xA2 **choco install yt-dlp** (Chocolatey)\n"
                        "   \xE2\x80\xA2 **pip install yt-dlp** (Python)\n"
                        "   \xE2\x80\xA2 **brew install yt-dlp** (macOS)\n\n"
                        "\xF0\x9F\x92\xA1 **Consejo:** Si ya instalaste yt-dlp, haz clic en **Reintentar** "
                        "para que lo detecte autom\xC3\xA1ticamente.\n"
                        "Mientras tanto, arrastra un archivo **WAV/MP3** directamente.");
                    coachPanel_->setSuggestions({"Reintentar", "Arrastrar WAV/MP3"});
                    coachPanel_->setShowSuggestionsOverride(true);
                    coachPanel_->showReferenceAnalysisProgress(false);
                    return;
                }

                // Crear UrlDownloader si no existe
                if (!urlDownloader_) {
                    urlDownloader_ = std::make_unique<UrlDownloader>();
                }

                // ─── Callback: metadata obtenida (título, artista) ──────────────
                urlDownloader_->onMetadataFetched = [this](const UrlDownloader::TrackMetadata& meta) {
                    juce::String display = meta.displayName();
                    postUIEvent("\xC2\xB7",
                                "Descargando: **" + display + "**"
                                + (meta.durationString.isNotEmpty()
                                    ? " (" + meta.durationString + ")"
                                    : ""));
                    // Mostrar el título en la tarjeta de progreso si está visible
                    if (coachPanel_->getRefAnalysisProgressCard().isVisible()) {
                        coachPanel_->getRefAnalysisProgressCard().setExternalProgress(0.05f);
                    }
                };

                // ─── Callback: progreso de descarga ───────────────────────────
                urlDownloader_->onProgress = [this, lastReportedPct = -1](float pct) mutable {
                    // Throttle: solo reportar cada 10% para no spamear el chat
                    int pctInt = juce::jlimit(0, 99, (int)(pct * 100.0f));
                    if (pctInt / 10 != lastReportedPct / 10) {
                        lastReportedPct = pctInt;
                        // Incluir metadata en el mensaje si está disponible
                        juce::String songInfo;
                        if (urlDownloader_->getMetadata().valid) {
                            songInfo = urlDownloader_->getMetadata().displayName() + ": ";
                        }
                        postUIEvent("\xC2\xB7",
                                    songInfo + juce::String(pctInt) + "% descargado");
                        // Actualizar tarjeta de progreso inline si está visible
                        if (coachPanel_->getRefAnalysisProgressCard().isVisible()) {
                            coachPanel_->getRefAnalysisProgressCard().setExternalProgress(pct);
                        }
                    }
                };

                urlDownloader_->onComplete = [this, cacheRefs](const juce::String& downloadedPath) {
                    if (downloadedPath.isEmpty()) {
                        // Error en descarga
                        coachPanel_->addSystemMessage(
                            "\xE2\x9D\x8C **Error al descargar la URL.**\n\n"
                            "Verifica que la URL sea v\xC3\xA1lida y que tengas conexi\xC3\xB3n "
                            "a internet. Puedes intentar de nuevo o arrastrar un archivo directamente.");
                        coachPanel_->showReferenceAnalysisProgress(false);
                        return;
                    }

                    // Descarga exitosa: ahora analizar como archivo
                    LogHelper::writeToLog("[NavigationShell] URL descargada a: " + downloadedPath);
                    {
                        // Mostrar nombre de la canción si hay metadata
                        juce::String songDisplay = urlDownloader_->getMetadata().displayName();
                        if (urlDownloader_->getMetadata().valid && songDisplay.isNotEmpty()) {
                            postUIEvent("OK",
                                        "**" + songDisplay + "** descargada. Analizando...");
                        } else {
                            postUIEvent("OK", "Descarga completada, analizando referencia...");
                        }
                    }
                    coachPanel_->getRefAnalysisProgressCard().startAnimation();

                    CoachEngine* eng = processorRef_.getCoachEngine();
                    if (eng == nullptr) {
                        coachPanel_->showReferenceAnalysisProgress(false);
                        return;
                    }

                    eng->setReferenceAudio(downloadedPath);

                    // Notificar al SceneManager que se cargó una referencia
                    processDirectorEvent({DirectorEvent::Type::ReferenceLoaded});

                    auto& refAnalyzer = eng->getReferenceAnalyzer();
                    float lufsI = refAnalyzer.getIntegratedLUFS();
                    float lra   = refAnalyzer.getLoudnessRange();
                    float tp    = refAnalyzer.getTruePeakDBTP();

                    auto& fp = eng->getReferenceFingerprint();
                    float crest   = fp.crestFactor;
                    float corr    = fp.correlation;
                    float centroid = fp.spectralCentroidHz;

                    coachPanel_->getRefAnalysisProgressCard().showSummary(
                        lufsI, lra, tp, crest, corr, centroid, fp.bandEnergies);
                    cacheRefs();

                    // Avanzar al siguiente estado
                    if (coachPanel_->onReferenceAnalysisComplete)
                        coachPanel_->onReferenceAnalysisComplete();

                    if (getCoachRoomState() == CoachRoomState::ReferenceStage) {
                        coachPanel_->setShowReferenceCards(false);
                        coachPanel_->setShowSuggestionsOverride(false);
                        coachPanel_->setSuggestions({});
                        // ═══ SceneManager: ReferenceAnalyzed → SetupComplete ═══
                        processDirectorEvent({DirectorEvent::Type::ReferenceAnalyzed});
                        setAvatarExpression(AvatarExpression::Happy);
                        setAvatarNod(400);
                        // ═══ Auto-advance: robot asiente + delay antes de mostrar SessionPrep + fade-in ═══
                        callAfterDelaySafe(400, [this]() {
                            auto* coach = processorRef_.getCoachEngine();
                            // ═══ Saludo del Coach: llenar nombre si disponible ═══
                            {
                                juce::String name = coach != nullptr ? coach->getEngineerName() : juce::String();
                                juce::String greeting = "¡Excelente " + (name.isNotEmpty() ? name : "ingeniero")
                                                        + "! Ya sé cómo quieres sonar.\nAhora organicemos la sesión.";
                                coachPanel_->setGreetingText(greeting);
                            }
                            coachPanel_->setShowSessionPrepCard(true, coach, &sharedData_.getSlotRegistry());
                            startSetupTransition();
                            // ═══ Gap 5: Escanear plugins del usuario después de la referencia ═══
                            scanUserPlugins();
                        });
                    }
                };

                // Iniciar descarga (defer 50ms para que la tarjeta de progreso
                // se renderice ANTES de que fetchMetadata() bloquee el UI thread)
                postUIEvent("\xC2\xB7", "Iniciando descarga desde URL...");
                callAfterDelaySafe(50, [this, url]() {
                    urlDownloader_->downloadURL(url);
                });

            } else {
                // ═══ FLUJO ARCHIVO: Usar animación interna 3s + deferir summary ═══
                // BUG A FIX: NO usar externalProgress para análisis síncrono.
                // setReferenceAudio() es blocking y nunca dispara onProgress.
                // En vez de eso, la animación interna auto-incrementa targetProgress_
                // de 0 a 1 en ~3s. Después del análisis síncrono, seteamos target a 1.0
                // y diferimos showSummary ~600ms para que la barra sea visible.
                CoachEngine* eng = processorRef_.getCoachEngine();
                if (eng == nullptr) {
                    coachPanel_->showReferenceAnalysisProgress(false);
                    return;
                }

                // Start the card's built-in timer (internal animation, ~3s countdown)
                // NOTA: NO llamamos setExternalProgress() aquí — queremos que
                // la animación interna auto-incremente targetProgress_ cada frame.
                progressCard.startAnimation();

                // ═══ Defer the blocking analysis 50ms so the progress card's timer
                // gets a chance to fire its first repaint BEFORE eng->setReferenceAudio
                // blocks the Message Thread (~100-500ms). Same pattern as the URL flow.
                callAfterDelaySafe(50, [this, eng, filePath, &progressCard, cacheRefs]() {
                    // Run the analysis NOW (blocks Message Thread ~100-500ms)
                    try {
                        LogHelper::writeToLog("[NavigationShell] Analizando referencia: " + filePath);
                        eng->setReferenceAudio(filePath);

                        auto& refAnalyzer = eng->getReferenceAnalyzer();
                        float lufsI = refAnalyzer.getIntegratedLUFS();
                        float lra   = refAnalyzer.getLoudnessRange();
                        float tp    = refAnalyzer.getTruePeakDBTP();

                        auto& fp = eng->getReferenceFingerprint();
                        float crest   = fp.crestFactor;
                        float corr    = fp.correlation;
                        float centroid = fp.spectralCentroidHz;

                        // ═══ BUG A FIX: Deferir showSummary ~600ms para que la
                        // animación interna de la barra de progreso sea visible.
                        // Sin este defer, la barra pasa de 0% a resumen instantáneamente
                        // porque el análisis síncrono bloqueó el timer.
                        // Con setExternalProgress(1.0f), el smooth value anima a 100%
                        // en ~200ms. Luego showSummary detiene el timer.
                        progressCard.setExternalProgress(1.0f);
                        cacheRefs();

                        // Defer showSummary to let progress bar animate
                        callAfterDelaySafe(600, [this, eng, &progressCard, lufsI, lra, tp, crest, corr, centroid, cacheRefs]() {
                            // Copy band energies into a local stack array for safe capture
                            auto& fp = eng->getReferenceFingerprint();
                            float bandEnergies[30];
                            for (int i = 0; i < 30; ++i)
                                bandEnergies[i] = fp.bandEnergies[i];

                            progressCard.showSummary(
                                lufsI, lra, tp, crest, corr, centroid, bandEnergies);

                            if (coachPanel_->onReferenceAnalysisComplete)
                                coachPanel_->onReferenceAnalysisComplete();

                            if (getCoachRoomState() == CoachRoomState::ReferenceStage) {
                                LogHelper::writeToLog("[NavigationShell] Referencia OK, avanzando a SessionPrep...");
                                coachPanel_->setInlineReferenceDropZone(false);
                                coachPanel_->setShowSuggestionsOverride(false);
                                coachPanel_->setSuggestions({});
                                processDirectorEvent({DirectorEvent::Type::ReferenceAnalyzed});
                                setAvatarExpression(AvatarExpression::Happy);
                                setAvatarNod(400);
                                // ═══ Auto-advance: robot asiente + delay antes de mostrar checklist ═══
                                callAfterDelaySafe(400, [this]() {
                                    auto* coach = processorRef_.getCoachEngine();
                                    // ═══ Saludo del Coach ═══
                                    {
                                        juce::String name = coach != nullptr ? coach->getEngineerName() : juce::String();
                                        juce::String greeting = "¡Excelente " + (name.isNotEmpty() ? name : "ingeniero")
                                                                + "! Ya sé cómo quieres sonar.\nAhora organicemos la sesión.";
                                        coachPanel_->setGreetingText(greeting);
                                    }
                                    coachPanel_->setShowSessionPrepCard(true, coach, &sharedData_.getSlotRegistry());
                                    // ═══ Gap 5: Escanear plugins del usuario ═══
                                    scanUserPlugins();
                                });
                            } else {
                                LogHelper::writeToLog("[NavigationShell] AVISO: Estado actual no es ReferenceStage ("
                                                      + juce::String(coachRoomStateLabel(coachRoomState_))
                                                      + ") — no se avanzó a SessionPrep");
                            }
                        });
                    }
                    catch (const std::exception& e) {
                        LogHelper::writeToLog("[NavigationShell] EXCEPCION en análisis de referencia: "
                                              + juce::String(e.what()));
                        coachPanel_->addSystemMessage(
                            "\xE2\x9D\x8C **Error al analizar la referencia.**\n\n"
                            "Ocurrió un error inesperado: " + juce::String(e.what()) + "\n\n"
                            "Puedes intentar de nuevo o arrastrar otro archivo.");
                        coachPanel_->showReferenceAnalysisProgress(false);
                    }
                    catch (...) {
                        LogHelper::writeToLog("[NavigationShell] EXCEPCION desconocida en análisis de referencia");
                        coachPanel_->addSystemMessage(
                            "\xE2\x9D\x8C **Error desconocido al analizar la referencia.**\n\n"
                            "Puedes intentar de nuevo o arrastrar otro archivo.");
                        coachPanel_->showReferenceAnalysisProgress(false);
                    }
                });
            }
        };

        // Assign the reusable analyze lambda to the component card
        coachPanel_->onStartReferenceAnalysis = analyzeRefFunc;

        coachPanel_->onReferenceAnalysisComplete = [this]() {
            LogHelper::writeToLog("[NavigationShell] Reference analysis complete");
        };

        // --- Reference selection callback ---
        coachPanel_->onReferenceSelected = [this](int refIndex) {
            CoachEngine* eng = processorRef_.getCoachEngine();
            if (eng == nullptr) return;
            auto& refs = coachPanel_->getRefPanel();
            if (refIndex >= 0 && refIndex < refs.getNumReferences()) {
                const auto& ref = refs.getReference(refIndex);
                if (ref.type == MixReference::Type::File)
                    eng->setReferenceAudio(ref.path);
            } else {
                refs.updateMatchData(DifferenceProfile{});
            }
        };

        // --- Ollama retry callback ---
        coachPanel_->onRetryOllama = [this]() { processorRef_.retryOllamaConnection(); };

        // ═══ Smooth scroll: asegurar timer activo durante la animación ═══
        coachPanel_->onSmoothScrollNeeded = [this]() {
            startTimerHz(60);
        };

        // --- MixMap confirmed → GainStaging ---
        coachPanel_->onMixMapConfirmed = [this]() {
            if (getCoachRoomState() == CoachRoomState::MixMapStage) {
                // ═══ EventLog: mix map confirmado → coaching iniciado ════════
                if (auto* coach = processorRef_.getCoachEngine()) {
                    coach->getEventLog().logEvent(SessionEvent::Type::MixMapConfirmed);
                    coach->getEventLog().logEvent(SessionEvent::Type::CoachingStarted);
                }
                // ═══ Auto-advance: robot celebra + delay 400ms antes de entrar a coaching ═══
                setAvatarExpression(AvatarExpression::Happy);
                setAvatarNod(600);
                callAfterDelaySafe(400, [this]() {
                    setCoachRoomState(CoachRoomState::GainStaging);
                    processDirectorEvent({DirectorEvent::Type::MixMapConfirmed});
                });
            } else {
                processDirectorEvent({DirectorEvent::Type::MixMapConfirmed});
            }
        };

        // ═══ P7: CoachingGuideWidget callbacks (Fase 8: simplificados via Director) ═══
        // El mapeo MentorPhase→CoachingStage ahora está centralizado en
        // CoachingNarrativeDirector::updateCoachingGuide(), eliminando la
        // duplicación de lógica que existía aquí y en setCoachRoomState.
        coachPanel_->getCoachingGuide().onAdvanceStage = [this]() {
            auto* coach = processorRef_.getCoachEngine();
            if (coach == nullptr) return;
            coach->executeCommand("/next");
            if (narrativeDirector_)
                narrativeDirector_->updateCoachingGuide();
        };

        coachPanel_->getCoachingGuide().onRequestHelp = [this]() {
            auto* coach = processorRef_.getCoachEngine();
            if (coach == nullptr) return;
            coach->executeCommand("/next");
            if (narrativeDirector_)
                narrativeDirector_->updateCoachingGuide();
            coach->handleUserMessage("gu\xC3\xAD" "a de la etapa actual");
        };

        // Hacer clic en tier de sugerencia
        coachPanel_->getCoachingGuide().onTierClicked = [this](int tierIndex) {
            juce::String tierName;
            switch (tierIndex) {
                case 0: tierName = "ajustar con plugins nativos"; break;
                case 1: tierName = "verificar con plugins gratuitos"; break;
                case 2: tierName = "mejorar con plugins profesionales"; break;
                default: return;
            }
            auto* coach = processorRef_.getCoachEngine();
            if (coach != nullptr) {
                coach->handleUserMessage("Ay\xC3\xBA" "dame a " + tierName);
            }
        };

        // ═══ /skip — Saltar setup ═══════════════════════════════════════════
        coachPanel_->onSkipSetup = [this]() {
            if (!isPreFullUI(coachRoomState_)) return;
            auto* coach = processorRef_.getCoachEngine();
            if (coach != nullptr) coach->forceSetupComplete();
            coachPanel_->setShowSuggestionsOverride(false);
            coachPanel_->setSuggestions({});
            coachPanel_->setInlineReferenceDropZone(false);
            coachPanel_->setInlineMessengerStatus(false, 0, {});
            messengerTracksDetected_ = false;
            if (coach != nullptr && coach->getSetupGenre().isEmpty())
                coach->setSetupGenre("No especificado");
            setCoachRoomState(CoachRoomState::GainStaging);
            coachPanel_->addSystemMessage(
                "**Setup saltado.**\n\n"
                "Has omitido la configuraci\xC3\xB3n inicial. Se usar\xC3\xA1n valores "
                "por defecto. Puedes volver a configurar en cualquier momento "
                "pregunt\xC3\xA1ndome.\n\n"
                "**Bienvenido al modo coaching!**");
            auto sceneDef = sceneManager_.forceTransition(SceneId::Coaching);
            applyScene(sceneDef);
        };

        // --- Section selection ---
        coachPanel_->onSectionSelected = [this](int sectionIndex) {
            auto* coach = processorRef_.getCoachEngine();
            if (coach != nullptr) coach->setActiveSection(sectionIndex);
        };

        // --- Track selection → MixMapDetailPanel ---
        coachPanel_->onTrackSelected = [this](int slotIndex) {
            if (slotIndex < 0) {
                coachPanel_->getMixMapDetailPanel().setVisible(false);
                return;
            }
            // Leer datos desde SlotRegistry + TrackAudioResult
            auto& registry = sharedData_.getSlotRegistry();
            auto info = registry.getSlotInfo(slotIndex);
            auto* coach = processorRef_.getCoachEngine();
            float targetLevel = -18.0f;
            juce::String roleName;
            juce::String roleConfidenceStr;
            if (coach != nullptr) {
                auto trackRoles = coach->getTrackRoles();
                if (slotIndex >= 0 && slotIndex < (int)trackRoles.size()) {
                    auto& role = trackRoles[slotIndex];
                    roleName = juce::String(getRoleName(role));
                    // Target levels are genre-dependent; use -18dB default for now
                    targetLevel = (role == TrackRole::Kick || role == TrackRole::BassSub || role == TrackRole::Bass808) ? -12.0f
                                : (role == TrackRole::VozPrincipal || role == TrackRole::Snare) ? -15.0f
                                : -18.0f;
                }
            }
            // Obtener datos de audio en vivo desde TrackAudioResult
            auto trackResult = sharedData_.getTrackAudioResult(slotIndex);
            float peakDb = juce::jmax(trackResult.peakLeft, trackResult.peakRight);
            float rmsDb = juce::jmax(trackResult.rmsLeft, trackResult.rmsRight);
            float correlation = trackResult.correlation;
            const float* bandEnergies = trackResult.bandEnergies;
            // Mostrar panel MixMapDetail con datos vivos
            coachPanel_->getMixMapDetailPanel().setTrackData(
                slotIndex,
                juce::String(info.trackName),
                roleName,
                peakDb,
                rmsDb,
                correlation,
                0.0f, // stereoWidth not available from TrackAudioResult
                bandEnergies,
                targetLevel,
                0.0f  // roleConfidence not available directly
            );
            coachPanel_->getMixMapDetailPanel().setVisible(true);
            coachPanel_->getMixMapDetailPanel().toFront(false);
            // Configurar callback de refresco en vivo
            coachPanel_->getMixMapDetailPanel().onClose = [this]() {
                coachPanel_->getMixMapDetailPanel().setVisible(false);
            };
            coachPanel_->getMixMapDetailPanel().onTogglePinned = [this](bool pinned) {
                // BUG FIX: Callback nunca estaba cableado.
                // MixMapDetailPanel ya gestiona isPinned_ internamente.
                // El callback notifica al shell para que no cierre el panel
                // al seleccionar otra pista o hacer clic fuera.
                juce::ignoreUnused(pinned);
            };
            coachPanel_->getMixMapDetailPanel().onRequestSolo = [this](int soloSlot) {
                if (soloSlot >= 0) {
                    // Solo request through coach engine
                    auto* coach = processorRef_.getCoachEngine();
                    // Nota: requestSoloTrack no está disponible en CoachEngine aún
            // if (coach != nullptr) coach->requestSoloTrack(soloSlot);
            juce::ignoreUnused(coach, soloSlot);
                }
            };
            coachPanel_->getMixMapDetailPanel().onRefreshData = [this, slotIndex]() {
                auto& reg = sharedData_.getSlotRegistry();
                auto inf = reg.getSlotInfo(slotIndex);
                auto* coa = processorRef_.getCoachEngine();
                float tgt = -18.0f;
                juce::String rName;
                if (coa != nullptr) {
                    auto trackRoles = coa->getTrackRoles();
                    if (slotIndex >= 0 && slotIndex < (int)trackRoles.size()) {
                        rName = juce::String(getRoleName(trackRoles[slotIndex]));
                        tgt = (trackRoles[slotIndex] == TrackRole::Kick || trackRoles[slotIndex] == TrackRole::BassSub) ? -12.0f : -18.0f;
                    }
                }
                auto tRes = sharedData_.getTrackAudioResult(slotIndex);
                float pDb = juce::jmax(tRes.peakLeft, tRes.peakRight);
                float rDb = juce::jmax(tRes.rmsLeft, tRes.rmsRight);
                auto* bandEn = tRes.bandEnergies;
                coachPanel_->getMixMapDetailPanel().setTrackData(
                    slotIndex,
                    juce::String(inf.trackName),
                    rName,
                    pDb,
                    rDb,
                    tRes.correlation,
                    0.0f,
                    bandEn,
                    tgt,
                    0.0f
                );
            };
        };

        // ─── Genre selection (v\xC3\xADa GenreSelectionCard tarjetas grandes) ────────
        coachPanel_->onGenreCardSelected = [this](const juce::String& genreKey,
                                                  const juce::String& genreLabel) {
            auto* coach = processorRef_.getCoachEngine();
            coachPanel_->addUserMessage(genreLabel);
            if (coach != nullptr) {
                if (coach->getSetupStep() == CoachEngine::SetupStep::WaitingForGenre) {
                    coach->detectAndSetGenre(genreLabel);
                }
            }
            coachPanel_->setShowGenreCards(false);
            // ═══ SceneManager: GenreSelected → ReferenceLoad ═════════════
            processDirectorEvent({DirectorEvent::Type::GenreSelected});
            // ═══ P4: Mensaje de perfil de género + robot animado ═══
            setAvatarExpression(AvatarExpression::Encouraging);
            setAvatarNod(500);
            {
                // Mostrar perfil de género enriquecido
                auto* coach = processorRef_.getCoachEngine();
                if (coach != nullptr) {
                    auto richMsg = coach->getGenreCharacteristicsMessage(genreKey, genreLabel);
                    coachPanel_->addSystemMessage(richMsg);
                }
            }                    // ═══ Auto-advance: robot asiente + delay antes de mostrar referencia + fade-in ═══
            setAvatarNod(400);
            callAfterDelaySafe(400, [this]() {
                // ═══ FULL-SCREEN reference como mode/genre cards ═══
                postUIEvent("[REPORT]", "Abriendo pantalla de referencia...");
                coachPanel_->setGreetingText("¿Tienes una referencia de cómo quieres que suene?");
                coachPanel_->setShowReferenceCards(true);
                coachPanel_->setSuggestions({"Saltar referencia"});
                coachPanel_->setShowSuggestionsOverride(true);
                startSetupTransition();
            });
        };

        // ─── Chat message sent: robot asiente cuando el usuario escribe ──
        coachPanel_->onMessageSent = [this](const juce::String& text) {
            juce::ignoreUnused(text);
            setAvatarNod(400);
        };

        // ─── Setup flow: suggestion clicks ──────────────────────────────
        coachPanel_->onSuggestionClicked = [this](const juce::String& text) {
            LogHelper::writeToLog("[DIAG] NavigationShell onSuggestionClicked called with "" + text + "" state=" + juce::String(static_cast<int>(getCoachRoomState())));
            if (getCoachRoomState() == CoachRoomState::Intention) {
                bool isMaster = text.contains("Masterizar");
                coachPanel_->addUserMessage(text);
                auto* coach = processorRef_.getCoachEngine();
                if (coach != nullptr) {
                    coach->setCoachMode(isMaster ? CoachMode::Master : CoachMode::Mix);
                    if (coach->getSetupStep() == CoachEngine::SetupStep::WaitingForMode) {
                        coach->detectAndSetMode(text);
                    }
                    // ═══ EventLog: modo seleccionado ═════════════════════════
                    coach->getEventLog().logEvent(SessionEvent::Type::SetupModeSelected,
                                                  "Modo: " + juce::String(isMaster ? "Master" : "Mix"));
                }
                coachPanel_->setShowModeCards(false);
                coachPanel_->setShowSuggestionsOverride(false);
                coachPanel_->setSuggestions({});
                // ═══ SceneManager: ModeSelected → GenreSelection ═════════════
                processDirectorEvent({DirectorEvent::Type::ModeSelected});
                // ═══ Auto-advance: robot asiente + delay antes de mostrar genre grid ═══
                // ═══ FULL-SCREEN EXPERIENCE: genre grid centrado como las mode cards ═══
                setAvatarExpression(AvatarExpression::Thinking);
                setAvatarNod(400);
                callAfterDelaySafe(400, [this]() {
                    coachPanel_->setGreetingText("¡Excelente! ¿Qué género vamos a mezclar? Elige uno.");
                    coachPanel_->setShowGenreCards(true);
                });
            }
            else if (getCoachRoomState() == CoachRoomState::Genre) {
                // ═══ Strip emoji prefix de los chips inline ═══
                // ej: "🎸 Rock" → "Rock"
                juce::String rawGenre = text.fromFirstOccurrenceOf(" ", false, false).trim();
                if (rawGenre.isEmpty()) rawGenre = text;

                coachPanel_->addUserMessage(text);
                auto* coach = processorRef_.getCoachEngine();
                if (coach != nullptr) {
                    if (coach->getSetupStep() == CoachEngine::SetupStep::WaitingForGenre) {
                        coach->detectAndSetGenre(rawGenre);
                    }
                    // ═══ EventLog: género seleccionado ════════════════════════
                    coach->getEventLog().logEvent(SessionEvent::Type::SetupGenreSelected,
                                                  "Género: " + text);
                }
                coachPanel_->setShowSuggestionsOverride(false);
                coachPanel_->setSuggestions({});
                // ═══ SceneManager: GenreSelected → ReferenceLoad ═════════════
                processDirectorEvent({DirectorEvent::Type::GenreSelected});
                // ═══ P4: Robot animado al confirmar género ═══
                setAvatarExpression(AvatarExpression::Encouraging);
                setAvatarNod(500);
                {
                    auto* coach = processorRef_.getCoachEngine();
                    if (coach != nullptr) {
                        auto richMsg = coach->getGenreCharacteristicsMessage(rawGenre, rawGenre);
                        coachPanel_->addSystemMessage(richMsg);
                    }
                }
                // ═══ Auto-advance: delay antes de mostrar referencia ═══
                callAfterDelaySafe(400, [this]() {
                    // ═══ FULL-SCREEN reference como mode/genre cards ═══
                    postUIEvent("[REPORT]", "Abriendo pantalla de referencia...");
                    coachPanel_->setGreetingText("¿Tienes una referencia de cómo quieres que suene?");
                    coachPanel_->setShowReferenceCards(true);
                    coachPanel_->setSuggestions({"Saltar referencia"});
                    coachPanel_->setShowSuggestionsOverride(true);
                });
            }
            else if (getCoachRoomState() == CoachRoomState::ReferenceStage) {
                coachPanel_->addUserMessage(text);
                // ═══ GAP #8: Reintentar detección de yt-dlp después de instalar ═══
                if (text.containsIgnoreCase("reintentar")) {
                    if (UrlDownloader::isYtDlpAvailable()) {
                        coachPanel_->addSystemMessage(
                            "**yt-dlp detectado!**\n\n"
                            "Ahora puedes pegar un enlace de YouTube/Spotify para analizarlo.");
                        coachPanel_->setSuggestions({});
                        coachPanel_->setShowSuggestionsOverride(false);
                    } else {
                        coachPanel_->addSystemMessage(
                            "\xE2\x9D\x8C **yt-dlp sigue sin detectarse.**\n\n"
                            "Aseg\xC3\xBArate de:\n"
                            "  1. Descargar el ejecutable desde GitHub\n"
                            "  2. Colocarlo en **Documentos/MixCoach/Tools/**\n"
                            "  3. O instalarlo con **winget install yt-dlp**\n\n"
                            "Luego haz clic en Reintentar de nuevo.");
                    }
                    return;
                }
                if (text.containsIgnoreCase("saltar")) {
                    coachPanel_->setShowReferenceCards(false);
                    coachPanel_->setShowSuggestionsOverride(false);
                    coachPanel_->setSuggestions({});
                    processDirectorEvent({DirectorEvent::Type::ReferenceSkipped});
                    auto* coach = processorRef_.getCoachEngine();
                    // ═══ EventLog: referencia saltada ═════════════════════════
                    if (coach != nullptr)
                        coach->getEventLog().logEvent(SessionEvent::Type::ReferenceSkipped);
                    // ═══ Saludo del Coach ═══
                    {
                        juce::String name = coach != nullptr ? coach->getEngineerName() : juce::String();
                        juce::String greeting = "¡Excelente " + (name.isNotEmpty() ? name : "ingeniero")
                                                + "! Ya sé cómo quieres sonar.\nAhora organicemos la sesión.";
                        coachPanel_->setGreetingText(greeting);
                    }
                    coachPanel_->setShowSessionPrepCard(true, coach, &sharedData_.getSlotRegistry());
                }
            }
            else if (getCoachRoomState() == CoachRoomState::MixMapStage) {
                coachPanel_->addUserMessage(text);
                coachPanel_->setShowSuggestionsOverride(false);
                coachPanel_->setSuggestions({});
                if (coachPanel_->onMixMapConfirmed)
                    coachPanel_->onMixMapConfirmed();
            }
            else if (!isPreFullUI(coachRoomState_)) {
                coachPanel_->addUserMessage(text);
                auto* coach = processorRef_.getCoachEngine();
                if (coach != nullptr) {
                    coach->handleUserMessage(text);
                }
            }
        };

        // --- Wire progress screen + report panel ---
        addChildComponent(progressScreen_.get());
        progressScreen_->onViewReport = [this]() {
            if (progressScreen_ && reportPanel_) {
                // ═══ P8: Robot orgulloso al ver reporte ═══════════════
                setAvatarExpression(AvatarExpression::Happy);
                setAvatarWave(true);

                progressScreen_->setVisible(false);
                reportPanel_->setBounds(progressScreen_->getBounds());
                reportPanel_->setVisible(true);
                reportPanel_->toFront(false);
                refreshReport();
            }
        };

        wireReportPanel();

        // --- Restore saved references ---
        if (processorRef_.hasPendingReferences()) {
            auto filePaths = processorRef_.takePendingFilePaths();
            auto urls      = processorRef_.takePendingURLs();
            if (!filePaths.empty() || !urls.empty()) {
                coachPanel_->getRefPanel().restoreFromPaths(filePaths, urls);
            }
        }

        // ─── Conectar PanelRevealManager al MixCoachPanel ────────────────
        if (coachPanel_) {
            coachPanel_->setRevealManager(&revealManager_);
            revealManager_.onPanelRevealed = [this](PanelId panel, bool isFirstReveal) {
                if (!isFirstReveal || coachPanel_ == nullptr) return;
                auto welcomeScene = sceneManager_.resetToWelcome();
                applyScene(welcomeScene);
                {
                    const char* icon = "";
                    const char* name = "";
                    switch (panel) {
                        case PanelId::Reference:  icon = "[REPORT]"; name = "Reference"; break;
                        case PanelId::Messengers: icon = "[INFO]"; name = "Messengers"; break;
                        case PanelId::MixMap:     icon = "\xF0\x9F\x97\xBA"; name = "MixMap"; break;
                        case PanelId::Tools:      icon = "\xC2\xB7"; name = "Tools"; break;
                        case PanelId::Session:    icon = "[TREND]"; name = "Session"; break;
                        case PanelId::Report:     icon = "[EXPORT]"; name = "Report"; break;
                        default: break;
                    }
                    if (icon[0] != '\0' && name[0] != '\0') {
                        postUIEvent(juce::String(icon),
                                    "Panel \"" + juce::String(name) + "\" ahora disponible");
                    }
                }
                juce::Component* target = nullptr;
                switch (panel) {
                    case PanelId::Reference:  target = coachPanel_->getPanelComponent(PanelId::Reference); break;
                    case PanelId::Messengers: target = coachPanel_->getPanelComponent(PanelId::Messengers); break;
                    case PanelId::MixMap:     target = coachPanel_->getPanelComponent(PanelId::MixMap); break;
                    default: break;
                }
                if (target != nullptr) startRevealAnimation(target);
            };

            coachPanel_->onTrackHighlightRequest = [this](const juce::String& text) {
                auto result = revealManager_.processMessage(text);
                if (!result.tracksToHighlight.empty()) {
                    coachPanel_->getMessengerList().setTrackHighlights(result.tracksToHighlight);
                }
            };

            // ═══ Correction→Verify: usuario confirma que aplicó la corrección ═══
            // La lógica de verificación real (leer TrackAudioResult, comparar,
            // determinar Verified/Partial/Failed) está en CorrectionLearner.
            // NavigationShell solo maneja las reacciones UI (avatar, toast, auto-return).
            // ═══ GAP #4: Manejo de corrección delegado al Director ═══════════
            coachPanel_->onCorrectionApplied = [this](CorrectionCardData& data) {
                if (narrativeDirector_)
                    narrativeDirector_->handleCorrectionResult(data, true);
            };

            // ═══ Incremento 3c: A/B Reference indicator en MasterCheckPanel ═══
            coachPanel_->getMasterCheckPanel().onIsReferenceActive = [this]() {
                return processorRef_.getRefPlayer().isPlaying();
            };
        }

        // ─── Conectar Panels Menu ───────────────────────────────────────
        tabBar_.setRevealManager(&revealManager_);
        tabBar_.onPanelMenuSelected = [this](PanelId panelId) {
            LogHelper::writeToLog("[NavigationShell] Panel menu selected: "
                                  + juce::String(panelIdLabel(panelId)));
            // ═══ Acción explícita del usuario → cambiar a Expert scope ═══
            if (panelId != PanelId::Coach) {
                LogHelper::writeToLog("[ProgressiveReveal] User action → set Expert scope");
                revealManager_.setCurrentScope(AnalysisScope::Expert);
            }
            if (!revealManager_.isPanelRevealed(panelId)) {
                LogHelper::writeToLog("[NavigationShell] Panel menu selected for UNREVEALED panel, forcing reveal: "
                                      + juce::String(panelIdLabel(panelId)));
                revealPanel(panelId);
            }
            switch (panelId) {
                case PanelId::Coach:
                    if (getActiveTab() != TabBarComponent::Coach) setActiveTab(TabBarComponent::Coach);
                    break;
                case PanelId::Reference:
                    if (getActiveTab() != TabBarComponent::Coach) setActiveTab(TabBarComponent::Coach);
                    coachPanel_->showPanelReference();
                    break;
                case PanelId::Messengers:
                    if (getActiveTab() != TabBarComponent::Coach) setActiveTab(TabBarComponent::Coach);
                    coachPanel_->showPanelMessengers();
                    break;
                case PanelId::MixMap:
                    if (getActiveTab() != TabBarComponent::Coach) setActiveTab(TabBarComponent::Coach);
                    coachPanel_->showPanelMixMap();
                    break;
                case PanelId::Report:
                    if (getActiveTab() != TabBarComponent::Coach) setActiveTab(TabBarComponent::Coach);
                    break;
                case PanelId::Tools:
                    if (getActiveTab() != TabBarComponent::Tools) setActiveTab(TabBarComponent::Tools);
                    break;
                case PanelId::Session:
                    if (getActiveTab() != TabBarComponent::Session) setActiveTab(TabBarComponent::Session);
                    break;
                default: break;
            }
        };

        // ═══ Determinar estado inicial según el SetupStep real + V4 saved state ═══
        auto* initialCoach = processor.getCoachEngine();
        if (initialCoach != nullptr) {
            auto setupStep = initialCoach->getSetupStep();
            if (setupStep == CoachEngine::SetupStep::Complete) {
                // Setup completado → mostrar UI de coaching directamente
                coachRoomState_ = CoachRoomState::GainStaging;
                tabBar_.setVisible(true);
                coachPanel_->setVisible(true);
                coachPanel_->setWelcomeMode(false);
                welcomeComponent_->setVisible(false);
                updateTabLockState();
            }
            else if (setupStep != CoachEngine::SetupStep::NotStarted) {
                // Setup en progreso → restaurar coachPanel_ en el estado correcto
                // Intentar restaurar desde V4 saved state primero
                // ═══ FIX 2026-07-22: Ya NO excluimos Welcome del restore.
                // Antes, si savedState=Welcome, se caía al fallback que usaba
                // setupStep del CoachEngine (que podía tener datos de una sesión
                // anterior), causando UI mezclada al minimizar/restaurar.
                CoachRoomState savedState = processorRef_.getSavedCoachRoomState();
                bool hasSavedState = (savedState != CoachRoomState::Count);

                if (hasSavedState) {
                    coachRoomState_ = savedState;

                    // ═══ Si el estado guardado es Welcome, mostrar welcomeComponent_ ═══
                    if (coachRoomState_ == CoachRoomState::Welcome) {
                        tabBar_.setVisible(false);
                        coachPanel_->setVisible(false);
                        welcomeComponent_->setVisible(true);
                        welcomeComponent_->startWelcomeAnimation();
                        updateTabLockState();
                        LogHelper::writeToLog("[NavigationShell] Restaurado Welcome desde savedState");
                    } else {
                        // Mostrar coachPanel_ en modo welcome (setup) sin tabBar
                        tabBar_.setVisible(false);
                        coachPanel_->setVisible(true);
                        coachPanel_->setWelcomeMode(true);
                        welcomeComponent_->setVisible(false);

                        // ═══ Sincronizar el panel con el estado restaurado ═══════
                        coachPanel_->setCoachRoomState(coachRoomState_);

                        // ═══ Restaurar chips inline según el estado (en vez de old ModeCards/GenreGrid) ═══
                        {
                            if (coachRoomState_ == CoachRoomState::Intention) {
                                coachPanel_->setShowModeCards(true);
                            } else if (coachRoomState_ == CoachRoomState::Genre) {
                                // ═══ FULL-SCREEN genre grid (como mode cards) ═══
                                coachPanel_->setShowGenreCards(true);
                            } else if (coachRoomState_ == CoachRoomState::ReferenceStage) {
                                // ═══ FULL-SCREEN reference ═══
                                coachPanel_->setShowReferenceCards(true);
                                coachPanel_->setSuggestions({"Saltar referencia"});
                                coachPanel_->setShowSuggestionsOverride(true);
                            } else if (coachRoomState_ == CoachRoomState::SessionPrep) {
                                auto* restoreCoach = processorRef_.getCoachEngine();
                                if (restoreCoach != nullptr) {
                                    coachPanel_->setShowSessionPrepCard(true, restoreCoach, &sharedData_.getSlotRegistry());
                                }
                            } else if (coachRoomState_ == CoachRoomState::MixMapStage) {
                                coachPanel_->setSuggestions({"Confirmar mapa"});
                                coachPanel_->setShowSuggestionsOverride(true);
                            }
                        }

                        // ═══ FIX #2: Restaurar showSuggestionsOverride desde savedUIFlags ═══
                        // El constructor guarda coachRoomState pero NO restauraba los flags
                        // de UI como showSuggestionsOverride. Esto causaba que al reabrir
                        // el editor, las sugerencias se perdieran (ej: "Saltar referencia",
                        // "Confirmar mapa") y la UI se viera caótica.
                        {
                            uint32_t savedFlags = processorRef_.getSavedUIFlags();
                            if (savedFlags & (1 << 6)) {
                                // showSuggestionsOverride estaba activo
                                if (coachPanel_) {
                                    coachPanel_->setShowSuggestionsOverride(true);
                                }
                            }
                        }

                        updateTabLockState();
                        LogHelper::writeToLog("[NavigationShell] Estado restaurado: "
                                              + juce::String(coachRoomStateLabel(coachRoomState_))
                                              + " (SetupStep=" + juce::String(static_cast<int>(setupStep))
                                              + ", flags=0x" + juce::String::toHexString(processorRef_.getSavedUIFlags()) + ")");

                        // ═══ FASE 5: Preguntar si quiere retomar (defer ~500ms para UI lista) ═══
                        callAfterDelaySafe(500, [this]() {
                            juce::String name = processorRef_.getSavedUserName();
                            juce::String greeting = name.isNotEmpty()
                                ? "¡Bienvenido de vuelta, **" + name + "**!"
                                : "¡Bienvenido de vuelta!";
                            coachPanel_->addSystemMessage(
                                greeting + "\n\n"
                                "Parece que estabas en medio de una sesi\xC3\xB3n. "
                                "\xC2\xBFQuieres retomar donde te quedaste?");
                            coachPanel_->showQuickReplies({"S\xC3\xAD, continuar", "Empezar de nuevo"});
                            // Guardar el callback original para restaurarlo después
                            // Usar QuickReplyBar::onReplySelected en vez de SOBREESCRIBIR
                            // onSuggestionClicked (evita romper mode cards en flujo normal)
                            coachPanel_->getQuickReplyBar().onReplySelected = [this](const juce::String& text) {
                                if (text.contains("continuar")) {
                                    coachPanel_->hideQuickReplies();
                                    coachPanel_->addSystemMessage(
                                        "â Perfecto, continuemos donde estÃ¡" "bamos.");
                                } else {
                                    coachPanel_->hideQuickReplies();
                                    coachPanel_->clearMessages();
                                    coachPanel_->addSystemMessage(
                                        "ð Entendido. Empecemos una sesiÃ³n nueva.");
                                    setCoachRoomState(CoachRoomState::Welcome);
                                    coachPanel_->setWelcomeMode(false);
                                    coachPanel_->setShowModeCards(false);
                                    coachPanel_->setShowGenreCards(false);
                                    coachPanel_->setShowReferenceCards(false);
                                    coachPanel_->setShowSessionPrepCard(false);
                                    coachPanel_->setCoachRoomState(CoachRoomState::Intention);
                                    coachPanel_->setShowModeCards(true);
                                }
                            };
                        });
                    }
                } else {
                    // Fallback: mapear SetupStep → CoachRoomState
                    // ═══ FIX: También restaurar nombre y flags de usuario al usar fallback ═══
                    switch (setupStep) {
                        case CoachEngine::SetupStep::WaitingForMode:
                            coachRoomState_ = CoachRoomState::Intention;
                            break;
                        case CoachEngine::SetupStep::WaitingForReferenceFirst:
                        case CoachEngine::SetupStep::WaitingForReference:
                        case CoachEngine::SetupStep::WaitingForConfirm:
                            coachRoomState_ = CoachRoomState::ReferenceStage;
                            break;
                        case CoachEngine::SetupStep::WaitingForGenre:
                            coachRoomState_ = CoachRoomState::Genre;
                            break;
                        case CoachEngine::SetupStep::WaitingForSetupInstructions:
                            coachRoomState_ = CoachRoomState::SessionPrep;
                            break;
                        case CoachEngine::SetupStep::WaitingForDestination:
                            coachRoomState_ = CoachRoomState::Intention;
                            break;
                        default:
                            coachRoomState_ = CoachRoomState::Intention;
                            break;
                    }

                    // Mostrar coachPanel_ en modo welcome (setup) sin tabBar
                    tabBar_.setVisible(false);
                    coachPanel_->setVisible(true);
                    coachPanel_->setWelcomeMode(true);
                    welcomeComponent_->setVisible(false);

                    // ═══ Sincronizar el panel con el estado restaurado ═══
                    coachPanel_->setCoachRoomState(coachRoomState_);

                    // ═══ Restaurar chips inline según el estado (fallback) ═══
                    {
                        if (coachRoomState_ == CoachRoomState::Intention) {
                            coachPanel_->setShowModeCards(true);
                        } else if (coachRoomState_ == CoachRoomState::Genre) {
                            // ═══ FULL-SCREEN genre grid (como mode cards) ═══
                            coachPanel_->setShowGenreCards(true);
                        } else if (coachRoomState_ == CoachRoomState::ReferenceStage) {
                            coachPanel_->setInlineReferenceDropZone(true);
                            coachPanel_->setSuggestions({"Saltar referencia"});
                            coachPanel_->setShowSuggestionsOverride(true);
                            coachPanel_->setInlineReferenceDropZone(true);
                            coachPanel_->setSuggestions({"Saltar referencia"});
                            coachPanel_->setShowSuggestionsOverride(true);
                        } else if (coachRoomState_ == CoachRoomState::SessionPrep) {
                            auto* restoreCoach = processorRef_.getCoachEngine();
                            if (restoreCoach != nullptr) {
                                coachPanel_->setShowSessionPrepCard(true, restoreCoach, &sharedData_.getSlotRegistry());
                            }
                        } else if (coachRoomState_ == CoachRoomState::MixMapStage) {
                            coachPanel_->setSuggestions({"Confirmar mapa"});
                            coachPanel_->setShowSuggestionsOverride(true);
                        }
                    }
                }

                updateTabLockState();

                // ═══ V4: Restaurar nombre de usuario guardado (ambos paths) ═══
                juce::String savedName = processorRef_.getSavedUserName();
                if (savedName.isNotEmpty() && initialCoach->getEngineerName().isEmpty()) {
                    initialCoach->setEngineerName(savedName);
                }

                LogHelper::writeToLog("[NavigationShell] Estado restaurado: "
                                      + juce::String(coachRoomStateLabel(coachRoomState_))
                                      + " (SetupStep=" + juce::String(static_cast<int>(setupStep))
                                      + ", flags=0x" + juce::String::toHexString(processorRef_.getSavedUIFlags()) + ")");
            }
            else {
                // Setup no iniciado → mostrar Welcome
                tabBar_.setVisible(false);
                coachPanel_->setVisible(false);
                welcomeComponent_->setVisible(true);
                welcomeComponent_->startWelcomeAnimation();
                coachRoomState_ = CoachRoomState::Welcome;
                updateTabLockState();
                LogHelper::writeToLog("[NavigationShell] Mostrando Welcome (setup no iniciado)");
            }
        }
        else {
            // CoachEngine no disponible aún → mostrar Welcome (editor timer lo reintentará)
            tabBar_.setVisible(false);
            coachPanel_->setVisible(false);
            welcomeComponent_->setVisible(true);
            welcomeComponent_->startWelcomeAnimation();
            coachRoomState_ = CoachRoomState::Welcome;
            updateTabLockState();
        }

        // ═══ Conectar QuickReplyBar ═══════════════════════════════════════
        coachPanel_->getQuickReplyBar().onReplySelected = [this](const juce::String& reply) {
            auto* coach = processorRef_.getCoachEngine();
            if (coach == nullptr || !coach->hasPendingConfirmation()) return;
            int slotIndex = coach->getPendingConfirmationSlot();
            juce::String lower = reply.trim().toLowerCase();
            if (lower == "s\xC3\xAD" || lower == "si" || lower == "yes") {
                coach->handleConfirmationResponse(slotIndex, true);
                coachPanel_->hideQuickReplies();
            }
            else if (lower == "no") {
                coach->handleConfirmationResponse(slotIndex, false);
                coachPanel_->hideQuickReplies();
            }
            else {
                coachPanel_->hideQuickReplies();
            }
            if (coach->hasPendingConfirmation()) {
                coachPanel_->showQuickReplies({"S\xC3\xAD", "No", "Otro..."});
            }
        };

        // ═══ Cablear LlmCommandInterpreter ═════════════════════════════════
        commandInterpreter_.onRevealPanel = [this](const juce::String& panelId) {
            juce::String lower = panelId.trim().toLowerCase();
            if (lower == "reference")   { postUIEvent("[REPORT]", "Abriendo referencia..."); revealPanel(PanelId::Reference); }
            else if (lower == "messengers") { postUIEvent("[INFO]", "Detectando pistas..."); revealPanel(PanelId::Messengers); }
            else if (lower == "mixmap") { postUIEvent("\xF0\x9F\x97\xBA", "Generando mapa..."); revealPanel(PanelId::MixMap); }
            else if (lower == "tools")  { postUIEvent("·", "Abriendo analizadores..."); revealPanel(PanelId::Tools); }
            else if (lower == "session") { postUIEvent("[TREND]", "Mostrando progreso..."); revealPanel(PanelId::Session); }
            else if (lower == "report") { postUIEvent("[EXPORT]", "Abriendo reporte..."); revealPanel(PanelId::Report); setCoachRoomState(CoachRoomState::Report); }
        };

        commandInterpreter_.onSetCoachState = [this](const juce::String& state) {
            juce::String lower = state.trim().toLowerCase();
            if (lower == "welcome")        setCoachRoomState(CoachRoomState::Welcome);
            else if (lower == "intention") setCoachRoomState(CoachRoomState::Intention);
            else if (lower == "genre")     setCoachRoomState(CoachRoomState::Genre);
            else if (lower == "reference") setCoachRoomState(CoachRoomState::ReferenceStage);
            else if (lower == "messenger") setCoachRoomState(CoachRoomState::MessengerStage);
            else if (lower == "sessionprep") setCoachRoomState(CoachRoomState::SessionPrep);
            else if (lower == "mixmap")    setCoachRoomState(CoachRoomState::MixMapStage);
            else if (lower == "gain")      setCoachRoomState(CoachRoomState::GainStaging);
            else if (lower == "balance")   setCoachRoomState(CoachRoomState::Balance);
            else if (lower == "eq")        setCoachRoomState(CoachRoomState::EQ);
            else if (lower == "compression") setCoachRoomState(CoachRoomState::Compression);
            else if (lower == "space")     setCoachRoomState(CoachRoomState::Space);
            else if (lower == "automation") setCoachRoomState(CoachRoomState::Automation);
            else if (lower == "refinement") setCoachRoomState(CoachRoomState::Refinement);
            else if (lower == "mastercheck") setCoachRoomState(CoachRoomState::MasterCheck);
        };

        commandInterpreter_.onSwitchTab = [this](const juce::String& tab) {
            juce::String lower = tab.trim().toLowerCase();
            if (lower == "coach" || lower == "chat") { cancelAutoReturn(); setActiveTab(TabBarComponent::Coach); }
            else if (lower == "tools" || lower == "analyzers" || lower == "analyzer") { autoSwitchTab(TabBarComponent::Tools); }
            else if (lower == "session" || lower == "stats" || lower == "progress") { autoSwitchTab(TabBarComponent::Session); }
        };

        commandInterpreter_.onHighlightTrack = [this](const juce::String& track, int) {
            if (coachPanel_) coachPanel_->highlightTrackMention(track);
        };

        commandInterpreter_.onCelebrate = [this](const juce::String& message) {
            experienceManager_.celebrate(message);
        };

        commandInterpreter_.onSetMode = [this](bool isMixMode) {
            auto* coach = processorRef_.getCoachEngine();
            if (coach != nullptr) coach->setCoachMode(isMixMode ? CoachMode::Mix : CoachMode::Master);
        };

        commandInterpreter_.onReturnToCoach = [this]() {
            cancelAutoReturn();
            if (getActiveTab() != TabBarComponent::Coach) setActiveTab(TabBarComponent::Coach);
        };

        commandInterpreter_.onAdvancePhase = [this]() {
            auto* phaseMgr = processorRef_.getPhaseManager();
            if (phaseMgr != nullptr) phaseMgr->advanceToNextPhase();
        };

        commandInterpreter_.onShowReport = [this]() {
            // ═══ P8: Robot orgulloso al mostrar reporte ════════════
            setAvatarExpression(AvatarExpression::Happy);
            setAvatarWave(true);

            if (reportPanel_) {
                if (getActiveTab() != TabBarComponent::Session) setActiveTab(TabBarComponent::Session);
                refreshReport();
                if (progressScreen_) progressScreen_->setVisible(false);
                reportPanel_->setBounds(getLocalBounds().withTrimmedTop(TabBarComponent::kHeight));
                reportPanel_->setVisible(true);
                reportPanel_->toFront(false);
            }
        };

        commandInterpreter_.onShowSuggestions = [this](const std::vector<juce::String>& suggestions) {
            if (coachPanel_) coachPanel_->setSuggestions(suggestions);
        };

        // ═══ Auto-open Analyzer: select_analyzer ═══════════════════════════
        // NOTA: Durante coaching con el director activo, la evidencia ya se
        // muestra en el panel derecho (CoachingEvidenceHost). Este handler
        // solo aplica cuando el usuario pide explícitamente ver un analyzer.
        commandInterpreter_.onSelectAnalyzer = [this](const juce::String& analyzer) {
            juce::String lower = analyzer.trim().toLowerCase();
            if (analyzersPanel_ == nullptr) return;

            // Solo actuar si la UI de coaching está activa y Tools no está bloqueado
            if (!isCoachingState(coachRoomState_)) return;
            if (tabBar_.isTabLocked(TabBarComponent::Tools)) return;

            // ═══ Si el director narrativo está activo, no cambiar de tab ═══
            // La evidencia ya se muestra en CoachingEvidenceHost (panel derecho).
            // Solo postear evento UI para feedback visual.
            if (narrativeDirector_ && narrativeDirector_->isActive()) {
                postUIEvent("[SEARCH]", "Mostrando evidencia en panel derecho...");
                return;
            }

            // Primero cambiar a Tools tab (solo si el director NO está activo)
            if (getActiveTab() != TabBarComponent::Tools) {
                cancelAutoReturn();
                setActiveTab(TabBarComponent::Tools);
                startAutoReturn();
            }

            // Desactivar todos los toggles primero
            analyzersPanel_->setShowDNA(false);
            analyzersPanel_->setShowWidth(false);
            analyzersPanel_->setShowCrest(false);

            // Activar solo el analyzer solicitado
            if (lower == "spectrum") {
                // Spectrum es la vista por defecto, no necesita toggle
                postUIEvent("·", "Mostrando espectro...");
            }
            else if (lower == "vectorscope") {
                analyzersPanel_->setShowWidth(true);
                postUIEvent("\xF0\x9F\x94\xAE", "Mostrando vectorscope...");
            }
            else if (lower == "crest") {
                analyzersPanel_->setShowCrest(true);
                postUIEvent("·", "Mostrando crest factor...");
            }
            else if (lower == "stereo") {
                analyzersPanel_->setShowWidth(true);
                postUIEvent("\xF0\x9F\x94\x8A", "Mostrando ancho estereo...");
            }
            else if (lower == "dna") {
                analyzersPanel_->setShowDNA(true);
                postUIEvent("\xF0\x9F\xA7\xAC", "Mostrando Audio DNA...");
            }

            analyzersPanel_->resized();
            analyzersPanel_->repaint();
        };

        // ─── SpectrumHighlight ────────────────────────────────────────────
        commandInterpreter_.onSpectrumHighlight = [this](float frequencyHz, float bandwidthHz, const juce::String& label) {
            if (analyzersPanel_) {
                analyzersPanel_->getSpectrograph().setHighlightFrequency(frequencyHz, bandwidthHz, label);
            }
            auto* coach = processorRef_.getCoachEngine();
            if (coach != nullptr) coach->requestDiagnosticUpdate();
        };

        // ─── MixmapHighlight ──────────────────────────────────────────────
        commandInterpreter_.onMixmapHighlight = [this](const juce::String& trackName, const juce::String& busName) {
            int foundSlot = -1;
            if (trackName.isNotEmpty()) {
                sharedData_.getSlotRegistry().forEachActive([&](const SlotInfo& slotInfo) {
                    if (juce::String(slotInfo.trackName).equalsIgnoreCase(trackName.trim())) {
                        foundSlot = slotInfo.slotIndex;
                    }
                });
            }
            if (foundSlot >= 0) {
                showFocusOverlay(foundSlot);
            } else if (busName.isNotEmpty()) {
                juce::String lowerBus = busName.trim().toLowerCase();
                BusType bus = BusType::None;
                if (lowerBus == "drums")        bus = BusType::Drums;
                else if (lowerBus == "bass")    bus = BusType::Bass;
                else if (lowerBus == "guitars") bus = BusType::Guitars;
                else if (lowerBus == "keys")    bus = BusType::Keys;
                else if (lowerBus == "vocals")  bus = BusType::Vocals;
                else if (lowerBus == "fx")      bus = BusType::FX;
                else if (lowerBus == "melody")  bus = BusType::Melody;
                if (bus != BusType::None) showFocusOverlay(bus);
            }
        };

        // ─── AvatarEmotion ─────────────────────────────────────────────
        commandInterpreter_.onAvatarEmotion = [this](const juce::String& emotion) {
            juce::String lower = emotion.trim().toLowerCase();
            AvatarExpression exp = AvatarExpression::Neutral;
            if (lower == "happy")        exp = AvatarExpression::Happy;
            else if (lower == "serious")    exp = AvatarExpression::Serious;
            else if (lower == "thinking")    exp = AvatarExpression::Thinking;
            else if (lower == "surprised")  exp = AvatarExpression::Surprised;
            else if (lower == "encouraging") exp = AvatarExpression::Encouraging;
            setAvatarExpression(exp);
        };

        // ─── ShowIssueCard ─────────────────────────────────────────────
        commandInterpreter_.onShowIssueCard = [this](const juce::String& track, const juce::String& severity,
                                                      const juce::String& issueType, const juce::String& description) {
            if (!coachPanel_) return;
            juce::String icon;
            if (severity.trim().toLowerCase() == "critical") icon = "[EXCLAMATION]";
            else if (severity.trim().toLowerCase() == "warning") icon = "[WARN]";
            else icon = "\xE2\x84\xB9";
            juce::String msg;
            msg << icon << " [" << issueType.upToFirstOccurrenceOf(" ", false, false)
                << "] " << track;
            if (description.isNotEmpty()) msg << "\n    " << description;
            coachPanel_->addSystemMessage(msg);
            auto* coach = processorRef_.getCoachEngine();
            if (coach != nullptr) coach->requestDiagnosticUpdate();
        };

        // ═══ Callback unificado: onRevealPanel ═══════════════════════════
        onRevealPanel = [this](PanelId panel) {
            LogHelper::writeToLog("[NavigationShell] onRevealPanel: "
                                  + juce::String(panelIdLabel(panel)));
            switch (panel) {
                case PanelId::MixMap: experienceManager_.celebrate("Session Map desbloqueado"); break;
                default: break;
            }
        };

        // ═══ Pasar int\xC3\xA9rprete al AiCoachAdapter ════════════════════════
        auto* adapter = processorRef_.getAiCoachAdapter();
        if (adapter != nullptr) {
            adapter->setCommandInterpreter(&commandInterpreter_);
        }

        // ═══ Conectar ExperienceManager al CoachEngine ═════════════════
        if (initialCoach != nullptr) {
            experienceManager_.wireToEngine(*initialCoach);

            // ═══ Conectar sugerencia de reverb inline ═══════════════════════
            initialCoach->setReverbSuggestedCallback(
                [this](float preDelayMs, float decaySec, float highCutHz, float mixPct,
                        const juce::String& genre, const juce::String& algorithm,
                        const std::vector<juce::String>& trackNames,
                        const std::vector<juce::String>& trackRoles) {
                    // Construir ReverbCardData desde los parámetros del callback
                    ReverbCardData data;
                    data.preDelayMs = preDelayMs;
                    data.decaySec = decaySec;
                    data.highCutHz = highCutHz;
                    data.mixPct = mixPct;
                    data.genre = genre;
                    data.algorithm = algorithm;
                    data.trackNames = trackNames;
                    data.trackRoles = trackRoles;

                    // Mostrar la tarjeta inline en el chat
                    if (coachPanel_) {
                        coachPanel_->addReverbCard(data);
                    }
                });
        }
    } // End constructor

    // ═══════════════════════════════════════════════════════════════════════════
    //  ~NavigationShell — Salva estado al destruir el editor (minimize/cerrar)
    // ═══════════════════════════════════════════════════════════════════════════
    NavigationShell::~NavigationShell()
    {
        saveCurrentUIFlags();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  saveCurrentUIFlags — Persiste flags de UI + coachRoomState para restore
    //  Se llama desde visibilityChanged (al ocultar/minimizar) y ~NavigationShell
    // ═══════════════════════════════════════════════════════════════════════════
    void NavigationShell::saveCurrentUIFlags()
    {
        // ═══ Guardia: durante la destrucción del editor, coachPanel_ podría
        // estar nullptr si el orden de destrucción no es el esperado.
        // En ese caso, no intentamos guardar flags (el estado ya se guardó
        // desde visibilityChanged() al ocultar el plugin).
        if (coachPanel_ == nullptr) return;

        uint32_t flags = 0;

        // Reconstruir flags desde el coachRoomState_ actual + estado de coachPanel_
        // Layout de bits (mismo que en restore del constructor):
        // bit 1: showModeCards (Intention)
        // bit 2: showGenreCards (Genre)
        // bit 3: showSessionPrepCard (SessionPrep)
        // bit 4: inlineReferenceDropZone (ReferenceStage)
        // bit 6: showSuggestionsOverride
        if (coachRoomState_ == CoachRoomState::Intention) flags |= (1 << 1);
        if (coachRoomState_ == CoachRoomState::Genre)      flags |= (1 << 2);
        if (coachRoomState_ == CoachRoomState::SessionPrep) flags |= (1 << 3);
        if (coachRoomState_ == CoachRoomState::ReferenceStage) flags |= (1 << 4);
        // suggestions override — activo si el override de sugerencias est\xC3\xA1 habilitado
        if (coachPanel_ && coachPanel_->isShowSuggestionsOverride())
            flags |= (1 << 6);

        processorRef_.setSavedUIFlags(flags);
        processorRef_.setSavedCoachRoomState(coachRoomState_);

        // Guardar nombre de usuario si está disponible
        auto* coach = processorRef_.getCoachEngine();
        if (coach != nullptr && coach->getEngineerName().isNotEmpty()) {
            processorRef_.setSavedUserName(coach->getEngineerName());
        }
    }

    // =======================================================================
    //  resized
    // =======================================================================
    void NavigationShell::resized()
    {
        auto area = getLocalBounds();

        backgroundEffects_.setBounds(area);
        focusOverlay_.setBounds(area);

        if (!sceneManager_.getCurrentSceneDef().sidebarEnabled) {
            tabBar_.setVisible(false);
            if (analyzersPanel_) analyzersPanel_->setVisible(false);
            if (reportPanel_) reportPanel_->setVisible(false);

            // ═══ Back button: visible durante setup (excepto Welcome) ──────
            {
                auto currentScene = sceneManager_.getCurrentScene();
                bool showBack = (currentScene != SceneId::Welcome
                                 && currentScene != SceneId::Coaching
                                 && currentScene != SceneId::ToolInFocus
                                 && currentScene != SceneId::GroupFocus
                                 && currentScene != SceneId::TrackFocus
                                 && currentScene != SceneId::Refinement
                                 && currentScene != SceneId::RefinementTool
                                 && currentScene != SceneId::SessionEnd
                                 && currentScene != SceneId::ReportView);
                backButtonLabel_.setVisible(showBack);
                if (showBack) {
                    backButtonLabel_.setBounds(area.getX() + 6, area.getY() + 6, 70, 20);
                }
            }

            // PhaseProgressBar: only show during coaching, not during Welcome/setup
            // Onboarding (Intention/Genre/Reference/SessionPrep/MixMap) should be clean.
            if (isCoachingState(coachRoomState_)) {
                phaseProgressBar_.setBounds(area.removeFromTop(PhaseProgressBar::kHeight));
                phaseProgressBar_.setVisible(true);
            } else {
                phaseProgressBar_.setVisible(false);
            }

            auto scene = sceneManager_.getCurrentScene();
            // ═══ CRITICO: Si hay un crossfade activo, salir de resized() inmediatamente ═══
            // No cambiar visibilidad NI bounds durante la transición para evitar "pantalla dividida"
            if (crossfade_.active) {
                // Solo actualizar bounds de componentes ya visibles — no cambiar visibilidad
                if (welcomeComponent_ != nullptr && welcomeComponent_->isVisible())
                    welcomeComponent_->setBounds(area);
                if (coachPanel_ != nullptr && coachPanel_->isVisible())
                    coachPanel_->setBounds(area);
                return; // Salir de resized() completamente
            }
            // ═══ NO crossfade activo: decidir qué mostrar según la escena ═══
            if (scene == SceneId::Welcome && welcomeComponent_ && welcomeComponent_->isVisible()) {
                // Modo Welcome: ocultar coachPanel
                if (coachPanel_) coachPanel_->setVisible(false);
                welcomeComponent_->setBounds(area);
                welcomeComponent_->setVisible(true);
            } else {
                // ═══ BUG FIX #2: No estamos en Welcome scene o welcome no está visible ═══
                // Si welcome está visible pero no debería (ej: después de un crossfade
                // incompleto o race condition con minimize/restore), ocultarlo.
                if (scene != SceneId::Welcome) {
                    if (welcomeComponent_ && welcomeComponent_->isVisible())
                        welcomeComponent_->setVisible(false);
                } else {
                    // scene == Welcome pero welcome no está visible — NO ocultar coach
                    if (welcomeComponent_ && welcomeComponent_->isVisible())
                        welcomeComponent_->setBounds(area);
                }
                if (coachPanel_ && coachPanel_->isVisible()) {
                    coachPanel_->setBounds(area);
                } else if (scene != SceneId::Welcome) {
                    // Si coachPanel no es visible pero debería serlo, mostrarlo
                    // (ej: después de crossfade que dejó coach invisible)
                    coachPanel_->setBounds(area);
                    coachPanel_->setVisible(true);
                }
            }
            return;
        }

        if (focusOverlay_.isFocusActive() || focusOverlay_.isVisible()) {
            focusOverlay_.setBounds(getLocalBounds());
        }

        if (isCoachingState(coachRoomState_) || coachRoomState_ == CoachRoomState::Report) {
            tabBar_.setVisible(true);
            auto tabBarArea = area.removeFromTop(TabBarComponent::kHeight);
            tabBar_.setBounds(tabBarArea);

            // PhaseProgressBar: solo durante coaching
            phaseProgressBar_.setBounds(area.removeFromTop(PhaseProgressBar::kHeight));
            phaseProgressBar_.setVisible(true);

            // ─── ELIMINAR EL SWITCHO DE TABS CLÁSICO ────────────────────
            // Implementar Split View: Chat (60%) | Evidencia/Tools (40%)
            auto chatArea = area.removeFromLeft(area.getWidth() * 0.6f);
            auto evidenceArea = area; // El resto para analizadores

            if (coachPanel_) {
                coachPanel_->setBounds(chatArea.reduced(10)); // Aire alrededor
                coachPanel_->setVisible(true);
            }

            if (analyzersPanel_) {
                analyzersPanel_->setBounds(evidenceArea.reduced(10));
                // El panel de analizadores ahora solo es visible si hay evidencia o estamos en Tools
                bool shouldShowEvidence = (tabBar_.getActiveTab() == TabBarComponent::Tools) 
                                        || (narrativeDirector_ && narrativeDirector_->isActive());
                analyzersPanel_->setVisible(shouldShowEvidence);
            }

            if (progressScreen_) {
                progressScreen_->setBounds(evidenceArea.reduced(10));
                progressScreen_->setVisible(tabBar_.getActiveTab() == TabBarComponent::Session);
            }

            if (reportPanel_ && coachRoomState_ == CoachRoomState::Report) {
                reportPanel_->setBounds(area.reduced(20));
                reportPanel_->setVisible(true);
                reportPanel_->toFront(false);
            }
        }
    }


    // ═══════════════════════════════════════════════════════════════════════════
    //  visibilityChanged — Pausa el timer y salva estado al minimizar el plugin
    //  ═══ FIX: Detener timer al ocultar evita 60Hz CPU innecesario cuando el
    //  plugin está minimizado en el DAW. Salvar flags evita pérdida de estado
    //  (ej: session prep card desaparece tras restore).
    // ═══════════════════════════════════════════════════════════════════════════

    void NavigationShell::visibilityChanged()
    {
        if (isShowing()) {
            startTimerHz(60);
        } else {
            // ═══ Salvar estado actual ANTES de ocultar ═══
            saveCurrentUIFlags();
            stopTimer();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  mouseMove — Trackea hover sobre el botón "← Atrás" para glow sutil
    // ═══════════════════════════════════════════════════════════════════════════
    void NavigationShell::mouseMove(const juce::MouseEvent& e)
    {
        bool wasHovered = backButtonHovered_;
        backButtonHovered_ = (backButtonLabel_.isVisible()
                              && backButtonLabel_.getBounds().contains(e.getPosition()));
        if (backButtonHovered_ != wasHovered) {
            backButtonLabel_.setColour(juce::Label::textColourId,
                backButtonHovered_
                    ? MixCoachTheme::accentCyan().withAlpha(0.85f)
                    : MixCoachTheme::textMuted().withAlpha(0.6f));
            repaint();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  mouseDown — Maneja clic en el botón "← Atrás"
    // ═══════════════════════════════════════════════════════════════════════════
    void NavigationShell::mouseDown(const juce::MouseEvent& e)
    {
        if (backButtonLabel_.isVisible() && backButtonLabel_.getBounds().contains(e.getPosition())) {
            auto currentScene = sceneManager_.getCurrentScene();
            auto* coach = processorRef_.getCoachEngine();

            // ─── Coach asiente + mensaje al retroceder ───────────────────
            setAvatarNod(500);
            coachPanel_->addSystemMessage(
                "Volvamos. ¿Quieres corregir algo?");

            // ═══ Flag para silenciar el coach message en applyScene (evita duplicados) ═══
            suppressCoachMessage_ = true;

            switch (currentScene) {
                case SceneId::ModeSelection:
                    if (coach != nullptr) coach->setEngineerName({});
                    break;
                case SceneId::GenreSelection:
                    if (coach != nullptr) {
                        coach->setSetupGenre({});
                        coach->setCoachMode(CoachMode::Mix);
                    }
                    // ═══ Mostrar chips de modo (Mix/Masterizar) en vez de tarjetas ═══
                    coachPanel_->setSuggestions({});
                    coachPanel_->setShowSuggestionsOverride(false);
                    callAfterDelaySafe(200, [this]() {
                        coachPanel_->setShowModeCards(true);
                    });
                    break;
                case SceneId::ReferenceLoad:
                case SceneId::ReferenceAnalysis:
                    if (coach != nullptr) coach->clearReferences();
                    coachPanel_->setInlineReferenceDropZone(false);
                    coachPanel_->setShowSessionPrepCard(false);
                    // ═══ Mostrar chips de género en vez de grid ═══
                    coachPanel_->setSuggestions({});
                    coachPanel_->setShowSuggestionsOverride(false);
                    callAfterDelaySafe(200, [this]() {
                        coachPanel_->setSuggestions({
                            "\xF0\x9F\x8E\xB8 Rock", "\xF0\x9F\x8E\xA4 Pop",
                            "\xE2\x9A\xA1 EDM", "\xF0\x9F\x8E\xB9 House",
                            "\xF0\x9F\xA5\x81 Techno", "\xF0\x9F\x8E\xB7 Jazz",
                            "\xF0\x9F\x8E\xA7 Hip Hop", "\xF0\x9F\x8C\xB4 Reggaet\xC3\xB3n",
                            "\xF0\x9F\x8E\xBB Cl\xC3\xA1sica", "\xF0\x9F\x94\xA5 Otro"
                        });
                        coachPanel_->setShowSuggestionsOverride(true);
                    });
                    break;
                case SceneId::SetupComplete:
                    if (coach != nullptr) coach->clearReferences();
                    coachPanel_->setShowSessionPrepCard(false);
                    coachPanel_->setInlineReferenceDropZone(true);
                    break;
                case SceneId::MixMapReview:
                    coachPanel_->getMixMapComponent().clear();
                    break;
                default:
                    break;
            }
            processDirectorEvent({DirectorEvent::Type::Backward});
        }
    }

    // =======================================================================
    //  switchContent
    // =======================================================================
    void NavigationShell::switchContent(TabBarComponent::Tab tab)
    {
        if (tab != previousTab_ && coachPanel_) {
            switch (tab) {
                case TabBarComponent::Tools:   postUIEvent("·", "Abriendo analizadores..."); break;
                case TabBarComponent::Session: postUIEvent("[TREND]", "Mostrando progreso..."); break;
                default: break;
            }
        }

        if (crossfade_.active) {
            if (crossfade_.outgoing != nullptr) {
                crossfade_.outgoing->setAlpha(1.0f);
                crossfade_.outgoing->setVisible(false);
            }
            if (crossfade_.incoming != nullptr) crossfade_.incoming->setAlpha(1.0f);
            crossfade_.active = false;
            stopTimer();
        }

        juce::Component* outgoing = nullptr;
        switch (previousTab_) {
            case TabBarComponent::Coach:   outgoing = coachPanel_.get(); break;
            case TabBarComponent::Tools:   outgoing = analyzersPanel_.get(); break;
            case TabBarComponent::Session: outgoing = progressScreen_.get(); break;
            default: break;
        }
        previousTab_ = tab;

        juce::Component* incoming = nullptr;
        switch (tab) {
            case TabBarComponent::Coach:   incoming = coachPanel_.get(); break;
            case TabBarComponent::Tools:   incoming = analyzersPanel_.get(); break;
            case TabBarComponent::Session:
                incoming = progressScreen_.get();
                refreshProgress();
                break;
            default: incoming = coachPanel_.get(); break;
        }

        // Hide all panels that are NOT part of the crossfade
        // (outgoing/incoming are managed by startCrossfade/advanceCrossfade)
        juce::Component* panelsToHide[] = {
            coachPanel_.get(),
            analyzersPanel_.get(),
            progressScreen_.get(),
            reportPanel_.get()
        };
        for (auto* panel : panelsToHide) {
            if (panel != nullptr && panel != outgoing && panel != incoming) {
                panel->setVisible(false);
            }
        }

        tabBar_.setActiveTab(tab);
        auto contentArea = getLocalBounds();
        contentArea.removeFromTop(TabBarComponent::kHeight);
        if (outgoing != nullptr) outgoing->setBounds(contentArea);
        if (incoming != nullptr) incoming->setBounds(contentArea);
        startCrossfade(outgoing, incoming);
    }

    // =======================================================================
    //  updateFooterInfo
    // =======================================================================
    void NavigationShell::updateFooterInfo(const juce::String& phaseName,
                                           const juce::String& genre,
                                           const juce::String& target,
                                           const juce::String& sampleRate,
                                           int expLevel)
    {
        if (coachPanel_) {
            coachPanel_->updateFooterInfo(phaseName, genre, target, sampleRate, expLevel);
        }
    }

    // =======================================================================
    //  processCoachMessage
    // =======================================================================
    void NavigationShell::processCoachMessage(const juce::String& text)
    {
        auto result = revealManager_.processMessage(text);
        for (auto panel : result.panelsToReveal) {
            const char* pIcon = "";
            switch (panel) {
                case PanelId::Reference:  pIcon = "[REPORT]"; break;
                case PanelId::Messengers: pIcon = "[INFO]"; break;
                case PanelId::MixMap:     pIcon = "\xF0\x9F\x97\xBA"; break;                    case PanelId::Tools:      pIcon = "\xC2\xB7"; break;
                case PanelId::Session:    pIcon = "[TREND]"; break;
                case PanelId::Report:     pIcon = "[EXPORT]"; break;
                default: break;
            }
            if (pIcon[0] != '\0') postUIEvent(juce::String::fromUTF8(pIcon), "Revelando panel...");
            revealPanel(panel);
        }

        if (isCoachingState(coachRoomState_) && getActiveTab() == TabBarComponent::Coach) {
            juce::String lower = text.toLowerCase();
            
            // ═══ Gap 3: Auto-open analyzer + Plugin Suggestions según tipo de problema ═══
            bool mentionsAnalyzer = false;
            juce::String analyzerToShow; // "spectrum", "vectorscope", "crest"
            ProblemType detectedProblem = ProblemType::Unknown;
            
            // EQ / Masking / Tonal → Spectrum
            if (lower.contains("enmascaramiento") || lower.contains("masking")
                || lower.contains("eq") || (lower.contains("frecuencia") && (lower.contains("compiten") || lower.contains("pelean")))
                || lower.contains("resonancia") || lower.contains("resonance")
                || lower.contains("espectro") || lower.contains("spectrum")
                || lower.contains("arm\xC3\xB3nico") || lower.contains("harmonic")
                || (lower.contains("db") && lower.contains("hz"))) {
                mentionsAnalyzer = true;
                analyzerToShow = "spectrum";
                detectedProblem = ProblemType::Masking;
            }
            
            // Phase / Correlation → Vectorscope
            if (lower.contains("fase") || lower.contains("phase")
                || lower.contains("correlaci\xC3\xB3n") || lower.contains("correlation")
                || lower.contains("cancelaci\xC3\xB3n") || lower.contains("cancellation")
                || lower.contains("polaridad") || lower.contains("polarity")
                || lower.contains("mono comp") || lower.contains("mono")) {
                mentionsAnalyzer = true;
                analyzerToShow = "vectorscope";
            }
            
            // Dynamics / Crest → Crest meter
            if (lower.contains("crest") || lower.contains("din\xC3\xA1mica") || lower.contains("dynamics")
                || lower.contains("compresi\xC3\xB3n") || lower.contains("compresion")
                || lower.contains("compression") || lower.contains("sobrecomprimido")
                || lower.contains("demasiado din\xC3\xA1mico")) {
                mentionsAnalyzer = true;
                analyzerToShow = "crest";
                detectedProblem = ProblemType::DynamicsTooDynamic;
            }
            
            // Stereo / Width → Vectorscope (width mode)
            if (lower.contains("ancho est\xC3\xA9reo") || lower.contains("stereo width")
                || lower.contains("imagen est\xC3\xA9reo") || lower.contains("stereo image")
                || lower.contains("vectorscope") || lower.contains("vector scope")) {
                mentionsAnalyzer = true;
                analyzerToShow = "stereo";
                detectedProblem = ProblemType::Spatial;
            }
            
            // LUFS / Loudness → show crest + tooltip
            if (lower.contains("lufs") || lower.contains("loudness")
                || lower.contains("volumen") || lower.contains("peak")
                || lower.contains("true peak") || lower.contains("clipping")
                || lower.contains("clip")) {
                mentionsAnalyzer = true;
                analyzerToShow = "lufs";
            }
            
            // Audio DNA → DNA view
            if (lower.contains("dna") || lower.contains("audio dna")
                || lower.contains("huella") || lower.contains("fingerprint")) {
                mentionsAnalyzer = true;
                analyzerToShow = "dna";
            }
            
            // ═══ Acción: auto-abrir el analyzer específico ═══
            if (mentionsAnalyzer && tabBar_.isTabLocked(TabBarComponent::Tools) == false) {
                // Usar commandInterpreter para auto-abrir el analyzer específico
                if (analyzersPanel_) {
                    // Desactivar todos primero
                    analyzersPanel_->setShowDNA(false);
                    analyzersPanel_->setShowWidth(false);
                    analyzersPanel_->setShowCrest(false);
                    
                    if (analyzerToShow == "spectrum") {
                        // Spectrum es la vista por defecto - no necesita toggle
                    }
                    else if (analyzerToShow == "vectorscope") {
                        analyzersPanel_->setShowWidth(true);
                    }
                    else if (analyzerToShow == "crest" || analyzerToShow == "lufs") {
                        analyzersPanel_->setShowCrest(true);
                    }
                    else if (analyzerToShow == "stereo") {
                        analyzersPanel_->setShowWidth(true);
                    }
                    else if (analyzerToShow == "dna") {
                        analyzersPanel_->setShowDNA(true);
                    }
                    analyzersPanel_->resized();
                    analyzersPanel_->repaint();
                }
                setActiveTab(TabBarComponent::Tools);
                startAutoReturn();

                // ═══ Sprint 2 → V2: CoachingNarrativeDirector flow ═══
                // Reemplaza el manual PluginSuggestionGroup + QuickReply setup
                // con el flujo narrativo completo: evidence → explain → options → verify → celebrate
                if (coachPanel_ && detectedProblem != ProblemType::Unknown
                    && narrativeDirector_ && !narrativeDirector_->isActive())
                {
                    auto* coach = processorRef_.getCoachEngine();
                    if (coach != nullptr) {
                        auto& provider = coach->getPluginSuggestionsProvider();
                        if (provider.isReady()) {
                            auto problem = CoachingNarrativeDirector::buildFromProblemType(
                                detectedProblem, provider);
                            if (problem.isValid()) {
                                // El director maneja todo el flujo:
                                // 1. Muestra evidencia + explica el problema
                                // 2. Ofrece 3 tiers de soluciones como QuickReplies
                                // 3. Espera selección del usuario
                                // 4. Verifica + celebra + otorga XP
                                // Cancelar auto-return previo (setActiveTab(Tools) + startAutoReturn()
                                // arriba inicia un countdown de 5s que chocar\xC3\xADa con el
                                // sequencer del director, que toma >5s para completar)
                                cancelAutoReturn();
                                narrativeDirector_->startProblem(std::move(problem));
                                coachPanel_->hideQuickReplies();
                                return; // Skip rest of processCoachMessage
                            }
                        }
                    }
                }
                // Fallback: si el director est\xC3\xA1 ocupado o no pudo manejar el
                // problema, simplemente retornamos. El analyzer ya est\xC3\xA1 abierto
                // y el pr\xC3\xB3ximo ciclo del engine recoger\xC3\xA1 el problema.
                return;
            }
            
            bool mentionsProgress =
                lower.contains("progreso") || lower.contains("avance")
                || lower.contains("sesi\xC3\xB3n")
                || lower.contains("has avanzado") || lower.contains("reporte")
                || lower.contains("logros") || lower.contains("achievements");
            if (mentionsProgress && tabBar_.isTabLocked(TabBarComponent::Session) == false) {
                setActiveTab(TabBarComponent::Session);
                startAutoReturn();
            }
        }

        if (!result.tracksToHighlight.empty() && coachPanel_) {
            coachPanel_->highlightTrackMention(text);
            coachPanel_->getMessengerList().setTrackHighlights(result.tracksToHighlight);
        }
    }

    // =======================================================================
    //  startAutoReturn
    // =======================================================================
    void NavigationShell::startAutoReturn()
    {
        startAutoReturn((autoReturnTimeoutOverride_ > 0.0f) ? autoReturnTimeoutOverride_ : kAutoReturnDelay);
    }

    void NavigationShell::startAutoReturn(float delaySecs)
    {
        // ═══ GAP #4+GAP #6: No auto-return en modo coaching activo ═══════
        // Cuando el Director narrativo está activo (ejecutando un ciclo de
        // coaching), el auto-return es innecesario — el usuario está en
        // medio de un flujo guiado con split-view chat+evidencia.
        // Auto-return solo aplica cuando el usuario EXPLÍCITAMENTE abrió
        // Tools/Session desde el chat sin el Director activo.
        if (narrativeDirector_ && narrativeDirector_->isActive()) {
            LogHelper::writeToLog("[NavigationShell] Auto-return saltado: Director activo");
            return;
        }
        if (tabBar_.getActiveTab() == TabBarComponent::Coach) {
            return;  // Already on Coach tab — no auto-return needed
        }

        float delay = delaySecs;
        if (autoReturnActive_) {
            autoReturnRestartCount_++;
            if (autoReturnRestartCount_ >= kAutoReturnMaxRestarts) {
                autoReturnActive_ = false;
                autoReturnCountdown_ = 0.0f;
                autoReturnPermanentlyCancelled_ = true;
                LogHelper::writeToLog("[NavigationShell] Auto-return PERMANENTEMENTE cancelado");
                postUIEvent("...", "Auto-return cancelado");
                return;
            }
            autoReturnCountdown_ = delay;
            return;
        }
        if (autoReturnPermanentlyCancelled_) return;
        autoReturnActive_ = true;
        autoReturnCountdown_ = delay;
        startTimerHz(60);
        postUIEvent("...", "Volviendo al chat en " + juce::String(static_cast<int>(delay)) + "s...");
    }

    // ═══ Auto-switch tab: cambia de tab con auto-return incluido ═══════════
    void NavigationShell::autoSwitchTab(TabBarComponent::Tab tab, float delaySecs)
    {
        if (tab == getActiveTab()) return;  // Ya estamos en ese tab
        // Si auto-return ya está activo y contando para el mismo tab, no reiniciar
        if (autoReturnActive_ && autoReturnCountdown_ > 0.0f) return;
        setActiveTab(tab);
        if (isCoachingState(coachRoomState_)) {
            // Mostrar toast indicando que volverá automáticamente
            juce::String tabName = (tab == TabBarComponent::Tools) ? "Analizadores" : "Sesión";
            postUIEvent("...", tabName + " abiertos | Volviendo al chat en " + juce::String(static_cast<int>(delaySecs)) + "s...");
            // Poner un timeout más corto para que el auto-return se sienta rápido
            startAutoReturn(delaySecs);
        }
    }

    // =======================================================================
    //  applyScene
    // =======================================================================
    void NavigationShell::applyScene(const SceneDef& scene)
    {
        // ─── Apply CoachRoomState from SceneDef ────────────────────────
        // Solo las escenas de setup asignan coachRoomState explícitamente.
        // Las escenas de coaching/report usan Count como centinela "no cambiar".
        // Esto evita resetear el estado a Welcome durante coaching.
        if (scene.coachRoomState != CoachRoomState::Count
            && scene.coachRoomState != coachRoomState_) {
            setCoachRoomState(scene.coachRoomState);
            // ═══ Propagar el estado al CoachPanel para que su layout coincida ═══
            if (coachPanel_ && isPreFullUI(scene.coachRoomState)) {
                coachPanel_->setCoachRoomState(scene.coachRoomState);
            }
            // ═══ Persistir estado al procesador para restaurar al minimizar/abrir ═══
            processorRef_.setSavedCoachRoomState(scene.coachRoomState);
        }

        // ═══ Suppress coach message during backward transitions (evita duplicados) ═══
        if (suppressCoachMessage_) {
            suppressCoachMessage_ = false;
            // Aún así postear el SceneDef.panels y demás, pero sin mensaje del coach
        }
        else if (!scene.coachMessage.isEmpty() && coachPanel_) {
            // ═══ Personalizar saludo con el nombre del usuario ═══
            // NOTA: Durante setup (pre-FullUI) el chat est\xC3\xA1 oculto.
            // addSystemMessage agrega al historial, pero tambi\xC3\xA9n mostramos
            // el saludo como label visible via setGreetingText().
            if (scene.coachRoomState == CoachRoomState::Intention) {
                juce::String userName = "Ingeniero";
                if (auto* eng = processorRef_.getCoachEngine()) {
                    if (eng->getEngineerName().isNotEmpty())
                        userName = eng->getEngineerName();
                }
                juce::String welcomeMsg = "\xC2\xA1" "Bienvenido **" + userName + "**! " + juce::String(juce::CharPointer_UTF8("\xC2\xBF")) + juce::String(juce::CharPointer_UTF8("Qu\xC3\xA9")) + " haremos hoy?";
                coachPanel_->addSystemMessage(welcomeMsg);
                // Tambi\xC3\xA9n mostrar como label visible (sin markdown)
                {
                    juce::String plainWelcome = "\xC2\xA1" "Bienvenido " + userName + "! " + juce::String(juce::CharPointer_UTF8("\xC2\xBF")) + juce::String(juce::CharPointer_UTF8("Qu\xC3\xA9")) + " haremos hoy?";
                    coachPanel_->setGreetingText(plainWelcome);
                }
            } else if (scene.coachRoomState == CoachRoomState::Genre) {
                // ═══ Saludo personalizado para selecci\xC3\xB3n de g\xC3\xA9nero ═══
                juce::String userName = "Ingeniero";
                if (auto* eng = processorRef_.getCoachEngine()) {
                    if (eng->getEngineerName().isNotEmpty())
                        userName = eng->getEngineerName();
                }
                juce::String genreMsg = "[TARGET] " + juce::String(juce::CharPointer_UTF8("\xC2\xA1")) + "Excelente **" + userName + "**! " + juce::String(juce::CharPointer_UTF8("\xC2\xBF")) + "Qu" + juce::String(juce::CharPointer_UTF8("\xC3\xA9")) + " g" + juce::String(juce::CharPointer_UTF8("\xC3\xA9")) + "nero vas a mezclar? Elige uno.";
                coachPanel_->addSystemMessage(genreMsg);
                {
                    juce::String plainGenreMsg = "[TARGET] " + juce::String(juce::CharPointer_UTF8("\xC2\xA1")) + "Excelente " + userName + "! " + juce::String(juce::CharPointer_UTF8("\xC2\xBF")) + "Qu" + juce::String(juce::CharPointer_UTF8("\xC3\xA9")) + " g" + juce::String(juce::CharPointer_UTF8("\xC3\xA9")) + "nero vas a mezclar? Elige uno.";
                    coachPanel_->setGreetingText(plainGenreMsg);
                }
            } else {
                coachPanel_->addSystemMessage(scene.coachMessage);
            }
        }
        // ═══ Referencia: saludo personalizado (coachMessage vac\xC3\xADo en SceneDef) ═══
        // ReferenceLoad entra con coachMessage vac\xC3\xADo porque el mensaje real
        // se postea desde el timer de 400ms en onGenreCardSelected. Pero aqu\xC3\xAD
        // aseguramos que el label de saludo se muestre inmediatamente.
        if (scene.coachRoomState == CoachRoomState::ReferenceStage && coachPanel_) {
            juce::String userName = "Ingeniero";
            juce::String genreName = "tu g" + juce::String(juce::CharPointer_UTF8("\xC3\xA9")) + "nero";
            if (auto* eng = processorRef_.getCoachEngine()) {
                if (eng->getEngineerName().isNotEmpty())
                    userName = eng->getEngineerName();
                if (eng->getSetupGenre().isNotEmpty())
                    genreName = eng->getSetupGenre();
            }
            // Obtener caracter\xC3\xADstica breve del g\xC3\xA9nero para el saludo
            juce::String trait = "su sonido caracter" + juce::String(juce::CharPointer_UTF8("\xC3\xAD")) + "stico";
            {
                auto g = genreName.trim().toLowerCase();
                if (g == "afrobeat" || g == "afrobeats" || g == "world")
                    trait = "su groove r" + juce::String(juce::CharPointer_UTF8("\xC3\xAD")) + "tmico y calidez";
                else if (g == "reggaeton" || g == "reggaeton/latin" || g == "latin" || g == "dembow")
                    trait = "su 808 profundo y dembow";
                else if (g == "trap")
                    trait = "su 808 masivo y din" + juce::String(juce::CharPointer_UTF8("\xC3\xA1")) + "mica agresiva";
                else if (g == "hiphop" || g == "hip-hop" || g == "rap")
                    trait = "su groove pesado y presencia vocal";
                else if (g == "pop")
                    trait = "su claridad y brillo vocal";
                else if (g == "rock")
                    trait = "su energ" + juce::String(juce::CharPointer_UTF8("\xC3\xAD")) + "a y punch de bater" + juce::String(juce::CharPointer_UTF8("\xC3\xAD")) + "a";
                else if (g == "edm" || g == "electronic" || g == "house" || g == "techno"
                         || g == "trance" || g == "dubstep")
                    trait = "su espectro lleno y din" + juce::String(juce::CharPointer_UTF8("\xC3\xA1")) + "mica controlada";
                else if (g == "jazz")
                    trait = "su calidez anal" + juce::String(juce::CharPointer_UTF8("\xC3\xB3")) + "gica y rango din" + juce::String(juce::CharPointer_UTF8("\xC3xA1")) + "mico natural";
                else if (g == "rnb" || g == "r&b" || g == "rb" || g == "soul")
                    trait = "su voz sedosa y graves aterciopelados";
            }
            juce::String refMsg = "[REPORT] " + juce::String(juce::CharPointer_UTF8("\xC2\xA1")) + "Excelente **" + userName + "**! " + genreName + " suena genial. Por " + trait + " hoy vamos a sonar igual.\n\n" + juce::String(juce::CharPointer_UTF8("\xC2\xBF")) + "Tienes una referencia? " + juce::String(juce::CharPointer_UTF8("\xC2\xA1")) + "C" + juce::String(juce::CharPointer_UTF8("\xC3\xA1")) + "rgala o pega un enlace!\n\nPuedo analizar archivos **WAV/MP3** o **enlaces de YouTube/Spotify**.";
            coachPanel_->addSystemMessage(refMsg);
            {
                juce::String plainRefMsg = "[REPORT] " + juce::String(juce::CharPointer_UTF8("\xC2\xA1")) + "Excelente " + userName + "! " + genreName + " suena genial. Por " + trait + " hoy vamos a sonar igual.\n\n" + juce::String(juce::CharPointer_UTF8("\xC2\xBF")) + "Tienes una referencia? " + juce::String(juce::CharPointer_UTF8("\xC2\xA1")) + "C" + juce::String(juce::CharPointer_UTF8("\xC3\xA1")) + "rgala o pega un enlace!\n\nPuedo analizar archivos WAV/MP3 o enlaces de YouTube/Spotify.";
                coachPanel_->setGreetingText(plainRefMsg);
            }
        }
        for (const auto& panelInfo : scene.panels) {
            juce::Component* panelComp = getPanelComponent(panelInfo.id);
            if (panelComp == nullptr) continue;
            bool isUnrevealed = !revealManager_.isPanelRevealed(panelInfo.id);
            if (isUnrevealed && panelInfo.visibility != PanelVisibility::Hidden) {
                revealPanel(panelInfo.id);
                panelComp->setVisible(true);
                if (panelInfo.visibility == PanelVisibility::Focused)
                    panelComp->toFront(false);
                if (panelInfo.resetOnShow)
                    resetPanelState(panelInfo.id, panelInfo.context);
                continue;
            }
            switch (panelInfo.visibility) {
                case PanelVisibility::Hidden: panelComp->setVisible(false); break;
                case PanelVisibility::Visible:
                    panelComp->setVisible(true);
                    panelComp->setAlpha(1.0f);
                    panelComp->toBack();
                    break;
                case PanelVisibility::Focused:
                    panelComp->setVisible(true);
                    panelComp->setAlpha(1.0f);
                    panelComp->toFront(false);
                    break;
            }
            if (panelInfo.resetOnShow && panelInfo.visibility != PanelVisibility::Hidden)
                resetPanelState(panelInfo.id, panelInfo.context);
        }
        if (scene.sidebarEnabled) {
            tabBar_.setVisible(true);
            if (tabBarFadeInActive_ == false && tabBar_.getAlpha() < 1.0f)
                tabBar_.setAlpha(1.0f);
        } else {
            tabBar_.setVisible(false);
        }
        if (scene.autoReturnTimeout > 0.0f) {
            autoReturnTimeoutOverride_ = scene.autoReturnTimeout;
            startAutoReturn();
        } else {
            autoReturnTimeoutOverride_ = 0.0f;
            cancelAutoReturn();
        }
        if (scene.celebrate && !scene.achievement.isEmpty())
            experienceManager_.celebrate(scene.achievement);
        repaint();
    }

    // =======================================================================
    //  getPanelComponent
    // =======================================================================
    juce::Component* NavigationShell::getPanelComponent(PanelId panelId) const noexcept
    {
        switch (panelId) {
            case PanelId::Coach:      return coachPanel_.get();
            case PanelId::Reference:  return coachPanel_ ? &coachPanel_->getRefPanel() : nullptr;
            case PanelId::Messengers: return coachPanel_ ? &coachPanel_->getMessengerList() : nullptr;
            case PanelId::MixMap:     return coachPanel_ ? coachPanel_->getPanelComponent(PanelId::MixMap) : nullptr;
            case PanelId::Tools:      return analyzersPanel_.get();
            case PanelId::Session:    return progressScreen_.get();
            case PanelId::Report:     return reportPanel_.get();
            default:                  return nullptr;
        }
    }

    // =======================================================================
    //  resetPanelState
    // =======================================================================
    void NavigationShell::resetPanelState(PanelId panelId, const juce::String& context)
    {
        switch (panelId) {
            case PanelId::Reference:
                if (coachPanel_) coachPanel_->getRefPanel().updateMatchData(DifferenceProfile{});
                break;
            case PanelId::Messengers:
                if (coachPanel_) coachPanel_->getMessengerList().setTrackHighlights({});
                break;
            default: break;
        }
        juce::ignoreUnused(context);
    }

    // =======================================================================
    //  cancelAutoReturn / postUIEvent / setAvatarExpression (duplicates removed)
    // =======================================================================
    void NavigationShell::cancelAutoReturn()
    {
        if (!autoReturnActive_) return;
        autoReturnActive_ = false;
        autoReturnCountdown_ = 0.0f;
        autoReturnRestartCount_ = 0;
    }

void NavigationShell::postUIEvent(const juce::String& icon, const juce::String& message)
{
    // ─── Mostrar toast flotante temporal (1s visible + 0.5s fade-out) ──
    toastLabel_.setText(icon + " " + message, juce::dontSendNotification);
    toastLabel_.setSize(juce::jmin(360, getWidth() - 40), 34);
    toastLabel_.setTopLeftPosition(getWidth() / 2 - toastLabel_.getWidth() / 2, 80);
    toastLabel_.setAlpha(1.0f);
    toastLabel_.setVisible(true);
    toastLabel_.toFront(true);
    toastState_.active = true;
    toastState_.framesVisible = 0;
    toastState_.alpha = 1.0f;
    toastState_.fadingOut = false;

    // Tambien agregar como mensaje del sistema en el chat
    if (coachPanel_) coachPanel_->addSystemMessage(icon + " " + message);
}

    void NavigationShell::setAvatarExpression(AvatarExpression exp)
    {
        if (coachPanel_) coachPanel_->setAvatarExpression(exp);
    }

    void NavigationShell::setAvatarWave(bool active)
    {
        if (coachPanel_) coachPanel_->setAvatarWave(active);
    }

    void NavigationShell::setAvatarNod(int64_t durationMs)
    {
        if (coachPanel_) coachPanel_->setAvatarNod(durationMs);
    }

    void NavigationShell::setAvatarPoint()
    {
        if (coachPanel_) coachPanel_->setAvatarPoint();
    }

    // =======================================================================
    //  setSpectrumHighlight — Propaga highlight al SpectrographComponent
    // =======================================================================
    void NavigationShell::setSpectrumHighlight(float freqHz, const juce::String& label)
    {
        if (freqHz <= 0.0f) {
            if (analyzersPanel_)
                analyzersPanel_->getSpectrograph().clearHighlight();
            return;
        }

        if (analyzersPanel_) {
            // Use setZoomHighlight! This zooms into ±1.5 octaves around the problem frequency
            // AND shows the glowing highlight band with label.
            analyzersPanel_->getSpectrograph().setZoomHighlight(
                freqHz, label, 1.5f);
        }

        // Tambi\xC3\xA9n propagar al EvidencePanel (mini-visor contextual)
        if (coachPanel_) {
            auto& evidence = coachPanel_->getEvidencePanel();
            evidence.highlightedFreq_ = freqHz;
            evidence.highlightLabel_ = label;
        }
    }

    // =======================================================================
    //  scanUserPlugins — Escanea plugins VST3 y actualiza sugerencias
    // =======================================================================
    void NavigationShell::scanUserPlugins()
    {
        // ═══ Flag anti-duplicados: solo escanear una vez por sesión ═══════
        if (pluginsScanned_) return;
        pluginsScanned_ = true;

        // ─── Escanear plugins VST3 del sistema ────────────────────────────
        PluginScanner scanner;
        auto* coach = processorRef_.getCoachEngine();
        if (coach == nullptr) { pluginsScanned_ = false; return; }

        auto& suggestionsProvider = coach->getPluginSuggestionsProvider();

        // Conectar callback para inyectar plugins detectados en el suggestions provider
        scanner.onPluginDetected = [&suggestionsProvider](const juce::String& pluginId) {
            suggestionsProvider.addKnownPlugin(pluginId);
        };

        // Ejecutar escaneo (síncrono)
        int scanned = scanner.scanAll();

        // ═══ Incremento 3d: Poblar PluginConfirmationPanel con resultados ═══
        {
            auto& db = PluginDatabase::getInstance();

            // Colectar nombres de plugins detectados
            std::vector<juce::String> allPluginNames;
            for (const auto& id : scanner.getDetectedPluginIds()) {
                if (const auto* entry = db.getById(id))
                    allPluginNames.push_back(entry->name);
            }
            for (const auto& name : scanner.getUnknownPlugins())
                allPluginNames.push_back(name);

            // Colectar pistas activas: (slotIndex, trackName, emoji)
            std::vector<std::tuple<int, juce::String, juce::String>> slotData;
            static constexpr const char* kEmojis[] = {
                "[DRUM]", "\xF0\x9F\x94\x8A", "[MIC]",
                "\xC2\xB7", "\xF0\x9F\x94\xB9", "\xF0\x9F\xA5\x82",
                "\xF0\x9F\x93\x9D", "\xF0\x9F\x94\xAD"
            };
            int eIdx = 0;
            auto& registry = sharedData_.getSlotRegistry();
            registry.forEachActive([&](const SlotInfo& info) {
                juce::String name(info.trackName);
                if (name.trim().isEmpty())
                    name = "Pista " + juce::String(info.slotIndex + 1);
                slotData.emplace_back(info.slotIndex, name.trim(),
                                      juce::String(kEmojis[eIdx % 8]));
                eIdx++;
            });

            // Poblar y mostrar panel si hay datos
            if (!allPluginNames.empty() && !slotData.empty()) {
                auto& pp = coachPanel_->getPluginConfirmationPanel();
                pp.populate(slotData, allPluginNames);

                pp.onConfirm = [this](const std::unordered_map<int, juce::String>& confirmed) {
                    int count = 0;
                    auto* eng = processorRef_.getCoachEngine();
                    if (!eng) return;
                    auto& sp = eng->getPluginSuggestionsProvider();
                    auto& db = PluginDatabase::getInstance();
                    for (const auto& [slot, pn] : confirmed) {
                        bool found = false;
                        for (const auto& e : db.getAllPlugins()) {
                            if (e.name == pn) { sp.addKnownPlugin(e.id); found = true; break; }
                        }
                        if (!found) sp.addKnownPlugin(pn);
                        count++;
                    }
                    if (count > 0) {
                        coachPanel_->addSystemMessage(
                            "OK **" + juce::String(count)
                            + " inserts confirmados.** Mis sugerencias ahora priorizar\xC3\xA1n estos plugins.");
                        setAvatarNod(400);
                    }
                };

                pp.onDismiss = [this]() {
                    LogHelper::writeToLog("[NavigationShell] Plugin confirmation dismissed");
                };

                int panelH = juce::jmin(400, 40 + (int)slotData.size() * 32);
                pp.setBounds(coachPanel_->getLocalBounds()
                    .withSizeKeepingCentre(420, panelH));
                pp.setVisible(true);
                pp.toFront(true);
            }
        }

        // Mostrar resumen en el chat si hay resultados
        if (scanned > 0) {
            juce::String summary = scanner.buildSummary();
            if (summary.isNotEmpty() && coachPanel_)
                coachPanel_->addSystemMessage(summary);
        } else if (coachPanel_) {
            coachPanel_->addSystemMessage(
                "[SEARCH] No se encontraron plugins VST3 instalados en los directorios "
                "est\xC3\xA1ndar. Puedes seguir usando los plugins nativos de tu DAW "
                "y las recomendaciones gratuitas.");
        }
    }

    // =======================================================================
    //  revealPanel
    // =======================================================================
    void NavigationShell::revealPanel(PanelId panelId)
    {
        // ═══ Progressive Revelation Guard ════════════════════════════════════
        // No revelar paneles que no estén permitidos en el scope actual.
        // El scope se setea en setCoachRoomState() según la fase.
        if (!revealManager_.validateReveal(revealManager_.getCurrentScope(), panelId)) {
            LogHelper::writeToLog("[ProgressiveReveal] DENIED reveal of "
                                  + juce::String(panelIdLabel(panelId))
                                  + " in scope "
                                  + juce::String(analysisScopeLabel(revealManager_.getCurrentScope())));
            return;
        }

        bool wasFirstReveal = !revealManager_.isPanelRevealed(panelId);
        revealManager_.markPanelRevealed(panelId);
        if (!wasFirstReveal) return;
        if (onRevealPanel) onRevealPanel(panelId);
        tabBar_.triggerMenuNewBadge();
        switch (panelId) {
            case PanelId::Reference:
                if (coachRoomState_ == CoachRoomState::Welcome
                    || coachRoomState_ == CoachRoomState::Intention
                    || coachRoomState_ == CoachRoomState::Genre)
                    setCoachRoomState(CoachRoomState::ReferenceStage);
                break;
            case PanelId::Messengers:
                if (coachRoomState_ == CoachRoomState::Welcome
                    || coachRoomState_ == CoachRoomState::Intention
                    || coachRoomState_ == CoachRoomState::Genre
                    || coachRoomState_ == CoachRoomState::ReferenceStage)
                    setCoachRoomState(CoachRoomState::MessengerStage);
                break;
            case PanelId::MixMap:
                if (coachRoomState_ == CoachRoomState::MessengerStage)
                    setCoachRoomState(CoachRoomState::MixMapStage);
                break;
            case PanelId::Tools:
                if (sceneManager_.getCurrentSceneDef().sidebarEnabled) {
                    tabBar_.setTabLocked(TabBarComponent::Tools, false);
                    tabBar_.triggerNewBadge(TabBarComponent::Tools);
                }
                break;
            case PanelId::Session:
                if (sceneManager_.getCurrentSceneDef().sidebarEnabled) {
                    tabBar_.setTabLocked(TabBarComponent::Session, false);
                    tabBar_.triggerNewBadge(TabBarComponent::Session);
                }
                break;
            default: break;
        }
        repaint();
    }

    // =======================================================================
    //  updateAllPanels
    // =======================================================================
    void NavigationShell::updateAllPanels(SlotRegistry& registry, SharedData& sharedData, double sampleRate)
    {
        try {
            auto* coach = processorRef_.getCoachEngine();
            int activeCount = registry.activeCount();

            if (coachRoomState_ == CoachRoomState::MessengerStage && coachPanel_) {
                std::vector<juce::String> trackNames;
                registry.forEachActive([&](const SlotInfo& info) {
                    trackNames.push_back(juce::String(info.trackName));
                });
                coachPanel_->setInlineMessengerStatus(
                    activeCount > 0 || messengerTracksDetected_, activeCount, trackNames);
            }

            if (coachRoomState_ == CoachRoomState::MessengerStage
                && activeCount > 0 && !messengerTracksDetected_) {
                messengerTracksDetected_ = true;

                // ═══ P5: Mostrar SessionScanCard con animación de escaneo ═══
                if (coachPanel_) {
                    std::vector<juce::String> trackNames;
                    registry.forEachActive([&](const SlotInfo& info) {
                        trackNames.push_back(juce::String(info.trackName));
                    });
                    coachPanel_->showSessionScanCard(activeCount, trackNames);
                    coachPanel_->onSessionScanComplete = [this, activeCount]() {
                        // Al completar escaneo, avanzar al MixMap
                        if (coachPanel_) coachPanel_->hideSessionScanCard();
                        postUIEvent("\xF0\x9F\x97\xBA", "Generando mapa de sesi\xC3\xB3n...");
                        revealPanel(PanelId::MixMap);
                        if (coachPanel_) {
                            coachPanel_->addSystemMessage(
                                "\xF0\x9F\x94\x8E **" + juce::String(activeCount)
                                + (activeCount == 1 ? " pista" : " pistas")
                                + " detectada" + (activeCount == 1 ? "" : "s") + "!**\n\n"
                                "Veo que has insertado los Messengers. "
                                "Voy a construir el **mapa de sesi\xC3\xB3n**.\n\n"
                                "Revisa que el ruteo sea correcto y conf\xC3\xADrmalo para empezar.");
                            coachPanel_->showPanelMixMap();
                            coachPanel_->setSuggestions({"Confirmar mapa"});
                            coachPanel_->setShowSuggestionsOverride(true);
                        }
                    };
                }

                // Solo limpiar estado inline; el mensaje y MixMap se muestran
                // al completar el escaneo animado (via onSessionScanComplete).
                if (coachPanel_) coachPanel_->setInlineMessengerStatus(false, 0, {});
            }

            std::vector<juce::String> trackNames;
            registry.forEachActive([&](const SlotInfo& info) {
                trackNames.push_back(juce::String(info.trackName));
            });
            revealManager_.updateTrackNames(trackNames);

            if (coach != nullptr && coach->hasPendingConfirmation()) {
                if (!coachPanel_->hasQuickReplies())
                    coachPanel_->showQuickReplies({"S\xC3\xAD", "No", "Otro..."});
            } else {
                if (coachPanel_ && coachPanel_->hasQuickReplies())
                    coachPanel_->hideQuickReplies();
            }

            if (coachPanel_) {
                coachPanel_->updateMessengers(registry, sharedData);

                // ═══ Live meter data → CoachingGuideWidget (si visible) ═══
                auto& guide = coachPanel_->getCoachingGuide();
                if (guide.isVisible()) {
                    CoachingGuideWidget::LiveMeterData meterData;

                    // Master analysis del AudioAnalyzer
                    auto& analyzer = processorRef_.getAudioAnalyzer();
                    const auto& master = analyzer.getMasterAnalysis();
                    meterData.peakDb = master.getPeak();
                    meterData.rmsDb = master.getRMS();
                    meterData.correlation = master.getCorrelation();
                    meterData.stereoWidth = analyzer.getAvgStereoWidth();
                    meterData.activeTrackCount = registry.activeCount();
                    meterData.totalTrackCount = registry.totalSlots();

                    // Crest factor: promedio de per-band si disponible
                    float crestSum = 0.0f;
                    int crestCount = 0;
                    registry.forEachActive([&](const SlotInfo& info) {
                        auto result = sharedData.getTrackAudioResult(info.slotIndex);
                        for (int b = 0; b < 6; ++b) {
                            if (result.crestPerBand[b] > 0.01f) {
                                crestSum += result.crestPerBand[b];
                                crestCount++;
                            }
                        }
                    });
                    if (crestCount > 0)
                        meterData.crestFactor = crestSum / (float)crestCount;

                    // Spectral centroid usando hi-res FFT (16384-FFT, 8192 bins, ~2.93Hz resolution)
                    const float* hiResSpec = master.getHiResSpectrum();
                    if (hiResSpec != nullptr) {
                        float num = 0.0f, den = 0.0f;
                        int numBins = master.getHiResNumBins(); // 8192
                        for (int b = 0; b < numBins; ++b) {
                            float freq = b * (float)sampleRate / (2.0f * (float)numBins);
                            num += hiResSpec[b] * freq;
                            den += hiResSpec[b];
                        }
                        if (den > 1e-12f)
                            meterData.spectralCentroidHz = num / den;
                    }

                    guide.setLiveMeterData(meterData);
                }
            }

            if (analyzersPanel_->isVisible()) {
                analyzersPanel_->updateAnalyzers(sampleRate);
                analyzersPanel_->updateAudioDNA(registry, sharedData);
            }
        }
        catch (const std::exception& e) {
            juce::Logger::outputDebugString("[NavigationShell::updateAllPanels] Exception: "
                                            + juce::String(e.what()));
        }
    }

    // =======================================================================
    //  smoothAnalyzersPanel
    // =======================================================================
    void NavigationShell::smoothAnalyzersPanel(double sampleRateHz)
    {
        if (analyzersPanel_) analyzersPanel_->smoothVisuals(sampleRateHz);
    }

    // =======================================================================
    //  mentorPhaseToCoachRoomState
    // =======================================================================
    static CoachRoomState mentorPhaseToCoachRoomState(MentorPhase phase) noexcept
    {
        switch (phase) {
            case MentorPhase::Organizacion: return CoachRoomState::GainStaging;
            case MentorPhase::GainStaging:  return CoachRoomState::GainStaging;
            case MentorPhase::Balance:      return CoachRoomState::Balance;
            case MentorPhase::EQ:           return CoachRoomState::EQ;
            case MentorPhase::Compresion:   return CoachRoomState::Compression;
            case MentorPhase::Espacio:      return CoachRoomState::Space;
            case MentorPhase::MasterCheck:  return CoachRoomState::MasterCheck;
            default:                        return CoachRoomState::GainStaging;
        }
    }

    // =======================================================================
    //  setCoachRoomState
    // =======================================================================
    void NavigationShell::setCoachRoomState(CoachRoomState newState)
    {
        if (coachRoomState_ == newState) return;

        auto oldState = coachRoomState_;
        coachRoomState_ = newState;

        // ═══ Progressive Revelation: setear scope según la fase ════════════
        if (newState == CoachRoomState::Report) {
            revealManager_.setCurrentScope(AnalysisScope::Report);
        } else if (isCoachingState(newState)) {
            revealManager_.setCurrentScope(AnalysisScope::Coaching);
        } else {
            revealManager_.setCurrentScope(AnalysisScope::Setup);
        }

        if (isPreFullUI(oldState) && !isPreFullUI(newState) && newState != CoachRoomState::Report) {
            tabBar_.setVisible(true);
            tabBar_.setAlpha(0.0f);
            tabBarFadeInActive_ = true;
            tabBarFadeInProgress_ = 0.0f;
            startTimerHz(60);
            if (coachPanel_) {
                coachPanel_->setWelcomeMode(false);
                coachPanel_->setShowCoachingGuide(true);
                // Fase 8: Delegado al Director (elimina duplicación MentorPhase→CoachingStage)
                if (narrativeDirector_)
                    narrativeDirector_->updateCoachingGuide();
                auto area = getLocalBounds();
                auto tabBarArea = area.removeFromTop(TabBarComponent::kHeight);
                tabBar_.setBounds(tabBarArea);
                coachPanel_->setBounds(area);
                coachPanel_->setVisible(true);
            }
            // ═══ Sprint 4: Notificar al SceneManager sobre el cambio de fase ═══
            DirectorEvent phaseEv;
            phaseEv.type = DirectorEvent::Type::PhaseChanged;
            phaseEv.numericValue = static_cast<float>(newState);
            phaseEv.timestamp = juce::Time::getMillisecondCounter() * 1000;
            processDirectorEvent(phaseEv);

            tabBar_.setActiveTab(TabBarComponent::Coach);
            updateTabLockState();
            // ═══ Sprint 3: Phase panel visibility ═════════════════════
            if (coachPanel_) {
                // ─── Al entrar a coaching, ocultar TODOS los paneles de setup ──
                // Solo el chat + el panel activo deben ser visibles.
                // El Coach dicta que se ve. La UI obedece.
                if (isCoachingState(newState)) {
                    coachPanel_->setShowModeCards(false);
                    coachPanel_->setShowGenreCards(false);
                    coachPanel_->setShowSessionPrepCard(false);
                    coachPanel_->setInlineReferenceDropZone(false);
                    coachPanel_->setInlineMessengerStatus(false, 0, {});
                }

                // ═══ GAP #4: Delegado al Director (applyPhaseUI + populateGainStaging) ═══
                if (narrativeDirector_) {
                    narrativeDirector_->applyPhaseUI(newState);
                    if (newState == CoachRoomState::GainStaging) {
                        auto* coachG = processorRef_.getCoachEngine();
                        if (coachG != nullptr)
                            narrativeDirector_->populateGainStaging(*coachG);
                    }
                }
            }
            repaint();
            return;
        }

        // ═══ Sprint 3: Phase panel visibility (catch-all para transiciones preFullUI→coaching)
        //     MixMapStage→GainStaging no pasa por el bloque `!isPreFullUI(oldState)` porque
        //     MixMapStage es preFullUI. Esta guardia catch-all asegura visibilidad correcta
        //     para TODAS las transiciones hacia un coaching state.
        if (isCoachingState(newState) && coachPanel_) {
            coachPanel_->getGainStagingPanel().setVisible(newState == CoachRoomState::GainStaging);
            coachPanel_->getEQPanel().setVisible(newState == CoachRoomState::EQ);
            coachPanel_->getMasterCheckPanel().setVisible(
                newState == CoachRoomState::MasterCheck || newState == CoachRoomState::Automation);
            // Gap #N: Auto-open analyzer + 2-track overlay en EQ
            if (newState == CoachRoomState::EQ && coachPanel_ && analyzersPanel_) {
                // Evidence already shows in split-view panel — no Tools tab switch needed
                cancelAutoReturn();
                analyzersPanel_->setShowDNA(false);
                analyzersPanel_->setShowWidth(false);
                analyzersPanel_->setShowCrest(false);
                analyzersPanel_->resized();
                analyzersPanel_->repaint();
                postUIEvent("·", "Analizando espectro de pistas...");

                // Poblar EQPanel con 2 tracks conflictivos
                if (auto* coach = processorRef_.getCoachEngine()) {
                    auto& registry = sharedData_.getSlotRegistry();
                    std::vector<std::pair<int, float>> candidates;
                    registry.forEachActive([&](const SlotInfo& info) {
                        auto advice = coach->analyzeTrackTonal(info.slotIndex);
                        if (advice.isActionable())
                            candidates.push_back({info.slotIndex, std::abs(advice.worstDeviation)});
                    });
                    std::sort(candidates.begin(), candidates.end(),
                        [](const auto& a, const auto& b) { return a.second > b.second; });

                    if (candidates.size() >= 2) {
                        auto adv1 = coach->analyzeTrackTonal(candidates[0].first);
                        auto adv2 = coach->analyzeTrackTonal(candidates[1].first);
                        EQTrackData p, s;
                        auto fill = [](EQTrackData& d, const CoachEngine::TrackTonalAdvice& a) {
                            d.slotIndex = a.slotIndex; d.trackName = a.trackName;
                            for (int r = 0; r < 6; ++r) {
                                d.currentEnergy[r] = a.regionEnergy[r];
                                d.targetEnergy[r] = a.regionExpected[r];
                            }
                            d.hasTarget = true;
                        };
                        fill(p, adv1); fill(s, adv2);
                        coachPanel_->getEQPanel().setTrackPair(p, s);
                        float freqHz[] = {60.0f, 200.0f, 600.0f, 2000.0f, 5000.0f, 12000.0f};
                        float hz = (adv1.worstRegion >= 0 && adv1.worstRegion < 6) ? freqHz[adv1.worstRegion] : 1000.0f;
                        analyzersPanel_->getSpectrograph().setHighlightFrequency(hz, hz*0.5f, p.trackName + " vs " + s.trackName);
                        coachPanel_->addSystemMessage(juce::String("\xF0\x9F\x94\x8E **Enmascaramiento** \"") + p.trackName + "\" vs \"" + s.trackName + "\" — " + juce::String(std::abs(adv1.worstDeviation),1) + " dB en " + juce::String(adv1.kRegionName(adv1.worstRegion)));

                        // Gap #N: TrackProblemCard inline para masking
                        if (coachPanel_ && coach) {
                            auto groups = buildTrackProblemGroups(*coach);
                            for (const auto& grp : groups)
                                coachPanel_->addTrackGroupCard(grp);
                        }
                    } else if (candidates.size() >= 1) {
                        auto adv1 = coach->analyzeTrackTonal(candidates[0].first);
                        EQTrackData single;
                        single.slotIndex = adv1.slotIndex;
                        single.trackName = adv1.trackName;
                        for (int r = 0; r < 6; ++r) {
                            single.currentEnergy[r] = adv1.regionEnergy[r];
                            single.targetEnergy[r] = adv1.regionExpected[r];
                        }
                        single.hasTarget = true;
                        coachPanel_->getEQPanel().setTrackData(single);
                    }
                }
            }

            coachPanel_->getCompressionPanel().setVisible(newState == CoachRoomState::Compression);
            coachPanel_->getSpacePanel().setVisible(newState == CoachRoomState::Space);

            // Limpiar overlay de correlación al salir de Space
            if (oldState == CoachRoomState::Space && newState != CoachRoomState::Space && analyzersPanel_) {
                analyzersPanel_->getPhaseScopePanel().getVectorscope().clearTargetCorrelation();
            }

            // Auto-open vectorscope al entrar en Space
            if (newState == CoachRoomState::Space && analyzersPanel_) {
                // Evidence already shows in split-view panel — no Tools tab switch needed
                cancelAutoReturn();
                analyzersPanel_->setShowDNA(false);
                analyzersPanel_->setShowWidth(true);  // Vectorscope + width meter
                analyzersPanel_->setShowCrest(false);
                // Set target correlation ideal para mostrar overlay
                analyzersPanel_->getPhaseScopePanel().getVectorscope().setTargetCorrelation(
                    0.85f, "Correlaci\xC3\xB3n objetivo: 0.85");
                analyzersPanel_->resized();
                analyzersPanel_->repaint();
                postUIEvent("\xF0\x9F\x94\xAE", "Abriendo vectorscope para visualizar imagen est\xC3\xA9reo...");
            }

            coachPanel_->getMasterCheckPanel().setVisible(newState == CoachRoomState::MasterCheck);

            // Gap #2: Space -- Pregunta interactiva al entrar
            if (newState == CoachRoomState::Space && coachPanel_ && !inSpaceQuestion_) {
                inSpaceQuestion_ = true;
                auto prevReplyCb = coachPanel_->getQuickReplyBar().onReplySelected;
                coachPanel_->getQuickReplyBar().onReplySelected = [this, prevReplyCb](const juce::String& reply) {
                    // Guard: si ya no estamos en Space, delegar al callback original
                    if (getCoachRoomState() != CoachRoomState::Space) {
                        coachPanel_->hideQuickReplies();
                        coachPanel_->getQuickReplyBar().onReplySelected = prevReplyCb;
                        if (prevReplyCb) prevReplyCb(reply);
                        return;
                    }
                    coachPanel_->hideQuickReplies();
                    coachPanel_->getQuickReplyBar().onReplySelected = prevReplyCb;
                    juce::String lower = reply.trim().toLowerCase();
                    float preDelay = 40.0f, decay = 1.8f, mixPct = 25.0f;
                    juce::String reverbType = "Room";
                    juce::String msg;
                    if (lower == "voz" || lower == "vocals") {
                        preDelay = 40.0f; decay = 1.8f; mixPct = 25.0f; reverbType = "Room";
                        msg = "🎤 **Voz:** Reverb de sala, pre-delay 40ms, decay 1.8s, envío al 25%";
                    } else if (lower == "guitarras" || lower == "guitars") {
                        preDelay = 30.0f; decay = 2.2f; mixPct = 20.0f; reverbType = "Hall";
                        msg = "🎸 **Guitarras:** Reverb de hall, pre-delay 30ms, decay 2.2s, envío al 20%";
                    } else if (lower == "batería" || lower == "bateria" || lower == "drums") {
                        preDelay = 20.0f; decay = 1.5f; mixPct = 15.0f; reverbType = "Plate";
                        msg = "🥁 **Batería:** Reverb de plate, pre-delay 20ms, decay 1.5s, envío al 15%";
                    } else {
                        msg = "🌍 **Todos los instrumentos:** Aplicando sugerencias de espacio personalizadas...";
                    }
                    coachPanel_->addSystemMessage(msg);
                    // Plugin suggestions 3 tiers for reverb
                    {
                        juce::String sugMsg;
                        sugMsg << juce::String("Sugerencias de plugins de reverb:\n")
                               << juce::String("\xf0\x9f\x94\xb9 **Nativo:** Fruity Reeverb 2 (FL Studio) \xe2\x86\x92 pre-delay ")
                               << juce::String(preDelay, 0) << juce::String("ms, decay ")
                               << juce::String(decay, 1) << juce::String("s\n")
                               << juce::String("\xf0\x9f\x9f\xa2 **Gratis:** Valhalla Supermassive (valhalladsp.com) \xe2\x86\x92 ")
                               << reverbType << juce::String(" pre-delay ")
                               << juce::String(preDelay, 0) << juce::String("ms\n")
                               << juce::String("\xf0\x9f\x9f\xa1 **Premium:** Valhalla VintageVerb (\u20AC50) \xe2\x86\x92 ")
                               << reverbType << juce::String(" decay ")
                               << juce::String(decay, 1) << juce::String("s");
                        coachPanel_->addSystemMessage(sugMsg);
                    }
                    // Build SpacePanel rows matching selected instrument roles
                    std::vector<SpaceTrackRow> spaceRows;
                    if (auto* coachSP = processorRef_.getCoachEngine()) {
                        auto trackRoles = coachSP->getTrackRoles();
                        bool matchAll = (lower == "todos" || lower == "all");
                        for (int s = 0; s < (int)trackRoles.size(); ++s) {
                            auto role = trackRoles[s];
                            bool matches = matchAll;
                            if (!matches) {
                                juce::String rn(getRoleName(role));
                                matches = (lower.startsWith("voz") && (rn.containsIgnoreCase("voz") || rn.containsIgnoreCase("vocal") || role == TrackRole::VozPrincipal))
                                       || (lower.startsWith("guitar") && (rn.containsIgnoreCase("guitar") || rn.containsIgnoreCase("guit")))
                                       || ((lower.startsWith("bater") || lower == "drums") && (rn.containsIgnoreCase("kick") || rn.containsIgnoreCase("snare") || rn.containsIgnoreCase("drum") || rn.containsIgnoreCase("hihat") || rn.containsIgnoreCase("cymbal") || rn.containsIgnoreCase("tom") || rn.containsIgnoreCase("percu")));
                            }
                            if (matches) {
                                SpaceTrackRow row;
                                row.slotIndex = s;
                                auto info = sharedData_.getSlotRegistry().getSlotInfo(s);
                                row.trackName = juce::String(info.trackName);
                                row.roleName = juce::String(getRoleName(role));
                                auto tr = sharedData_.getTrackAudioResult(s);
                                row.correlation = tr.correlation;
                                row.stereoWidth = 0.5f;
                                row.suggestedPreDelayMs = preDelay;
                                row.suggestedDecaySec = decay;
                                row.suggestedMixPct = mixPct;
                                row.reverbType = reverbType;
                                row.isApplied = false;
                                spaceRows.push_back(row);
                            }
                        }
                    }
                    if (spaceRows.empty()) {
                        SpaceTrackRow fallback;
                        fallback.slotIndex = -1;
                        fallback.trackName = "Track seleccionado";
                        fallback.roleName = lower;
                        fallback.correlation = 0.95f;
                        fallback.stereoWidth = 0.5f;
                        fallback.suggestedPreDelayMs = preDelay;
                        fallback.suggestedDecaySec = decay;
                        fallback.suggestedMixPct = mixPct;
                        fallback.reverbType = reverbType;
                        fallback.isApplied = false;
                        spaceRows.push_back(fallback);
                    }
                    coachPanel_->getSpacePanel().setTrackData(spaceRows);

                    // ═══ Wire onApplyReverb callback (con guard contra re-wiring) ══
                    // Solo cablear una vez para evitar duplicación de callbacks
                    if (!reverbWired_) {
                        reverbWired_ = true;
                        coachPanel_->getSpacePanel().onApplyReverb =
                            [this](int slotIndex, float preDelayMs, float decaySec,
                                   float mixPct, const juce::String& reverbType) {
                                // Obtener correlación actual antes de la aplicación
                                float beforeCorr = sharedData_.getTrackAudioResult(slotIndex).correlation;
                                // Si no hay señal (sentinel -1.0f), usar fallback por defecto
                                if (beforeCorr <= -0.99f) beforeCorr = 0.95f;

                                // Estimar correlación después: más reverb = menos correlación
                                float mixFactor = juce::jlimit(0.0f, 1.0f, mixPct / 100.0f);
                                float afterCorr = juce::jlimit(0.0f, 1.0f, beforeCorr - (0.17f * mixFactor));

                                // Mensaje del robot con el cambio de correlación
                                coachPanel_->addSystemMessage(
                                    juce::String::formatted(
                                        "OK **Reverb aplicado: %s**\n\n"
                                        "Pre-delay: %.0fms | Decay: %.1fs | Mix: %.0f%%\n\n"
                                        "\xF0\x9F\x94\xAE **Correlaci\xF3n:** %.2f [RIGHT] %.2f. "
                                        "\xC2\xA1La mezcla ahora tiene profundidad!",
                                        reverbType.toRawUTF8(),
                                        preDelayMs, decaySec, mixPct,
                                        beforeCorr, afterCorr));

                                // Registrar plugin aplicado
                                auto* eng = processorRef_.getCoachEngine();
                                if (eng != nullptr) {
                                    juce::String pluginName = "Reverb: " + reverbType
                                        + " (Pre=" + juce::String(preDelayMs, 0) + "ms"
                                        + ", Decay=" + juce::String(decaySec, 1) + "s"
                                        + ", Mix=" + juce::String(mixPct, 0) + "%)";
                                    eng->recordAppliedPlugin(pluginName);
                                }

                                setAvatarExpression(AvatarExpression::Happy);
                                setAvatarNod(400);
                            };
                    }

                    setAvatarExpression(AvatarExpression::Happy);
                    setAvatarNod(500);
                };
                                                coachPanel_->addSystemMessage(
                    juce::String("\xf0\x9f\x8c\x8a **La mezcla est\xc3\xa1 seca. Vamos a darle profundidad.**\n\n")
                    + juce::String("\xc2\xbfQu\xc3\xa9 instrumentos quieres procesar con espacio?"));

                coachPanel_->showQuickReplies({"Voz", "Guitarras", "Batería", "Todos"});
                setAvatarExpression(AvatarExpression::Thinking);
            }
            if (newState != CoachRoomState::Space) {
                inSpaceQuestion_ = false;
            }
        }

        if (isCoachingState(oldState) && newState == CoachRoomState::Report) {
            // ═══ P8: Robot orgulloso al mostrar reporte final ═══════════
            // Solo expresión (el panel se oculta, no tiene sentido el wave)
            setAvatarExpression(AvatarExpression::Happy);
            setAvatarWave(false); // Asegurar limpieza si venía wave activo

            tabBar_.setVisible(false);
            if (coachPanel_) coachPanel_->setVisible(false);
            if (analyzersPanel_) analyzersPanel_->setVisible(false);
            if (reportPanel_) {
                reportPanel_->setBounds(getLocalBounds());
                reportPanel_->setVisible(true);
                refreshReport();
            }
            updateTabLockState();
            repaint();
            return;
        }

        if (oldState == CoachRoomState::Report && newState == CoachRoomState::Welcome) {
            // ═══ Limpiar expresión/wave del robot al salir del reporte ══
            setAvatarExpression(AvatarExpression::Neutral);
            setAvatarWave(false);

            tabBar_.setVisible(false);
            tabBar_.setAlpha(1.0f);
            tabBarFadeInActive_ = false;
            if (coachPanel_) {
                coachPanel_->setWelcomeMode(true);
                coachPanel_->setVisible(true);
            }
            if (reportPanel_) reportPanel_->setVisible(false);
            if (analyzersPanel_) analyzersPanel_->setVisible(false);
            messengerTracksDetected_ = false;
            if (coachPanel_) coachPanel_->setInlineMessengerStatus(false, 0, {});
            updateTabLockState();
            resized();
            repaint();
            return;
        }

        if (isCoachingState(oldState) && isCoachingState(newState)) {
            bool isMasterCheckPhase = (newState == CoachRoomState::MasterCheck);
            const char* phaseIcon = "";
            switch (newState) {
                case CoachRoomState::GainStaging: phaseIcon = "[CHANGE]"; break;
                case CoachRoomState::Balance:     phaseIcon = "\xE2\x9A\x96"; break;
                case CoachRoomState::EQ:          phaseIcon = "\xC2\xB7"; break;
                case CoachRoomState::Compression:  phaseIcon = "\xF0\x9F\x94\x8A"; break;
                case CoachRoomState::Space:        phaseIcon = "\xF0\x9F\x8C\x8C"; break;
                case CoachRoomState::MasterCheck:  phaseIcon = "\xC2\xB7"; break;
                default: break;
            }
            const char* phaseName = "";
            switch (newState) {
                case CoachRoomState::GainStaging: phaseName = "Gain Staging"; break;
                case CoachRoomState::Balance:     phaseName = "Balance"; break;
                case CoachRoomState::EQ:          phaseName = "EQ"; break;
                case CoachRoomState::Compression:  phaseName = "Compresi\xC3\xB3n"; break;
                case CoachRoomState::Space:        phaseName = "Espacio"; break;
                case CoachRoomState::MasterCheck:  phaseName = "Master Check"; break;
                default: break;
            }
            if (phaseIcon[0] != '\0' && phaseName[0] != '\0')
                postUIEvent(juce::String::fromUTF8(phaseIcon), "Fase iniciada: " + juce::String::fromUTF8(phaseName));

            // ═══ Sprint 2: MasterCheck Card Data Flow ═══
            if (isMasterCheckPhase && coachPanel_) {
                auto* coach = processorRef_.getCoachEngine();
                if (coach != nullptr) {
                    const auto& diffProfile = coach->getCachedDifferenceProfile();
                    if (diffProfile.valid && diffProfile.hasData()) {
                        MasterCheckCardData cardData;
                        // Match score: usar deltaScore (0.0-1.0) o spectralSimilarity
                        cardData.matchScore = juce::jmax(diffProfile.deltaScore, diffProfile.spectralSimilarity);
                        // Session duration: timestamps de SessionProgression
                        auto& prog = coach->getSessionProgression();
                        int64_t setupStart = prog.phaseStartedAtUs[
                            static_cast<int>(SessionProgression::Phase::Setup)];
                        int64_t now = juce::Time::getMillisecondCounterHiRes() * 1000;
                        if (setupStart > 0) {
                            int64_t elapsedUs = now - setupStart;
                            cardData.sessionDurationMinutes = (int)(elapsedUs / 60000000);
                        }
                        if (cardData.sessionDurationMinutes == 0) cardData.sessionDurationMinutes = 45; // default

                        // Poblar gaps desde DifferenceProfile
                        for (const auto& gap : diffProfile.domainGaps) {
                            if (gap.severity == GapSeverity::Praise) continue;
                            MasterCheckCardData::Gap cardGap;
                            cardGap.bandName = gap.metric;
                            cardGap.deviationDb = gap.gap;
                            cardGap.recommendation = gap.suggestion;
                            cardData.gaps.push_back(cardGap);
                        }

                        // Overall recommendation
                        if (diffProfile.criticalGaps > 0) {
                            cardData.overallRecommendation = "Hay "
                                + juce::String(diffProfile.criticalGaps) + " gaps cr\xC3\xADticos que corregir. "
                                + "Revisa las frecuencias Sub y Bass.";
                        } else if (diffProfile.warningGaps > 0) {
                            cardData.overallRecommendation = "La mezcla est\xC3\xA1 cerca de la referencia. "
                                + juce::String(diffProfile.warningGaps)
                                + " \xC3\xA1reas necesitan ajuste fino.";
                        } else {
                            cardData.overallRecommendation = "\xC2\xA1" "Excelente trabajo! La mezcla est\xC3\xA1 muy cerca del sonido objetivo.";
                        }

                        if (cardData.isValid()) {
                            coachPanel_->addMasterCheckCard(cardData);
                            // Mensaje del Coach acompañando la tarjeta
                            postUIEvent("·", "Match Score: "
                                        + juce::String((int)(cardData.matchScore * 100.0f)) + "%");

                            // ═══ Gap C: Diálogo interactivo post-MasterCheck ═══════════
                            // Preguntar al usuario qué hacer después del Master Check
                            // Guard contra re-entrada
                            auto* mcCoach = processorRef_.getCoachEngine();
                            if (mcCoach != nullptr && !inMasterCheckDialog_
                                && juce::String(mcCoach->getEngineerName()).isNotEmpty()) {
                                inMasterCheckDialog_ = true;
                                juce::String userName = mcCoach->getEngineerName();
                                coachPanel_->addSystemMessage(
                                    "**" + userName + "**, la mezcla est\xC3\xA1 lista.\n\n"
                                    "\xC2\xBFQuieres **masterizar aqu\xC3\xAD** para dejarla lista, "
                                    "**exportar** el master para procesarlo despu\xC3\xA9s, "
                                    "o **guardar la sesi\xC3\xB3n** y continuar luego?");

                                // Guardar callback anterior y mostrar QuickReplyBar
                                auto prev = coachPanel_->getQuickReplyBar().onReplySelected;
                                coachPanel_->getQuickReplyBar().onReplySelected =
                                    [this, prev](const juce::String& reply) {
                                        inMasterCheckDialog_ = false;
                                        coachPanel_->hideQuickReplies();
                                        coachPanel_->getQuickReplyBar().onReplySelected = prev;

                                        // Si el estado cambió mientras el diálogo estaba abierto,
                                        // no procesar la respuesta
                                        if (getCoachRoomState() != CoachRoomState::MasterCheck
                                            && getCoachRoomState() != CoachRoomState::Refinement) {
                                            return;
                                        }

                                        juce::String lower = reply.trim().toLowerCase();
                                        if (lower.contains("masterizar")) {
                                            // Cambiar modo a Masterización
                                            setAvatarExpression(AvatarExpression::Happy);
                                            setAvatarNod(400);
                                            auto* coach = processorRef_.getCoachEngine();
                                            if (coach != nullptr) {
                                                coach->setCoachMode(CoachMode::Master);
                                                coach->handleUserMessage("Quiero masterizar aqu\xC3\xAD");
                                            }
                                            postUIEvent("\xC2\xB7",
                                                        "Cambiando a modo Masterizaci\xC3\xB3n...");
                                        } else if (lower.contains("exportar")) {
                                            // Abrir el reporte para exportar
                                            setAvatarExpression(AvatarExpression::Encouraging);
                                            setAvatarNod(400);
                                            postUIEvent("[EXPORT]",
                                                        "Abriendo reporte para exportar...");
                                            if (reportPanel_) {
                                                if (getActiveTab() != TabBarComponent::Session)
                                                    setActiveTab(TabBarComponent::Session);
                                                refreshReport();
                                                if (progressScreen_) progressScreen_->setVisible(false);
                                                reportPanel_->setBounds(
                                                    getLocalBounds().withTrimmedTop(TabBarComponent::kHeight));
                                                reportPanel_->setVisible(true);
                                                reportPanel_->toFront(false);
                                            }
                                        } else if (lower.contains("guardar")) {
                                            // Guardar sesión y mostrar reporte
                                            setAvatarExpression(AvatarExpression::Happy);
                                            setAvatarWave(true);
                                            postUIEvent("\xF0\x9F\x92\xBE",
                                                        "Sesi\xC3\xB3n guardada. Abriendo reporte...");
                                            if (reportPanel_) {
                                                if (getActiveTab() != TabBarComponent::Session)
                                                    setActiveTab(TabBarComponent::Session);
                                                refreshReport();
                                                if (progressScreen_) progressScreen_->setVisible(false);
                                                reportPanel_->setBounds(
                                                    getLocalBounds().withTrimmedTop(TabBarComponent::kHeight));
                                                reportPanel_->setVisible(true);
                                                reportPanel_->toFront(false);
                                            }
                                        }
                                    };
                                coachPanel_->showQuickReplies(
                                    {"Masterizar aqu\xC3\xAD", "Exportar", "Guardar sesi\xC3\xB3n"});
                            }
                        }
                    } else {
                        // Sin DifferenceProfile válido: postear mensaje informativo
                        coachPanel_->addSystemMessage(
                            "**Master Check**\n\n"
                            "Para comparar tu mezcla con la referencia, necesito que "
                            "hayas cargado una referencia primero. "
                            "Puedes cargar una ahora en el panel de REFERENCE.");
                    }
                }
            }

            updateTabLockState();
            repaint();
            return;
        }

        // ═══ P2→P6: Content fade transition for setup→setup ═══════════
        // Solo animar si no estamos en Welcome→Intention (eso usa crossfade aparte)
        // y si el coachPanel está visible y no hay otra animación activa.
        if (isPreFullUI(oldState) && isPreFullUI(newState)
            && oldState != CoachRoomState::Welcome
            && coachPanel_ && coachPanel_->isVisible()
            && !setupFadeAnim_.active) {
            // Aplicar estado inmediatamente y ocultar panel
            coachRoomState_ = newState;
            coachPanel_->setAlpha(0.0f);
            updateTabLockState();
            resized();
            // Iniciar fade-in del nuevo contenido
            setupFadeAnim_.active = true;
            setupFadeAnim_.progress = 0.0f;
            startTimerHz(60);
            return;
        }

        // ═══ Safety: sincronizar welcomeMode_ con el estado actual ═══════
        // Si el flujo falla y nunca pasa por el bloque !isPreFullUI(oldState),
        // welcomeMode_ puede quedar true permanentemente, dejando la UI
        // en modo setup (full-width chat) aunque ya estemos en coaching.
        if (coachPanel_) {
            bool shouldBeWelcome = (newState == CoachRoomState::Welcome) || isPreFullUI(newState);
            if (coachPanel_->isWelcomeMode() != shouldBeWelcome) {
                coachPanel_->setWelcomeMode(shouldBeWelcome);
            }
        }

        // Default: update inmediato
        updateTabLockState();
        resized();
        repaint();
    }

    // =======================================================================
    //  updateTabLockState
    // =======================================================================
    void NavigationShell::updateTabLockState()
    {
        if (!sceneManager_.getCurrentSceneDef().sidebarEnabled) {
            tabBar_.setTabLocked(TabBarComponent::Coach, true);
            tabBar_.setTabLocked(TabBarComponent::Session, true);
            tabBar_.setTabLocked(TabBarComponent::Tools, true);
            return;
        }
        auto* coach = processorRef_.getCoachEngine();
        if (coach == nullptr) return;
        auto& progression = coach->getSessionProgression();
        auto& bridge      = processorRef_.getDiagnosticBridge();
        tabBar_.setTabLocked(TabBarComponent::Coach, false);
        auto currentPhase = progression.currentPhase;
        bool sessionKnown = (static_cast<int>(currentPhase) >= static_cast<int>(SessionProgression::Phase::DeepAnalysis));
        tabBar_.setTabLocked(TabBarComponent::Session, !sessionKnown);
        bool hasDiagnostics = bridge.hasDiagnostics() || bridge.hasPhaseDiagnostics();
        tabBar_.setTabLocked(TabBarComponent::Tools, !hasDiagnostics);
    }

    // =======================================================================
    //  wireReportPanel / refreshProgress / refreshReport
    // =======================================================================
    void NavigationShell::wireReportPanel()
    {
        auto* coach    = processorRef_.getCoachEngine();
        auto& analyzer = processorRef_.getAudioAnalyzer();
        reportPanel_->setCoachEngine(coach);
        reportPanel_->setAudioAnalyzer(&analyzer);
        if (coach != nullptr) reportPanel_->setGenre(coach->getSetupGenre());

        // ═══ GAP #3: Botón "Nueva sesión" → reiniciar flujo completo ═══════
        reportPanel_->onNewSession = [this]() {
            // 1. Reset CoachEngine
            auto* coach = processorRef_.getCoachEngine();
            if (coach != nullptr) {
                coach->resetAppliedPlugins();
                coach->clearMixHistory();
                coach->setEngineerName({});
            }

            // 2. Reset scene manager
            sceneManager_.resetToWelcome();

            // 3. Reset narrative director
            if (narrativeDirector_) {
                narrativeDirector_->reset();
            }

            // 4. Reset UI state
            coachRoomState_ = CoachRoomState::Welcome;

            // 5. Hide all panels, show welcome
            reportPanel_->setVisible(false);
            tabBar_.setVisible(false);
            coachPanel_->setVisible(false);
            coachPanel_->setWelcomeMode(true);
            coachPanel_->clearMessages();
            coachPanel_->setShowModeCards(false);
            coachPanel_->setShowGenreCards(false);
            coachPanel_->setShowSessionPrepCard(false);
            coachPanel_->setInlineReferenceDropZone(false);
            coachPanel_->setShowSuggestionsOverride(false);
            coachPanel_->setSuggestions({});
            coachPanel_->hideQuickReplies();
            analyzersPanel_->setVisible(false);
            progressScreen_->setVisible(false);
            phaseProgressBar_.setVisible(false);

            // 6. Show welcome component
            welcomeComponent_->setVisible(true);
            welcomeComponent_->startWelcomeAnimation();

            // 7. Reset initial score for fresh report
            if (reportPanel_) {
                reportPanel_->setState(EndOfSessionComponent::State::Empty);
            }

            // 8. Reset state variables
            inMasterCheckDialog_ = false;
            messengerTracksDetected_ = false;
            autoReturnPermanentlyCancelled_ = false;
            autoReturnRestartCount_ = 0;

            updateTabLockState();
            resized();
            repaint();

            LogHelper::writeToLog("[NavigationShell] Nueva sesión iniciada — volviendo a Welcome");

            // ═══ EventLog: nueva sesión ═══════════════════════════════════════
            if (auto* coachEv = processorRef_.getCoachEngine())
                coachEv->getEventLog().logEvent(SessionEvent::Type::CoachingStarted,
                                                "Nueva sesión desde Report");
        };
    }

    void NavigationShell::refreshProgress()
    {
        auto* coach = processorRef_.getCoachEngine();
        if (coach != nullptr && progressScreen_)
            progressScreen_->updateFromEngine(*coach);
    }

    void NavigationShell::refreshReport()
    {
        wireReportPanel();
        if (coachPanel_ != nullptr) {
            // Obtener plugins desde CoachChatComponent + CoachEngine
            juce::StringArray allPlugins = coachPanel_->getAppliedPlugins();

            // Tambien incluir plugins registrados en CoachEngine
            auto* coach = processorRef_.getCoachEngine();
            if (coach != nullptr) {
                const auto& enginePlugins = coach->getAppliedPluginNames();
                for (const auto& p : enginePlugins) {
                    if (!allPlugins.contains(p))
                        allPlugins.add(p);
                }
            }

            reportPanel_->setAppliedPlugins(allPlugins);
        }
        reportPanel_->refresh();
    }

    // =======================================================================
    //  startSetupTransition — Inicia fade-in de 300ms entre pasos del setup
    // =======================================================================
    /** Inicia la animación de fade-in para transiciones entre pantallas de setup.
        El nuevo contenido ya debe estar aplicado visualmente (cards, avatar, etc.).
        La animación lo mantiene invisible 30% del tiempo (~90ms) y luego
        lo fadea a alpha 1.0 con ease-out quad durante el 70% restante (~210ms).
        Duración total: ~300ms (18 frames a 60fps). */
    void NavigationShell::startSetupTransition()
    {
        setupFadeAnim_.active   = true;
        setupFadeAnim_.progress = 0.0f;
        if (coachPanel_)
            coachPanel_->setAlpha(0.0f);
        startTimerHz(60);
    }

    // =======================================================================
    //  timerCallback
    // =======================================================================
    void NavigationShell::timerCallback()
    {
        static int lockTick = 0;
        if (++lockTick % 60 == 0) updateTabLockState();

        if (isCoachingState(coachRoomState_) && lockTick % 30 == 0) {
            auto* phaseMgr = processorRef_.getPhaseManager();
            if (phaseMgr != nullptr) {
                auto mentorPhase = phaseMgr->getCurrentPhase();
                auto expectedState = mentorPhaseToCoachRoomState(mentorPhase);
                if (coachRoomState_ != expectedState) setCoachRoomState(expectedState);
            }
        }

        // ─── PhaseProgressBar: update dots + XP cada 15 frames ──────────
        if (lockTick % 15 == 0) {
            auto* coach = processorRef_.getCoachEngine();
            if (coach != nullptr) {
                auto& phaseMgr = coach->getPhaseManager();
                phaseProgressBar_.updateFromPhaseManager(phaseMgr);

                // NOTA: Los dots de progreso se manejan exclusivamente en
                // PhaseProgressBar (updateFromPhaseManager arriba).
                // CoachingEvidenceHost NO tiene dots duplicados.
            }

            // ═══ GAP #4: Evidence feed delegado al Director ═══════════════
            if (coachPanel_ != nullptr && coachPanel_->getEvidencePanel().isVisible()
                && narrativeDirector_ && narrativeDirector_->isActive()) {
                auto& analyzer = processorRef_.getAudioAnalyzer();
                auto* coach = processorRef_.getCoachEngine();
                if (coach != nullptr)
                    narrativeDirector_->feedEvidenceForPhase(analyzer, *coach);
            }
        }

        if (lockTick % 300 == 0) {
            auto activeTab = tabBar_.getActiveTab();
            if (activeTab == TabBarComponent::Session) refreshProgress();
        }

        if (autoReturnActive_) {
            float deltaSecs = 1.0f / 60.0f;
            autoReturnCountdown_ -= deltaSecs;
            if (autoReturnCountdown_ <= 0.0f) {
                autoReturnCountdown_ = 0.0f;
                autoReturnActive_ = false;
                autoReturnRestartCount_ = 0;
                postUIEvent("...", "De vuelta al chat");
                // Postear mensaje del sistema cuando el coach "vuelve"
                coachPanel_->addSystemMessage(
                    "**De vuelta al chat.**\n\n"
                    "Si quieres explorar algo m\xC3\xA1s, solo p\xC3\xAD" "demelo.");
                setActiveTab(TabBarComponent::Coach);
                auto sceneDef = sceneManager_.processEvent({DirectorEvent::Type::TimerTick});
                applyScene(sceneDef);
            }
        }

        // ═══ Gap #5: Detección de clipping periódica (cada ~60 ticks = 1s) ═══
        if (lockTick % 60 == 0 && isCoachingState(coachRoomState_)) {
            auto* coach = processorRef_.getCoachEngine();
            if (coach != nullptr && coachPanel_ != nullptr) {
                auto& registry = sharedData_.getSlotRegistry();
                std::vector<TrackHighlightInfo> highlights;
                int worstClippingSlot = -1;
                float worstPeak = -100.0f;

                registry.forEachActive([&](const SlotInfo& info) {
                    auto& feedCore = coach->getTrackFeedCore();
                    auto state = feedCore.getTrackState(info.slotIndex);
                    if (state.isClipping()) {
                        TrackHighlightInfo hli;
                        hli.slotIndex = info.slotIndex;
                        hli.domain = 0;
                        hli.severity = 1.0f;
                        hli.keyword = "clipping";
                        juce::String name = juce::String(info.trackName).trim();
                        hli.description = name.isEmpty()
                            ? ("Track " + juce::String(info.slotIndex + 1) + " clipeando")
                            : (name + " clipeando");
                        highlights.push_back(hli);
                        if (state.peakCombined > worstPeak) {
                            worstPeak = state.peakCombined;
                            worstClippingSlot = info.slotIndex;
                        }
                    }
                });

                // Always update highlights in messenger list
                coachPanel_->getMessengerList().setTrackHighlights(highlights);

                // Transition guard: only alert on new clipping (wasn't clipping last check)
                static bool prevHadClipping = false;
                bool nowHasClipping = !highlights.empty();

                if (nowHasClipping && !prevHadClipping) {
                    // NEW clipping detected — alert once
                    auto slotInfo = registry.getSlotInfo(worstClippingSlot);
                    juce::String trackName = juce::String(slotInfo.trackName).trim();
                    if (trackName.isEmpty()) trackName = "Track " + juce::String(worstClippingSlot + 1);
                    focusOverlay_.focusTrack(worstClippingSlot, trackName,
                                             juce::Colours::red, getLocalBounds());
                    setAvatarExpression(AvatarExpression::Serious);
                    postUIEvent("\xF0\x9F\x9A\xA8", trackName + " est\xC3\xA1 clipeando! Mira el medidor y b\xC3\xA1jale el fader.");
                } else if (!nowHasClipping && prevHadClipping) {
                    // Clipping resolved — clear overlay
                    auto focusedSlot = focusOverlay_.getFocusedSlot();
                    if (focusedSlot >= 0)
                        focusOverlay_.clearFocus(300);
                }

                prevHadClipping = nowHasClipping;
            }

            // ═══ IPC overrun detection — muestra toast si algún slot perdió datos ═══
            // Compara el overrunCount actual de cada slot activo contra el último
            // conocido. Si aumentó, significa que el ring buffer se desbordó y se
            // perdieron muestras de audio — hay congestión en el canal IPC.
            // Throttle: máximo 1 toast cada 5s para no spamear.

            // Primer ciclo: solo poblar lastKnown sin mostrar toast (evita falso
            // positivo con overruns acumulados antes de abrir la UI).
            if (!overrunInitialized_) {
                auto& reg = sharedData_.getSlotRegistry();
                reg.forEachActive([&](const SlotInfo& info) {
                    int slotIdx = info.slotIndex;
                    if (slotIdx >= 0 && slotIdx < SlotRegistry::kMaxSlots)
                        overrunLastCounts_[slotIdx] =
                            sharedData_.getAudioMemoryV2().getOverrunCount(slotIdx);
                });
                overrunInitialized_ = true;
            } else if (overrunThrottleFrames_ <= 0) {
                // Usar el primer slot con overrun para el mensaje
                int worstOverrunSlot = -1;
                uint32_t worstDelta = 0;
                auto& reg = sharedData_.getSlotRegistry();

                reg.forEachActive([&](const SlotInfo& info) {
                    int slotIdx = info.slotIndex;
                    if (slotIdx < 0 || slotIdx >= SlotRegistry::kMaxSlots) return;

                    uint32_t current = sharedData_.getAudioMemoryV2().getOverrunCount(slotIdx);
                    uint32_t lastKnown = overrunLastCounts_[slotIdx];

                    if (current > lastKnown) {
                        uint32_t delta = current - lastKnown;
                        if (delta > worstDelta) {
                            worstDelta = delta;
                            worstOverrunSlot = slotIdx;
                        }
                        overrunLastCounts_[slotIdx] = current;
                    }
                });

                if (worstOverrunSlot >= 0 && worstDelta > 0) {
                    juce::String trackName = juce::String(
                        reg.getSlotInfo(worstOverrunSlot).trackName).trim();
                    if (trackName.isEmpty())
                        trackName = "Track " + juce::String(worstOverrunSlot + 1);

                    postUIEvent("[WARN]",
                                "Congesti\xF3n IPC en " + trackName
                                + " (" + juce::String(worstDelta) + " overruns)");

                    // Throttle: 5s antes del próximo toast IPC
                    overrunThrottleFrames_ = 300; // 5s a 60fps
                }
            } else {
                overrunThrottleFrames_--;
            }
        }

        // ─── Compression analysis: cada 60 ticks en estado Compression ──────
        if (lockTick % 60 == 0 && coachRoomState_ == CoachRoomState::Compression
            && coachPanel_ && coachPanel_->getCompressionPanel().isVisible()) {
            auto* coach = processorRef_.getCoachEngine();
            if (coach != nullptr) {
                auto& registry = coach->getSharedData().getSlotRegistry();
                auto trackRoles = coach->getTrackRoles();
                auto dynamicsAdvices = analyzeAllTracksDynamics(
                    sharedData_, trackRoles, coach->getSetupGenre(), false);

                // Convertir TrackDynamicsAdvice → TrackCompressionRow
                std::vector<TrackCompressionRow> compRows;
                for (const auto& adv : dynamicsAdvices) {
                    if (!adv.isActionable()) continue;
                    TrackCompressionRow row;
                    row.slotIndex = adv.slotIndex;
                    row.trackName = adv.trackName;
                    row.roleName = juce::String(getRoleName(adv.role));
                    row.currentCrestDb = adv.currentCrest;
                    row.targetCrestDb = adv.crestTarget;
                    row.suggestedRatio = 4.0f;
                    row.suggestedAttackMs = 15.0f;
                    row.suggestedReleaseMs = 80.0f;
                    row.suggestedThresholdDb = -20.0f;
                    compRows.push_back(std::move(row));
                }

                // Siempre actualizar setTrackData (vacío = limpiar panel para evitar datos obsoletos)
                coachPanel_->getCompressionPanel().setTrackData(compRows);
            }
        }

        experienceManager_.updateAnimations();

        if (tabBarFadeInActive_) {
            tabBarFadeInProgress_ += kTabBarFadeStep;
            if (tabBarFadeInProgress_ >= 1.0f) {
                tabBarFadeInProgress_ = 1.0f;
                tabBarFadeInActive_ = false;
            }
            float t = tabBarFadeInProgress_;
            float eased = 1.0f - (1.0f - t) * (1.0f - t);
            tabBar_.setAlpha(eased);
            repaint();
        }

        advanceCrossfade();

        bool revealAnimActive = advanceRevealAnimations();

        // ═══ Avanzar smooth scroll del chat ═══════════════════════════════
        if (coachPanel_ && coachPanel_->advanceSmoothScroll())
            revealAnimActive = true;

        if (revealAnimActive) repaint();

        // ─── Avanzar animaciones de Crest Meter en CompressionPanel ────
        if (coachPanel_ && coachPanel_->getCompressionPanel().isVisible())
            coachPanel_->getCompressionPanel().advanceVisuals();

        // ═══ Setup Fade Transition (simple fade-in, estado ya aplicado) ═══
        if (setupFadeAnim_.active) {
            setupFadeAnim_.progress += SetupFadeAnim::kStep;

            if (coachPanel_) {
                if (setupFadeAnim_.progress < SetupFadeAnim::kFadeInStart) {
                    // Hold at invisible (60% de la animación)
                    coachPanel_->setAlpha(0.0f);
                } else {
                    // Fade in: alpha 0→1 con ease-out quad
                    float t = (setupFadeAnim_.progress - SetupFadeAnim::kFadeInStart)
                              / (1.0f - SetupFadeAnim::kFadeInStart);
                    float eased = t * (2.0f - t); // ease-out quad
                    coachPanel_->setAlpha(eased);
                }
            }

            if (setupFadeAnim_.progress >= 1.0f) {
                setupFadeAnim_.active = false;
                if (coachPanel_) {
                    coachPanel_->setAlpha(1.0f);
                    repaint();
                }
            }
            repaint();
        }

        // ─── Toast fade-out animation (1s visible + 0.5s fade) ────────
        if (toastState_.active) {
            toastState_.framesVisible++;
            if (!toastState_.fadingOut
                && toastState_.framesVisible > ToastState::kVisibleFrames) {
                toastState_.fadingOut = true;
            }
            if (toastState_.fadingOut) {
                int fadeElapsed = toastState_.framesVisible - ToastState::kVisibleFrames;
                float fadeProgress = (float)fadeElapsed / (float)ToastState::kFadeFrames;
                toastState_.alpha = juce::jmax(0.0f, 1.0f - fadeProgress);
                toastLabel_.setAlpha(toastState_.alpha);
                if (fadeElapsed >= ToastState::kFadeFrames) {
                    toastState_.active = false;
                    toastLabel_.setVisible(false);
                }
            }
        }

        // Stop timer if no animations active
        // ═══ FIX: Eliminamos lockTick % N de la condici\xC3\xB3n. Esas tareas peri\xC3\xB3dicas
        // ya no dependen del timer (se lanzan via callAfterDelay al inicio).
        // Sin este fix, el timer NUNCA se detiene porque cada 60 ticks (1s)
        // lockTick % 60 == 0 es true, evitando el stop.
        if (!autoReturnActive_ && !tabBarFadeInActive_ && !crossfade_.active
            && !toastState_.active && !revealAnimActive && !setupFadeAnim_.active) {
            stopTimer();
        }
    }

    // =======================================================================
    //  Crossfade helpers
    // =======================================================================
    void NavigationShell::startCrossfade(juce::Component* from, juce::Component* to)
    {
        if (from == to) {
            if (from != nullptr) {
                from->setVisible(true);
                from->setAlpha(1.0f);
            }
            return;
        }
        crossfade_.outgoing = from;
        crossfade_.incoming = to;
        crossfade_.progress = 0.0f;
        crossfade_.active = true;
        // ═══ CRITICO: mostrar AMBOS componentes para que el crossfade sea visible ═══
        // `from` debe estar visible para poder fade-out (mantener su alpha actual).
        // `to` debe estar visible (alpha 0) para poder fade-in.
        // NO forzar alpha de `from` a 1.0f porque puede estar en alpha 0 por diseño
        // (ej: WelcomeComponent ya está en alpha 0 antes del crossfade).
        if (from != nullptr) {
            from->setVisible(true);
            // NO forzar alpha a 1.0f - mantener el alpha actual
        }
        if (to != nullptr) {
            to->setVisible(true);
            to->setAlpha(0.0f);
        }
        startTimerHz(60);
    }

    bool NavigationShell::advanceCrossfade()
    {
        if (!crossfade_.active) return false;
        // FASE 8: 300ms ease-out fade entre fases (era 150ms)
        crossfade_.progress += 1.0f / (60.0f * (NarrativeTiming::kPhaseFadeMs / 1000.0f));
        if (crossfade_.progress >= 1.0f) {
            crossfade_.progress = 1.0f;
            crossfade_.active = false;
            // ═══ BUG FIX #1: Forzar estado final del incoming componente ═══
            // Garantizar que incoming esté completamente visible y opaco
            // incluso si el easedAlpha no llegó exactamente a 1.0f por
            // precisión de punto flotante.
            if (crossfade_.incoming != nullptr) {
                crossfade_.incoming->setAlpha(1.0f);
                crossfade_.incoming->setVisible(true);
            }
            // ═══ BUG FIX #1: Ocultar outgoing si no se ocultó antes ═══
            if (crossfade_.outgoing != nullptr) {
                crossfade_.outgoing->setVisible(false);
                crossfade_.outgoing->setAlpha(0.0f);
            }
        }
        else {
            // Solo actualizar alphas si el crossfade sigue activo
            float t = crossfade_.progress;
            float easedAlpha = t * (2.0f - t); // ease-out quad
            if (crossfade_.outgoing != nullptr) {
                float newAlpha = juce::jmax(0.0f, 1.0f - easedAlpha);
                crossfade_.outgoing->setAlpha(newAlpha);
                if (newAlpha <= 0.0f)
                    crossfade_.outgoing->setVisible(false);
            }
            if (crossfade_.incoming != nullptr) {
                crossfade_.incoming->setAlpha(easedAlpha);
            }
        }
        repaint();
        return crossfade_.active;
    }

    // =======================================================================
    //  Focus overlay helpers
    // =======================================================================
    void NavigationShell::showFocusOverlay(BusType busType)
    {
        if (!coachPanel_) return;
        auto& mixMap = coachPanel_->getMixMapComponent();
        auto busBounds = mixMap.getBusGroupBounds(busType);
        if (busBounds.isEmpty()) return;
        juce::String busName = juce::String(static_cast<int>(busType));
        juce::Colour busColour = MixCoachTheme::accent().withAlpha(0.3f);
        focusOverlay_.focusGroup(busType, busName, busColour, busBounds);
    }

    void NavigationShell::showFocusOverlay(int slotIndex)
    {
        if (!coachPanel_) return;

        // Obtener nombre real de la pista desde el registry
        juce::String trackName = juce::String(slotIndex);
        juce::Colour trackColour = MixCoachTheme::accent().withAlpha(0.3f);
        {
            auto& registry = sharedData_.getSlotRegistry();
            registry.forEachActive([&](const SlotInfo& info) {
                if (info.slotIndex == slotIndex) {
                    juce::String name(info.trackName);
                    if (name.isNotEmpty()) trackName = name.trim();
                    // Color dinámico según el bus
                    switch (info.bus) {
                        case BusType::Drums:   trackColour = juce::Colour(0xFFF59E0B).withAlpha(0.3f); break;
                        case BusType::Bass:    trackColour = juce::Colour(0xFF8B5CF6).withAlpha(0.3f); break;
                        case BusType::Guitars: trackColour = juce::Colour(0xFF3B82F6).withAlpha(0.3f); break;
                        case BusType::Keys:    trackColour = juce::Colour(0xFF10B981).withAlpha(0.3f); break;
                        case BusType::Vocals:  trackColour = juce::Colour(0xFFEC4899).withAlpha(0.3f); break;
                        default:              trackColour = MixCoachTheme::accent().withAlpha(0.3f); break;
                    }
                }
            });
        }

        // Si estamos en el tab Coach, usar bounds del messenger list
        if (getActiveTab() == TabBarComponent::Coach) {
            auto& messengerList = coachPanel_->getMessengerList();
            auto trackBounds = messengerList.getTrackCardBounds(slotIndex);
            if (!trackBounds.isEmpty()) {
                // Convertir coordenadas del messenger list a NavigationShell
                trackBounds = getLocalArea(&messengerList, trackBounds);
                focusOverlay_.focusTrack(slotIndex, trackName, trackColour, trackBounds);
                return;
            }
        }

        // Fallback: usar bounds del MixMap
        auto& mixMap = coachPanel_->getMixMapComponent();
        auto trackBounds = mixMap.getTrackRowBounds(slotIndex);
        if (trackBounds.isEmpty()) return;
        focusOverlay_.focusTrack(slotIndex, trackName, trackColour, trackBounds);
    }

    void NavigationShell::clearFocus()
    {
        focusOverlay_.clearFocus();
    }

    // =======================================================================
    //  WalkthroughOverlay — Tutorial interactivo (4.3)
    // =======================================================================
    void NavigationShell::showWalkthrough()
    {
        // Posicionar el overlay para que cubra todo el shell
        walkthroughOverlay_.setBounds(getLocalBounds());
        walkthroughOverlay_.toFront(true);
        walkthroughOverlay_.startWalkthrough();
        startTimerHz(60);
    }

    void NavigationShell::skipWalkthrough()
    {
        walkthroughOverlay_.skipWalkthrough();
    }

    void NavigationShell::restartWalkthrough()
    {
        walkthroughOverlay_.setBounds(getLocalBounds());
        walkthroughOverlay_.toFront(true);
        walkthroughOverlay_.restartWalkthrough();
        startTimerHz(60);
    }

    // =======================================================================
    //  Reveal animation helpers
    // =======================================================================
    bool NavigationShell::startRevealAnimation(juce::Component* component)
    {
        for (auto& anim : revealAnims_) {
            if (!anim.active) {
                anim.component = component;
                anim.progress = 0.0f;
                anim.active = true;
                if (component != nullptr) {
                    component->setAlpha(0.0f);
                    component->setTopLeftPosition(component->getX(), component->getY() + (int)PanelRevealAnim::kSlidePx);
                }
                return true;
            }
        }
        return false;
    }

    bool NavigationShell::advanceRevealAnimations()
    {
        bool anyActive = false;
        for (auto& anim : revealAnims_) {
            if (!anim.active) continue;
            anyActive = true;
            anim.progress += PanelRevealAnim::kStep;
            if (anim.progress >= 1.0f) {
                anim.progress = 1.0f;
                anim.active = false;
            }
            if (anim.component != nullptr) {
                float t = anim.progress;
                float eased = 1.0f - (1.0f - t) * (1.0f - t);
                anim.component->setAlpha(eased);
                anim.component->setTopLeftPosition(
                    anim.component->getX(),
                    anim.component->getY() - (int)(PanelRevealAnim::kSlidePx * eased));
            }
        }
        return anyActive;
    }

} // namespace mixcoach
