#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Common/Types.h"
#include "../Common/SharedData.h"
#include "../Common/SlotRegistry.h"
#include "PhaseManager.h"
#include "CoachEngine.h"
#include "AudioAnalyzer.h"
#include "UI/MixCoachTheme.h"

namespace mixcoach {

// ─── MixCoach AudioProcessor (Cerebro) ──────────────────────────────────────
class MixCoachAudioProcessor : public juce::AudioProcessor
{
public:
    MixCoachAudioProcessor();
    ~MixCoachAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MixCoach"; }

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

    // Acceso a datos compartidos
    SharedData& getSharedData() noexcept { return sharedData_; }

    // Analizador y coach
    AudioAnalyzer& getAudioAnalyzer() noexcept { return audioAnalyzer_; }
    CoachEngine&   getCoachEngine()   noexcept { return coachEngine_; }
    PhaseManager&  getPhaseManager()  noexcept { return phaseManager_; }

private:
    SharedData&    sharedData_;
    AudioAnalyzer  audioAnalyzer_;
    PhaseManager   phaseManager_;
    CoachEngine    coachEngine_;
    int64_t        lastAnalysisTime_{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixCoachAudioProcessor)
};

} // namespace mixcoach
