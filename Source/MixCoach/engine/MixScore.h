#pragma once

#include <juce_core/juce_core.h>

namespace mixcoach {

    // Forward declarations
    class CoachEngine;
    class AudioAnalyzer;

    // ═══════════════════════════════════════════════════════════════════════════
    //  MixScore — Puntaje de salud de la mezcla 0-100
    //
    //  Combina datos del CoachEngine (análisis por fase) + AudioAnalyzer
    //  (master bus) + ReferenceDrivenEngine (gaps contra referencia) en
    //  un puntaje único y sub-puntajes por dominio.
    //
    //  Cada dominio tiene sub-métricas que explican POR QUÉ el score es bajo.
    //
    //  Uso:
    //    auto score = MixScore::compute(coachEngine, audioAnalyzer, genre);
    //    respondWith(score.toTextSummary());
    //
    //  Pesos dinámicos:
    //    Gain:      25% (30% sin ref)
    //    Tonal:     25% (30% sin ref)
    //    Dynamics:  20%
    //    Spatial:   15% (20% sin ref)
    //    Reference: 15% (0% sin ref)
    // ═══════════════════════════════════════════════════════════════════════════
    struct MixScore
    {
        // ═══ Scores principales 0-100 ═════════════════════════════════════════
        int overall   = 0; // Weighted average
        int gain      = 0; // Gain staging health
        int tonal     = 0; // Spectral balance
        int dynamics  = 0; // Dynamic range & loudness
        int spatial   = 0; // Phase & stereo
        int reference = 0; // Reference alignment (0 if no reference loaded)

        // ═══ Breakdown por dominio — submétricas 0-100 ═══════════════════════
        // Gain
        int gainClipping  = 100; // Tracks near or at clipping
        int gainHeadroom  = 100; // Master peak headroom
        int gainLRBalance = 100; // L/R peak balance
        int gainLowSignal = 100; // Tracks with abnormally low signal

        // Tonal
        int tonalMasterSpec   = 100; // Master spectrum vs genre target
        int tonalPerTrack     = 100; // Per-track spectral balance (via SemanticComparator)
        int tonalRefAlignment = 100; // Alignment with reference (if loaded)

        // Dynamics
        int dynLoudness   = 100; // Integrated LUFS vs target
        int dynCrest      = 100; // Crest factor / dynamic range
        int dynTransients = 100; // Per-track transient control
        int dynTruePeak   = 100; // True peak / ISP safety

        // Spatial
        int spatCorrelation = 100; // Master phase correlation
        int spatStereoWidth = 100; // Per-track stereo width balance
        int spatMonoCompat  = 100; // Mono compatibility

        // ═══ Estadísticas adicionales ══════════════════════════════════════════
        int activeTrackCount       = 0;
        int clippingTrackCount     = 0;
        int lowSignalTrackCount    = 0;
        float masterPeakDb         = -100.0f;
        float masterIntegratedLUFS = -100.0f;
        float masterCorrelation    = 0.0f;
        bool hasReference          = false;
        juce::String genre;

        // ═══ Label descriptivo ═════════════════════════════════════════════════
        juce::String statusLabel; // "Excelente", "Buena", "Regular", "Necesita trabajo", "Critica"

        /** Compute el MixScore a partir de los datos actuales del motor y analyzer. */
        static MixScore compute(const CoachEngine& engine, const AudioAnalyzer& analyzer, const juce::String& genre);

        /** Texto corto formateado para el contexto del LLM. */
        [[nodiscard]] juce::String toTextSummary() const;

        /** Texto detallado con breakdown completo para mostrar en UI o enviar al chat. */
        [[nodiscard]] juce::String toDetailedString() const;
    };

} // namespace mixcoach
