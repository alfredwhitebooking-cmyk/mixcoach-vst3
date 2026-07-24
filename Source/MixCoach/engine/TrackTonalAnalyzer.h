#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include "../../Common/memory/SharedData.h"
#include "../../Common/memory/SlotRegistry.h"
#include "../../Common/types/Types.h"
#include "TrackRole.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  TrackTonalAdvice — Análisis de balance espectral por pista (Sprint 6C)
    //  Compara la energía en 6 regiones (Sub, Bass, LoMid, HiMid, Pres, Air)
    //  contra el spectralOffset esperado del rol (ExpectedProfile).
    //  Sin IA — reglas en C++, 0 tokens, instantáneo.
    // ═══════════════════════════════════════════════════════════════════════════
    struct TrackTonalAdvice
    {
        int slotIndex = -1;
        juce::String trackName;
        TrackRole role = TrackRole::Unknown;

        // Energía actual por región (6 regiones, dB)
        float regionEnergy[6] = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};
        // Energía esperada por región (desde peakDb + spectralOffset, dB)
        float regionExpected[6] = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};
        // Desviación por región (positivo = más energía de la esperada, dB)
        float regionDeviation[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

        float currentPeak = -100.0f;

        // Región con mayor desviación
        int worstRegion      = -1;
        float worstDeviation = 0.0f;
        bool isExcess        = false;

        enum class Status : uint8_t
        {
            OnTarget,
            NearTarget,
            OffTarget,
            NoSignal,
            UnknownRole
        };
        Status status = Status::UnknownRole;

        static constexpr float kToleranceDb     = 6.0f;
        static constexpr float kNearToleranceDb = 12.0f;

        static constexpr int kRegionSub   = 0;
        static constexpr int kRegionBass  = 1;
        static constexpr int kRegionLoMid = 2;
        static constexpr int kRegionHiMid = 3;
        static constexpr int kRegionPres  = 4;
        static constexpr int kRegionAir   = 5;

        static constexpr const char* kRegionName(int r) noexcept
        {
            switch (r) {
                case 0: return "Sub";
                case 1: return "Bass";
                case 2: return "LoMid";
                case 3: return "HiMid";
                case 4: return "Pres";
                case 5: return "Air";
                default: return "?";
            }
        }

        static constexpr const char* kRegionFreq(int r) noexcept
        {
            switch (r) {
                case 0: return "20-86 Hz";
                case 1: return "86-301 Hz";
                case 2: return "301-1076 Hz";
                case 3: return "1076-3532 Hz";
                case 4: return "3532-8355 Hz";
                case 5: return "8355-16458 Hz";
                default: return "";
            }
        }

        static constexpr const char* kExcessSuggestion(int r) noexcept
        {
            switch (r) {
                case 0: return "reduce 50-100 Hz";
                case 1: return "reduce 100-300 Hz";
                case 2: return "reduce 300-1000 Hz";
                case 3: return "reduce 1-3 kHz";
                case 4: return "reduce 3-8 kHz";
                case 5: return "reduce 8-16 kHz";
                default: return "";
            }
        }

        static constexpr const char* kDeficitSuggestion(int r) noexcept
        {
            switch (r) {
                case 0: return "refuerza 50-100 Hz";
                case 1: return "refuerza 100-300 Hz";
                case 2: return "refuerza 300-1000 Hz";
                case 3: return "refuerza 1-3 kHz";
                case 4: return "refuerza 3-8 kHz";
                case 5: return "refuerza 8-16 kHz";
                default: return "";
            }
        }

        juce::String message;

        [[nodiscard]] bool isActionable() const noexcept
        {
            return status == Status::OffTarget || status == Status::NearTarget;
        }

        [[nodiscard]] bool hasExcess() const noexcept { return isExcess && worstRegion >= 0; }
        [[nodiscard]] bool hasDeficit() const noexcept { return worstRegion >= 0 && !isExcess; }
    };

    // ═══ Free functions — Track Tonal Analysis ════════════════════════════════
    [[nodiscard]] TrackTonalAdvice analyzeTrackTonal(
        int slotIndex,
        SharedData& sharedData,
        const std::array<TrackRole, SlotRegistry::kMaxSlots>& trackRoles,
        const juce::String& setupGenre,
        bool soloActive);

    [[nodiscard]] std::vector<TrackTonalAdvice> analyzeAllTracksTonal(
        SharedData& sharedData,
        const std::array<TrackRole, SlotRegistry::kMaxSlots>& trackRoles,
        const juce::String& setupGenre,
        bool soloActive);

} // namespace mixcoach
