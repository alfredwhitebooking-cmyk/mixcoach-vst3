// ═══════════════════════════════════════════════════════════════════════════
//  TestLUFSMeter.cpp — Unit tests para LUFSMeter (EBU R128 loudness meter)
// ═══════════════════════════════════════════════════════════════════════════
//
//  Build: cmake --build build --config Release --target TestLUFSMeter
//  Run:   build/tests/Release/TestLUFSMeter.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/MixCoach/UI/LUFSMeter.h"

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

// ============================================================================
//  Tests
// ============================================================================

static void test_construction() {
    std::printf("\n── Construction ──\n");
    mixcoach::LUFSMeter meter;
    TEST("component is created", true);
    TEST("component is not visible until added", !meter.isVisible());
    meter.setSize(200, 300);
    TEST("setSize works", meter.getWidth() == 200 && meter.getHeight() == 300);
}

static void test_paint_no_crash() {
    std::printf("\n── Paint: No Crash ──\n");
    mixcoach::LUFSMeter meter;
    auto img = renderComponent(meter, 200, 300);
    TEST("paint completes without crash", true);
    // Verify pixels were drawn (not all zero/transparent)
    bool hasContent = false;
    for (int y = 0; y < 10 && !hasContent; ++y)
        for (int x = 0; x < 10 && !hasContent; ++x)
            if (img.getPixelAt(x, y).getARGB() != 0)
                hasContent = true;
    TEST("paint produces visual output", hasContent);
}

static void test_set_integrated() {
    std::printf("\n── setIntegrated ──\n");
    mixcoach::LUFSMeter meter;
    meter.setSize(200, 300);

    // Start value convergence (SmoothValue needs advanceVisuals to move)
    for (int i = 0; i < 50; ++i) {
        meter.setIntegrated(-10.0f);
        meter.advanceVisuals(60.0);
    }

    // Paint after convergence — should not crash
    auto img = renderComponent(meter, 200, 300);
    TEST("paint after setIntegrated completes", true);
}

static void test_set_all_values() {
    std::printf("\n── Set All Values ──\n");
    mixcoach::LUFSMeter meter;
    meter.setSize(200, 300);

    for (int i = 0; i < 50; ++i) {
        meter.setIntegrated(-23.0f);
        meter.setShortTerm(-14.0f);
        meter.setMomentary(-16.0f);
        meter.setTruePeak(-6.0f);
        meter.setRange(12.0f);
        meter.advanceVisuals(60.0);
    }

    auto img = renderComponent(meter, 200, 300);
    TEST("paint after all setters converges", true);
}

static void test_extreme_values() {
    std::printf("\n── Extreme Values ──\n");
    mixcoach::LUFSMeter meter;
    meter.setSize(200, 300);

    // Extremely low values
    for (int i = 0; i < 10; ++i) {
        meter.setIntegrated(-100.0f);
        meter.advanceVisuals(60.0);
    }
    auto img1 = renderComponent(meter, 200, 300);
    TEST("paint after -100 LUFS completes", true);

    // Extremely high values
    for (int i = 0; i < 10; ++i) {
        meter.setIntegrated(30.0f);
        meter.advanceVisuals(60.0);
    }
    auto img2 = renderComponent(meter, 200, 300);
    TEST("paint after +30 LUFS completes", true);

    // Rapid changes
    for (int i = 0; i < 20; ++i) {
        meter.setIntegrated((i % 2 == 0) ? -40.0f : 0.0f);
        meter.advanceVisuals(60.0);
    }
    auto img3 = renderComponent(meter, 200, 300);
    TEST("paint after rapid value changes completes", true);
}

static void test_zero_size() {
    std::printf("\n── Zero Size ──\n");
    mixcoach::LUFSMeter meter;
    auto img = renderComponent(meter, 0, 0);
    TEST("paint at zero size completes without crash", true);
}

static void test_small_size() {
    std::printf("\n── Small Size ──\n");
    mixcoach::LUFSMeter meter;
    auto img = renderComponent(meter, 10, 10);
    TEST("paint at 10x10 completes without crash", true);
    bool hasContent = false;
    for (int y = 0; y < 10 && !hasContent; ++y)
        for (int x = 0; x < 10 && !hasContent; ++x)
            if (img.getPixelAt(x, y).getARGB() != 0)
                hasContent = true;
    TEST("paint at 10x10 produces output", hasContent);
}

static void test_target_markers() {
    std::printf("\n── Target Markers ──\n");
    mixcoach::LUFSMeter meter;
    meter.setSize(200, 300);

    // Set integrated to -23 (exactly at kTargetIntegrated) — marker should draw
    for (int i = 0; i < 50; ++i) {
        meter.setIntegrated(-23.0f);
        meter.advanceVisuals(60.0);
    }
    auto img = renderComponent(meter, 200, 300);
    TEST("paint at target value -23 LUFS completes", true);
}

static void test_lufs_resize() {
    std::printf("\n── Resize ──\n");
    mixcoach::LUFSMeter meter;
    meter.setSize(200, 300);

    // Resize multiple times
    for (int w = 50; w <= 400; w += 50) {
        meter.setSize(w, 200);
        auto img = renderComponent(meter, w, 200);
    }
    TEST("multiple resizes + paints complete without crash", true);

    // Final resize check
    meter.setSize(400, 600);
    auto img = renderComponent(meter, 400, 600);
    bool hasContent = false;
    for (int y = 0; y < 10 && !hasContent; ++y)
        for (int x = 0; x < 10 && !hasContent; ++x)
            if (img.getPixelAt(x, y).getARGB() != 0)
                hasContent = true;
    TEST("paint at 400x600 produces output", hasContent);
}

// ============================================================================
//  Main
// ============================================================================

int main() {
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  LUFSMeter Unit Tests\n");
    std::printf("  Construction | Paint | Setters | Extreme Values | Resize\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");

    test_construction();
    test_paint_no_crash();
    test_set_integrated();
    test_set_all_values();
    test_extreme_values();
    test_zero_size();
    test_small_size();
    test_target_markers();
    test_lufs_resize();

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
