#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  LoudnessAnalyzer — Medición de sonoridad estilo EBU R128 (simplificado)
    //
    //  Provee:
    //    - Momentary LUFS (ventana 400ms, overlap 75%)
    //    - Short-term LUFS (ventana 3s deslizante)
    //    - Integrated LUFS (gated, con threshold relativo -10 LU)
    //    - True Peak (dBTP, con sobremuestreo x4)
    //    - Loudness Range (LRA — aproximación estadística)
    //
    //  Filtro K-weighting completo: HPF @ 48.1 Hz + shelving +4 dB @ 1.5 kHz
    // ═══════════════════════════════════════════════════════════════════════════
    class LoudnessAnalyzer
    {
    public:
        LoudnessAnalyzer();
        ~LoudnessAnalyzer() = default;

        void prepare(double sampleRate, int samplesPerBlock);
        void processBlock(const float* left, const float* right, int numSamples);

        // ─── Getters thread-safe (memory_order_acquire) ──────────────────────
        [[nodiscard]] float getMomentary() const noexcept { return momentaryLUFS_.load(std::memory_order_acquire); }

        [[nodiscard]] float getShortTerm() const noexcept { return shortTermLUFS_.load(std::memory_order_acquire); }

        [[nodiscard]] float getIntegrated() const noexcept { return integratedLUFS_.load(std::memory_order_acquire); }

        [[nodiscard]] float getTruePeak() const noexcept { return truePeakDBTP_.load(std::memory_order_acquire); }

        [[nodiscard]] float getRange() const noexcept { return loudnessRange_.load(std::memory_order_acquire); }

        // Reset acumuladores (Integrated empieza de nuevo)
        void resetAccumulators();

    private:
        double sampleRate_ = 48000.0;

        // ─── K-weighting filter (EBU R128 / ITU-R BS.1770-4) ───────────────
        // Stage 1: 2nd-order Butterworth high-pass @ 48.1 Hz
        // Stage 2: High-shelving filter +4 dB @ 1.5 kHz, Q=0.5
        //
        // Both filters are chained in series: mono → HPF → Shelf → RMS²
        juce::dsp::IIR::Filter<float> hpFilter_;    // Stage 1
        juce::dsp::IIR::Filter<float> shelfFilter_; // Stage 2

        void initFilters();

        // ─── Overlap buffers (75% overlap for momentary) ────────────────────
        std::vector<float> momentaryBlocks_; // RMS² per block
        std::vector<float> shortTermBlocks_; // RMS² per block
        int momentaryWrite_ = 0;
        int shortTermWrite_ = 0;
        int blockCounter_   = 0;
        int samplesPerBlock_{0};

        // RMS² acumulado para el bloque actual
        double blockSumSq_    = 0.0;
        int blockSampleCount_ = 0;

        // ─── Integrated (gated) ─────────────────────────────────────────────
        double gatedSumSq_  = 0.0;
        int gatedCount_     = 0;
        double prelimSumSq_ = 0.0; // Gate 1: sum of all RMS² > -70 LUFS
        int prelimCount_    = 0;
        float prelimLUFS_   = -100.0f;
        bool gatingDone_    = false;

        // ═══ Campos atómicos (thread-safe audio ↔ message thread) ═══════════=
        std::atomic<float> momentaryLUFS_{-100.0f};
        std::atomic<float> shortTermLUFS_{-100.0f};
        std::atomic<float> integratedLUFS_{-100.0f};
        std::atomic<float> truePeakDBTP_{-100.0f};
        std::atomic<float> loudnessRange_{0.0f};

        // ─── True peak (oversampling x4) ───────────────────────────────────
        float truePeak_ = 0.0f;
        std::array<float, 3> upsamplerDelay_{};

        // ─── Loudness Range (percentiles de short-term) ─────────────────────
        std::vector<float> stHistogram_;         // short-term values for LRA
        static constexpr int kMaxHistogram = 90; // ~30s a 3s por entrada

        // ─── Flat LUFS computation ──────────────────────────────────────────
        [[nodiscard]] static float computeLUFS(double sumSq, int count) noexcept
        {
            if (count <= 0 || sumSq <= 0.0) return -100.0f;
            return static_cast<float>(juce::Decibels::gainToDecibels(static_cast<float>(std::sqrt(sumSq / count))));
        }

        void updateIntegrated();
        void updateTruePeak(const float* left, const float* right, int numSamples);
        void flushBlock();
    };

} // namespace mixcoach
