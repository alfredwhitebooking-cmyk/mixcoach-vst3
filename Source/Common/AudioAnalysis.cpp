#include "AudioAnalysis.h"
#include <algorithm>
#include <cmath>

namespace mixcoach {

// ─── AudioAnalysis constructor ───────────────────────────────────────────────
// NOTA: NO creamos el FFT aquí porque juce::dsp::FFT aloca memoria interna
// que puede crashear durante el escaneo V3 en FL Studio (sandbox).
// El FFT se crea lazy en prepare().
AudioAnalysis::AudioAnalysis() = default;

void AudioAnalysis::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    sampleRate_ = sampleRate;

    // Crear FFT (lazy — no en constructor para evitar crashes durante escaneo VST3)
    if (!fft_)
        fft_ = std::make_unique<juce::dsp::FFT>(9); // 512-point FFT

    rms_ = -100.0f;
    peak_ = -100.0f;
    correlation_ = 1.0f;
    spectrum_.fill(0.0f);
}

void AudioAnalysis::process(const float* channelData, int numSamples)
{
    if (!fft_) return; // No preparado aún, skip

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

    // Copiar datos a buffer temporal y aplicar ventana Hann.
    // JUCE requiere 2 * kFFTSize floats para la transformacion real in-place.
    std::array<float, kFFTSize * 2> fftData{};
    int copyLen = std::min(numSamples, kFFTSize);

    for (int i = 0; i < kFFTSize; ++i) {
        float hann = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / kFFTSize));
        fftData[i] = (i < copyLen) ? data[i] * hann : 0.0f;
    }

    // FFT in-place (real-only forward transform)
    fft_->performRealOnlyForwardTransform(fftData.data());

    // Extraer magnitudes: salida es [re(0), im(0), re(1), im(1), ... re(N/2), im(N/2)]
    const int numBins = std::min(kNumSpectrumBins, kFFTSize / 2);
    for (int i = 0; i < numBins; ++i) {
        float real = fftData[i * 2];
        float imag = fftData[i * 2 + 1];
        spectrum_[i] = std::sqrt(real * real + imag * imag);
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
