#include "ReferenceProfile.h"
#include "DifferenceProfile.h"     // para computeFromMix
#include "../audio/ReferenceAnalyzer.h" // para computeFromReference
#include "../../Common/types/LogHelper.h"
#include <algorithm>
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Builders
    // ═══════════════════════════════════════════════════════════════════════════

    ReferenceProfile ReferenceProfile::computeFromMix(const DifferenceProfile& dp) noexcept
    {
        ReferenceProfile p;
        p.valid               = dp.valid;
        p.timestampUs         = juce::Time::getMillisecondCounter() * 1000;
        p.referenceName       = dp.referenceName;
        p.durationSeconds     = dp.durationSeconds;
        p.integratedLUFS      = dp.mixIntegratedLUFS;
        p.shortTermLUFS       = dp.mixShortTermLUFS;
        p.momentaryLUFS       = dp.mixMomentaryLUFS;
        p.truePeakDBTP        = dp.mixTruePeakDBTP;
        p.crestFactor         = dp.mixCrestFactor;
        p.correlation         = dp.mixCorrelation;
        p.loudnessRange       = dp.mixLoudnessRange;
        p.spectralCentroidHz  = dp.mixSpectralCentroidHz;
        // stereoWidth no está en DifferenceProfile — se queda en 0.0f

        for (int i = 0; i < 6; ++i)
            p.regionEnergy[i] = dp.mixRegionEnergy[i];

        // Scores
        p.subBalance      = computeSubBalance(p.regionEnergy);
        p.punchScore      = computePunchScore(p.crestFactor);
        p.densityScore    = computeDensityScore(p.crestFactor, p.loudnessRange, p.regionEnergy);
        p.brightnessScore = computeBrightnessScore(p.spectralCentroidHz, p.regionEnergy);
        p.dynamicScore    = computeDynamicScore(p.crestFactor, p.loudnessRange);
        return p;
    }

    ReferenceProfile ReferenceProfile::computeFromReference(const ReferenceAnalyzer& ref) noexcept
    {
        ReferenceProfile p;
        if (!ref.hasReference()) return p;

        p.valid          = true;
        p.timestampUs    = juce::Time::getMillisecondCounter() * 1000;
        p.referenceName  = ref.getFileName();
        p.filePath       = ref.getFilePath();
        p.durationSeconds = ref.getDuration();
        p.sampleRate     = ref.getSampleRate();

        // LUFS + TruePeak + LoudnessRange
        p.momentaryLUFS  = ref.getMomentaryLUFS();
        p.shortTermLUFS  = ref.getShortTermLUFS();
        p.integratedLUFS = ref.getIntegratedLUFS();
        p.truePeakDBTP   = ref.getTruePeakDBTP();
        p.loudnessRange  = ref.getLoudnessRange();

        // Crest = peak - RMS from AudioAnalysis
        {
            const auto& analysis = ref.getAnalysis();
            float peak = analysis.getPeak();
            float rms  = analysis.getRMS();
            if (rms > -60.0f && peak > -60.0f)
                p.crestFactor = peak - rms;
        }

        // Correlation desde AudioAnalysis
        p.correlation = ref.getAnalysis().getCorrelation();

        // Stereo width estimado desde el buffer de audio L/R
        {
            const auto& buf = ref.getAudioBuffer();
            if (buf.getNumChannels() >= 2 && buf.getNumSamples() > 0) {
                int numSamples = buf.getNumSamples();
                int step       = std::max(1, numSamples / 1024); // Decimar ~1024 samples
                float sumL2 = 0.0f, sumR2 = 0.0f, sumLR = 0.0f;
                int count = 0;
                for (int i = 0; i < numSamples; i += step) {
                    float l = buf.getSample(0, i);
                    float r = buf.getSample(1, i);
                    sumL2 += l * l;
                    sumR2 += r * r;
                    sumLR += l * r;
                    ++count;
                }
                if (count > 0) {
                    float avgL2 = sumL2 / count;
                    float avgR2 = sumR2 / count;
                    float avgLR = sumLR / count;
                    float denom = std::sqrt(avgL2 * avgR2);
                    if (denom > 1e-10f) {
                        float corr = avgLR / denom; // -1 a +1
                        // stereoWidth = 1 - |correlation| (0=mono, 1=full stereo)
                        p.stereoWidth = 1.0f - std::abs(corr);
                    }
                }
            }
        }

        // 7 band energies de la referencia → 6 regiones
        // ReferenceAnalyzer usa 7 bandas: Sub(0), Bass(1), Low-Mid(2), High-Mid(3), Presence(4), High(5), Air(6)
        // Mapeamos a 6 regiones: Sub=0, Bass=1, LoMid=2, HiMid=3, Pres/High=4, Air=5
        float band7[7];
        for (int b = 0; b < 7; ++b) band7[b] = ref.getBandEnergy(b);
        p.regionEnergy[0] = band7[0];                 // Sub
        p.regionEnergy[1] = band7[1];                 // Bass
        p.regionEnergy[2] = band7[2];                 // Low-Mid
        p.regionEnergy[3] = band7[3];                 // High-Mid
        p.regionEnergy[4] = (band7[4] + band7[5]) * 0.5f; // Presence + High → Presence
        p.regionEnergy[5] = band7[6];                 // Air

        // Spectral centroid estimado desde region energies
        {
            static const float kRegionCenterFreqs[6] = {60.0f, 200.0f, 700.0f, 2000.0f, 5000.0f, 10000.0f};
            double weightedSum = 0.0;
            double totalEnergy = 0.0;
            for (int r = 0; r < 6; ++r) {
                if (p.regionEnergy[r] > -80.0f) {
                    float linear = juce::Decibels::decibelsToGain(p.regionEnergy[r]);
                    weightedSum += static_cast<double>(linear) * kRegionCenterFreqs[r];
                    totalEnergy += static_cast<double>(linear);
                }
            }
            if (totalEnergy > 0.0)
                p.spectralCentroidHz = static_cast<float>(weightedSum / totalEnergy);
        }

        // Scores
        p.subBalance      = computeSubBalance(p.regionEnergy);
        p.punchScore      = computePunchScore(p.crestFactor);
        p.densityScore    = computeDensityScore(p.crestFactor, p.loudnessRange, p.regionEnergy);
        p.brightnessScore = computeBrightnessScore(p.spectralCentroidHz, p.regionEnergy);
        p.dynamicScore    = computeDynamicScore(p.crestFactor, p.loudnessRange);
        return p;
    }

    ReferenceProfile ReferenceProfile::computeFromData(
        float crestFactor, float loudnessRange, float spectralCentroidHz,
        const float regionEnergy[6],
        float integratedLUFS, float truePeak, float stereoWidth, float correlation) noexcept
    {
        ReferenceProfile p;
        p.timestampUs        = juce::Time::getMillisecondCounter() * 1000;
        p.integratedLUFS     = integratedLUFS;
        p.truePeakDBTP       = truePeak;
        p.stereoWidth        = stereoWidth;
        p.correlation        = correlation;
        p.crestFactor        = crestFactor;
        p.loudnessRange      = loudnessRange;
        p.spectralCentroidHz = spectralCentroidHz;
        for (int i = 0; i < 6; ++i) p.regionEnergy[i] = regionEnergy[i];

        // Validez: al menos crestFactor > 0 o LUFS > -90 indican datos reales
        p.valid = (crestFactor > 0.1f || integratedLUFS > -90.0f);

        p.subBalance      = computeSubBalance(regionEnergy);
        p.punchScore      = computePunchScore(crestFactor);
        p.densityScore    = computeDensityScore(crestFactor, loudnessRange, regionEnergy);
        p.brightnessScore = computeBrightnessScore(spectralCentroidHz, regionEnergy);
        p.dynamicScore    = computeDynamicScore(crestFactor, loudnessRange);
        return p;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Per-Score Compute Functions
    // ═══════════════════════════════════════════════════════════════════════════

    float ReferenceProfile::computeSubBalance(const float regionEnergy[6]) noexcept
    {
        float lowEnd = 0.0f, rest = 0.0f;
        int lowCount = 0, restCount = 0;
        for (int i = 0; i < 2; ++i)
            if (regionEnergy[i] > -80.0f) { lowEnd += regionEnergy[i]; ++lowCount; }
        for (int i = 2; i < 6; ++i)
            if (regionEnergy[i] > -80.0f) { rest += regionEnergy[i]; ++restCount; }
        if (lowCount == 0 || restCount == 0) return 0.5f;

        float ratioDb = (lowEnd / lowCount) - (rest / restCount);
        return juce::jmap(juce::jlimit(-12.0f, 6.0f, ratioDb), -12.0f, 6.0f, 0.0f, 1.0f);
    }

    float ReferenceProfile::computePunchScore(float crestFactor) noexcept
    {
        return juce::jmap(juce::jlimit(6.0f, 14.0f, crestFactor), 6.0f, 14.0f, 0.0f, 1.0f);
    }

    float ReferenceProfile::computeDensityScore(
        float crestFactor, float loudnessRange, const float regionEnergy[6]) noexcept
    {
        float densityFromCrest = 1.0f - juce::jmap(juce::jlimit(6.0f, 14.0f, crestFactor),
                                                     6.0f, 14.0f, 0.0f, 1.0f);
        float densityFromLRA   = 1.0f - juce::jmap(juce::jlimit(4.0f, 12.0f, loudnessRange),
                                                     4.0f, 12.0f, 0.0f, 1.0f);

        float maxEnergy = -100.0f, minEnergy = 0.0f;
        int validBands = 0;
        for (int i = 0; i < 6; ++i) {
            if (regionEnergy[i] > -80.0f) {
                if (validBands == 0) maxEnergy = minEnergy = regionEnergy[i];
                else {
                    if (regionEnergy[i] > maxEnergy) maxEnergy = regionEnergy[i];
                    if (regionEnergy[i] < minEnergy) minEnergy = regionEnergy[i];
                }
                ++validBands;
            }
        }

        float densityFromFill = 0.5f;
        if (validBands >= 3) {
            float gap = maxEnergy - minEnergy;
            densityFromFill = 1.0f - juce::jmap(juce::jlimit(6.0f, 18.0f, gap), 6.0f, 18.0f, 0.0f, 1.0f);
        }

        return densityFromCrest * 0.35f + densityFromLRA * 0.35f + densityFromFill * 0.30f;
    }

    float ReferenceProfile::computeBrightnessScore(
        float spectralCentroidHz, const float regionEnergy[6]) noexcept
    {
        float brightnessFromCentroid = juce::jmap(
            juce::jlimit(2000.0f, 8000.0f, spectralCentroidHz),
            2000.0f, 8000.0f, 0.0f, 1.0f);

        float highEnergy = 0.0f, lowEnergy = 0.0f;
        int highCount = 0, lowCount = 0;
        for (int i = 4; i < 6; ++i)
            if (regionEnergy[i] > -80.0f) { highEnergy += regionEnergy[i]; ++highCount; }
        for (int i = 0; i < 2; ++i)
            if (regionEnergy[i] > -80.0f) { lowEnergy += regionEnergy[i]; ++lowCount; }

        float brightnessFromRatio = 0.5f;
        if (highCount > 0 && lowCount > 0) {
            float ratio = (highEnergy / highCount) - (lowEnergy / lowCount);
            brightnessFromRatio = juce::jmap(juce::jlimit(-6.0f, 6.0f, ratio), -6.0f, 6.0f, 0.0f, 1.0f);
        }

        return brightnessFromCentroid * 0.5f + brightnessFromRatio * 0.5f;
    }

    float ReferenceProfile::computeDynamicScore(float crestFactor, float loudnessRange) noexcept
    {
        float dynFromCrest = juce::jmap(juce::jlimit(6.0f, 14.0f, crestFactor), 6.0f, 14.0f, 0.0f, 1.0f);
        float dynFromLRA   = juce::jmap(juce::jlimit(4.0f, 12.0f, loudnessRange), 4.0f, 12.0f, 0.0f, 1.0f);
        return dynFromCrest * 0.5f + dynFromLRA * 0.5f;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Serialization — JSON (via juce::DynamicObject)
    // ═══════════════════════════════════════════════════════════════════════════

    #define ID(s) juce::Identifier(juce::String(s))

    void ReferenceProfile::toJson(juce::DynamicObject& obj) const
    {
        obj.setProperty(ID("valid"),       juce::var(valid));
        obj.setProperty(ID("timestampUs"),  juce::var(static_cast<juce::int64>(timestampUs)));
        obj.setProperty(ID("referenceName"), juce::var(referenceName));
        obj.setProperty(ID("filePath"),     juce::var(filePath));
        obj.setProperty(ID("genreGuess"),   juce::var(genreGuess));
        obj.setProperty(ID("durationSeconds"), juce::var(durationSeconds));
        obj.setProperty(ID("sampleRate"),   juce::var(sampleRate));

        obj.setProperty(ID("integratedLUFS"),     juce::var((double)integratedLUFS));
        obj.setProperty(ID("shortTermLUFS"),      juce::var((double)shortTermLUFS));
        obj.setProperty(ID("momentaryLUFS"),      juce::var((double)momentaryLUFS));
        obj.setProperty(ID("truePeakDBTP"),       juce::var((double)truePeakDBTP));
        obj.setProperty(ID("crestFactor"),        juce::var((double)crestFactor));
        obj.setProperty(ID("stereoWidth"),        juce::var((double)stereoWidth));
        obj.setProperty(ID("correlation"),        juce::var((double)correlation));
        obj.setProperty(ID("loudnessRange"),      juce::var((double)loudnessRange));
        obj.setProperty(ID("spectralCentroidHz"), juce::var((double)spectralCentroidHz));

        obj.setProperty(ID("subBalance"),      juce::var((double)subBalance));
        obj.setProperty(ID("punchScore"),      juce::var((double)punchScore));
        obj.setProperty(ID("densityScore"),    juce::var((double)densityScore));
        obj.setProperty(ID("brightnessScore"), juce::var((double)brightnessScore));
        obj.setProperty(ID("dynamicScore"),    juce::var((double)dynamicScore));

        // Array: regionEnergy[6]
        juce::Array<juce::var> arr;
        for (int i = 0; i < 6; ++i)
            arr.add(juce::var((double)regionEnergy[i]));
        obj.setProperty(ID("regionEnergy"), arr);
    }

    ReferenceProfile ReferenceProfile::fromJson(const juce::DynamicObject& obj)
    {
        ReferenceProfile p;

        auto rd = [&](const char* key, double def) -> double {
            juce::var v = obj.getProperty(ID(key));
            if (!v.isVoid() && !v.isObject() && !v.isArray() && !v.isString()) return (double)v;
            return def;
        };
        auto rs = [&](const char* key, const char* def) -> juce::String {
            juce::var v = obj.getProperty(ID(key));
            return v.isString() ? v.toString() : juce::String(def);
        };

        p.valid          = (bool)rd("valid", 0.0);
        p.timestampUs    = (int64_t)rd("timestampUs", 0.0);
        p.referenceName  = rs("referenceName", "");
        p.filePath       = rs("filePath", "");
        p.genreGuess     = rs("genreGuess", "");
        p.durationSeconds = rd("durationSeconds", 0.0);
        p.sampleRate     = (int)rd("sampleRate", 0.0);

        p.integratedLUFS     = (float)rd("integratedLUFS", -100.0);
        p.shortTermLUFS      = (float)rd("shortTermLUFS", -100.0);
        p.momentaryLUFS      = (float)rd("momentaryLUFS", -100.0);
        p.truePeakDBTP       = (float)rd("truePeakDBTP", -100.0);
        p.crestFactor        = (float)rd("crestFactor", 0.0);
        p.stereoWidth        = (float)rd("stereoWidth", 0.0);
        p.correlation        = (float)rd("correlation", 0.0);
        p.loudnessRange      = (float)rd("loudnessRange", 0.0);
        p.spectralCentroidHz = (float)rd("spectralCentroidHz", 0.0);

        p.subBalance      = (float)rd("subBalance", 0.5);
        p.punchScore      = (float)rd("punchScore", 0.5);
        p.densityScore    = (float)rd("densityScore", 0.5);
        p.brightnessScore = (float)rd("brightnessScore", 0.5);
        p.dynamicScore    = (float)rd("dynamicScore", 0.5);

        // Array: regionEnergy[6]
        {
            juce::var arrVar = obj.getProperty(ID("regionEnergy"));
            if (arrVar.isArray()) {
                auto* arr = arrVar.getArray();
                for (int i = 0; i < 6 && i < arr->size(); ++i)
                    p.regionEnergy[i] = (float)(double)(*arr)[i];
            }
        }

        return p;
    }

    bool ReferenceProfile::saveToFile(const juce::File& file) const
    {
        try {
            juce::DynamicObject obj;
            toJson(obj);
            juce::File parent = file.getParentDirectory();
            if (!parent.exists()) parent.createDirectory();
            juce::FileOutputStream fos(file);
            if (fos.openedOk()) {
                fos.setPosition(0);
                fos.truncate();
                juce::JSON::writeToStream(fos, juce::var(&obj));
                return true;
            }
        } catch (const std::exception& e) {
            LogHelper::writeToLog("[ReferenceProfile] Error saving: " + juce::String(e.what()));
        } catch (...) {
            LogHelper::writeToLog("[ReferenceProfile] Unknown error saving profile");
        }
        return false;
    }

    ReferenceProfile ReferenceProfile::loadFromFile(const juce::File& file)
    {
        if (!file.existsAsFile()) return ReferenceProfile{};
        try {
            juce::FileInputStream fis(file);
            if (fis.openedOk()) {
                auto parsed = juce::JSON::parse(fis);
                if (auto* obj = parsed.getDynamicObject())
                    return fromJson(*obj);
            }
        } catch (...) {
            LogHelper::writeToLog("[ReferenceProfile] Excepcion desconocida al cargar perfil desde " + file.getFileName());
        }
        return ReferenceProfile{};
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Text Helpers
    // ═══════════════════════════════════════════════════════════════════════════

    juce::String ReferenceProfile::toShortText() const noexcept
    {
        return "SubBal: " + juce::String(subBalance, 2)
            + " | Punch: " + juce::String(punchScore, 2)
            + " | Density: " + juce::String(densityScore, 2)
            + " | Bright: " + juce::String(brightnessScore, 2)
            + " | Dynamic: " + juce::String(dynamicScore, 2);
    }

    juce::String ReferenceProfile::toVerboseText() const noexcept
    {
        auto label = [](float v, const char* low, const char* mid, const char* high) -> const char* {
            if (v < 0.33f) return low;
            if (v < 0.66f) return mid;
            return high;
        };

        auto fmt = [](float v, const char* unit) -> juce::String {
            if (v < -90.0f) return "-inf " + juce::String(unit);
            return juce::String(v, 1) + " " + juce::String(unit);
        };

        juce::String s;
        s += "[REFERENCE PROFILE]";
        if (referenceName.isNotEmpty()) s += " " + referenceName;
        s += "\n";

        s += "── Raw Metrics ────────────────────────────────\n";
        s += "  Integrated LUFS:  " + fmt(integratedLUFS, "LUFS") + "\n";
        s += "  Short-Term LUFS:  " + fmt(shortTermLUFS, "LUFS") + "\n";
        s += "  Momentary LUFS:   " + fmt(momentaryLUFS, "LUFS") + "\n";
        s += "  True Peak:        " + fmt(truePeakDBTP, "dBTP") + "\n";
        s += "  Crest Factor:     " + fmt(crestFactor, "dB") + "\n";
        s += "  Stereo Width:     " + juce::String(stereoWidth, 2) + "\n";
        s += "  Correlation:      " + juce::String(correlation, 2) + "\n";
        s += "  Loudness Range:   " + fmt(loudnessRange, "LU") + "\n";
        s += "  Centroid:         " + juce::String(spectralCentroidHz, 0) + " Hz\n";
        s += "\n";

        s += "── High-Level Scores ──────────────────────────\n";
        s += "  Sub Balance:  " + juce::String(subBalance, 2) + " (" + label(subBalance, "Thin bass", "Balanced", "Bass-heavy") + ")\n";
        s += "  Punch:        " + juce::String(punchScore, 2) + " (" + label(punchScore, "Compressed", "Moderate", "Punchy") + ")\n";
        s += "  Density:      " + juce::String(densityScore, 2) + " (" + label(densityScore, "Hollow", "Moderate", "Dense") + ")\n";
        s += "  Brightness:   " + juce::String(brightnessScore, 2) + " (" + label(brightnessScore, "Dark", "Neutral", "Bright") + ")\n";
        s += "  Dynamic:      " + juce::String(dynamicScore, 2) + " (" + label(dynamicScore, "Compressed", "Moderate", "Dynamic") + ")\n";

        if (genreGuess.isNotEmpty())
            s += "  Genre Guess:  " + genreGuess + "\n";

        return s;
    }

} // namespace mixcoach
