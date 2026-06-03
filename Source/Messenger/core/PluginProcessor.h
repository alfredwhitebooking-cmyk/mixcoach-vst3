#pragma once
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Common/types/Types.h"
#include "../../Common/types/TelemetryData.h"
#include "../../Common/types/Constants.h"
#include "../../Common/memory/SharedData.h"
#include "../telemetry/TelemetryCollector.h"

namespace mixcoach {

// ─── Messenger Plugin Processor (Oídos) ────────────────────────────────────
class MessengerAudioProcessor : public juce::AudioProcessor,
                                private juce::Timer
{
public:
    MessengerAudioProcessor();
    ~MessengerAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Messenger"; }

    bool acceptsMidi() const override    { return false; }
    bool producesMidi() const override   { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override                                      { return 1; }
    int getCurrentProgram() override                                   { return 0; }
    void setCurrentProgram(int) override                               {}
    const juce::String getProgramName(int) override                    { return {}; }
    void changeProgramName(int, const juce::String&) override          {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ─── Nuevas APIs para rename, colorear y ruteo ──────────────────────────
    void setTrackName(const juce::String& newName);
    [[nodiscard]] juce::String getTrackName() const noexcept { return trackName_; }

    void setTrackColour(const juce::Colour& newColour);
    [[nodiscard]] juce::Colour getTrackColour() const noexcept { return trackColour_; }

    void setBusAssignment(BusType bus);
    [[nodiscard]] BusType getBusAssignment() const noexcept { return busAssignment_; }

    // Acceso para el editor
    [[nodiscard]] int getSlotIndex() const noexcept { return slotIndex_; }
    [[nodiscard]] SharedData* getSharedData() noexcept { return sharedData_; }

    // Registrar slot lazy (llamado desde prepareToPlay/processBlock, NUNCA desde constructor)
    void ensureSlotRegistered();

    // Timer callback: un solo disparo 500ms tras creación para registrar slot
    // en DAWs que no llaman setStateInformation() para instancias nuevas.
    void timerCallback() override;

    [[nodiscard]] bool isMuted() const noexcept { return muted_; }
    void setMuted(bool mute);

private:
    SharedData* sharedData_ = nullptr;
    int slotIndex_{-1};
    // Telemetry handled by TelemetryManager singleton (no local buffer needed)
    TelemetryCollector collector_;

    // Buffer mono pre-asignado para evitar alloc en audio thread
    juce::AudioBuffer<float> monoBuffer_;

    // Logger de diagnóstico LOCAL (sin usar Logger global de JUCE para evitar
    // conflictos entre plugins en el mismo proceso)
    // Cada plugin escribe su propio archivo de log directamente
    mutable std::unique_ptr<juce::FileOutputStream> logStream_;
    void logMessage(const juce::String& msg) const;
    void logCrash(const juce::String& msg) const;

    // Flag: prepareToPlay fue llamado (protege contra DAWs que llaman processBlock antes)
    bool prepared_{false};

    // ═══ Flag: slot ya registrado (evita chequeo redundante en hot path) ═══
    // ANTES: processBlock() llamaba a if (slotIndex_ < 0) ensureSlotRegistered()
    // en CADA bloque de audio. ensureSlotRegistered() verificaba sharedData_
    // y slotIndex_ nuevamente. Con 60+ Messengers en paralelo, el overhead
    // de 60+ llamadas a una función con chequeos y una clausura try/catch
    // se acumulaba en cada bloque de audio (~11ms para 512 samples).
    // AHORA: Flag booleana simple para el hot path. El slot se registra en
    // prepareToPlay, setStateInformation, o en el timer (fire once 500ms).
    bool slotRegistered_{false};

    // ═══ Flag: backup file pendiente de escribir (diferido al timer) ═══
    // Cuando registerSlot() ya no escribe backup (para evitar I/O durante
    // la inserción masiva de 100 Messengers), el backup inicial se escribe
    // desde el timerCallback() en el message thread ~500ms después.
    // Esto es SEGURO porque el timer corre en el message thread, NO en
    // el audio thread. La única desventaja es que el backup tarda ~500ms
    // en escribirse, pero cuando FL Studio se reinicie, forceFullSync()
    // leerá los backups existentes de la sesión anterior.
    bool pendingBackupWrite_{false};

    // Contador de escrituras a shared memory (por instancia, thread-safe)
    int shmWriteCounter_ = 0;

    // ─── MUTE: detiene el envío de telemetría (el audio sigue pasando)
    std::atomic<bool> muted_{false};

    // Estado persistente de la pista
    juce::String trackName_;
    juce::Colour trackColour_{0xFF808080};
    BusType busAssignment_{BusType::None};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MessengerAudioProcessor)
};

} // namespace mixcoach
