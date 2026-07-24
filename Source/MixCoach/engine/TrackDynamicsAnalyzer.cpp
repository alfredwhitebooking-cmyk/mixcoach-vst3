#include "TrackDynamicsAnalyzer.h"
#include "TrackGainAnalyzer.h" // for getLatestTelemetry
#include "../../Common/types/LogHelper.h"
#include <algorithm>
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  analyzeTrackDynamics — Analiza dinámica de una pista vs su rol
    //  Compara crestFactor contra ExpectedProfile.crestTargetDb ± crestTolerance.
    //  Sin IA — reglas C++, 0 tokens.
    // ═══════════════════════════════════════════════════════════════════════════
    TrackDynamicsAdvice analyzeTrackDynamics(int slotIndex,
                                              SharedData& sharedData,
                                              const std::array<TrackRole, SlotRegistry::kMaxSlots>& trackRoles,
                                              const juce::String& setupGenre,
                                              bool soloActive)
    {
        TrackDynamicsAdvice advice;
        advice.slotIndex = slotIndex;

        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) {
            advice.status = TrackDynamicsAdvice::Status::UnknownRole;
            return advice;
        }

        auto& registry  = sharedData.getSlotRegistry();
        auto info       = registry.getSlotInfo(slotIndex);
        if (info.muted || (soloActive && !info.soloed)) {
            advice.status = TrackDynamicsAdvice::Status::NoSignal;
            return advice;
        }

        advice.trackName = juce::String(info.trackName).trim();
        if (advice.trackName.isEmpty()) advice.trackName = "Pista " + juce::String(slotIndex + 1);

        // Get role
        advice.role = trackRoles[static_cast<size_t>(slotIndex)];
        if (advice.role == TrackRole::Unknown || advice.role == TrackRole::Master) {
            advice.status = TrackDynamicsAdvice::Status::UnknownRole;
            return advice;
        }

        // Get telemetry
        auto telem = getLatestTelemetry(sharedData, slotIndex);
        if (telem.timestamp == 0 || telem.rmsLeft < -50.0f) {
            advice.status = TrackDynamicsAdvice::Status::NoSignal;
            return advice;
        }

        // Current values
        advice.currentCrest = telem.crestFactor;
        advice.currentPeak  = juce::jmax(telem.peakLeft, telem.peakRight);
        advice.currentRMS   = (telem.rmsLeft + telem.rmsRight) * 0.5f;

        if (advice.currentCrest <= 0.0f) {
            advice.status = TrackDynamicsAdvice::Status::NoSignal;
            return advice;
        }

        // Target from role (genre-aware)
        auto profile          = getExpectedProfile(advice.role, setupGenre);
        advice.crestTarget    = profile.crestTargetDb;
        advice.crestTolerance = profile.crestTolerance;

        // Deviation: positivo = mas dinamico que el target
        advice.crestDeviation = advice.currentCrest - advice.crestTarget;

        // Status
        float tol = advice.crestTolerance;
        if (std::abs(advice.crestDeviation) <= tol) {
            advice.status  = TrackDynamicsAdvice::Status::OnTarget;
            advice.subType = TrackDynamicsAdvice::SubType::None;
        }
        else if (std::abs(advice.crestDeviation) <= tol * 2.0f) {
            advice.status  = TrackDynamicsAdvice::Status::NearTarget;
            advice.subType = (advice.crestDeviation < 0) ? TrackDynamicsAdvice::SubType::Overcompressed
                                                         : TrackDynamicsAdvice::SubType::TooDynamic;
        }
        else {
            advice.status  = TrackDynamicsAdvice::Status::OffTarget;
            advice.subType = (advice.crestDeviation < 0) ? TrackDynamicsAdvice::SubType::Overcompressed
                                                         : TrackDynamicsAdvice::SubType::TooDynamic;
        }

        // Suggested action and message
        switch (advice.subType) {
            case TrackDynamicsAdvice::SubType::Overcompressed: {
                float deficit = advice.crestTarget - advice.currentCrest;
                advice.suggestedAction =
                    "Baja el ratio del compresor o sube el threshold " + juce::String(deficit * 0.5f, 1) + " dB.";
                advice.message = "\xF0\x9F\x94\xB4 " + advice.trackName + " \u2014 sobre-comprimido (crest "
                                 + juce::String(advice.currentCrest, 1) + " dB, target "
                                 + juce::String(advice.crestTarget, 1) + " dB). " + advice.suggestedAction;
                break;
            }
            case TrackDynamicsAdvice::SubType::TooDynamic: {
                advice.suggestedAction = "Prueba un compresor con ratio 4:1 y attack r\xC3\xA1pido (~10ms).";
                if (advice.currentCrest > advice.crestTarget + tol * 3.0f)
                    advice.suggestedAction = "Aplica compresi\xC3\xB3n suave 2:1 con threshold en -20 dB.";
                advice.message = "\xF0\x9F\x94\xB4 " + advice.trackName + " \u2014 demasiado din\xC3\xA1mico (crest "
                                 + juce::String(advice.currentCrest, 1) + " dB, target "
                                 + juce::String(advice.crestTarget, 1) + " dB). " + advice.suggestedAction;
                break;
            }
            case TrackDynamicsAdvice::SubType::None: {
                if (advice.status == TrackDynamicsAdvice::Status::NearTarget) {
                    advice.message = "\xF0\x9F\x9F\xA1 " + advice.trackName + " \u2014 crest "
                                     + juce::String(advice.currentCrest, 1) + " dB (target "
                                     + juce::String(advice.crestTarget, 1) + " dB). Cerca del l\xC3\xADmite.";
                    advice.suggestedAction = "Monitorea, no requiere acci\xC3\xB3n inmediata.";
                }
                else {
                    advice.message = "\u2705 " + advice.trackName + " \u2014 crest "
                                     + juce::String(advice.currentCrest, 1) + " dB en rango (target "
                                     + juce::String(advice.crestTarget, 1) + " dB).";
                }
                break;
            }
            default:
                break;
        }

        return advice;
    }

    std::vector<TrackDynamicsAdvice> analyzeAllTracksDynamics(
        SharedData& sharedData,
        const std::array<TrackRole, SlotRegistry::kMaxSlots>& trackRoles,
        const juce::String& setupGenre,
        bool soloActive)
    {
        std::vector<TrackDynamicsAdvice> results;
        auto& registry = sharedData.getSlotRegistry();

        registry.forEachActive([&](const SlotInfo& info) {
            if (info.muted || (soloActive && !info.soloed)) return;
            auto advice = analyzeTrackDynamics(info.slotIndex, sharedData, trackRoles, setupGenre, soloActive);
            if (advice.isActionable()) results.push_back(advice);
        });

        std::sort(results.begin(), results.end(), [](const TrackDynamicsAdvice& a, const TrackDynamicsAdvice& b) {
            auto sev = [](const TrackDynamicsAdvice& adv) -> int {
                if (adv.status == TrackDynamicsAdvice::Status::OffTarget) return adv.isOvercompressed() ? 0 : 1;
                if (adv.status == TrackDynamicsAdvice::Status::NearTarget) return 2;
                return 3;
            };
            return sev(a) < sev(b);
        });

        return results;
    }

} // namespace mixcoach
