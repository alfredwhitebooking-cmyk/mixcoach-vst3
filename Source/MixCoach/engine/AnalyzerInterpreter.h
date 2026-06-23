#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include "../../Common/audio/DiagnosticBridge.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  AnalyzerInterpretation — Resultado de interpretar una lectura de analizador
//
//  No es solo un dato — es una interpretación con consecuencias y acciones.
//  Representa el Nivel 4 de conocimiento: "¿Qué significa esto para mi mezcla?"
// ═══════════════════════════════════════════════════════════════════════════
struct AnalyzerInterpretation {
    // Dominio del analizador que produjo esta interpretación
    enum class Domain : uint8_t {
        Correlation,  // Phase correlation meter
        Crest,        // Crest factor / dynamics
        Centroid,     // Spectral centroid
        LUFS,         // Loudness (I, S, M, TP, LRA)
        PhaseScope,   // Vectorscope / goniometer
        SpectralBand, // RTA / frequency band energy
        Gain,         // Peak / RMS levels
        MidSide,      // Mid-Side balance
        StereoWidth   // Stereo width per band
    };
    Domain domain = Domain::Correlation;

    // La lectura en texto legible (e.g. "Correlation = -0.23")
    juce::String reading;

    // El nombre de la interpretación (e.g. "FASE INVERTIDA")
    juce::String interpretation;

    // Qué significa para la mezcla (e.g. "Posible cancelación de fase en mono")
    juce::String consequence;

    // Qué hacer al respecto (e.g. "Revisa polaridad de micrófonos o wideners estéreo")
    juce::String action;

    // Severidad 0.0 (info) a 1.0 (crítico)
    float severity = 0.0f;

    // Crítico: requiere atención inmediata
    bool isCritical = false;

    // Praise: algo que está bien (logro)
    bool isPraise = false;

    // Offsets espectrales (si aplica al dominio)
    float lowFreqHz  = 0.0f;
    float highFreqHz = 0.0f;

    // Nombre de pista asociada (vacío = master)
    juce::String trackName;
    juce::String trackRole;

    // Texto completo listo para el coach (incluye reading + interpretation + consequence + action)
    [[nodiscard]] juce::String toFullMessage() const;

    // Convierte a BandDiagnostic para overlay visual
    [[nodiscard]] BandDiagnostic toBandDiagnostic() const;
};

// ═══════════════════════════════════════════════════════════════════════════
//  AnalyzerInterpreter — Intérprete de analizadores (Nivel 4 de conocimiento)
//
//  Toma lecturas crudas de los analizadores (correlación, crest, LUFS, etc.)
//  y produce interpretaciones semánticas con:
//    - Qué significa la lectura
//    - Por qué es importante (consecuencia)
//    - Qué hacer (acción recomendada)
//
//  Todo el conocimiento de mapeo proviene de:
//    knowledge/interpretation/analyzer_readings.md
// ═══════════════════════════════════════════════════════════════════════════
class AnalyzerInterpreter {
public:
    // ─── Interpretación individual por dominio ──────────────────────────

    /** Interpreta la lectura del correlation meter.
        @param correlation  Valor de correlación (-1.0 a +1.0)
        @param genre        Género musical (para contexto) */
    static AnalyzerInterpretation interpretCorrelation(float correlation,
                                                       const juce::String& genre = {});

    /** Interpreta el crest factor (dinámica).
        @param crestDb  Crest factor en dB (Peak - RMS)
        @param genre    Género musical (para contexto) */
    static AnalyzerInterpretation interpretCrest(float crestDb,
                                                 const juce::String& genre = {});

    /** Interpreta el spectral centroid (brillo relativo).
        @param centroidRatio  Ratio vs referencia (1.0 = igual, 1.2 = +20% brillante)
        @param genre          Género musical */
    static AnalyzerInterpretation interpretCentroid(float centroidRatio,
                                                    const juce::String& genre = {});

    /** Interpreta métricas de loudness (LUFS + True Peak + LRA).
        @param integratedLUFS  LUFS integrado (EBU R128)
        @param truePeakDBTP    True Peak en dBTP
        @param lra             Loudness Range en LU
        @param genre           Género musical */
    static AnalyzerInterpretation interpretLUFS(float integratedLUFS,
                                                float truePeakDBTP,
                                                float lra,
                                                const juce::String& genre = {});

    /** Interpreta el phase scope / vectorscope.
        @param correlation  Correlación (para contexto de mono compat)
        @param stereoWidth  Ancho estéreo percibido (0.0 = mono, 1.0 = muy ancho) */
    static AnalyzerInterpretation interpretPhaseScope(float correlation,
                                                      float stereoWidth);

    /** Interpreta la energía de una banda espectral específica.
        @param bandEnergyDb   Energía de la banda (dBFS)
        @param bandIndex      Índice de banda (0=Sub, 1=Bass, ..., 6=Air)
        @param bandName       Nombre de la banda (e.g. "Sub", "Presence")
        @param referenceDb    Valor de referencia (dBFS) */
    static AnalyzerInterpretation interpretSpectralBand(float bandEnergyDb,
                                                        int bandIndex,
                                                        const juce::String& bandName,
                                                        float referenceDb = -100.0f);

    // ─── Generación masiva ─────────────────────────────────────────────

    /** Genera TODAS las interpretaciones activas en un solo llamado.
        Útil para el timer periódico que actualiza el DiagnosticBridge.
        @param correlation      Correlación estéreo
        @param crestDb          Crest factor promedio (dB)
        @param centroidRatio    Ratio de centroid vs referencia
        @param integratedLUFS   LUFS integrado
        @param truePeakDBTP     True Peak (dBTP)
        @param lra              Loudness Range (LU)
        @param genre            Género musical
        @return Lista de interpretaciones activas (con severidad > 0) */
    static std::vector<AnalyzerInterpretation> interpretAll(float correlation,
                                                             float crestDb,
                                                             float centroidRatio,
                                                             float integratedLUFS,
                                                             float truePeakDBTP,
                                                             float lra,
                                                             const juce::String& genre = {});

    /** Convierte una lista de interpretaciones a BandDiagnostics para overlay.
        @param interpretations  Lista de interpretaciones activas
        @return Lista de BandDiagnostic listos para DiagnosticBridge */
    static std::vector<BandDiagnostic> toBandDiagnostics(
        const std::vector<AnalyzerInterpretation>& interpretations);

    /** Genera un mensaje de coach resumido con las interpretaciones más importantes.
        @param interpretations  Lista de interpretaciones activas
        @param maxEntries       Máximo de entradas a incluir
        @return Texto formateado para el chat del coach */
    static juce::String formatCoachMessage(
        const std::vector<AnalyzerInterpretation>& interpretations,
        int maxEntries = 3);

    // ─── Per-Track interpretations ──────────────────────────────────────

    /** Interpreta los niveles de gain (peak y RMS por canal L/R).
        Detecta clipping, near-clipping, L/R imbalance, y señal baja.
        @param peakLeft   Peak del canal izquierdo (dBFS)
        @param peakRight  Peak del canal derecho (dBFS)
        @param rmsLeft    RMS del canal izquierdo (dBFS)
        @param rmsRight   RMS del canal derecho (dBFS) */
    static AnalyzerInterpretation interpretGain(float peakLeft, float peakRight,
                                                 float rmsLeft, float rmsRight);

    /** Interpreta el ancho estéreo por banda espectral (6 bandas).
        @param stereoWidthPerBand  Ancho estéreo por banda (0.0=mono, 1.0=muy ancho)
        @param avgStereoWidth      Ancho estéreo promedio (0.0-1.0) */
    static AnalyzerInterpretation interpretStereoWidth(
        const float stereoWidthPerBand[6], float avgStereoWidth);

    /** Interpreta el balance Mid/Side por banda espectral (6 bandas).
        @param midEnergyPerBand   Energía del canal Mid por banda (dBFS)
        @param sideEnergyPerBand  Energía del canal Side por banda (dBFS)
        @param avgStereoWidth     Ancho estéreo promedio (para contexto) */
    static AnalyzerInterpretation interpretMidSide(
        const float midEnergyPerBand[6], const float sideEnergyPerBand[6],
        float avgStereoWidth);

    // ─── Helpers de contexto ──────────────────────────────────────────

    /** Retorna el target de crest para un género. */
    static float getCrestTarget(const juce::String& genre);

    /** Retorna el target de LUFS integrado para un género. */
    static float getLUFSTarget(const juce::String& genre);

    /** Retorna el rango de crest saludable para un género. */
    static juce::String getCrestRangeString(const juce::String& genre);

private:
    // ─── Mapeo estático: lectura → interpretación ─────────────────────
    static const char* interpretCorrelationLabel(float correlation);
    static const char* interpretCrestLabel(float crestDb, const juce::String& genre);
    static const char* interpretCentroidLabel(float ratio, const juce::String& genre);
    static float getSeverityFromCorrelation(float correlation);
    static float getSeverityFromCrest(float crestDb, const juce::String& genre);
    static bool isCrestCritical(float crestDb, const juce::String& genre);

    // ─── Nombres de bandas espectrales ────────────────────────────────
    static constexpr const char* kBandNames[7] = {
        "Sub", "Bass", "Low-Mid", "High-Mid", "Presence", "High", "Air"
    };
    static constexpr float kBandFreqs[7][2] = {
        { 20.0f, 86.0f },     // Sub
        { 86.0f, 301.0f },    // Bass
        { 301.0f, 1076.0f },  // Low-Mid
        { 1076.0f, 3532.0f }, // High-Mid
        { 3532.0f, 8355.0f }, // Presence
        { 8355.0f, 16458.0f },// High
        { 16458.0f, 20000.0f }// Air
    };
};

} // namespace mixcoach
