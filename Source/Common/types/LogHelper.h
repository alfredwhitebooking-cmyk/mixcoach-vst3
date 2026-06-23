#pragma once
#include <juce_core/juce_core.h>

namespace mixcoach {

    // ─── LogHelper ────────────────────────────────────────────────────────────────
    // Reemplaza el Logger global de JUCE para logging de diagnóstico.
    //
    // Cada plugin inicializa LogHelper con su propio archivo de log.
    // Como cada plugin es un proceso separado (VST3 sandboxing en FL Studio),
    // la instancia static es independiente para Brain y Messenger.
    //
    // ═══ Thread Safety ═══
    // writeToLog() es thread-safe via mutex interno. Puede ser llamada desde
    // cualquier thread sin riesgo de corrupción del FileOutputStream.
    //
    // Uso:
    //   1. En PluginProcessor constructor:
    //      LogHelper::setLogFile(juce::File::getSpecialLocation(
    //          juce::File::userDocumentsDirectory)
    //          .getChildFile("MixCoach_Logs")
    //          .getChildFile("MixCoach_Brain.log"));
    //
    //   2. En cualquier parte del código común (SharedMemory, SlotRegistry, etc.):
    //      LogHelper::writeToLog("[SharedMemory] Mensaje de diagnóstico");
    //
    class LogHelper
    {
    public:
        // ─── Inicializar con archivo de log específico para este plugin ──────────
        // Cada plugin llama esto una vez desde su constructor.
        static void setLogFile(const juce::File& logFile)
        {
            auto& inst = instance();
            logFile.getParentDirectory().createDirectory();
            inst.logger_.reset(new juce::FileLogger(logFile, "MixCoach",
                                                    1024 * 1024)); // max 1MB antes de rotar
        }

        // ─── Escribir mensaje de log (thread-safe) ───────────────────────────────
        // Puede ser llamado desde cualquier thread. Usa un mutex estático para
        // proteger el logger_ compartido entre threads.
        static void writeToLog(const juce::String& message)
        {
            auto& inst = instance();
            const juce::ScopedLock lock(inst.mutex_);
            if (inst.logger_) {
                inst.logger_->logMessage(message);
            }
            else {
                // Fallback si no hay logger configurado
                juce::Logger::outputDebugString(message);
            }
        }

    private:
        static LogHelper& instance()
        {
            static LogHelper helper;
            return helper;
        }

        LogHelper()  = default;
        ~LogHelper() = default;

        std::unique_ptr<juce::FileLogger> logger_;
        juce::CriticalSection mutex_; // Thread safety para writeToLog

        JUCE_DECLARE_NON_COPYABLE(LogHelper)
    };

} // namespace mixcoach
