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

        // Crear FFT 1024 (lazy — no en constructor para evitar crashes durante escaneo VST3)
        if (!fft_) fft_ = std::make_unique<juce::dsp::FFT>(10); // 1024-point FFT

        // Crear Hi-Res FFT 32768 (lazy — no en constructor para evitar crashes VST3 scan)
        if (!hiResFFT_) hiResFFT_ = std::make_unique<juce::dsp::FFT>(kHiResFFTOrder);

        setRMS(-100.0f);
        setPeak(-100.0f);
        setCorrelation(1.0f);
        setLastUpdateTime(0);
        spectrum_.fill(0.0f);
        hiResSpectrum_.fill(0.0f);
        for (auto& bin : hiResSpectrumAtomic_) bin.store(0.0f, std::memory_order_release);
        hiResCount_ = 0;
        hiResFifo_.fill(0.0f);
    }

    void AudioAnalysis::process(const float* channelData, int numSamples)
    {
        if (!fft_) return; // No preparado aún, skip

        computeRMS(channelData, numSamples);
        computeFFT(channelData, numSamples);
        // ═══ Hi-Res FFT con overlap (solo si hay datos) ═══════════════════════
        if (hiResFFT_ && numSamples > 0) feedHiResFFT(channelData, numSamples);
        // correlation needs two channels; handled externally
        setLastUpdateTime(static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000);
    }

    void AudioAnalysis::computeRMS(const float* data, int numSamples)
    {
        double sumSq = 0.0;
        float maxAbs = 0.0f;
        for (int i = 0; i < numSamples; ++i) {
            float s = data[i];
            sumSq += static_cast<double>(s) * s;
            float absVal = std::abs(s);
            if (absVal > maxAbs) maxAbs = absVal;
        }

        setRMS((sumSq > 0.0) ? static_cast<float>(
                                   juce::Decibels::gainToDecibels(static_cast<float>(std::sqrt(sumSq / numSamples))))
                             : -100.0f);

        // ═══ CRÍTICO: Peak se computaba AQUÍ (en computeRMS, un solo loop)
        // Antes NO se computaba en absoluto, lo que hacía que getPeak()
        // devolviera siempre -100.0f y las barras L/R del MeterPanel
        // NUNCA se movieran.
        setPeak((maxAbs > 1e-10f) ? juce::Decibels::gainToDecibels(maxAbs) : -100.0f);
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

        // Extraer magnitudes NORMALIZADAS: salida es [re(0), im(0), ...]
        // Normalizar por kFFTSize * 0.25 (para ventana Hann, magnitud pico ≈ N/4)
        // Esto asegura que 0dBFS = 1.0 en la escala normalizada
        constexpr float kNorm1024 = static_cast<float>(kFFTSize) * 0.25f; // 256
        const int numBins         = std::min(kNumSpectrumBins, kFFTSize / 2);
        for (int i = 0; i < numBins; ++i) {
            float real   = fftData[i * 2];
            float imag   = fftData[i * 2 + 1];
            spectrum_[i] = std::sqrt(real * real + imag * imag) / kNorm1024;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Hi-Res FFT (16384-point, 16x overlap, Blackman-Harris) —
    //  Especificaciones IK Multimedia Metering:
    //   - FFT Size: 16384 (2^14, ~2.93Hz resolution @ 48kHz)
    //   - Overlap: 16x (Hop Size = 1024 samples por update)
    //   - Window: Blackman-Harris 4-term (-92dB sidelobe rejection)
    //   - Normalización: coherent gain de BH ≈ 0.179375
    //   - Actualización: ~47 FPS a 48kHz
    // ═══════════════════════════════════════════════════════════════════════════
    void AudioAnalysis::feedHiResFFT(const float* data, int numSamples)
    {
        if (!hiResFFT_ || data == nullptr || numSamples <= 0) return;

        // Blackman-Harris 4-term coefficients (-92dB sidelobe rejection)
        // w[i] = a0 - a1*cos(2πi/N) + a2*cos(4πi/N) - a3*cos(6πi/N)
        // Coherent gain = a0 = 0.35875
        // Peak magnitude for 0dBFS ≈ N * a0 / 2 = N * 0.179375
        constexpr float kBH_a0 = 0.35875f;
        constexpr float kBH_a1 = 0.48829f;
        constexpr float kBH_a2 = 0.14128f;
        constexpr float kBH_a3 = 0.01168f;

        for (int s = 0; s < numSamples; ++s) {
            // Acumular muestra en el ring buffer
            hiResFifo_[hiResCount_] = data[s];
            hiResCount_++;

            // Cuando el buffer está lleno (8192 muestras), procesar FFT
            if (hiResCount_ >= kHiResFFTSize) {
                // Preparar buffer FFT con ventana Blackman-Harris 4-term
                std::array<float, kHiResFFTSize * 2> fftFrame{};
                const float twoPi = juce::MathConstants<float>::twoPi;
                for (int i = 0; i < kHiResFFTSize; ++i) {
                    const float angle = twoPi * i / kHiResFFTSize;
                    const float bh    = kBH_a0 - kBH_a1 * std::cos(angle) + kBH_a2 * std::cos(2.0f * angle)
                                        - kBH_a3 * std::cos(3.0f * angle);
                    fftFrame[i]       = hiResFifo_[i] * bh;
                }

                // FFT in-place
                hiResFFT_->performRealOnlyForwardTransform(fftFrame.data());

                // Extraer magnitudes NORMALIZADAS (4096 bins)
                // Normalizar por coherent gain de Blackman-Harris: N * a0 / 2
                // Esto asegura que 0dBFS = 1.0 en escala normalizada
                // (misma escala que FFT 1024 con ventana Hann)
                for (int i = 0; i < kHiResNumBins; ++i) {
                    float real        = fftFrame[i * 2];
                    float imag        = fftFrame[i * 2 + 1];
                    const float mag   = std::sqrt(real * real + imag * imag) / kBHNormFactor;
                    hiResSpectrum_[i] = mag;
                    hiResSpectrumAtomic_[i].store(mag, std::memory_order_release);
                }

                // ═══ Sliding window: 8x overlap ═══════════════════════════
                // Mantener los últimos (kHiResFFTSize - kHiResHopSize) = 7168 samples
                // y dejar espacio para 1024 samples nuevos.
                int keep = kHiResFFTSize - kHiResHopSize; // 7168
                std::memmove(hiResFifo_.data(), hiResFifo_.data() + kHiResHopSize, keep * sizeof(float));
                hiResCount_ = keep;
            }
        }
    }

    int AudioAnalysis::copyHiResSpectrum(float* dest, int maxBins) const noexcept
    {
        if (dest == nullptr || maxBins <= 0) return 0;

        const int count = std::min(maxBins, kHiResNumBins);
        for (int i = 0; i < count; ++i) dest[i] = hiResSpectrumAtomic_[i].load(std::memory_order_acquire);

        return count;
    }

    void AudioAnalysis::computePhase(const float* left, const float* right, int numSamples)
    {
        double sumProduct = 0.0;
        double sumLeftSq  = 0.0;
        double sumRightSq = 0.0;

        for (int i = 0; i < numSamples; ++i) {
            sumProduct += static_cast<double>(left[i]) * right[i];
            sumLeftSq += static_cast<double>(left[i]) * left[i];
            sumRightSq += static_cast<double>(right[i]) * right[i];
        }

        auto denom = std::sqrt(sumLeftSq * sumRightSq);
        setCorrelation((denom > 1e-12) ? static_cast<float>(sumProduct / denom) : 1.0f);
    }

} // namespace mixcoach
