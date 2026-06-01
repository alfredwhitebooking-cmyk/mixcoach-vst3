#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  WaveformView — Mini waveform animado
// ═══════════════════════════════════════════════════════════════════════════
class WaveformView : public juce::Component
{
public:
    WaveformView();

    void pushSample(float amplitude);
    void setWaveColour(juce::Colour col);
    void setSignalPresent(bool hasSignal);
    void setRMS(float rms);

    void paint(juce::Graphics& g) override;

private:
    static constexpr int kNumSamples = 128;
    static constexpr int kDecimation = 4;
    std::array<float, kNumSamples> waveData_{};
    int writePos_ = 0;
    int samplesSinceUpdate_ = 0;
    bool hasSignal_ = false;
    float currentRMS_ = 0.0f;
    juce::Colour waveColour_{0xFF3498DB};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformView)
};

} // namespace mixcoach
