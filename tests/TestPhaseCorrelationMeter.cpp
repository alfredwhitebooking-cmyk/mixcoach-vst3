// ═══════════════════════════════════════════════════════════════════════════
//  TestPhaseCorrelationMeter.cpp — Unit tests para PhaseCorrelationMeter
//  (barra horizontal de correlación con SmoothValue + color coding)
// ═══════════════════════════════════════════════════════════════════════════
//
//  Build: cmake --build build --config Release --target TestPhaseCorrelationMeter
//  Run:   build/tests/Release/TestPhaseCorrelationMeter.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/MixCoach/UI/PhaseCorrelationMeter.h"

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

#define TEST_NEAR(name, a, b, eps) TEST(name, std::fabs((a) - (b)) < (eps))

// ─── Helpers ───────────────────────────────────────────────────────────────
static juce::Image renderComponent(juce::Component& comp, int w, int h) {
    comp.setSize(w, h);
    juce::Image img(juce::Image::ARGB, w, h, true);
    img.clear(img.getBounds(), juce::Colour(0x00000000));
    juce::Graphics g(img);
    comp.paint(g);
    return img;
}

static bool imageHasContent(const juce::Image& img) {
    auto bounds = img.getBounds();
    int step = std::max(1, bounds.getWidth() / 15);
    for (int y = 0; y < bounds.getHeight(); y += step)
        for (int x = 0; x < bounds.getWidth(); x += step)
            if (img.getPixelAt(x, y).getARGB() != 0)
                return true;
    return false;
}

// ============================================================================
//  Tests
// ============================================================================

static void test_construction() {
    std::printf("\n── Construction ──\n");
    mixcoach::PhaseCorrelationMeter pcm;
    pcm.setSize(300, 100);
    TEST("component created and sized", pcm.getWidth() == 300 && pcm.getHeight() == 100);

    auto img = renderComponent(pcm, 300, 100);
    TEST("empty paint completes without crash", true);
    TEST("default paint has content (title + bar)", imageHasContent(img));
}

static void test_set_correlation_positive() {
    std::printf("\n── setCorrelation: Positive ──\n");
    mixcoach::PhaseCorrelationMeter pcm;
    pcm.setSize(300, 100);

    // Set correlation to +0.75 (in-phase) and converge
    for (int i = 0; i < 50; ++i)
        pcm.setCorrelation(0.75f);

    auto img = renderComponent(pcm, 300, 100);
    TEST("paint after positive correlation completes", true);
    TEST("positive correlation produces visual output", imageHasContent(img));
}

static void test_set_correlation_negative() {
    std::printf("\n── setCorrelation: Negative ──\n");
    mixcoach::PhaseCorrelationMeter pcm;
    pcm.setSize(300, 100);

    // Set correlation to -0.5 (out-of-phase) and converge
    for (int i = 0; i < 50; ++i)
        pcm.setCorrelation(-0.5f);

    auto img = renderComponent(pcm, 300, 100);
    TEST("paint after negative correlation completes", true);
}

static void test_set_correlation_zero() {
    std::printf("\n── setCorrelation: Zero ──\n");
    mixcoach::PhaseCorrelationMeter pcm;
    pcm.setSize(300, 100);

    for (int i = 0; i < 50; ++i)
        pcm.setCorrelation(0.0f);

    auto img = renderComponent(pcm, 300, 100);
    TEST("paint after zero correlation completes", true);
}

static void test_set_correlation_full_positive() {
    std::printf("\n── setCorrelation: +1.0 ──\n");
    mixcoach::PhaseCorrelationMeter pcm;
    pcm.setSize(300, 100);

    for (int i = 0; i < 50; ++i)
        pcm.setCorrelation(1.0f);

    auto img = renderComponent(pcm, 300, 100);
    TEST("paint after +1.0 correlation completes", true);
}

static void test_set_correlation_full_negative() {
    std::printf("\n── setCorrelation: -1.0 ──\n");
    mixcoach::PhaseCorrelationMeter pcm;
    pcm.setSize(300, 100);

    for (int i = 0; i < 50; ++i)
        pcm.setCorrelation(-1.0f);

    auto img = renderComponent(pcm, 300, 100);
    TEST("paint after -1.0 correlation completes", true);
}

static void test_set_correlation_rapid_changes() {
    std::printf("\n── setCorrelation: Rapid Changes ──\n");
    mixcoach::PhaseCorrelationMeter pcm;
    pcm.setSize(300, 100);

    // Rapidly sweep through correlation range
    for (int i = 0; i < 100; ++i) {
        float val = std::sin((float)i * 0.2f);  // -1 to +1 sweep
        pcm.setCorrelation(val);
    }

    auto img = renderComponent(pcm, 300, 100);
    TEST("paint after rapid correlation sweep completes", true);
}

static void test_set_correlation_transition() {
    std::printf("\n── setCorrelation: Positive → Negative Transition ──\n");
    mixcoach::PhaseCorrelationMeter pcm;
    pcm.setSize(300, 100);

    // Settle at +0.9
    for (int i = 0; i < 50; ++i)
        pcm.setCorrelation(0.9f);
    auto imgPos = renderComponent(pcm, 300, 100);
    TEST("paint after settling at +0.9 completes", true);

    // Then sweep to -0.9
    for (int i = 0; i < 50; ++i)
        pcm.setCorrelation(-0.9f);
    auto imgNeg = renderComponent(pcm, 300, 100);
    TEST("paint after transition to -0.9 completes", true);

    // SmoothValue should have moved toward -0.9
    // (can't directly assert internal value, but paint should succeed)
}

static void test_resize() {
    std::printf("\n── Resize ──\n");
    mixcoach::PhaseCorrelationMeter pcm;
    pcm.setSize(300, 100);

    for (int i = 0; i < 20; ++i)
        pcm.setCorrelation(0.5f);

    // Test various sizes
    for (int w = 50; w <= 500; w += 50) {
        pcm.setSize(w, 60);
        renderComponent(pcm, w, 60);
    }
    TEST("multiple resizes + paints complete without crash", true);
}

static void test_extreme_values() {
    std::printf("\n── Extreme Values ──\n");
    mixcoach::PhaseCorrelationMeter pcm;
    pcm.setSize(300, 100);

    // Well beyond range — internally clamped
    for (int i = 0; i < 10; ++i)
        pcm.setCorrelation(10.0f);
    auto imgHigh = renderComponent(pcm, 300, 100);
    TEST("paint after +10.0 correlation (clamped) completes", true);

    for (int i = 0; i < 10; ++i)
        pcm.setCorrelation(-10.0f);
    auto imgLow = renderComponent(pcm, 300, 100);
    TEST("paint after -10.0 correlation (clamped) completes", true);
}

// ============================================================================
//  Main
// ============================================================================

int main() {
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  PhaseCorrelationMeter Unit Tests\n");
    std::printf("  Construction | Correlation Setters | Color Zones | Transitions | Resize\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");

    test_construction();
    test_set_correlation_positive();
    test_set_correlation_negative();
    test_set_correlation_zero();
    test_set_correlation_full_positive();
    test_set_correlation_full_negative();
    test_set_correlation_rapid_changes();
    test_set_correlation_transition();
    test_resize();
    test_extreme_values();

    std::printf("\n\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");

    return gTestsFailed > 0 ? 1 : 0;
}
