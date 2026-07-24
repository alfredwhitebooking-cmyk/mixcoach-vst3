#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../../Common/types/LogHelper.h"

#include <cstdio>
#include <ctime>
#ifdef _WIN32
#include <windows.h>
#endif

#include <cstdlib>

// ─── EarlyCrashLog — C puro, sin JUCE, para diagnosticar crashes tempranos ─
// (FUERA del namespace mixcoach para que createPluginFilter() pueda llamarla)
// NO es static para que PluginEditor.cpp también pueda usarla via extern declaration.
// Escribe a un archivo de log SIN usar ninguna API de JUCE ni C++.
// Esto permite capturar crashes que ocurren ANTES de que JUCE esté inicializado.
//
// Formato: [timestamp] [PUNTO] mensaje
//
// Puntos de logging:
//   ENTRY    — createPluginFilter()
//   CTOR     — MixCoachAudioProcessor() constructor
//   CREATE   — createEditor()
//   P2P      — prepareToPlay()
//   ESD      — ensureSharedData()
//   EDITOR   — MixCoachAudioProcessorEditor() constructor
//
// Si el crash ocurre, el ÚLTIMO mensaje en el log indica DÓNDE crasheó.
void earlyCrashLog(const char* point, const char* msg)
{
    // Usar solo C puro — fopen en modo append, fprintf, fclose
    // Ruta: %USERPROFILE%/Documents/MixCoach_Logs/MixCoach_Crash.log
    // (OJO: en Windows, GetEnvironmentVariable es la forma portable)

    char path[512];
    path[0] = '\0';

    // Intentar obtener Documents path via USERPROFILE
    const char* home = getenv("USERPROFILE");
    if (home) {
        snprintf(path, sizeof(path), "%s/Documents/MixCoach_Logs/MixCoach_Crash.log", home);
    }
    else {
        // Fallback: raiz de C:
        snprintf(path, sizeof(path), "C:/MixCoach_Crash.log");
    }

    // Crear directorio si no existe (usando Windows API directamente)
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/Documents/MixCoach_Logs", home ? home : "C:");
#ifdef _WIN32
    CreateDirectoryA(dir, nullptr); // No falla si ya existe
#endif

    FILE* f = fopen(path, "a");
    if (f) {
        // Timestamp
        time_t t           = time(nullptr);
        struct tm* tm_info = localtime(&t);
        char ts[32];
        strftime(ts, sizeof(ts), "%H:%M:%S", tm_info);

        fprintf(f, "[%s] [%s] %s\n", ts, point, msg);
        fflush(f);
        fclose(f);
    }
}

namespace mixcoach {

    // ─── WriteCrashLog (con JUCE, cuando ya está disponible) ────────────────────
    static void writeCrashLog(const juce::String& msg)
    {
        try {
            auto logFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                               .getChildFile("MixCoach_Logs")
                               .getChildFile("MixCoach_Crash.log");
            logFile.getParentDirectory().createDirectory();
            juce::FileOutputStream fos(logFile, true);
            if (fos.openedOk()) {
                fos << "[" << juce::Time::getCurrentTime().toString(true, true) << "] [BRAIN] " << msg << "\n";
                fos.flush();
            }
        }
        catch (...) {
            // writeCrashLog no puede usar LogHelper (crash path), silencio intencional
        }
    }

    // ─── Constructor: ABSOLUTAMENTE NADA que pueda crashear ─────────────────────
    // Durante el escaneo VST3 de FL Studio, el plugin se carga en un sandbox.
    // Cualquier CreateFileMapping, heap allocation grande, o file I/O puede
    // causar un structured exception (SEH). El try/catch C++ normal NO captura SEH,
    // y __try/__except no se puede usar en constructores con objetos C++.
    //
    // Por eso: todo se inicializa LAZY en ensureSharedData() / prepareToPlay().
    MixCoachAudioProcessor::MixCoachAudioProcessor() :
        AudioProcessor(BusesProperties()
                           .withInput("Input", juce::AudioChannelSet::stereo(), true)
                           .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
        earlyCrashLog("CTOR", "Constructor del procesador");
        // NADA — ni SharedData, ni LogHelper, ni archivos, ni FFT
        // sharedData_ = nullptr (por defecto)
        // phaseManager_ = nullptr (unique_ptr)
        // coachEngine_ = nullptr (unique_ptr)
        // audioAnalyzer_ = default constructible (sin heap alloc)
    }

    MixCoachAudioProcessor::~MixCoachAudioProcessor()
    {
        try {
            // ═══ Detener background service PRIMERO (ya no se necesita análisis) ═══
            bgService_.stop();

            // ═══ Guardar estado de sesión antes de destruir módulos ═══════════
            // Esto asegura que el historial del chat, género, modo, fase,
            // nivel de experiencia y configuración se persistan en
            // %LOCALAPPDATA%/MixCoach/ al cerrar FL Studio.
            // Los unique_ptrs aún están vivos aquí (se destruyen después del body).
            if (aiCoachAdapter_.get() != nullptr) aiCoachAdapter_.get()->autoSave();

            if (logStream_.get()) logStream_.get()->flush();

            LogHelper::writeToLog("[MixCoach] Plugin destruido — sesión guardada");
        }
        catch (...) {
            // No lanzar excepciones desde destructor
            MIXCOACH_LOG_CATCH("MixCoach dtor save");
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  ensureSharedData — Inicialización LAZY con retry exponencial
    // ═══════════════════════════════════════════════════════════════════════════
    //  Estrategia de reintentos:
    //    Intento 1: inmediato
    //    Intento 2: después de 500ms
    //    Intento 3: después de 1s
    //    Intento 4: después de 2s
    //    Intento 5+: cada 5s hasta éxito
    //
    //  Cada intento intenta:
    //    1. Crear el singleton SharedData (si no existe)
    //    2. Si shared memory no disponible, llamar retryInitSharedMemory()
    //    3. Si shared memory disponible, inicializar PhaseManager y CoachEngine
    //    4. ForceFullSync y notificar al editor via ChangeBroadcaster
    //
    //  Es llamada desde:
    //    - prepareToPlay() — primera oportunidad
    //    - Editor timer (initSharedData) — reintentos continuos
    //    - ChangeBroadcaster callback del editor — cuando otro evento notifica
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachAudioProcessor::ensureSharedData()
    {
        earlyCrashLog("ESD", "ensureSharedData llamado");
        // ═══ CASO 1: Ya está completamente inicializado y shared memory OK ═══
        // IMPORTANTE: Si hubo una reconexión de shared memory (isAvailable pasó de
        // false a true), aún necesitamos hacer forceFullSync para detectar slots
        // que se registraron mientras shared memory no estaba disponible.
        // Si no hay cambios pendientes, este check nos permite retornar early
        // en condiciones normales.
        if (sharedData_ != nullptr && sharedData_->isAvailable() && phaseManager_ != nullptr) {
            // Si el health check recién reconectó shared memory (fue un intento
            // de reconexión), no podemos retornar early — necesitamos forzar
            // un forceFullSync para detectar Messengers que se registraron
            // mientras estábamos desconectados.
            // Usamos el contador de intentos: si es > 1 y isAvailable es true,
            // significa que reconectamos después de fallos previos.
            if (ensureSharedDataAttempts_ <= 1) {
                return;
            }
            // Si llegamos aquí, hubo reconexión — caemos a CASO 3/4 para
            // forzar full sync sin recrear módulos.
        }

        auto nowMs = juce::Time::getMillisecondCounter();

        // ═══ Backoff exponencial ══════════════════════════════════════════════
        // Calcular el tiempo de backoff requerido basado en el número de
        // intentos REALES (no se incrementa hasta que el backoff expira).
        // Esto evita que el contador suba a 30fps desde el timer del editor.
        if (lastEnsureAttemptTimeMs_ > 0) {
            uint32_t elapsed         = nowMs - lastEnsureAttemptTimeMs_;
            uint32_t requiredBackoff = 0;
            if (ensureSharedDataAttempts_ == 0) requiredBackoff = 0;
            else if (ensureSharedDataAttempts_ == 1)
                requiredBackoff = 500; // reducido de 1000ms
            else if (ensureSharedDataAttempts_ == 2)
                requiredBackoff = 1000; // reducido de 2000ms
            else if (ensureSharedDataAttempts_ == 3)
                requiredBackoff = 2000; // reducido de 4000ms
            else
                requiredBackoff = 5000; // reducido de 10000ms

            if (elapsed < requiredBackoff) {
                return; // Aún en backoff
            }
        }

        ensureSharedDataAttempts_++;
        lastEnsureAttemptTimeMs_ = nowMs;

        // ─── Log de diagnóstico con formato solicitado ────────────────────────
        {
            juce::String errorDetail;
            if (sharedData_ != nullptr && !sharedData_->isAvailable()) {
                if (sharedData_->isSharedMemoryAvailable()) {
                    bool shmHealthy = sharedData_->getSharedMemory().healthCheck();
                    errorDetail     = shmHealthy ? "healthCheck=OK pero init=false" : "healthCheck=FAIL";
                }
                else {
                    errorDetail = "shm_=nullptr";
                }
            }
            else if (sharedData_ == nullptr) {
                errorDetail = "sharedData_ no creado aun";
            }

            juce::String logMsg = "[MixCoach] Intento de conexion #" + juce::String(ensureSharedDataAttempts_)
                                  + " | sharedData_=" + (sharedData_ == nullptr ? "NULL" : "OK")
                                  + " | isAvailable=" + (sharedData_ && sharedData_->isAvailable() ? "SI" : "NO")
                                  + " | phaseManager_=" + (phaseManager_.get() ? "OK" : "NULL");
            if (errorDetail.isNotEmpty()) {
                logMsg += " | Fallo de mapeo de memoria: [" + errorDetail + "]";
            }
            logMessage(logMsg);
            writeCrashLog(logMsg);
        }

        try {
            // ═══ CASO 2: SharedData singleton no creado aún ═══════════════════
            if (sharedData_ == nullptr) {
                sharedData_ = &SharedData::getInstance();
            }

            // ═══ CASO 3: SharedData existe pero shared memory no disponible ═══
            // Reintentar conexión a la memoria compartida. El otro plugin
            // (Messenger) puede haberla creado después de que nosotros
            // la intentáramos abrir.
            if (sharedData_ != nullptr && !sharedData_->isAvailable()) {
                bool reconnected = sharedData_->retryInitSharedMemory();
                logMessage("[MixCoach] Intento de conexion #" + juce::String(ensureSharedDataAttempts_)
                           + " | retryInitSharedMemory=" + (reconnected ? "OK" : "FAIL"));
                writeCrashLog("[MixCoach] retryInit=" + juce::String(reconnected ? "OK" : "FAIL"));

                if (!reconnected) {
                    // Log de diagnóstico adicional del estado de shared memory
                    bool shmHealth = sharedData_->isSharedMemoryAvailable();
                    juce::String errorDetail;
                    if (!shmHealth) {
                        errorDetail = "shm_=nullptr";
                    }
                    else {
                        bool shmHealthy = sharedData_->getSharedMemory().healthCheck();
                        errorDetail     = shmHealthy ? "healthCheck=OK pero init=false" : "healthCheck=FAIL";
                    }
                    logMessage("[MixCoach] Intento de conexion #" + juce::String(ensureSharedDataAttempts_)
                               + " | Fallo de mapeo de memoria: [" + errorDetail + "]");
                    return; // Reintentar en el próximo ciclo
                }

                // ═══ Sub-caso 3A: Shared memory RECONECTADA con phaseManager ya existente ═══
                // Esto ocurre cuando la shared memory se pierde temporalmente (ej: el DAW
                // reinicia el sandbox de un Messenger) y luego se recupera.
                // Necesitamos forzar un resync completo y notificar al editor.
                if (phaseManager_.get() != nullptr) {
                    logMessage("[MixCoach] Intento de conexion #" + juce::String(ensureSharedDataAttempts_)
                               + " | Shared memory RECONECTADA — forzando resync completo...");

                    auto& registry = sharedData_->getSlotRegistry();
                    int found      = registry.forceFullSync();

                    juce::String syncMsg = "[MixCoach] Sincronizacion exitosa: " + juce::String(found)
                                           + " Messenger(s) encontrado(s) post-reconexion";
                    logMessage(syncMsg);
                    writeCrashLog(syncMsg);

                    if (found > 0) {
                        registry.forEachActive([&](const SlotInfo& info) {
                            logMessage("[MixCoach]   -> Slot[" + juce::String(info.slotIndex) + "] \""
                                       + juce::String(info.trackName).trim()
                                       + "\" bus=" + juce::String(static_cast<int>(info.bus)));
                        });
                    }

                    sharedDataChangeBroadcaster_.sendChangeMessage();
                    logMessage("[MixCoach] ChangeBroadcaster enviado al editor post-reconexion");
                    return;
                }
            }

            // ═══ CASO 4: Shared memory disponible, inicializar módulos (o reinstanciar tras reconexión) ═══════
            if (sharedData_ != nullptr && sharedData_->isAvailable()) {
                // ═══ Sub-caso 4A: Módulos ya existen — solo forzar resync ═════
                // Esto ocurre cuando hubo una reconexión de shared memory (CASO 3
                // retornó early porque phaseManager_ ya existía). Necesitamos
                // forzar full sync para detectar slots que se registraron mientras
                // shared memory no estaba disponible.
                if (phaseManager_.get() != nullptr) {
                    logMessage("[MixCoach] CASO 4 con phaseManager_ existente: forzando resync post-reconexion...");

                    auto& registry = sharedData_->getSlotRegistry();
                    int found      = registry.forceFullSync();

                    if (found > 0) {
                        logMessage("[MixCoach] Resync post-reconexion: " + juce::String(found)
                                   + " Messenger(s) encontrado(s)");
                        registry.forEachActive([&](const SlotInfo& info) {
                            logMessage("[MixCoach]   -> Slot[" + juce::String(info.slotIndex) + "] \""
                                       + juce::String(info.trackName).trim()
                                       + "\" bus=" + juce::String(static_cast<int>(info.bus)));
                        });
                    }

                    sharedDataChangeBroadcaster_.sendChangeMessage();
                    logMessage("[MixCoach] ChangeBroadcaster enviado al editor (post-reconexion)");
                    return;
                }

                // ═══ Sub-caso 4B: Primera inicialización completa ═════════════

                // ─── Inicializar sesión IPC con GUID aislamiento ───────────────
                // MixCoach (brain) genera un GUID único y lo publica en la shared
                // memory de descubrimiento (SessionDiscovery). Los Messengers leen
                // este GUID para conectarse a la sesión correcta, evitando que
                // dos proyectos DAW compartan slots de forma accidental.
                if (!sessionInitialized_) {
                    juce::String guid = generateSessionGUID();
                    sessionGUID_ = guid;

                    if (sessionDiscovery_.initialize()) {
                        sessionDiscovery_.publishGUID(guid);

                        // Reconectar shared memory con GUID-derived names
                        bool sessionOK = sharedData_->initializeSession(guid, true);
                        if (sessionOK) {
                            sessionInitialized_ = true;
                            logMessage("[MixCoach] Sesión IPC iniciada: " + guid);
                        }
                        else {
                            logMessage("[MixCoach] Sesión IPC NO disponible aun, retry posterior");
                        }
                    }
                    else {
                        logMessage("[MixCoach] SessionDiscovery no disponible, usando modo legacy");
                    }
                }

                auto& registry = sharedData_->getSlotRegistry();

                phaseManager_ = std::make_unique<PhaseManager>(registry);
                coachEngine_  = std::make_unique<CoachEngine>(*phaseManager_.get(), *sharedData_, audioAnalyzer_);

                // Inicializar LogHelper (usamos un archivo distinto para evitar file locking con logStream_)
                LogHelper::setLogFile(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                          .getChildFile("MixCoach_Logs")
                                          .getChildFile("MixCoach_UI.log"));

                logMessage("========== MixCoach BRAIN iniciado ==========");
                writeCrashLog("[MixCoach] BRAIN OK (intento #" + juce::String(ensureSharedDataAttempts_) + ")");

                // ═══ FORZAR SINCRONIZACIÓN INICIAL ═════════════════════════════
                int found = registry.forceFullSync();

                juce::String syncMsg =
                    "[MixCoach] Sincronizacion exitosa: " + juce::String(found) + " Messenger(s) activo(s)";
                logMessage(syncMsg);
                writeCrashLog(syncMsg);

                // Log detallado de slots encontrados
                if (found > 0) {
                    registry.forEachActive([&](const SlotInfo& info) {
                        logMessage("[MixCoach]   -> Slot[" + juce::String(info.slotIndex) + "] \""
                                   + juce::String(info.trackName).trim()
                                   + "\" bus=" + juce::String(static_cast<int>(info.bus)));
                    });
                }

                // ═══ NOTIFICAR AL EDITOR ═══════════════════════════════════════
                // El editor se registra como listener de este broadcaster.
                // Al recibir el cambio, el editor refresca sharedData_ y
                // construye la UI si no se había construido aún.
                sharedDataChangeBroadcaster_.sendChangeMessage();                    // ═══ INCREMENTO 1: Start background service ─────────────────
                    // La telemetría NUNCA se detiene aunque el editor esté cerrado,
                    // porque el servicio vive en el processor, no en el editor.
                    if (!bgService_.isRunning()) {
                        bgService_.start(*sharedData_);

                        // Wire reference analysis callback
                        bgService_.setReferenceAnalysisCallback(
                            [this](const juce::String& refPath) {
                                // Run reference analysis on the bg thread
                                auto* coach = coachEngine_.get();
                                if (coach != nullptr) {
                                    auto& refPlayer = refPlayer_;
                                    const int64_t loadedSamples = refPlayer.getLoadedBufferSize();
                                    const float* bufL = refPlayer.getRefBufferL();
                                    const float* bufR = refPlayer.getRefBufferR();
                                    if (loadedSamples > 0 && bufL != nullptr) {
                                        coach->applyReferenceAnalysis(
                                            bufL, bufR, loadedSamples,
                                            refPlayer.getLoadedNumChannels(),
                                            refPlayer.getLoadedSampleRate(),
                                            refPath);
                                    }
                                    else {
                                        coach->applyReferenceAnalysis(refPath);
                                    }
                                }
                            });

                        logMessage("[MixCoach] Background service iniciado (independiente de UI)");
                    }

                    logMessage("[MixCoach] ChangeBroadcaster enviado al editor");
            }
        }
        catch (const std::exception& e) {
            auto s = juce::String("[MixCoach] EXCEPCION en ensureSharedData intento #")
                     + juce::String(ensureSharedDataAttempts_) + ": " + juce::String(e.what());
            writeCrashLog(s);
            logMessage(s);
            // NO seteamos sharedData_ a nullptr — el singleton existe,
            // solo falló la inicialización de módulos. Reintentaremos.
        }
        catch (...) {
            auto s = juce::String("[MixCoach] EXCEPCION desconocida en ensureSharedData intento #")
                     + juce::String(ensureSharedDataAttempts_);
            writeCrashLog(s);
            logMessage(s);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  initBrainModules — Inicialización LIGERA (SIN I/O pesada)
    // ═══════════════════════════════════════════════════════════════════════════
    // Crea PhaseManager y CoachEngine si shared memory está disponible.
    // NO hace forceFullSync ni loadSlotsFromBackupFiles — esas operaciones
    // se delegan al background worker del editor (backgroundRunLoop).
    //
    // Retorna true si los módulos se crearon (o ya existían).
    // Retorna false si shared memory no está disponible aún.
    bool MixCoachAudioProcessor::initBrainModules()
    {
        earlyCrashLog("BRAIN", "initBrainModules START");

        if (sharedData_ == nullptr) {
            sharedData_ = &SharedData::getInstance();
        }

        if (sharedData_ == nullptr || !sharedData_->isAvailable()) {
            earlyCrashLog("BRAIN", "initBrainModules: sharedData not available");
            return false;
        }

        // ═══ Si AiCoachAdapter ya existe, ya estamos completamente inicializados ═══
        // NOTA: No usar phaseManager_ como guard — ensureSharedData() lo crea primero
        // y si retornamos acá, nunca se crean AiCoachAdapter, LlmClient ni callbacks.
        if (aiCoachAdapter_.get() != nullptr) {
            earlyCrashLog("BRAIN", "initBrainModules: already fully initialized");
            return true;
        }

        try {
            earlyCrashLog("BRAIN", "initBrainModules: inicializando modulos");
            auto& registry = sharedData_->getSlotRegistry();

            // ═══ Crear PhaseManager + CoachEngine si ensureSharedData() no lo hizo ═══
            if (phaseManager_.get() == nullptr) {
                phaseManager_ = std::make_unique<PhaseManager>(registry);
                coachEngine_  = std::make_unique<CoachEngine>(*phaseManager_.get(), *sharedData_, audioAnalyzer_);
                earlyCrashLog("BRAIN", "initBrainModules: PhaseManager+CoachEngine creados");
            }
            else {
                earlyCrashLog("BRAIN", "initBrainModules: PhaseManager+CoachEngine ya existian");
            }

            // ═══ Crear AiCoachAdapter (si no existe) y conectar callbacks ═════════════════
            auto* cePtr     = coachEngine_.get();
            auto* pmPtr     = phaseManager_.get();
            aiCoachAdapter_ = std::make_unique<AiCoachAdapter>(*sharedData_, audioAnalyzer_, *cePtr, *pmPtr);

            // ═══ Detectar DAW anfitrión via PluginHostType ════════════════════════
            // PluginHostType detecta el DAW examinando el ejecutable anfitrión y
            // los metadatos del wrapper VST3. Usa getHostDescription() para obtener
            // el nombre legible (ej: "FL Studio", "Ableton Live").
            {
                juce::PluginHostType host;
                const char* desc = host.getHostDescription();
                if (desc != nullptr && strlen(desc) > 0) {
                    juce::String dawName = juce::String(desc).trim();
                    aiCoachAdapter_->setHostName(dawName);
                    logMessage("[MixCoach] DAW detectado: " + dawName + " — catalogo de plugins nativos disponible");
                }
                else {
                    logMessage("[MixCoach] No se pudo detectar el DAW via PluginHostType");
                }
            }

            earlyCrashLog("BRAIN", "initBrainModules: AiCoachAdapter OK");

            // ═══ Crear LlmClient con Qwen2.5 7B local + OpenRouter fallback ═══
            //
            // PROVEEDOR PRIMARIO: Ollama local con Qwen2.5 7B.
            //   - qwen2.5:7b es mucho más capaz que phi3:mini (~2x parámetros,
            //     mejor razonamiento, contexto 32K vs 4K)
            //   - Instala: ollama pull qwen2.5:7b
            //   - Otros modelos locales compatibles:
            //       • llama3.2:3b  — rápido, bueno para respuestas cortas
            //       • llama3.2:1b  — ultra rápido, ligero
            //       • qwen2.5:14b — más capaz pero requiere ~12GB RAM
            //       • deepseek-r1:7b — excelente razonamiento
            //
            // FALLBACK: OpenRouter con meta-llama/llama-3.2-3b-instruct.
            //   - Si Ollama no está disponible o falla la request,
            //     el LlmClient reenvía automáticamente al fallback.
            //   - El usuario DEBE configurar su API key de OpenRouter en la UI.
            //   - Regístrate gratis: https://openrouter.ai/keys
            //   - Modelo fallback: meta-llama/llama-3.2-3b-instruct:free
            //
            // El proveedor activo persiste en session_state.json y se restaura
            // entre sesiones de FL Studio.
            auto llmClient = std::make_unique<LlmClient>();
            LlmClient::Config llmConfig;
            llmConfig.provider    = LlmClient::Provider::Ollama;
            llmConfig.endpointUrl = "http://localhost:11434";
            llmConfig.apiKey      = {};
            llmConfig.model       = "qwen2.5:7b";
            llmConfig.temperature = 0.7f;
            llmConfig.maxTokens   = 1024;
            llmConfig.timeoutMs   = 30000;

            // ─── Fallback a OpenRouter (requiere API key del usuario) ────────
            llmConfig.useFallback         = true;
            llmConfig.fallbackProvider    = LlmClient::Provider::OpenAICompatible;
            llmConfig.fallbackEndpointUrl = "https://openrouter.ai/api/v1";
            llmConfig.fallbackApiKey      = {}; // Usuario debe configurar en UI
            llmConfig.fallbackModel       = "meta-llama/llama-3.2-3b-instruct:free";
            llmConfig.fallbackTemperature = 0.7f;
            llmConfig.fallbackMaxTokens   = 1024;
            llmConfig.fallbackTimeoutMs   = 15000;

            llmClient->setConfig(llmConfig);

            // ⚠ NO hacer checkAvailability() aquí — es HTTP síncrono y bloquearía
            // el message thread (timeout 5s). El LlmClient ya hará el retry
            // automáticamente desde su background thread cuando llegue el primer
            // mensaje del usuario (processRequest → checkAvailability).
            LogHelper::writeToLog("[PluginProcessor] LLM config: primario=Ollama/" + llmConfig.model
                                  + ", fallback=OpenRouter/" + llmConfig.fallbackModel);
            // Asignar al adapter
            aiCoachAdapter_.get()->setLlmClient(llmClient.get());
            earlyCrashLog("BRAIN", "initBrainModules: LlmClient OK");

            // Conectar callback de cambios en tracks (verifyTrackCorrections → session memory)
            coachEngine_.get()->setTrackChangeCallback([this](int slotIndex,
                                                              const juce::String& trackName,
                                                              const juce::String& description,
                                                              float beforeValue,
                                                              float afterValue) {
                if (aiCoachAdapter_.get())
                    aiCoachAdapter_.get()->recordChange(slotIndex, trackName, description, beforeValue, afterValue);
            });

            // Conectar callback de consulta de historial (/session command)
            coachEngine_.get()->setSessionQueryCallback([this]() -> juce::String {
                if (aiCoachAdapter_.get()) return aiCoachAdapter_.get()->buildSessionHistoryString();
                return {};
            });

            // Conectar callback de sugerencia de roles via LLM
            // Se usa en requestLLMRoleSuggestions() para que el LLM sugiera roles
            // de pistas que quedaron Unknown tras la inferencia deterministica.
            coachEngine_.get()->setRoleSuggestionCallback(
                [this](const juce::String& prompt, std::function<void(const juce::String&)> responseCallback) {
                    auto* rawClient = llmClient_.get();
                    if (rawClient && rawClient->isAvailable()) {
                        rawClient->sendPrompt(
                            "You are MixCoach's role suggestion assistant. "
                            "Suggest instrument roles based on track names and genre.\n",
                            prompt,
                            [responseCallback](bool success, const juce::String& response, const juce::String&) {
                                responseCallback(success ? response : juce::String{});
                            });
                    }
                    else {
                        responseCallback({});
                    }
                });

            LogHelper::writeToLog("[PluginProcessor] RoleSuggestionCallback conectado");

            // Conectar callback de respuestas LLM (retorna bool: true=manejado, false=no disponible)
            // ⚠ NO pre-verificar isLlmAvailable() aquí: askLlm() internamente llama
            // checkAvailability() con retry, así que funciona incluso si Ollama se
            // inició después de que el plugin cargó.
            coachEngine_.get()->setLlmResponseCallback(
                [this](const juce::String& userMessage,
                       std::function<void(const juce::String&)> responseCallback) -> bool {
                    if (aiCoachAdapter_.get()) {
                        // askLlm() maneja disponibilidad internamente con retry
                        aiCoachAdapter_.get()->askLlm(userMessage,
                                                      [responseCallback](bool success, const juce::String& response) {
                                                          if (success) responseCallback(response);
                                                          else
                                                              responseCallback("[PHASE] " + response);
                                                      });
                        return true;
                    }
                    return false;
                });

            // Conectar callback de respuestas LLM con STREAMING
            // (retorna bool: true=manejado, false=no disponible)
            // El onToken se llama por cada token recibido (message thread).
            // El onComplete se llama cuando termina, con la respuesta completa.
            coachEngine_.get()->setLlmStreamingCallback(
                [this](const juce::String& userMessage,
                       std::function<void(const juce::String& token)> onToken,
                       std::function<void(const juce::String&)> onComplete) -> bool {
                    if (aiCoachAdapter_.get()) {
                        // askLlmStream() maneja disponibilidad y conversation history
                        aiCoachAdapter_.get()->askLlmStream(
                            userMessage, onToken, [onComplete](bool success, const juce::String& response) {
                                if (success) onComplete(response);
                                else
                                    onComplete("[PHASE] " + response);
                            });
                        return true;
                    }
                    return false;
                });

            // Conectar callback de setup LLM (FASE 0 — bienvenida personalizada + sugerencia de roles)
            // ⚠ Usamos llmClient_.get()->sendPrompt() DIRECTAMENTE, NO askLlm(), porque el prompt de setup
            // ya incluye instrucciones completas y NO debe mezclarse con telemetría ni la personalidad
            // de ingeniero de mezcla (buildSystemPrompt + buildFullContext).
            coachEngine_.get()->setSetupLlmCallback(
                [this](const juce::String& prompt, std::function<void(const juce::String&)> responseCallback) {
                    // Use raw pointer to avoid const unique_ptr access issue in MSVC
                    auto* rawClient = llmClient_.get();
                    if (rawClient && rawClient->isAvailable()) {
                        rawClient->sendPrompt(
                            "You are MixCoach's setup assistant, starting a new mixing session. "
                            "Be warm, professional, and concise. Speak Spanish or English as appropriate.",
                            prompt,
                            [responseCallback](bool success, const juce::String& response, const juce::String&) {
                                responseCallback(success ? response : juce::String{});
                            });
                    }
                    else {
                        responseCallback({}); // No LLM available, fallback to hardcoded rules
                    }
                });

            // Cargar sesión previa desde disco
            if (aiCoachAdapter_.get()) aiCoachAdapter_.get()->autoLoad();

            // ═══ Guardar LlmClient como miembro (debe vivir más que el callback) ═══
            llmClient_ = std::move(llmClient);

            // ═══ Auto-aplicar API key guardada de sesión anterior ═══════════════
            if (apiKey_.isNotEmpty() && llmClient_ != nullptr) {
                LogHelper::writeToLog("[MixCoach] Aplicando API key guardada de sesi\xC3\xB3n anterior...");
                auto savedKey = apiKey_;
                auto config   = llmClient_->getConfig();

                // Detectar proveedor según prefijo (misma lógica que setApiKey)
                juce::String lowerKey = savedKey.trim().toLowerCase();
                if (lowerKey.startsWith("nvapi-")) {
                    config = LlmClient::makeNvidiaConfig(savedKey);
                }
                else if (lowerKey.startsWith("gsk_")) {
                    config.provider    = LlmClient::Provider::OpenAICompatible;
                    config.endpointUrl = "https://api.groq.com/openai/v1";
                    config.apiKey      = savedKey;
                    config.model       = "llama3-70b-8192";
                    config.timeoutMs   = 15000;
                }
                else {
                    config.provider = LlmClient::Provider::OpenAICompatible;
                    config.apiKey   = savedKey;
                }

                llmClient_->setConfig(config);

                // Notificar a la UI (sin checkAvailability — se hará lazy en primer mensaje)
                if (ollamaStatusCallback_) {
                    juce::String label = llmClient_->getProviderModelLabel();
                    juce::MessageManager::callAsync([cb = ollamaStatusCallback_, label]() {
                        cb(false, label); // false = sin verificar aún
                    });
                }

                LogHelper::writeToLog("[MixCoach] API key aplicada: " + llmClient_->getProviderModelLabel());
            }

            // Inicializar LogHelper (usamos un archivo distinto)
            LogHelper::setLogFile(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                      .getChildFile("MixCoach_Logs")
                                      .getChildFile("MixCoach_UI.log"));

            logMessage("========== MixCoach BRAIN iniciado (via initBrainModules) ==========");
            writeCrashLog("[MixCoach] BRAIN OK (initBrainModules)");
            earlyCrashLog("BRAIN", "initBrainModules COMPLETE");

            // ═══ NO hacemos forceFullSync aquí — el background worker lo hace ═══

            // Notificar al editor
            sharedDataChangeBroadcaster_.sendChangeMessage();
            logMessage("[MixCoach] ChangeBroadcaster enviado al editor (via initBrainModules)");

            earlyCrashLog("BRAIN", "initBrainModules SUCCESS");
            return true;
        }
        catch (const std::exception& e) {
            auto s = juce::String("[MixCoach] EXCEPCION en initBrainModules: ") + juce::String(e.what());
            writeCrashLog(s);
            logMessage(s);
            earlyCrashLog("BRAIN", "initBrainModules EXCEPTION");
            return false;
        }
        catch (...) {
            writeCrashLog("[MixCoach] EXCEPCION desconocida en initBrainModules");
            earlyCrashLog("BRAIN", "initBrainModules UNKNOWN EXCEPTION");
            return false;
        }
    }

    void MixCoachAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
    {
        earlyCrashLog("P2P", "prepareToPlay llamado");
        try {
            ensureSharedData();

            audioAnalyzer_.prepare(sampleRate, samplesPerBlock);
            refPlayer_.prepare(sampleRate, samplesPerBlock);
            celebrationChime_.prepare(sampleRate);
            prepared_ = true;

            logMessage("prepareToPlay: sampleRate=" + juce::String(sampleRate)
                       + " samplesPerBlock=" + juce::String(samplesPerBlock));
        }
        catch (const std::exception& e) {
            writeCrashLog("[MixCoach] EXCEPCION en prepareToPlay: " + juce::String(e.what()));
        }
        catch (...) {
            writeCrashLog("[MixCoach] EXCEPCION desconocida en prepareToPlay");
        }
    }

    void MixCoachAudioProcessor::releaseResources()
    {
        refPlayer_.releaseResources();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  P1: readTransportInfo — Lee contexto de transporte del DAW anfitrión
    // ═══════════════════════════════════════════════════════════════════════════
    // Usa AudioProcessor::getPlayHead() para obtener PositionInfo con:
    //   - isPlaying / isRecording
    //   - BPM (tempo)
    //   - Time signature (compás)
    //   - Position in seconds / samples / ppq
    //
    // Thread-safe: llamado desde el message thread (timer del editor).
    // Retorna TransportInfo vacío (valid=false) si no hay playhead disponible.
    MixCoachAudioProcessor::TransportInfo MixCoachAudioProcessor::readTransportInfo() const noexcept
    {
        TransportInfo info;

        auto* playHead = getPlayHead();
        if (playHead == nullptr) return info;

        auto pos = playHead->getPosition();
        if (!pos.hasValue()) return info;

        info.isPlaying  = pos->getIsPlaying();
        info.isRecording = pos->getIsRecording();
        info.isLooping = pos->getIsLooping();
        {
            auto v = pos->getTimeInSeconds();
            info.timeInSeconds = v.hasValue() ? *v : 0.0;
        }
        {
            auto v = pos->getTimeInSamples();
            info.timeInSamples = v.hasValue() ? *v : 0;
        }
        {
            auto v = pos->getPpqPosition();
            info.ppqPosition = v.hasValue() ? *v : 0.0;
        }
        {
            auto v = pos->getPpqPositionOfLastBarStart();
            info.ppqPositionOfLastBarStart = v.hasValue() ? *v : 0.0;
        }

        // BPM (tempo)
        auto bpmOpt = pos->getBpm();
        if (bpmOpt.hasValue())
            info.bpm = *bpmOpt;

        // Time signature
        auto tsOpt = pos->getTimeSignature();
        if (tsOpt.hasValue()) {
            info.timeSigNumerator   = tsOpt->numerator;
            info.timeSigDenominator = tsOpt->denominator;
        }

        info.valid = true;
        return info;
    }

    // ═══ Helper NIVEL 1: protege contra C++ exceptions (try/catch) ══════════
    // SEPARADO de safeProcessAudio() porque MSVC C2713 prohibe try/catch
    // y __try/__except en la misma función.
    static void safeProcessAudioInner(MixCoachAudioProcessor& proc, juce::AudioBuffer<float>& buffer)
    {
        try {
            // ═══ QUICK WIN 2: Render-safe flag ═══════════════════════════════
            // Si el DAW está en modo render/bounce, activar renderSafe
            // para que mixIntoBuffer() no mezcle la referencia en el bounce.
            if (proc.isNonRealtime())
                proc.getRefPlayer().setRenderSafe(true);
            else
                proc.getRefPlayer().setRenderSafe(false);

            proc.getAudioAnalyzer().processBlock(buffer);
            // Usar ganancia configurable (referenceGain_ interna)
            proc.getRefPlayer().mixIntoBuffer(buffer, -1.0f);
            // Celebrar correcciones exitosas con un chime sutil
            proc.getCelebrationChime().mixIntoBuffer(buffer, 0.35f);
        }
        catch (const std::exception& e) {
            earlyCrashLog("AUDIO_CPP", e.what());
            proc.getRefPlayer().pause();
        }
        catch (...) {
            earlyCrashLog("AUDIO_CPP", "unknown");
            proc.getRefPlayer().pause();
        }
    }

    // ═══ Helper NIVEL 2: protege contra SEH (Access Violations) ═════════════
    // SEPARADO de processBlock() porque MSVC C2712 prohíbe __try en funciones
    // con objetos C++ que tengan destructores (como ScopedNoDenormals).
    // __try/__except SÍ captura Access Violations (SEH). C++ catch() NO.
    // Llamamos a safeProcessAudioInner() desde DENTRO del __try para que
    // las C++ exceptions se capturen allí y los SEH aquí.
    // Las funciones SEPARADAS evitan C2713 (solo una forma de EH por función).
    static void safeProcessAudio(MixCoachAudioProcessor& proc, juce::AudioBuffer<float>& buffer)
    {
        __try {
            safeProcessAudioInner(proc, buffer);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            // SEH capturado (AV, heap corrupto, etc.)
            // Diagnóstico: registrar el código de excepción exacto.
            // ⚠ SOLO earlyCrashLog (C puro, sin heap) — NO llamar stop() ni clear()
            // porque juce::String desasigna heap y podría AV si está corrupto,
            // causando un nested SEH que mata el proceso.
            // pause() solo setea isPlaying_=false (atomic, sin heap).
            DWORD code = GetExceptionCode();
            char buf[128];
            snprintf(buf, sizeof(buf), "SEH 0x%08lX (code=%lu)", (unsigned long)code, (unsigned long)code);
            earlyCrashLog("AUDIO_SEH", buf);
            proc.getRefPlayer().pause();
        }
    }

    void MixCoachAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        juce::ScopedNoDenormals noDenormals;

        if (!prepared_) {
            return;
        }

        safeProcessAudio(*this, buffer);
    }

    juce::AudioProcessorEditor* MixCoachAudioProcessor::createEditor()
    {
        earlyCrashLog("CREATE", "createEditor llamado");
        // CRÍTICO: NO llamar ensureSharedData() aquí. Eso haría CreateFileMapping
        // en el message thread de FL Studio, congelando la UI si hay problemas.
        //
        // El editor se crea inmediatamente con sharedData_ (que puede ser nullptr).
        // El timer del editor se encarga de llamar ensureSharedData()
        // gradualmente desde su timerCallback.
        return new MixCoachAudioProcessorEditor(*this, sharedData_);
    }

    void MixCoachAudioProcessor::logMessage(const juce::String& msg) const
    {
        try {
            auto logFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                               .getChildFile("MixCoach_Logs")
                               .getChildFile("MixCoach_Brain.log");
            logFile.getParentDirectory().createDirectory();
            if (!logStream_.get() || logStream_.get()->getFile() != logFile) {
                logStream_ = std::make_unique<juce::FileOutputStream>(logFile, true);
            }
            if (logStream_.get() && logStream_.get()->openedOk()) {
                *logStream_.get() << "[" << juce::Time::getCurrentTime().toString(true, true) << "] [BRAIN] " << msg
                                  << "\n";
                logStream_.get()->flush();
            }
        }
        catch (...) {
            MIXCOACH_LOG_CATCH("MixCoach logMessage");
        }
    }

    void MixCoachAudioProcessor::logCrash(const juce::String& msg) const
    {
        writeCrashLog(msg);
    }

    void MixCoachAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
    {
        juce::MemoryOutputStream mos(destData, false);

        // ═══ Versión 4: + savedCoachRoomState_ + savedUIFlags_ + savedUserName_ ═════
        constexpr int kDataVersion = 4;
        mos.writeInt(kDataVersion);

        // Phase
        auto* pm = phaseManager_.get();
        if (pm) {
            mos.writeInt(static_cast<int>(pm->getCurrentPhase()));
        }
        else {
            mos.writeInt(static_cast<int>(MentorPhase::Organizacion));
        }

        // Referencias — leemos del cache del processor (siempre disponible,
        // actualizado via callbacks en cada cambio).
        const auto& filePaths = getCachedFilePaths();
        const auto& urls      = getCachedURLs();

        mos.writeInt(static_cast<int>(filePaths.size()));
        for (const auto& p : filePaths) mos.writeString(p);

        mos.writeInt(static_cast<int>(urls.size()));
        for (const auto& u : urls) mos.writeString(u);

        // ═══ V2 Dual Mode: CoachMode + MasterDestination ═══════════════════
        auto* ce = coachEngine_.get();
        if (ce) {
            mos.writeInt(static_cast<int>(ce->getCoachMode()));
            mos.writeInt(static_cast<int>(ce->getMasterDestination()));
        }
        else {
            mos.writeInt(static_cast<int>(CoachMode::Mix));
            mos.writeInt(static_cast<int>(MasterDestination::StreamingGeneral));
        }

        // ═══ V3: API key ═════════════════════════════════════════════════════
        mos.writeString(apiKey_);

        // ═══ V4: UI state persistence ═══════════════════════════════════════
        mos.writeInt(static_cast<int>(savedCoachRoomState_));
        mos.writeInt(static_cast<int>(savedUIFlags_));
        mos.writeString(savedUserName_);
    }

    void MixCoachAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
    {
        if (sizeInBytes < 4) return;

        juce::MemoryInputStream mis(data, sizeInBytes, false);

        int version = mis.readInt();

        // ─── Phase (compatible con version 0 sin version header) ──────────────
        auto* pm = phaseManager_.get();
        if (version == 1 || version == 2) {
            // Versión 1/2: tiene phase después del version header
            auto phase = static_cast<MentorPhase>(mis.readInt());
            if (pm) {
                pm->setPhase(phase);
            }
        }
        else {
            // Versión 0 (legacy, sin version header): el primer int es la phase
            auto phase = static_cast<MentorPhase>(version);
            if (pm) {
                pm->setPhase(phase);
            }
            return;
        }

        // ─── Referencias ─────────────────────────────────────────────────────
        std::vector<juce::String> filePaths, urls;

        int numFiles = mis.readInt();
        filePaths.reserve(numFiles);
        for (int i = 0; i < numFiles; ++i) filePaths.push_back(mis.readString());

        int numUrls = mis.readInt();
        urls.reserve(numUrls);
        for (int i = 0; i < numUrls; ++i) urls.push_back(mis.readString());

        // ═══ V2 Dual Mode: restauracion desde version 2 ═════════════════════
        if (version >= 2 && mis.getNumBytesRemaining() >= 8) {
            CoachMode mode         = static_cast<CoachMode>(mis.readInt());
            MasterDestination dest = static_cast<MasterDestination>(mis.readInt());

            auto* ce = coachEngine_.get();
            if (ce) {
                ce->setCoachMode(mode);
                ce->setMasterDestination(dest);
            }
        }

        // ═══ V3: API key persistence ═════════════════════════════════════════
        if (version >= 3 && mis.getNumBytesRemaining() > 0) {
            juce::String savedKey = mis.readString();
            if (savedKey.isNotEmpty()) {
                apiKey_ = savedKey;
                LogHelper::writeToLog("[MixCoach] API key restaurada de sesi\xC3\xB3n anterior");
            }
        }

        // ═══ V4: UI state persistence (coachRoomState + flags + userName) ═══
        if (version >= 4 && mis.getNumBytesRemaining() >= 8) {
            int savedState = mis.readInt();
            if (savedState >= 0 && savedState < static_cast<int>(CoachRoomState::Count)) {
                savedCoachRoomState_ = static_cast<CoachRoomState>(savedState);
            }
            savedUIFlags_ = static_cast<uint32_t>(mis.readInt());
            savedUserName_ = mis.readString();

            if (savedUserName_.isNotEmpty()) {
                LogHelper::writeToLog("[MixCoach] Estado UI restaurado: state="
                                      + juce::String(coachRoomStateLabel(savedCoachRoomState_))
                                      + ", userName=" + savedUserName_);
            }
        }

        // Almacenar para que el editor las restaure cuando la UI esté lista
        setPendingReferencePaths(filePaths, urls);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setApiKey — Actualiza la API key en runtime desde la UI del chat
    //  Detecta automáticamente el proveedor según el prefijo de la key:
    //    "nvapi-"   → NVIDIA AI Foundation API (OpenAI-compatible)
    //    "gsk_"     → Groq
    //    "sk-"      → OpenAI / DeepSeek / OpenRouter (genérico)
    //    otro       → se usa como Bearer token genérico (OpenAI-compatible)
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachAudioProcessor::setApiKey(const juce::String& apiKey)
    {
        if (apiKey.isEmpty()) return;

        apiKey_ = apiKey;

        if (llmClient_ != nullptr) {
            // ─── Detectar proveedor según prefijo de la API key ────────────────
            juce::String lowerKey = apiKey.trim().toLowerCase();

            if (lowerKey.startsWith("nvapi-")) {
                // NVIDIA AI Foundation API
                auto nvConfig = LlmClient::makeNvidiaConfig(apiKey);
                llmClient_->setConfig(nvConfig);
                LogHelper::writeToLog("[MixCoach] Proveedor NVIDIA configurado: "
                                      + nvConfig.model + " @ " + nvConfig.endpointUrl);
            }
            else if (lowerKey.startsWith("gsk_")) {
                // Groq
                auto config   = llmClient_->getConfig();
                config.apiKey = apiKey;
                config.provider = LlmClient::Provider::OpenAICompatible;
                config.endpointUrl = "https://api.groq.com/openai/v1";
                config.model = "llama3-70b-8192";
                config.timeoutMs = 15000;
                llmClient_->setConfig(config);
                LogHelper::writeToLog("[MixCoach] Proveedor Groq configurado");
            }
            else {
                // Genérico OpenAI-compatible (OpenRouter, DeepSeek, etc.)
                // Solo actualiza la key — el usuario debe haber configurado
                // endpoint y modelo desde la UI o por defecto.
                auto config   = llmClient_->getConfig();
                config.apiKey = apiKey;
                // Si el proveedor actual es Ollama, cambiar a OpenAI por defecto
                if (config.provider == LlmClient::Provider::Ollama) {
                    config.provider = LlmClient::Provider::OpenAICompatible;
                    config.endpointUrl = "https://openrouter.ai/api/v1";
                    config.model = "meta-llama/llama-3.2-3b-instruct:free";
                    LogHelper::writeToLog("[MixCoach] API key genérica — cambiando a OpenAI-compatible");
                }
                llmClient_->setConfig(config);
                LogHelper::writeToLog("[MixCoach] API key actualizada desde la UI");
            }

            // Verificar disponibilidad de la API key inmediatamente.
            // El check es ligero (GET /models) y permite que el usuario sepa
            // si la key es valida sin esperar el background worker.
            llmClient_->checkAvailability();
            bool available = llmClient_->isAvailable();

            // Notificar a la UI para que muestre el proveedor + modelo actualizados
            if (ollamaStatusCallback_) {
                juce::String label = llmClient_->getProviderModelLabel();
                ollamaStatusCallback_(available, label);
            }
        }
        else {
            logMessage("[MixCoach] API key guardada para cuando LlmClient se inicialice");
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  retryOllamaConnection — Re-intenta conectar con el proveedor LLM actual
    //  Notifica a la UI con el nombre del proveedor + modelo en formato legible.
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachAudioProcessor::retryOllamaConnection()
    {
        if (llmClient_ != nullptr) {
            llmClient_->checkAvailability();
            bool available = llmClient_->isAvailable();

            // Notificar a la UI con display name del proveedor + modelo
            if (ollamaStatusCallback_) {
                juce::MessageManager::callAsync([this, available]() {
                    juce::String label = llmClient_->getProviderModelLabel();
                    ollamaStatusCallback_(available, label);
                });
            }

            LogHelper::writeToLog("[MixCoach] LLM retry: " + juce::String(available ? "conectado" : "desconectado")
                                  + " | " + llmClient_->getProviderModelLabel());
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  isLlmAvailable — Retorna true si el LlmClient está disponible
    // ═══════════════════════════════════════════════════════════════════════════
    bool MixCoachAudioProcessor::isLlmAvailable() const noexcept
    {
        return llmClient_ != nullptr && llmClient_->isAvailable();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getLlmProviderModelLabel — Label "Proveedor: Modelo" para la UI
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String MixCoachAudioProcessor::getLlmProviderModelLabel() const noexcept
    {
        if (llmClient_ != nullptr)
            return llmClient_->getProviderModelLabel();
        return {};
    }

} // namespace mixcoach

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    earlyCrashLog("ENTRY", "createPluginFilter");
    return new mixcoach::MixCoachAudioProcessor();
}
