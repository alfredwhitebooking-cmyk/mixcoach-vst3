#include "AudioAnalyzer.h"

namespace mixcoach {

void AudioAnalyzer::prepare(double sampleRate, int samplesPerBlock)
{
    sampleRate_ = sampleRate;
    masterAnalysis_.prepare(sampleRate, samplesPerBlock);
    leftAnalysis_.prepare(sampleRate, samplesPerBlock);
    rightAnalysis_.prepare(sampleRate, samplesPerBlock);
}

void AudioAnalyzer::processBlock(const juce::AudioBuffer<float>& buffer)
{
    auto numSamples = buffer.getNumSamples();

    if (buffer.getNumChannels() >= 2) {
        auto left  = buffer.getReadPointer(0);
        auto right = buffer.getReadPointer(1);

        leftAnalysis_.process(left, numSamples);
        rightAnalysis_.process(right, numSamples);

        // Master = mezcla de L + R
        juce::AudioBuffer<float> masterBuffer(1, numSamples);
        auto* masterData = masterBuffer.getWritePointer(0);
        for (int i = 0; i < numSamples; ++i)
            masterData[i] = (left[i] + right[i]) * 0.5f;

        masterAnalysis_.process(masterData, numSamples);
    } else if (buffer.getNumChannels() == 1) {
        auto mono = buffer.getReadPointer(0);
        masterAnalysis_.process(mono, numSamples);
        leftAnalysis_.process(mono, numSamples);
        rightAnalysis_.process(mono, numSamples);
    }
}

} // namespace mixcoach
