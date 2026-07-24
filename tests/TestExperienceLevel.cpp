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
//  Test 2: Novice — Prompt debe contener lenguaje simple y alentador (ESPAÑOL)
// ═══════════════════════════════════════════════════════════════════════════
static void test_novice_prompt_content()
{
    std::printf("\n── Test 2: Novice Level Prompt (ES) ──\n");
    std::fflush(stdout);

    TestHarness h;
    h.adapter.setExperienceLevel(AiCoachAdapter::ExperienceLevel::Novice);
    auto prompt = h.adapter.buildSystemPrompt();

    TEST("Novice prompt contains 'NIVEL DEL USUARIO'",
         prompt.contains("NIVEL DEL USUARIO"));
    TEST("Novice prompt contains level name 'Novice'",
         prompt.contains("Novice"));
    TEST("Novice prompt mentions '[NIVEL:'",
         prompt.contains("[NIVEL:"));
    TEST("Novice prompt says 'Ensena con paciencia'",
         prompt.contains("Ensena con paciencia"));
    TEST("Novice prompt says 'Explica el concepto'",
         prompt.contains("Explica el concepto"));
    TEST("Novice prompt says 'Celebra los avances'",
         prompt.contains("Celebra los avances"));
    TEST("Novice prompt says 'Evita jerga'",
         prompt.contains("Evita jerga"));
    TEST("Novice prompt does NOT mention 'pre-ring'",
         !prompt.contains("pre-ring"));
    TEST("Novice prompt does NOT say 'no fluff'",
         !prompt.contains("no fluff"));
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 3: Intermediate — Prompt debe usar terminologia estandar (ESPAÑOL)
// ═══════════════════════════════════════════════════════════════════════════
static void test_intermediate_prompt_content()
{
    std::printf("\n── Test 3: Intermediate Level Prompt (ES) ──\n");
    std::fflush(stdout);

    TestHarness h;
    h.adapter.setExperienceLevel(AiCoachAdapter::ExperienceLevel::Intermediate);
    auto prompt = h.adapter.buildSystemPrompt();

    TEST("Intermediate prompt contains level name 'Intermediate'",
         prompt.contains("Intermediate"));
    TEST("Intermediate prompt contains '[NIVEL:'",
         prompt.contains("[NIVEL:"));
    TEST("Intermediate prompt says 'terminologia estandar'",
         prompt.contains("terminologia estandar"));
    TEST("Intermediate prompt says 'Da frecuencias y ratios exactos'",
         prompt.contains("frecuencias y ratios exactos"));
    TEST("Intermediate prompt says 'Explica el POR QUE'",
         prompt.contains("POR QUE"));
    TEST("Intermediate prompt does NOT say 'Avoid jargon'",
         !prompt.contains("Avoid jargon"));
    TEST("Intermediate prompt does NOT mention 'pre-ring'",
         !prompt.contains("pre-ring"));
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 4: Advanced — Prompt debe usar jerga tecnica (ESPAÑOL)
// ═══════════════════════════════════════════════════════════════════════════
static void test_advanced_prompt_content()
{
    std::printf("\n── Test 4: Advanced Level Prompt (ES) ──\n");
    std::fflush(stdout);

    TestHarness h;
    h.adapter.setExperienceLevel(AiCoachAdapter::ExperienceLevel::Advanced);
    auto prompt = h.adapter.buildSystemPrompt();

    TEST("Advanced prompt contains level name 'Advanced'",
         prompt.contains("Advanced"));
    TEST("Advanced prompt contains '[NIVEL:'",
         prompt.contains("[NIVEL:"));
    TEST("Advanced prompt says 'Ve directo al grano'",
         prompt.contains("Ve directo al grano"));
    TEST("Advanced prompt says 'Da numeros exactos'",
         prompt.contains("Da numeros exactos"));
    TEST("Advanced prompt says 'Discute trade-offs'",
         prompt.contains("Discute trade-offs"));
    TEST("Advanced prompt says 'No expliques conceptos basicos'",
         prompt.contains("No expliques conceptos basicos"));
    TEST("Advanced prompt says 'frecuencias, ratios, attack/release'",
         prompt.contains("frecuencias, ratios, attack/release"));
    TEST("Advanced prompt does NOT say 'step-by-step'",
         !prompt.contains("step-by-step"));
    TEST("Advanced prompt does NOT say 'Avoid jargon'",
         !prompt.contains("Avoid jargon"));
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 5: Expert — Prompt debe ser directo, sin emojis, solo numeros (ESPAÑOL)
// ═══════════════════════════════════════════════════════════════════════════
static void test_expert_prompt_content()
{
    std::printf("\n── Test 5: Expert Level Prompt (ES) ──\n");
    std::fflush(stdout);

    TestHarness h;
    h.adapter.setExperienceLevel(AiCoachAdapter::ExperienceLevel::Expert);
    auto prompt = h.adapter.buildSystemPrompt();

    TEST("Expert prompt contains level name 'Expert'",
         prompt.contains("Expert"));
    TEST("Expert prompt contains '[NIVEL:'",
         prompt.contains("[NIVEL:"));
    TEST("Expert prompt says 'Se directo'",
         prompt.contains("Se directo"));
    TEST("Expert prompt says 'sin emojis, sin rodeos'",
         prompt.contains("sin emojis, sin rodeos"));
    TEST("Expert prompt says 'Solo numeros'",
         prompt.contains("Solo numeros"));
    TEST("Expert prompt says 'Tratalo como colega'",
         prompt.contains("Tratalo como colega"));
    TEST("Expert prompt says 'No des opiniones. Da datos'",
         prompt.contains("No des opiniones. Da datos"));
    TEST("Expert prompt does NOT contain 'Avoid jargon'",
         !prompt.contains("Avoid jargon"));
    TEST("Expert prompt does NOT contain 'step-by-step'",
         !prompt.contains("step-by-step"));
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 6: Todos los niveles incluyen Chain-of-Thought (ESPAÑOL, 5 pasos)
// ═══════════════════════════════════════════════════════════════════════════
static void test_all_levels_include_chain_of_thought()
{
    std::printf("\n── Test 6: Chain-of-Thought in all levels (ES) ──\n");
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

        // CoT verification: check that each level prompt includes the level
        // instructions block (e.g., "[NIVEL: PRINCIPIANTE]") which is the
        // equivalent of chain-of-thought reasoning instructions.
        TEST(("CoT present in " + levelName).toRawUTF8(),
             prompt.contains("[NIVEL:") ||
             prompt.contains("PRINCIPIANTE") ||
             prompt.contains("Experto") ||
             prompt.contains("Avanzado"));

        TEST(("CoT Step 1 in " + levelName).toRawUTF8(),
             prompt.contains("1. Que problema es MAS CRITICO"));

        TEST(("CoT Step 2 in " + levelName).toRawUTF8(),
             prompt.contains("2. Cual es la UNA accion"));

        TEST(("CoT Step 3 in " + levelName).toRawUTF8(),
             prompt.contains("3. Que numero exacto"));

        TEST(("CoT Step 4 in " + levelName).toRawUTF8(),
             prompt.contains("4. Que va a pasar"));

        TEST(("CoT Step 5 in " + levelName).toRawUTF8(),
             prompt.contains("5. Como pregunto"));
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
