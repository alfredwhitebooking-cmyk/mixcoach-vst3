#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include "DomainGap.h"

namespace mixcoach {

// Forward declarations — evitamos incluir AudioAnalyzer.h, ReferenceAnalyzer.h
// y sus cadenas pesadas (juce_audio_basics, etc.)
struct ReferenceFingerprint;
class AudioAnalyzer;
struct ReferenceComparison;

// ═══════════════════════════════════════════════════════════════════════════
//  DifferenceProfile — Unifica en un solo struct:
//    • Current (lo que la mezcla tiene AHORA)
//    • Reference (lo que la REFERENCIA tiene)
//    • Delta (la diferencia entre ambas)
//
//  Combina datos que antes estaban dispersos en:
//    - ReferenceFingerprint  (datos de la referencia)
//    - ReferenceComparison   (diferencia por bandas + similitud)
//    - ReferenceMatchData    (datos para la UI de matching espectral)
//    - DomainGap             (gaps priorizados por dominio)
//
//  Es persistente: se guarda en session_memory.json via toJson/fromJson.
// ═══════════════════════════════════════════════════════════════════════════
struct DifferenceProfile {
    // ═══════════════════════════════════════════════════════════════════════
    //  Metadata
    // ═══════════════════════════════════════════════════════════════════════
    bool         valid            = false;
    int64_t      timestampUs      = 0;
    juce::String referenceName;
    juce::String referencePath;
    juce::String activeSectionLabel;  // "Full", "Chorus", "Verse", etc.
    double       durationSeconds = 0.0;

    // ═══════════════════════════════════════════════════════════════════════
    //  Current Mix — desde AudioAnalyzer en vivo
    // ═══════════════════════════════════════════════════════════════════════
    float mixIntegratedLUFS  = -100.0f;
    float mixShortTermLUFS   = -100.0f;
    float mixMomentaryLUFS   = -100.0f;
    float mixCrestFactor     = 0.0f;
    float mixCorrelation     = 0.0f;
    float mixTruePeakDBTP    = -100.0f;
    float mixLoudnessRange   = 0.0f;
    float mixSpectralCentroidHz = 0.0f;

    // 6 regiones espectrales (Sub, Bass, Low-Mid, High-Mid, Presence, Air)
    float mixRegionEnergy[6] = { -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f };

    // ═══════════════════════════════════════════════════════════════════════
    //  Reference — desde ReferenceFingerprint
    // ═══════════════════════════════════════════════════════════════════════
    float refIntegratedLUFS  = -100.0f;
    float refShortTermLUFS   = -100.0f;
    float refMomentaryLUFS   = -100.0f;
    float refCrestFactor     = 0.0f;
    float refCorrelation     = 0.0f;
    float refTruePeakDBTP    = -100.0f;
    float refLoudnessRange   = 0.0f;
    float refSpectralCentroidHz = 0.0f;

    // 6 regiones espectrales (Sub, Bass, Low-Mid, High-Mid, Presence, Air)
    float refRegionEnergy[6] = { -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f };

    // ═══════════════════════════════════════════════════════════════════════
    //  Delta (ref - mix, positivo = referencia tiene más)
    // ═══════════════════════════════════════════════════════════════════════
    float deltaLUFS           = 0.0f;  // ref - mix integrated LUFS
    float deltaShortTermLUFS  = 0.0f;
    float deltaCrestFactor   = 0.0f;
    float deltaCorrelation   = 0.0f;
    float deltaTruePeak      = 0.0f;
    float deltaLoudnessRange = 0.0f;
    float deltaCentroidHz    = 0.0f;

    // Delta por región espectral (ref - mix en dB)
    float deltaRegionEnergy[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    // ═══════════════════════════════════════════════════════════════════════
    //  Overall match scores
    // ═══════════════════════════════════════════════════════════════════════
    float spectralSimilarity = 0.0f;  // 0.0-1.0 desde ReferenceComparison
    float deltaScore         = 0.0f;  // Score agregado simple: 1.0 = idéntico, 0.0 = nada que ver

    // ═══════════════════════════════════════════════════════════════════════
    //  Domain gap summary (desde ReferenceDrivenEngine::computeGaps)
    // ═══════════════════════════════════════════════════════════════════════
    int totalGaps    = 0;
    int criticalGaps = 0;
    int warningGaps  = 0;
    int infoGaps     = 0;
    int praiseCount  = 0;

    // Gaps detallados (para UI o LLM)
    std::vector<DomainGap> domainGaps;

    // ─── Helpers ─────────────────────────────────────────────────────────
    [[nodiscard]] bool hasData() const noexcept {
        return valid && mixIntegratedLUFS > -90.0f && refIntegratedLUFS > -90.0f;
    }

    /** Retorna true si hay al menos un gap crítico o warning. */
    [[nodiscard]] bool hasIssues() const noexcept {
        return criticalGaps > 0 || warningGaps > 0;
    }

    /** Retorna el gap más severo (o nullptr si no hay gaps). */
    [[nodiscard]] const DomainGap* getMostSevereGap() const noexcept {
        if (domainGaps.empty()) return nullptr;
        const DomainGap* worst = &domainGaps[0];
        for (const auto& g : domainGaps) {
            if (static_cast<int>(g.severity) < static_cast<int>(worst->severity))
                worst = &g;
        }
        return worst;
    }

    // ═══════════════════════════════════════════════════════════════════════
    //  Builder — Construye un DifferenceProfile desde fuentes existentes
    // ═══════════════════════════════════════════════════════════════════════

    /** Construye un DifferenceProfile completo desde los datos disponibles.
        @param fp              Fingerprint de la referencia (30 bandas + LUFS + crest)
        @param master          AudioAnalyzer del master (datos actuales de la mezcla)
        @param refComparison   ReferenceComparison (última comparación mix vs ref)
        @param gaps             Gaps priorizados desde ReferenceDrivenEngine
        @param refName         Nombre de la referencia
        @param refPath         Ruta de la referencia
        @param sectionLabel    Etiqueta de la sección activa ("Full", "Chorus", etc.) */
    static DifferenceProfile build(
        const ReferenceFingerprint& fp,
        const AudioAnalyzer& master,
        const ReferenceComparison& refComparison,
        const std::vector<DomainGap>& gaps,
        const juce::String& refName = {},
        const juce::String& refPath = {},
        const juce::String& sectionLabel = "Full");

    // ═══════════════════════════════════════════════════════════════════════
    //  Serialization
    // ═══════════════════════════════════════════════════════════════════════

    /** Serializa este DifferenceProfile a un DynamicObject. */
    void toJson(juce::DynamicObject& obj) const;

    /** Deserializa desde un DynamicObject. */
    static DifferenceProfile fromJson(const juce::DynamicObject& obj);

    /** Genera un resumen textual formateado para el chat o LLM context.
        Ejemplo:
          [DIFFERENCE PROFILE: MyReference.wav vs Mix - Full]
          LUFS: ref -10.2 | mix -8.5 | delta +1.7 LUFS (mix LOUDER)
          Crest: ref 8.0dB | mix 9.2dB | delta +1.2dB (mix MORE DYNAMIC)
          Similarity: 0.72 | Gaps: 2 crit, 1 warn, 0 info */
    [[nodiscard]] juce::String toTextSummary() const;

    /** Versión detallada para depuración. */
    [[nodiscard]] juce::String toVerboseText() const;

private:
    // ═══ Helpers internos ════════════════════════════════════════════════

    /** Agrupa 30 bandas espectrales en 6 regiones. */
    static void computeRegionEnergies(
        const float bandEnergies[30],
        float regionEnergies[6]) noexcept;

    /** Computa el delta score agregado (0.0-1.0) desde los deltas individuales. */
    static float computeDeltaScore(
        const float regionDeltas[6],
        float deltaLUFS,
        float deltaCrest) noexcept;
};

} // namespace mixcoach
