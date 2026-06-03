#pragma once

#include <vector>
#include <cstdint>

namespace mixcoach {

struct TelemetryData {
    int slotIndex = -1;
    float peakLeft = 0.0f;
    float peakRight = 0.0f;
    float rmsLeft = 0.0f;
    float rmsRight = 0.0f;
    float correlation = 0.0f;
    float crestFactor = 0.0f;
    float sampleL = 0.0f;
    float sampleR = 0.0f;
    float lufsIntegrated = 0.0f;
    float lufsShortTerm = 0.0f;
    float lufsMomentary = 0.0f;
    float lufsTruePeak = 0.0f;
    uint64_t timestampMs = 0; // Milliseconds since epoch
    std::vector<float> spectrum; // FFT magnitudes
};

} // namespace mixcoach
