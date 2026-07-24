#include "ReferenceSummary.h"
#include "CoachEngine.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  ReferenceSummary implementations
    // ═══════════════════════════════════════════════════════════════════════════

    float ReferenceSummary::energyFromLUFS(float l) noexcept
    {
        if (l <= -30.0f) return 0.0f;
        if (l >= -6.0f)  return 1.0f;
        return (l + 30.0f) / 24.0f;
    }

    float ReferenceSummary::brightnessFromCentroid(float hz) noexcept
    {
        if (hz <= 200.0f)  return 0.0f;
        if (hz >= 3500.0f) return 1.0f;
        return (hz - 200.0f) / 3300.0f;
    }

    float ReferenceSummary::widthFromCorrelation(float c) noexcept
    {
        float a = std::abs(c);
        if (a >= 1.0f) return 0.0f;
        return 1.0f - a;
    }

    float ReferenceSummary::dynamicsFromCrest(float db) noexcept
    {
        if (db <= 2.0f)  return 0.0f;
        if (db >= 18.0f) return 1.0f;
        return (db - 2.0f) / 16.0f;
    }

    void ReferenceSummary::regionEnergies(const float be[30], float re[6]) noexcept
    {
        constexpr int kR[6][2] = {{0,2},{2,5},{5,10},{10,18},{18,25},{25,30}};
        for (int r = 0; r < 6; ++r) {
            float s = 0.0f; int n = 0;
            for (int b = kR[r][0]; b < kR[r][1] && b < 30; ++b)
                if (be[b] > -80.0f) { s += be[b]; ++n; }
            re[r] = (n > 0) ? (s / n) : -100.0f;
        }
    }

    float ReferenceSummary::depthFromProfile(const float be[30], float c, float hz) noexcept
    {
        float re[6]; regionEnergies(be, re);
        float subE = (re[0] > -80.0f) ? (re[0] + 60.0f) / 60.0f : 0.0f;
        float basE = (re[1] > -80.0f) ? (re[1] + 60.0f) / 60.0f : 0.0f;
        float low  = juce::jlimit(0.0f, 1.0f, (subE + basE) * 0.5f);
        float corr = 1.0f - std::abs(c - 0.6f);
        float brPen = 1.0f - juce::jlimit(0.0f, 1.0f, hz / 4000.0f * 0.3f);
        return juce::jlimit(0.0f, 1.0f, low * 0.50f + corr * 0.30f + brPen * 0.20f);
    }

    juce::String ReferenceSummary::inferGenre(const float be[30], float l, float cr,
                                               float c, float hz) noexcept
    {
        juce::ignoreUnused(hz);
        float re[6]; regionEnergies(be, re);
        float tot = 0.0f;
        for (int r = 0; r < 6; ++r) if (re[r] > -80.0f) tot += (re[r] + 80.0f);
        if (tot < 1.0f) return "Unknown";

        auto rr = [&](int r) -> float { return (re[r] > -80.0f) ? ((re[r] + 80.0f) / tot) : 0.0f; };
        float sub = rr(0), bas = rr(1), loM = rr(2), hiM = rr(3), pre = rr(4), air = rr(5);
        float lowF = sub + bas, mid = loM + hiM, high = pre + air;
        float lScore = energyFromLUFS(l);
        bool comp = (cr < 6.0f);
        bool vWide = (c < 0.3f), mWide = (c >= 0.3f && c < 0.6f), narr = (c >= 0.6f);
        float bal = 1.0f - (std::abs(lowF - 0.35f) + std::abs(mid - 0.40f) + std::abs(high - 0.25f)) * 1.5f;

        struct GS { const char* n; float s; };
        GS gs[8]; int g = 0;
        gs[g++] = {"EDM",        lScore*0.25f + (comp?0.20f:0)+vWide*0.15f+high*0.20f+sub*0.20f};
        gs[g++] = {"Afrobeat",   (1-std::abs(lScore-0.5f))*0.20f+mWide*0.20f+mid*0.30f+(1-std::abs(high-0.25f))*0.15f+sub*0.15f};
        gs[g++] = {"Pop",        (1-std::abs(lScore-0.4f))*0.15f+bal*0.35f+mWide*0.15f+(1-comp*0.5f)*0.15f};
        gs[g++] = {"Hip-Hop",    lowF*0.35f+(1-std::abs(lScore-0.55f))*0.20f+mWide*0.10f+(comp?0.10f:0)};
        gs[g++] = {"Rock",       lScore*0.15f+(1-comp)*0.15f+narr*0.20f+hiM*0.25f+bas*0.15f};
        gs[g++] = {"Jazz",       (1-lScore)*0.20f+(1-comp)*0.25f+mWide*0.15f+loM*0.25f};
        gs[g++] = {"Classical",  (1-lScore)*0.20f+(1-comp)*0.30f+vWide*0.15f+mid*0.20f};
        gs[g++] = {"Electronic", lScore*0.25f+comp*0.20f+(narr||mWide?0.10f:0)+sub*0.25f};

        float best = 0.0f; juce::String bg = "Unknown";
        for (int i = 0; i < g; ++i) if (gs[i].s > best) { best = gs[i].s; bg = gs[i].n; }
        return (best >= 0.30f) ? bg : "Unknown";
    }

    juce::String ReferenceSummary::energyLabelFor(float s) noexcept
    {
        if (s < 0.20f) return "Very Low";
        if (s < 0.40f) return "Low";
        if (s < 0.60f) return "Moderate";
        if (s < 0.80f) return "High";
        return "Very High";
    }

    juce::String ReferenceSummary::brightnessLabelFor(float s) noexcept
    {
        if (s < 0.15f) return "Dark";
        if (s < 0.30f) return "Warm";
        if (s < 0.50f) return "Balanced";
        if (s < 0.70f) return "Bright";
        return "Very Bright";
    }

    juce::String ReferenceSummary::depthLabelFor(float s) noexcept
    {
        if (s < 0.20f) return "Flat";
        if (s < 0.40f) return "Shallow";
        if (s < 0.60f) return "Moderate";
        if (s < 0.80f) return "Deep";
        return "Very Deep";
    }

    juce::String ReferenceSummary::dynamicsLabelFor(float s) noexcept
    {
        if (s < 0.20f) return "Max Compressed";
        if (s < 0.40f) return "Compressed";
        if (s < 0.60f) return "Moderate";
        if (s < 0.80f) return "Dynamic";
        return "Very Dynamic";
    }

    juce::String ReferenceSummary::widthLabelFor(float s) noexcept
    {
        if (s < 0.15f) return "Mono";
        if (s < 0.35f) return "Narrow";
        if (s < 0.55f) return "Moderate";
        if (s < 0.75f) return "Wide";
        return "Very Wide";
    }

    ReferenceSummary ReferenceSummary::compute(const ReferenceFingerprint& fp)
    {
        ReferenceSummary s;
        if (!fp.valid) return s;

        s.valid              = true;
        s.integratedLUFS     = fp.lufsIntegrated;
        s.crestFactor        = fp.crestFactor;
        s.correlation        = fp.correlation;
        s.spectralCentroidHz = fp.spectralCentroidHz;
        s.lufsMomentary      = fp.lufsMomentary;
        s.truePeakDBTP       = fp.truePeakDBTP;

        s.energy      = energyFromLUFS(fp.lufsIntegrated);
        s.brightness  = brightnessFromCentroid(fp.spectralCentroidHz);
        s.depth       = depthFromProfile(fp.bandEnergies, fp.correlation, fp.spectralCentroidHz);
        s.dynamics    = dynamicsFromCrest(fp.crestFactor);
        s.stereoWidth = widthFromCorrelation(fp.correlation);

        s.energyLabel     = energyLabelFor(s.energy);
        s.brightnessLabel = brightnessLabelFor(s.brightness);
        s.depthLabel      = depthLabelFor(s.depth);
        s.dynamicsLabel   = dynamicsLabelFor(s.dynamics);
        s.widthLabel      = widthLabelFor(s.stereoWidth);

        s.inferredGenre = inferGenre(fp.bandEnergies, fp.lufsIntegrated, fp.crestFactor,
                                     fp.correlation, fp.spectralCentroidHz);
        return s;
    }

    juce::String ReferenceSummary::toShortDisplay() const
    {
        if (!valid) return {};
        return juce::String(static_cast<int>(energy * 100.0f)) + "% Energy "
               "\xE2\x80\xA2 " + brightnessLabel + " \xE2\x80\xA2 " + widthLabel
               + ((inferredGenre.isNotEmpty() && inferredGenre != "Unknown")
                      ? (juce::String(" \xE2\x80\xA2 ") + inferredGenre)
                      : juce::String());
    }

    const char* ReferenceSummary::genreEmoji() const noexcept
    {
        if (inferredGenre == "EDM" || inferredGenre == "Electronic")
            return "\xF0\x9F\x92\xA1";
        if (inferredGenre == "Afrobeat") return "[DRUM]";
        if (inferredGenre == "Pop")      return "[MIC]";
        if (inferredGenre == "Hip-Hop")  return "\xF0\x9F\x91\x91";
        if (inferredGenre == "Rock")     return "[MUSIC]";
        if (inferredGenre == "Jazz")     return "\xF0\x9F\x8E\xB7";
        if (inferredGenre == "Classical") return "\xF0\x9F\x8E\xBB";
        return "[MUSIC]";
    }

} // namespace mixcoach
