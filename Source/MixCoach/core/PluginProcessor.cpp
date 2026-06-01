#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../../Common/types/LogHelper.h"

#include <cstdio>
#include <ctime>
#ifdef _WIN32
#include <windows.h>
#endif

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
    } else {
        // Fallback: raiz de C:
        snprintf(path, sizeof(path), "C:/MixCoach_Crash.log");
    }
    
    // Crear directorio si no existe (usando Windows API directamente)
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/Documents/MixCoach_Logs", home ? home : "C:");
#ifdef _WIN32
    CreateDirectoryA(dir, nullptr);  // No falla si ya existe
#endif
    
    FILE* f = fopen(path, "a");
    if (f) {
        // Timestamp
        time_t t = time(nullptr);
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
        auto logFile = juce::File::getSpecialLocation(
            juce::File::userDocumentsDirectory)
            .getChildFile("MixCoach_Logs")
            .getChildFile("MixCoach_Crash.log");
        logFile.getParentDirectory().createDirectory();
        juce::FileOutputStream fos(logFile, true);
        if (fos.openedOk()) {
            fos << "[" << juce::Time::getCurrentTime().toString(true, true)
                << "] [BRAIN] " << msg << "\n";
            fos.flush();
        }
    } catch (...) {
        // No podemos hacer nada, silencio total
    }
}

// ─── Constructor: ABSOLUTAMENTE NADA que pueda crashear ─────────────────────
// Durante el escaneo VST3 de FL Studio, el plugin se carga en un sandbox.
// Cualquier CreateFileMapping, heap allocation grande, o file I/O puede
// causar un structured exception (SEH). El try/catch C++ normal NO captura SEH,
// y __try/__except no se puede usar en constructores con objetos C++.
//
// Por eso: todo se inicializa LAZY en ensureSharedData() / prepareToPlay().
MixCoachAudioProcessor::MixCoachAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input",  juce::AudioChannelSet::stereo(), true)
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
        if (logStream_)
            logStream_->flush();
    }
    catch (...) {}
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
        uint32_t elapsed = nowMs - lastEnsureAttemptTimeMs_;
        uint32_t requiredBackoff = 0;
        if      (ensureSharedDataAttempts_ == 0) requiredBackoff = 0;
        else if (ensureSharedDataAttempts_ == 1) requiredBackoff = 500;   // reducido de 1000ms
        else if (ensureSharedDataAttempts_ == 2) requiredBackoff = 1000;  // reducido de 2000ms
        else if (ensureSharedDataAttempts_ == 3) requiredBackoff = 2000;  // reducido de 4000ms
        else                                     requiredBackoff = 5000;  // reducido de 10000ms

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
                errorDetail = shmHealthy ? "healthCheck=OK pero init=false" : "healthCheck=FAIL";
            } else {
                errorDetail = "shm_=nullptr";
            }
        } else if (sharedData_ == nullptr) {
            errorDetail = "sharedData_ no creado aun";
        }

        juce::String logMsg = "[MixCoach] Intento de conexion #"
            + juce::String(ensureSharedDataAttempts_)
            + " | sharedData_=" + (sharedData_ == nullptr ? "NULL" : "OK")
            + " | isAvailable=" + (sharedData_ && sharedData_->isAvailable() ? "SI" : "NO")
            + " | phaseManager_=" + (phaseManager_ ? "OK" : "NULL");
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
            logMessage("[MixCoach] Intento de conexion #"
                + juce::String(ensureSharedDataAttempts_)
                + " | retryInitSharedMemory=" + (reconnected ? "OK" : "FAIL"));
            writeCrashLog("[MixCoach] retryInit=" + juce::String(reconnected ? "OK" : "FAIL"));
            
            if (!reconnected) {
                // Log de diagnóstico adicional del estado de shared memory
                bool shmHealth = sharedData_->isSharedMemoryAvailable();
                juce::String errorDetail;
                if (!shmHealth) {
                    errorDetail = "shm_=nullptr";
                } else {
                    bool shmHealthy = sharedData_->getSharedMemory().healthCheck();
                    errorDetail = shmHealthy ? "healthCheck=OK pero init=false" : "healthCheck=FAIL";
                }
                logMessage("[MixCoach] Intento de conexion #"
                    + juce::String(ensureSharedDataAttempts_)
                    + " | Fallo de mapeo de memoria: [" + errorDetail + "]");
                return; // Reintentar en el próximo ciclo
            }

            // ═══ Sub-caso 3A: Shared memory RECONECTADA con phaseManager ya existente ═══
            // Esto ocurre cuando la shared memory se pierde temporalmente (ej: el DAW
            // reinicia el sandbox de un Messenger) y luego se recupera.
            // Necesitamos forzar un resync completo y notificar al editor.
            if (phaseManager_ != nullptr) {
                logMessage("[MixCoach] Intento de conexion #"
                    + juce::String(ensureSharedDataAttempts_)
                    + " | Shared memory RECONECTADA — forzando resync completo...");

                auto& registry = sharedData_->getSlotRegistry();
                int found = registry.forceFullSync();

                juce::String syncMsg = "[MixCoach] Sincronizacion exitosa: "
                    + juce::String(found) + " Messenger(s) encontrado(s) post-reconexion";
                logMessage(syncMsg);
                writeCrashLog(syncMsg);

                if (found > 0) {
                    registry.forEachActive([&](const SlotInfo& info) {
                        logMessage("[MixCoach]   -> Slot[" + juce::String(info.slotIndex)
                            + "] \"" + juce::String(info.trackName).trim()
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
            if (phaseManager_ != nullptr) {
                logMessage("[MixCoach] CASO 4 con phaseManager_ existente: forzando resync post-reconexion...");
                
                auto& registry = sharedData_->getSlotRegistry();
                int found = registry.forceFullSync();
                
                if (found > 0) {
                    logMessage("[MixCoach] Resync post-reconexion: "
                        + juce::String(found) + " Messenger(s) encontrado(s)");
                    registry.forEachActive([&](const SlotInfo& info) {
                        logMessage("[MixCoach]   -> Slot[" + juce::String(info.slotIndex)
                            + "] \"" + juce::String(info.trackName).trim()
                            + "\" bus=" + juce::String(static_cast<int>(info.bus)));
                    });
                }
                
                sharedDataChangeBroadcaster_.sendChangeMessage();
                logMessage("[MixCoach] ChangeBroadcaster enviado al editor (post-reconexion)");
                return;
            }
            
            // ═══ Sub-caso 4B: Primera inicialización completa ═════════════
            auto& registry = sharedData_->getSlotRegistry();

            phaseManager_ = std::make_unique<PhaseManager>(registry);
            coachEngine_  = std::make_unique<CoachEngine>(*phaseManager_, *sharedData_);

            // Inicializar LogHelper (usamos un archivo distinto para evitar file locking con logStream_)
            LogHelper::setLogFile(
                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                    .getChildFile("MixCoach_Logs")
                    .getChildFile("MixCoach_UI.log"));

            logMessage("========== MixCoach BRAIN iniciado ==========");
            writeCrashLog("[MixCoach] BRAIN OK (intento #"
                + juce::String(ensureSharedDataAttempts_) + ")");

            // ═══ FORZAR SINCRONIZACIÓN INICIAL ═════════════════════════════
            int found = registry.forceFullSync();

            juce::String syncMsg = "[MixCoach] Sincronizacion exitosa: "
                + juce::String(found) + " Messenger(s) activo(s)";
            logMessage(syncMsg);
            writeCrashLog(syncMsg);

            // Log detallado de slots encontrados
            if (found > 0) {
                registry.forEachActive([&](const SlotInfo& info) {
                    logMessage("[MixCoach]   -> Slot[" + juce::String(info.slotIndex)
                        + "] \"" + juce::String(info.trackName).trim()
                        + "\" bus=" + juce::String(static_cast<int>(info.bus)));
                });
            }

            // ═══ NOTIFICAR AL EDITOR ═══════════════════════════════════════
            // El editor se registra como listener de este broadcaster.
            // Al recibir el cambio, el editor refresca sharedData_ y
            // construye la UI si no se había construido aún.
            sharedDataChangeBroadcaster_.sendChangeMessage();

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
    
    if (phaseManager_ != nullptr) {
        earlyCrashLog("BRAIN", "initBrainModules: already initialized");
        return true; // Ya inicializado
    }
    
    try {
        earlyCrashLog("BRAIN", "initBrainModules: creando PhaseManager");
        auto& registry = sharedData_->getSlotRegistry();
        
        phaseManager_ = std::make_unique<PhaseManager>(registry);
        earlyCrashLog("BRAIN", "initBrainModules: PhaseManager OK, creando CoachEngine");
        coachEngine_  = std::make_unique<CoachEngine>(*phaseManager_, *sharedData_);
        earlyCrashLog("BRAIN", "initBrainModules: CoachEngine OK");
        
        // Inicializar LogHelper (usamos un archivo distinto)
        LogHelper::setLogFile(
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
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
}

void MixCoachAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Protección: algunos DAWs llaman processBlock ANTES de prepareToPlay
    if (!prepared_) {
        return;
    }

    // Pasamos el audio (MixCoach esta en el Master, el audio pasa directo)
    auto now = juce::Time::getMillisecondCounter();
    if (now - lastAnalysisTime_ > 500) // analisis cada 500ms como maximo
    {
        audioAnalyzer_.processBlock(buffer);
        lastAnalysisTime_ = now;
    }
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
        auto logFile = juce::File::getSpecialLocation(
            juce::File::userDocumentsDirectory)
            .getChildFile("MixCoach_Logs")
            .getChildFile("MixCoach_Brain.log");
        logFile.getParentDirectory().createDirectory();
        if (!logStream_ || logStream_->getFile() != logFile) {
            logStream_ = std::make_unique<juce::FileOutputStream>(logFile, true);
        }
        if (logStream_ && logStream_->openedOk()) {
            *logStream_ << "[" << juce::Time::getCurrentTime().toString(true, true)
                       << "] [BRAIN] " << msg << "\n";
            logStream_->flush();
        }
    }
    catch (...) {}
}

void MixCoachAudioProcessor::logCrash(const juce::String& msg) const
{
    writeCrashLog(msg);
}

void MixCoachAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos(destData, false);
    if (phaseManager_) {
        mos.writeInt(static_cast<int>(phaseManager_->getCurrentPhase()));
    } else {
        mos.writeInt(static_cast<int>(MentorPhase::Welcome));
    }
}

void MixCoachAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream mis(data, sizeInBytes, false);
    if (sizeInBytes >= 4) {
        auto phase = static_cast<MentorPhase>(mis.readInt());
        if (phaseManager_) {
            phaseManager_->setPhase(phase);
        }
    }
}

} // namespace mixcoach

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    earlyCrashLog("ENTRY", "createPluginFilter");
    return new mixcoach::MixCoachAudioProcessor();
}
