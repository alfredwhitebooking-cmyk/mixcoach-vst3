#include "CoachEngine.h"
#include "../../Common/types/Constants.h"
#include "../../Common/types/LogHelper.h"
#include <algorithm>
#include <cmath>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  COLLECT ALL ISSUES — Recolecta issues de todas las pistas
// ═══════════════════════════════════════════════════════════════════════════

std::vector<CoachEngine::TrackIssue> CoachEngine::collectAllIssues()
{
    std::vector<TrackIssue> issues;

    auto& registry = sharedData_.getSlotRegistry();
    if (registry.activeCount() == 0)
        return issues;

    registry.forEachActive([&](const SlotInfo& info) {
        auto telem = getLatestTelemetry(info.slotIndex);
        if (telem.timestamp == 0) return;

        TrackIssue issue;
        issue.slotIndex = info.slotIndex;
        issue.trackName = juce::String(info.trackName).trim();
        if (issue.trackName.isEmpty())
            issue.trackName = "Pista " + juce::String(info.slotIndex + 1);

        float peak = juce::jmax(telem.peakLeft, telem.peakRight);

        // === Role-aware ExpectedProfile ===
        TrackRole role = trackRoles_[info.slotIndex];
        ExpectedProfile expProfile = getExpectedProfile(role);
        bool hasRole = (role != TrackRole::Unknown);

        // Clipping detection
        if (peak > -0.5f) {
            issue.domain = "gain";
            issue.issueType = "CLIPPING";
            issue.severity = 1.0f;
            issue.isCritical = true;
            issue.currentValue = peak;
            issue.targetValue = -6.0f;
            issue.description = issue.trackName + " esta recortando a " + juce::String(peak, 1) + " dB";
            issue.actionVerb = "reducir";
            issue.suggestedDelta = peak + 6.0f;
            generateOptionsForIssue(issue, setupGenre_);
            issues.push_back(issue);
        }

        // Near-clipping detection (role-aware)
        float nearClipThreshold = hasRole ? (expProfile.peakTargetDb + expProfile.peakTolerance) : -3.0f;
        if (peak <= -0.5f && peak > nearClipThreshold) {
            issue.domain = "gain";
            issue.issueType = "NEAR_CLIPPING";
            issue.severity = 0.6f;
            issue.isCritical = false;
            issue.currentValue = peak;
            issue.targetValue = expProfile.peakTargetDb;
            juce::String roleHint = hasRole ? (" para " + juce::String(expProfile.name)) : "";
            issue.description = issue.trackName + " esta cerca del clipping (" + juce::String(peak, 1) + " dB" + roleHint + ")";
            issue.actionVerb = "reducir";
            issue.suggestedDelta = peak + 6.0f;
            generateOptionsForIssue(issue, setupGenre_);
            issues.push_back(issue);
        }

        // Low signal detection (role-aware)
        float lowSignalThreshold = hasRole ? (expProfile.peakTargetDb - expProfile.peakTolerance - 6.0f) : -30.0f;
        if (peak < lowSignalThreshold && registry.activeCount() > 1) {
            issue.domain = "gain";
            issue.issueType = "SENIAL_BAJA";
            issue.severity = 0.3f;
            issue.isCritical = false;
            issue.currentValue = peak;
            issue.targetValue = hasRole ? expProfile.peakTargetDb : -18.0f;
            juce::String roleHint = hasRole ? (" para " + juce::String(expProfile.name) + " (target: " + juce::String(expProfile.peakTargetDb, 1) + " dB)") : "";
            issue.description = issue.trackName + " tiene senial muy baja (" + juce::String(peak, 1) + " dB" + roleHint + ")";
            issue.actionVerb = "subir";
            issue.suggestedDelta = hasRole ? (expProfile.peakTargetDb - peak) : (-peak - 18.0f);
            generateOptionsForIssue(issue, setupGenre_);
            issues.push_back(issue);
        }

        // L/R imbalance detection
        float lrDiff = std::fabs(telem.peakLeft - telem.peakRight);
        if (lrDiff > 6.0f && telem.rmsLeft > -40.0f) {
            issue.domain = "spatial";
            issue.issueType = "LR_IMBALANCE";
            issue.severity = 0.5f;
            issue.isCritical = false;
            issue.currentValue = lrDiff;
            issue.targetValue = 3.0f;
            issue.description = issue.trackName + " tiene desbalance L/R de " + juce::String(lrDiff, 1) + " dB";
            issue.actionVerb = "ajustar balance";
            issue.suggestedDelta = lrDiff / 2.0f;
            generateOptionsForIssue(issue, setupGenre_);
            issues.push_back(issue);
        }

        // Crest/dynamics check (role-aware)
        if (telem.crestFactor > 0.0f && telem.rmsLeft > -40.0f) {
            float crestTarget = hasRole ? expProfile.crestTargetDb : 12.0f;
            float crestTol = hasRole ? expProfile.crestTolerance : 6.0f;
            if (telem.crestFactor < (crestTarget - crestTol)) {
                TrackIssue crestIssue;
                crestIssue.slotIndex = info.slotIndex;
                crestIssue.trackName = issue.trackName;
                crestIssue.domain = "dynamics";
                crestIssue.issueType = "SOBRECOMPRIMIDO";
                crestIssue.severity = 0.7f;
                crestIssue.isCritical = false;
                crestIssue.currentValue = telem.crestFactor;
                crestIssue.targetValue = crestTarget;
                juce::String roleHint = hasRole ? (" para " + juce::String(expProfile.name) + " (target: " + juce::String(crestTarget, 1) + " dB)") : "";
                crestIssue.description = issue.trackName + " tiene poca dinamica (crest: " + juce::String(telem.crestFactor, 1) + " dB" + roleHint + ")";
                crestIssue.actionVerb = "comprimir menos";
                crestIssue.suggestedDelta = crestTarget - telem.crestFactor;
                generateOptionsForIssue(crestIssue, setupGenre_);
                issues.push_back(crestIssue);
            }
        }

        // Phase check
        if (telem.correlation < 0.3f && telem.rmsLeft > -30.0f) {
            TrackIssue phaseIssue;
            phaseIssue.slotIndex = info.slotIndex;
            phaseIssue.trackName = issue.trackName;
            phaseIssue.domain = "spatial";
            if (telem.correlation < 0.0f) {
                phaseIssue.issueType = "FASE_INVERTIDA";
                phaseIssue.severity = 0.8f;
                phaseIssue.isCritical = true;
            } else {
                phaseIssue.issueType = "BAJA_CORRELACION";
                phaseIssue.severity = 0.5f;
                phaseIssue.isCritical = false;
            }
            phaseIssue.currentValue = telem.correlation;
            phaseIssue.targetValue = 0.8f;
            phaseIssue.description = issue.trackName + " tiene correlacion " + juce::String(telem.correlation, 2);
            phaseIssue.actionVerb = "ajustar fase";
            generateOptionsForIssue(phaseIssue, setupGenre_);
            issues.push_back(phaseIssue);
        }

        // ─── Spectral detectors (band energies) ────────────────────────
        {
            // Compute region averages from bandEnergies[30]
            // Region mapping: Sub(0-1), Bass(2-4), LoMid(5-9), HiMid(10-17),
            //                 Pres(18-24), Air(25-29)
            float subAvg = -100.0f, bassAvg = -100.0f, loMidAvg = -100.0f;
            float hiMidAvg = -100.0f, presAvg = -100.0f, airAvg = -100.0f;
            int subN = 0, bassN = 0, loMidN = 0, hiMidN = 0, presN = 0, airN = 0;

            for (int b = 0; b < 30; ++b) {
                float e = telem.bandEnergies[b];
                if (e < -90.0f) continue; // skip uninitialized bands
                if (b < 2)  { subAvg   = (subAvg   * subN   + e) / (subN   + 1); ++subN;   }
                else if (b < 5)  { bassAvg  = (bassAvg  * bassN  + e) / (bassN  + 1); ++bassN;  }
                else if (b < 10) { loMidAvg = (loMidAvg * loMidN + e) / (loMidN + 1); ++loMidN; }
                else if (b < 18) { hiMidAvg = (hiMidAvg * hiMidN + e) / (hiMidN + 1); ++hiMidN; }
                else if (b < 25) { presAvg  = (presAvg  * presN  + e) / (presN  + 1); ++presN;  }
                else             { airAvg   = (airAvg   * airN   + e) / (airN   + 1); ++airN;    }
            }

            bool hasSpectralData = (subN + bassN + loMidN + hiMidN + presN + airN) >= 6;

            if (hasSpectralData)
            {
                // Combined region averages
                float subBassAvg = -100.0f;
                if (subN + bassN > 0) {
                    float sumSubBass = subAvg * subN + bassAvg * bassN;
                    subBassAvg = sumSubBass / (subN + bassN);
                }

                float presEnergy = presAvg; // region 4 = Presence band

                float bodyAvg = -100.0f;
                if (bassN + loMidN + hiMidN + presN > 0) {
                    float sumBody = bassAvg * bassN + loMidAvg * loMidN
                                  + hiMidAvg * hiMidN + presAvg * presN;
                    bodyAvg = sumBody / (bassN + loMidN + hiMidN + presN);
                }

                float presAirAvg = -100.0f;
                if (presN + airN > 0) {
                    presAirAvg = (presAvg * presN + airAvg * airN) / (presN + airN);
                }

                // EXCESO_GRABS: Sub+Bass dominates over body
                if (subBassAvg > -80.0f && bodyAvg > -80.0f
                    && subBassAvg > bodyAvg + 6.0f)
                {
                    TrackIssue gravesIssue;
                    gravesIssue.slotIndex = info.slotIndex;
                    gravesIssue.trackName = issue.trackName;
                    gravesIssue.domain = "tonal";
                    gravesIssue.issueType = "EXCESO_GRABS";
                    gravesIssue.severity = 0.5f;
                    gravesIssue.isCritical = false;
                    gravesIssue.currentValue = subBassAvg - bodyAvg;
                    gravesIssue.targetValue = 4.0f;
                    gravesIssue.description = issue.trackName
                        + " tiene exceso de graves (sub-bass " + juce::String(subBassAvg, 1)
                        + " dB vs body " + juce::String(bodyAvg, 1) + " dB)";
                    gravesIssue.actionVerb = "reducir";
                    gravesIssue.suggestedDelta = 2.0f;
                    generateOptionsForIssue(gravesIssue, setupGenre_);
                    issues.push_back(gravesIssue);
                }

                // EXCESO_PRESENCIA: Presence band dominates over body
                if (presEnergy > -80.0f && bodyAvg > -80.0f
                    && presEnergy > bodyAvg + 6.0f)
                {
                    TrackIssue presIssue;
                    presIssue.slotIndex = info.slotIndex;
                    presIssue.trackName = issue.trackName;
                    presIssue.domain = "tonal";
                    presIssue.issueType = "EXCESO_PRESENCIA";
                    presIssue.severity = 0.4f;
                    presIssue.isCritical = false;
                    presIssue.currentValue = presEnergy - bodyAvg;
                    presIssue.targetValue = 4.0f;
                    presIssue.description = issue.trackName
                        + " tiene exceso de presencia (" + juce::String(presEnergy, 1)
                        + " dB vs body " + juce::String(bodyAvg, 1) + " dB)";
                    presIssue.actionVerb = "reducir";
                    presIssue.suggestedDelta = 2.0f;
                    generateOptionsForIssue(presIssue, setupGenre_);
                    issues.push_back(presIssue);
                }

                // FALTA_PRESENCIA: Presence+Air is too quiet vs body
                if (presAirAvg > -80.0f && bodyAvg > -80.0f
                    && presAirAvg < bodyAvg - 6.0f)
                {
                    TrackIssue faltaIssue;
                    faltaIssue.slotIndex = info.slotIndex;
                    faltaIssue.trackName = issue.trackName;
                    faltaIssue.domain = "tonal";
                    faltaIssue.issueType = "FALTA_PRESENCIA";
                    faltaIssue.severity = 0.4f;
                    faltaIssue.isCritical = false;
                    faltaIssue.currentValue = bodyAvg - presAirAvg;
                    faltaIssue.targetValue = 4.0f;
                    faltaIssue.description = issue.trackName
                        + " falta de presencia/aire (body " + juce::String(bodyAvg, 1)
                        + " dB vs " + juce::String(presAirAvg, 1) + " dB)";
                    faltaIssue.actionVerb = "subir";
                    faltaIssue.suggestedDelta = 2.0f;
                    generateOptionsForIssue(faltaIssue, setupGenre_);
                    issues.push_back(faltaIssue);
                }
            }
        }

        // Healthy track detection (role-aware)
        {
            float lowSignalThreshold = hasRole ? (expProfile.peakTargetDb - expProfile.peakTolerance - 6.0f) : -30.0f;
            bool isClipping = (peak > -0.5f);
            float healthyNearClipThreshold = hasRole ? (expProfile.peakTargetDb + expProfile.peakTolerance) : -3.0f;
            bool isNearClip = (peak <= -0.5f && peak > healthyNearClipThreshold);
            bool isLowSignal = (peak < lowSignalThreshold && registry.activeCount() > 1);
            bool hasLRImbalance = (std::fabs(telem.peakLeft - telem.peakRight) > 6.0f
                                   && telem.rmsLeft > -40.0f);
            float crestTarget = hasRole ? expProfile.crestTargetDb : 12.0f;
            float crestTol = hasRole ? expProfile.crestTolerance : 6.0f;
            bool isOvercompressed = (telem.crestFactor > 0.0f && telem.crestFactor < (crestTarget - crestTol)
                                     && telem.rmsLeft > -40.0f);
            bool hasPhaseIssue = (telem.correlation < 0.3f && telem.rmsLeft > -30.0f);
            if (!isClipping && !isNearClip && !isLowSignal && !hasLRImbalance
                && !isOvercompressed && !hasPhaseIssue) {
                bool goodLevel = hasRole ? expProfile.isPeakInRange(peak)
                                         : (peak > -24.0f && peak < -6.0f);
                bool goodCrest = hasRole ? expProfile.isCrestInRange(telem.crestFactor)
                                         : (telem.crestFactor > 6.0f || telem.crestFactor < 0.01f);
                bool goodPhase = (telem.correlation > 0.3f);

                if (goodLevel && goodCrest && goodPhase) {
                    TrackIssue optimalIssue;
                    optimalIssue.slotIndex = info.slotIndex;
                    optimalIssue.trackName = issue.trackName;
                    optimalIssue.isOptimal = true;
                    optimalIssue.isCritical = false;
                    optimalIssue.domain = "gain";
                    optimalIssue.issueType = "SALUDABLE";
                    optimalIssue.severity = 0.1f;
                    optimalIssue.currentValue = peak;
                    optimalIssue.targetValue = hasRole ? expProfile.peakTargetDb : -12.0f;
                    juce::String roleHint = hasRole ? (" segun perfil de " + juce::String(expProfile.name)) : "";
                    optimalIssue.description = issue.trackName + " tiene niveles optimos" + roleHint;
                    issues.push_back(optimalIssue);
                }
            }
        }
    });

    // Sort by severity descending
    std::sort(issues.begin(), issues.end(), [](const TrackIssue& a, const TrackIssue& b) {
        return a.severity > b.severity;
    });

    return issues;
}

// ═══════════════════════════════════════════════════════════════════════════
//  IDENTIFY OPTIMAL TRACKS — Pistas que estan en rango optimo
// ═══════════════════════════════════════════════════════════════════════════

std::vector<int> CoachEngine::identifyOptimalTracks()
{
    std::vector<int> optimal;

    auto& registry = sharedData_.getSlotRegistry();
    if (registry.activeCount() == 0)
        return optimal;

    registry.forEachActive([&](const SlotInfo& info) {
        auto telem = getLatestTelemetry(info.slotIndex);
        if (telem.timestamp == 0) return;

        float peak = juce::jmax(telem.peakLeft, telem.peakRight);

        bool goodLevel = (peak > -24.0f && peak < -6.0f);
        bool goodCrest = (telem.crestFactor > 6.0f || telem.crestFactor < 0.01f);
        bool goodPhase = (telem.correlation > 0.3f);

        if (goodLevel && goodCrest && goodPhase)
            optimal.push_back(info.slotIndex);
    });

    return optimal;
}

// ═══════════════════════════════════════════════════════════════════════════
//  GENERATE OPTIONS FOR ISSUE — Crea opciones A/B para un issue
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::generateOptionsForIssue(TrackIssue& issue, const juce::String& genre)
{
    juce::ignoreUnused(genre);

    if (issue.domain == "gain") {
        float delta = issue.suggestedDelta;
        if (delta < 0.0f) delta = -delta;
        issue.optionA = issue.actionVerb + " el fader " + juce::String(delta, 1) + " dB";
        issue.optionB = issue.actionVerb + " el output del plugin " + juce::String(delta * 0.7f, 1) + " dB";
    } else if (issue.domain == "dynamics") {
        issue.optionA = "Reducir ratio del compresor a 2:1";
        issue.optionB = "Subir threshold del compresor 3 dB";
    } else if (issue.domain == "spatial") {
        issue.optionA = "Invertir fase de un canal";
        issue.optionB = "Reducir ancho est\xC3\xA9" "reo o centrar panorama";
    } else if (issue.domain == "masking") {
        issue.optionA = "Aplicar EQ subtractivo en la banda conflictiva";
        issue.optionB = "Ajustar panoramas para separar las pistas";
    } else {
        issue.optionA = "Revisar procesamiento de la pista";
        issue.optionB = "Consultar an\xC3\xA1" "lisis detallado";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  MASTER COOLDOWN RESET — Limpia cooldowns al cambiar de modo
// ═══════════════════════════════════════════════════════════════════════════

void CoachEngine::resetMasterCooldowns() noexcept
{
    lastPeakWarningUs_ = 0;
    lastCrestWarningUs_ = 0;
    lastPhaseWarningUs_ = 0;
    lastHeadroomWarningUs_ = 0;
    lastTonalWarningUs_ = 0;
    lastDynamicWarningUs_ = 0;
    lastLoudnessWarningUs_ = 0;
    lastMaskingWarningUs_ = 0;
    lastPairwiseMaskingWarningUs_ = 0;
    lastPrePostWarningUs_ = 0;
    lastUnmonitoredWarningUs_ = 0;
    lastBusBalanceWarningUs_ = 0;
    lastTransientWarningUs_ = 0;
    lastCrestBandWarningUs_ = 0;
    lastStereoWidthWarningUs_ = 0;
    lastReferenceGapMessageUs_ = 0;
    lastReferenceGapImprovedUs_ = 0;
    lastReferenceGapWorsenedUs_ = 0;
    lastProactiveTipTimeUs_ = 0;
    lastProactiveLlmTipTimeUs_ = 0;
    lastCelebrationTimeUs_ = 0;
    lastConsolidatedAnalysisUs_ = 0;
    lastSemanticAnalysisUs_ = 0;
}

} // namespace mixcoach
