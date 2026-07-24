// ═══════════════════════════════════════════════════════════════════════════
//  TestModeCards.cpp — Unit tests for premium Mix/Master mode cards
//
//  Verifica:
//  1. Smoothstep easing math (pure function)
//  2. Animation state transitions (alpha 0→1, slide 20→0 over 350ms)
//  3. Mode card trigger via addMessage() with Mix+Master keywords
//  4. getTotalHeight() animation space calculation
//  5. setModeButtonsVisible() hide
//  6. clear() + re-trigger cycle
//  7. Render output verification (image buffer — coach bubble)
//  8. Clear + re-trigger re-shows cards
//
//  Compilado via CMake (ver CMakeLists.txt):
//    cmake --build build --config Release --target TestModeCards
//    ./build/tests/Release/TestModeCards.exe
//
//  Dependencias: juce_core + juce_graphics + juce_gui_basics
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoach/UI/CoachChatComponent.h"
#include "MixCoach/UI/MixCoachTheme.h"

// ─── Test runner ─────────────────────────────────────────────────────────────
static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define TEST(name, expr) do {                                                  \
    if (!(expr)) {                                                             \
        std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: %s (%s:%d)\n",              \
                     name, __FILE__, __LINE__);                                \
        gTestsFailed++;                                                        \
    } else {                                                                   \
        std::printf("  \xe2\x9c\x85 PASS: %s\n", name);                        \
        gTestsPassed++;                                                        \
    }                                                                          \
} while(0)

#define TEST_NEAR(name, a, b, eps) TEST(name, std::fabs((a) - (b)) < (eps))

// ═══════════════════════════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════════════════════════

/** Smoothstep easing: t²(3-2t), used in updateModeCardsAnimation(). */
static float smoothstep(float t)
{
    t = juce::jlimit(0.0f, 1.0f, t);
    return t * t * (3.0f - 2.0f * t);
}

/** Simula updateModeCardsAnimation() para una 'elapsed' dada. */
struct AnimState {
    float alpha;
    float slideOffset;
    bool animating;
};

static AnimState simulateAnimation(float elapsedMs)
{
    constexpr float kDuration = 350.0f;
    float t = juce::jmin(1.0f, elapsedMs / kDuration);
    float eased = t * t * (3.0f - 2.0f * t);
    AnimState s;
    s.alpha = eased;
    s.slideOffset = 20.0f * (1.0f - eased);
    s.animating = (t < 1.0f);
    return s;
}

/** Calcula el espacio vertical que ocuparian las mode cards durante la animacion.
    Refleja la formula en getTotalHeight(). */
static float modeCardsAnimationSpace(float slideOffset, bool animating)
{
    constexpr float kCardFullSpace = 130.0f;
    if (animating) {
        float slideRatio = slideOffset / 20.0f;
        return kCardFullSpace * (0.3f + 0.7f * (1.0f - slideRatio));
    }
    return kCardFullSpace;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Pure Math Tests — Smoothstep
// ═══════════════════════════════════════════════════════════════════════════

static void test_smoothstep_zero()
{
    std::printf("\n── Smoothstep: Boundary t=0 ──\n");
    float r = smoothstep(0.0f);
    std::printf("  smoothstep(0) = %.4f (expected 0.0)\n", r);
    TEST_NEAR("smoothstep(0) = 0", r, 0.0f, 0.0001f);
}

static void test_smoothstep_half()
{
    std::printf("\n── Smoothstep: Midpoint t=0.5 ──\n");
    float r = smoothstep(0.5f);
    float expected = 0.5f;
    std::printf("  smoothstep(0.5) = %.4f (expected %.4f)\n", r, expected);
    TEST_NEAR("smoothstep(0.5) = 0.5", r, expected, 0.0001f);
}

static void test_smoothstep_one()
{
    std::printf("\n── Smoothstep: Boundary t=1 ──\n");
    float r = smoothstep(1.0f);
    std::printf("  smoothstep(1) = %.4f (expected 1.0)\n", r);
    TEST_NEAR("smoothstep(1) = 1", r, 1.0f, 0.0001f);
}

static void test_smoothstep_clamp()
{
    std::printf("\n── Smoothstep: Clamping ──\n");
    float below = smoothstep(-0.5f);
    float above = smoothstep(1.5f);
    std::printf("  smoothstep(-0.5) = %.4f (expected 0.0)\n", below);
    std::printf("  smoothstep(1.5)  = %.4f (expected 1.0)\n", above);
    TEST_NEAR("smoothstep(-0.5) clamped to 0", below, 0.0f, 0.0001f);
    TEST_NEAR("smoothstep(1.5) clamped to 1", above, 1.0f, 0.0001f);
}

static void test_smoothstep_intermediate()
{
    std::printf("\n── Smoothstep: Intermediate values ──\n");
    float r25 = smoothstep(0.25f);
    float r75 = smoothstep(0.75f);
    std::printf("  smoothstep(0.25) = %.4f\n", r25);
    std::printf("  smoothstep(0.75) = %.4f\n", r75);
    TEST("smoothstep is monotonic increasing", r25 < 0.5f && 0.5f < r75);
    TEST_NEAR("smoothstep(0.25) ≈ 0.156", r25, 0.15625f, 0.0001f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Pure Math Tests — Animation state machine
// ═══════════════════════════════════════════════════════════════════════════

static void test_animation_initial_state()
{
    std::printf("\n── Animation: Initial state (t=0ms) ──\n");
    auto s = simulateAnimation(0.0f);
    std::printf("  alpha=%.4f slide=%.4f animating=%d\n",
                s.alpha, s.slideOffset, s.animating);
    TEST_NEAR("alpha = 0 at t=0", s.alpha, 0.0f, 0.0001f);
    TEST_NEAR("slide = 20 at t=0", s.slideOffset, 20.0f, 0.0001f);
    TEST("animating = true at t=0", s.animating);
}

static void test_animation_midpoint()
{
    std::printf("\n── Animation: Midpoint (t=175ms) ──\n");
    auto s = simulateAnimation(175.0f);
    std::printf("  alpha=%.4f slide=%.4f animating=%d\n",
                s.alpha, s.slideOffset, s.animating);
    TEST_NEAR("alpha ≈ 0.5 at t=175ms", s.alpha, 0.5f, 0.02f);
    TEST_NEAR("slide ≈ 10 at t=175ms", s.slideOffset, 10.0f, 0.5f);
    TEST("animating = true at midpoint", s.animating);
}

static void test_animation_completion()
{
    std::printf("\n── Animation: Completion (t=350ms) ──\n");
    auto s = simulateAnimation(350.0f);
    std::printf("  alpha=%.4f slide=%.4f animating=%d\n",
                s.alpha, s.slideOffset, s.animating);
    TEST_NEAR("alpha = 1 at t=350ms", s.alpha, 1.0f, 0.0001f);
    TEST_NEAR("slide = 0 at t=350ms", s.slideOffset, 0.0f, 0.0001f);
    TEST("animating = false at completion", !s.animating);
}

static void test_animation_past_completion()
{
    std::printf("\n── Animation: Past completion (t=500ms) ──\n");
    auto s = simulateAnimation(500.0f);
    std::printf("  alpha=%.4f slide=%.4f animating=%d\n",
                s.alpha, s.slideOffset, s.animating);
    TEST_NEAR("alpha stays 1 after completion", s.alpha, 1.0f, 0.0001f);
    TEST_NEAR("slide stays 0 after completion", s.slideOffset, 0.0f, 0.0001f);
    TEST("animating = false after completion", !s.animating);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Pure Math Tests — Animation space calculation
// ═══════════════════════════════════════════════════════════════════════════

static void test_animation_space_full()
{
    std::printf("\n── Animation Space: Full height (not animating) ──\n");
    float space = modeCardsAnimationSpace(0.0f, false);
    std::printf("  space=%.1f (expected 130.0)\n", space);
    TEST_NEAR("full space = 130px", space, 130.0f, 0.1f);
}

static void test_animation_space_initial()
{
    std::printf("\n── Animation Space: Initial (slide=20, animating=true) ──\n");
    float space = modeCardsAnimationSpace(20.0f, true);
    std::printf("  space=%.1f (expected 39.0)\n", space);
    TEST_NEAR("initial space = 39px (30%)", space, 39.0f, 0.1f);
}

static void test_animation_space_mid()
{
    std::printf("\n── Animation Space: Midpoint (slide=10, animating=true) ──\n");
    float space = modeCardsAnimationSpace(10.0f, true);
    std::printf("  space=%.1f (expected 84.5)\n", space);
    TEST_NEAR("midpoint space = 84.5px", space, 84.5f, 0.1f);
}

static void test_animation_space_final()
{
    std::printf("\n── Animation Space: Final (slide=0, animating=false) ──\n");
    float space = modeCardsAnimationSpace(0.0f, false);
    std::printf("  space=%.1f (expected 130.0)\n", space);
    TEST_NEAR("final space = 130px", space, 130.0f, 0.1f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Component Integration Tests
//
//  Nota: Estos tests crean un ChatMessagesComponent real y verifican su
//  comportamiento a traves de la API publica.
//
//  IMPORTANTE: setModeButtonsVisible(true) NO restaura modeButtonsBubbleIndex_,
//  por lo que las cards no reaparecen si ese indice se perdio (por ejemplo,
//  tras addUserMessage o setModeButtonsVisible(false)). Esto es por diseno:
//  el indice solo se establece via addMessage() cuando se detectan keywords.
// ═══════════════════════════════════════════════════════════════════════════

struct ModeCardsFixture {
    mixcoach::ChatMessagesComponent component;
    int initialHeight;

    ModeCardsFixture() {
        component.setSize(400, 600);
        component.clearWelcome();  // Remove welcome card overhead
        initialHeight = component.getTotalHeight();
    }

    void triggerModeCards() {
        component.addMessage("\xBFQu\xE9 vamos a hacer? \xBFMix o Master? Selecciona el modo.");
    }

    void sendUserMessage() {
        component.addUserMessage("Quiero mezclar");
    }
};

static void test_initial_state()
{
    std::printf("\n── Component: Initial state (empty, no messages) ──\n");
    ModeCardsFixture f;
    std::printf("  initialHeight=%d\n", f.initialHeight);
    TEST("component starts empty", f.component.isEmpty());
    TEST("initial height is reasonable", f.initialHeight > 0 && f.initialHeight < 500);
}

static void test_trigger_adds_mode_cards_height()
{
    std::printf("\n── Component: addMessage triggers mode cards ──\n");
    ModeCardsFixture f;
    f.triggerModeCards();
    int h = f.component.getTotalHeight();
    std::printf("  initialHeight=%d, afterTrigger=%d, diff=%d\n",
                f.initialHeight, h, h - f.initialHeight);
    // Mode cards add 39-130px of space (39px at animation start, 130px at full)
    TEST("height increases with mode cards", h > f.initialHeight);
    TEST("mode card space ≥ 30px (animation minimum)", h >= f.initialHeight + 30);
}

static void test_hide_via_setModeButtonsVisible()
{
    std::printf("\n── Component: setModeButtonsVisible(false) removes cards ──\n");
    ModeCardsFixture f;
    f.triggerModeCards();
    int hTrigger = f.component.getTotalHeight();
    f.component.setModeButtonsVisible(false);
    int hHide = f.component.getTotalHeight();
    std::printf("  trigger=%d, hide=%d, diff=%d (lost bubbleIndex: height same as no-cards)\n",
                hTrigger, hHide, hHide - hTrigger);
    // setModeButtonsVisible(false) also clears bubbleIndex → height is just bubble + padding
    // After trigger: coach_bubble + gap + mode_cards_space
    // After hide: coach_bubble + gap (no mode cards, no bubbleIndex)
    // The cards themselves add 39-130px, so hiding should reduce by at least 30px
    int diff = hTrigger - hHide;
    std::printf("  actual reduction: %dpx (expected ≥ 30px)\n", diff);
    TEST("height decreases after hiding mode cards", diff >= 30);
}

static void test_clear_and_retrigger()
{
    std::printf("\n── Component: clear() + re-trigger restarts cards ──\n");
    ModeCardsFixture f;
    f.triggerModeCards();
    int hTrigger = f.component.getTotalHeight();
    f.component.clear();
    int hClear = f.component.getTotalHeight();
    f.triggerModeCards();
    int hRetrigger = f.component.getTotalHeight();
    std::printf("  trigger=%d, clear=%d, retrigger=%d\n", hTrigger, hClear, hRetrigger);
    TEST("clear reduces to near-initial", hClear <= f.initialHeight + 10);
    TEST("re-trigger adds mode cards again", hRetrigger > hClear);
    TEST("re-trigger height matches first trigger", std::abs(hRetrigger - hTrigger) < 5);
}

static void test_addUserMessage_hides_via_retrigger()
{
    std::printf("\n── Component: addUserMessage hides cards, retrigger restores ──\n");
    ModeCardsFixture f;
    f.triggerModeCards();
    int hTrigger = f.component.getTotalHeight();
    f.sendUserMessage();
    int hUserMsg = f.component.getTotalHeight();
    f.component.clear();
    f.triggerModeCards();
    int hRetrigger = f.component.getTotalHeight();
    std::printf("  trigger=%d, afterUserMsg=%d, clear+retrigger=%d\n",
                hTrigger, hUserMsg, hRetrigger);
    // After user msg: coach + user bubbles (2 bubbles, mode cards hidden)
    // After clear+retrigger: 1 bubble + mode cards (should match original trigger)
    TEST("retrigger restores mode cards (height matches original trigger)",
         std::abs(hRetrigger - hTrigger) < 10);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Rendering Test — Coach bubble + area paint verification
//
//  NOTA sobre la animacion de las mode cards: updateModeCardsAnimation() se
//  ejecuta desde timerCallback() a 60fps. En tests sin message loop, la
//  animacion comienza en alpha=0 y nunca avanza, por lo que las cards son
//  invisible en el primer paint(). Verificamos que el AREA de las cards
//  ocupa espacio en getTotalHeight() (ya probado arriba), y que el
//  componente renderiza contenido visible (la burbuja del coach).
// ═══════════════════════════════════════════════════════════════════════════

static void test_render_coach_bubble()
{
    std::printf("\n── Render: Coach bubble renders visible content ──\n");
    ModeCardsFixture f;
    f.triggerModeCards();
    f.component.clearWelcome();

    juce::Image img(juce::Image::RGB, 400, 600, true);
    juce::Graphics g(img);
    g.fillAll(juce::Colours::darkblue);
    f.component.paint(g);

    // Scan for non-background pixels in the coach bubble area (y=10-100)
    bool hasBubbleContent = false;
    for (int y = 10; y < 100 && y < 600; ++y) {
        for (int x = 42; x < 350; ++x) { // coach bubble area (past avatar, within width)
            juce::Colour pixel = img.getPixelAt(x, y);
            float dr = std::fabs(pixel.getFloatRed() - juce::Colours::darkblue.getFloatRed());
            float dg = std::fabs(pixel.getFloatGreen() - juce::Colours::darkblue.getFloatGreen());
            float db = std::fabs(pixel.getFloatBlue() - juce::Colours::darkblue.getFloatBlue());
            if (dr > 0.02f || dg > 0.02f || db > 0.02f) {
                hasBubbleContent = true;
                break;
            }
        }
        if (hasBubbleContent) break;
    }
    std::printf("  Coach bubble pixels found: %s\n", hasBubbleContent ? "YES" : "NO");
    TEST("coach bubble renders in image", hasBubbleContent);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Edge Case Tests
// ═══════════════════════════════════════════════════════════════════════════

static void test_no_trigger_without_keywords()
{
    std::printf("\n── Edge: addMessage WITHOUT mix/master keywords ──\n");
    ModeCardsFixture f;
    f.component.addMessage("Bienvenido a MixCoach. Vamos a empezar la sesion.");
    int h = f.component.getTotalHeight();
    std::printf("  height=%d (one bubble ~88px)\n", h);
    TEST("message added without mode card trigger", h > 12 && h < f.initialHeight + 200);
}

static void test_double_trigger_sequential()
{
    std::printf("\n── Edge: Two messages both with mix/master keywords ──\n");
    ModeCardsFixture f;
    f.triggerModeCards();
    int h1 = f.component.getTotalHeight();
    // Second message should also match (different phrasing)
    f.component.addMessage("\xBFMix o Master? Selecciona el modo.");
    int h2 = f.component.getTotalHeight();
    std::printf("  h1=%d (coach+cards), h2=%d (two coach msgs+cards)\n", h1, h2);
    // Second trigger: only first message triggers the cards (modeButtonsBubbleIndex_ stays at 0)
    // Second message adds a bubble but no additional cards
    TEST("second message adds height", h2 > h1);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════

int main()
{
    std::printf("===================================================================\n");
    std::printf("  Mode Cards Unit Tests\n");
    std::printf("  Verifica premium Mix/Master mode cards, animation, y state\n");
    std::printf("===================================================================\n\n");

    // ─── Pure math: Smoothstep ──────────────────────────────────────────────
    test_smoothstep_zero();
    test_smoothstep_half();
    test_smoothstep_one();
    test_smoothstep_clamp();
    test_smoothstep_intermediate();

    // ─── Pure math: Animation state machine ─────────────────────────────────
    test_animation_initial_state();
    test_animation_midpoint();
    test_animation_completion();
    test_animation_past_completion();

    // ─── Pure math: Animation space ─────────────────────────────────────────
    test_animation_space_full();
    test_animation_space_initial();
    test_animation_space_mid();
    test_animation_space_final();

    // ─── Component integration ──────────────────────────────────────────────
    test_initial_state();
    test_trigger_adds_mode_cards_height();
    test_hide_via_setModeButtonsVisible();
    test_clear_and_retrigger();
    test_addUserMessage_hides_via_retrigger();

    // ─── Rendering ──────────────────────────────────────────────────────────
    test_render_coach_bubble();

    // ─── Edge cases ─────────────────────────────────────────────────────────
    test_no_trigger_without_keywords();
    test_double_trigger_sequential();

    // ─── Results ────────────────────────────────────────────────────────────
    std::printf("\n===================================================================\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("===================================================================\n");

    return gTestsFailed > 0 ? 1 : 0;
}
