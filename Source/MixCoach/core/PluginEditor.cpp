#include "PluginEditor.h"
#include "../../Common/types/LogHelper.h"
#include "../engine/SemanticComparator.h"
#include "../engine/SpectralProfiler.h"
#include "../engine/AnalyzerInterpreter.h"

// earlyCrashLog definida en PluginProcessor.cpp — C puro, sin JUCE
extern void earlyCrashLog(const char* point, const char* msg);

// ═══════════════════════════════════════════════════════════════════════════
//  Helper: Convierte los resultados de SemanticComparator a BandDiagnostic
//  y los envía al DiagnosticBridge para overlay visual.
//  AHORA con Nivel 4: AnalyzerInterpreter añade interpretación semántica
//  (consecuencias y acciones) a los diagnósticos.
// ═══════════════════════════════════════════════════════════════════════════
namespace mixcoach {

    void pushDiagnosticBridge(CoachEngine& coach, DiagnosticBridge& bridge)
    {
        auto diffs = coach.runSemanticAnalysis();

        std::vector<BandDiagnostic> bandDiags;

        if (!diffs.empty()) {
            bandDiags.reserve(diffs.size() * 2);

            for (const auto& diff : diffs) {
                for (const auto& issue : diff.issues) {
                    // Only process spectral and gain issues (they map to frequency ranges)
                    if (issue.domain != SemanticIssue::Domain::Spectral && issue.domain != SemanticIssue::Domain::Gain)
                        continue;

                    BandDiagnostic diag;
                    diag.severity    = 0.5f;
                    diag.isCritical  = (issue.severity == SemanticIssue::Severity::Critical);
                    diag.isPraise    = false;
                    diag.description = issue.message;
                    diag.trackName   = issue.trackName;

                    // Map TrackRole to display name
                    if (issue.role != TrackRole::Unknown) diag.trackRole = juce::String(getRoleName(issue.role));

                    // Use exact frequency range from SemanticIssue (set by checkSpectralBalance)
                    diag.lowFreqHz  = issue.affectedLowHz;
                    diag.highFreqHz = issue.affectedHighHz;

                    // Severity from deviation (for spectral issues) or issue severity directly
                    if (issue.domain == SemanticIssue::Domain::Spectral) {
                        float absDev = std::abs(issue.deviation);
                        if (absDev > 12.0f) diag.severity = 1.0f;
                        else if (absDev > 8.0f)
                            diag.severity = 0.8f;
                        else if (absDev > 5.0f)
                            diag.severity = 0.6f;
                        else
                            diag.severity = 0.4f;
                    }
                    else if (issue.domain == SemanticIssue::Domain::Gain) {
                        diag.severity = issue.isProblem() ? 0.5f : 0.3f;
                    }
                    else {
                        // Default severity based on issue severity level
                        switch (issue.severity) {
                            case SemanticIssue::Severity::Critical:
                                diag.severity = 1.0f;
                                break;
                            case SemanticIssue::Severity::Warning:
                                diag.severity = 0.7f;
                                break;
                            case SemanticIssue::Severity::Info:
                                diag.severity = 0.4f;
                                break;
                            default:
                                diag.severity = 0.5f;
                                break;
                        }
                    }

                    if (diag.description.isNotEmpty() && diag.highFreqHz > diag.lowFreqHz)
                        bandDiags.push_back(std::move(diag));
                }

                for (const auto& praise : diff.praises) {
                    BandDiagnostic diag;
                    diag.lowFreqHz   = 20.0f;
                    diag.highFreqHz  = 20000.0f;
                    diag.severity    = 0.2f;
                    diag.isCritical  = false;
                    diag.isPraise    = true;
                    diag.description = praise.message;
                    diag.trackName   = praise.trackName;
                    if (praise.role != TrackRole::Unknown) diag.trackRole = juce::String(getRoleName(praise.role));
                    bandDiags.push_back(std::move(diag));
                }
            }
        }

        // ═══════════════════════════════════════════════════════════════════════
        //  Nivel 4 — AnalyzerInterpreter: Añade interpretaciones semánticas
        //  basadas en las lecturas actuales de los analizadores del master.
        //  Convierte datos en consecuencias: "Correlation = -0.3 → posible
        //  cancelación en mono"
        // ═══════════════════════════════════════════════════════════════════════
        std::vector<PhaseDiagnostic> phaseDiags;
        {
            // Obtener género desde el CoachEngine
            juce::String genre;
            if (coach.hasReference()) genre = coach.getReferenceGenre();
            if (genre.isEmpty()) genre = coach.getSetupGenre();

            // Obtener interpretaciones Nivel 4 desde el CoachEngine
            // (lee correlación, crest, LUFS, etc. desde AudioAnalyzer)
            auto interpretations = coach.getCurrentInterpretations(genre);

            // Convertir a BandDiagnostic y añadir a la lista
            auto analyzerDiags = AnalyzerInterpreter::toBandDiagnostics(interpretations);
            if (!analyzerDiags.empty()) {
                // Insertar al inicio (más visibles en el overlay del spectrograph)
                bandDiags.insert(bandDiags.begin(),
                                 std::make_move_iterator(analyzerDiags.begin()),
                                 std::make_move_iterator(analyzerDiags.end()));
            }

            // ═══ Phase Diagnostics: Extraer interpretaciones de Correlation/PhaseScope ═══
            // Se convierten a PhaseDiagnostic para el overlay en VectorscopeComponent
            // y PhaseCorrelationMeter. No son frequency-based como BandDiagnostic.
            for (const auto& interp : interpretations) {
                if (interp.domain != AnalyzerInterpretation::Domain::Correlation
                    && interp.domain != AnalyzerInterpretation::Domain::PhaseScope)
                    continue;

                PhaseDiagnostic pd;
                pd.severity    = interp.severity;
                pd.isWarning   = (interp.severity > 0.3f && !interp.isPraise);
                pd.isPraise    = interp.isPraise;
                pd.correlation = 1.0f; // Se sobreescribe abajo si hay lectura parseable
                pd.description = interp.interpretation;
                if (pd.description.isNotEmpty() && interp.consequence.isNotEmpty())
                    pd.description += ": " + interp.consequence;
                if (pd.description.isEmpty()) pd.description = interp.toFullMessage();

                // Intentar extraer correlación numérica desde la lectura
                juce::String reading = interp.reading.trim();
                if (reading.isNotEmpty()) {
                    // Buscar patrón: "= -0.23" o "= +0.85" o similar
                    auto eqPos = reading.lastIndexOfChar('=');
                    if (eqPos >= 0) {
                        juce::String numStr = reading.substring(eqPos + 1).trim();
                        bool ok             = false;
                        float val           = numStr.getFloatValue();
                        ok                  = (val >= -2.0f && val <= 2.0f);
                        if (ok) pd.correlation = val;
                    }
                }

                phaseDiags.push_back(pd);
            }
        }

        if (bandDiags.empty() && phaseDiags.empty()) {
            bridge.clearDiagnostics();
        }
        else {
            bridge.setAllDiagnostics(bandDiags, phaseDiags);
        }
    }

} // namespace mixcoach

namespace mixcoach {

    // ═══ Flags estáticos: persisten entre recreaciones del editor ═══════════
    // Cuando el usuario minimiza/restaura el plugin en FL Studio, el editor se
    // destruye y recrea, pero estos flags retienen su estado. Así evitamos
    // re-escanear backup files y re-anunciar tracks en cada reapertura.
    std::atomic<bool> MixCoachAudioProcessorEditor::s_initialFullSyncDone_{false};
    std::array<bool, SlotRegistry::kMaxSlots> MixCoachAudioProcessorEditor::s_announcedSlots_{};

    // ═══════════════════════════════════════════════════════════════════════════
    //  CONSTRUCTOR
    // ═══════════════════════════════════════════════════════════════════════════
    MixCoachAudioProcessorEditor::MixCoachAudioProcessorEditor(MixCoachAudioProcessor& processor,
                                                               SharedData* sharedData) :
        AudioProcessorEditor(&processor),
        processorRef_(processor),
        sharedData_(sharedData)
    {
        earlyCrashLog("EDITOR", "Editor constructor INICIO");

        setSize(960, 640);
        setResizable(true, true);
        setResizeLimits(800, 500, 1920, 1440);
        earlyCrashLog("EDITOR2", "setSize/setResizable OK");

        // ─── Placeholder invisible (solo ocupa espacio, sin texto molesto) ──
        // CRÍTICO: NUNCA llamar buildFullUI() aquí, incluso si sharedData_ está
        // disponible. La creación de MainTabbedComponent + forceFullSync() pueden
        // bloquear el message thread de FL Studio, causando timeout y crasheo.
        //
        // La UI completa se construye desde initSharedData() en el primer tick
        // del timer (~3s después en primera carga). El placeholder se mantiene
        // invisible para no molestar al usuario. Cuando buildFullUI() se ejecuta,
        // el placeholder se oculta y la UI real aparece.
        placeholderLabel_.setText({}, juce::dontSendNotification);
        placeholderLabel_.setVisible(false);
        earlyCrashLog("EDITOR3", "Placeholder/buildFullUI OK");

        earlyCrashLog("EDITOR4", "Version label removed");

        // ═══ Init header animation ────────────────────────────────────
        headerTabAnim_.setTargetValue(0.0f);

        // ═══ Timer a 60fps — Fast UI + Slow updates ─────────────────────
        // AHORA: Heavy I/O está en el background worker (backgroundRunLoop),
        // el timer SOLO hace lectura ligera de telemetry + actualización UI.
        //
        // - Cada tick (~16ms): actualiza barras de Messengers y spectrograph
        //   (solo lectura de shared memory y telemetry — operaciones rápidas)
        // - Cada 16 ticks (~266ms): full updateAllPanels (analyzers completos)
        // - Log cada ~5s
        //
        // Esto da movimiento fluido como un DAW profesional (60fps) sin
        // saturar el message thread, porque las operaciones I/O pesadas
        // están en el bg.
        // s_announcedSlots_ es estático — NO se resetea aquí. Persiste entre
        // recreaciones del editor para evitar re-anunciar tracks al restaurar.

        // ═══ SCAN INICIAL: Solo se ejecuta UNA VEZ por vida del proceso DLL ═══
        // s_initialFullSyncDone_ evita re-escanear backup files y s_announcedSlots_
        // evita re-anunciar tracks en futuras reaperturas del editor.
        // El background worker ejecuta forceFullSync + loadBackupFiles UNA SOLA
        // vez al inicio del proceso. Al minimizar/restaurar, NO se re-escanéa.
        if (!s_initialFullSyncDone_.load()) {
            bgForceSyncRequested_.store(true);
            bgBackupScanRequested_.store(true);
        }

        startTimerHz(60);

        // Registrar el tiempo de creación del editor para el log de diagnóstico
        editorCreatedMs_ = juce::Time::getMillisecondCounter();

        // ═══ Start background worker para I/O pesada ─────────────────────────
        // Ejecuta forceFullSync, loadBackupFiles, healthCheck fuera del
        // message thread para no bloquear la UI de FL Studio.
        backgroundWorker_ = std::make_unique<MixCoachBgWorker>(*this);
        backgroundWorker_->startThread();
        earlyCrashLog("EDITOR6", "Background worker iniciado OK");

        // ═══ ChangeBroadcaster Listener ═══════════════════════════════════════
        processorRef_.sharedDataChangeBroadcaster_.addChangeListener(this);
        LogHelper::writeToLog("[MixCoachEditor] ChangeBroadcaster listener registrado");
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  DESTRUCTOR
    // ═══════════════════════════════════════════════════════════════════════════
    MixCoachAudioProcessorEditor::~MixCoachAudioProcessorEditor()
    {
        // ═══ CRÍTICO: Marcar flag ANTES de cualquier cleanup ════════════════
        // Esto evita que callbacks pendientes accedan a miembros parcialmente
        // destruidos.
        editorBeingDestroyed_ = true;

        // ═══ 1. Detener BACKGROUND WORKER primero ═══════════════════════════
        // Esto asegura que no haya callbacks (onSlotChanged, etc.) ejecutándose
        // en el background thread mientras destruimos miembros.
        if (backgroundWorker_) {
            backgroundWorker_->signalThreadShouldExit();
            backgroundWorker_->notify();         // Wake up if sleeping in wait()
            backgroundWorker_->stopThread(5000); // Wait up to 5s
            backgroundWorker_ = nullptr;
        }

        // ═══ 2. Remover ChangeBroadcaster listener ═══════════════════════════
        processorRef_.sharedDataChangeBroadcaster_.removeChangeListener(this);

        // ═══ 3. Detener timer (ya no hay background thread que pueda
        //         disparar callbacks vía forceFullSync) ═══════════════════
        stopTimer();

        // ═══ 4. Limpiar callbacks del SlotRegistry ═══════════════════════════
        // Ahora es seguro porque el background thread ya se detuvo.
        if (sharedData_) {
            auto& registry            = sharedData_->getSlotRegistry();
            registry.onSlotChanged    = nullptr;
            registry.onSlotRegistered = nullptr;
            registry.onSlotReleased   = nullptr;
        }

        // ═══ 5. Desconectar callbacks del coach engine (evita dangling ref en recreate) ═══
        auto* coach = processorRef_.getCoachEngine();
        if (coach != nullptr) {
            coach->setMessagePushedCallback(nullptr);
            coach->setDiagnosticUpdateCallback(nullptr);
            coach->setEngineerNameCallback(nullptr);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  BACKGROUND RUN LOOP — Heavy I/O en hilo separado
    // ═══════════════════════════════════════════════════════════════════════════
    // Se ejecuta en MixCoachBgWorker (juce::Thread). No bloquea el message thread.
    // Realiza forceFullSync (~1s), loadSlotsFromBackupFiles (~2s), healthCheck (~5s)
    // con protección via bgLock_ (CriticalSection).
    //
    // ═══ PROTECCIÓN SEH ═══
    // backgroundRunLoop() tiene el __try/__except y llama a bgIteration().
    // bgIteration() tiene try/catch para C++ exceptions + toda la lógica.
    // Esto evita MSVC C2712 (objetos C++ con destructor no pueden estar
    // en la misma función que __try/__except).
    //


    // ─── Construir UI completa (solo cuando sharedData está disponible) ─────────
    void MixCoachAudioProcessorEditor::buildFullUI()
    {
        // ═══ Guardia antichoque ═══════════════════════════════════════════════
        if (editorBeingDestroyed_) return;

        if (fullUIBuilt_ || sharedData_ == nullptr) {
            LogHelper::writeToLog("[MixCoachEditor] buildFullUI SKIPPED: fullUIBuilt_="
                                  + juce::String(fullUIBuilt_ ? "SI" : "NO")
                                  + " sharedData_=" + (sharedData_ == nullptr ? "NULL" : "OK"));
            return;
        }

        LogHelper::writeToLog("[MixCoachEditor] ═══════ BUILD UI START ═══════");

        earlyCrashLog("BUILD", "buildFullUI START");

        placeholderLabel_.setVisible(false);

        LogHelper::writeToLog("[MixCoachEditor] Creando MainTabbedComponent...");
        earlyCrashLog("BUILD", "before new MainTabbedComponent");
        tabbedComponent_ = std::make_unique<MainTabbedComponent>(processorRef_, *sharedData_);
        earlyCrashLog("BUILD", "after new MainTabbedComponent");

        // ═══ Wire DiagnosticBridge and CoachEngine to Analyzers Panel ═══════
        auto& analyzersPanel = tabbedComponent_->getAnalyzersPanel();
        analyzersPanel.setDiagnosticBridge(&processorRef_.getDiagnosticBridge());
        if (auto* coach = processorRef_.getCoachEngine()) analyzersPanel.setCoachEngine(coach);

        addAndMakeVisible(tabbedComponent_.get());
        fullUIBuilt_ = true;

        // ═══ TIMESTAMP: registrar cuándo se construyó la UI ════════════════════
        uiBuiltTimeMs_   = juce::Time::getMillisecondCounter();
        initialSyncDone_ = false;

        // ═══ forceFullSync NO SE HACE AQUÍ ═══════════════════════════════════
        // El background worker (backgroundRunLoop) ejecuta forceFullSync cada
        // ~1s y loadSlotsFromBackupFiles cada ~2s. Los resultados se entregan
        // al timer via bgHasNewResults_ + bgForceSyncResult_ / bgBackupResult_.
        //
        // Esto evita bloquear el message thread con operaciones I/O durante
        // la creación de la UI, lo que causaba timeout de FL Studio.
        // ═══ Wire Experience Level UI ↔ AiCoachAdapter ═══════════════════
        {
            auto* adapter = processorRef_.getAiCoachAdapter();
            if (adapter != nullptr) {
                auto& coachPanel = tabbedComponent_->getCoachPanel();

                // Cuando el usuario cambia el nivel desde la UI
                coachPanel.onExperienceLevelChanged = [adapter](int levelIndex) {
                    adapter->setExperienceLevel(static_cast<AiCoachAdapter::ExperienceLevel>(levelIndex));
                };

                // Sincronizar UI con el nivel cargado desde la sesión
                coachPanel.setExperienceLevel(static_cast<int>(adapter->getExperienceLevel()));
            }
        }

        // ═══ Wire Chat Input (onMessageSent / onSuggestionClicked) ═══════
        {
            auto& coachPanel = tabbedComponent_->getCoachPanel();
            auto* coach      = processorRef_.getCoachEngine();

            coachPanel.onMessageSent = [this, &coachPanel, coach](const juce::String& text) {
                // Mostrar mensaje del usuario en el chat
                coachPanel.addUserMessage(text);

                // Enviar al motor de mentoría
                if (coach != nullptr) coach->handleUserMessage(text);
            };

            coachPanel.onSuggestionClicked = [this, &coachPanel, coach](const juce::String& text) {
                // Mostrar sugerencia clickeada como mensaje del usuario
                coachPanel.addUserMessage(text);

                // Enviar al motor de mentoría
                if (coach != nullptr) coach->handleUserMessage(text);
            };

            // ═══ Wire Ollama retry → retryOllamaConnection() ═════════
            coachPanel.onRetryOllama = [this]() { processorRef_.retryOllamaConnection(); };

            // ═══ Wire Ollama status callback → UI ═════════════════════
            processorRef_.setOllamaStatusCallback([&coachPanel](bool connected, const juce::String& modelName) {
                coachPanel.setOllamaStatus(connected, modelName);
            });

            // ═══ Poblar estado inicial de Ollama inmediatamente ═══════
            processorRef_.retryOllamaConnection();
        }

        // ═══ Wire CoachEngine::diagnosticUpdateCb_ → DiagnosticBridge (baja latencia) ═══
        // Esto reemplaza la dependencia exclusiva del timer lento (~5s) para actualizar
        // los overlays del spectrograph. Ahora el overlay se actualiza inmediatamente
        // después de handleUserMessage() y periodicAnalysis().
        {
            auto* coach = processorRef_.getCoachEngine();
            if (coach != nullptr) {
                auto& bridge = processorRef_.getDiagnosticBridge();
                coach->setDiagnosticUpdateCallback([coach, &bridge]() { pushDiagnosticBridge(*coach, bridge); });
            }
        }

        // ═══ Wire EngineerName callback (CoachEngine → AiCoachAdapter) ═══
        {
            auto* coach   = processorRef_.getCoachEngine();
            auto* adapter = processorRef_.getAiCoachAdapter();
            if (coach != nullptr && adapter != nullptr) {
                coach->setEngineerNameCallback([adapter](const juce::String& name) {
                    adapter->setEngineerName(name);
                    LogHelper::writeToLog("[PluginEditor] Ingeniero nombre guardado: " + name);
                });
            }
        }

        // ═══ Wire CoachEngine::respondWith() → Chat UI (direct callback) ═══
        {
            auto& coachPanel = tabbedComponent_->getCoachPanel();
            auto* coach      = processorRef_.getCoachEngine();
            if (coach != nullptr) {
                coach->setMessagePushedCallback([&coachPanel](const juce::String& text, bool isSystem) {
                    if (isSystem) coachPanel.addSystemMessage(text);
                    else
                        coachPanel.addMessage(text);
                });

                // ═══ Wire streaming UI callbacks ════════════════════════════
                coach->setStreamingCallbacks(
                    [&coachPanel]() {
                        // Stream started: crear burbuja vacía en el chat
                        coachPanel.startStreamingMessage();
                    },
                    [&coachPanel](const juce::String& token) {
                        // Token received: append to streaming bubble
                        coachPanel.appendStreamingToken(token);
                    },
                    [&coachPanel]() {
                        // Stream ended: finalizar burbuja
                        coachPanel.finalizeStreamingMessage();
                    });
            }
        }

        earlyCrashLog("BUILD", "UI created (sync deferred to bg worker)");
        LogHelper::writeToLog("[MixCoachEditor] forceFullSync diferido: background worker lo ejecutara en ~1s");
        LogHelper::writeToLog("[MixCoachEditor] ═══════ BUILD UI END ═══════");

        // Botón Re-scan eliminado: onRescanRequested ya no existe en AnalyzersPanelComponent.
        // El background worker maneja el sync automáticamente.

        // ═══ Callbacks ELIMINADOS ═══
        // Los callbacks onSlotChanged/onSlotRegistered/onSlotReleased llamaban
        // a updateMessengers via callAsync, causando un flood de mensajes
        // asíncronos que saturaban el message thread al minimizar/volver.
        //
        // El timer ya llama a:
        //   • smoothMeters()   → 60fps (actualiza barras, SIN lock)
        //   • updateAllPanels  → cada ~266ms (puebla lista, CON lock)
        //
        // Estos callbacks son REDUNDANTES y causaban el re-scan molesto.
        auto& registry            = sharedData_->getSlotRegistry();
        registry.onSlotChanged    = nullptr;
        registry.onSlotRegistered = nullptr;
        registry.onSlotReleased   = nullptr;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Layout y pintado
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachAudioProcessorEditor::resized()
    {
        earlyCrashLog("RESIZE", "resized called");

        if (editorBeingDestroyed_) return;

        // ═══ __try/__except: captura SEH en resize ═════════════════════
        // Cualquier AV aquí (tabbedComponent_ parcialmente construido, etc.)
        // tumba FL Studio. Lo capturamos y seguimos vivos.
        __try {
            auto area = getLocalBounds();

            // Header con tabs + branding (36px)
            auto headerBounds = area.removeFromTop(36);
            juce::ignoreUnused(headerBounds);

            if (tabbedComponent_) {
                tabbedComponent_->setBounds(area);
            }
            else {
                placeholderLabel_.setBounds(area);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            earlyCrashLog("RESIZE", "SEH capturado en resized");
        }
    }

    void MixCoachAudioProcessorEditor::paint(juce::Graphics& g)
    {
        if (editorBeingDestroyed_) return;

        auto bounds = getLocalBounds();

        // ═══════════════════════════════════════════════════════════════════════
        //  CANVAS — Deep blue-black background with subtle grid (iZotope style)
        // ═══════════════════════════════════════════════════════════════════════
        g.fillAll(MixCoachTheme::bgCanvas());

        // Subtle dot grid pattern (like NUGEN / iZotope)
        g.setColour(juce::Colour(0x06FFFFFF));
        for (int gy = 0; gy < getHeight(); gy += 32) {
            for (int gx = 0; gx < getWidth(); gx += 32) {
                g.fillRect(gx, gy, 1, 1);
            }
        }

        // ═══════════════════════════════════════════════════════════════════════
        //  HEADER BAR — Premium brand header (36px) con tabs integrados
        //  Layout: [⚡ MixCoach] [AI COACH | ANALYZERS] [VERIFICAR PROGRESO] [≡ ? ⚙]
        // ═══════════════════════════════════════════════════════════════════════
        auto headerBounds = bounds.removeFromTop(36);

        // ─── Background con glass effect premium ────────────────────────────
        {
            // Base oscura
            g.setColour(MixCoachTheme::bgDark().darker(0.88f));
            g.fillRect(headerBounds);

            // Glass highlight en la parte superior (8px)
            auto glassTop = headerBounds.withHeight(8).toFloat();
            juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.05f),
                                           glassTop.getCentreX(),
                                           glassTop.getY(),
                                           juce::Colour(0x00000000),
                                           glassTop.getCentreX(),
                                           glassTop.getBottom(),
                                           false);
            g.setGradientFill(glassGrad);
            g.fillRect(glassTop);

            // Accent glow en top-left corner
            juce::ColourGradient accentGlowGrad(MixCoachTheme::accent().withAlpha(0.07f),
                                                juce::Point<float>(0.0f, 0.0f),
                                                MixCoachTheme::accent().withAlpha(0.0f),
                                                juce::Point<float>(250.0f, 0.0f),
                                                false);
            g.setGradientFill(accentGlowGrad);
            g.fillRect(headerBounds);

            // Glass edge sutil en el borde superior
            g.setColour(juce::Colours::white.withAlpha(0.04f));
            g.drawHorizontalLine(headerBounds.getY(), 0.0f, (float)headerBounds.getWidth());
        }

        // ─── Accent underline con glow ───────────────────────────────────────
        {
            float accentLineY = (float)headerBounds.getBottom() - 1.0f;
            int headerW       = headerBounds.getWidth();

            // Shadow line (oscura, debajo)
            g.setColour(juce::Colours::black.withAlpha(0.30f));
            g.drawHorizontalLine((int)accentLineY + 1, 0.0f, (float)headerW);

            // Glow line sutil
            g.setColour(MixCoachTheme::accent().withAlpha(0.10f));
            g.drawHorizontalLine((int)accentLineY, 0.0f, (float)headerW);

            // Gradient glow en el centro del underline
            float glowCx = (float)headerW * 0.35f;
            float glowW  = (float)headerW * 0.40f;
            juce::ColourGradient underlineGrad(MixCoachTheme::accent().withAlpha(0.25f),
                                               glowCx,
                                               accentLineY,
                                               MixCoachTheme::accent().withAlpha(0.0f),
                                               glowCx + glowW * 0.5f,
                                               accentLineY,
                                               false);
            underlineGrad.addColour(0.5f, MixCoachTheme::accent().withAlpha(0.12f));
            g.setGradientFill(underlineGrad);
            g.fillRect(glowCx - glowW * 0.5f, accentLineY, glowW, 1.0f);
        }

        // ═══════════════════════════════════════════════════════════════════════
        //  LEFT: ⚡ MixCoach branding
        // ═══════════════════════════════════════════════════════════════════════
        auto headerInner = headerBounds.reduced(0, 2);

        // ─── Bolt icon con glow ──────────────────────────────────────────────
        auto leftArea = headerInner.removeFromLeft(140);
        auto iconArea = leftArea.removeFromLeft(36);

        // Glow detrás del icono
        g.setColour(MixCoachTheme::accent().withAlpha(0.06f));
        g.fillEllipse((float)(iconArea.getCentreX() - 14), (float)(iconArea.getCentreY() - 14), 28.0f, 28.0f);

        g.setFont(juce::Font(juce::FontOptions(20.0f)));
        g.setColour(MixCoachTheme::accentGlow());
        g.drawText(juce::CharPointer_UTF8("\xE2\x9A\xA1"), iconArea, juce::Justification::centred);

        leftArea.removeFromLeft(2);

        // ─── MixCoach name ───────────────────────────────────────────────────
        auto nameArea = leftArea.removeFromLeft(95);
        g.setFont(juce::Font(juce::FontOptions(15.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText("MIXCOACH", nameArea, juce::Justification::centredLeft);

        // ═══════════════════════════════════════════════════════════════════════
        //  CENTER: Custom tabs con hover glow y underline animado
        // ═══════════════════════════════════════════════════════════════════════
        auto centreArea = headerInner.removeFromLeft(280);
        int tabH        = centreArea.getHeight();

        // ─── Tab 1: AI COACH ─────────────────────────────────────────────────
        auto tab1Area      = centreArea.removeFromLeft(100);
        headerTab1Bounds_  = tab1Area;
        bool isTab1Active  = (headerActiveTab_ == 0);
        bool isTab1Hovered = (hoveredHeaderElement_ == 0);

        // Hover glow (sutil, animado implícitamente por repaint a 60fps)
        if (isTab1Hovered && !isTab1Active) {
            g.setColour(MixCoachTheme::accent().withAlpha(0.06f));
            g.fillRoundedRectangle(tab1Area.toFloat().reduced(2, 2), 4.0f);
        }

        // Active tab background
        if (isTab1Active) {
            g.setColour(juce::Colour(0x14FFFFFF));
            g.fillRoundedRectangle(tab1Area.toFloat().reduced(2, 2), 4.0f);
        }

        g.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
        g.setColour(isTab1Active    ? MixCoachTheme::accent()
                    : isTab1Hovered ? MixCoachTheme::accentDim()
                                    : MixCoachTheme::textDim());
        g.drawText("AI COACH", tab1Area.reduced(0, 4), juce::Justification::centred);

        centreArea.removeFromLeft(4);

        // ─── Tab 2: ANALYZERS ────────────────────────────────────────────────
        auto tab2Area      = centreArea.removeFromLeft(110);
        headerTab2Bounds_  = tab2Area;
        bool isTab2Active  = (headerActiveTab_ == 1);
        bool isTab2Hovered = (hoveredHeaderElement_ == 1);

        if (isTab2Hovered && !isTab2Active) {
            g.setColour(MixCoachTheme::accent().withAlpha(0.06f));
            g.fillRoundedRectangle(tab2Area.toFloat().reduced(2, 2), 4.0f);
        }

        if (isTab2Active) {
            g.setColour(juce::Colour(0x14FFFFFF));
            g.fillRoundedRectangle(tab2Area.toFloat().reduced(2, 2), 4.0f);
        }

        g.setColour(isTab2Active    ? MixCoachTheme::accent()
                    : isTab2Hovered ? MixCoachTheme::accentDim()
                                    : MixCoachTheme::textDim());
        g.drawText("ANALYZERS", tab2Area.reduced(0, 4), juce::Justification::centred);

        // ─── Animated underline (slides smoothly between tabs via SmoothValue) ──
        {
            float animPos = headerTabAnim_.getCurrent(); // 0.0 = tab1, 1.0 = tab2
            float tab1CX  = (float)(tab1Area.getX() + tab1Area.getWidth() / 2);
            float tab2CX  = (float)(tab2Area.getX() + tab2Area.getWidth() / 2);
            float cx      = tab1CX + (tab2CX - tab1CX) * animPos;

            float underW = 28.0f;
            float underY = (float)tab1Area.getBottom() - 3.0f;
            float underH = 2.5f;

            // Glow exterior
            auto glowRect = juce::Rectangle<float>(cx - underW * 0.8f, underY - 1.0f, underW * 1.6f, underH + 2.0f);
            juce::ColourGradient glow(MixCoachTheme::accent().withAlpha(0.20f),
                                      cx,
                                      underY,
                                      MixCoachTheme::accent().withAlpha(0.0f),
                                      cx + underW,
                                      underY,
                                      false);
            glow.addColour(0.5f, MixCoachTheme::accent().withAlpha(0.10f));
            g.setGradientFill(glow);
            g.fillRoundedRectangle(glowRect, 2.0f);

            // Underline core
            auto underRect = juce::Rectangle<float>(cx - underW * 0.5f, underY, underW, underH);
            g.setColour(MixCoachTheme::accent());
            g.fillRoundedRectangle(underRect, 1.5f);
        }

        // ═══════════════════════════════════════════════════════════════════════
        //  RIGHT: VERIFICAR PROGRESO button + icons
        // ═══════════════════════════════════════════════════════════════════════
        auto rightArea = headerInner.removeFromRight(300);

        // ─── Icon buttons (≡ ? ⚙) con hover glow ────────────────────────────
        auto iconsArea = rightArea.removeFromRight(90);
        int iconW      = 28;

        // Settings icon ⚙
        auto settingsIconArea     = iconsArea.removeFromRight(iconW);
        headerSettingsIconBounds_ = settingsIconArea;
        bool settingsHovered      = (hoveredHeaderElement_ == 5);
        if (settingsHovered) {
            g.setColour(MixCoachTheme::accent().withAlpha(0.08f));
            g.fillRoundedRectangle(settingsIconArea.toFloat().reduced(3, 3), 4.0f);
        }
        g.setFont(juce::Font(juce::FontOptions(14.0f)));
        g.setColour(settingsHovered ? MixCoachTheme::accentGlow() : MixCoachTheme::textMuted());
        g.drawText(juce::CharPointer_UTF8("\xE2\x9A\x99"), settingsIconArea, juce::Justification::centred);

        // Help icon ?
        auto helpIconArea     = iconsArea.removeFromRight(iconW);
        headerHelpIconBounds_ = helpIconArea;
        bool helpHovered      = (hoveredHeaderElement_ == 4);
        if (helpHovered) {
            g.setColour(MixCoachTheme::accent().withAlpha(0.08f));
            g.fillRoundedRectangle(helpIconArea.toFloat().reduced(3, 3), 4.0f);
        }
        g.setFont(juce::Font(juce::FontOptions(14.0f)).boldened());
        g.setColour(helpHovered ? MixCoachTheme::accentGlow() : MixCoachTheme::textMuted());
        g.drawText("?", helpIconArea, juce::Justification::centred);

        // Menu icon ≡
        auto menuIconArea     = iconsArea.removeFromRight(iconW);
        headerMenuIconBounds_ = menuIconArea;
        bool menuHovered      = (hoveredHeaderElement_ == 3);
        if (menuHovered) {
            g.setColour(MixCoachTheme::accent().withAlpha(0.08f));
            g.fillRoundedRectangle(menuIconArea.toFloat().reduced(3, 3), 4.0f);
        }
        g.setFont(juce::Font(juce::FontOptions(16.0f)));
        g.setColour(menuHovered ? MixCoachTheme::accentGlow() : MixCoachTheme::textMuted());
        g.drawText(juce::CharPointer_UTF8("\xE2\x89\xA1"), menuIconArea, juce::Justification::centred);

        rightArea.removeFromRight(8);

        // ─── VERIFICAR PROGRESO button con hover glow ──────────────────────
        auto verifyArea     = rightArea.removeFromRight(160);
        headerVerifyBounds_ = verifyArea;
        bool verifyHovered  = (hoveredHeaderElement_ == 2);

        // Button shadow
        auto shadowRect = verifyArea.toFloat().reduced(1, 6);
        g.setColour(juce::Colours::black.withAlpha(0.20f));
        g.fillRoundedRectangle(shadowRect.translated(0.0f, 1.5f), 5.0f);

        // Button background (gradient accent)
        juce::ColourGradient btnGrad(MixCoachTheme::accent().brighter(0.10f),
                                     shadowRect.getCentreX(),
                                     shadowRect.getY(),
                                     verifyHovered ? MixCoachTheme::accentGlow() : MixCoachTheme::accentDim(),
                                     shadowRect.getCentreX(),
                                     shadowRect.getBottom(),
                                     false);
        g.setGradientFill(btnGrad);
        g.fillRoundedRectangle(shadowRect, 5.0f);

        // Hover glow exterior
        if (verifyHovered) {
            g.setColour(MixCoachTheme::accentGlow().withAlpha(0.20f));
            g.fillRoundedRectangle(shadowRect.expanded(3.0f, 2.0f), 7.0f);
        }

        // Button glass highlight
        auto btnGlow = shadowRect.withHeight(shadowRect.getHeight() * 0.45f);
        juce::ColourGradient btnGlass(juce::Colours::white.withAlpha(0.12f),
                                      btnGlow.getCentreX(),
                                      btnGlow.getY(),
                                      juce::Colour(0x00000000),
                                      btnGlow.getCentreX(),
                                      btnGlow.getBottom(),
                                      false);
        g.setGradientFill(btnGlass);
        g.fillRoundedRectangle(btnGlow, 5.0f);

        // Button border sutil
        g.setColour(MixCoachTheme::accentGlow().withAlpha(0.15f));
        g.drawRoundedRectangle(shadowRect, 5.0f, 0.5f);

        // Button text
        g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
        g.setColour(juce::Colours::white);
        g.drawText("VERIFICAR PROGRESO", verifyArea.reduced(4, 2), juce::Justification::centred);

        // ═══════════════════════════════════════════════════════════════════════
        //  DECORATIVE: Corner accent animado (bottom-left)
        // ═══════════════════════════════════════════════════════════════════════
        auto cornerArea = juce::Rectangle<int>(0, getHeight() - 20, 50, 20).toFloat();
        juce::ColourGradient cornerGrad(MixCoachTheme::accent().withAlpha(0.05f),
                                        juce::Point<float>(0.0f, cornerArea.getBottom()),
                                        MixCoachTheme::accent().withAlpha(0.0f),
                                        juce::Point<float>(cornerArea.getRight(), cornerArea.getY()),
                                        false);
        g.setGradientFill(cornerGrad);
        g.fillRect(cornerArea);

        // Corner hardware-style accent lines
        g.setColour(MixCoachTheme::accent().withAlpha(0.06f));
        g.fillRect(0.0f, (float)getHeight() - 1.0f, 50.0f, 1.0f);
        g.fillRect(0.0f, (float)getHeight() - 20.0f, 1.0f, 20.0f);

        // Tiny accent dot at corner
        g.setColour(MixCoachTheme::accent().withAlpha(0.12f));
        g.fillEllipse(5.0f, (float)getHeight() - 15.0f, 3.0f, 3.0f);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  mouseMove — Hover tracking para header (tabs, botones, iconos)
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachAudioProcessorEditor::mouseMove(const juce::MouseEvent& e)
    {
        if (editorBeingDestroyed_) return;

        auto pos     = e.getPosition();
        int newHover = -1;

        if (headerTab1Bounds_.contains(pos)) newHover = 0;
        else if (headerTab2Bounds_.contains(pos))
            newHover = 1;
        else if (headerVerifyBounds_.contains(pos))
            newHover = 2;
        else if (headerMenuIconBounds_.contains(pos))
            newHover = 3;
        else if (headerHelpIconBounds_.contains(pos))
            newHover = 4;
        else if (headerSettingsIconBounds_.contains(pos))
            newHover = 5;

        if (newHover != hoveredHeaderElement_) {
            hoveredHeaderElement_ = newHover;
            repaint();
        }

        setMouseCursor(newHover >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  mouseDown — Manejo de clicks en el header (tabs, botones, iconos)
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
    {
        if (editorBeingDestroyed_ || !tabbedComponent_) return;

        auto pos = e.getPosition();

        // ─── Tab click: AI COACH (tab 0) ─────────────────────────────────────
        if (headerTab1Bounds_.contains(pos) && headerActiveTab_ != 0) {
            headerActiveTab_ = 0;
            headerTabAnim_.setTargetValue(0.0f);
            tabbedComponent_->setCurrentTabIndex(0);
            repaint();
            return;
        }

        // ─── Tab click: ANALYZERS (tab 1) ────────────────────────────────────
        if (headerTab2Bounds_.contains(pos) && headerActiveTab_ != 1) {
            headerActiveTab_ = 1;
            headerTabAnim_.setTargetValue(1.0f);
            tabbedComponent_->setCurrentTabIndex(1);
            repaint();
            return;
        }

        // ─── VERIFICAR PROGRESO button ───────────────────────────────────────
        if (headerVerifyBounds_.contains(pos)) {
            // Dar feedback visual inmediato en el chat
            if (tabbedComponent_) {
                auto& coachPanel = tabbedComponent_->getCoachPanel();
                coachPanel.addSystemMessage("🔄 Verificando progreso... Analizando mezcla...");
            }

            // Trigger a full re-scan + coach analysis
            if (sharedData_) {
                bgBackupScanRequested_.store(true);
                bgForceSyncRequested_.store(true);
            }

            // Notificar al coach engine para evaluar progreso
            auto* coach = processorRef_.getCoachEngine();
            if (coach != nullptr) coach->periodicAnalysis();

            repaint();
            return;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Detección de nuevos Messengers
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachAudioProcessorEditor::ensureSlotFFT()
    {
        if (slotFFTPrepared_) return;
        slotFFT_ = std::make_unique<juce::dsp::FFT>(kSlotFftOrder);
        for (int i = 0; i < kSlotFftSize; ++i)
            slotHann_[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (kSlotFftSize - 1)));
        slotFFTPrepared_ = true;
    }

    void MixCoachAudioProcessorEditor::detectNewMessengers()
    {
        if (!sharedData_ || !tabbedComponent_) return;
        auto* coach = processorRef_.getCoachEngine();

        if (!coach) return;

        int currentActiveCount = 0;

        // ═══ tryLock no-bloqueante (protegido del background worker) ═════╗
        if (bgLock_.tryEnter()) {
            auto& registry = sharedData_->getSlotRegistry();

            currentActiveCount = registry.activeCount();

            registry.forEachActive([&](const SlotInfo& info) {
                int idx = info.slotIndex;
                if (idx >= 0 && idx < SlotRegistry::kMaxSlots && !s_announcedSlots_[idx]) {
                    s_announcedSlots_[idx] = true;
                    coach->announceNewTrack(idx, juce::String(info.trackName), info.colour);
                }
            });

            if (currentActiveCount < lastActiveSlotCount_) {
                for (int i = 0; i < SlotRegistry::kMaxSlots; ++i) {
                    auto info = registry.getSlotInfo(i);
                    if (!info.active && s_announcedSlots_[i]) {
                        s_announcedSlots_[i] = false;
                    }
                }
            }

            lastActiveSlotCount_ = currentActiveCount;
            bgLock_.exit();
        }
    }

} // namespace mixcoach
