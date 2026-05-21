#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Common/Types.h"

namespace mixcoach {

// ─── Colector de telemetría (CPU ultraligero) ───────────────────────────────
class TelemetryCollector
{
public:
    TelemetryCollector() = default;

    TrackTelemetry collect(const juce::AudioBuffer<float>& buffer);

private:
    float computePeak(const float* data, int numSamples) const;
    float computeRMS(const float* data, int numSamples) const;
    float computeCorrelation(const float* left, const float* right, int numSamples) const;
};

} // namespace mixcoach
