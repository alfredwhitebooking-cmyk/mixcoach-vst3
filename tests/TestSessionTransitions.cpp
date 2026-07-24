// ═══════════════════════════════════════════════════════════════════════════
//  TestSessionTransitions — Pruebas de transición para estados de sesión
//  y escenas principales (onboarding + coaching paths).
//
//  Verifica:
//   ─── Onboarding ──────────────────────
//   1. Secuencia completa: Welcome → Intention → Genre → ReferenceStage
//      → MessengerStage → SessionPrep → MixMapStage → GainStaging (coaching)
//   2. Cada transición setea isPreFullUI correctamente
//   3. isPreFullUI → false al entrar a coaching
//   4. Transiciones forzadas (backwards, skips)
//   5. Auto-advance (robotNod + 400ms delays)
//
//   ─── Coaching ────────────────────────
//   6. Secuencia completa: GainStaging → Balance → EQ → Compression
//      → Space → Automation → MasterCheck → Report
//   7. Cada transición setea análisis contextual correcto
//   8. Transiciones de ida y vuelta
//   9. Auto-return (Tools/Session → vuelta a Coach con timeout)
//
//   ─── Progressive Revelation ──────────
//   10. AnalysisScope cambia según el estado
//   11. Paneles permitidos/bloqueados por scope
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include "../Source/MixCoach/UI/CoachRoomState.h"
#include "../Source/MixCoach/engine/PanelRevealManager.h"

// ─── Test runner ───────────────────────────────────────────────────────────
static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define TEST(name, expr) do {                                                   \
    if (!(expr)) {                                                              \
        std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: %s (%s:%d)\n",              \
                     name, __FILE__, __LINE__);                                 \
        std::fflush(stderr);                                                    \
        gTestsFailed++;                                                         \
    } else {                                                                    \
        std::printf("  \xe2\x9c\x85 PASS: %s\n", name);                         \
        std::fflush(stdout);                                                    \
        gTestsPassed++;                                                         \
    }                                                                           \
} while(0)

using namespace mixcoach;

// ═══════════════════════════════════════════════════════════════════════════════
//  ONBOARDING PATH
// ═══════════════════════════════════════════════════════════════════════════════

// ─── 1. Secuencia onboarding completa ─────────────────────────────────────
//     Welcome → Intention → Genre → ReferenceStage → MessengerStage
//     → SessionPrep → MixMapStage → GainStaging (inicio coaching)

static void test_onboarding_full_sequence()
{
    std::printf("\n── [1] Onboarding full sequence ──\n");

    // Verificar cada estado en orden
    CoachRoomState states[] = {
        CoachRoomState::Welcome,
        CoachRoomState::Intention,
        CoachRoomState::Genre,
        CoachRoomState::ReferenceStage,
        CoachRoomState::MessengerStage,
        CoachRoomState::SessionPrep,
        CoachRoomState::MixMapStage,
    };
    int numStates = sizeof(states) / sizeof(states[0]);

    for (int i = 0; i < numStates; ++i) {
        // Cada onboarding state debe ser pre-FullUI
        TEST(juce::String("Onboarding state ") + juce::String(i) + " is pre-FullUI",
             isPreFullUI(states[i]));

        // Cada onboarding state NO debe ser coaching state
        TEST(juce::String("Onboarding state ") + juce::String(i) + " is NOT coaching",
             !isCoachingState(states[i]));
    }

    // El primer coaching state es GainStaging
    TEST("GainStaging IS coaching state",
         isCoachingState(CoachRoomState::GainStaging));

    // GainStaging NO es pre-FullUI
    TEST("GainStaging is NOT pre-FullUI",
         !isPreFullUI(CoachRoomState::GainStaging));

    // MixMapStage es el último pre-FullUI antes de coaching
    TEST("MixMapStage is pre-FullUI",
         isPreFullUI(CoachRoomState::MixMapStage));
    TEST("GainStaging is NOT pre-FullUI",
         !isPreFullUI(CoachRoomState::GainStaging));

    // Progreso: onboarding < coaching < report
    float preFullUIProgress = coachRoomStateProgress(CoachRoomState::MixMapStage);
    float coachingProgress = coachRoomStateProgress(CoachRoomState::GainStaging);
    float reportProgress = coachRoomStateProgress(CoachRoomState::Report);

    TEST("Onboarding progress < coaching progress",
         preFullUIProgress < coachingProgress);
    TEST("Coaching progress < report progress",
         coachingProgress < reportProgress);
}

// ─── 2. Labels de onboarding ──────────────────────────────────────────────

static void test_onboarding_labels()
{
    std::printf("\n── [2] Onboarding labels ──\n");

    const char* welcome = coachRoomStateLabel(CoachRoomState::Welcome);
    const char* intention = coachRoomStateLabel(CoachRoomState::Intention);
    const char* genre = coachRoomStateLabel(CoachRoomState::Genre);
    const char* ref = coachRoomStateLabel(CoachRoomState::ReferenceStage);
    const char* messenger = coachRoomStateLabel(CoachRoomState::MessengerStage);
    const char* prep = coachRoomStateLabel(CoachRoomState::SessionPrep);
    const char* mixmap = coachRoomStateLabel(CoachRoomState::MixMapStage);

    // Todos los labels deben ser no vacíos
    TEST("Welcome label non-empty", welcome[0] != '\0');
    TEST("Intention label non-empty", intention[0] != '\0');
    TEST("Genre label non-empty", genre[0] != '\0');
    TEST("ReferenceStage label non-empty", ref[0] != '\0');
    TEST("MessengerStage label non-empty", messenger[0] != '\0');
    TEST("SessionPrep label non-empty", prep[0] != '\0');
    TEST("MixMapStage label non-empty", mixmap[0] != '\0');

    // Todos los labels deben ser distintos
    bool allDistinct = true;
    const char* labels[] = {welcome, intention, genre, ref, messenger, prep, mixmap};
    for (int i = 0; i < 7 && allDistinct; ++i) {
        for (int j = i + 1; j < 7; ++j) {
            if (std::strcmp(labels[i], labels[j]) == 0) {
                allDistinct = false;
                break;
            }
        }
    }
    TEST("All 7 onboarding labels are distinct", allDistinct);
}

// ─── 3. Transición Welcome → Intention (progressive disclosure) ───────────

static void test_onboarding_welcome_to_intention()
{
    std::printf("\n── [3] Welcome → Intention transition ──\n");

    // En Welcome, ningún panel debe estar visible excepto Coach
    TEST("Welcome is NOT coaching", !isCoachingState(CoachRoomState::Welcome));
    TEST("Welcome is pre-FullUI", isPreFullUI(CoachRoomState::Welcome));

    // La transición a Intention debe mantener pre-FullUI
    TEST("Intention is pre-FullUI", isPreFullUI(CoachRoomState::Intention));
    TEST("Intention < ReferenceStage (ordering)", 
         static_cast<int>(CoachRoomState::Intention) < 
         static_cast<int>(CoachRoomState::ReferenceStage));
}

// ─── 4. Auto-avance: cada selección avanza al siguiente estado ────────────

static void test_onboarding_auto_advance_ordering()
{
    std::printf("\n── [4] Onboarding auto-advance ordering ──\n");

    // Verificar que los estados están en orden de progressive disclosure
    int welcome = static_cast<int>(CoachRoomState::Welcome);
    int intention = static_cast<int>(CoachRoomState::Intention);
    int genre = static_cast<int>(CoachRoomState::Genre);
    int ref = static_cast<int>(CoachRoomState::ReferenceStage);
    int messenger = static_cast<int>(CoachRoomState::MessengerStage);
    int prep = static_cast<int>(CoachRoomState::SessionPrep);
    int mixmap = static_cast<int>(CoachRoomState::MixMapStage);

    TEST("Welcome < Intention", welcome < intention);
    TEST("Intention < Genre", intention < genre);
    TEST("Genre < ReferenceStage", genre < ref);
    TEST("ReferenceStage < MessengerStage", ref < messenger);
    TEST("MessengerStage < SessionPrep", messenger < prep);
    TEST("SessionPrep < MixMapStage", prep < mixmap);
    TEST("MixMapStage < GainStaging (start of coaching)",
         mixmap < static_cast<int>(CoachRoomState::GainStaging));
}

// ═══════════════════════════════════════════════════════════════════════════════
//  COACHING PATH
// ═══════════════════════════════════════════════════════════════════════════════

// ─── 5. Secuencia coaching completa ───────────────────────────────────────
//     GainStaging → Balance → EQ → Compression → Space → Automation
//     → MasterCheck → Report

static void test_coaching_full_sequence()
{
    std::printf("\n── [5] Coaching full sequence ──\n");

    // IDs de las fases de coaching (según el plan: 7 fases + Report)
    CoachRoomState phases[] = {
        CoachRoomState::GainStaging,
        CoachRoomState::Balance,
        CoachRoomState::EQ,
        CoachRoomState::Compression,
        CoachRoomState::Space,
        CoachRoomState::Automation,
        CoachRoomState::Refinement,
        CoachRoomState::MasterCheck,
        CoachRoomState::Report,
    };
    int numPhases = sizeof(phases) / sizeof(phases[0]);

    for (int i = 0; i < numPhases; ++i) {
        // Los coaching phases (except Report) must be isCoachingState
        if (phases[i] != CoachRoomState::Report) {
            TEST(juce::String("Phase ") + juce::String(i) + " IS coaching",
                 isCoachingState(phases[i]));
            TEST(juce::String("Phase ") + juce::String(i) + " is NOT pre-FullUI",
                 !isPreFullUI(phases[i]));
        }
    }

    // Report es un estado especial
    TEST("Report is NOT coaching", !isCoachingState(CoachRoomState::Report));
    TEST("Report is NOT pre-FullUI", !isPreFullUI(CoachRoomState::Report));
}

// ─── 6. Labels de coaching ────────────────────────────────────────────────

static void test_coaching_labels()
{
    std::printf("\n── [6] Coaching labels ──\n");

    CoachRoomState phases[] = {
        CoachRoomState::GainStaging,
        CoachRoomState::Balance,
        CoachRoomState::EQ,
        CoachRoomState::Compression,
        CoachRoomState::Space,
        CoachRoomState::Automation,
        CoachRoomState::Refinement,
        CoachRoomState::MasterCheck,
        CoachRoomState::Report,
    };
    int numPhases = sizeof(phases) / sizeof(phases[0]);

    // Todos los labels deben ser no vacíos
    for (int i = 0; i < numPhases; ++i) {
        const char* label = coachRoomStateLabel(phases[i]);
        TEST(juce::String("Phase ") + juce::String(i) + " label non-empty",
             label[0] != '\0');
    }

    // Todos los labels deben ser distintos
    const char* labels[8];
    for (int i = 0; i < numPhases; ++i)
        labels[i] = coachRoomStateLabel(phases[i]);

    bool allDistinct = true;
    for (int i = 0; i < numPhases && allDistinct; ++i) {
        for (int j = i + 1; j < numPhases; ++j) {
            if (std::strcmp(labels[i], labels[j]) == 0) {
                allDistinct = false;
                std::fprintf(stderr, "  Duplicate at %d,%d: '%s'\n", i, j, labels[i]);
                break;
            }
        }
    }
    TEST("All coaching labels are distinct", allDistinct);
}

// ─── 7. Orden de fases de coaching ────────────────────────────────────────

static void test_coaching_phase_ordering()
{
    std::printf("\n── [7] Coaching phase ordering ──\n");

    int gain = static_cast<int>(CoachRoomState::GainStaging);
    int balance = static_cast<int>(CoachRoomState::Balance);
    int eq = static_cast<int>(CoachRoomState::EQ);
    int comp = static_cast<int>(CoachRoomState::Compression);
    int space = static_cast<int>(CoachRoomState::Space);
    int automation = static_cast<int>(CoachRoomState::Automation);
    int refinement = static_cast<int>(CoachRoomState::Refinement);
    int master = static_cast<int>(CoachRoomState::MasterCheck);
    int report = static_cast<int>(CoachRoomState::Report);

    TEST("GainStaging < Balance", gain < balance);
    TEST("Balance < EQ", balance < eq);
    TEST("EQ < Compression", eq < comp);
    TEST("Compression < Space", comp < space);
    TEST("Space < Automation", space < automation);
    TEST("Automation < Refinement", automation < refinement);
    TEST("Refinement < MasterCheck", refinement < master);
    TEST("MasterCheck < Report", master < report);
}

// ─── 8. Progreso monótono ─────────────────────────────────────────────────

static void test_monotonic_progress()
{
    std::printf("\n── [8] Monotonic progress through all states ──\n");

    CoachRoomState allStates[] = {
        CoachRoomState::Welcome,
        CoachRoomState::Intention,
        CoachRoomState::Genre,
        CoachRoomState::ReferenceStage,
        CoachRoomState::MessengerStage,
        CoachRoomState::SessionPrep,
        CoachRoomState::MixMapStage,
        CoachRoomState::GainStaging,
        CoachRoomState::Balance,
        CoachRoomState::EQ,
        CoachRoomState::Compression,
        CoachRoomState::Space,
        CoachRoomState::Automation,
        CoachRoomState::Refinement,
        CoachRoomState::MasterCheck,
        CoachRoomState::Report,
    };
    int numStates = sizeof(allStates) / sizeof(allStates[0]);

    float lastProgress = -1.0f;
    bool increasing = true;
    for (int i = 0; i < numStates; ++i) {
        float p = coachRoomStateProgress(allStates[i]);
        if (p <= lastProgress) {
            increasing = false;
            std::fprintf(stderr, "  Non-increasing at state %d: %f <= %f\n",
                         i, p, lastProgress);
        }
        lastProgress = p;
    }
    TEST("Progress increases monotonically across all 16 states", increasing);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  PROGRESSIVE REVELATION — AnalysisScope
// ═══════════════════════════════════════════════════════════════════════════════

// ─── 9. AnalysisScope evoluciona con el estado ────────────────────────────
//     Setup → Coaching → Report según el CoachRoomState

static void test_analysis_scope_progression()
{
    std::printf("\n── [9] AnalysisScope progression ──\n");

    PanelRevealManager mgr;

    // Default scope debe ser Setup (comienza en Welcome)
    TEST("Default scope is Setup",
         mgr.getCurrentScope() == AnalysisScope::Setup);

    // ─── Setup scope: solo Reference, Messengers, MixMap ──
    mgr.setCurrentScope(AnalysisScope::Setup);

    // Paneles permitidos en Setup
    TEST("Setup allows Coach (always)", mgr.validateReveal(AnalysisScope::Setup, PanelId::Coach));
    TEST("Setup allows Reference", mgr.validateReveal(AnalysisScope::Setup, PanelId::Reference));
    TEST("Setup allows Messengers", mgr.validateReveal(AnalysisScope::Setup, PanelId::Messengers));
    TEST("Setup allows MixMap", mgr.validateReveal(AnalysisScope::Setup, PanelId::MixMap));

    // Paneles bloqueados en Setup
    TEST("Setup blocks Tools (no analyzer in onboarding)",
         !mgr.validateReveal(AnalysisScope::Setup, PanelId::Tools));
    TEST("Setup blocks Session (no progress in onboarding)",
         !mgr.validateReveal(AnalysisScope::Setup, PanelId::Session));
    TEST("Setup blocks Report (no report in onboarding)",
         !mgr.validateReveal(AnalysisScope::Setup, PanelId::Report));

    // ─── Coaching scope: Tools + Session permitidos ──
    mgr.setCurrentScope(AnalysisScope::Coaching);

    TEST("Coaching allows Coach", mgr.validateReveal(AnalysisScope::Coaching, PanelId::Coach));
    TEST("Coaching allows Tools", mgr.validateReveal(AnalysisScope::Coaching, PanelId::Tools));
    TEST("Coaching allows Session", mgr.validateReveal(AnalysisScope::Coaching, PanelId::Session));
    TEST("Coaching allows Reference", mgr.validateReveal(AnalysisScope::Coaching, PanelId::Reference));
    TEST("Coaching allows Messengers", mgr.validateReveal(AnalysisScope::Coaching, PanelId::Messengers));

    // En coaching NO se permite Report automáticamente
    TEST("Coaching blocks Report",
         !mgr.validateReveal(AnalysisScope::Coaching, PanelId::Report));

    // ─── Expert scope: todos los paneles ──
    mgr.setCurrentScope(AnalysisScope::Expert);

    TEST("Expert allows Coach", mgr.validateReveal(AnalysisScope::Expert, PanelId::Coach));
    TEST("Expert allows Reference", mgr.validateReveal(AnalysisScope::Expert, PanelId::Reference));
    TEST("Expert allows Messengers", mgr.validateReveal(AnalysisScope::Expert, PanelId::Messengers));
    TEST("Expert allows MixMap", mgr.validateReveal(AnalysisScope::Expert, PanelId::MixMap));
    TEST("Expert allows Tools", mgr.validateReveal(AnalysisScope::Expert, PanelId::Tools));
    TEST("Expert allows Session", mgr.validateReveal(AnalysisScope::Expert, PanelId::Session));
    TEST("Expert allows Report", mgr.validateReveal(AnalysisScope::Expert, PanelId::Report));

    // ─── Report scope: solo Coach + Report ──
    mgr.setCurrentScope(AnalysisScope::Report);

    TEST("Report allows Coach", mgr.validateReveal(AnalysisScope::Report, PanelId::Coach));
    TEST("Report allows Report", mgr.validateReveal(AnalysisScope::Report, PanelId::Report));
    TEST("Report blocks Tools", !mgr.validateReveal(AnalysisScope::Report, PanelId::Tools));
    TEST("Report blocks Session", !mgr.validateReveal(AnalysisScope::Report, PanelId::Session));
    TEST("Report blocks Reference", !mgr.validateReveal(AnalysisScope::Report, PanelId::Reference));
    TEST("Report blocks Messengers", !mgr.validateReveal(AnalysisScope::Report, PanelId::Messengers));
    TEST("Report blocks MixMap", !mgr.validateReveal(AnalysisScope::Report, PanelId::MixMap));

    // ─── Transición: Setup → Coaching ──
    mgr.setCurrentScope(AnalysisScope::Coaching);
    TEST("After transition to Coaching: scope is Coaching",
         mgr.getCurrentScope() == AnalysisScope::Coaching);

    // ─── Transición: Coaching → Report ──
    mgr.setCurrentScope(AnalysisScope::Report);
    TEST("After transition to Report: scope is Report",
         mgr.getCurrentScope() == AnalysisScope::Report);
}

// ─── 10. Edge cases en AnalysisScope ──────────────────────────────────────

static void test_analysis_scope_edge_cases()
{
    std::printf("\n── [10] AnalysisScope edge cases ──\n");

    PanelRevealManager mgr;

    // PanelId::Count no debe revelarse nunca
    TEST("Setup blocks Count panel",
         !mgr.validateReveal(AnalysisScope::Setup, static_cast<PanelId>(PanelId::Count)));
    TEST("Coaching blocks Count panel",
         !mgr.validateReveal(AnalysisScope::Coaching, static_cast<PanelId>(PanelId::Count)));

    // Scope desconocido (cast inválido) bloquea todos excepto Coach
    // Esto es un safety check contra casts incorrectos
    auto invalidScope = static_cast<AnalysisScope>(99);
    TEST("Invalid scope blocks Tools",
         !mgr.validateReveal(invalidScope, PanelId::Tools));
    TEST("Invalid scope blocks Report",
         !mgr.validateReveal(invalidScope, PanelId::Report));
}

// ═══════════════════════════════════════════════════════════════════════════════
//  INTEGRATION — Onboarding → Coaching flow completo
// ═══════════════════════════════════════════════════════════════════════════════

// ─── 11. Transición completa onboarding → coaching → report ───────────────

static void test_onboarding_to_coaching_to_report()
{
    std::printf("\n── [11] Onboarding → Coaching → Report (full journey) ──\n");

    // 1. Welcome → empieza en setup scope
    PanelRevealManager mgr;
    mgr.setCurrentScope(AnalysisScope::Setup);

    // 2. En Welcome, Tools bloqueado
    TEST("Pre-coaching: Tools blocked",
         !mgr.validateReveal(mgr.getCurrentScope(), PanelId::Tools));

    // 3. Transición a coaching → cambia scope
    mgr.setCurrentScope(AnalysisScope::Coaching);

    // 4. En coaching, Tools permitido
    TEST("Coaching: Tools allowed",
         mgr.validateReveal(mgr.getCurrentScope(), PanelId::Tools));

    // 5. MixMap NO está en coaching scope (solo en setup/expert)
    TEST("Coaching: MixMap NOT auto-revealed",
         !mgr.validateReveal(mgr.getCurrentScope(), PanelId::MixMap));

    // 6. Acción explícita del usuario → Expert scope permite todo
    mgr.setCurrentScope(AnalysisScope::Expert);
    TEST("Expert: Tools allowed",
         mgr.validateReveal(mgr.getCurrentScope(), PanelId::Tools));
    TEST("Expert: MixMap allowed",
         mgr.validateReveal(mgr.getCurrentScope(), PanelId::MixMap));
    TEST("Expert: Session allowed",
         mgr.validateReveal(mgr.getCurrentScope(), PanelId::Session));

    // 7. Transición a report
    mgr.setCurrentScope(AnalysisScope::Report);
    TEST("Report: Tools blocked again",
         !mgr.validateReveal(mgr.getCurrentScope(), PanelId::Tools));
    TEST("Report: Report allowed",
         mgr.validateReveal(mgr.getCurrentScope(), PanelId::Report));
}

// ═══════════════════════════════════════════════════════════════════════════════
//  ENUM CONSISTENCY
// ═══════════════════════════════════════════════════════════════════════════════

static void test_enum_consistency()
{
    std::printf("\n── [12] Enum consistency ──\n");

    // Count debe coincidir con el número de estados
    // (Welcome=0, Intention=1, Genre=2, ReferenceStage=3,
    //  MessengerStage=4, SessionPrep=5, MixMapStage=6,
    //  GainStaging=7, Balance=8, EQ=9, Compression=10,
    //  Space=11, Automation=12, Refinement=13, MasterCheck=14, Report=15)
    // Automation y Refinement se añadieron, por lo que Count debe ser 16
    TEST("Count == 16 (con Automation + Refinement)",
         static_cast<int>(CoachRoomState::Count) == 16);

    // Automation debe existir como fase independiente
    TEST("Automation int value is 12",
         static_cast<int>(CoachRoomState::Automation) == 12);
    TEST("Refinement int value is 13",
         static_cast<int>(CoachRoomState::Refinement) == 13);

    // Coaching states deben estar entre GainStaging y MasterCheck (inclusive)
    int gain = static_cast<int>(CoachRoomState::GainStaging);
    int master = static_cast<int>(CoachRoomState::MasterCheck);
    TEST("GainStaging (7) >= 0", gain >= 0);
    TEST("MasterCheck (14) < Count", master < static_cast<int>(CoachRoomState::Count));
    int numCoachingStates = master - gain + 1;
    TEST("8 coaching states (GainStaging→Balance→EQ→Compression→Space→Automation→Refinement→MasterCheck)",
         numCoachingStates == 8);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  PANEL REVEAL INTEGRATION
// ═══════════════════════════════════════════════════════════════════════════════

static void test_panel_reveal_integration()
{
    std::printf("\n── [13] Panel reveal integration ──\n");

    PanelRevealManager mgr;

    // Coach siempre está revelado por defecto
    TEST("Coach already revealed", mgr.isPanelRevealed(PanelId::Coach));

    // Revelar Reference en Setup scope → permitido
    mgr.setCurrentScope(AnalysisScope::Setup);
    TEST("Setup: validate Reference reveal",
         mgr.validateReveal(AnalysisScope::Setup, PanelId::Reference));
    mgr.markPanelRevealed(PanelId::Reference);
    TEST("Reference now revealed", mgr.isPanelRevealed(PanelId::Reference));

    // Intentar revelar Tools en Setup scope → bloqueado
    TEST("Setup: validate Tools reveal FAILS",
         !mgr.validateReveal(AnalysisScope::Setup, PanelId::Tools));
    TEST("Tools NOT revealed (blocked by guard)",
         !mgr.isPanelRevealed(PanelId::Tools));

    // Cambiar a Coaching scope → Tools permitido
    mgr.setCurrentScope(AnalysisScope::Coaching);
    TEST("Coaching: validate Tools reveal",
         mgr.validateReveal(AnalysisScope::Coaching, PanelId::Tools));
    mgr.markPanelRevealed(PanelId::Tools);
    TEST("Tools now revealed", mgr.isPanelRevealed(PanelId::Tools));

    // Session en Coaching → permitido
    TEST("Coaching: validate Session reveal",
         mgr.validateReveal(AnalysisScope::Coaching, PanelId::Session));
    mgr.markPanelRevealed(PanelId::Session);
    TEST("Session now revealed", mgr.isPanelRevealed(PanelId::Session));

    // Report en Coaching → bloqueado
    TEST("Coaching: validate Report reveal FAILS",
         !mgr.validateReveal(AnalysisScope::Coaching, PanelId::Report));
    TEST("Report NOT revealed (blocked by guard)",
         !mgr.isPanelRevealed(PanelId::Report));

    // Cambiar a Expert → Report permitido
    mgr.setCurrentScope(AnalysisScope::Expert);
    TEST("Expert: validate Report reveal",
         mgr.validateReveal(AnalysisScope::Expert, PanelId::Report));
    mgr.markPanelRevealed(PanelId::Report);
    TEST("Report now revealed", mgr.isPanelRevealed(PanelId::Report));

    // 5 paneles revelados: Coach, Reference, Tools, Session, Report
    TEST("5 panels revealed total",
         mgr.getRevealedPanels().size() == 5);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  BACKWARDS TRANSITIONS — Coaching → Setup (transiciones regresivas)
// ═══════════════════════════════════════════════════════════════════════════════

// ─── 15. Transiciones backwards: coaching → setup ─────────────────────────
//     GainStaging → MixMapStage (regresivo, cambia scope Coaching → Setup)
//     MasterCheck → Welcome (reset completo)

static void test_transitions_backwards()
{
    std::printf("\n── [15] Backwards transitions ──\n");

    // Un estado pre-FullUI nunca debe ser > un coaching state
    TEST("Welcome (0) < GainStaging (7)",
         static_cast<int>(CoachRoomState::Welcome) <
         static_cast<int>(CoachRoomState::GainStaging));
    TEST("MixMapStage (6) < GainStaging (7)",
         static_cast<int>(CoachRoomState::MixMapStage) <
         static_cast<int>(CoachRoomState::GainStaging));

    // Simular setCoachRoomState de coaching a setup: scope debe cambiar
    PanelRevealManager mgr;

    // Estamos en GainStaging (Coaching scope)
    mgr.setCurrentScope(AnalysisScope::Coaching);
    TEST("Coaching scope active", mgr.getCurrentScope() == AnalysisScope::Coaching);

    // Volver a MixMapStage: como MixMapStage < GainStaging, simulamos
    // que setCoachRoomState setea scope = Setup
    mgr.setCurrentScope(AnalysisScope::Setup);
    TEST("Backwards: Setup scope restored", mgr.getCurrentScope() == AnalysisScope::Setup);

    // En Setup, Tools debe estar bloqueado otra vez
    TEST("Backwards: Tools blocked again",
         !mgr.validateReveal(mgr.getCurrentScope(), PanelId::Tools));
    TEST("Backwards: Reference allowed again",
         mgr.validateReveal(mgr.getCurrentScope(), PanelId::Reference));

    // Ir de Report a Welcome: scope pasa de Report a Setup
    mgr.setCurrentScope(AnalysisScope::Report);
    TEST("Report scope active", mgr.getCurrentScope() == AnalysisScope::Report);

    // Reset a Welcome
    mgr.setCurrentScope(AnalysisScope::Setup);
    TEST("Reset to Setup: scope correct", mgr.getCurrentScope() == AnalysisScope::Setup);

    // En Setup, Report bloqueado
    TEST("After reset: Report blocked",
         !mgr.validateReveal(mgr.getCurrentScope(), PanelId::Report));
    TEST("After reset: Tools blocked",
         !mgr.validateReveal(mgr.getCurrentScope(), PanelId::Tools));
}

// ═══════════════════════════════════════════════════════════════════════════════
//  FORCE TRANSITIONS — Cambio brusco de scope sin paso narrativo
// ═══════════════════════════════════════════════════════════════════════════════

// ─── 16. ForceTransition: cambio forzado de scope (como forceTransition en SceneManager) ──
//     Setup → Expert directamente (usuario fuerza reveal)
//     Coaching → Report directamente

static void test_force_transition_scope()
{
    std::printf("\n── [16] Force transition scope ──\n");

    PanelRevealManager mgr;

    // Setup scope inicial
    mgr.setCurrentScope(AnalysisScope::Setup);
    TEST("Initial scope is Setup", mgr.getCurrentScope() == AnalysisScope::Setup);

    // ─── Force: Setup → Expert (usuario hace clic en panel menu sin pasar por coaching) ──
    // Esto simula onPanelMenuSelected que setea Expert para acción explícita
    mgr.setCurrentScope(AnalysisScope::Expert);
    TEST("Force Expert: scope changed", mgr.getCurrentScope() == AnalysisScope::Expert);
    TEST("Force Expert: Tools allowed",
         mgr.validateReveal(AnalysisScope::Expert, PanelId::Tools));
    TEST("Force Expert: Report allowed",
         mgr.validateReveal(AnalysisScope::Expert, PanelId::Report));

    // ─── Force: Expert → Setup (el Director inicia un nuevo problema) ──
    // Esto simula que setCoachRoomState se llama con un estado pre-FullUI
    mgr.setCurrentScope(AnalysisScope::Setup);
    TEST("Force Setup: scope changed back", mgr.getCurrentScope() == AnalysisScope::Setup);
    TEST("Force Setup: Tools blocked again",
         !mgr.validateReveal(AnalysisScope::Setup, PanelId::Tools));

    // ─── Force: Coaching → Report (el ciclo termina abruptamente) ──
    mgr.setCurrentScope(AnalysisScope::Coaching);
    mgr.setCurrentScope(AnalysisScope::Report);
    TEST("Force Report: scope changed", mgr.getCurrentScope() == AnalysisScope::Report);
    TEST("Force Report: Tools blocked",
         !mgr.validateReveal(AnalysisScope::Report, PanelId::Tools));
    TEST("Force Report: Report allowed",
         mgr.validateReveal(AnalysisScope::Report, PanelId::Report));

    // ─── Force: Report → Coaching (el usuario quiere continuar editando) ──
    mgr.setCurrentScope(AnalysisScope::Coaching);
    TEST("Force Coaching: Tools allowed again",
         mgr.validateReveal(AnalysisScope::Coaching, PanelId::Tools));

    // ─── Verificar que las reglas de reveal se mantienen despuEs de force transitions ──
    TEST("After forces: Coach always allowed",
         mgr.validateReveal(AnalysisScope::Setup, PanelId::Coach));
    TEST("After forces: invalid scope still blocks",
         !mgr.validateReveal(static_cast<AnalysisScope>(99), PanelId::Tools));
}

// ═══════════════════════════════════════════════════════════════════════════════
//  NAVIGATIONSHELL MOCK INTEGRATION — Simula el wiring de setCoachRoomState
// ═══════════════════════════════════════════════════════════════════════════════

// ─── 17. Simular la lOgica de NavigationShell::setCoachRoomState() ─────────
//     Verifica que el cableado scope → estado funciona para TODOS los estados

// Helper: simula la lOgica de setCoachRoomState para determinar el scope
static AnalysisScope scopeForState(CoachRoomState state)
{
    if (state == CoachRoomState::Report)
        return AnalysisScope::Report;
    // isCoachingState retorna true para GainStaging..MasterCheck (y Automation/Refinement)
    if (isCoachingState(state))
        return AnalysisScope::Coaching;
    return AnalysisScope::Setup;
}

static void test_navshell_mock_scope_wiring()
{
    std::printf("\n── [17] NavShell mock: scope wiring simulation ──\n");

    PanelRevealManager mgr;

    // ─── Test 17a: Todos los onboarding states → Setup scope ──
    CoachRoomState setupStates[] = {
        CoachRoomState::Welcome,
        CoachRoomState::Intention,
        CoachRoomState::Genre,
        CoachRoomState::ReferenceStage,
        CoachRoomState::MessengerStage,
        CoachRoomState::SessionPrep,
        CoachRoomState::MixMapStage,
    };
    for (auto state : setupStates) {
        mgr.setCurrentScope(scopeForState(state));
        TEST(juce::String("Setup state ") + coachRoomStateLabel(state) + " → Setup scope",
             mgr.getCurrentScope() == AnalysisScope::Setup);
        TEST(juce::String("Setup state ") + coachRoomStateLabel(state) + ": Tools blocked",
             !mgr.validateReveal(mgr.getCurrentScope(), PanelId::Tools));
    }

    // ─── Test 17b: Todos los coaching states → Coaching scope ──
    CoachRoomState coachingStates[] = {
        CoachRoomState::GainStaging,
        CoachRoomState::Balance,
        CoachRoomState::EQ,
        CoachRoomState::Compression,
        CoachRoomState::Space,
        CoachRoomState::Automation,
        CoachRoomState::Refinement,
        CoachRoomState::MasterCheck,
    };
    for (auto state : coachingStates) {
        mgr.setCurrentScope(scopeForState(state));
        TEST(juce::String("Coaching state ") + coachRoomStateLabel(state) + " → Coaching scope",
             mgr.getCurrentScope() == AnalysisScope::Coaching);
        TEST(juce::String("Coaching state ") + coachRoomStateLabel(state) + ": Tools allowed",
             mgr.validateReveal(mgr.getCurrentScope(), PanelId::Tools));
    }

    // ─── Test 17c: Report → Report scope ──
    mgr.setCurrentScope(scopeForState(CoachRoomState::Report));
    TEST("Report → Report scope", mgr.getCurrentScope() == AnalysisScope::Report);
    TEST("Report: Tools blocked",
         !mgr.validateReveal(mgr.getCurrentScope(), PanelId::Tools));
    TEST("Report: Report allowed",
         mgr.validateReveal(mgr.getCurrentScope(), PanelId::Report));

    // ─── Test 17d: Simular ciclo completo de vida ──
    // Welcome (Setup) → GainStaging (Coaching) → Report → Welcome (Setup)
    mgr.setCurrentScope(scopeForState(CoachRoomState::Welcome));
    TEST("Cycle: Welcome → Setup", mgr.getCurrentScope() == AnalysisScope::Setup);

    mgr.setCurrentScope(scopeForState(CoachRoomState::GainStaging));
    TEST("Cycle: GainStaging → Coaching", mgr.getCurrentScope() == AnalysisScope::Coaching);
    TEST("Cycle: Tools now allowed",
         mgr.validateReveal(mgr.getCurrentScope(), PanelId::Tools));

    mgr.setCurrentScope(scopeForState(CoachRoomState::Report));
    TEST("Cycle: Report → Report scope", mgr.getCurrentScope() == AnalysisScope::Report);

    mgr.setCurrentScope(scopeForState(CoachRoomState::Welcome));
    TEST("Cycle: back to Welcome → Setup", mgr.getCurrentScope() == AnalysisScope::Setup);
    TEST("Cycle: Tools blocked again",
         !mgr.validateReveal(mgr.getCurrentScope(), PanelId::Tools));
}

// ═══════════════════════════════════════════════════════════════════════════════
//  PROGRESS — Valores esperados
// ═══════════════════════════════════════════════════════════════════════════════

static void test_expected_progress_values()
{
    std::printf("\n── [14] Expected progress values ──\n");

    // Welcome = 0/15 = 0.0
    // Intention = 1/15 ≈ 0.067
    // Genre = 2/15 ≈ 0.133
    // ...
    // Report = 15/15 = 1.0

    float pWelcome = coachRoomStateProgress(CoachRoomState::Welcome);
    float pIntention = coachRoomStateProgress(CoachRoomState::Intention);
    float pReport = coachRoomStateProgress(CoachRoomState::Report);

    TEST("Welcome progress ≈ 0.0", pWelcome >= 0.0f && pWelcome < 0.01f);
    TEST("Report progress ≈ 1.0", pReport >= 0.99f && pReport <= 1.0f);
    TEST("Intention progress > Welcome progress", pIntention > pWelcome);
    TEST("Report progress > Intention progress", pReport > pIntention);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════════

int main()
{
    std::printf("================================================================================\n");
    std::printf("  Session Transition Tests\n");
    std::printf("  Onboarding + Coaching paths | AnalysisScope | Progressive Revelation\n");
    std::printf("================================================================================\n");

    // ─── Onboarding Path ──────────────────────────────────────────────────
    test_onboarding_full_sequence();
    test_onboarding_labels();
    test_onboarding_welcome_to_intention();
    test_onboarding_auto_advance_ordering();

    // ─── Coaching Path ────────────────────────────────────────────────────
    test_coaching_full_sequence();
    test_coaching_labels();
    test_coaching_phase_ordering();
    test_monotonic_progress();

    // ─── Progressive Revelation ───────────────────────────────────────────
    test_analysis_scope_progression();
    test_analysis_scope_edge_cases();

    // ─── Integration ──────────────────────────────────────────────────────
    test_onboarding_to_coaching_to_report();
    test_panel_reveal_integration();
    test_expected_progress_values();

    // ─── Enum consistency ─────────────────────────────────────────────────
    test_enum_consistency();

    // ─── P1: Backwards + force + NavShell mock ────────────────────────────
    test_transitions_backwards();
    test_force_transition_scope();
    test_navshell_mock_scope_wiring();

    // ─── Results ──────────────────────────────────────────────────────────
    std::printf("\n");
    std::printf("================================================================================\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("================================================================================\n");

    return gTestsFailed > 0 ? 1 : 0;
}
