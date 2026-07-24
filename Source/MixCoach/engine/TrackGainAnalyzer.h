#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include "../../Common/memory/SharedData.h"
#include "../../Common/memory/SlotRegistry.h"
#include "../../Common/types/Types.h"
#include "TrackRole.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  TrackGainAdvice — Análisis de ganancia por pista (Sprint 6A)
    //  Compara el nivel actual contra el target del rol (ExpectedProfile)
    //  y genera un mensaje accionable para el usuario.
    //  Sin IA — reglas en C++, 0 tokens, instantáneo.
    // ═══════════════════════════════════════════════════════════════════════════
    struct TrackGainAdvice
    {
        int slotIndex = -1;
        juce::String trackName;
        TrackRole role = TrackRole::Unknown;

        // Valores actuales
        float currentPeak  = -100.0f;
        float currentRMS   = -100.0f;
        float currentLUFS  = -100.0f;
        float currentCrest = 0.0f;

        // Targets del rol
        float peakTarget    = -8.0f;
        float crestTarget   = 10.0f;
        float peakTolerance = 4.0f;

        // Desviación vs target (dB, positivo = más fuerte que target)
        float peakDeviation = 0.0f;

        // Cambio sugerido (dB, positivo = subir fader)
        float suggestedDeltaDb = 0.0f;

        enum class Status : uint8_t
        {
            OnTarget,   // Dentro del rango ± tolerance
            NearTarget, // Cerca del target (± 2*tolerance)
            OffTarget,  // Fuera del rango aceptable
            NoSignal,   // Sin señal detectable
            UnknownRole // Rol no especificado
        };
        Status status = Status::UnknownRole;

        // Mensaje accionable para el usuario
        juce::String message;

        [[nodiscard]] bool isActionable() const noexcept
        {
            return status == Status::OffTarget || status == Status::NearTarget;
        }
    };

    // ═══ Helpers compartidos entre módulos de análisis ═══════════════════════
    /** Obtiene TrackTelemetry desde SharedData para un slot. */
    [[nodiscard]] TrackTelemetry getLatestTelemetry(SharedData& sharedData, int slotIndex);

    /** Computa una aproximación de LUFS per-pista usando RMS + K-weighting. */
    [[nodiscard]] float computePerTrackLUFS(const TrackAudioResult& result) noexcept;

    // ═══ Free functions — Track Gain Analysis ═══════════════════════════════
    /** Analiza la ganancia de una pista contra el target del rol. */
    [[nodiscard]] TrackGainAdvice analyzeTrackGain(
        int slotIndex,
        SharedData& sharedData,
        const std::array<TrackRole, SlotRegistry::kMaxSlots>& trackRoles,
        const juce::String& setupGenre,
        bool soloActive);

    /** Analiza TODAS las pistas activas y retorna advices ordenados por severidad. */
    [[nodiscard]] std::vector<TrackGainAdvice> analyzeAllTracksGain(
        SharedData& sharedData,
        const std::array<TrackRole, SlotRegistry::kMaxSlots>& trackRoles,
        const juce::String& setupGenre,
        bool soloActive);

} // namespace mixcoach
