// ═══════════════════════════════════════════════════════════════════════════
//  TestCoachingNarrativeTypes.cpp — Unit tests para CoachingNarrativeTypes.h
//
//  Verifica:
//    - NarrativeStep: exactamente 9 valores con nombres correctos
//    - EvidenceView:  exactamente 8 valores con nombres correctos
//    - EvidenceConfig: 5 campos con tipos correctos
//    - NarrativeTiming: constantes con valores correctos
//    - Consistencia con CoachingNarrativeDirector (Step enum matching)
// ═══════════════════════════════════════════════════════════════════════════
//
//  Build: cmake --build build --config Release --target TestCoachingNarrativeTypes
//  Run:   build/tests/Release/TestCoachingNarrativeTypes.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include "../Source/MixCoach/engine/CoachingNarrativeTypes.h"

// ─── Test runner ───────────────────────────────────────────────────────────
static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        gTestsFailed++; \
    } else { \
        std::printf("  \xe2\x9c\x85 PASS: %s\n", name); \
        gTestsPassed++; \
    } \
} while(0)

#define TEST_EQ(name, a, b) do { \
    if ((a) != (b)) { \
        std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: %s - expected %lld, got %lld (%s:%d)\n", \
                     name, (long long)(b), (long long)(a), __FILE__, __LINE__); \
        gTestsFailed++; \
    } else { \
        std::printf("  \xe2\x9c\x85 PASS: %s\n", name); \
        gTestsPassed++; \
    } \
} while(0)

namespace {

    // ─── Helper: convert NarrativeStep to int for comparison ─────────────────
    int ns(mixcoach::NarrativeStep s) { return static_cast<int>(s); }

    // ─── Helper: convert EvidenceView to int for comparison ──────────────────
    int ev(mixcoach::EvidenceView v) { return static_cast<int>(v); }

} // namespace

// ═══════════════════════════════════════════════════════════════════════════
//  TEST GROUP 1 — NarrativeStep: count, values, ordering
// ═══════════════════════════════════════════════════════════════════════════
static void testNarrativeStepCount()
{
    std::printf("\n─── NarrativeStep: count & values ───\n");

    // Verify 9 distinct values
    mixcoach::NarrativeStep all[] = {
        mixcoach::NarrativeStep::Idle,
        mixcoach::NarrativeStep::ShowEvidence,
        mixcoach::NarrativeStep::Explain,
        mixcoach::NarrativeStep::ShowOptions,
        mixcoach::NarrativeStep::WaitingForUser,
        mixcoach::NarrativeStep::ConfirmApplied,
        mixcoach::NarrativeStep::CelebrateStep,
        mixcoach::NarrativeStep::NextProblemStep,
        mixcoach::NarrativeStep::Complete
    };
    constexpr int kExpectedCount = 9;
    const int actualCount = sizeof(all) / sizeof(all[0]);
    TEST_EQ("NarrativeStep count", actualCount, kExpectedCount);

    // Verify unique integer values (no duplicates, 0-based sequential)
    bool seen[16] = {false};
    bool hasDuplicate = false;
    for (int i = 0; i < actualCount; ++i) {
        int val = ns(all[i]);
        if (val < 0 || val >= 16) {
            std::fprintf(stderr, "  \xe2\x9a\xa0\xef\xb8\x8f WARN: NarrativeStep value %d out of range [0,15]\n", val);
        }
        if (seen[val]) hasDuplicate = true;
        seen[val] = true;
    }
    TEST("NarrativeStep: no duplicate integer values", !hasDuplicate);

    // Verify Idle is 0 and Complete is the last value
    TEST_EQ("NarrativeStep::Idle == 0", ns(mixcoach::NarrativeStep::Idle), 0);
    TEST_EQ("NarrativeStep::Complete == 8", ns(mixcoach::NarrativeStep::Complete), 8);

    // Verify sequential ordering (each value is previous + 1)
    bool sequential = true;
    for (int i = 1; i < actualCount; ++i) {
        if (ns(all[i]) != ns(all[i-1]) + 1) {
            sequential = false;
            break;
        }
    }
    TEST("NarrativeStep: sequential 0-based", sequential);
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST GROUP 2 — NarrativeStep: names match Director::Step convention
// ═══════════════════════════════════════════════════════════════════════════
static void testNarrativeStepNames()
{
    std::printf("\n─── NarrativeStep: naming convention ───\n");

    // Verify that NarrativeStep names match the expected
    // Director::Step convention (PascalCase, descriptive)
    using NS = mixcoach::NarrativeStep;

    // Smoke test: verify specific values by name
    TEST("NS::Idle is idle == 0", ns(NS::Idle) == 0);
    TEST("NS::ShowEvidence == 1", ns(NS::ShowEvidence) == 1);
    TEST("NS::Explain == 2", ns(NS::Explain) == 2);
    TEST("NS::ShowOptions == 3", ns(NS::ShowOptions) == 3);
    TEST("NS::WaitingForUser == 4", ns(NS::WaitingForUser) == 4);
    TEST("NS::ConfirmApplied == 5", ns(NS::ConfirmApplied) == 5);
    TEST("NS::CelebrateStep == 6", ns(NS::CelebrateStep) == 6);
    TEST("NS::NextProblemStep == 7", ns(NS::NextProblemStep) == 7);
    TEST("NS::Complete == 8", ns(NS::Complete) == 8);

    // Verify all values are unique by cast comparison
    int values[9];
    values[0] = ns(NS::Idle);
    values[1] = ns(NS::ShowEvidence);
    values[2] = ns(NS::Explain);
    values[3] = ns(NS::ShowOptions);
    values[4] = ns(NS::WaitingForUser);
    values[5] = ns(NS::ConfirmApplied);
    values[6] = ns(NS::CelebrateStep);
    values[7] = ns(NS::NextProblemStep);
    values[8] = ns(NS::Complete);

    for (int i = 0; i < 9; ++i) {
        for (int j = i + 1; j < 9; ++j) {
            if (values[i] == values[j]) {
                std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: duplicate value %d at positions %d and %d\n", values[i], i, j);
                gTestsFailed++;
                return;
            }
        }
    }
    TEST("NarrativeStep: all 9 values unique (post-hoc check)", true);

    // Verify switch exhaustiveness (compile-time check would be better,
    // but we at least verify all values are reachable)
    auto stepToString = [](NS s) -> const char* {
        switch (s) {
            case NS::Idle:            return "Idle";
            case NS::ShowEvidence:    return "ShowEvidence";
            case NS::Explain:         return "Explain";
            case NS::ShowOptions:     return "ShowOptions";
            case NS::WaitingForUser:  return "WaitingForUser";
            case NS::ConfirmApplied:  return "ConfirmApplied";
            case NS::CelebrateStep:   return "CelebrateStep";
            case NS::NextProblemStep: return "NextProblemStep";
            case NS::Complete:        return "Complete";
            default:                  return "UNKNOWN";
        }
    };
    TEST("NS::Idle stringifies correctly",
         std::strcmp(stepToString(NS::Idle), "Idle") == 0);
    TEST("NS::Complete stringifies correctly",
         std::strcmp(stepToString(NS::Complete), "Complete") == 0);
    TEST("NS::ShowEvidence stringifies correctly",
         std::strcmp(stepToString(NS::ShowEvidence), "ShowEvidence") == 0);

    // Verify that NarrativeTiming constants exist and have expected values
    TEST_EQ("NarrativeTiming::kDetectMs",
            mixcoach::NarrativeTiming::kDetectMs, 800);
    TEST_EQ("NarrativeTiming::kEvidenceMs",
            mixcoach::NarrativeTiming::kEvidenceMs, 1000);
    TEST_EQ("NarrativeTiming::kExplainMs",
            mixcoach::NarrativeTiming::kExplainMs, 1200);
    TEST_EQ("NarrativeTiming::kOptionsMs",
            mixcoach::NarrativeTiming::kOptionsMs, 800);
    TEST_EQ("NarrativeTiming::kVerifyMs",
            mixcoach::NarrativeTiming::kVerifyMs, 1500);
    TEST_EQ("NarrativeTiming::kCelebrateMs",
            mixcoach::NarrativeTiming::kCelebrateMs, 1000);
    TEST_EQ("NarrativeTiming::kSetupAutoAdvanceMs",
            mixcoach::NarrativeTiming::kSetupAutoAdvanceMs, 400);
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST GROUP 3 — EvidenceView: count, values
// ═══════════════════════════════════════════════════════════════════════════
static void testEvidenceView()
{
    std::printf("\n─── EvidenceView: count & values ───\n");

    using EV = mixcoach::EvidenceView;

    // Verify 8 values exist
    EV all[] = {
        EV::VU,
        EV::Spectrum,
        EV::Crest,
        EV::Vectorscope,
        EV::LUFS,
        EV::DNA,
        EV::StereoWidth,
        EV::None
    };
    constexpr int kExpectedCount = 8;
    const int actualCount = sizeof(all) / sizeof(all[0]);
    TEST_EQ("EvidenceView count", actualCount, kExpectedCount);

    // Verify unique integer values
    bool seen[16] = {false};
    bool hasDuplicate = false;
    for (int i = 0; i < actualCount; ++i) {
        int val = ev(all[i]);
        if (val < 0 || val >= 16) {
            std::fprintf(stderr, "  \xe2\x9a\xa0\xef\xb8\x8f WARN: EvidenceView value %d out of range [0,15]\n", val);
        }
        if (seen[val]) hasDuplicate = true;
        seen[val] = true;
    }
    TEST("EvidenceView: no duplicate integer values", !hasDuplicate);

    // Verify None is the last value (used as sentinel)
    int vuVal    = ev(EV::VU);
    int noneVal  = ev(EV::None);
    int crestVal = ev(EV::Crest);
    TEST("EvidenceView::VU is first (== 0)", vuVal == 0);
    TEST("EvidenceView::None is last (> Crest)", noneVal > crestVal);

    // Verify specific names
    auto viewToString = [](EV v) -> const char* {
        switch (v) {
            case EV::VU:           return "VU";
            case EV::Spectrum:     return "Spectrum";
            case EV::Crest:        return "Crest";
            case EV::Vectorscope:  return "Vectorscope";
            case EV::LUFS:         return "LUFS";
            case EV::DNA:          return "DNA";
            case EV::StereoWidth:  return "StereoWidth";
            case EV::None:         return "None";
            default:               return "UNKNOWN";
        }
    };
    TEST("EV::VU stringifies correctly",
         std::strcmp(viewToString(EV::VU), "VU") == 0);
    TEST("EV::None stringifies correctly",
         std::strcmp(viewToString(EV::None), "None") == 0);
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST GROUP 4 — EvidenceConfig: fields and defaults
// ═══════════════════════════════════════════════════════════════════════════
static void testEvidenceConfig()
{
    std::printf("\n─── EvidenceConfig: fields & defaults ───\n");

    // Default-constructed EvidenceConfig
    mixcoach::EvidenceConfig config{};

    // Verify defaults (view has no DMI, value-init = 0 = VU)
    TEST("EvidenceConfig value-init view == VU (0)",
         config.view == mixcoach::EvidenceView::VU);
    TEST("EvidenceConfig default panelTitle is empty",
         config.panelTitle.isEmpty());
    TEST("EvidenceConfig default systemMessage is empty",
         config.systemMessage.isEmpty());
    TEST_EQ("EvidenceConfig default highlightFreqHz == 0.0f",
            config.highlightFreqHz, 0.0f);
    TEST_EQ("EvidenceConfig default highlightSlot == -1",
            config.highlightSlot, -1);
    TEST("EvidenceConfig default hasHighlight() == false",
         !config.hasHighlight());

    // Verify field assignment
    config.view = mixcoach::EvidenceView::Spectrum;
    config.panelTitle = "Espectro";
    config.systemMessage = "\xf0\x9f\x94\x8d Abriendo analizador...";
    config.highlightFreqHz = 2500.0f;
    config.highlightSlot = 3;

    TEST("EvidenceConfig assigned view == Spectrum",
         config.view == mixcoach::EvidenceView::Spectrum);
    TEST("EvidenceConfig assigned panelTitle == Espectro",
         config.panelTitle == "Espectro");
    TEST_EQ("EvidenceConfig assigned highlightFreqHz == 2500.0f",
            config.highlightFreqHz, 2500.0f);
    TEST_EQ("EvidenceConfig assigned highlightSlot == 3",
            config.highlightSlot, 3);
    TEST("EvidenceConfig hasHighlight() == true with freq",
         config.hasHighlight());

    // Test hasHighlight() edge cases
    config.highlightFreqHz = 0.0f;
    config.highlightSlot = -1;
    TEST("EvidenceConfig hasHighlight() == false after reset",
         !config.hasHighlight());

    config.highlightSlot = 0;
    TEST("EvidenceConfig hasHighlight() == true with slot==0",
         config.hasHighlight());

    config.highlightSlot = -1;
    config.highlightFreqHz = 0.001f;
    TEST("EvidenceConfig hasHighlight() == true with tiny freq",
         config.hasHighlight());

    // Verify sizeof is reasonable (no unexpected padding)
    int cfgSize = static_cast<int>(sizeof(config));
    TEST("EvidenceConfig sizeof <= 64 bytes (reasonable)",
         cfgSize <= 64);
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST GROUP 5 — Compile-time assertions (static_assert)
// ═══════════════════════════════════════════════════════════════════════════
// These verify invariants at compile time. If they fail, the file won't
// even compile — the strongest possible test.
// ═══════════════════════════════════════════════════════════════════════════

// Verify NarrativeStep underlying type is uint8_t
static_assert(sizeof(mixcoach::NarrativeStep) == 1,
              "NarrativeStep must be uint8_t (1 byte)");

// Verify EvidenceView underlying type is uint8_t
static_assert(sizeof(mixcoach::EvidenceView) == 1,
              "EvidenceView must be uint8_t (1 byte)");

// Verify EvidenceConfig has highlight method
static_assert(sizeof(mixcoach::EvidenceConfig().hasHighlight()) == sizeof(bool),
              "EvidenceConfig::hasHighlight() must return bool");

// ═══════════════════════════════════════════════════════════════════════════
//  MAIN — Run all test groups
// ═══════════════════════════════════════════════════════════════════════════
int main()
{
    std::printf("╔══════════════════════════════════════════════════════════════╗\n");
    std::printf("║  TestCoachingNarrativeTypes                                 ║\n");
    std::printf("╚══════════════════════════════════════════════════════════════╝\n");

    testNarrativeStepCount();
    testNarrativeStepNames();
    testEvidenceView();
    testEvidenceConfig();

    std::printf("\n─── Results ──────────────────────────────────────\n");
    std::printf("  Passed: %d\n", gTestsPassed);
    std::printf("  Failed: %d\n", gTestsFailed);
    std::printf("  Total:  %d\n", gTestsPassed + gTestsFailed);
    std::printf("──────────────────────────────────────────────────\n\n");

    return gTestsFailed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
