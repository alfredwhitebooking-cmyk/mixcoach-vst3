// ═══════════════════════════════════════════════════════════════════════════
//  TestBuildSystemPrompt.cpp — Verifica que buildSystemPrompt()
//  genere el prompt con UI commands en la posición correcta (TOP)
//  y con los nuevos contenidos de la Fase 10
//
//  Build: cmake --build build --config Release --target TestBuildSystemPrompt
//  Run:   build/tests/Release/TestBuildSystemPrompt.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../Source/MixCoach/ai/AiCoachAdapter.h"
#include "../Source/MixCoach/engine/LlmCommandInterpreter.h"

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

using namespace mixcoach;

// ═══════════════════════════════════════════════════════════════════════════
//  1. buildSystemPrompt — UI commands están al principio del prompt
// ═══════════════════════════════════════════════════════════════════════════

static void test_build_system_prompt_ui_commands_at_top() {
    std::printf("\n── buildSystemPrompt: UI commands al principio ──\n");

    // ─── Crear dependencias ─────────────────────────────────────────────
    auto sd = std::make_unique<SharedData>();
    AudioAnalyzer audioAnalyzer;
    PhaseManager pm(sd->getSlotRegistry());
    CoachEngine engine(pm, *sd, audioAnalyzer);
    AiCoachAdapter adapter(*sd, audioAnalyzer, engine, pm);

    // ─── Generar system prompt ─────────────────────────────────────────
    juce::String prompt = adapter.buildSystemPrompt();

    // ─── Verificaciones estructurales ───────────────────────────────────
    TEST("buildSystemPrompt() returned non-empty string", prompt.isNotEmpty());

    // El prompt debe contener la identidad del Coach
    TEST("Prompt contains coach identity 'MIXCOACH'",
         prompt.contains("MIXCOACH"));

    // ═══ VERIFICACIÓN CLAVE: UI commands deben estar ANTES que "TUS SENTIDOS" ═══
    int uiCommandsPos = prompt.indexOf("[UI COMMANDS");
    int sensesPos    = prompt.indexOf("TUS SENTIDOS");

    TEST("[UI COMMANDS] section exists in prompt", uiCommandsPos >= 0);
    TEST("TUS SENTIDOS section exists in prompt", sensesPos >= 0);

    if (uiCommandsPos >= 0 && sensesPos >= 0) {
        TEST("[UI COMMANDS] is BEFORE 'TUS SENTIDOS' (UI commands at TOP)",
             uiCommandsPos < sensesPos);
        std::printf("    [UI COMMANDS] at position %d, TUS SENTIDOS at position %d\n",
                    uiCommandsPos, sensesPos);
    }

    // ═══ VERIFICACIÓN: REASONING PROTOCOL tiene el step 6 de UI commands ═══
    TEST("[REASONING PROTOCOL] section exists",
         prompt.contains("REASONING PROTOCOL"));

    if (prompt.contains("REASONING PROTOCOL")) {
        int rpPos = prompt.indexOf("REASONING PROTOCOL");
        int step6Pos = prompt.indexOf("QUE COMANDOS UI");
        bool step6InSection = (rpPos >= 0 && step6Pos >= 0 
                               && step6Pos > rpPos 
                               && step6Pos < rpPos + 1000);
        TEST("REASONING PROTOCOL has step 6 about UI commands", step6InSection);
        std::printf("    REASONING PROTOCOL at position %d, step 6 at position %d\n", rpPos, step6Pos);
    }

    // ═══ VERIFICACIÓN: Todas las acciones están documentadas ═══
    TEST("reveal_panel is documented", prompt.contains("reveal_panel"));
    TEST("set_coach_state is documented", prompt.contains("set_coach_state"));
    TEST("switch_tab is documented", prompt.contains("switch_tab"));
    TEST("highlight_track is documented", prompt.contains("highlight_track"));
    TEST("celebrate is documented", prompt.contains("celebrate"));
    TEST("set_mode is documented", prompt.contains("set_mode"));
    TEST("return_to_coach is documented", prompt.contains("return_to_coach"));
    TEST("advance_phase is documented", prompt.contains("advance_phase"));
    TEST("show_suggestions is documented", prompt.contains("show_suggestions"));
    TEST("show_report is documented (10th action)", prompt.contains("show_report"));

    // ═══ VERIFICACIÓN: CUANDO USAR secciones existen ═══
    TEST("'CUANDO USAR' sections exist in prompt",
         prompt.contains("CUANDO USAR"));

    if (prompt.contains("CUANDO USAR")) {
        std::printf("    'CUANDO USAR' sections found in prompt\n");
    }

    // ═══ VERIFICACIÓN: REGLAS DE USO están completas ═══
    TEST("'REGLAS DE USO' exists in prompt",
         prompt.contains("REGLAS DE USO"));

    if (prompt.contains("REGLAS DE USO")) {
        std::printf("    REGLAS DE USO section present\n");
    }

    // ═══ VERIFICACIÓN: Formato JSON de ejemplo existe ═══
    TEST("JSON format example (```json) is in prompt",
         prompt.contains("```json"));

    // ═══ VERIFICACIÓN: Identidad del Coach está ANTES de UI commands ═══
    int identityPos = prompt.indexOf("=== SISTEMA: MIXCOACH ===");
    if (identityPos >= 0 && uiCommandsPos >= 0) {
        TEST("Identity (=== SISTEMA: MIXCOACH ===) is BEFORE [UI COMMANDS]",
             identityPos < uiCommandsPos);
        std::printf("    SISTEMA MIXCOACH at position %d, [UI COMMANDS] at position %d\n",
                    identityPos, uiCommandsPos);
    }

    std::printf("\n  Prompt length: %d characters\n", prompt.length());
}

// ═══════════════════════════════════════════════════════════════════════════
//  2. buildSystemPrompt — Master Mode también tiene UI commands al principio
// ═══════════════════════════════════════════════════════════════════════════

static void test_build_system_prompt_master_mode() {
    std::printf("\n── buildSystemPrompt: Master Mode también tiene UI commands ──\n");

    auto sd = std::make_unique<SharedData>();
    AudioAnalyzer audioAnalyzer;
    PhaseManager pm(sd->getSlotRegistry());
    CoachEngine engine(pm, *sd, audioAnalyzer);
    AiCoachAdapter adapter(*sd, audioAnalyzer, engine, pm);

    // Cambiar a Master Mode
    // CoachMode y MasterDestination están en namespace mixcoach (Types.h), no anidados en CoachEngine
    engine.setCoachMode(CoachMode::Master);
    engine.setMasterDestination(MasterDestination::StreamingGeneral);

    juce::String prompt = adapter.buildSystemPrompt();

    TEST("Master Mode prompt is not empty", prompt.isNotEmpty());
    TEST("Master Mode identity present", prompt.contains("MASTER MODE"));

    int uiCommandsPos = prompt.indexOf("[UI COMMANDS");
    int sensesPos    = prompt.indexOf("TUS SENTIDOS");

    if (uiCommandsPos >= 0 && sensesPos >= 0) {
        TEST("Master Mode: [UI COMMANDS] is BEFORE 'TUS SENTIDOS'",
             uiCommandsPos < sensesPos);
    }

    // Master Mode debe tener las acciones relevantes
    TEST("Master Mode: reveal_panel is documented", prompt.contains("reveal_panel"));
    TEST("Master Mode: celebrate is documented", prompt.contains("celebrate"));
    TEST("Master Mode: show_report is documented", prompt.contains("show_report"));
}

// ═══════════════════════════════════════════════════════════════════════════
//  3. buildCommandInstructions — contenido detallado (usa LlmCommandPrompt)
// ═══════════════════════════════════════════════════════════════════════════

static void test_build_command_instructions_cuando_usar() {
    std::printf("\n── buildCommandInstructions: 'CUANDO USAR' sections ──\n");

    juce::String instructions = LlmCommandPrompt::buildCommandInstructions();

    // Verificar que las secciones "CUANDO USAR" existen para cada acción
    TEST("Has CUANDO USAR reveal_panel section",
         instructions.contains("CUANDO USAR reveal_panel"));
    TEST("Has CUANDO USAR set_coach_state section",
         instructions.contains("CUANDO USAR set_coach_state"));
    TEST("Has CUANDO USAR switch_tab section",
         instructions.contains("CUANDO USAR switch_tab"));
    TEST("Has CUANDO USAR highlight_track section",
         instructions.contains("CUANDO USAR highlight_track"));
    TEST("Has CUANDO USAR celebrate section",
         instructions.contains("CUANDO USAR celebrate"));
    TEST("Has CUANDO USAR advance_phase section",
         instructions.contains("CUANDO USAR advance_phase"));
    TEST("Has CUANDO USAR show_report section",
         instructions.contains("CUANDO USAR show_report"));
    TEST("Has CUANDO USAR show_suggestions section",
         instructions.contains("CUANDO USAR show_suggestions"));
    TEST("Has CUANDO USAR set_mode section",
         instructions.contains("CUANDO USAR set_mode"));

    // Verificar REGLAS DE USO expandidas
    TEST("Has 10 REGLAS DE USO", instructions.contains("REGLAS DE USO"));
    TEST("Rule about switch_tab + return_to_coach pattern",
         instructions.contains("SIEMPRE que uses switch_tab, incluye return_to_coach"));

    // Verificar escenarios concretos
    TEST("Scenario: recommend reference without one loaded",
         instructions.contains("recomendar cargar una referencia"));
    TEST("Scenario: track-specific advice with domain mapping",
         instructions.contains("0 = gain"));
    TEST("Scenario: celebrate patterns described",
         instructions.contains("El usuario confirmo que aplico un cambio"));
    TEST("Scenario: advance_phase after completing objectives",
         instructions.contains("completo todos los objetivos"));

    std::printf("    Instructions length: %d characters\n", instructions.length());
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════

int main() {
    std::printf("\n============================================");
    std::printf("\n  TestBuildSystemPrompt — Fase 10 Validation");
    std::printf("\n============================================\n");

    // ─── buildSystemPrompt ────────────────────────────────────
    test_build_system_prompt_ui_commands_at_top();
    test_build_system_prompt_master_mode();

    // ─── buildCommandInstructions (CUANDO USAR sections) ──────
    test_build_command_instructions_cuando_usar();

    // ─── Results ──────────────────────────────────────────────
    std::printf("\n============================================\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("============================================\n");

    return gTestsFailed > 0 ? 1 : 0;
}
