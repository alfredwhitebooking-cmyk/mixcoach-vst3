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
        } else {
            LogHelper::writeToLog("[SharedData] SharedMemory IPC NO disponible, modo local");
            shmInitialized_ = false;
        }

        // ─── Inicializar SharedAudioMemory (IPC de audio RAW) ───────────
        if (audioMemory_.initialize()) {
            LogHelper::writeToLog("[SharedData] SharedAudioMemory audio RAW IPC OK");
        } else {
            LogHelper::writeToLog("[SharedData] SharedAudioMemory NO disponible");
        }
        // ─── Inicializar SharedAudioMemoryV2 (stereo) ────────────────
        if (audioMemoryV2_.initialize()) {
            LogHelper::writeToLog("[SharedData] SharedAudioMemoryV2 stereo IPC OK");
        } else {
            LogHelper::writeToLog("[SharedData] SharedAudioMemoryV2 NO disponible");
        }
    }
    catch (const std::exception& e) {
        shmInitialized_ = false;
        // No podemos usar LogHelper aquí porque podría no estar inicializado
        try {
            auto logFile = juce::File::getSpecialLocation(
                juce::File::userDocumentsDirectory)
                .getChildFile("MixCoach_Logs")
                .getChildFile("MixCoach_Crash.log");
            logFile.getParentDirectory().createDirectory();
            juce::FileOutputStream fos(logFile, true);
            if (fos.openedOk()) {
                fos << "[" << juce::Time::getCurrentTime().toString(true, true)
                    << "] [SharedData] EXCEPCION: " << e.what() << "\n";
                fos.flush();
            }
        } catch (...) {}
    }
    catch (...) {
        shmInitialized_ = false;
    }
}

// ─── Reintentar inicialización de SharedMemory ──────────────────────────────
// Si falló en el constructor (shmInitialized_=false), este método cierra
// el gestor anterior e intenta crear uno nuevo. Útil cuando el otro plugin
// aún no había creado la shared memory cuando nosotros la intentamos abrir.
bool SharedData::retryInitSharedMemory()
{
    if (shmInitialized_)
        return true;

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
            auto logFile = juce::File::getSpecialLocation(
                juce::File::userDocumentsDirectory)
                .getChildFile("MixCoach_Logs")
                .getChildFile("MixCoach_Crash.log");
            logFile.getParentDirectory().createDirectory();
            juce::FileOutputStream fos(logFile, true);
            if (fos.openedOk()) {
                fos << "[" << juce::Time::getCurrentTime().toString(true, true)
                    << "] [SharedData] retryInitSharedMemory EXCEPTION: " << e.what() << "\n";
                fos.flush();
            }
        } catch (...) {}
    }
    catch (...) {
        // Silencio total
    }

    return false;
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
    if (index >= 0 && index < messageCount_)
        return messages_[index];
    return MentorMessage{};
}
SharedAudioMemoryV2& SharedData::getAudioMemoryV2() noexcept { return audioMemoryV2_; }
const SharedAudioMemoryV2& SharedData::getAudioMemoryV2() const noexcept { return audioMemoryV2_; }
} // namespace mixcoach
