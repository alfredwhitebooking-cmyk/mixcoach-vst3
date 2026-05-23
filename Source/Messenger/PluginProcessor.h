#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Common/Types.h"
#include "../Common/TelemetryData.h"
#include "../Common/Constants.h"
#include "../Common/SharedData.h"
#include "TelemetryCollector.h"

namespace mixcoach {

// ─── Messenger Plugin Processor (Oídos) ────────────────────────────────────
class MessengerAudioProcessor : public juce::AudioProcessor
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

private:
    SharedData* sharedData_ = nullptr;
    int slotIndex_{-1};
    TelemetryBuffer localTelemetry_;
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

    // Contador de escrituras a shared memory (por instancia, thread-safe)
    int shmWriteCounter_ = 0;

    // Estado persistente de la pista
    juce::String trackName_;
    juce::Colour trackColour_{0xFF808080};
    BusType busAssignment_{BusType::None};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MessengerAudioProcessor)
};

} // namespace mixcoach
