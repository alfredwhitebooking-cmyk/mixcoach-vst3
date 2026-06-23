#pragma once

#include "DomainGap.h"
#include <juce_core/juce_core.h>
#include "../audio/AudioAnalyzer.h"

namespace mixcoach {
    struct ReferenceFingerprint; // Forward decl para romper ciclo CoachEngine.h ↔ ReferenceDrivenEngine.h

    // ═══════════════════════════════════════════════════════════════════════════
    //  ReferenceDrivenEngine — Produce un plan estructurado de acción
    //  comparando la mezcla actual contra la referencia cargada.
    //
    //  Uso:
    //    auto gaps = ReferenceDrivenEngine::computeGaps(
    //        referenceFingerprint, audioAnalyzer, genre);
    //
    //    for (auto& gap : gaps)
    //        coach.respondWith(gap.toTextSummary());
    //
    //  Los gaps SALEN ORDENADOS por prioridad (Gain > Tonal > Dynamics > ...)
    //  y dentro de cada dominio, por severidad (Critical > Warning > Info).
    // ═══════════════════════════════════════════════════════════════════════════
    class ReferenceDrivenEngine
    {
    public:
        /** Produce gaps priorizados entre la mezcla actual y la referencia.
            @param fp        Fingerprint de la referencia (30 bandas + LUFS + crest)
            @param master    AudioAnalyzer del master (datos actuales de la mezcla)
            @param genre     Género para target de referencia (si no hay fingerprint)
            @return Vector de DomainGap ordenado por prioridad y severidad */
        static std::vector<DomainGap>
        computeGaps(const ReferenceFingerprint& fp, const AudioAnalyzer& master, const juce::String& genre = "Unknown");

        // ─── Helpers estáticos ───────────────────────────────────────────────
        static const char* domainName(Domain d) noexcept;
        static const char* severityLabel(GapSeverity s) noexcept;
        static juce::Colour severityColour(GapSeverity s) noexcept;

    private:
        // ─── Análisis por dominio ────────────────────────────────────────────
        static DomainGap analyzeGainDomain(const ReferenceFingerprint& fp, const AudioAnalyzer& master);

        static std::vector<DomainGap> analyzeTonalDomain(const ReferenceFingerprint& fp, const AudioAnalyzer& master);

        static DomainGap analyzeDynamicsDomain(const ReferenceFingerprint& fp, const AudioAnalyzer& master);

        static DomainGap analyzeSpatialDomain(const ReferenceFingerprint& fp, const AudioAnalyzer& master);

        static DomainGap analyzeLoudnessRangeDomain(const ReferenceFingerprint& fp, const AudioAnalyzer& master);

        // ─── Helpers de agrupación espectral ─────────────────────────────────
        /** Agrupa 30 bandas en 6 regiones (Sub, Bass, Low-Mid, High-Mid, Presence, Air). */
        static float regionEnergy(const float bandEnergies[30], int regionStartBand, int regionEndBand);

        /** Nombre de la región espectral por índice (0-5). */
        static const char* regionName(int regionIdx) noexcept;

        /** Frecuencia central aproximada de una región para sugerencias EQ. */
        static const char* regionCenterFreq(int regionIdx) noexcept;

        /** Rango de frecuencia de una región como string. */
        static const char* regionFreqRange(int regionIdx) noexcept;

        /** Tipo de filtro EQ recomendado para la región. */
        static const char* regionFilterType(int regionIdx) noexcept;

        /** Q recomendado para la región. */
        static float regionQ(int regionIdx) noexcept;

        /** Clase de instrumento objetivo para la región. */
        static const char* regionInstrumentTarget(int regionIdx) noexcept;

        /** Ganancia de corrección sugerida (no simplemente 50% del gap).
            Regresa cuánto aplicar en el EQ (dB), considerando que a veces
            menos es más (especialmente en Sub y Air). */
        static float suggestedEqGainDb(float gapDb, int regionIdx) noexcept;

        // ─── Thresholds ─────────────────────────────────────────────────────
        static constexpr float kLufsCriticalThreshold = 3.0f;  // LUFS
        static constexpr float kLufsWarningThreshold  = 1.5f;  // LUFS
        static constexpr float kSpectralCriticalDb    = 6.0f;  // dB por región
        static constexpr float kSpectralWarningDb     = 3.0f;  // dB por región
        static constexpr float kCrestCriticalDb       = 6.0f;  // dB
        static constexpr float kCrestWarningDb        = 3.0f;  // dB
        static constexpr float kCorrelationThreshold  = 0.15f; // correlation units
        static constexpr float kLoudnessRangeCritical = 4.0f;  // LU
        static constexpr float kLoudnessRangeWarning  = 2.0f;  // LU
    };


} // namespace mixcoach
