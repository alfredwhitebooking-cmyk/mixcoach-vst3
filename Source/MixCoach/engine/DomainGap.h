#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Domain — Los 5 dominios de análisis de mezcla contra referencia
    //  Mismos valores que en ReferenceDrivenEngine.h (extraídos aquí para
    //  que DifferenceProfile.h pueda usarlos sin arrastrar todo AudioAnalyzer)
    // ═══════════════════════════════════════════════════════════════════════════
    enum class Domain : uint8_t
    {
        Gain,     // LUFS, true peak, headroom
        Tonal,    // Espectro 30-bandas, 6 regiones
        Dynamics, // Crest factor, transient ratio
        Spatial,  // Correlación, stereo width
        Loudness  // Loudness range
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  Severity — Qué tan lejos está el gap de su objetivo
    // ═══════════════════════════════════════════════════════════════════════════
    enum class GapSeverity : uint8_t
    {
        Critical, // > 2x threshold — needs immediate attention
        Warning,  // > threshold — should address
        Info,     // Within threshold but not optimal
        Praise    // At or better than target — acknowledge
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  DomainGap — Una diferencia específica entre la mezcla y la referencia
    // ═══════════════════════════════════════════════════════════════════════════
    struct DomainGap
    {
        Domain domain;        // Gain, Tonal, Dynamics, Spatial, Loudness
        GapSeverity severity; // Qué tan grave es
        int priority;         // 1 (más urgente) a 5 (menos urgente)

        juce::String metric; // "LUFS Integrated", "Sub-Bass (43-86Hz)", etc.
        float actualValue;   // Lo que la mezcla tiene AHORA
        float targetValue;   // Lo que la REFERENCIA tiene
        float gap;           // actual - target (dB o unidades nativas)
        float normalisedGap; // 0.0 = igual a ref, 1.0 = en el límite

        juce::String description;   // "El Sub está 4.2dB por debajo de la referencia"
        juce::String suggestion;    // "Sube 2dB en 60Hz con un EQ shelving"
        juce::String frequencyHint; // "60Hz" o "43-86Hz" — para display en UI
        juce::String actionVerb;    // "subir", "reducir", "ajustar"

        // Para mostrar en el chat (implementados en ReferenceDrivenEngine.cpp)
        [[nodiscard]] juce::String toTextSummary() const;
        [[nodiscard]] juce::String toShortLabel() const;

        /** Versión optimizada para LLM context: incluye severidad con ícono,
            dominio, gap numérico (actual vs target con unidades), y sugerencia
            accionable. Formato compacto (~200 chars por gap). */
        [[nodiscard]] juce::String toLLMContextGap() const;
    };

} // namespace mixcoach
