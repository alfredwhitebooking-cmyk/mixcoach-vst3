#pragma once

namespace mixcoach {

/**
    Struct that holds extended reference analysis metrics.
    Includes basic spectral centroid, RMS per band, crest factor, and multiband LUFS.
*/
struct ReferenceMetrics {
    // Basic spectral metrics
    float centroid = 0.0f;                 // Frequency centroid (Hz)
    float spread   = 0.0f;                 // Spectral spread (Hz)

    // RMS per band (7 bands as defined in ReferenceAnalyzer)
    static constexpr int kNumBands = 7;
    float rmsByBand[kNumBands] = {};

    // Crest factor (peak / RMS) of the whole reference signal
    float crestFactor = 0.0f;

    // Multiband LUFS (same 7 bands), values in LUFS
    float lufsByBand[kNumBands] = {};
};

} // namespace mixcoach
