#include "SharedData.h"
#include "LogHelper.h"
#include <juce_core/juce_core.h>

namespace mixcoach {

// ─── Constructor: inicializa SharedMemoryManager con protección total ────────
// NOTA: Este constructor NUNCA debe lanzar excepciones porque getInstance()
//       es llamado desde el initializer list de MixCoachAudioProcessor.
//       Si lanza, el plugin crashea durante el escaneo VST3 de FL Studio.
SharedData::SharedData() noexcept
{
    try {
        // Inicializar gestor de memoria compartida (IPC entre procesos)
        // CreateFileMapping permite que MixCoach y Messenger vean los mismos slots
        shm_ = std::make_unique<SharedMemoryManager>();
        if (shm_->initialize()) {
            slotRegistry_.setSharedMemory(shm_.get());
            shmInitialized_ = true;
            LogHelper::writeToLog("[SharedData] SharedMemory IPC iniciado OK");
        } else {
            LogHelper::writeToLog("[SharedData] SharedMemory IPC NO disponible, modo local");
            shmInitialized_ = false;
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

} // namespace mixcoach
