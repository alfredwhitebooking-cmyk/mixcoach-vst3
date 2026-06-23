#pragma once
#include "ReferenceMetrics.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include "../../Common/audio/AudioAnalysis.h"
#include "../../Common/audio/LoudnessAnalyzer.h"
#include <atomic>

namespace mixcoach {

class VectorscopeComponent;

// ─── Analizador multicanal (envuelve AudioAnalysis + LoudnessAnalyzer) ──────
class AudioAnalyzer
{
public:
    // Compute reference metrics based on current analysis
    // Returns a struct with spectral and loudness metrics for comparison with a reference.
    mixcoach::ReferenceMetrics computeReferenceMetrics() const noexcept;
public:
    AudioAnalyzer() = default;
    ~AudioAnalyzer() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void processBlock(const juce::AudioBuffer<float>& buffer);

    [[nodiscard]] const AudioAnalysis& getMasterAnalysis() const noexcept { return masterAnalysis_; }

    [[nodiscard]] const AudioAnalysis& getLeftAnalysis()   const noexcept { return leftAnalysis_; }
    [[nodiscard]] const AudioAnalysis& getRightAnalysis()  const noexcept { return rightAnalysis_; }

    // ─── LUFS (EBU R128) ──────────────────────────────────────────────────
    [[nodiscard]] const LoudnessAnalyzer& getLoudness() const noexcept { return loudness_; }
    [[nodiscard]] float getMomentaryLUFS()  const noexcept { return loudness_.getMomentary(); }
    [[nodiscard]] float getShortTermLUFS()  const noexcept { return loudness_.getShortTerm(); }
    [[nodiscard]] float getIntegratedLUFS() const noexcept { return loudness_.getIntegrated(); }
    [[nodiscard]] float getTruePeakDBTP()   const noexcept { return loudness_.getTruePeak(); }
    [[nodiscard]] float getLoudnessRange()  const noexcept { return loudness_.getRange(); }

    // ─── Stereo Width (mid/side ratio) ───────────────────────────────────────
    [[nodiscard]] float getAvgStereoWidth() const noexcept { return avgStereoWidth_; }

    // ─── Vectorscope sample buffer ───────────────────────────────────────
    /** Empuja todos los samples nuevos del ring buffer al vectorscope. */
    void flushSampleBufferToVectorscope(VectorscopeComponent& vectorscope);

    /**
     * Lee los samples no leídos del ring buffer decimado.
     * @param leftOut  Buffer de salida para canal izquierdo
     * @param rightOut Buffer de salida para canal derecho
     * @param maxCount Máximo de samples a leer
     * @return Número de samples escritos en los buffers
     */
    int flushSamples(float* leftOut, float* rightOut, int maxCount) const;

private:
    // lastReadIndex_ es mutable porque flushSamples() es const — la posición
    // de lectura es estado de UI thread, no estado lógico del analizador.
    mutable int lastReadIndex_{0};

private:
    AudioAnalysis masterAnalysis_;
    AudioAnalysis leftAnalysis_;
    AudioAnalysis rightAnalysis_;
    LoudnessAnalyzer loudness_;
    double sampleRate_ = 44100.0;

    // ─── Master buffer preasignado (NO heap alloc en audio thread) ──────
    // Antes se creaba un juce::AudioBuffer en cada processBlock(), lo que
    // hacia heap allocation en el audio thread — una violacion de tiempo real
    // que podia causar crashes bajo contencion del heap.
    // Ahora se preasigna en prepare() con el maximo block size.
    // ═══ SIN heap allocation en constructor: empty buffer, se crea en prepare() ═══
    juce::AudioBuffer<float> masterBuffer_; // Empty, zero allocation
    int maxBlockSize_ = 0;

    // ─── Sample buffer para vectorscope (decimado ~8x) ────────────────────
    static constexpr int kSampleBufferSize = 1024;
    struct StereoPair { float l, r; };
    std::array<StereoPair, kSampleBufferSize> sampleBuffer_{};
    std::atomic<int> sampleBufferWrite_{0};
    int decimateCounter_{0};
    static constexpr int kDecimateFactor = 8;

    // ─── Stereo Width State ──────────────────────────────────────────────
    std::atomic<float> avgStereoWidth_{0.0f};
};

} // namespace mixcoach
