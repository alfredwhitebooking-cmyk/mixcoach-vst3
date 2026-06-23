#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include "../../Common/types/Types.h"
#include "TrackRole.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  TrackHealth — Estado semántico de salud de la pista
//  Es el "diagnóstico" que el TrackStateBuilder produce a partir de los
//  datos crudos. El CoachEngine y la UI leen esto sin re-analizar.
// ═══════════════════════════════════════════════════════════════════════════
enum class TrackHealth : uint8_t {
    Clean,              // 🟢 Todo en rango
    NeedsEQ,            // 🟡 Problema espectral (demasiados graves/agudos)
    NeedsCompression,   // 🟡 Demasiada dinámica (crest alto)
    Overcompressed,     // 🔴 Crest muy bajo, dinámica escasa
    ClippingRisk,       // 🔴 Pico cerca de 0dBFS
    MaskingIssue,       // 🟡 Enmascaramiento con otra pista
    LowSignal,          // ⚪ Señal muy baja
    StereoCollapse,     // 🔴 Correlación negativa o casi nula
    PhaseIssue,         // 🟡 Problema de fase
    Silent,             // ⚪ Sin señal detectable
    Unknown             // Sin datos suficientes
};

// ═══════════════════════════════════════════════════════════════════════════
//  TrackEventType — Tipos de eventos que el TrackFeed puede generar
// ═══════════════════════════════════════════════════════════════════════════
enum class TrackEventType : uint8_t {
    LevelSpike,         // Pico repentino de nivel
    ClippingDetected,   // Se detectó clipping
    ClippingCleared,    // Dejó de clipear
    FrequencyMasking,   // Enmascaramiento con otra pista
    StereoCollapse,     // Correlación bajó peligrosamente
    LUFSTooLow,         // LUFS por debajo del target
    LUFSTooHigh,        // LUFS por encima del target
    PhaseIssue,         // Problema de fase / correlación negativa
    LowSignal,          // Señal muy baja
    GoodBalance,        // 🟢 Todo en rango
    CrestTooHigh,       // Crest factor muy alto (necesita compresión)
    CrestTooLow,        // Crest factor muy bajo (sobre-comprimido)
    SpectralImbalance,  // Desbalance tonal detectado
    TransientDetected,  // Transiente fuerte detectado
    RoleAssigned,       // TrackRole asignado (manual o inferido)
    TrackAppeared,      // Nueva pista detectada
    TrackDisappeared,   // Pista se desconectó
    Improvement,        // Mejora detectada respecto al ciclo anterior
    ReferenceGapImproved,   // ✅ Gap contra la referencia se redujo (mix mejorando)
    ReferenceGapWorsened,   // ⚠️ Gap contra la referencia aumentó (mix empeorando)
    Warning,            // Advertencia genérica (con mensaje)
    Info                // Informativo genérico
};

// ═══════════════════════════════════════════════════════════════════════════
//  TrackEvent — Un evento ocurrido en una pista
//  Cada evento tiene tipo, severidad, timestamp y mensaje para mostrar
//  en el UI feed o enviar al CoachEngine/LLM.
// ═══════════════════════════════════════════════════════════════════════════
struct TrackEvent {
    TrackEventType type        = TrackEventType::Info;
    int            trackId     = -1;          // slotIndex
    float          severity    = 0.0f;        // 0.0 (info) a 1.0 (critical)
    int64_t        timestampUs = 0;           // μs
    juce::String   message;                   // Texto legible para mostrar
    juce::String   context;                   // Contexto adicional (ej. "Bajo", "3.2dB sobre target")

    // Valores numéricos asociados (para debugging o display preciso)
    float value      = 0.0f;
    float threshold  = 0.0f;
    float deviation  = 0.0f;                 // valor - threshold

    [[nodiscard]] bool isProblem() const noexcept {
        return severity > 0.5f;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
//  TrackState — Estado UNIFICADO de una pista (TrackFeed V4)
//
//  UNIFICA:
//    • TrackAudioResult   → raw metrics desde background worker
//    • TrackAnalysisState → cooldowns y transiciones (CoachEngine)
//    • MessengerEntry     → UI state + sugerencias
//    • SlotInfo           → identidad (slotIndex, name, bus, colour)
//    • TrackRole          → rol semántico
//
//  NUEVO:
//    • health             → diagnóstico semántico (Clean / NeedsEQ / etc.)
//    • attentionScore     → prioridad para el coach (0.0 = baja, 1.0 = urgente)
//    • lastEventTimestamps → cooldowns por tipo de evento
// ═══════════════════════════════════════════════════════════════════════════
struct TrackState {
    // ═══ IDENTIDAD (desde SlotInfo + TrackRole) ═══════════════════════════
    int         slotIndex   = -1;
    juce::String trackName;
    juce::Colour colour     = juce::Colours::grey;
    BusType     bus         = BusType::None;
    TrackRole   role        = TrackRole::Unknown;
    bool        active      = false;

    // ═══ AUDIO RAW (desde TrackAudioResult / background worker) ═══════════
    float peakLeft    = -100.0f;   // dB
    float peakRight   = -100.0f;   // dB
    float rmsLeft     = -100.0f;   // dB
    float rmsRight    = -100.0f;   // dB
    float correlation = 0.0f;      // -1 a +1
    int64_t timestampUs = 0;       // Última actualización

    // ═══ ESPECTRO (30-bandas + 6 regiones) ════════════════════════════════
    std::array<float, 30> bandEnergies{};      // dBFS cada banda
    std::array<float, 6>  crestPerBand{};      // dB
    std::array<float, 6>  stereoWidthPerBand{};// 0-1
    std::array<float, 6>  midEnergyPerBand{};  // dB
    std::array<float, 6>  sideEnergyPerBand{}; // dB
    float transientRatio    = 0.0f;
    float attackTimeMs      = 0.0f;
    float releaseTimeMs     = 0.0f;
    float sustainLevelDb    = -100.0f;

    // ═══ DERIVADO (computado por TrackStateBuilder) ═══════════════════════
    float peakCombined  = -100.0f;  // dB, max(L,R)
    float rmsCombined   = -100.0f;  // dB, max(L,R)
    float crestFactor   = 0.0f;     // peak - rms (dB)
    float avgStereoWidth = 0.0f;    // promedio de stereoWidthPerBand

    // ═══ SEMÁNTICO (nuevo en TrackFeed) ═══════════════════════════════════
    TrackHealth health          = TrackHealth::Unknown;
    float       attentionScore  = 0.0f;  // 0.0 → 1.0 (prioridad para el coach)

    // ═══ TRANSICIONAL (desde TrackAnalysisState) ══════════════════════════
    bool  wasClipping   = false;   // ¿Estaba clipeando en el ciclo anterior?
    bool  wasLowSignal  = false;   // ¿Tenía señal baja en el ciclo anterior?
    float prevCrest     = 0.0f;    // Crest del ciclo anterior (para detectar cambios)
    float prevPeak      = -100.0f; // Peak del ciclo anterior

    // ═══ COOLDOWNS POR TIPO DE EVENTO (μs) ════════════════════════════════
    int64_t lastClipWarningUs       = 0;
    int64_t lastLowSignalUs         = 0;
    int64_t lastCrestWarningUs      = 0;
    int64_t lastPhaseWarningUs      = 0;
    int64_t lastSpectralWarningUs   = 0;
    int64_t lastStereoWarningUs     = 0;
    int64_t lastTransientWarningUs  = 0;

    // ═══ HELPERS ═══════════════════════════════════════════════════════════

    [[nodiscard]] bool hasSignal() const noexcept {
        return peakCombined > -60.0f && timestampUs > 0;
    }

    [[nodiscard]] bool isClipping() const noexcept {
        return peakCombined > -0.5f;
    }

    [[nodiscard]] float getPeakCombinedDb() const noexcept {
        return juce::jmax(peakLeft, peakRight);
    }

    [[nodiscard]] float getRmsCombinedDb() const noexcept {
        return juce::jmax(rmsLeft, rmsRight);
    }

    [[nodiscard]] bool isStale(int64_t nowUs, int64_t timeoutUs = 3000000) const noexcept {
        return (nowUs - timestampUs) > timeoutUs;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
//  Constantes de cooldown para TrackState
// ═══════════════════════════════════════════════════════════════════════════
namespace TrackStateDefaults {
    inline constexpr int64_t kCooldownWarningUs    = 60 * 1000 * 1000;   // 60s entre warnings del mismo tipo
    inline constexpr int64_t kCooldownTrackUs      = 120 * 1000 * 1000;  // 2min entre eventos de la misma pista
    inline constexpr int64_t kStaleTimeoutUs       = 3 * 1000 * 1000;    // 3s sin datos = pista desconectada
    inline constexpr int64_t kEventRetentionUs     = 60 * 1000 * 1000;   // 60s de historial de eventos
    inline constexpr int   kMaxEventsPerTrack      = 64;                  // Máximo eventos guardados por pista
    inline constexpr int   kMaxGlobalEvents        = 256;                 // Máximo eventos globales
}

} // namespace mixcoach
