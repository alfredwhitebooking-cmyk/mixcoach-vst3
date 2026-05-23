#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <cstdint>
#include "Constants.h"
#include "Types.h"

namespace mixcoach {

// ─── Análisis de audio (FFT, RMS, picos, fase) ──────────────────────────────
class AudioAnalysis
{
public:
    AudioAnalysis();
    ~AudioAnalysis() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void process(const float* channelData, int numSamples);

    // Resultados
    [[nodiscard]] float getRMS()     const noexcept { return rms_; }
    [[nodiscard]] float getPeak()    const noexcept { return peak_; }
    [[nodiscard]] float getCorrelation() const noexcept { return correlation_; }
    [[nodiscard]] const float* getSpectrum() const noexcept { return spectrum_.data(); }
    [[nodiscard]] int64_t getLastUpdateTime() const noexcept { return lastUpdateUs_; }

private:
    void computeFFT(const float* data, int numSamples);
    void computeRMS(const float* data, int numSamples);
    void computePhase(const float* left, const float* right, int numSamples);

    // FFT
    std::unique_ptr<juce::dsp::FFT> fft_;
    std::array<float, kNumSpectrumBins> spectrum_{};

    float rms_         = -100.0f;
    float peak_        = -100.0f;
    float correlation_ = 1.0f;
    int64_t lastUpdateUs_ = 0;

    double sampleRate_ = 44100.0;
};

} // namespace mixcoach
