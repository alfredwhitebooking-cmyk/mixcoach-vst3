#include "MessengerListComponent.h"
#include "MessengerListAdvice.h"
#include "../engine/CoachEngine.h"
#include "../../Common/types/LogHelper.h"
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  syncTelemetryFromRegistry — Member: reads data from SharedAudioMemory
    //  NOTE: All constants are defined as local constexpr INSIDE the function
    //  body but BEFORE the lambda to avoid MSVC C++ parsing issues with
    //  namespace-scope static constexpr inside complex lambdas.
    // ═══════════════════════════════════════════════════════════════════════════
    void
    MessengerListComponent::syncTelemetryFromRegistry(SlotRegistry& registry, bool& anyDataOut, SharedData& sharedData)
    {
        anyDataOut   = false;
        uint32_t now = juce::Time::getMillisecondCounter();

        registry.forEachActive([&](const SlotInfo& info) {
            int idx = info.slotIndex;
            if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;
            anyDataOut = true;

            auto& entry = messengers_[idx];
            entry.info  = info;

            TrackAudioResult audioResult = sharedData.getTrackAudioResult(idx);

            float rawPeakCombined = -60.0f;
            float rawRmsCombined  = -60.0f;
            float rawPeakL = -60.0f, rawPeakR = -60.0f;
            bool hasSignal = false;

            bool haveCacheData = (audioResult.timestampUs > 0);

            if (haveCacheData) {
                rawPeakCombined = audioResult.getPeakCombined();
                rawRmsCombined  = audioResult.getRmsCombined();
                rawPeakL        = audioResult.peakLeft;
                rawPeakR        = audioResult.peakRight;
                hasSignal       = (rawPeakCombined > -60.0f);
            }
            else {
                // Fallback: SharedAudioMemoryV2 (stereo)
                if (sharedData.getAudioMemoryV2().isInitialized()) {
                    auto& v2                  = sharedData.getAudioMemoryV2();
                    constexpr int kV2ReadSize = 512;
                    float left[kV2ReadSize], right[kV2ReadSize];

                    int nRead = v2.readStereoSamples(idx, left, right, kV2ReadSize);
                    if (nRead > 0) {
                        float peakL = 0.0f, peakR = 0.0f;
                        double sumSqL = 0.0, sumSqR = 0.0;
                        for (int i = 0; i < nRead; ++i) {
                            float al = std::abs(left[i]), ar = std::abs(right[i]);
                            if (al > peakL) peakL = al;
                            if (ar > peakR) peakR = ar;
                            sumSqL += (double)left[i] * left[i];
                            sumSqR += (double)right[i] * right[i];
                        }
                        rawPeakL   = (peakL > 1e-10f) ? juce::Decibels::gainToDecibels(peakL) : -100.0f;
                        rawPeakR   = (peakR > 1e-10f) ? juce::Decibels::gainToDecibels(peakR) : -100.0f;
                        float rmsL = (sumSqL > 0.0) ? juce::Decibels::gainToDecibels((float)std::sqrt(sumSqL / nRead))
                                                    : -100.0f;
                        float rmsR = (sumSqR > 0.0) ? juce::Decibels::gainToDecibels((float)std::sqrt(sumSqR / nRead))
                                                    : -100.0f;
                        rawPeakCombined = juce::jmax(rawPeakL, rawPeakR, -60.0f);
                        rawRmsCombined  = juce::jmax(rmsL, rmsR, -60.0f);
                        hasSignal       = (rawPeakCombined > -60.0f);

                        TrackAudioResult directResult;
                        directResult.timestampUs = juce::Time::getMillisecondCounter() * 1000;
                        directResult.peakLeft    = rawPeakL;
                        directResult.peakRight   = rawPeakR;
                        directResult.rmsLeft     = rmsL;
                        directResult.rmsRight    = rmsR;
                        sharedData.updateTrackAudioResult(idx, directResult);
                    }
                }

                // Fallback: SharedAudioMemory V1 (mono legacy)
                if (!hasSignal && sharedData.getAudioMemory().isInitialized()) {
                    auto& v1                  = sharedData.getAudioMemory();
                    constexpr int kV1ReadSize = 512;
                    float mono[kV1ReadSize];

                    int nRead = v1.readSamples(idx, mono, kV1ReadSize);
                    if (nRead > 0) {
                        float peak   = 0.0f;
                        double sumSq = 0.0;
                        for (int i = 0; i < nRead; ++i) {
                            float a = std::abs(mono[i]);
                            if (a > peak) peak = a;
                            sumSq += (double)mono[i] * mono[i];
                        }
                        rawPeakL  = (peak > 1e-10f) ? juce::Decibels::gainToDecibels(peak) : -100.0f;
                        rawPeakR  = rawPeakL;
                        float rms = (sumSq > 0.0) ? juce::Decibels::gainToDecibels((float)std::sqrt(sumSq / nRead))
                                                  : -100.0f;
                        rawPeakCombined = rawPeakL;
                        rawRmsCombined  = rms;
                        hasSignal       = (rawPeakCombined > -60.0f);

                        TrackAudioResult directResult;
                        directResult.timestampUs = juce::Time::getMillisecondCounter() * 1000;
                        directResult.peakLeft    = rawPeakL;
                        directResult.peakRight   = rawPeakL;
                        directResult.rmsLeft     = rms;
                        directResult.rmsRight    = rms;
                        sharedData.updateTrackAudioResult(idx, directResult);
                    }
                }
            }

            // DIAG log if no signal
            if (!hasSignal && !haveCacheData) {
                uint32_t nowMs = juce::Time::getMillisecondCounter();
                if (nowMs - lastNoSignalLogMs_ > 10000) {
                    lastNoSignalLogMs_ = nowMs;
                    LogHelper::writeToLog("[DIAG] Slot " + juce::String(idx) + " sin senal: V2init="
                                          + juce::String(sharedData.getAudioMemoryV2().isInitialized() ? 1 : 0)
                                          + " V2avail=" + juce::String(sharedData.getAudioMemoryV2().available(idx)));
                }
            }

            // Attack instantáneo + peak hold
            {
                if (rawPeakCombined > entry.barLevel) entry.barLevel = rawPeakCombined;
                if (rawRmsCombined > entry.rmsSmooth) entry.rmsSmooth = rawRmsCombined;
                if (rawPeakCombined > entry.peakHold) {
                    entry.peakHold       = rawPeakCombined;
                    entry.peakHoldTimeMs = now;
                    entry.peakHoldAlpha  = 1.0f;
                }
            }

            entry.peakLeft    = rawPeakL;
            entry.peakRight   = rawPeakR;
            entry.rmsAvg      = rawRmsCombined;
            entry.hasSignal   = hasSignal;
            entry.correlation = audioResult.correlation;

            // ═══ Sprint 3: Per-Track Analysis ───────────────────────────────
            entry.crestDb = juce::jmax(0.0f, rawPeakCombined - rawRmsCombined);

            // Compute 6-region band levels (average 5 bands each from 30-band data)
            for (int r = 0; r < 6; ++r) {
                float sum = 0.0f;
                int count = 0;
                for (int b = r * 5; b < juce::jmin((r + 1) * 5, 30); ++b) {
                    float val = audioResult.bandEnergies[b];
                    if (val > -90.0f) {
                        sum += val;
                        ++count;
                    }
                }
                entry.bandLevelDb[r] = (count > 0) ? (sum / (float)count) : -100.0f;
            }

            // Average stereo width from 6-band data
            {
                float widthSum = 0.0f;
                int widthCount = 0;
                for (int b = 0; b < 6; ++b) {
                    if (audioResult.stereoWidthPerBand[b] >= 0.0f) {
                        widthSum += audioResult.stereoWidthPerBand[b];
                        ++widthCount;
                    }
                }
                entry.avgStereoWidth = (widthCount > 0) ? (widthSum / (float)widthCount) : 0.0f;
            }

            // Update suggestion
            auto suggestion        = analyzeTrackSuggestion(rawPeakCombined, rawRmsCombined, hasSignal, info);
            entry.aiSuggestion     = suggestion.text;
            entry.suggestionStatus = suggestion.status;

            // TrackFeedCore override (health + attention)
            if (coachEngine_ != nullptr) {
                auto& feed           = coachEngine_->getTrackFeedCore();
                TrackState ts        = feed.getTrackState(idx);
                entry.trackHealth    = ts.health;
                entry.attentionScore = ts.attentionScore;

                if (ts.hasSignal() && ts.health != TrackHealth::Unknown && ts.health != TrackHealth::Silent) {
                    juce::String healthMsg;
                    SuggestionStatus healthStatus;
                    bool overrideExisting = false;

                    switch (ts.health) {
                        case TrackHealth::ClippingRisk:
                            healthStatus     = SuggestionStatus::Red;
                            healthMsg        = "\xF0\x9F\x94\xB4 Clipping \xE2\x80\x94 baja ganancia YA";
                            overrideExisting = true;
                            break;
                        case TrackHealth::Overcompressed:
                            healthStatus     = SuggestionStatus::Red;
                            healthMsg        = "\xF0\x9F\x94\xB4 Sobre-comprimido \xE2\x80\x94 reduce compresion";
                            overrideExisting = true;
                            break;
                        case TrackHealth::StereoCollapse:
                            healthStatus     = SuggestionStatus::Red;
                            healthMsg        = "\xF0\x9F\x94\xB4 Colapso stereo \xE2\x80\x94 revisa paneo";
                            overrideExisting = true;
                            break;
                        case TrackHealth::NeedsEQ:
                            healthStatus     = SuggestionStatus::Yellow;
                            healthMsg        = "\xF0\x9F\x9F\xA1 Desbalance espectral \xE2\x80\x94 ajusta EQ";
                            overrideExisting = (suggestion.status <= SuggestionStatus::Green);
                            break;
                        case TrackHealth::NeedsCompression:
                            healthStatus     = SuggestionStatus::Yellow;
                            healthMsg        = "\xF0\x9F\x9F\xA1 Demasiado dinamico \xE2\x80\x94 comprime";
                            overrideExisting = (suggestion.status <= SuggestionStatus::Green);
                            break;
                        case TrackHealth::MaskingIssue:
                            healthStatus     = SuggestionStatus::Yellow;
                            healthMsg        = "\xF0\x9F\x9F\xA1 Posible enmascaramiento";
                            overrideExisting = (suggestion.status <= SuggestionStatus::Green);
                            break;
                        case TrackHealth::PhaseIssue:
                            healthStatus     = SuggestionStatus::Yellow;
                            healthMsg        = "\xF0\x9F\x9F\xA1 Problema de fase";
                            overrideExisting = (suggestion.status <= SuggestionStatus::Green);
                            break;
                        case TrackHealth::LowSignal:
                            healthStatus     = SuggestionStatus::White;
                            healthMsg        = "\xE2\x9A\xAA Senal baja \xE2\x80\x94 sube ganancia";
                            overrideExisting = (suggestion.status <= SuggestionStatus::Green);
                            break;
                        case TrackHealth::Clean:
                            healthStatus     = SuggestionStatus::Green;
                            healthMsg        = "\xF0\x9F\x9F\xA2 Todo en rango";
                            overrideExisting = (suggestion.status == SuggestionStatus::None
                                                || suggestion.status == SuggestionStatus::White);
                            break;
                        default:
                            healthStatus = suggestion.status;
                            healthMsg    = suggestion.text;
                            break;
                    }

                    if (overrideExisting) {
                        entry.aiSuggestion     = healthMsg;
                        entry.suggestionStatus = healthStatus;
                    }
                }
            }

            // Decaimiento constante
            if (!(audioResult.timestampUs > 0 && rawPeakCombined > entry.barLevel)) {
                entry.barLevel -= 0.167f;
                entry.barLevel = juce::jmax(entry.barLevel, -60.0f);
            }

            if (!(audioResult.timestampUs > 0 && rawRmsCombined > entry.rmsSmooth)) {
                entry.rmsSmooth = juce::jmax(-80.0f, entry.rmsSmooth - 0.167f);
            }

            // Peak hold decay
            if ((now - entry.peakHoldTimeMs) > 500) {
                float elapsed       = (now - entry.peakHoldTimeMs - 500) * 0.001f;
                float decay         = 30.0f * elapsed;
                entry.peakHold      = juce::jmax(-60.0f, entry.peakHold - decay, -60.0f);
                entry.peakHoldAlpha = juce::jmax(0.25f, 1.0f - elapsed / 2.0f, 0.25f);
            }

            // Update cache directly without intermediate reference
            telemetryCache_[idx].peakLeft    = rawPeakL;
            telemetryCache_[idx].peakRight   = rawPeakR;
            telemetryCache_[idx].rmsAvg      = rawRmsCombined;
            telemetryCache_[idx].correlation = audioResult.correlation;
            telemetryCache_[idx].hasSignal   = hasSignal;
            telemetryCache_[idx].active      = true;
        });
    }

    // ═══ refreshTelemetryFromRegistry — Convenience: sync + rebuild ═══════════
    void MessengerListComponent::refreshTelemetryFromRegistry(SlotRegistry& registry, SharedData& sharedData)
    {
        bool anyData = false;
        syncTelemetryFromRegistry(registry, anyData, sharedData);
        rebuildBusGroups();
        juce::ignoreUnused(anyData);
    }

    // ═══ rebuildBusGroups — Member: rebuild bus groups from messenger data ═══
    void MessengerListComponent::rebuildBusGroups()
    {
        for (auto& group : busGroups_) group.count = 0;

        for (int idx = 0; idx < SlotRegistry::kMaxSlots; ++idx) {
            auto& entry = messengers_[idx];
            if (!entry.info.active) continue;

            int busIdx = static_cast<int>(entry.info.bus);
            if (busIdx < 0 || busIdx > kNumBuses) busIdx = kNumBuses;

            auto& group = busGroups_[busIdx];
            if (group.count < SlotRegistry::kMaxSlots) group.slotIndices[group.count++] = idx;
        }
    }

    // ═══ smoothMeters — Member: fixed decay per frame ════════════════════════
    void MessengerListComponent::smoothMeters()
    {
        hoverGlow_.advance(120.0);

        for (int idx = 0; idx < SlotRegistry::kMaxSlots; ++idx) {
            if (!telemetryCache_[idx].active) continue;

            // Raw values from cache - direct array access
            float cachePeakL = telemetryCache_[idx].peakLeft;
            float cachePeakR = telemetryCache_[idx].peakRight;
            float cacheRms   = telemetryCache_[idx].rmsAvg;
            float cacheCorr  = telemetryCache_[idx].correlation;
            bool cacheSignal = telemetryCache_[idx].hasSignal;

            auto& entry       = messengers_[idx];
            entry.peakLeft    = cachePeakL;
            entry.peakRight   = cachePeakR;
            entry.rmsAvg      = cacheRms;
            entry.correlation = cacheCorr;
            entry.hasSignal   = cacheSignal;

            // Fixed decay
            entry.barLevel = juce::jmax(entry.barLevel - 0.167f, -60.0f, -60.0f);

            // RMS smooth decay
            if (entry.rmsSmooth > entry.rmsAvg + 0.5f) entry.rmsSmooth += (entry.rmsAvg - entry.rmsSmooth) * 0.06f;
            else if (entry.hasSignal)
                entry.rmsSmooth = entry.rmsAvg;
            else
                entry.rmsSmooth = juce::jmax(-80.0f, entry.rmsSmooth - 0.167f, -80.0f);

            // Peak hold decay
            uint32_t now  = juce::Time::getMillisecondCounter();
            float elapsed = (now - entry.peakHoldTimeMs) * 0.001f;
            if (elapsed > 0.5f) {
                float decay         = 30.0f * (elapsed - 0.5f);
                entry.peakHold      = juce::jmax(entry.barLevel, entry.peakHold - decay, entry.barLevel);
                entry.peakHoldAlpha = juce::jmax(0.25f, 1.0f - (elapsed - 0.5f) / 2.0f, 0.25f);
            }
        }

        // Fade-in animation
        uint32_t now = juce::Time::getMillisecondCounter();
        for (int idx = 0; idx < SlotRegistry::kMaxSlots; ++idx) {
            auto& entry = messengers_[idx];
            if (entry.fadeAlpha >= 1.0f) continue;
            entry.fadeAlpha = juce::jmin(1.0f, (float)(now - entry.fadeStartMs) / 350.0f);
        }
    }

    // ═══ restoreFromPersistent — Member: restore from static data ════════════
    void MessengerListComponent::restoreFromPersistent()
    {
        messengers_           = s_persistentData_;
        busGroups_            = s_persistentGroups_;
        activeMessengerCount_ = s_persistentCount_;

        for (int i = 0; i < SlotRegistry::kMaxSlots; ++i) {
            auto& entry = messengers_[i];

            if (entry.peakLeft > -90.0f || entry.peakRight > -90.0f) {
                entry.barLevel = juce::jmax(entry.peakLeft, entry.peakRight, -60.0f);

                telemetryCache_[i].peakLeft  = entry.peakLeft;
                telemetryCache_[i].peakRight = entry.peakRight;
                telemetryCache_[i].rmsAvg    = entry.rmsAvg;
                telemetryCache_[i].hasSignal = entry.hasSignal;
                telemetryCache_[i].active    = true;

                entry.fadeAlpha = 1.0f;
            }
        }
    }

} // namespace mixcoach
