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

    // ─── Nuevas APIs para rename y colorear ─────────────────────────────────
    void setTrackName(const juce::String& newName);
    [[nodiscard]] juce::String getTrackName() const noexcept { return trackName_; }

    void setTrackColour(const juce::Colour& newColour);
    [[nodiscard]] juce::Colour getTrackColour() const noexcept { return trackColour_; }

    // Acceso para el editor
    [[nodiscard]] int getSlotIndex() const noexcept { return slotIndex_; }
    [[nodiscard]] SharedData& getSharedData() noexcept { return sharedData_; }

private:
    SharedData& sharedData_;
    int slotIndex_{-1};
    TelemetryBuffer localTelemetry_;
    TelemetryCollector collector_;

    // Buffer mono pre-asignado para evitar alloc en audio thread
    juce::AudioBuffer<float> monoBuffer_;

    // Estado persistente de la pista
    juce::String trackName_;
    juce::Colour trackColour_{0xFF808080};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MessengerAudioProcessor)
};

} // namespace mixcoach
