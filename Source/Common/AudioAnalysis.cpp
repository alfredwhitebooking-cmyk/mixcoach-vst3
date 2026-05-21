#include "AudioAnalysis.h"
#include <algorithm>
#include <cmath>
#include <complex>

namespace mixcoach {

AudioAnalysis::AudioAnalysis()
{
    fft_ = std::make_unique<juce::dsp::FFT>(9); // 512-point FFT
}

void AudioAnalysis::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    sampleRate_ = sampleRate;
    rms_ = -100.0f;
    peak_ = -100.0f;
    correlation_ = 1.0f;
    spectrum_.fill(0.0f);
}

void AudioAnalysis::process(const float* channelData, int numSamples)
{
    computeRMS(channelData, numSamples);
    computeFFT(channelData, numSamples);
    // correlation needs two channels; handled externally
    lastUpdateUs_ = juce::Time::getMillisecondCounter() * 1000;
}

void AudioAnalysis::computeRMS(const float* data, int numSamples)
{
    double sumSq = 0.0;
    for (int i = 0; i < numSamples; ++i)
        sumSq += static_cast<double>(data[i]) * data[i];

    rms_ = (sumSq > 0.0)
        ? static_cast<float>(juce::Decibels::gainToDecibels(
              static_cast<float>(std::sqrt(sumSq / numSamples))))
        : -100.0f;
}

void AudioAnalysis::computeFFT(const float* data, int numSamples)
{
    if (!fft_) return;

    // Copiar datos a un buffer temporal y aplicar ventana Hann
    std::array<float, kFFTSize> windowed{};
    int copyLen = std::min(numSamples, kFFTSize);
    std::copy(data, data + copyLen, windowed.begin());

    // Aplicar ventana Hann manualmente
    for (int i = 0; i < kFFTSize; ++i) {
        float hann = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / kFFTSize));
        windowed[i] *= hann;
    }

    // Llenar buffer FFT como std::complex<float>
    std::array<std::complex<float>, kFFTSize> complexData{};
    for (int i = 0; i < kFFTSize; ++i) {
        complexData[i] = std::complex<float>(windowed[i], 0.0f);
    }

    auto* inPtr = complexData.data();
    auto* outPtr = complexData.data();
    fft_->perform(inPtr, outPtr, false);

    // Magnitudes
    const int numBins = std::min(kNumSpectrumBins, kFFTSize / 2);
    for (int i = 0; i < numBins; ++i) {
        spectrum_[i] = std::abs(complexData[i]);
    }
}

void AudioAnalysis::computePhase(const float* left, const float* right, int numSamples)
{
    double sumProduct = 0.0;
    double sumLeftSq  = 0.0;
    double sumRightSq = 0.0;

    for (int i = 0; i < numSamples; ++i) {
        sumProduct += static_cast<double>(left[i]) * right[i];
        sumLeftSq  += static_cast<double>(left[i]) * left[i];
        sumRightSq += static_cast<double>(right[i]) * right[i];
    }

    auto denom = std::sqrt(sumLeftSq * sumRightSq);
    correlation_ = (denom > 1e-12)
        ? static_cast<float>(sumProduct / denom)
        : 1.0f;
}

} // namespace mixcoach
