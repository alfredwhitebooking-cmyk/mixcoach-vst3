#include "SharedData.h"
#include "../types/LogHelper.h"
#include <juce_core/juce_core.h>

namespace mixcoach {

    // ─── Constructor: inicializa SharedMemoryManager con protección total ────────
    // NOTA: Este constructor NUNCA debe lanzar excepciones porque getInstance()
    //       es llamado desde el initializer list de MixCoachAudioProcessor.
    //       Si lanza, el plugin crashea durante el escaneo VST3 de FL Studio.
    SharedData::SharedData() noexcept
    {
        try {
            // ─── Inicializar SharedMemory (IPC de identidad) ────────────────
            shm_ = std::make_unique<SharedMemoryManager>();
            if (shm_->initialize()) {
                slotRegistry_.setSharedMemory(shm_.get());
                shmInitialized_ = true;
                LogHelper::writeToLog("[SharedData] SharedMemory IPC iniciado OK");
            }
            else {
                LogHelper::writeToLog("[SharedData] SharedMemory IPC NO disponible, modo local");
                shmInitialized_ = false;
            }

            // ─── Inicializar SharedAudioMemory (IPC de audio RAW) ───────────
            if (audioMemory_.initialize()) {
                LogHelper::writeToLog("[SharedData] SharedAudioMemory audio RAW IPC OK");
            }
            else {
                LogHelper::writeToLog("[SharedData] SharedAudioMemory NO disponible");
            }
            // ─── Inicializar SharedAudioMemoryV2 (stereo) ────────────────
            if (audioMemoryV2_.initialize()) {
                LogHelper::writeToLog("[SharedData] SharedAudioMemoryV2 stereo IPC OK");
            }
            else {
                LogHelper::writeToLog("[SharedData] SharedAudioMemoryV2 NO disponible");
            }
        }
        catch (const std::exception& e) {
            shmInitialized_ = false;
            // No podemos usar LogHelper aquí porque podría no estar inicializado
            try {
                auto logFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                   .getChildFile("MixCoach_Logs")
                                   .getChildFile("MixCoach_Crash.log");
                logFile.getParentDirectory().createDirectory();
                juce::FileOutputStream fos(logFile, true);
                if (fos.openedOk()) {
                    fos << "[" << juce::Time::getCurrentTime().toString(true, true)
                        << "] [SharedData] EXCEPCION: " << e.what() << "\n";
                    fos.flush();
                }
            }
            catch (const std::exception& e) {
                LogHelper::writeToLog("[SharedData] ctor fallback-log EXCEPTION: " + juce::String(e.what()));
            }
            catch (...) {
                MIXCOACH_LOG_CATCH("SharedData ctor fallback-log");
            }
        }
        catch (...) {
            shmInitialized_ = false;
            MIXCOACH_LOG_CATCH("SharedData ctor");
        }
    }

    // ─── Reintentar inicialización de SharedMemory ──────────────────────────────
    // Si falló en el constructor (shmInitialized_=false), este método cierra
    // el gestor anterior e intenta crear uno nuevo. Útil cuando el otro plugin
    // aún no había creado la shared memory cuando nosotros la intentamos abrir.
    bool SharedData::retryInitSharedMemory()
    {
        if (shmInitialized_) return true;

        if (shm_) {
            // Cerrar y reemplazar el gestor existente
            shm_->close();
            shm_.reset();
        }

        try {
            shm_ = std::make_unique<SharedMemoryManager>();
            if (shm_->initialize()) {
                slotRegistry_.setSharedMemory(shm_.get());
                shmInitialized_ = true;
                LogHelper::writeToLog("[SharedData] retryInitSharedMemory: CONEXIÓN EXITOSA en reintento");
                return true;
            }
            LogHelper::writeToLog("[SharedData] retryInitSharedMemory: aun NO disponible");
        }
        catch (const std::exception& e) {
            try {
                auto logFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                   .getChildFile("MixCoach_Logs")
                                   .getChildFile("MixCoach_Crash.log");
                logFile.getParentDirectory().createDirectory();
                juce::FileOutputStream fos(logFile, true);
                if (fos.openedOk()) {
                    fos << "[" << juce::Time::getCurrentTime().toString(true, true)
                        << "] [SharedData] retryInitSharedMemory EXCEPTION: " << e.what() << "\n";
                    fos.flush();
                }
            }
            catch (const std::exception& e) {
                LogHelper::writeToLog("[SharedData] retryInit fallback-log EXCEPTION: " + juce::String(e.what()));
            }
            catch (...) {
                MIXCOACH_LOG_CATCH("SharedData retryInit fallback-log");
            }
        }
        catch (...) {
            MIXCOACH_LOG_CATCH("SharedData retryInit");
        }

        return false;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  initializeSession — Conecta a shared memory con GUID-derivated names
    // ═══════════════════════════════════════════════════════════════════════════
    // MixCoach genera un GUID y lo publica via SessionDiscovery. Luego llama
    // a este método para reconectar SharedMemoryManager y SharedAudioMemoryV2
    // con los nombres derivados del GUID.
    //
    // Messenger lee el GUID del SessionDiscovery y llama a este método para
    // conectarse a la misma sesión.
    //
    // Si falla la conexión (porque el otro plugin aún no creó la shared memory),
    // retorna false y el llamador debe reintentar (como retryInitSharedMemory).
    bool SharedData::initializeSession(const juce::String& sessionGUID, bool isBrain)
    {
        if (sessionGUID.isEmpty()) return false;

        sessionGUID_ = sessionGUID;

        // Construir nombres derivados del GUID
        juce::String slotShmName = makeSlotShmName(sessionGUID);
        juce::String audioShmName = makeAudioShmName(sessionGUID);

        juce::String roleStr = isBrain ? juce::String("BRAIN") : juce::String("SENSOR");
        LogHelper::writeToLog("[SharedData] initializeSession: " + roleStr
                              + " | Slots=" + slotShmName
                              + " | Audio=" + audioShmName);

        try {
            // ─── (1) Cerrar SharedMemoryManager actual ───────────────────────
            if (shm_) {
                shm_->close();
            }
            else {
                shm_ = std::make_unique<SharedMemoryManager>();
            }

            // (Re)abrir con GUID-derived name
            bool shmOK = shm_->initialize(slotShmName);
            if (!shmOK) {
                LogHelper::writeToLog("[SharedData] initializeSession: SHM slota aun no disponible");
                // Si somos el brain y fallamos, algo anda mal; somos creadores.
                // Si somos el sensor, el brain aún no creó la shm — reintentar después.
                if (isBrain) {
                    // Reintentar como brain
                    shm_->close();
                    shm_->initialize(slotShmName);
                }
                return false;
            }

            slotRegistry_.setSharedMemory(shm_.get());
            shmInitialized_ = true;
            LogHelper::writeToLog("[SharedData] SharedMemory sesion OK: " + slotShmName);

            // ─── (2) Cerrar SharedAudioMemoryV2 y reconectar ─────────────────
            audioMemoryV2_.close();
            bool audioOK = audioMemoryV2_.initialize(audioShmName);
            if (!audioOK) {
                LogHelper::writeToLog("[SharedData] initializeSession: Audio SHM aun no disponible");
                return false;
            }

            LogHelper::writeToLog("[SharedData] SharedAudioMemoryV2 sesion OK: " + audioShmName);
            return true;
        }
        catch (const std::exception& e) {
            LogHelper::writeToLog("[SharedData] initializeSession EXCEPTION: " + juce::String(e.what()));
            return false;
        }
        catch (...) {
            MIXCOACH_LOG_CATCH("SharedData::initializeSession");
            return false;
        }
    }

    SharedData::~SharedData()
    {
        // SharedMemoryManager se destruye automáticamente via unique_ptr
        // El destructor cierra el file mapping handle
    }

    void SharedData::pushMessage(const MentorMessage& msg)
    {
        if (messageCount_ < kMaxMessages) {
            messages_[messageCount_++] = msg;
        }
    }

    int SharedData::getMessageCount() const noexcept
    {
        return messageCount_;
    }

    MentorMessage SharedData::getMessage(int index) const
    {
        if (index >= 0 && index < messageCount_) return messages_[index];
        return MentorMessage{};
    }

    SharedAudioMemoryV2& SharedData::getAudioMemoryV2() noexcept
    {
        return audioMemoryV2_;
    }

    const SharedAudioMemoryV2& SharedData::getAudioMemoryV2() const noexcept
    {
        return audioMemoryV2_;
    }
} // namespace mixcoach
