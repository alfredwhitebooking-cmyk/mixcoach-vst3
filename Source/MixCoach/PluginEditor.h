#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "UI/MainTabbedComponent.h"
#include "UI/MixCoachTheme.h"
#include "../Common/SharedData.h"

namespace mixcoach {

// ─── MixCoach Plugin Editor ─────────────────────────────────────────────────
class MixCoachAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    MixCoachAudioProcessorEditor(MixCoachAudioProcessor& processor, SharedData& sharedData);
    ~MixCoachAudioProcessorEditor() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    MixCoachAudioProcessor& processorRef_;
    SharedData&             sharedData_;
    MainTabbedComponent     tabbedComponent_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixCoachAudioProcessorEditor)
};

} // namespace mixcoach
