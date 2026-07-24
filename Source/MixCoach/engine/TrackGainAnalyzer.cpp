#include "TrackGainAnalyzer.h"
#include "../../Common/types/Constants.h"
#include "../../Common/types/LogHelper.h"
#include <algorithm>
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  getLatestTelemetry — Lee telemetría desde SharedData
    // ═══════════════════════════════════════════════════════════════════════════
    TrackTelemetry getLatestTelemetry(SharedData& sharedData, int slotIndex)
    {
        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) return TrackTelemetry{};
        auto result = sharedData.getTrackAudioResult(slotIndex);
        if (result.timestampUs <= 0) return TrackTelemetry{};
        TrackTelemetry telem;
        telem.timestamp     = result.timestampUs;
        telem.peakLeft      = result.peakLeft;
        telem.peakRight     = result.peakRight;
        telem.rmsLeft       = result.rmsLeft;
        telem.rmsRight      = result.rmsRight;
        telem.correlation   = result.correlation;
        telem.crestFactor   = result.crestPerBand[0];
        telem.lufsMomentary = -100.0f;
        telem.lufsShortTerm = -100.0f;
        for (int b = 0; b < 30; ++b) telem.bandEnergies[b] = result.bandEnergies[b];
        return telem;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  computePerTrackLUFS — Aproximación de LUFS desde RMS + high-frequency boost
    // ═══════════════════════════════════════════════════════════════════════════
    float computePerTrackLUFS(const TrackAudioResult& result) noexcept
    {
        float rmsDb = result.getRmsCombined();
        if (rmsDb < -90.0f) return -100.0f;
        float highEnergy = 0.0f;
        int highCount    = 0;
        for (int b = 20; b < 30 && b < kNumSpectralBands; ++b) {
            if (result.bandEnergies[b] > -80.0f) {
                highEnergy += result.bandEnergies[b];
                highCount++;
            }
        }
        float highBoost = (highCount > 0) ? (highEnergy / highCount) : 0.0f;
        return rmsDb + juce::jmin(highBoost * 0.1f, 2.5f);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  analyzeTrackGain — Analiza ganancia de una pista vs su rol
    //  Usa ExpectedProfile de TrackRole.h. Sin IA — reglas C++, 0 tokens.
    // ═══════════════════════════════════════════════════════════════════════════
    TrackGainAdvice analyzeTrackGain(int slotIndex,
                                     SharedData& sharedData,
                                     const std::array<TrackRole, SlotRegistry::kMaxSlots>& trackRoles,
                                     const juce::String& setupGenre,
                                     bool soloActive)
    {
        TrackGainAdvice advice;
        advice.slotIndex = slotIndex;

        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) {
            advice.status = TrackGainAdvice::Status::UnknownRole;
            return advice;
        }

        auto& registry  = sharedData.getSlotRegistry();
        auto info       = registry.getSlotInfo(slotIndex);
        if (info.muted || (soloActive && !info.soloed)) {
            advice.status = TrackGainAdvice::Status::NoSignal;
            return advice;
        }

        advice.trackName = juce::String(info.trackName).trim();
        if (advice.trackName.isEmpty()) advice.trackName = "Pista " + juce::String(slotIndex + 1);

        // Get role
        advice.role = trackRoles[static_cast<size_t>(slotIndex)];
        if (advice.role == TrackRole::Unknown || advice.role == TrackRole::Master) {
            advice.status = TrackGainAdvice::Status::UnknownRole;
            return advice;
        }

        // Get telemetry
        auto telem = getLatestTelemetry(sharedData, slotIndex);
        if (telem.timestamp == 0) {
            advice.status = TrackGainAdvice::Status::NoSignal;
            return advice;
        }

        // Current values
        advice.currentPeak  = juce::jmax(telem.peakLeft, telem.peakRight);
        advice.currentRMS   = (telem.rmsLeft + telem.rmsRight) * 0.5f;
        advice.currentCrest = telem.crestFactor;
        advice.currentLUFS  = computePerTrackLUFS(sharedData.getTrackAudioResult(slotIndex));

        // Target from role (genre-aware)
        auto profile         = getExpectedProfile(advice.role, setupGenre);
        advice.peakTarget    = profile.peakTargetDb;
        advice.crestTarget   = profile.crestTargetDb;
        advice.peakTolerance = profile.peakTolerance;

        // No signal check
        if (advice.currentPeak < -60.0f) {
            advice.status = TrackGainAdvice::Status::NoSignal;
            return advice;
        }

        // Deviation: positivo = track suena mas fuerte que el target
        advice.peakDeviation = advice.currentPeak - advice.peakTarget;

        // Status
        float tol = advice.peakTolerance;
        if (std::abs(advice.peakDeviation) <= tol) advice.status = TrackGainAdvice::Status::OnTarget;
        else if (std::abs(advice.peakDeviation) <= tol * 2.0f)
            advice.status = TrackGainAdvice::Status::NearTarget;
        else
            advice.status = TrackGainAdvice::Status::OffTarget;

        // Suggested delta: queremos acercarnos al target
        advice.suggestedDeltaDb = juce::jlimit(-12.0f, 12.0f, -advice.peakDeviation);

        // Generate human-readable message
        switch (advice.status) {
            case TrackGainAdvice::Status::OnTarget:
                advice.message = "\u2705 " + advice.trackName + " \u2014 nivel en rango ("
                                 + juce::String(advice.currentPeak, 1) + " dBFS, target "
                                 + juce::String(advice.peakTarget, 1) + " dBFS)";
                break;
            case TrackGainAdvice::Status::NearTarget:
                if (advice.peakDeviation > 0.0f)
                    advice.message = "\xF0\x9F\x9F\xA1 " + advice.trackName + " \u2014 ligeramente alto ("
                                     + juce::String(advice.currentPeak, 1) + " dBFS). Baja ~"
                                     + juce::String(std::abs(advice.suggestedDeltaDb), 1) + " dB.";
                else
                    advice.message = "\xF0\x9F\x9F\xA1 " + advice.trackName + " \u2014 ligeramente bajo ("
                                     + juce::String(advice.currentPeak, 1) + " dBFS). Sube ~"
                                     + juce::String(std::abs(advice.suggestedDeltaDb), 1) + " dB.";
                break;
            case TrackGainAdvice::Status::OffTarget:
                if (advice.peakDeviation > 0.0f)
                    advice.message = "\xF0\x9F\x94\xB4 " + advice.trackName + " \u2014 demasiado alto ("
                                     + juce::String(advice.currentPeak, 1) + " dBFS, target "
                                     + juce::String(advice.peakTarget, 1) + " dBFS). Baja ~"
                                     + juce::String(std::abs(advice.suggestedDeltaDb), 1) + " dB.";
                else
                    advice.message = "\xF0\x9F\x94\xB4 " + advice.trackName + " \u2014 demasiado bajo ("
                                     + juce::String(advice.currentPeak, 1) + " dBFS, target "
                                     + juce::String(advice.peakTarget, 1) + " dBFS). Sube ~"
                                     + juce::String(std::abs(advice.suggestedDeltaDb), 1) + " dB.";
                break;
            case TrackGainAdvice::Status::NoSignal:
                advice.message = "\xF0\x9F\x94\x87 " + advice.trackName + " \u2014 sin se\xC3\xB1" "al detectable.";
                break;
            case TrackGainAdvice::Status::UnknownRole:
                advice.message = "[QUESTION] " + advice.trackName + " \u2014 rol no especificado.";
                break;
        }

        return advice;
    }

    std::vector<TrackGainAdvice> analyzeAllTracksGain(SharedData& sharedData,
                                                       const std::array<TrackRole, SlotRegistry::kMaxSlots>& trackRoles,
                                                       const juce::String& setupGenre,
                                                       bool soloActive)
    {
        std::vector<TrackGainAdvice> results;
        auto& registry = sharedData.getSlotRegistry();

        registry.forEachActive([&](const SlotInfo& info) {
            if (info.muted || (soloActive && !info.soloed)) return;
            auto advice = analyzeTrackGain(info.slotIndex, sharedData, trackRoles, setupGenre, soloActive);
            if (advice.isActionable()) results.push_back(advice);
        });

        // Sort by severity: OffTarget first, then NearTarget
        std::sort(results.begin(), results.end(), [](const TrackGainAdvice& a, const TrackGainAdvice& b) {
            auto sev = [](TrackGainAdvice::Status s) -> int {
                return (s == TrackGainAdvice::Status::OffTarget) ? 0 : 1;
            };
            return sev(a.status) < sev(b.status);
        });

        return results;
    }

} // namespace mixcoach
