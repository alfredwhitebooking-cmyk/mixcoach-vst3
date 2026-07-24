// ═══════════════════════════════════════════════════════════════════════════
//  TestSceneManager — Tests del director de la experiencia (Fase 3)
//
//  Valida:
//   - SceneId enum: labels, count, ordering
//   - DirectorEvent enum: labels
//   - SceneManager::processEvent(): transiciones válidas, reglas ignoradas
//   - SceneManager::forceTransition(): transiciones forzadas
//   - SceneManager::resetToWelcome(): reset a Welcome
//   - Builders de escenas: coachMessage, sidebar, panels, celebrate
//   - PanelSceneInfo: visibilidad correcta por escena
//   - Consultas: isPreFullUI, isCoachingScene, isRefinementScene, getAutoReturnTimeout
// ═══════════════════════════════════════════════════════════════════════════

#include "../Source/MixCoach/engine/SceneManager.h"
#include "../Source/MixCoach/engine/PanelRevealManager.h"
#include <cstdio>
#include <cstring>
#include <cmath>

using namespace mixcoach;

// ─── Test runner ───────────────────────────────────────────────────────────
static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        ++gTestsFailed; \
    } else { \
        ++gTestsPassed; \
    } \
} while(0)

#define TEST_EQ(name, a, b) TEST(name, (a) == (b))
#define TEST_STR(name, a, b) TEST(name, std::strcmp((a), (b)) == 0)
#define TEST_LT(name, a, b) TEST(name, (a) < (b))

// ═══════════════════════════════════════════════════════════════════════════════
//  1. SceneId enum — labels, count, ordering
// ═══════════════════════════════════════════════════════════════════════════════

static void testSceneIdLabels()
{
    TEST_STR("Welcome label",    sceneIdLabel(SceneId::Welcome),           "Welcome");
    TEST_STR("Onboarding label", sceneIdLabel(SceneId::Onboarding),        "Onboarding");
    TEST_STR("ReferenceLoad label", sceneIdLabel(SceneId::ReferenceLoad),  "ReferenceLoad");
    TEST_STR("ReferenceAnalysis label", sceneIdLabel(SceneId::ReferenceAnalysis), "ReferenceAnalysis");
    TEST_STR("MixMapReview label", sceneIdLabel(SceneId::MixMapReview),    "MixMapReview");
    TEST_STR("SetupComplete label", sceneIdLabel(SceneId::SetupComplete),  "SetupComplete");
    TEST_STR("Coaching label",   sceneIdLabel(SceneId::Coaching),          "Coaching");
    TEST_STR("ToolInFocus label", sceneIdLabel(SceneId::ToolInFocus),      "ToolInFocus");
    TEST_STR("Refinement label", sceneIdLabel(SceneId::Refinement),        "Refinement");
    TEST_STR("RefinementTool label", sceneIdLabel(SceneId::RefinementTool),"RefinementTool");
    TEST_STR("SessionEnd label", sceneIdLabel(SceneId::SessionEnd),        "SessionEnd");
    TEST_STR("ReportView label", sceneIdLabel(SceneId::ReportView),        "ReportView");
    TEST_STR("Unknown label",    sceneIdLabel(static_cast<SceneId>(99)),   "Unknown");

    // Verify Count is correct
    TEST_EQ("SceneId Count", static_cast<int>(SceneId::Count), 12);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  2. DirectorEvent enum — labels
// ═══════════════════════════════════════════════════════════════════════════════

static void testDirectorEventLabels()
{
    TEST_STR("CoachMessage label",
             directorEventTypeLabel(DirectorEvent::Type::CoachMessage),      "CoachMessage");
    TEST_STR("PhaseChanged label",
             directorEventTypeLabel(DirectorEvent::Type::PhaseChanged),      "PhaseChanged");
    TEST_STR("MixScoreChanged label",
             directorEventTypeLabel(DirectorEvent::Type::MixScoreChanged),   "MixScoreChanged");
    TEST_STR("ReferenceLoaded label",
             directorEventTypeLabel(DirectorEvent::Type::ReferenceLoaded),   "ReferenceLoaded");
    TEST_STR("UserInteracted label",
             directorEventTypeLabel(DirectorEvent::Type::UserInteracted),    "UserInteracted");
    TEST_STR("UserIdle label",
             directorEventTypeLabel(DirectorEvent::Type::UserIdle),          "UserIdle");
    TEST_STR("Unknown label",
             directorEventTypeLabel(static_cast<DirectorEvent::Type>(99)),   "Unknown");
}

// ═══════════════════════════════════════════════════════════════════════════════
//  3. Escenas iniciales — Welcome
// ═══════════════════════════════════════════════════════════════════════════════

static void testInitialState()
{
    SceneManager sm;

    // Default scene is Welcome
    TEST_EQ("Initial scene is Welcome",
            sm.getCurrentScene(), SceneId::Welcome);

    // Welcome has coachMessage
    auto def = sm.getCurrentSceneDef();
    TEST_EQ("Welcome has panels",
            def.panels.empty(), false);
    TEST_EQ("Welcome coachMessage not empty",
            def.coachMessage.isEmpty(), false);
    TEST_EQ("Welcome sidebar disabled",
            def.sidebarEnabled, false);
    TEST_EQ("Welcome no auto-return",
            def.autoReturnTimeout, 0.0f);

    // Welcome: solo Coach focused, resto hidden
    for (const auto& panel : def.panels) {
        if (panel.id == PanelId::Coach) {
            TEST_EQ("Welcome Coach is Focused",
                    panel.visibility == PanelVisibility::Focused, true);
        } else {
            TEST_EQ("Welcome non-Coach is Hidden",
                    panel.visibility == PanelVisibility::Hidden, true);
        }
    }

    // isPreFullUI: true for Welcome
    TEST_EQ("Welcome is pre-FullUI", sm.isPreFullUI(), true);
    TEST_EQ("Welcome is not coaching", sm.isCoachingScene(), false);
    TEST_EQ("Welcome is not refinement", sm.isRefinementScene(), false);
    TEST_EQ("Welcome auto-return timeout", sm.getAutoReturnTimeout(), 0.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  4. Transition matrix — Welcome → Onboarding
// ═══════════════════════════════════════════════════════════════════════════════

static void testTransitionWelcomeToOnboarding()
{
    SceneManager sm;
    TEST_EQ("Initial: Welcome", sm.getCurrentScene(), SceneId::Welcome);

    // CoachMessage → Onboarding
    auto def = sm.processEvent({DirectorEvent::Type::CoachMessage, "Hola"});
    TEST_EQ("After CoachMessage: Onboarding",
            sm.getCurrentScene(), SceneId::Onboarding);
    TEST_EQ("Onboarding has panels", def.panels.empty(), false);
    TEST_EQ("Onboarding sidebar disabled", def.sidebarEnabled, false);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  5. Transition matrix — Onboarding → ReferenceLoad
// ═══════════════════════════════════════════════════════════════════════════════

static void testTransitionOnboardingToReferenceLoad()
{
    SceneManager sm;
    sm.processEvent({DirectorEvent::Type::CoachMessage, "Hola"}); // → Onboarding
    TEST_EQ("Before: Onboarding", sm.getCurrentScene(), SceneId::Onboarding);

    // ReferenceLoaded → ReferenceLoad
    auto def = sm.processEvent({DirectorEvent::Type::ReferenceLoaded});
    TEST_EQ("After ReferenceLoaded: ReferenceLoad",
            sm.getCurrentScene(), SceneId::ReferenceLoad);

    // ReferenceLoad: Coach + Reference visible
    for (const auto& panel : def.panels) {
        if (panel.id == PanelId::Coach) {
            TEST_EQ("ReferenceLoad Coach is Focused",
                    panel.visibility == PanelVisibility::Focused, true);
        } else if (panel.id == PanelId::Reference) {
            TEST_EQ("ReferenceLoad Reference is Visible",
                    panel.visibility == PanelVisibility::Visible, true);
            TEST_EQ("ReferenceLoad Reference context not empty",
                    panel.context.isEmpty(), false);
        } else {
            TEST_EQ("ReferenceLoad non-C/Ref is Hidden",
                    panel.visibility == PanelVisibility::Hidden, true);
        }
    }

    TEST_EQ("ReferenceLoad is pre-FullUI", sm.isPreFullUI(), true);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  6. Transition matrix — ReferenceLoad → ReferenceAnalysis
// ═══════════════════════════════════════════════════════════════════════════════

static void testTransitionReferenceLoadToAnalysis()
{
    SceneManager sm;
    sm.processEvent({DirectorEvent::Type::CoachMessage, "Hola"});
    sm.processEvent({DirectorEvent::Type::ReferenceLoaded});

    // ReferenceLoaded → ReferenceAnalysis
    auto def = sm.processEvent({DirectorEvent::Type::ReferenceLoaded});
    TEST_EQ("After second ReferenceLoaded: ReferenceAnalysis",
            sm.getCurrentScene(), SceneId::ReferenceAnalysis);
    TEST_EQ("ReferenceAnalysis reference visible",
            def.panels.size() > 2, true);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  7. Transition matrix — Full flow: Welcome → ... → Coaching
// ═══════════════════════════════════════════════════════════════════════════════

static void testFullFlowToCoaching()
{
    SceneManager sm;

    // Welcome → Onboarding
    sm.processEvent({DirectorEvent::Type::CoachMessage, "Hola"});
    TEST_EQ("Step 1: Onboarding", sm.getCurrentScene(), SceneId::Onboarding);

    // Onboarding → ReferenceLoad
    sm.processEvent({DirectorEvent::Type::ReferenceLoaded});
    TEST_EQ("Step 2: ReferenceLoad", sm.getCurrentScene(), SceneId::ReferenceLoad);

    // ReferenceLoad → ReferenceAnalysis
    sm.processEvent({DirectorEvent::Type::ReferenceLoaded});
    TEST_EQ("Step 3: ReferenceAnalysis", sm.getCurrentScene(), SceneId::ReferenceAnalysis);

    // ReferenceAnalysis → MixMapReview (via CoachMessage)
    sm.processEvent({DirectorEvent::Type::CoachMessage, "Confirma el mapa"});
    TEST_EQ("Step 4: MixMapReview", sm.getCurrentScene(), SceneId::MixMapReview);

    // MixMap → SetupComplete
    sm.processEvent({DirectorEvent::Type::MixMapConfirmed});
    TEST_EQ("Step 5: SetupComplete", sm.getCurrentScene(), SceneId::SetupComplete);

    // SetupComplete → Coaching
    auto def = sm.processEvent({DirectorEvent::Type::CoachMessage, "Empecemos"});
    TEST_EQ("Step 6: Coaching", sm.getCurrentScene(), SceneId::Coaching);

    // Coaching: sidebar enabled, no auto-return, celebrate false
    TEST_EQ("Coaching sidebar enabled", def.sidebarEnabled, true);
    TEST_EQ("Coaching no auto-return", def.autoReturnTimeout, 0.0f);
    TEST_EQ("Coaching not pre-FullUI", sm.isPreFullUI(), false);
    TEST_EQ("Coaching is coaching scene", sm.isCoachingScene(), true);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  8. Transition: Coaching → ToolInFocus → Coaching (auto-return)
// ═══════════════════════════════════════════════════════════════════════════════

static void testToolInFocusAndAutoReturn()
{
    SceneManager sm;
    // Fast-forward to Coaching
    sm.processEvent({DirectorEvent::Type::CoachMessage, "Hola"});
    sm.processEvent({DirectorEvent::Type::ReferenceLoaded});
    sm.processEvent({DirectorEvent::Type::ReferenceLoaded});
    sm.processEvent({DirectorEvent::Type::CoachMessage, "Confirma"});
    sm.processEvent({DirectorEvent::Type::MixMapConfirmed});
    sm.processEvent({DirectorEvent::Type::CoachMessage, "Empecemos"});
    TEST_EQ("Pre: Coaching", sm.getCurrentScene(), SceneId::Coaching);

    // CoachCommand → ToolInFocus
    auto def = sm.processEvent({DirectorEvent::Type::CoachCommand, "switch_tab(tools)"});
    TEST_EQ("After CoachCommand: ToolInFocus",
            sm.getCurrentScene(), SceneId::ToolInFocus);
    TEST_EQ("ToolInFocus has auto-return",
            def.autoReturnTimeout > 0.0f, true);

    // TimerTick → Coaching (auto-return completed)
    def = sm.processEvent({DirectorEvent::Type::TimerTick});
    TEST_EQ("After TimerTick: Coaching",
            sm.getCurrentScene(), SceneId::Coaching);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  9. Transition: Coaching → Refinement
// ═══════════════════════════════════════════════════════════════════════════════

static void testTransitionToRefinement()
{
    SceneManager sm;
    // Fast-forward to Coaching
    sm.processEvent({DirectorEvent::Type::CoachMessage, "Hola"});
    sm.processEvent({DirectorEvent::Type::ReferenceLoaded});
    sm.processEvent({DirectorEvent::Type::ReferenceLoaded});
    sm.processEvent({DirectorEvent::Type::CoachMessage, "Confirma"});
    sm.processEvent({DirectorEvent::Type::MixMapConfirmed});
    sm.processEvent({DirectorEvent::Type::CoachMessage, "Empecemos"});

    // MixScoreChanged (≥ 70) → Refinement
    auto def = sm.processEvent({DirectorEvent::Type::MixScoreChanged, "", 72.0f});
    TEST_EQ("After MixScore 72: Refinement",
            sm.getCurrentScene(), SceneId::Refinement);
    TEST_EQ("Refinement sidebar enabled", def.sidebarEnabled, true);
    TEST_EQ("Refinement not pre-FullUI", sm.isPreFullUI(), false);
    TEST_EQ("Refinement is refinement scene", sm.isRefinementScene(), true);
    TEST_EQ("Refinement has celebration", def.celebrate, true);
    TEST_EQ("Refinement has achievement", def.achievement.isEmpty(), false);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  10. Transition: Refinement → SessionEnd → ReportView → Welcome (ciclo completo)
// ═══════════════════════════════════════════════════════════════════════════════

static void testFullCycleToReport()
{
    SceneManager sm;
    // Fast-forward to Refinement
    sm.processEvent({DirectorEvent::Type::CoachMessage, "Hola"});
    sm.processEvent({DirectorEvent::Type::ReferenceLoaded});
    sm.processEvent({DirectorEvent::Type::ReferenceLoaded});
    sm.processEvent({DirectorEvent::Type::CoachMessage, "Confirma"});
    sm.processEvent({DirectorEvent::Type::MixMapConfirmed});
    sm.processEvent({DirectorEvent::Type::CoachMessage, "Empecemos"});
    sm.processEvent({DirectorEvent::Type::MixScoreChanged, "", 72.0f});
    TEST_EQ("Pre: Refinement", sm.getCurrentScene(), SceneId::Refinement);

    // PhaseChanged → SessionEnd
    auto def = sm.processEvent({DirectorEvent::Type::PhaseChanged});
    TEST_EQ("After PhaseChanged: SessionEnd",
            sm.getCurrentScene(), SceneId::SessionEnd);
    TEST_EQ("SessionEnd sidebar disabled", def.sidebarEnabled, false);
    TEST_EQ("SessionEnd has celebration", def.celebrate, true);

    // CoachMessage → ReportView
    def = sm.processEvent({DirectorEvent::Type::CoachMessage, "Genera reporte"});
    TEST_EQ("After CoachMessage: ReportView",
            sm.getCurrentScene(), SceneId::ReportView);
    TEST_EQ("ReportView Report is Focused", true,
            [&def]() {
                for (const auto& p : def.panels)
                    if (p.id == PanelId::Report && p.visibility == PanelVisibility::Focused)
                        return true;
                return false;
            }());

    // CoachMessage → Welcome (nuevo ciclo)
    def = sm.processEvent({DirectorEvent::Type::CoachMessage, "Nueva sesión"});
    TEST_EQ("After CoachMessage: Welcome (new cycle)",
            sm.getCurrentScene(), SceneId::Welcome);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  11. Ignored transitions — eventos que no matchean no cambian la escena
// ═══════════════════════════════════════════════════════════════════════════════

static void testIgnoredTransitions()
{
    SceneManager sm;
    TEST_EQ("Start: Welcome", sm.getCurrentScene(), SceneId::Welcome);

    // Welcome + PhaseChanged → no rule → stay
    sm.processEvent({DirectorEvent::Type::PhaseChanged});
    TEST_EQ("Welcome + PhaseChanged: still Welcome",
            sm.getCurrentScene(), SceneId::Welcome);

    // Welcome + CorrectionApplied → no rule → stay
    sm.processEvent({DirectorEvent::Type::CorrectionApplied});
    TEST_EQ("Welcome + CorrectionApplied: still Welcome",
            sm.getCurrentScene(), SceneId::Welcome);

    // Welcome + MixMapConfirmed → no rule → stay
    sm.processEvent({DirectorEvent::Type::MixMapConfirmed});
    TEST_EQ("Welcome + MixMapConfirmed: still Welcome",
            sm.getCurrentScene(), SceneId::Welcome);

    // Welcome + StreakMilestone → no rule → stay
    sm.processEvent({DirectorEvent::Type::StreakMilestone});
    TEST_EQ("Welcome + StreakMilestone: still Welcome",
            sm.getCurrentScene(), SceneId::Welcome);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  12. forceTransition — Forzar escena sin evento
// ═══════════════════════════════════════════════════════════════════════════════

static void testForceTransition()
{
    SceneManager sm;
    TEST_EQ("Start: Welcome", sm.getCurrentScene(), SceneId::Welcome);

    // Force → Coaching
    auto def = sm.forceTransition(SceneId::Coaching);
    TEST_EQ("Forced: Coaching", sm.getCurrentScene(), SceneId::Coaching);
    TEST_EQ("Forced Coaching sidebar enabled", def.sidebarEnabled, true);

    // Force → ReportView
    def = sm.forceTransition(SceneId::ReportView);
    TEST_EQ("Forced: ReportView", sm.getCurrentScene(), SceneId::ReportView);
    TEST_EQ("Forced ReportView no auto-return", def.autoReturnTimeout, 0.0f);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  13. resetToWelcome — Reset
// ═══════════════════════════════════════════════════════════════════════════════

static void testResetToWelcome()
{
    SceneManager sm;
    sm.forceTransition(SceneId::Coaching);
    TEST_EQ("Before reset: not Welcome",
            sm.getCurrentScene() == SceneId::Welcome, false);

    auto def = sm.resetToWelcome();
    TEST_EQ("After reset: Welcome",
            sm.getCurrentScene(), SceneId::Welcome);
    TEST_EQ("Reset sidebar disabled", def.sidebarEnabled, false);
    TEST_EQ("Reset no auto-return", def.autoReturnTimeout, 0.0f);
    TEST_EQ("Reset no celebration", def.celebrate, false);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  14. Scene builders — verificar que cada escena tiene datos válidos
// ═══════════════════════════════════════════════════════════════════════════════

static void testSceneBuildersValidity()
{
    // Verificar que todas las escenas se construyen sin crash
    // y tienen datos razonables
    auto checkScene = [](SceneId id, bool expectSidebar) -> bool {
        SceneManager sm;
        auto def = sm.forceTransition(id);

        // Cada escena debe tener una definición válida
        if (def.panels.empty()) {
            std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: Scene %s has no panels\n", sceneIdLabel(id));
            ++gTestsFailed;
            return false;
        }

        // Sidebar debe coincidir
        if (def.sidebarEnabled != expectSidebar) {
            std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: Scene %s sidebar mismatch (expected=%d, got=%d)\n",
                         sceneIdLabel(id), expectSidebar, def.sidebarEnabled);
            ++gTestsFailed;
            return false;
        }

        // Cada panel debe tener un PanelId válido
        for (const auto& p : def.panels) {
            if (p.id == PanelId::Count) {
                std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: Scene %s has Count panel\n", sceneIdLabel(id));
                ++gTestsFailed;
                return false;
            }
        }

        ++gTestsPassed;
        return true;
    };

    // Track whether any check failed
    bool allPassed = true;
    auto runCheck = [&](SceneId id, bool expectSidebar) {
        if (!checkScene(id, expectSidebar)) allPassed = false;
    };

    runCheck(SceneId::Welcome,           false);
    runCheck(SceneId::Onboarding,        false);
    runCheck(SceneId::ReferenceLoad,     false);
    runCheck(SceneId::ReferenceAnalysis, false);
    runCheck(SceneId::MixMapReview,      false);
    runCheck(SceneId::SetupComplete,     true);
    runCheck(SceneId::Coaching,          true);
    runCheck(SceneId::ToolInFocus,       true);
    runCheck(SceneId::Refinement,        true);
    runCheck(SceneId::RefinementTool,    true);
    runCheck(SceneId::SessionEnd,        false);
    runCheck(SceneId::ReportView,        false);

    if (!allPassed) {
        // testSceneBuildersValidity will be counted as having failures
        // The individual checkScene calls already incremented gTestsFailed
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  15. Panel count consistency — cada escena tiene 7 paneles (1 por PanelId)
// ═══════════════════════════════════════════════════════════════════════════════

static void testPanelCountConsistency()
{
    auto checkPanels = [](SceneId id, int expectedCount) {
        SceneManager sm;
        auto def = sm.forceTransition(id);
        if (static_cast<int>(def.panels.size()) != expectedCount) {
            std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: Scene %s has %zu panels, expected %d\n",
                         sceneIdLabel(id), def.panels.size(), expectedCount);
            ++gTestsFailed;
            return;
        }
        ++gTestsPassed;
    };

    // Cada escena debe tener 7 paneles (1 por cada PanelId)
    int expected = static_cast<int>(PanelId::Count);
    checkPanels(SceneId::Welcome,           expected);
    checkPanels(SceneId::Onboarding,        expected);
    checkPanels(SceneId::ReferenceLoad,     expected);
    checkPanels(SceneId::ReferenceAnalysis, expected);
    checkPanels(SceneId::MixMapReview,      expected);
    checkPanels(SceneId::SetupComplete,     expected);
    checkPanels(SceneId::Coaching,          expected);
    checkPanels(SceneId::ToolInFocus,       expected);
    checkPanels(SceneId::Refinement,        expected);
    checkPanels(SceneId::RefinementTool,    expected);
    checkPanels(SceneId::SessionEnd,        expected);
    checkPanels(SceneId::ReportView,        expected);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════════

int main()
{
    std::printf("╔══════════════════════════════════════════════════════════╗\n");
    std::printf("║  TestSceneManager — Fase 3: Director de la Experiencia ║\n");
    std::printf("╚══════════════════════════════════════════════════════════╝\n\n");

    // ─── 1. Enums ──────────────────────────────────────────────────────────
    std::printf("[1/9] SceneId enum labels and count...\n");
    testSceneIdLabels();
    std::printf("[2/9] DirectorEvent enum labels...\n");
    testDirectorEventLabels();

    // ─── 2. Escena inicial ─────────────────────────────────────────────────
    std::printf("[3/9] Welcome initial state...\n");
    testInitialState();

    // ─── 3. Transiciones ──────────────────────────────────────────────────
    std::printf("[4/9] Welcome → Onboarding transition...\n");
    testTransitionWelcomeToOnboarding();

    std::printf("[5/9] Onboarding → ReferenceLoad transition...\n");
    testTransitionOnboardingToReferenceLoad();

    std::printf("[6/9] ReferenceLoad → ReferenceAnalysis transition...\n");
    testTransitionReferenceLoadToAnalysis();

    std::printf("[7/9] Full flow to Coaching (6 steps)...\n");
    testFullFlowToCoaching();

    std::printf("[8/9] ToolInFocus + Refinement + full cycle to Report...\n");
    testToolInFocusAndAutoReturn();
    testTransitionToRefinement();
    testFullCycleToReport();

    // ─── 4. Edge cases ────────────────────────────────────────────────────
    std::printf("[9/9] Edge cases: ignored transitions, force, reset, builders...\n");
    testIgnoredTransitions();
    testForceTransition();
    testResetToWelcome();
    testSceneBuildersValidity();
    testPanelCountConsistency();

    // ─── Results ───────────────────────────────────────────────────────────
    std::printf("\n");
    std::printf("╔══════════════════════════════════════════════════╗\n");
    std::printf("║  RESULTS                                      ║\n");
    std::printf("╠══════════════════════════════════════════════════╣\n");
    std::printf("║  Passed:  %3d                                 ║\n", gTestsPassed);
    std::printf("║  Failed:  %3d                                 ║\n", gTestsFailed);
    std::printf("╚══════════════════════════════════════════════════╝\n");

    return gTestsFailed > 0 ? 1 : 0;
}
