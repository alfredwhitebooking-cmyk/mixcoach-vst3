#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../Common/Types.h"
#include <array>
#include <atomic>
#include <deque>
#include <cmath>

namespace mixcoach {

// ─── Medidor LUFS simplificado (EBU R128 / ITU BS.1770) ─────────────────────
// Implementa filtrado K-weighting y medición de loudness en tres ventanas
// temporales: Momentary (400ms), Short-term (3s), Integrated (acumulativo).
class LoudnessMeter
{
public:
    LoudnessMeter();

    // Preparar con sample rate
    void prepare(double sampleRate);

    // Procesar un bloque de audio estéreo
    void process(const float* left, const float* right, int numSamples);

    // Resultados
    [[nodiscard]] float getMomentaryLUFS()  const noexcept { return momentaryLUFS_; }
    [[nodiscard]] float getShortTermLUFS()  const noexcept { return shortTermLUFS_; }
    [[nodiscard]] float getIntegratedLUFS() const noexcept { return integratedLUFS_; }
    [[nodiscard]] float getLoudnessRange()  const noexcept { return loudnessRange_; }

    void reset();

private:
    // K-weighting filter: pre-filter + shelving
    void applyKFilter(const float* input, float* output, int numSamples);

    // Calcular mean square de un bloque
    float computeMeanSquare(const float* data, int numSamples) const;

    // Convertir mean square a LUFS
    static float toLUFS(float meanSquare);

    // Estado del filtro IIR (biquad direct form I)
    struct BiquadState {
        double x1 = 0.0, x2 = 0.0;
        double y1 = 0.0, y2 = 0.0;
    };

    // Filtros K-weighting
    BiquadState hpState_;  // High-pass 20Hz 2nd order
    BiquadState shState_;  // Shelving +4dB @ 1.5kHz

    // Coeficientes
    double hpB0_{}, hpB1_{}, hpB2_{}, hpA1_{}, hpA2_{};
    double shB0_{}, shB1_{}, shB2_{}, shA1_{}, shA2_{};

    // Buffers de mean square para cada ventana temporal
    std::deque<float> momentaryQueue_;   // 400ms
    std::deque<float> shortTermQueue_;   // 3s
    std::deque<float> integratedQueue_;  // todo el tiempo

    // Buffers pre-asignados para audio thread (evitar heap allocations en process())
    std::vector<float> monoBuffer_;
    std::vector<float> filterBuffer_;
    // Buffer reutilizable para loudness range (evita std::vector temporal en audio thread)
    std::vector<float> lrBuffer_;

    double sampleRate_ = 44100.0;

    float momentaryLUFS_  = -100.0f;
    float shortTermLUFS_  = -100.0f;
    float integratedLUFS_ = -100.0f;
    float loudnessRange_  = 0.0f;

    int blocksSinceLastCalc_ = 0;
    static constexpr int kCalcInterval = 10; // Cada 10 bloques recalcular
};

// ═══════════════════════════════════════════════════════════════════════════
//  TelemetryCollector — DSP en tiempo real para Messenger
// ═══════════════════════════════════════════════════════════════════════════
class TelemetryCollector
{
public:
    TelemetryCollector();
    ~TelemetryCollector() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    TrackTelemetry collect(const juce::AudioBuffer<float>& buffer);

private:
    float computePeak(const float* data, int numSamples) const;
    float computeRMS(const float* data, int numSamples) const;
    float computeCorrelation(const float* left, const float* right, int numSamples) const;
    float computeCrestFactor(float peakDb, float rmsDb) const;

    // FFT
    void computeSpectrum(const float* data, int numSamples, float* outSpectrum, int numBins);

    // Medidor LUFS (EBU R128)
    LoudnessMeter loudnessMeter_;

    // Rolling buffer para FFT
    std::vector<float> fftBuffer_;
    int fftWritePos_ = 0;

    std::unique_ptr<juce::dsp::FFT> fft_;
    std::vector<float> window_;

    static constexpr int kFFTOrder = 9;  // 512-point FFT
    static constexpr int kFFTSize = 1 << kFFTOrder;
    static constexpr int kSpectrumBins = 256;

    int blockCount_ = 0;
    static constexpr int kFFTInterval = 4; // Cada 4 bloques computar FFT

    double sampleRate_ = 44100.0;
};

} // namespace mixcoach
