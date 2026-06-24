#include <cstdio>
#include <cstring>
#include <string>
#include <cmath>

// ─── Standalone test: include the engine directly ────────────────────────────
#include "../Source/MixCoach/engine/TrackRole.h"
#include "../Source/MixCoach/engine/MixPriorityEngine.h"

// ─── Simple test framework ──────────────────────────────────────────────────
static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST(name, expr)                                                          \
    do {                                                                          \
        if (!(expr)) {                                                            \
            std::printf("  FAIL  %s (%s:%d)\n", name, __FILE__, __LINE__);        \
            ++g_testsFailed;                                                      \
        } else {                                                                  \
            std::printf("  PASS  %s\n", name);                                    \
            ++g_testsPassed;                                                      \
        }                                                                         \
    } while (false)

using namespace mixcoach;

// ═══════════════════════════════════════════════════════════════════════════════
//  Tests
// ═══════════════════════════════════════════════════════════════════════════════

static void testRoleWeights()
{
    std::printf("\n── Role Weights ──\n");

    // Critical roles (10)
    TEST("VozPrincipal weight = 10",
         MixPriorityEngine::getRoleWeight(TrackRole::VozPrincipal) == 10);

    // Foundation roles (9)
    TEST("Kick weight = 9",
         MixPriorityEngine::getRoleWeight(TrackRole::Kick) == 9);
    TEST("BassSub weight = 9",
         MixPriorityEngine::getRoleWeight(TrackRole::BassSub) == 9);
    TEST("Snare weight = 9",
         MixPriorityEngine::getRoleWeight(TrackRole::Snare) == 9);

    // Lead roles (8)
    TEST("SynthLead weight = 8",
         MixPriorityEngine::getRoleWeight(TrackRole::SynthLead) == 8);
    TEST("GuitarLead weight = 8",
         MixPriorityEngine::getRoleWeight(TrackRole::GuitarLead) == 8);

    // Texture roles (6)
    TEST("HiHat weight = 6",
         MixPriorityEngine::getRoleWeight(TrackRole::HiHat) == 6);
    TEST("SynthPad weight = 6",
         MixPriorityEngine::getRoleWeight(TrackRole::SynthPad) == 6);

    // Percussion roles (5)
    TEST("Percussion weight = 5",
         MixPriorityEngine::getRoleWeight(TrackRole::Percussion) == 5);

    // FX roles (3)
    TEST("FxRiser weight = 3",
         MixPriorityEngine::getRoleWeight(TrackRole::FxRiser) == 3);
    TEST("FxAmbience weight = 3",
         MixPriorityEngine::getRoleWeight(TrackRole::FxAmbience) == 3);

    // Unknown (1)
    TEST("Unknown weight = 1",
         MixPriorityEngine::getRoleWeight(TrackRole::Unknown) == 1);
    TEST("Master weight = 1",
         MixPriorityEngine::getRoleWeight(TrackRole::Master) == 1);

    // All weights in range 1-10
    TEST("Kick weight in range",
         MixPriorityEngine::getRoleWeight(TrackRole::Kick) >= 1
         && MixPriorityEngine::getRoleWeight(TrackRole::Kick) <= 10);
    TEST("HiHat weight in range",
         MixPriorityEngine::getRoleWeight(TrackRole::HiHat) >= 1
         && MixPriorityEngine::getRoleWeight(TrackRole::HiHat) <= 10);
}

static void testWeightLabels()
{
    std::printf("\n── Weight Labels ──\n");

    TEST("Weight 10 = CRITICAL",
         std::strcmp(MixPriorityEngine::getRoleWeightLabel(10), "CRITICAL") == 0);
    TEST("Weight 9 = CRITICAL",
         std::strcmp(MixPriorityEngine::getRoleWeightLabel(9), "CRITICAL") == 0);
    TEST("Weight 7 = HIGH",
         std::strcmp(MixPriorityEngine::getRoleWeightLabel(7), "HIGH") == 0);
    TEST("Weight 5 = MEDIUM",
         std::strcmp(MixPriorityEngine::getRoleWeightLabel(5), "MEDIUM") == 0);
    TEST("Weight 3 = LOW",
         std::strcmp(MixPriorityEngine::getRoleWeightLabel(3), "LOW") == 0);
    TEST("Weight 1 = MINIMAL",
         std::strcmp(MixPriorityEngine::getRoleWeightLabel(1), "MINIMAL") == 0);
}

static void testDomainWeights()
{
    std::printf("\n── Domain Weights ──\n");

    TEST("Gain domain = 1.20",
         std::abs(DomainWeight::forDomain("gain") - 1.20f) < 0.001f);
    TEST("Dynamics domain = 1.00",
         std::abs(DomainWeight::forDomain("dynamics") - 1.00f) < 0.001f);
    TEST("Tonal domain = 0.90",
         std::abs(DomainWeight::forDomain("tonal") - 0.90f) < 0.001f);
    TEST("Spatial domain = 0.80",
         std::abs(DomainWeight::forDomain("spatial") - 0.80f) < 0.001f);
    TEST("Masking domain = 0.85",
         std::abs(DomainWeight::forDomain("masking") - 0.85f) < 0.001f);
    TEST("Unknown domain = 1.00",
         std::abs(DomainWeight::forDomain("unknown") - 1.00f) < 0.001f);
}

static void testGenreModifiers()
{
    std::printf("\n── Genre Modifiers ──\n");

    // Reggaeton
    TEST("Reggaeton gain = 1.10",
         std::abs(MixPriorityEngine::getGenreModifier("reggaeton", "gain") - 1.10f) < 0.001f);
    TEST("Reggaeton dynamics = 1.15",
         std::abs(MixPriorityEngine::getGenreModifier("reggaeton", "dynamics") - 1.15f) < 0.001f);
    TEST("Reggaeton tonal = 1.05",
         std::abs(MixPriorityEngine::getGenreModifier("reggaeton", "tonal") - 1.05f) < 0.001f);

    // Trap / Hip-Hop
    TEST("Trap dynamics = 1.20",
         std::abs(MixPriorityEngine::getGenreModifier("trap", "dynamics") - 1.20f) < 0.001f);
    TEST("Trap gain = 1.15",
         std::abs(MixPriorityEngine::getGenreModifier("trap", "gain") - 1.15f) < 0.001f);

    // Rock
    TEST("Rock tonal = 1.15",
         std::abs(MixPriorityEngine::getGenreModifier("rock", "tonal") - 1.15f) < 0.001f);
    TEST("Rock dynamics = 1.10",
         std::abs(MixPriorityEngine::getGenreModifier("rock", "dynamics") - 1.10f) < 0.001f);

    // Pop
    TEST("Pop tonal = 1.10",
         std::abs(MixPriorityEngine::getGenreModifier("pop", "tonal") - 1.10f) < 0.001f);

    // EDM
    TEST("EDM spatial = 1.10",
         std::abs(MixPriorityEngine::getGenreModifier("edm", "spatial") - 1.10f) < 0.001f);
    TEST("EDM gain = 1.10",
         std::abs(MixPriorityEngine::getGenreModifier("edm", "gain") - 1.10f) < 0.001f);

    // Jazz
    TEST("Jazz dynamics = 1.15",
         std::abs(MixPriorityEngine::getGenreModifier("jazz", "dynamics") - 1.15f) < 0.001f);
    TEST("Jazz tonal = 1.20",
         std::abs(MixPriorityEngine::getGenreModifier("jazz", "tonal") - 1.20f) < 0.001f);
    TEST("Jazz gain = 0.90",
         std::abs(MixPriorityEngine::getGenreModifier("jazz", "gain") - 0.90f) < 0.001f);

    // Afrobeat
    TEST("Afrobeat dynamics = 1.15",
         std::abs(MixPriorityEngine::getGenreModifier("afrobeat", "dynamics") - 1.15f) < 0.001f);

    // Unknown genre = default 1.0
    TEST("Unknown genre = 1.00",
         std::abs(MixPriorityEngine::getGenreModifier("unknown_genre", "gain") - 1.00f) < 0.001f);
}

static void testScoreComputation()
{
    std::printf("\n── Score Computation ──\n");

    // Basic score: severity 0.8, Kick (weight 9), gain domain (1.20), no genre
    {
        auto score = MixPriorityEngine::computeScore(
            0.8f, TrackRole::Kick, "gain", "CLIPPING", "Kick_01", "");

        float expected = 0.8f * (9.0f / 10.0f) * 1.20f * 1.00f; // 0.864
        TEST("Kick clipping score = 0.864",
             std::abs(score.finalScore - expected) < 0.001f);
        TEST("Kick role weight = 9", score.roleWeight == 9);
        TEST("Domain weight = 1.20",
             std::abs(score.domainWeight - 1.20f) < 0.001f);
        TEST("Genre modifier = 1.00",
             std::abs(score.genreModifier - 1.00f) < 0.001f);
    }

    // Vocal lead clipping: severity 1.0, VozPrincipal (weight 10), gain (1.20)
    {
        auto score = MixPriorityEngine::computeScore(
            1.0f, TrackRole::VozPrincipal, "gain", "CLIPPING", "Vocal", "");

        float expected = 1.0f * (10.0f / 10.0f) * 1.20f * 1.00f; // 1.20
        TEST("Vocal clipping score = 1.20",
             std::abs(score.finalScore - expected) < 0.001f);
        TEST("Vocal role weight = 10", score.roleWeight == 10);
    }

    // HiHat with low severity issue: severity 0.3, HiHat (weight 6), tonal (0.90)
    {
        auto score = MixPriorityEngine::computeScore(
            0.3f, TrackRole::HiHat, "tonal", "FALTA_PRESENCIA", "HH", "");

        float expected = 0.3f * (6.0f / 10.0f) * 0.90f * 1.00f; // 0.162
        TEST("HiHat tonal score = 0.162",
             std::abs(score.finalScore - expected) < 0.001f);
    }

    // FX Ambience: severity 0.5, FxAmbience (weight 3), spatial (0.80)
    {
        auto score = MixPriorityEngine::computeScore(
            0.5f, TrackRole::FxAmbience, "spatial", "BAJA_CORRELACION", "Ambience", "");

        float expected = 0.5f * (3.0f / 10.0f) * 0.80f * 1.00f; // 0.12
        TEST("FX Ambience spatial score = 0.12",
             std::abs(score.finalScore - expected) < 0.001f);
    }
}

static void testScoreWithGenreModifier()
{
    std::printf("\n── Score with Genre Modifier ──\n");

    // Kick in Reggaeton with gain issue
    {
        auto score = MixPriorityEngine::computeScore(
            0.8f, TrackRole::Kick, "gain", "CLIPPING", "Kick", "reggaeton");

        float expected = 0.8f * (9.0f / 10.0f) * 1.20f * 1.10f; // 0.9504
        TEST("Kick reggaeton score = 0.950",
             std::abs(score.finalScore - expected) < 0.001f);
    }

    // Bass in Trap with dynamics issue (heaviest modifier)
    {
        auto score = MixPriorityEngine::computeScore(
            0.7f, TrackRole::Bass808, "dynamics", "SOBRECOMPRIMIDO", "808", "trap");

        float expected = 0.7f * (9.0f / 10.0f) * 1.00f * 1.20f; // 0.756
        TEST("808 Trap dynamics score = 0.756",
             std::abs(score.finalScore - expected) < 0.001f);
    }

    // Guitar in Rock with tonal issue (rock guitar tonal is important)
    {
        auto score = MixPriorityEngine::computeScore(
            0.6f, TrackRole::GuitarLead, "tonal", "EXCESO_PRESENCIA", "Lead Gtr", "rock");

        float expected = 0.6f * (8.0f / 10.0f) * 0.90f * 1.15f; // 0.4968
        TEST("Guitar rock tonal score = 0.497",
             std::abs(score.finalScore - expected) < 0.002f);
    }
}

static void testSortByPriority()
{
    std::printf("\n── Sort by Priority ──\n");

    // Create 3 scores in arbitrary order
    std::vector<PriorityScore> scores;

    auto s1 = MixPriorityEngine::computeScore(
        0.8f, TrackRole::Kick, "gain", "CLIPPING", "Kick", ""); // 0.864
    s1.slotIndex = 0;
    scores.push_back(s1);

    auto s2 = MixPriorityEngine::computeScore(
        0.3f, TrackRole::VozPrincipal, "tonal", "FALTA_PRESENCIA", "Vocal", ""); // 0.3 * 1.0 * 0.9 = 0.27
    s2.slotIndex = 1;
    scores.push_back(s2);

    auto s3 = MixPriorityEngine::computeScore(
        0.5f, TrackRole::HiHat, "spatial", "BAJA_CORRELACION", "HH", ""); // 0.5 * 0.6 * 0.8 = 0.24
    s3.slotIndex = 2;
    scores.push_back(s3);

    MixPriorityEngine::sortByPriority(scores);

    TEST("First = Kick (highest score 0.864)", scores[0].slotIndex == 0);
    TEST("Second = Vocal (middle score 0.27)", scores[1].slotIndex == 1);
    TEST("Third = HiHat (lowest score 0.24)", scores[2].slotIndex == 2);
    TEST("Scores sorted descending",
         scores[0].finalScore >= scores[1].finalScore
         && scores[1].finalScore >= scores[2].finalScore);
}

static void testGetTopPriority()
{
    std::printf("\n── Get Top Priority ──\n");

    std::vector<PriorityScore> scores;

    for (int i = 0; i < 10; ++i) {
        auto s = MixPriorityEngine::computeScore(
            0.5f, TrackRole::Kick, "gain", "CLIPPING", "Track", "");
        s.slotIndex = i;
        s.finalScore = 1.0f - i * 0.05f; // decreasing scores 1.0, 0.95, ...
        scores.push_back(s);
    }

    auto top3 = MixPriorityEngine::getTopPriority(scores, 3);
    TEST("Top 3 has 3 elements", static_cast<int>(top3.size()) == 3);
    TEST("Top 3 sorted descending",
         top3[0].finalScore >= top3[1].finalScore
         && top3[1].finalScore >= top3[2].finalScore);

    auto top0 = MixPriorityEngine::getTopPriority(scores, 0);
    TEST("Top 0 is empty", top0.empty());

    auto topAll = MixPriorityEngine::getTopPriority(scores, 100);
    TEST("Top 100 capped to 10", static_cast<int>(topAll.size()) == 10);
}

static void testScoresToLLMContext()
{
    std::printf("\n── Scores to LLM Context ──\n");

    std::vector<PriorityScore> scores;
    auto s = MixPriorityEngine::computeScore(
        0.8f, TrackRole::Kick, "gain", "CLIPPING", "Kick_01", "reggaeton");
    s.slotIndex = 0;
    scores.push_back(s);

    juce::String context = MixPriorityEngine::scoresToLLMContext(scores);
    TEST("Context contains header", context.contains("PRIORITY ISSUES"));
    TEST("Context contains track name", context.contains("Kick_01"));
    TEST("Context contains domain", context.contains("gain"));
    TEST("Context contains issue type", context.contains("CLIPPING"));

    // Empty test
    juce::String emptyContext = MixPriorityEngine::scoresToLLMContext({});
    TEST("Empty context says no issues", emptyContext.contains("No priority issues"));
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════════

int main()
{
    std::printf("╔══════════════════════════════════════════════════════════╗\n");
    std::printf("║           MixPriorityEngine — Unit Tests                ║\n");
    std::printf("╚══════════════════════════════════════════════════════════╝\n");

    testRoleWeights();
    testWeightLabels();
    testDomainWeights();
    testGenreModifiers();
    testScoreComputation();
    testScoreWithGenreModifier();
    testSortByPriority();
    testGetTopPriority();
    testScoresToLLMContext();

    int total = g_testsPassed + g_testsFailed;
    std::printf("\n══════════════════════════════════════════════════════════\n");
    std::printf("  Results: %d/%d passed, %d failed\n", g_testsPassed, total, g_testsFailed);
    std::printf("══════════════════════════════════════════════════════════\n");

    return g_testsFailed > 0 ? 1 : 0;
}
