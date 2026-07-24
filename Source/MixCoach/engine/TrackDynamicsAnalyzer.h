#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include "../../Common/memory/SharedData.h"
#include "../../Common/memory/SlotRegistry.h"
#include "../../Common/types/Types.h"
#include "TrackRole.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  TrackDynamicsAdvice — Análisis de dinámica por pista (Sprint 6B)
    //  Compara el crest factor actual contra el target del rol (ExpectedProfile)
    //  y sugiere ajustes de compresión/expansión.
    //  Sin IA — reglas en C++, 0 tokens, instantáneo.
    // ═══════════════════════════════════════════════════════════════════════════
    struct TrackDynamicsAdvice
    {
        int slotIndex = -1;
        juce::String trackName;
        TrackRole role = TrackRole::Unknown;

        // Valores actuales
        float currentCrest = 0.0f;
        float currentPeak  = -100.0f;
        float currentRMS   = -100.0f;

        // Targets del rol
        float crestTarget    = 10.0f;
        float crestTolerance = 6.0f;

        // Desviación (dB, positivo = más dinámico que target)
        float crestDeviation = 0.0f;

        // Acción sugerida
        juce::String suggestedAction;

        enum class Status : uint8_t
        {
            OnTarget,
            NearTarget,
            OffTarget,
            NoSignal,
            UnknownRole
        };
        Status status = Status::UnknownRole;

        enum class SubType : uint8_t
        {
            None,
            Overcompressed,
            TooDynamic
        };
        SubType subType = SubType::None;

        juce::String message;

        [[nodiscard]] bool isActionable() const noexcept
        {
            return status == Status::OffTarget || status == Status::NearTarget;
        }

        [[nodiscard]] bool isOvercompressed() const noexcept { return subType == SubType::Overcompressed; }
        [[nodiscard]] bool isTooDynamic() const noexcept { return subType == SubType::TooDynamic; }
    };

    // ═══ Free functions — Track Dynamics Analysis ═════════════════════════════
    [[nodiscard]] TrackDynamicsAdvice analyzeTrackDynamics(
        int slotIndex,
        SharedData& sharedData,
        const std::array<TrackRole, SlotRegistry::kMaxSlots>& trackRoles,
        const juce::String& setupGenre,
        bool soloActive);

    [[nodiscard]] std::vector<TrackDynamicsAdvice> analyzeAllTracksDynamics(
        SharedData& sharedData,
        const std::array<TrackRole, SlotRegistry::kMaxSlots>& trackRoles,
        const juce::String& setupGenre,
        bool soloActive);

} // namespace mixcoach
