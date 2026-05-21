#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Common/AudioAnalysis.h"

namespace mixcoach {

// ─── Analizador multicanal (envuelve AudioAnalysis) ─────────────────────────
class AudioAnalyzer
{
public:
    AudioAnalyzer() = default;
    ~AudioAnalyzer() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void processBlock(const juce::AudioBuffer<float>& buffer);

    [[nodiscard]] const AudioAnalysis& getMasterAnalysis() const noexcept { return masterAnalysis_; }

private:
    AudioAnalysis masterAnalysis_;
    AudioAnalysis leftAnalysis_;
    AudioAnalysis rightAnalysis_;
    double sampleRate_ = 44100.0;
};

} // namespace mixcoach
