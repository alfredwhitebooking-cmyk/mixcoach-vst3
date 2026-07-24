// ═══════════════════════════════════════════════════════════════════════════
//  TestExperienceManager.cpp — Unit tests para ExperienceManager
//  (celebrate() + updateAnimations() lifecycle)
//
//  ESTRATEGIA:
//  - Incluye TabBarComponent.h real para que el tipo TabBarComponent::Tab
//    tenga el MISMO mangling que el usado por ExperienceManager.obj.
//  - Mock NavigationShell define los 4 métodos que ExperienceManager.obj
//    necesita fuera del cuerpo de la clase (no-inline, COMDAT separados).
//  - MixCoachEngine.lib se linkea pero el linker encuentra NavigationShell
//    symbols en TestExperienceManager.obj ANTES que en la lib.
// ═══════════════════════════════════════════════════════════════════════════
//
//  Build: cmake --build build --config Release --target TestExperienceManager
//  Run:   build/tests/Release/TestExperienceManager.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cstdlib>
#include <string>
#include "../Source/MixCoach/UI/CoachRoomState.h"
#include "../Source/MixCoach/UI/TabBarComponent.h"
#include "../Source/MixCoach/engine/PanelRevealManager.h"
#include "../Source/MixCoach/engine/ExperienceManager.h"

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

// ═══════════════════════════════════════════════════════════════════════════
//  MOCK NavigationShell
//  Define EXACTAMENTE los métodos que ExperienceManager.obj necesita.
//  Usa el TabBarComponent real (incluido arriba) para mangling correcto.
//  NO inline — definiciones fuera del cuerpo de la clase.
// ═══════════════════════════════════════════════════════════════════════════

namespace mixcoach {    // NavigationShell — mock con métodos definidos FUERA de la clase
    // Debe exponer TODOS los métodos que ExperienceManager llama,
    // incluyendo postUIEvent() (añadido en Fase 2).
    class NavigationShell {
    public:
        PanelRevealManager revealManager_;
        PanelRevealManager& getRevealManager();

        void refreshReport();
        void revealPanel(PanelId panel);
        void setCoachRoomState(CoachRoomState state);
        void postUIEvent(const juce::String& icon, const juce::String& msg);
        void setAvatarExpression(AvatarExpression exp);

    private:
        void switchContent(TabBarComponent::Tab tab);
    };

    // ─── Definiciones NO inline ─────────────────────────────────────
    PanelRevealManager& NavigationShell::getRevealManager() {
        return revealManager_;
    }

    void NavigationShell::refreshReport() {
        // mock — no-op
    }

    void NavigationShell::revealPanel(PanelId) {
        // mock — no-op
    }

    void NavigationShell::setCoachRoomState(CoachRoomState) {
        // mock — no-op
    }

    void NavigationShell::switchContent(TabBarComponent::Tab) {
        // mock — no-op
    }

    void NavigationShell::postUIEvent(const juce::String& /*icon*/, const juce::String& /*msg*/) {
        // mock — no-op
    }

    void NavigationShell::setAvatarExpression(AvatarExpression /*exp*/) {
        // mock — no-op
    }

} // namespace mixcoach

using namespace mixcoach;

// ═══════════════════════════════════════════════════════════════════════════
//  Tests — Solo celebration cycle (no toca navShell_)
// ═══════════════════════════════════════════════════════════════════════════

static void run_tests() {
    std::printf("\n── Construcción básica ──\n");

    NavigationShell navShell;
    PanelRevealManager& revealMgr = navShell.revealManager_;
    ExperienceManager em(navShell, revealMgr);

    TEST("isWired is false initially", !em.isWired());
    TEST("isAnimating is false initially", !em.isAnimating());

    // ─── celebrate + updateAnimations lifecycle ───
    std::printf("\n── Celebration lifecycle ──\n");

    em.celebrate("Test!");
    TEST("isAnimating true after celebrate", em.isAnimating());

    // Advance halfway
    for (int i = 0; i < 15; ++i) {
        em.updateAnimations();
        TEST("Still animating at frame " + std::to_string(i), em.isAnimating());
    }

    // Complete animation
    for (int i = 15; i < 30; ++i)
        em.updateAnimations();
    TEST("isAnimating false after 30 frames", !em.isAnimating());

    // Second celebration
    em.celebrate("Second!");
    TEST("isAnimating true after second celebrate", em.isAnimating());
    for (int i = 0; i < 30; ++i)
        em.updateAnimations();
    TEST("isAnimating false after second completion", !em.isAnimating());

    // ─── Duplicate ignore ───
    std::printf("\n── Duplicate celebration ignore ──\n");

    em.celebrate("First");
    TEST("isAnimating true after first celebrate", em.isAnimating());
    em.celebrate("Second - should be ignored");
    for (int i = 0; i < 30; ++i)
        em.updateAnimations();
    TEST("isAnimating false after completing", !em.isAnimating());

    // ─── Multiple cycles ───
    std::printf("\n── Multiple cycles ──\n");

    for (int cycle = 0; cycle < 5; ++cycle) {
        em.celebrate("Cycle " + std::to_string(cycle));
        TEST("Animating at cycle start " + std::to_string(cycle), em.isAnimating());
        for (int i = 0; i < 30; ++i)
            em.updateAnimations();
        TEST("Not animating after cycle " + std::to_string(cycle), !em.isAnimating());
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════

int main() {
    std::printf("  ExperienceManager Unit Tests\n");
    std::printf("  (celebration lifecycle only)\n");
    std::printf("================================================================================\n");

    run_tests();

    std::printf("\n================================================================================\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("================================================================================\n");

    return gTestsFailed > 0 ? 1 : 0;
}
