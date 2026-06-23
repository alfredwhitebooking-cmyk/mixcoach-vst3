// ═══════════════════════════════════════════════════════════════════════════
//  TestExperienceLevel.cpp — Unit tests for AiCoachAdapter::ExperienceLevel
//
//  Verifica que cada nivel de experiencia (Novice, Intermediate, Advanced,
//  Expert) genere el contenido correcto en buildSystemPrompt().
//
//  Compilado via CMake (ver CMakeLists.txt):
//    cmake --build build --config Release --target TestExperienceLevel
//    ./build/tests/Release/TestExperienceLevel.exe
//
//  Dependencias: SharedData (~65MB, heap), AudioAnalyzer, PhaseManager,
//                CoachEngine, AiCoachAdapter.
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <memory>
#include <algorithm>

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "Common/types/Types.h"
#include "Common/memory/SlotRegistry.h"
#include "Common/memory/SharedData.h"
#include "MixCoach/engine/PhaseManager.h"
#include "MixCoach/engine/CoachEngine.h"
#include "MixCoach/audio/AudioAnalyzer.h"
#include "MixCoach/ai/AiCoachAdapter.h"

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

// ─── RAII container: TODOS los objetos deben vivir más que AiCoachAdapter
//     porque AiCoachAdapter guarda REFERENCIAS a SharedData, AudioAnalyzer,
//     PhaseManager y CoachEngine.
struct TestHarness {
    // Heap allocation for SharedData (~65MB)
    std::unique_ptr<SharedData> sd;
    AudioAnalyzer  audioAnalyzer;
    PhaseManager   pm;
    CoachEngine    coachEngine;
    AiCoachAdapter adapter;

    TestHarness()
        : sd(std::make_unique<SharedData>())
        , audioAnalyzer()
        , pm(sd->getSlotRegistry())
        , coachEngine(pm, *sd, audioAnalyzer)
        , adapter(*sd, audioAnalyzer, coachEngine, pm)
    {
        adapter.setGenre("Rock");
    }
};

// ═══════════════════════════════════════════════════════════════════════════
//  Test 1: experienceLevelName — Static method, no deps needed
// ═══════════════════════════════════════════════════════════════════════════
static void test_experience_level_name()
{
    std::printf("\n── Test 1: experienceLevelName() ──\n");
    std::fflush(stdout);

    TEST("Novice name is 'Novice'",
         juce::String(AiCoachAdapter::experienceLevelName(
             AiCoachAdapter::ExperienceLevel::Novice)) == "Novice");

    TEST("Intermediate name is 'Intermediate'",
         juce::String(AiCoachAdapter::experienceLevelName(
             AiCoachAdapter::ExperienceLevel::Intermediate)) == "Intermediate");

    TEST("Advanced name is 'Advanced'",
         juce::String(AiCoachAdapter::experienceLevelName(
             AiCoachAdapter::ExperienceLevel::Advanced)) == "Advanced");

    TEST("Expert name is 'Expert'",
         juce::String(AiCoachAdapter::experienceLevelName(
             AiCoachAdapter::ExperienceLevel::Expert)) == "Expert");
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 2: Novice — Prompt debe contener lenguaje simple y alentador
// ═══════════════════════════════════════════════════════════════════════════
static void test_novice_prompt_content()
{
    std::printf("\n── Test 2: Novice Level Prompt ──\n");
    std::fflush(stdout);

    TestHarness h;
    h.adapter.setExperienceLevel(AiCoachAdapter::ExperienceLevel::Novice);
    auto prompt = h.adapter.buildSystemPrompt();

    TEST("Novice prompt contains 'USER EXPERIENCE LEVEL'",
         prompt.contains("USER EXPERIENCE LEVEL"));
    TEST("Novice prompt contains level name 'Novice'",
         prompt.contains("Novice"));
    TEST("Novice prompt encourages simple explanations",
         prompt.contains("Explain audio concepts simply") ||
         prompt.contains("simply"));
    TEST("Novice prompt mentions step-by-step",
         prompt.contains("step-by-step"));
    TEST("Novice prompt says 'Be encouraging'",
         prompt.contains("encouraging") || prompt.contains("Encouraging") ||
         prompt.contains("patient"));
    TEST("Novice prompt says 'Avoid jargon'",
         prompt.contains("Avoid jargon") || prompt.contains("jargon"));
    TEST("Novice prompt does NOT mention 'pre-ring'",
         !prompt.contains("pre-ring"));
    TEST("Novice prompt does NOT say 'no fluff'",
         !prompt.contains("no fluff"));
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 3: Intermediate — Prompt debe usar terminologia estandar
// ═══════════════════════════════════════════════════════════════════════════
static void test_intermediate_prompt_content()
{
    std::printf("\n── Test 3: Intermediate Level Prompt ──\n");
    std::fflush(stdout);

    TestHarness h;
    h.adapter.setExperienceLevel(AiCoachAdapter::ExperienceLevel::Intermediate);
    auto prompt = h.adapter.buildSystemPrompt();

    TEST("Intermediate prompt contains level name 'Intermediate'",
         prompt.contains("Intermediate"));
    TEST("Intermediate prompt mentions 'threshold'",
         prompt.contains("threshold"));
    TEST("Intermediate prompt mentions 'ratio'",
         prompt.contains("ratio"));
    TEST("Intermediate prompt mentions 'Q, attack'",
         prompt.contains("Q, attack"));
    TEST("Intermediate prompt does NOT say 'Avoid jargon'",
         !prompt.contains("Avoid jargon"));
    TEST("Intermediate prompt does NOT mention 'pre-ring'",
         !prompt.contains("pre-ring"));
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 4: Advanced — Prompt debe usar jerga tecnica
// ═══════════════════════════════════════════════════════════════════════════
static void test_advanced_prompt_content()
{
    std::printf("\n── Test 4: Advanced Level Prompt ──\n");
    std::fflush(stdout);

    TestHarness h;
    h.adapter.setExperienceLevel(AiCoachAdapter::ExperienceLevel::Advanced);
    auto prompt = h.adapter.buildSystemPrompt();

    TEST("Advanced prompt contains level name 'Advanced'",
         prompt.contains("Advanced"));
    TEST("Advanced prompt mentions 'pre-ring'",
         prompt.contains("pre-ring"));
    TEST("Advanced prompt mentions 'phase coherence'",
         prompt.contains("phase coherence"));
    TEST("Advanced prompt mentions 'transient shaping'",
         prompt.contains("transient shaping"));
    TEST("Advanced prompt says 'Skip basic explanations'",
         prompt.contains("Skip basic explanations"));
    TEST("Advanced prompt says 'Give exact numbers'",
         prompt.contains("exact numbers") ||
         prompt.contains("frequencies, ratios"));
    TEST("Advanced prompt does NOT say 'step-by-step'",
         !prompt.contains("step-by-step"));
    TEST("Advanced prompt does NOT say 'Avoid jargon'",
         !prompt.contains("Avoid jargon"));
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 5: Expert — Prompt debe ser directo, sin emojis, solo numeros
// ═══════════════════════════════════════════════════════════════════════════
static void test_expert_prompt_content()
{
    std::printf("\n── Test 5: Expert Level Prompt ──\n");
    std::fflush(stdout);

    TestHarness h;
    h.adapter.setExperienceLevel(AiCoachAdapter::ExperienceLevel::Expert);
    auto prompt = h.adapter.buildSystemPrompt();

    TEST("Expert prompt contains level name 'Expert'",
         prompt.contains("Expert"));
    TEST("Expert prompt says 'Be direct'",
         prompt.contains("Be direct"));
    TEST("Expert prompt says 'no fluff, no emojis'",
         prompt.contains("no fluff") && prompt.contains("no emojis"));
    TEST("Expert prompt says 'exact solution'",
         prompt.contains("exact solution") ||
         prompt.contains("exact numbers"));
    TEST("Expert prompt says 'Assume deep technical knowledge'",
         prompt.contains("deep technical knowledge") ||
         prompt.contains("technical knowledge"));
    TEST("Expert prompt does NOT contain 'step-by-step'",
         !prompt.contains("step-by-step"));
    TEST("Expert prompt does NOT contain 'Avoid jargon'",
         !prompt.contains("Avoid jargon"));
    TEST("Expert prompt does NOT contain 'encouraging and patient'",
         !prompt.contains("encouraging and patient"));
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 6: Todos los niveles incluyen Chain-of-Thought
// ═══════════════════════════════════════════════════════════════════════════
static void test_all_levels_include_chain_of_thought()
{
    std::printf("\n── Test 6: Chain-of-Thought in all levels ──\n");
    std::fflush(stdout);

    auto levels = {
        AiCoachAdapter::ExperienceLevel::Novice,
        AiCoachAdapter::ExperienceLevel::Intermediate,
        AiCoachAdapter::ExperienceLevel::Advanced,
        AiCoachAdapter::ExperienceLevel::Expert
    };

    for (auto level : levels)
    {
        TestHarness h;
        h.adapter.setExperienceLevel(level);
        auto prompt = h.adapter.buildSystemPrompt();
        juce::String levelName =
            AiCoachAdapter::experienceLevelName(level);

        TEST(("CoT present in " + levelName).toRawUTF8(),
             prompt.contains("Step 1") &&
             prompt.contains("REASONING PROTOCOL"));

        TEST(("CoT Step 2 in " + levelName).toRawUTF8(),
             prompt.contains("Step 2") &&
             (prompt.contains("PRIORITIZE") ||
              prompt.contains("Critical > Warning")));

        TEST(("CoT Step 7 in " + levelName).toRawUTF8(),
             prompt.contains("Step 7") &&
             (prompt.contains("OUTPUT") ||
              prompt.contains("do NOT dump everything")));
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 7: Default level is Intermediate
// ═══════════════════════════════════════════════════════════════════════════
static void test_default_level_is_intermediate()
{
    std::printf("\n── Test 7: Default Experience Level ──\n");
    std::fflush(stdout);

    TestHarness h;

    TEST("Default experience level is Intermediate",
         h.adapter.getExperienceLevel() ==
             AiCoachAdapter::ExperienceLevel::Intermediate);

    auto prompt = h.adapter.buildSystemPrompt();
    TEST("Default prompt contains 'Intermediate'",
         prompt.contains("Intermediate"));
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════
int main()
{
    std::printf("═══════════════════════════════════════════\n");
    std::printf("  ExperienceLevel Unit Tests\n");
    std::printf("═══════════════════════════════════════════\n\n");
    std::fflush(stdout);

    test_experience_level_name();
    test_novice_prompt_content();
    test_intermediate_prompt_content();
    test_advanced_prompt_content();
    test_expert_prompt_content();
    test_all_levels_include_chain_of_thought();
    test_default_level_is_intermediate();

    std::printf("\n═══════════════════════════════════════════\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("═══════════════════════════════════════════\n");
    std::fflush(stdout);

    return gTestsFailed > 0 ? 1 : 0;
}
