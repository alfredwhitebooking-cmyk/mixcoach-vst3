#include "TrackPhaseAnalyzer.h"
#include "TrackGainAnalyzer.h" // for getLatestTelemetry
#include <algorithm>
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  analyzeTrackPhase — Analiza fase estéreo de una pista
    //  Compara correlación contra umbral: <0 = OffTarget, <0.3 = NearTarget
    // ═══════════════════════════════════════════════════════════════════════════
    TrackPhaseAdvice analyzeTrackPhase(int slotIndex,
                                        SharedData& sharedData,
                                        const std::array<TrackRole, SlotRegistry::kMaxSlots>& trackRoles,
                                        bool soloActive)
    {
        TrackPhaseAdvice advice;
        advice.slotIndex = slotIndex;
        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) return advice;

        auto& registry = sharedData.getSlotRegistry();
        auto info      = registry.getSlotInfo(slotIndex);
        if (info.muted || (soloActive && !info.soloed)) {
            advice.status = TrackPhaseAdvice::Status::NoSignal;
            return advice;
        }

        advice.trackName = juce::String(info.trackName).trim();
        if (advice.trackName.isEmpty()) advice.trackName = "Track " + juce::String(slotIndex + 1);
        advice.role = trackRoles[static_cast<size_t>(slotIndex)];

        auto telem = getLatestTelemetry(sharedData, slotIndex);
        if (telem.timestamp == 0) {
            advice.status = TrackPhaseAdvice::Status::NoSignal;
            return advice;
        }

        float corr                = telem.correlation;
        float peak                = juce::jmax(telem.peakLeft, telem.peakRight);
        advice.currentCorrelation = corr;
        advice.currentPeak        = peak;

        if (peak < -40.0f) {
            advice.status = TrackPhaseAdvice::Status::NoSignal;
            return advice;
        }

        if (corr < 0.0f) {
            advice.status               = TrackPhaseAdvice::Status::OffTarget;
            advice.correlationDeviation = corr - 0.0f;
            advice.message = "\xF0\x9F\x94\xAE " + advice.trackName + " correlacion negativa (" + juce::String(corr, 2) + ")";
        }
        else if (corr < 0.3f) {
            advice.status               = TrackPhaseAdvice::Status::NearTarget;
            advice.correlationDeviation = corr - 0.3f;
            advice.message = "\xF0\x9F\x94\xAE " + advice.trackName + " correlacion baja (" + juce::String(corr, 2) + ")";
        }
        else
            advice.status = TrackPhaseAdvice::Status::OnTarget;

        return advice;
    }

    std::vector<TrackPhaseAdvice> analyzeAllTracksPhase(
        SharedData& sharedData,
        const std::array<TrackRole, SlotRegistry::kMaxSlots>& trackRoles,
        bool soloActive)
    {
        std::vector<TrackPhaseAdvice> results;
        auto& registry = sharedData.getSlotRegistry();

        registry.forEachActive([&](const SlotInfo& info) {
            if (info.muted || (soloActive && !info.soloed)) return;
            int idx = info.slotIndex;
            if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;

            auto telem = getLatestTelemetry(sharedData, idx);
            if (telem.timestamp == 0) return;
            float peak = juce::jmax(telem.peakLeft, telem.peakRight);
            if (peak < -40.0f) return;

            auto phase = analyzeTrackPhase(idx, sharedData, trackRoles, soloActive);
            if (phase.isActionable()) results.push_back(phase);
        });

        std::sort(results.begin(), results.end(), [](const TrackPhaseAdvice& a, const TrackPhaseAdvice& b) {
            if (a.status != b.status) return a.status == TrackPhaseAdvice::Status::OffTarget;
            return std::abs(a.correlationDeviation) > std::abs(b.correlationDeviation);
        });

        return results;
    }

} // namespace mixcoach
