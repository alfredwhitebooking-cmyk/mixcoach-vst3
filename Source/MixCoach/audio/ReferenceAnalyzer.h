#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../../Common/audio/AudioAnalysis.h"
#include "../../Common/audio/LoudnessAnalyzer.h"
#include "../audio/AudioAnalyzer.h"
#include <atomic>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  ComparisonResult — Diferencia entre referencia y master por bandas
//  Se genera llamando a ReferenceAnalyzer::compareWith().
// ═══════════════════════════════════════════════════════════════════════════
struct ReferenceComparison {
    bool valid = false;

    // Diferencia por banda (referencia - master en dB, positivo = ref tiene más)
    float subDiff       = 0.0f;   // 43-215 Hz
    float bassDiff      = 0.0f;   // 215-516 Hz
    float lowMidDiff    = 0.0f;   // 516-1500 Hz
    float highMidDiff   = 0.0f;   // 1500-3000 Hz
    float presenceDiff  = 0.0f;   // 3000-5160 Hz
    float highDiff      = 0.0f;   // 5160-8600 Hz
    float airDiff       = 0.0f;   // 8600-12900 Hz

    // Diferencia de loudness (ref - master en LUFS)
    float lufsIntegratedDiff = 0.0f;
    float lufsShortTermDiff  = 0.0f;

    // Similitud espectral general (0.0 = nada que ver, 1.0 = idéntico)
    float spectralSimilarity = 0.0f;

    // Resumen textual para el chat del Coach o para mostrar en UI
    juce::String summary;

    // Valores de la referencia (para display)
    float refMomentaryLUFS  = -100.0f;
    float refShortTermLUFS  = -100.0f;
    float refIntegratedLUFS = -100.0f;
};

// ═══════════════════════════════════════════════════════════════════════════
//  ReferenceAnalyzer — Carga un archivo de audio de referencia y analiza
//  su espectro FFT + LUFS para compararlo contra el AudioAnalyzer del master.
//
//  Uso:
//    1. Llama loadFile(ruta) cuando el usuario carga una referencia
//    2. En cada ciclo de UI, llama compareWith(audioAnalyzer) para obtener
//       la diferencia instantánea entre la referencia y la mezcla actual
//    3. El ComparisonResult se puede mostrar en el chat del Coach o en
//       una UI de comparación espectral
// ═══════════════════════════════════════════════════════════════════════════
class ReferenceAnalyzer {
public:
    ReferenceAnalyzer() = default;
    ~ReferenceAnalyzer() = default;

    // ─── Carga y análisis ─────────────────────────────────────────────────
    /** Carga un archivo de audio, lo procesa por AudioAnalysis + LoudnessAnalyzer.
        Retorna true si el archivo se cargó y analizó correctamente.
        Esto es síncrono — puede tomar ~100-500ms para una canción completa. */
    bool loadFile(const juce::String& filePath);

    /** Carga una referencia desde un buffer ya en memoria (evita segunda lectura de disco).
        El buffer debe ser float32, rango [-1, 1].
        Retorna true si se analizó correctamente. */
    bool loadFromBuffer(const float* bufferL, const float* bufferR,
                        int64_t numSamples, int numChannels,
                        double sampleRate,
                        const juce::String& filePath);

    /** Limpia la referencia cargada. */
    void clear();

    /** ¿Hay una referencia cargada? */
    [[nodiscard]] bool hasReference() const noexcept { return loaded_; }

    // ─── Acceso a métricas de la referencia ───────────────────────────────
    [[nodiscard]] const AudioAnalysis& getAnalysis() const noexcept { return analysis_; }
    [[nodiscard]] float getMomentaryLUFS()  const noexcept;
    [[nodiscard]] float getShortTermLUFS()  const noexcept;
    [[nodiscard]] float getIntegratedLUFS() const noexcept;

    /** Energía promedio en una de las 7 bandas (0=Sub ... 6=Air). */
    [[nodiscard]] float getBandEnergy(int band) const noexcept;

    // ─── Comparación contra el master actual ──────────────────────────────
    /** Compara el espectro + LUFS de la referencia contra el AudioAnalyzer
        del master. Retorna un ComparisonResult con diferencias por banda,
        similitud espectral, y un resumen textual. */
    [[nodiscard]] ReferenceComparison compareWith(const AudioAnalyzer& master) const;

    // ─── Loudness Range + True Peak (desde LoudnessAnalyzer) ──────────────
    /** Rango de loudness (LRA) en LU. */
    [[nodiscard]] float getLoudnessRange() const noexcept { return loudness_.getRange(); }
    /** True Peak en dBTP. */
    [[nodiscard]] float getTruePeakDBTP() const noexcept { return loudness_.getTruePeak(); }

    // ─── Metadatos del archivo ────────────────────────────────────────────
    [[nodiscard]] juce::String getFileName() const noexcept { return fileName_; }
    [[nodiscard]] juce::String getFilePath() const noexcept { return filePath_; }
    [[nodiscard]] int getSampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] double getDuration() const noexcept { return duration_; }

    /** Retorna el buffer de audio completo (para análisis de secciones). */
    [[nodiscard]] const juce::AudioBuffer<float>& getAudioBuffer() const noexcept { return audioBuffer_; }

private:
    // ─── Análisis interno ─────────────────────────────────────────────────
    /** Procesa el buffer de audio por bloques a través de AudioAnalysis y
        LoudnessAnalyzer para generar el espectro y LUFS de la referencia. */
    void analyzeAudioBuffer(const juce::AudioBuffer<float>& buffer, int sampleRate);

    /** Calcula las 7 energías de banda desde el espectro FFT. */
    void computeBandEnergies();

    /** Helper: energía promedio de un rango de bins espectrales. */
    [[nodiscard]] float spectrumBandEnergy(const float* spectrum, int startBin, int endBin) const noexcept;

    // ═══ Estado ───────────────────────────────────────────────────────────
    bool loaded_ = false;
    juce::String filePath_;
    juce::String fileName_;
    int sampleRate_ = 0;
    double duration_ = 0.0;

    // Análisis de la referencia
    AudioAnalysis   analysis_;
    LoudnessAnalyzer loudness_;

    // Buffer de audio completo (cargado en RAM, para análisis off-line)
    juce::AudioBuffer<float> audioBuffer_;

    // Energía por banda espectral (mismas 7 bandas que CoachEngine)
    static constexpr int kNumBands = 7;
    float bandEnergies_[kNumBands] = {};

    // Constantes de banda (bins del espectro para cada banda @ FFT 1024 a 44100Hz)
    static constexpr int kBandBins[kNumBands][2] = {
        { 1, 5 },     // Sub:       43-215 Hz
        { 5, 12 },    // Bass:      215-516 Hz
        { 12, 35 },   // Low-Mid:   516-1500 Hz
        { 35, 70 },   // High-Mid:  1500-3000 Hz
        { 70, 120 },  // Presence:  3000-5160 Hz
        { 120, 200 }, // High:      5160-8600 Hz
        { 200, 300 }  // Air:       8600-12900 Hz
    };
    static constexpr const char* kBandNames[kNumBands] = {
        "Sub", "Bass", "Low-Mid", "High-Mid", "Presence", "High", "Air"
    };
};

} // namespace mixcoach
