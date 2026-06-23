#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include "../types/Constants.h"
#include "../types/Types.h"

namespace mixcoach {

    // ─── Análisis de audio (FFT, RMS, picos, fase) ──────────────────────────────
    // ⚠️ Thread safety:
    //   - Audio thread ESCRIBE (processBlock)
    //   - Message thread (CoachEngine + UI) LEE
    //   - Valores escalares: std::atomic con memory_order release/acquire
    //   - Espectro (spectrum_): data race aceptado (array grande, glitches tolerables)
    class AudioAnalysis
    {
    public:
        AudioAnalysis();
        ~AudioAnalysis() = default;

        void prepare(double sampleRate, int samplesPerBlock);
        void process(const float* channelData, int numSamples);

        // ─── Getters thread-safe (memory_order_acquire) ─────────────────────
        [[nodiscard]] float getRMS() const noexcept { return rms_.load(std::memory_order_acquire); }

        [[nodiscard]] float getPeak() const noexcept { return peak_.load(std::memory_order_acquire); }

        [[nodiscard]] float getCorrelation() const noexcept { return correlation_.load(std::memory_order_acquire); }

        [[nodiscard]] int64_t getLastUpdateTime() const noexcept
        {
            return lastUpdateUs_.load(std::memory_order_acquire);
        }

        // ─── Setters thread-safe (memory_order_release) ─────────────────────
        void setRMS(float val) noexcept { rms_.store(val, std::memory_order_release); }

        void setPeak(float val) noexcept { peak_.store(val, std::memory_order_release); }

        void setCorrelation(float corr) noexcept { correlation_.store(corr, std::memory_order_release); }

        void setLastUpdateTime(int64_t t) noexcept { lastUpdateUs_.store(t, std::memory_order_release); }

        // ─── Espectro (data race aceptado, glitch tolerable) ────────────────
        [[nodiscard]] const float* getSpectrum() const noexcept { return spectrum_.data(); }

        [[nodiscard]] float* getSpectrumWritable() noexcept { return spectrum_.data(); }

    private:
        void computeFFT(const float* data, int numSamples);
        void computeRMS(const float* data, int numSamples);
        void computePhase(const float* left, const float* right, int numSamples);

        // FFT (1024-point, legacy)
        std::unique_ptr<juce::dsp::FFT> fft_;
        std::array<float, kNumSpectrumBins> spectrum_{};

        // ═══ Hi-Res FFT (16384-point con 16x overlap) ════════════════════════
        // Especificaciones IK Multimedia Metering:
        //   - FFT Size: 16384 (2^14, resolución de ~2.93Hz @ 48kHz)
        //   - Overlap: 16x (Hop Size = 1024 samples = 21ms @ 48kHz)
        //   - Window: Blackman-Harris 4-term (-92dB sidelobe rejection)
        //   - Normalización: coherent gain de Blackman-Harris ≈ 0.179375
        //   - Actualización: ~47 FPS a 48kHz
        // IK Multimedia Metering quality:
        //   - FFT Size: 16384 (2^14, ~2.93Hz resolution @ 48kHz)
        //   - Overlap: 16x (Hop Size = 1024 = 21ms @ 48kHz → ~47 FPS)
        //   - Window: Blackman-Harris 4-term (-92dB sidelobe rejection)
        //   - Balance óptimo entre resolución espectral y rendimiento CPU
        static constexpr int kHiResFFTOrder = 14; // 16384 = 2^14
        static constexpr int kHiResFFTSize  = 16384;
        static constexpr int kHiResNumBins  = 8192; // 16384 / 2
        static constexpr int kHiResHopSize  = 1024; // 16384/16 = 16x overlap
        // Blackman-Harris 4-term coherent gain (for 0dBFS normalization)
        // BH coefficients: a0=0.35875, mean = a0 = 0.35875
        // Peak magnitude for 0dBFS sine ≈ N * mean / 2 = N * 0.179375
        static constexpr float kBHNormFactor = static_cast<float>(kHiResFFTSize) * 0.179375f;

        std::unique_ptr<juce::dsp::FFT> hiResFFT_;
        std::array<float, kHiResFFTSize> hiResFifo_{}; // ring buffer de entrada (32768)
        int hiResCount_ = 0;
        std::array<float, kHiResNumBins> hiResSpectrum_{};
        std::array<std::atomic<float>, kHiResNumBins> hiResSpectrumAtomic_{};

        void feedHiResFFT(const float* data, int numSamples);

    public:
        /** Hi-Res spectrum para display (8192-FFT, 8x overlap, Blackman-Harris). */
        [[nodiscard]] const float* getHiResSpectrum() const noexcept { return hiResSpectrum_.data(); }

        [[nodiscard]] int getHiResNumBins() const noexcept { return kHiResNumBins; }

        [[nodiscard]] int copyHiResSpectrum(float* dest, int maxBins) const noexcept;

    private:
        // ═══ Campos atómicos (thread-safe audio ↔ message) ═══════════════════
        std::atomic<float> rms_{-100.0f};
        std::atomic<float> peak_{-100.0f};
        std::atomic<float> correlation_{1.0f};
        std::atomic<int64_t> lastUpdateUs_{0};

        double sampleRate_ = 44100.0;
    };

} // namespace mixcoach
