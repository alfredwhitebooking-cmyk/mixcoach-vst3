// ═══════════════════════════════════════════════════════════════════════════
//  TestSemanticComparator.cpp — Unit tests for SemanticComparator
//  Verifica que el análisis semántico detecte correctamente problemas
//  como clipping en pistas según su rol (TrackRole).
//
//  Compilado via CMake (ver CMakeLists.txt):
//    cmake --build build --config Release --target TestSemanticComparator
//    ./build/tests/Release/TestSemanticComparator.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "MixCoach/engine/TrackRole.h"
#include "MixCoach/engine/SpectralProfiler.h"
#include "MixCoach/engine/SemanticComparator.h"

using namespace mixcoach;

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

// ─── Helper: crea un TrackSpectralProfile básico para un Kick ──────────────
static TrackSpectralProfile makeKickProfile(float peakDb, float rmsDb)
{
    TrackSpectralProfile p;
    p.peakDb  = peakDb;
    p.rmsDb   = rmsDb;
    p.crestDb = peakDb - rmsDb;
    // Kick típico: energía concentrada en Sub + Bass bands
    p.crestPerBand[0] = 10.0f;   // Sub  (94Hz)  — dominante
    p.crestPerBand[1] = 8.0f;    // Bass (281Hz) — presente
    p.crestPerBand[2] = 3.0f;    // LoMid
    p.crestPerBand[3] = 1.0f;    // HiMid
    p.crestPerBand[4] = 0.5f;    // Pres
    p.crestPerBand[5] = 0.0f;    // Air
    // Stereo width (Kick debe ser casi mono)
    for (int b = 0; b < 6; ++b)
        p.stereoWidthPerBand[b] = 0.1f;
    p.avgStereoWidth = 0.1f;
    p.isMonoCompatible = true;
    p.transientRatio = 2.5f;     // Kick tiene ataque marcado
    p.spectralCentroid = 150.0f;
    p.fundamentalEstimate = 60.0f;
    p.hasTransientCharacter = true;
    p.isBassHeavy = true;
    return p;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 1: Kick con clipping → SemanticIssue::Critical
// ═══════════════════════════════════════════════════════════════════════════
static void test_kick_clipping_gets_critical()
{
    std::printf("\n── Test 1: Kick Clipping → Critical ──\n");
    std::fflush(stdout);

    // Simular un Kick con clipping (peakDb > -0.5f)
    auto profile = makeKickProfile(-0.1f, -10.0f);

    auto diff = SemanticComparator::compareTrack(0, "Kick Test", TrackRole::Kick, profile);

    // Debe tener al menos un issue
    TEST("compareTrack returned issues",
         !diff.issues.empty());

    // Buscar un issue de severidad Critical
    bool foundCritical = false;
    for (const auto& issue : diff.issues)
    {
        if (issue.severity == SemanticIssue::Severity::Critical)
        {
            foundCritical = true;

            TEST("Critical issue is in Gain domain",
                 issue.domain == SemanticIssue::Domain::Gain);

            TEST("Critical issue message mentions CLIPPING or clipping",
                 issue.message.contains("CLIPPING") || issue.message.contains("clipping"));

            TEST("Critical issue has a non-empty recommendation",
                 issue.recommendation.isNotEmpty());

            TEST("Critical issue actualValue is close to -0.1 dB",
                 std::abs(issue.actualValue - (-0.1f)) < 0.01f);

            TEST("Critical issue has expectedValue = Kick's peakTargetDb (-6.0)",
                 std::abs(issue.expectedValue - (-6.0f)) < 0.01f);

            TEST("Critical issue deviation is positive (actual > expected)",
                 issue.deviation > 0.0f);

            break;
        }
    }

    TEST("Found at least one SemanticIssue::Critical", foundCritical);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 2: Kick con nivel óptimo → SIN Critical
// ═══════════════════════════════════════════════════════════════════════════
static void test_kick_good_levels_no_critical()
{
    std::printf("\n── Test 2: Good Kick → No Critical ──\n");
    std::fflush(stdout);

    // Kick con peak exactamente en target (dev = 0.0, abs = 0.0 < tol*0.5 = 2.0)
    // crest = -6.0 - (-18.0) = 12.0 (dentro del rango 8-20 para Kick)
    auto profile = makeKickProfile(-6.0f, -18.0f);

    auto diff = SemanticComparator::compareTrack(0, "Good Kick", TrackRole::Kick, profile);

    // Verificar que NO haya issues Critical
    bool hasCritical = false;
    for (const auto& issue : diff.issues)
    {
        if (issue.severity == SemanticIssue::Severity::Critical)
        {
            hasCritical = true;
            break;
        }
    }
    TEST("No Critical issues for optimal gain levels", !hasCritical);

    // Debería tener al menos un Praise (nivel óptimo)
    TEST("Has Praise for optimal gain",
         !diff.praises.empty());

    // Verificar que el Praise sea de Gain domain
    bool foundGainPraise = false;
    for (const auto& praise : diff.praises)
    {
        if (praise.domain == SemanticIssue::Domain::Gain)
        {
            foundGainPraise = true;
            TEST("Gain Praise mentions 'optimo' or 'optimo'",
                 praise.message.contains("optimo") || praise.message.contains("ptimo"));
            break;
        }
    }
    TEST("Found Gain Praise for optimal level", foundGainPraise);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 3: Kick sin datos → Sin issues (hasData() = false)
// ═══════════════════════════════════════════════════════════════════════════
static void test_kick_no_data_returns_empty()
{
    std::printf("\n── Test 3: No Data → Empty Diff ──\n");
    std::fflush(stdout);

    TrackSpectralProfile emptyProfile; // peakDb = -100.0f → hasData() = false
    auto diff = SemanticComparator::compareTrack(0, "Empty", TrackRole::Kick, emptyProfile);

    TEST("No issues for empty profile", diff.issues.empty());
    TEST("No praises for empty profile", diff.praises.empty());
    TEST("hasProblems returns false for empty", !diff.hasProblems());
    TEST("problemCount is 0 for empty", diff.problemCount() == 0);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 4: Rol Unknown con clipping → Critical (rango genérico)
// ═══════════════════════════════════════════════════════════════════════════
static void test_unknown_role_clipping()
{
    std::printf("\n── Test 4: Unknown Role + Clipping → Critical ──\n");
    std::fflush(stdout);

    auto profile = makeKickProfile(-0.3f, -12.0f); // clipping
    auto diff = SemanticComparator::compareTrack(1, "Unknown Clip", TrackRole::Unknown, profile);

    bool foundCritical = false;
    for (const auto& issue : diff.issues)
    {
        if (issue.severity == SemanticIssue::Severity::Critical)
        {
            foundCritical = true;
            TEST("Unknown role Critical issue mentions CLIPPING",
                 issue.message.contains("CLIPPING") || issue.message.contains("clipping"));
            break;
        }
    }
    TEST("Unknown role with clipping gets Critical", foundCritical);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 5: toTextSummary incluye mensaje de clipping
// ═══════════════════════════════════════════════════════════════════════════
static void test_text_summary_includes_clipping()
{
    std::printf("\n── Test 5: toTextSummary → Clipping Mention ──\n");
    std::fflush(stdout);

    auto profile = makeKickProfile(-0.1f, -10.0f);
    auto diff = SemanticComparator::compareTrack(2, "ClipSummary", TrackRole::Kick, profile);

    auto summary = diff.toTextSummary();
    TEST("toTextSummary is not empty", summary.isNotEmpty());
    TEST("toTextSummary contains 'Kick' role name", summary.contains("Kick"));
    TEST("toTextSummary contains track name 'ClipSummary'", summary.contains("ClipSummary"));
    TEST("toTextSummary mentions CLIPPING or clipping",
         summary.contains("CLIPPING") || summary.contains("clipping") ||
         summary.contains("reduce"));
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════
int main()
{
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  SemanticComparator Unit Tests\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n\n");
    std::fflush(stdout);

    test_kick_clipping_gets_critical();
    test_kick_good_levels_no_critical();
    test_kick_no_data_returns_empty();
    test_unknown_role_clipping();
    test_text_summary_includes_clipping();

    std::printf("\n\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::fflush(stdout);

    return gTestsFailed > 0 ? 1 : 0;
}
