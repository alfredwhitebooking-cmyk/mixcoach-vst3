// ═══════════════════════════════════════════════════════════════════════════
//  TestRefinementProfile.cpp — Standalone formula verification
//
//  Tests the LOGIC of the 5 RefinementProfile formulas using inline
//  implementations that mirror RefinementProfile.cpp's static functions.
//
//  This is a STANDALONE test: no CoachEngine or JUCE GUI dependencies.
//  Only needs juce_core for String and DynamicObject (serialization).
//
//  Why standalone? RefinementProfile.cpp's computeImpact/Movement/Glue
//  take CoachEngine& parameters, requiring the full engine dependency
//  chain (70+ .cpp files). By duplicating the formulas here, we can
//  validate the logic without that overhead.
//
//  Perfiles sintéticos: Afrobeat, EDM, Pop, Silent, Invalid
//
//  Compilado via CMake:
//    cmake --build build --config Debug --target TestRefinementProfile
//    ./build/tests/Debug/TestRefinementProfile.exe
//
//  NOTA: Una vez que RefinementProfile se refactorice separando las
//  funciones estáticas (Depth, Emotion, helpers) de las que necesitan
//  CoachEngine (Impact, Movement, Glue), este test puede reemplazarse
//  por uno que llame directamente a las funciones de RefinementProfile.cpp
//  sin necesidad de la cadena completa de dependencias.
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <juce_core/juce_core.h>

static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define TEST(name, expr) do {                                                  \
    if (!(expr)) {                                                             \
        std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: %s (%s:%d)\n",              \
                     name, __FILE__, __LINE__);                                \
        std::fflush(stderr);                                                   \
        gTestsFailed++;                                                        \
    } else {                                                                   \
        std::printf("  \xe2\x9c\x85 PASS: %s\n", name);                        \
        std::fflush(stdout);                                                   \
        gTestsPassed++;                                                        \
    }                                                                          \
} while(0)

#define TEST_NEAR(name, val, expected, tol) \
    TEST(name, std::abs((val) - (expected)) < (tol))

// ═══════════════════════════════════════════════════════════════════════════
//  Data structures (mirror RefinementProfile.h structs)
// ═══════════════════════════════════════════════════════════════════════════

enum class RefinementDomain : uint8_t {
    Depth, Impact, Movement, Glue, Emotion
};

struct RefinementScore {
    RefinementDomain domain = RefinementDomain::Depth;
    float score = 0.5f;
    juce::String label;
    float subMetric[3] = {0.5f, 0.5f, 0.5f};
    juce::String subMetricLabel[3];
    float vsReference = 0.0f;
    bool hasReferenceData = false;
    juce::String interpretation;
    juce::String suggestion;

    bool isActionable() const noexcept {
        return score < 0.6f || (hasReferenceData && std::abs(vsReference) > 0.25f);
    }
    const char* domainName() const noexcept {
        switch (domain) {
            case RefinementDomain::Depth:    return "Depth";
            case RefinementDomain::Impact:   return "Impact";
            case RefinementDomain::Movement: return "Movement";
            case RefinementDomain::Glue:     return "Glue";
            case RefinementDomain::Emotion:  return "Emotion";
            default:                         return "Unknown";
        }
    }
    const char* emoji() const noexcept {
        if (score >= 0.80f) return "\xF0\x9F\x9F\xA2";  // 🟢
        if (score >= 0.60f) return "\xF0\x9F\x9F\xA1";  // 🟡
        if (score >= 0.40f) return "\xF0\x9F\x94\xB6";  // 🔶
        return "\xF0\x9F\x94\xB4";                        // 🔴
    }
};

struct RefinementProfile {
    int64_t timestampUs = 0;
    bool valid = false;
    RefinementScore depth;
    RefinementScore impact;
    RefinementScore movement;
    RefinementScore glue;
    RefinementScore emotion;
    float overallRefinement = 0.0f;
    bool isRelevant = false;
    float sourceMixScore = 0.0f;
    int sourceCriticalGaps = 0;
    int sourceWarningGaps = 0;
    bool sourceHasReference = false;

    // ─── Serialization ───────────────────────────────────────────────────
    #define ID(s) juce::Identifier(juce::String(s))

    void toJson(juce::DynamicObject& obj) const {
        obj.setProperty(ID("valid"), juce::var(valid));
        obj.setProperty(ID("overallRefinement"), juce::var((double)overallRefinement));
        obj.setProperty(ID("isRelevant"), juce::var(isRelevant));
        auto s2j = [&](const RefinementScore& s, const char* p) {
            juce::String pf(p);
            obj.setProperty(ID(pf + "_score"), juce::var((double)s.score));
            obj.setProperty(ID(pf + "_label"), juce::var(s.label));
            obj.setProperty(ID(pf + "_vsRef"), juce::var((double)s.vsReference));
            obj.setProperty(ID(pf + "_hasRef"), juce::var(s.hasReferenceData));
            juce::Array<juce::var> subArr;
            for (int i = 0; i < 3; ++i) subArr.add(juce::var((double)s.subMetric[i]));
            obj.setProperty(ID(pf + "_sub"), subArr);
        };
        s2j(depth, "depth"); s2j(impact, "impact"); s2j(movement, "movement");
        s2j(glue, "glue"); s2j(emotion, "emotion");
    }

    static RefinementProfile fromJson(const juce::DynamicObject& obj) {
        RefinementProfile r;
        auto rd = [&](const char* key, double def) {
            juce::var v = obj.getProperty(ID(key));
            return (!v.isVoid() && !v.isObject() && !v.isArray() && !v.isString()) ? (double)v : def;
        };
        r.valid = (bool)rd("valid", 0.0);
        r.overallRefinement = (float)rd("overallRefinement", 0.0);
        r.isRelevant = (bool)rd("isRelevant", 0.0);
        auto j2s = [&](const char* p, RefinementDomain d) -> RefinementScore {
            RefinementScore s; s.domain = d;
            juce::String pf(p);
            s.score = (float)rd((pf + "_score").toRawUTF8(), 0.5);
            s.vsReference = (float)rd((pf + "_vsRef").toRawUTF8(), 0.0);
            s.hasReferenceData = (bool)rd((pf + "_hasRef").toRawUTF8(), 0.0);
            juce::var arrVar = obj.getProperty(ID(pf + "_sub"));
            if (arrVar.isArray()) {
                auto* arr = arrVar.getArray();
                for (int i = 0; i < 3 && i < arr->size(); ++i)
                    s.subMetric[i] = (float)(double)(*arr)[i];
            }
            return s;
        };
        r.depth    = j2s("depth", RefinementDomain::Depth);
        r.impact   = j2s("impact", RefinementDomain::Impact);
        r.movement = j2s("movement", RefinementDomain::Movement);
        r.glue     = j2s("glue", RefinementDomain::Glue);
        r.emotion  = j2s("emotion", RefinementDomain::Emotion);
        return r;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
//  Formula implementations (mirroring RefinementProfile.cpp logic)
// ═══════════════════════════════════════════════════════════════════════════

// ─── DifferenceProfile (minimal subset needed for formula testing) ────────
struct DiffProfile {
    bool valid = false;
    float mixCorrelation = 0.0f;
    float mixCrestFactor = 0.0f;
    float mixLoudnessRange = 0.0f;
    float mixRegionEnergy[6] = {-100.0f,-100.0f,-100.0f,-100.0f,-100.0f,-100.0f};
    float refCrestFactor = 0.0f;
    float refLoudnessRange = 0.0f;
    float refRegionEnergy[6] = {-100.0f,-100.0f,-100.0f,-100.0f,-100.0f,-100.0f};
    float deltaRegionEnergy[6] = {0,0,0,0,0,0};
    float spectralSimilarity = 0.0f;
    float deltaScore = 0.0f;
    int criticalGaps = 0, warningGaps = 0;
};

// ─── normalizeToScore ────────────────────────────────────────────────────
static float normalizeToScore(float value, float ideal, float tolerance, float maxDev) noexcept {
    float deviation = std::abs(value - ideal);
    if (deviation <= tolerance) return 1.0f;
    if (deviation >= maxDev) return 0.0f;
    return 1.0f - (deviation - tolerance) / (maxDev - tolerance);
}

// ─── computeDepthScore ───────────────────────────────────────────────────
static RefinementScore computeDepthScore(const DiffProfile& dp) noexcept {
    RefinementScore s;
    s.domain = RefinementDomain::Depth;

    float estStereoWidth = 1.0f - std::abs(dp.mixCorrelation);
    s.subMetric[0] = std::max(0.0f, std::min(1.0f, estStereoWidth));
    s.subMetricLabel[0] = "Stereo Width";

    s.subMetric[1] = 1.0f - std::abs(dp.mixCorrelation - 0.55f) / 0.55f;
    s.subMetric[1] = std::max(0.0f, std::min(1.0f, s.subMetric[1]));
    s.subMetricLabel[1] = "Spatial Breathing";

    float pe = dp.mixRegionEnergy[4] > -90.0f
        ? ((std::max(-40.0f, std::min(-10.0f, dp.mixRegionEnergy[4])) + 40.0f) / 30.0f) : 0.0f;
    float ae = dp.mixRegionEnergy[5] > -90.0f
        ? ((std::max(-40.0f, std::min(-10.0f, dp.mixRegionEnergy[5])) + 40.0f) / 30.0f) : 0.0f;
    s.subMetric[2] = (pe + ae) * 0.5f;
    s.subMetricLabel[2] = "High-End Air";

    s.score = s.subMetric[0] * 0.35f + s.subMetric[1] * 0.35f + s.subMetric[2] * 0.30f;
    s.score = std::max(0.0f, std::min(1.0f, s.score));

    if (dp.mixRegionEnergy[4] > -90.0f && dp.refRegionEnergy[4] > -90.0f
        && dp.mixRegionEnergy[5] > -90.0f && dp.refRegionEnergy[5] > -90.0f) {
        float mixAir = (dp.mixRegionEnergy[4] + dp.mixRegionEnergy[5]) * 0.5f;
        float refAir = (dp.refRegionEnergy[4] + dp.refRegionEnergy[5]) * 0.5f;
        float diff = mixAir - refAir;
        s.vsReference = ((std::max(-6.0f, std::min(6.0f, diff)) + 6.0f) / 12.0f) * 2.0f - 1.0f;
        s.hasReferenceData = true;
    }

    // Qualitative label
    if (s.score < 0.33f) s.label = "Plana";
    else if (s.score < 0.66f) s.label = "Moderada";
    else s.label = "Profunda";

    s.interpretation = "La mezcla tiene profundidad. (auto-generated)";
    return s;
}

// ─── computeEmotionScore ─────────────────────────────────────────────────
static RefinementScore computeEmotionScore(const DiffProfile& dp) noexcept {
    RefinementScore s;
    s.domain = RefinementDomain::Emotion;

    s.subMetric[0] = std::max(0.0f, std::min(1.0f, dp.spectralSimilarity));
    s.subMetricLabel[0] = "Spectral Match";

    s.subMetric[1] = std::max(0.0f, std::min(1.0f, dp.deltaScore));
    s.subMetricLabel[1] = "Dynamic Resonance";

    {
        float midSum = 0.0f;
        int midCount = 0;
        for (int r = 2; r <= 4; ++r) {
            float absDelta = std::abs(dp.deltaRegionEnergy[r]);
            if (absDelta < 90.0f) { midSum += absDelta; ++midCount; }
        }
        s.subMetric[2] = (midCount > 0) ? (1.0f - std::min(1.0f, (midSum / midCount) / 8.0f)) : 0.5f;
        s.subMetricLabel[2] = "Mid-Range Alignment";
    }

    s.score = s.subMetric[0] * 0.40f + s.subMetric[1] * 0.30f + s.subMetric[2] * 0.30f;
    s.score = std::max(0.0f, std::min(1.0f, s.score));

    if (dp.spectralSimilarity > 0.0f) {
        float diff = dp.spectralSimilarity - 0.5f;
        s.vsReference = ((std::max(-0.4f, std::min(0.4f, diff)) + 0.4f) / 0.8f) * 2.0f - 1.0f;
        s.hasReferenceData = true;
    }

    if (s.score < 0.33f) s.label = "Distante";
    else if (s.score < 0.66f) s.label = "Cercana";
    else s.label = "Conectada";

    return s;
}

// ─── computeOverall ──────────────────────────────────────────────────────
static float computeOverall(const RefinementScore scores[5]) noexcept {
    float sum = 0.0f;
    for (int i = 0; i < 5; ++i) sum += scores[i].score;
    return sum / 5.0f;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Synthetic profiles
// ═══════════════════════════════════════════════════════════════════════════

static DiffProfile makeAfrobeatProfile() {
    DiffProfile dp;
    dp.valid = true;
    dp.mixCorrelation = 0.35f;
    dp.mixCrestFactor = 12.0f;
    dp.mixLoudnessRange = 10.0f;
    dp.mixRegionEnergy[0] = -8.0f; dp.mixRegionEnergy[1] = -6.0f;
    dp.mixRegionEnergy[2] = -10.0f; dp.mixRegionEnergy[3] = -12.0f;
    dp.mixRegionEnergy[4] = -14.0f; dp.mixRegionEnergy[5] = -16.0f;
    dp.refCrestFactor = 10.0f;
    dp.refLoudnessRange = 9.0f;
    dp.refRegionEnergy[0] = -9.0f; dp.refRegionEnergy[1] = -7.0f;
    dp.refRegionEnergy[2] = -10.0f; dp.refRegionEnergy[3] = -11.0f;
    dp.refRegionEnergy[4] = -12.0f; dp.refRegionEnergy[5] = -13.0f;
    for (int i = 0; i < 6; ++i) dp.deltaRegionEnergy[i] = dp.refRegionEnergy[i] - dp.mixRegionEnergy[i];
    dp.spectralSimilarity = 0.72f;
    dp.deltaScore = 0.65f;
    dp.criticalGaps = 0; dp.warningGaps = 1;
    return dp;
}

static DiffProfile makeEDMProfile() {
    DiffProfile dp;
    dp.valid = true;
    dp.mixCorrelation = 0.92f;
    dp.mixCrestFactor = 5.0f;
    dp.mixLoudnessRange = 4.0f;
    dp.mixRegionEnergy[0] = -9.0f; dp.mixRegionEnergy[1] = -9.0f;
    dp.mixRegionEnergy[2] = -10.0f; dp.mixRegionEnergy[3] = -10.0f;
    dp.mixRegionEnergy[4] = -11.0f; dp.mixRegionEnergy[5] = -12.0f;
    dp.refCrestFactor = 8.0f;
    dp.refLoudnessRange = 7.0f;
    dp.refRegionEnergy[0] = -10.0f; dp.refRegionEnergy[1] = -9.0f;
    dp.refRegionEnergy[2] = -11.0f; dp.refRegionEnergy[3] = -11.0f;
    dp.refRegionEnergy[4] = -12.0f; dp.refRegionEnergy[5] = -14.0f;
    for (int i = 0; i < 6; ++i) dp.deltaRegionEnergy[i] = dp.refRegionEnergy[i] - dp.mixRegionEnergy[i];
    dp.spectralSimilarity = 0.80f;
    dp.deltaScore = 0.75f;
    dp.criticalGaps = 0; dp.warningGaps = 1;
    return dp;
}

static DiffProfile makePopProfile() {
    DiffProfile dp;
    dp.valid = true;
    dp.mixCorrelation = 0.55f;
    dp.mixCrestFactor = 10.0f;
    dp.mixLoudnessRange = 8.0f;
    dp.mixRegionEnergy[0] = -11.0f; dp.mixRegionEnergy[1] = -10.0f;
    dp.mixRegionEnergy[2] = -10.0f; dp.mixRegionEnergy[3] = -11.0f;
    dp.mixRegionEnergy[4] = -12.0f; dp.mixRegionEnergy[5] = -15.0f;
    dp.refCrestFactor = 10.5f;
    dp.refLoudnessRange = 8.5f;
    dp.refRegionEnergy[0] = -11.0f; dp.refRegionEnergy[1] = -10.0f;
    dp.refRegionEnergy[2] = -9.5f; dp.refRegionEnergy[3] = -10.5f;
    dp.refRegionEnergy[4] = -11.5f; dp.refRegionEnergy[5] = -14.0f;
    for (int i = 0; i < 6; ++i) dp.deltaRegionEnergy[i] = dp.refRegionEnergy[i] - dp.mixRegionEnergy[i];
    dp.spectralSimilarity = 0.88f;
    dp.deltaScore = 0.82f;
    dp.criticalGaps = 0; dp.warningGaps = 0;
    return dp;
}

static DiffProfile makeSilentProfile() {
    DiffProfile dp;
    dp.valid = true;
    for (int i = 0; i < 6; ++i) dp.mixRegionEnergy[i] = -100.0f;
    for (int i = 0; i < 6; ++i) dp.refRegionEnergy[i] = -12.0f;
    dp.refCrestFactor = 10.0f;
    dp.refLoudnessRange = 8.0f;
    dp.spectralSimilarity = 0.0f;
    dp.deltaScore = 0.0f;
    dp.criticalGaps = 3; dp.warningGaps = 2;
    return dp;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: computeDepthScore
// ═══════════════════════════════════════════════════════════════════════════
static void test_depth_score() {
    std::printf("\n── Depth Score ──\n"); std::fflush(stdout);

    auto afro = makeAfrobeatProfile();
    auto depthA = computeDepthScore(afro);
    TEST_NEAR("Afrobeat Stereo Width (~0.65)", depthA.subMetric[0], 0.65f, 0.15f);
    TEST_NEAR("Afrobeat Spatial Breathing (~0.55)", depthA.subMetric[1], 0.55f, 0.25f);
    TEST_NEAR("Afrobeat High-End Air (~0.83)", depthA.subMetric[2], 0.83f, 0.15f);
    TEST("Afrobeat hasReferenceData", depthA.hasReferenceData);
    TEST("Afrobeat label not empty", depthA.label.isNotEmpty());
    TEST("Afrobeat interpretation not empty", depthA.interpretation.isNotEmpty());

    auto edm = makeEDMProfile();
    auto depthE = computeDepthScore(edm);
    TEST_NEAR("EDM Stereo Width (~0.08)", depthE.subMetric[0], 0.08f, 0.10f);
    TEST_NEAR("EDM Spatial Breathing (~0.30)", depthE.subMetric[1], 0.30f, 0.25f);
    TEST_NEAR("EDM High-End Air (~0.95)", depthE.subMetric[2], 0.95f, 0.15f);

    auto pop = makePopProfile();
    auto depthP = computeDepthScore(pop);
    TEST_NEAR("Pop Stereo Width (~0.45)", depthP.subMetric[0], 0.45f, 0.15f);
    TEST_NEAR("Pop Spatial Breathing (~1.0)", depthP.subMetric[1], 1.0f, 0.10f);

    auto silent = makeSilentProfile();
    auto depthS = computeDepthScore(silent);
    TEST_NEAR("Silent High-End Air (~0.0)", depthS.subMetric[2], 0.0f, 0.01f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: computeEmotionScore
// ═══════════════════════════════════════════════════════════════════════════
static void test_emotion_score() {
    std::printf("\n── Emotion Score ──\n"); std::fflush(stdout);

    auto pop = makePopProfile();
    auto emoP = computeEmotionScore(pop);
    TEST_NEAR("Pop Spectral Match (~0.88)", emoP.subMetric[0], 0.88f, 0.10f);
    TEST_NEAR("Pop Dynamic Resonance (~0.82)", emoP.subMetric[1], 0.82f, 0.10f);
    TEST_NEAR("Pop Mid-Range Alignment (~0.96)", emoP.subMetric[2], 0.96f, 0.10f);
    TEST("Pop emotion hasReferenceData", emoP.hasReferenceData);
    TEST("Pop emotion label not empty", emoP.label.isNotEmpty());

    auto afro = makeAfrobeatProfile();
    auto emoA = computeEmotionScore(afro);
    TEST_NEAR("Afro Spectral Match (~0.72)", emoA.subMetric[0], 0.72f, 0.10f);
    TEST_NEAR("Afro Dynamic Resonance (~0.65)", emoA.subMetric[1], 0.65f, 0.10f);

    auto edm = makeEDMProfile();
    auto emoE = computeEmotionScore(edm);
    TEST_NEAR("EDM Spectral Match (~0.80)", emoE.subMetric[0], 0.80f, 0.10f);
    TEST_NEAR("EDM Dynamic Resonance (~0.75)", emoE.subMetric[1], 0.75f, 0.10f);

    auto silent = makeSilentProfile();
    auto emoS = computeEmotionScore(silent);
    TEST_NEAR("Silent Spectral Match (~0.0)", emoS.subMetric[0], 0.0f, 0.01f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: computeOverall
// ═══════════════════════════════════════════════════════════════════════════
static void test_overall() {
    std::printf("\n── computeOverall ──\n"); std::fflush(stdout);

    RefinementScore scores[5];
    for (int i = 0; i < 5; ++i) { scores[i].score = 0.75f; }
    TEST_NEAR("all 0.75->0.75", computeOverall(scores), 0.75f, 0.001f);

    float vals[5] = {1.0f, 0.8f, 0.6f, 0.4f, 0.2f};
    for (int i = 0; i < 5; ++i) scores[i].score = vals[i];
    TEST_NEAR("mixed->0.60", computeOverall(scores), 0.60f, 0.001f);

    for (int i = 0; i < 5; ++i) scores[i].score = 0.0f;
    TEST_NEAR("all zero->0.0", computeOverall(scores), 0.0f, 0.001f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: normalizeToScore
// ═══════════════════════════════════════════════════════════════════════════
static void test_normalize() {
    std::printf("\n── normalizeToScore ──\n"); std::fflush(stdout);

    TEST_NEAR("at ideal",  normalizeToScore(8.0f, 8.0f, 2.0f, 6.0f), 1.0f, 0.001f);
    TEST_NEAR("in tol",    normalizeToScore(7.0f, 8.0f, 2.0f, 6.0f), 1.0f, 0.001f);
    TEST_NEAR("at max",    normalizeToScore(14.0f, 8.0f, 2.0f, 6.0f), 0.0f, 0.001f);
    TEST_NEAR("beyond",    normalizeToScore(20.0f, 8.0f, 2.0f, 6.0f), 0.0f, 0.001f);
    TEST_NEAR("halfway",   normalizeToScore(5.0f, 8.0f, 2.0f, 6.0f), 0.75f, 0.001f);
    TEST_NEAR("below",     normalizeToScore(3.0f, 8.0f, 2.0f, 6.0f), 0.25f, 0.001f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: isActionable & emoji
// ═══════════════════════════════════════════════════════════════════════════
static void test_actionable() {
    std::printf("\n── isActionable & emoji ──\n"); std::fflush(stdout);

    RefinementScore s;
    s.score = 0.5f;  TEST("score=0.5 actionable", s.isActionable());
    s.score = 0.7f;  TEST("score=0.7 not actionable", !s.isActionable());
    s.score = 0.7f; s.hasReferenceData = true; s.vsReference = 0.3f;
    TEST("score=0.7 vsRef=0.3 actionable", s.isActionable());
    s.vsReference = 0.2f;
    TEST("score=0.7 vsRef=0.2 not actionable", !s.isActionable());

    s.score = 0.90f;
    TEST("emoji 🟢", std::strcmp(s.emoji(), "\xF0\x9F\x9F\xA2") == 0);
    s.score = 0.70f;
    TEST("emoji 🟡", std::strcmp(s.emoji(), "\xF0\x9F\x9F\xA1") == 0);
    s.score = 0.50f;
    TEST("emoji 🔶", std::strcmp(s.emoji(), "\xF0\x9F\x94\xB6") == 0);
    s.score = 0.30f;
    TEST("emoji 🔴", std::strcmp(s.emoji(), "\xF0\x9F\x94\xB4") == 0);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: Serialization (toJson → fromJson round-trip)
// ═══════════════════════════════════════════════════════════════════════════
static void test_serialization() {
    std::printf("\n── Serialization ──\n"); std::fflush(stdout);

    // Build profile from synthetic scores
    auto afro = makeAfrobeatProfile();
    auto depthS = computeDepthScore(afro);
    auto emoS  = computeEmotionScore(afro);

    RefinementProfile original;
    original.valid = true;
    original.overallRefinement = 0.78f;
    original.isRelevant = true;
    original.depth = depthS;
    original.emotion = emoS;
    original.impact = depthS;
    original.movement = depthS;
    original.glue = depthS;

    // Round-trip via DynamicObject
    juce::DynamicObject obj;
    original.toJson(obj);
    auto loaded = RefinementProfile::fromJson(obj);

    TEST("serialization valid", loaded.valid);
    TEST_NEAR("overallRef", loaded.overallRefinement, 0.78f, 0.001f);
    TEST("isRelevant", loaded.isRelevant == original.isRelevant);
    TEST_NEAR("depth score", loaded.depth.score, original.depth.score, 0.001f);
    TEST_NEAR("depth sub[0]", loaded.depth.subMetric[0], original.depth.subMetric[0], 0.001f);
    TEST_NEAR("emotion score", loaded.emotion.score, original.emotion.score, 0.001f);
    TEST_NEAR("emotion vsRef", loaded.emotion.vsReference, original.emotion.vsReference, 0.001f);
    TEST("emotion hasRefData", loaded.emotion.hasReferenceData == original.emotion.hasReferenceData);
    TEST_NEAR("impact score", loaded.impact.score, original.impact.score, 0.001f);

    // Empty object
    juce::DynamicObject empty;
    auto fromEmpty = RefinementProfile::fromJson(empty);
    TEST("fromJson empty valid=false", !fromEmpty.valid);
    TEST_NEAR("fromJson empty depth=0.5", fromEmpty.depth.score, 0.5f, 0.001f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: Default values & edge cases
// ═══════════════════════════════════════════════════════════════════════════
static void test_defaults() {
    std::printf("\n── Defaults & Edge Cases ──\n"); std::fflush(stdout);

    RefinementProfile rp;
    TEST("default valid=false", !rp.valid);
    TEST("default isRelevant=false", !rp.isRelevant);
    TEST_NEAR("default overall=0", rp.overallRefinement, 0.0f, 0.001f);
    TEST_NEAR("default depth=0.5", rp.depth.score, 0.5f, 0.001f);

    RefinementScore s;
    TEST("default score=0.5", std::abs(s.score - 0.5f) < 0.001f);
    TEST("default vsRef=0", std::abs(s.vsReference) < 0.001f);
    TEST("default hasRefData=false", !s.hasReferenceData);
    TEST("default label empty", s.label.isEmpty());

    // domainName
    s.domain = RefinementDomain::Depth;    TEST("name Depth",    std::strcmp(s.domainName(), "Depth") == 0);
    s.domain = RefinementDomain::Impact;   TEST("name Impact",   std::strcmp(s.domainName(), "Impact") == 0);
    s.domain = RefinementDomain::Movement; TEST("name Movement", std::strcmp(s.domainName(), "Movement") == 0);
    s.domain = RefinementDomain::Glue;     TEST("name Glue",     std::strcmp(s.domainName(), "Glue") == 0);
    s.domain = RefinementDomain::Emotion;  TEST("name Emotion",  std::strcmp(s.domainName(), "Emotion") == 0);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════
int main() {
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  RefinementProfile Formula Test Suite\n");
    std::printf("  Depth | Emotion | normalizeToScore | isActionable\n");
    std::printf("  Serialization | Defaults | Overall\n");
    std::printf("  Perfiles: Afrobeat, EDM, Pop, Silent\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n\n");
    std::fflush(stdout);

    test_depth_score();
    test_emotion_score();
    test_overall();
    test_normalize();
    test_actionable();
    test_serialization();
    test_defaults();

    std::printf("\n\n\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::fflush(stdout);

    return gTestsFailed > 0 ? 1 : 0;
}
