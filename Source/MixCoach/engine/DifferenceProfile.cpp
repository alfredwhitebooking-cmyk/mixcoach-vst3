#include "DifferenceProfile.h"
#include "../../Common/types/Constants.h"
#include "../../Common/types/LogHelper.h"
#include "../audio/AudioAnalyzer.h"
#include "../audio/ReferenceAnalyzer.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <algorithm>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  ReferenceFingerprint — Definición local para romper dependencia cíclica
    //  con CoachEngine.h. Debe ser idéntica a la definición en CoachEngine.h.
    //  Si CoachEngine.h cambia ReferenceFingerprint, actualizar aquí también.
    // ═══════════════════════════════════════════════════════════════════════════
    struct ReferenceFingerprint
    {
        float bandEnergies[30];
        float lufsMomentary      = -100.0f;
        float lufsShortTerm      = -100.0f;
        float lufsIntegrated     = -100.0f;
        float lufsRange          = 0.0f;
        float crestFactor        = 0.0f;
        float correlation        = 0.0f;
        float truePeakDBTP       = -100.0f;
        float spectralCentroidHz = 0.0f;
        bool valid               = false;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  Helpers estáticos
    // ═══════════════════════════════════════════════════════════════════════════

    static float safeDb(float value) noexcept
    {
        return (value > 1e-10f) ? juce::Decibels::gainToDecibels(value) : -100.0f;
    }

    static juce::String fmtLUFS(float value)
    {
        if (value < -90.0f) return "-inf LUFS";
        return juce::String(value, 1) + " LUFS";
    }

    static juce::String fmtDb(float value)
    {
        if (value < -90.0f) return "-inf dB";
        return juce::String(value, 1) + " dB";
    }

    static juce::String fmtDelta(float value, const char* unit)
    {
        juce::String prefix = (value >= 0) ? "+" : "";
        return prefix + juce::String(value, 1) + " " + juce::String(unit);
    }

    static const char* regionLabel(int idx) noexcept
    {
        static const char* labels[] = {"Sub", "Bass", "Low-Mid", "High-Mid", "Presence", "Air"};
        return (idx >= 0 && idx < 6) ? labels[idx] : "?";
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  computeRegionEnergies — Agrupa 30 bandas espectrales en 6 regiones
    // ═══════════════════════════════════════════════════════════════════════════

    void DifferenceProfile::computeRegionEnergies(const float bandEnergies[30], float regionEnergies[6]) noexcept
    {
        static const int kRegionRanges[6][2] = {{0, 2}, {2, 5}, {5, 10}, {10, 18}, {18, 25}, {25, 30}};

        for (int r = 0; r < 6; ++r) {
            float sum = 0.0f;
            int count = 0;
            int start = kRegionRanges[r][0];
            int end   = std::min(kRegionRanges[r][1], 30);
            for (int b = start; b < end; ++b) {
                if (bandEnergies[b] > -90.0f) {
                    sum += bandEnergies[b];
                    ++count;
                }
            }
            regionEnergies[r] = (count > 0) ? (sum / count) : -100.0f;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  computeDeltaScore — Score agregado simple 0.0-1.0
    // ═══════════════════════════════════════════════════════════════════════════

    float DifferenceProfile::computeDeltaScore(const float regionDeltas[6], float deltaLUFS, float deltaCrest) noexcept
    {
        float lufsScore  = 1.0f - std::min(1.0f, std::abs(deltaLUFS) / 6.0f);
        float crestScore = 1.0f - std::min(1.0f, std::abs(deltaCrest) / 12.0f);

        float spectralSum = 0.0f;
        int spectralCount = 0;
        for (int r = 0; r < 6; ++r) {
            float absDelta = std::abs(regionDeltas[r]);
            if (absDelta < 90.0f) {
                spectralSum += 1.0f - std::min(1.0f, absDelta / 12.0f);
                ++spectralCount;
            }
        }
        float spectralScore = (spectralCount > 0) ? (spectralSum / spectralCount) : 0.0f;

        return lufsScore * 0.40f + crestScore * 0.20f + spectralScore * 0.40f;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  DifferenceProfile::build — Constructor estático desde fuentes existentes
    // ═══════════════════════════════════════════════════════════════════════════

    DifferenceProfile DifferenceProfile::build(const ReferenceFingerprint& fp,
                                               const AudioAnalyzer& master,
                                               const ReferenceComparison& refComparison,
                                               const std::vector<DomainGap>& gaps,
                                               const juce::String& refName,
                                               const juce::String& refPath,
                                               const juce::String& sectionLabel)
    {
        DifferenceProfile dp;

        if (!fp.valid) {
            dp.valid = false;
            return dp;
        }

        dp.valid              = true;
        dp.timestampUs        = juce::Time::getMillisecondCounter() * 1000;
        dp.referenceName      = refName;
        dp.referencePath      = refPath;
        dp.activeSectionLabel = sectionLabel;

        // Current Mix
        dp.mixIntegratedLUFS = master.getIntegratedLUFS();
        dp.mixShortTermLUFS  = master.getShortTermLUFS();
        dp.mixMomentaryLUFS  = master.getMomentaryLUFS();
        dp.mixCorrelation    = master.getMasterAnalysis().getCorrelation();
        dp.mixTruePeakDBTP   = master.getTruePeakDBTP();
        dp.mixLoudnessRange  = master.getLoudnessRange();

        {
            float masterPeak = juce::jmax(master.getLeftAnalysis().getPeak(), master.getRightAnalysis().getPeak());
            float masterRMS  = juce::jmax(master.getLeftAnalysis().getRMS(), master.getRightAnalysis().getRMS());
            if (masterRMS > -60.0f && masterPeak > -60.0f) dp.mixCrestFactor = masterPeak - masterRMS;
        }

        {
            const auto& masterAnalysis = master.getMasterAnalysis();
            const float* spectrum      = masterAnalysis.getSpectrum();
            if (spectrum != nullptr && masterAnalysis.getLastUpdateTime() > 0) {
                double weightedSum = 0.0;
                double totalEnergy = 0.0;
                for (int b = 0; b < 30; ++b) {
                    int startBin = kSpectralBandBins[b][0];
                    int endBin   = std::min(kSpectralBandBins[b][1], kNumSpectrumBins);
                    float energy = 0.0f;
                    int count    = 0;
                    for (int i = startBin; i < endBin; ++i) {
                        if (spectrum[i] > 1e-10f) {
                            energy += safeDb(spectrum[i]);
                            ++count;
                        }
                    }
                    if (count > 0 && (energy / count) > -80.0f) {
                        float avgEnergyDb  = energy / count;
                        float linearEnergy = juce::Decibels::decibelsToGain(avgEnergyDb);
                        float lowFreq      = kSpectralBandFreqs[b][0];
                        float highFreq     = kSpectralBandFreqs[b][1];
                        float centerFreq   = std::sqrt(lowFreq * highFreq);
                        if (lowFreq < 1.0f) centerFreq = 30.0f;
                        weightedSum += static_cast<double>(linearEnergy) * centerFreq;
                        totalEnergy += static_cast<double>(linearEnergy);
                    }
                }
                if (totalEnergy > 0.0) dp.mixSpectralCentroidHz = static_cast<float>(weightedSum / totalEnergy);
            }
        }

        // Reference
        dp.refIntegratedLUFS     = fp.lufsIntegrated;
        dp.refShortTermLUFS      = fp.lufsShortTerm;
        dp.refMomentaryLUFS      = fp.lufsMomentary;
        dp.refCrestFactor        = fp.crestFactor;
        dp.refCorrelation        = fp.correlation;
        dp.refTruePeakDBTP       = fp.truePeakDBTP;
        dp.refLoudnessRange      = fp.lufsRange;
        dp.refSpectralCentroidHz = fp.spectralCentroidHz;

        // Spectral regions
        computeRegionEnergies(fp.bandEnergies, dp.refRegionEnergy);

        {
            const auto& masterAnalysis = master.getMasterAnalysis();
            const float* spectrum      = masterAnalysis.getSpectrum();
            if (spectrum != nullptr && masterAnalysis.getLastUpdateTime() > 0) {
                float masterBands[30];
                for (int b = 0; b < 30; ++b) {
                    int startBin = kSpectralBandBins[b][0];
                    int endBin   = std::min(kSpectralBandBins[b][1], kNumSpectrumBins);
                    float sum    = 0.0f;
                    int count    = 0;
                    for (int i = startBin; i < endBin; ++i) {
                        float mag = spectrum[i];
                        if (mag > 1e-10f) {
                            sum += safeDb(mag);
                            ++count;
                        }
                    }
                    masterBands[b] = (count > 0) ? (sum / count) : -100.0f;
                }
                computeRegionEnergies(masterBands, dp.mixRegionEnergy);
            }
        }

        // Deltas
        dp.deltaLUFS          = dp.refIntegratedLUFS - dp.mixIntegratedLUFS;
        dp.deltaShortTermLUFS = dp.refShortTermLUFS - dp.mixShortTermLUFS;
        dp.deltaCrestFactor   = dp.refCrestFactor - dp.mixCrestFactor;
        dp.deltaCorrelation   = dp.refCorrelation - dp.mixCorrelation;
        dp.deltaTruePeak      = dp.refTruePeakDBTP - dp.mixTruePeakDBTP;
        dp.deltaLoudnessRange = dp.refLoudnessRange - dp.mixLoudnessRange;
        dp.deltaCentroidHz    = dp.refSpectralCentroidHz - dp.mixSpectralCentroidHz;

        for (int r = 0; r < 6; ++r) {
            if (dp.mixRegionEnergy[r] > -90.0f && dp.refRegionEnergy[r] > -90.0f)
                dp.deltaRegionEnergy[r] = dp.refRegionEnergy[r] - dp.mixRegionEnergy[r];
            else
                dp.deltaRegionEnergy[r] = 0.0f;
        }

        // Match scores
        dp.spectralSimilarity = refComparison.spectralSimilarity;

        if (dp.spectralSimilarity > 0.001f) {
            dp.deltaScore = dp.spectralSimilarity;
        }
        else if (dp.mixIntegratedLUFS > -90.0f && dp.refIntegratedLUFS > -90.0f) {
            dp.deltaScore = computeDeltaScore(dp.deltaRegionEnergy, dp.deltaLUFS, dp.deltaCrestFactor);
        }

        // Domain gaps
        dp.domainGaps   = gaps;
        dp.totalGaps    = static_cast<int>(gaps.size());
        dp.criticalGaps = 0;
        dp.warningGaps  = 0;
        dp.infoGaps     = 0;
        dp.praiseCount  = 0;

        for (const auto& g : gaps) {
            switch (g.severity) {
                case GapSeverity::Critical:
                    dp.criticalGaps++;
                    break;
                case GapSeverity::Warning:
                    dp.warningGaps++;
                    break;
                case GapSeverity::Info:
                    dp.infoGaps++;
                    break;
                case GapSeverity::Praise:
                    dp.praiseCount++;
                    break;
            }
        }

        LogHelper::writeToLog("[DifferenceProfile] Built: " + refName + " | " + "LUFS mix="
                              + juce::String(dp.mixIntegratedLUFS, 1) + " ref=" + juce::String(dp.refIntegratedLUFS, 1)
                              + " delta=" + juce::String(dp.deltaLUFS, 1) + " | Score=" + juce::String(dp.deltaScore, 2)
                              + " | Gaps: " + juce::String(dp.criticalGaps) + "c/" + juce::String(dp.warningGaps) + "w/"
                              + juce::String(dp.infoGaps) + "i");

        return dp;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  toJson — Serializa a DynamicObject para persistencia
    // ═══════════════════════════════════════════════════════════════════════════

    void DifferenceProfile::toJson(juce::DynamicObject& obj) const
    {
        auto id = [](const char* s) { return juce::Identifier(juce::String(s)); };

        obj.setProperty(id("valid"), juce::var(valid));
        obj.setProperty(id("timestampUs"), juce::var(static_cast<juce::int64>(timestampUs)));
        obj.setProperty(id("referenceName"), juce::var(referenceName));
        obj.setProperty(id("referencePath"), juce::var(referencePath));
        obj.setProperty(id("activeSectionLabel"), juce::var(activeSectionLabel));
        obj.setProperty(id("durationSeconds"), juce::var(durationSeconds));

        obj.setProperty(id("mixIntegratedLUFS"), juce::var((double)mixIntegratedLUFS));
        obj.setProperty(id("mixShortTermLUFS"), juce::var((double)mixShortTermLUFS));
        obj.setProperty(id("mixMomentaryLUFS"), juce::var((double)mixMomentaryLUFS));
        obj.setProperty(id("mixCrestFactor"), juce::var((double)mixCrestFactor));
        obj.setProperty(id("mixCorrelation"), juce::var((double)mixCorrelation));
        obj.setProperty(id("mixTruePeakDBTP"), juce::var((double)mixTruePeakDBTP));
        obj.setProperty(id("mixLoudnessRange"), juce::var((double)mixLoudnessRange));
        obj.setProperty(id("mixSpectralCentroidHz"), juce::var((double)mixSpectralCentroidHz));

        obj.setProperty(id("refIntegratedLUFS"), juce::var((double)refIntegratedLUFS));
        obj.setProperty(id("refShortTermLUFS"), juce::var((double)refShortTermLUFS));
        obj.setProperty(id("refMomentaryLUFS"), juce::var((double)refMomentaryLUFS));
        obj.setProperty(id("refCrestFactor"), juce::var((double)refCrestFactor));
        obj.setProperty(id("refCorrelation"), juce::var((double)refCorrelation));
        obj.setProperty(id("refTruePeakDBTP"), juce::var((double)refTruePeakDBTP));
        obj.setProperty(id("refLoudnessRange"), juce::var((double)refLoudnessRange));
        obj.setProperty(id("refSpectralCentroidHz"), juce::var((double)refSpectralCentroidHz));

        obj.setProperty(id("deltaLUFS"), juce::var((double)deltaLUFS));
        obj.setProperty(id("deltaShortTermLUFS"), juce::var((double)deltaShortTermLUFS));
        obj.setProperty(id("deltaCrestFactor"), juce::var((double)deltaCrestFactor));
        obj.setProperty(id("deltaCorrelation"), juce::var((double)deltaCorrelation));
        obj.setProperty(id("deltaTruePeak"), juce::var((double)deltaTruePeak));
        obj.setProperty(id("deltaLoudnessRange"), juce::var((double)deltaLoudnessRange));
        obj.setProperty(id("deltaCentroidHz"), juce::var((double)deltaCentroidHz));

        obj.setProperty(id("spectralSimilarity"), juce::var((double)spectralSimilarity));
        obj.setProperty(id("deltaScore"), juce::var((double)deltaScore));

        obj.setProperty(id("totalGaps"), juce::var(totalGaps));
        obj.setProperty(id("criticalGaps"), juce::var(criticalGaps));
        obj.setProperty(id("warningGaps"), juce::var(warningGaps));
        obj.setProperty(id("infoGaps"), juce::var(infoGaps));
        obj.setProperty(id("praiseCount"), juce::var(praiseCount));

        // Arrays: region energies (6 regions) — stack allocation matches SessionMap pattern
        {
            juce::Array<juce::var> mixArr, refArr, deltaArr;
            for (int r = 0; r < 6; ++r) {
                mixArr.add(juce::var((double)mixRegionEnergy[r]));
                refArr.add(juce::var((double)refRegionEnergy[r]));
                deltaArr.add(juce::var((double)deltaRegionEnergy[r]));
            }
            obj.setProperty(id("mixRegionEnergy"), mixArr);
            obj.setProperty(id("refRegionEnergy"), refArr);
            obj.setProperty(id("deltaRegionEnergy"), deltaArr);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  fromJson — Deserializa desde DynamicObject
    // ═══════════════════════════════════════════════════════════════════════════

    DifferenceProfile DifferenceProfile::fromJson(const juce::DynamicObject& obj)
    {
        DifferenceProfile dp;

        auto id = [](const char* s) { return juce::Identifier(juce::String(s)); };

        auto readDbl = [&](const char* key, double def) -> double {
            juce::var v = obj.getProperty(id(key));
            if (!v.isVoid() && !v.isObject() && !v.isArray() && !v.isString()) return (double)v;
            return def;
        };

        auto readStr = [&](const char* key, const char* def) -> juce::String {
            juce::var v = obj.getProperty(id(key));
            if (v.isString()) return v.toString();
            return juce::String(def);
        };

        dp.valid              = readDbl("valid", 0.0) > 0.5;
        dp.timestampUs        = 0;
        dp.referenceName      = readStr("referenceName", "");
        dp.referencePath      = readStr("referencePath", "");
        dp.activeSectionLabel = readStr("activeSectionLabel", "Full");
        dp.durationSeconds    = readDbl("durationSeconds", 0.0);

        dp.mixIntegratedLUFS     = (float)readDbl("mixIntegratedLUFS", -100.0);
        dp.mixShortTermLUFS      = (float)readDbl("mixShortTermLUFS", -100.0);
        dp.mixMomentaryLUFS      = (float)readDbl("mixMomentaryLUFS", -100.0);
        dp.mixCrestFactor        = (float)readDbl("mixCrestFactor", 0.0);
        dp.mixCorrelation        = (float)readDbl("mixCorrelation", 0.0);
        dp.mixTruePeakDBTP       = (float)readDbl("mixTruePeakDBTP", -100.0);
        dp.mixLoudnessRange      = (float)readDbl("mixLoudnessRange", 0.0);
        dp.mixSpectralCentroidHz = (float)readDbl("mixSpectralCentroidHz", 0.0);

        dp.refIntegratedLUFS     = (float)readDbl("refIntegratedLUFS", -100.0);
        dp.refShortTermLUFS      = (float)readDbl("refShortTermLUFS", -100.0);
        dp.refMomentaryLUFS      = (float)readDbl("refMomentaryLUFS", -100.0);
        dp.refCrestFactor        = (float)readDbl("refCrestFactor", 0.0);
        dp.refCorrelation        = (float)readDbl("refCorrelation", 0.0);
        dp.refTruePeakDBTP       = (float)readDbl("refTruePeakDBTP", -100.0);
        dp.refLoudnessRange      = (float)readDbl("refLoudnessRange", 0.0);
        dp.refSpectralCentroidHz = (float)readDbl("refSpectralCentroidHz", 0.0);

        dp.deltaLUFS          = (float)readDbl("deltaLUFS", 0.0);
        dp.deltaShortTermLUFS = (float)readDbl("deltaShortTermLUFS", 0.0);
        dp.deltaCrestFactor   = (float)readDbl("deltaCrestFactor", 0.0);
        dp.deltaCorrelation   = (float)readDbl("deltaCorrelation", 0.0);
        dp.deltaTruePeak      = (float)readDbl("deltaTruePeak", 0.0);
        dp.deltaLoudnessRange = (float)readDbl("deltaLoudnessRange", 0.0);
        dp.deltaCentroidHz    = (float)readDbl("deltaCentroidHz", 0.0);

        dp.spectralSimilarity = (float)readDbl("spectralSimilarity", 0.0);
        dp.deltaScore         = (float)readDbl("deltaScore", 0.0);

        dp.totalGaps    = (int)readDbl("totalGaps", 0.0);
        dp.criticalGaps = (int)readDbl("criticalGaps", 0.0);
        dp.warningGaps  = (int)readDbl("warningGaps", 0.0);
        dp.infoGaps     = (int)readDbl("infoGaps", 0.0);
        dp.praiseCount  = (int)readDbl("praiseCount", 0.0);

        // Arrays: region energies (6 regions)
        {
            juce::var mixVar   = obj.getProperty(id("mixRegionEnergy"));
            juce::var refVar   = obj.getProperty(id("refRegionEnergy"));
            juce::var deltaVar = obj.getProperty(id("deltaRegionEnergy"));
            if (mixVar.isArray()) {
                auto* arr = mixVar.getArray();
                for (int r = 0; r < 6 && r < arr->size(); ++r) dp.mixRegionEnergy[r] = (float)(double)(*arr)[r];
            }
            if (refVar.isArray()) {
                auto* arr = refVar.getArray();
                for (int r = 0; r < 6 && r < arr->size(); ++r) dp.refRegionEnergy[r] = (float)(double)(*arr)[r];
            }
            if (deltaVar.isArray()) {
                auto* arr = deltaVar.getArray();
                for (int r = 0; r < 6 && r < arr->size(); ++r) dp.deltaRegionEnergy[r] = (float)(double)(*arr)[r];
            }
        }

        return dp;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  toTextSummary — Resumen compacto para LLM context o debug
    // ═══════════════════════════════════════════════════════════════════════════

    juce::String DifferenceProfile::toTextSummary() const
    {
        if (!valid) return "[DIFFERENCE PROFILE] No reference loaded.\n";

        juce::String s;

        s += "[DIFFERENCE PROFILE: " + referenceName;
        if (activeSectionLabel.isNotEmpty() && activeSectionLabel != "Full") s += " - " + activeSectionLabel;
        s += "]\n";

        s += "LUFS: ref " + fmtLUFS(refIntegratedLUFS) + " | mix " + fmtLUFS(mixIntegratedLUFS) + " | delta "
             + fmtDelta(deltaLUFS, "LUFS");
        if (deltaLUFS > 1.5f) s += " (mix quieter)";
        else if (deltaLUFS < -1.5f)
            s += " (mix louder)";
        s += "\n";

        s += "Crest: ref " + juce::String(refCrestFactor, 1) + " dB" + " | mix " + juce::String(mixCrestFactor, 1)
             + " dB" + " | delta " + fmtDelta(deltaCrestFactor, "dB") + "\n";

        s += "Corr:  ref " + juce::String(refCorrelation, 2) + " | mix " + juce::String(mixCorrelation, 2) + " | delta "
             + fmtDelta(deltaCorrelation, "") + "\n";

        s += "Similarity: " + juce::String(spectralSimilarity, 2) + " | DeltaScore: " + juce::String(deltaScore, 2);

        if (totalGaps > 0) {
            s += " | Gaps: ";
            if (criticalGaps > 0) s += juce::String(criticalGaps) + " crit ";
            if (warningGaps > 0) s += juce::String(warningGaps) + " warn ";
            if (infoGaps > 0) s += juce::String(infoGaps) + " info";
            if (praiseCount > 0) s += juce::String(praiseCount) + " praise";
        }
        s += "\n";

        s += "Regions (ref - mix dB): ";
        for (int r = 0; r < 6; ++r) {
            s += juce::String(regionLabel(r)) + " " + fmtDelta(deltaRegionEnergy[r], "dB");
            if (r < 5) s += " | ";
        }
        s += "\n";

        s += "TruePeak: ref " + juce::String(refTruePeakDBTP, 1) + " dBTP" + " | mix "
             + juce::String(mixTruePeakDBTP, 1) + " dBTP\n";
        s += "LRA:      ref " + juce::String(refLoudnessRange, 1) + " LU" + " | mix "
             + juce::String(mixLoudnessRange, 1) + " LU" + " | delta " + fmtDelta(deltaLoudnessRange, "LU") + "\n";

        return s;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  toVerboseText — Versión detallada para depuración
    // ═══════════════════════════════════════════════════════════════════════════

    juce::String DifferenceProfile::toVerboseText() const
    {
        if (!valid) return "[DIFFERENCE PROFILE] INVALID\n";

        juce::String s;
        s += "╔══════════════════════════════════════════════════════════╗\n";
        s += "║          DIFFERENCE PROFILE (Verbose)                  ║\n";
        s += "╚══════════════════════════════════════════════════════════╝\n";
        s += "Reference: " + referenceName + "\n";
        s += "Section:   " + activeSectionLabel + "\n";
        s += "Duration:  " + juce::String(durationSeconds, 1) + "s\n";
        s += "Timestamp: " + juce::String(timestampUs / 1000) + "ms\n\n";

        s += "── Current Mix ──────────────────────────────────────\n";
        s += "  Integrated LUFS:  " + fmtLUFS(mixIntegratedLUFS) + "\n";
        s += "  Short-Term LUFS:  " + fmtLUFS(mixShortTermLUFS) + "\n";
        s += "  Momentary LUFS:   " + fmtLUFS(mixMomentaryLUFS) + "\n";
        s += "  Crest Factor:     " + juce::String(mixCrestFactor, 1) + " dB\n";
        s += "  Correlation:      " + juce::String(mixCorrelation, 2) + "\n";
        s += "  True Peak:        " + juce::String(mixTruePeakDBTP, 1) + " dBTP\n";
        s += "  Loudness Range:   " + juce::String(mixLoudnessRange, 1) + " LU\n";
        s += "  Centroid:         " + juce::String(mixSpectralCentroidHz, 0) + " Hz\n\n";

        s += "── Reference ─────────────────────────────────────────\n";
        s += "  Integrated LUFS:  " + fmtLUFS(refIntegratedLUFS) + "\n";
        s += "  Short-Term LUFS:  " + fmtLUFS(refShortTermLUFS) + "\n";
        s += "  Momentary LUFS:   " + fmtLUFS(refMomentaryLUFS) + "\n";
        s += "  Crest Factor:     " + juce::String(refCrestFactor, 1) + " dB\n";
        s += "  Correlation:      " + juce::String(refCorrelation, 2) + "\n";
        s += "  True Peak:        " + juce::String(refTruePeakDBTP, 1) + " dBTP\n";
        s += "  Loudness Range:   " + juce::String(refLoudnessRange, 1) + " LU\n";
        s += "  Centroid:         " + juce::String(refSpectralCentroidHz, 0) + " Hz\n\n";

        s += "── Delta ────────────────────────────────────────────\n";
        s += "  LUFS:             " + fmtDelta(deltaLUFS, "LUFS") + "\n";
        s += "  Crest:            " + fmtDelta(deltaCrestFactor, "dB") + "\n";
        s += "  Correlation:      " + fmtDelta(deltaCorrelation, "") + "\n";
        s += "  True Peak:        " + fmtDelta(deltaTruePeak, "dBTP") + "\n";
        s += "  LRA:              " + fmtDelta(deltaLoudnessRange, "LU") + "\n";
        s += "  Centroid:         " + fmtDelta(deltaCentroidHz, "Hz") + "\n\n";

        s += "── Regions (ref - mix dB) ───────────────────────────\n";
        for (int r = 0; r < 6; ++r) {
            s += juce::String("  ") + juce::String(regionLabel(r)) + ": " + "ref " + fmtDb(refRegionEnergy[r]) + " | "
                 + "mix " + fmtDb(mixRegionEnergy[r]) + " | " + "delta " + fmtDelta(deltaRegionEnergy[r], "dB") + "\n";
        }
        s += "\n";

        s += "── Match Scores ─────────────────────────────────────\n";
        s += "  Spectral Similarity: " + juce::String(spectralSimilarity, 3) + "\n";
        s += "  Delta Score:         " + juce::String(deltaScore, 3) + "\n\n";

        s += "── Domain Gaps ──────────────────────────────────────\n";
        s += "  Total: " + juce::String(totalGaps) + " | Critical: " + juce::String(criticalGaps)
             + " | Warning: " + juce::String(warningGaps) + " | Info: " + juce::String(infoGaps)
             + " | Praise: " + juce::String(praiseCount) + "\n";
        for (const auto& g : domainGaps) {
            s += "  " + g.toShortLabel() + "\n";
        }

        return s;
    }

} // namespace mixcoach
