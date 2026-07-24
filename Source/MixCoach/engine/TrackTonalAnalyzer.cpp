#include "TrackTonalAnalyzer.h"
#include "TrackGainAnalyzer.h" // for getLatestTelemetry
#include "../../Common/types/LogHelper.h"
#include <algorithm>
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  analyzeTrackTonal — Analiza balance espectral de una pista vs su rol
    //  Convierte 30 bandEnergies en 6 regiones espectrales y compara
    //  contra ExpectedProfile.spectralOffset para detectar exceso/déficit.
    //  Sin IA — reglas C++, 0 tokens.
    // ═══════════════════════════════════════════════════════════════════════════
    TrackTonalAdvice analyzeTrackTonal(int slotIndex,
                                        SharedData& sharedData,
                                        const std::array<TrackRole, SlotRegistry::kMaxSlots>& trackRoles,
                                        const juce::String& setupGenre,
                                        bool soloActive)
    {
        TrackTonalAdvice advice;
        advice.slotIndex = slotIndex;

        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) {
            advice.status = TrackTonalAdvice::Status::NoSignal;
            return advice;
        }

        auto& registry = sharedData.getSlotRegistry();
        auto info      = registry.getSlotInfo(slotIndex);
        if (info.muted || (soloActive && !info.soloed)) {
            advice.status = TrackTonalAdvice::Status::NoSignal;
            return advice;
        }

        juce::String trackName = juce::String(info.trackName).trim();
        if (trackName.isEmpty()) trackName = "Pista " + juce::String(slotIndex + 1);
        advice.trackName = trackName;

        // Get role
        TrackRole role = trackRoles[static_cast<size_t>(slotIndex)];
        if (role == TrackRole::Unknown || role == TrackRole::Master) {
            advice.role   = role;
            advice.status = TrackTonalAdvice::Status::UnknownRole;
            return advice;
        }
        advice.role = role;

        // Get telemetry
        auto telem = getLatestTelemetry(sharedData, slotIndex);
        if (telem.timestamp == 0) {
            advice.status = TrackTonalAdvice::Status::NoSignal;
            return advice;
        }

        float peakDb = juce::jmax(telem.peakLeft, telem.peakRight);
        if (peakDb < -60.0f) {
            advice.currentPeak = peakDb;
            advice.status      = TrackTonalAdvice::Status::NoSignal;
            return advice;
        }
        advice.currentPeak = peakDb;

        // Get expected profile (genre-aware)
        auto profile = getExpectedProfile(role, setupGenre);

        // Map 30 bandEnergies to 6 regions
        for (int region = 0; region < 6; ++region) {
            int bandStart = region * 5;
            int bandEnd   = bandStart + 5;

            float sumEnergies = 0.0f;
            int validBands    = 0;
            for (int b = bandStart; b < bandEnd && b < 30; ++b) {
                if (telem.bandEnergies[b] > -80.0f) {
                    sumEnergies += telem.bandEnergies[b];
                    validBands++;
                }
            }

            if (validBands > 0) advice.regionEnergy[region] = sumEnergies / (float)validBands;
            else                    advice.regionEnergy[region] = -100.0f;

            advice.regionExpected[region] = peakDb + profile.spectralOffset[region];

            if (advice.regionEnergy[region] > -80.0f)
                advice.regionDeviation[region] = advice.regionEnergy[region] - advice.regionExpected[region];
            else
                advice.regionDeviation[region] = 0.0f;
        }

        // Find worst region (max absolute deviation)
        float maxAbsDev = 0.0f;
        int worstRegion = -1;
        for (int r = 0; r < 6; ++r) {
            float absDev = std::abs(advice.regionDeviation[r]);
            if (absDev > maxAbsDev) {
                maxAbsDev   = absDev;
                worstRegion = r;
            }
        }
        advice.worstRegion    = worstRegion;
        advice.worstDeviation = (worstRegion >= 0) ? advice.regionDeviation[worstRegion] : 0.0f;

        if (worstRegion < 0) {
            advice.status = TrackTonalAdvice::Status::OnTarget;
            return advice;
        }

        float worstAbs = std::abs(advice.worstDeviation);
        if (worstAbs <= TrackTonalAdvice::kToleranceDb) {
            advice.status  = TrackTonalAdvice::Status::OnTarget;
            advice.message = "\xF0\x9F\x9F\xA2 **" + trackName + "** \xE2\x80\x94 balance espectral en rango ("
                             + juce::String(profile.name) + ").";
        }
        else if (worstAbs <= TrackTonalAdvice::kNearToleranceDb) {
            advice.status          = TrackTonalAdvice::Status::NearTarget;
            advice.isExcess        = (advice.worstDeviation > 0.0f);
            const char* regionName = TrackTonalAdvice::kRegionName(worstRegion);
            float deviation        = advice.worstDeviation;
            juce::String action;
            if (advice.isExcess)
                action = "Exceso en **" + juce::String(regionName) + "** (" + juce::String(deviation, 1)
                         + " dB sobre target). " + juce::String(TrackTonalAdvice::kExcessSuggestion(worstRegion));
            else
                action = "Falta en **" + juce::String(regionName) + "** (" + juce::String(-deviation, 1)
                         + " dB bajo target). " + juce::String(TrackTonalAdvice::kDeficitSuggestion(worstRegion));

            advice.message = "\xF0\x9F\x9F\xA1 **" + trackName + "** \xE2\x80\x94 ligero desbalance espectral " + action;
        }
        else {
            advice.status          = TrackTonalAdvice::Status::OffTarget;
            advice.isExcess        = (advice.worstDeviation > 0.0f);
            const char* regionName = TrackTonalAdvice::kRegionName(worstRegion);
            const char* freqRange  = TrackTonalAdvice::kRegionFreq(worstRegion);

            if (advice.isExcess) {
                advice.message = "\xF0\x9F\x94\xB4 **" + trackName + "** \xE2\x80\x94 exceso de energ\xC3\xAD" "a en **"
                            + juce::String(regionName) + "** (" + juce::String(freqRange) + ", "
                            + juce::String(advice.regionEnergy[worstRegion], 1) + " dBFS, esperado "
                            + juce::String(advice.regionExpected[worstRegion], 1) + " dBFS). "
                            + "Prueba " + juce::String(TrackTonalAdvice::kExcessSuggestion(worstRegion)) + ".";
            }
            else {
                advice.message = "\xF0\x9F\x94\xB4 **" + trackName + "** \xE2\x80\x94 falta de energ\xC3\xAD" "a en **"
                            + juce::String(regionName) + "** (" + juce::String(freqRange) + ", "
                            + juce::String(advice.regionEnergy[worstRegion], 1) + " dBFS, esperado "
                            + juce::String(advice.regionExpected[worstRegion], 1) + " dBFS). "
                            + "Prueba " + juce::String(TrackTonalAdvice::kDeficitSuggestion(worstRegion)) + ".";
            }
        }

        return advice;
    }

    std::vector<TrackTonalAdvice> analyzeAllTracksTonal(
        SharedData& sharedData,
        const std::array<TrackRole, SlotRegistry::kMaxSlots>& trackRoles,
        const juce::String& setupGenre,
        bool soloActive)
    {
        std::vector<TrackTonalAdvice> results;
        auto& registry = sharedData.getSlotRegistry();

        registry.forEachActive([&](const SlotInfo& info) {
            if (info.muted || (soloActive && !info.soloed)) return;
            auto advice = analyzeTrackTonal(info.slotIndex, sharedData, trackRoles, setupGenre, soloActive);
            if (advice.isActionable()) results.push_back(advice);
        });

        std::sort(results.begin(), results.end(), [](const TrackTonalAdvice& a, const TrackTonalAdvice& b) {
            auto sev = [](const TrackTonalAdvice& adv) -> int {
                if (adv.status == TrackTonalAdvice::Status::OffTarget) return adv.isExcess ? 0 : 1;
                if (adv.status == TrackTonalAdvice::Status::NearTarget) return 2;
                return 3;
            };
            return sev(a) < sev(b);
        });

        return results;
    }

} // namespace mixcoach
