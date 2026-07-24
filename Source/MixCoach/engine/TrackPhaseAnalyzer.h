#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include "../../Common/memory/SharedData.h"
#include "../../Common/memory/SlotRegistry.h"
#include "../../Common/types/Types.h"
#include "TrackRole.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  TrackPhaseAdvice — Análisis de fase estéreo por pista (Sprint 8)
    //  Compara la correlación actual contra umbrales por rol.
    //  Sin IA — reglas en C++, 0 tokens, instantáneo.
    // ═══════════════════════════════════════════════════════════════════════════
    struct TrackPhaseAdvice
    {
        int slotIndex = -1;
        juce::String trackName;
        TrackRole role = TrackRole::Unknown;

        float currentCorrelation = 0.0f;
        float currentPeak        = -100.0f;
        float correlationDeviation = 0.0f;

        enum class Status : uint8_t
        {
            OnTarget,
            NearTarget,
            OffTarget,
            NoSignal,
            UnknownRole
        };
        Status status = Status::UnknownRole;

        juce::String message;

        [[nodiscard]] bool isActionable() const noexcept
        {
            return status == Status::OffTarget || status == Status::NearTarget;
        }
    };

    // ═══ Free functions — Track Phase Analysis ════════════════════════════════
    [[nodiscard]] TrackPhaseAdvice analyzeTrackPhase(
        int slotIndex,
        SharedData& sharedData,
        const std::array<TrackRole, SlotRegistry::kMaxSlots>& trackRoles,
        bool soloActive);

    [[nodiscard]] std::vector<TrackPhaseAdvice> analyzeAllTracksPhase(
        SharedData& sharedData,
        const std::array<TrackRole, SlotRegistry::kMaxSlots>& trackRoles,
        bool soloActive);

} // namespace mixcoach
