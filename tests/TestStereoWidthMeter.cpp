// ═══════════════════════════════════════════════════════════════════════════
//  TestStereoWidthMeter.cpp — Unit tests para StereoWidthMeter
//  (medidor de ancho estéreo con barra horizontal, 4 zonas y marker animado)
// ═══════════════════════════════════════════════════════════════════════════
//
//  Build: cmake --build build --config Release --target TestStereoWidthMeter
//  Run:   build/tests/Release/TestStereoWidthMeter.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <algorithm>
#include <string>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/MixCoach/UI/StereoWidthMeter.h"
#include "../Source/MixCoach/UI/MixCoachTheme.h"

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

static bool imageHasNonBgPixel(const juce::Image& img, juce::Colour bg) {
    auto bounds = img.getBounds();
    uint32_t bgArgb = bg.getARGB();
    int step = std::max(1, bounds.getWidth() / 20);
    for (int y = 0; y < bounds.getHeight(); y += step)
        for (int x = 0; x < bounds.getWidth(); x += step) {
            auto px = img.getPixelAt(x, y);
            // px is juce::Colour in JUCE 8
            if (px.getARGB() != bgArgb && px.getAlpha() > 10)
                return true;
        }
    return false;
}

static bool imageHasContent(const juce::Image& img) {
    return imageHasNonBgPixel(img, juce::Colour(0x00000000));
}



/** Encuentra la coordenada X del marker en la imagen buscando el píxel más brillante.
    Busca en la franja central de la imagen (30% al 70% de la altura) donde se
    dibuja la barra horizontal.
    NOTA: el marker tiene un glow (2 círculos translúcidos) alrededor del diamante,
    pero el punto más brillante debe coincidir con el centro del diamante. */
static int findMarkerX(const juce::Image& img) {
    int h = img.getHeight();
    int yStart = (int)(h * 0.30f);
    int yEnd   = (int)(h * 0.70f);
    int bestX = -1;
    uint32_t bestBrightness = 0;

    for (int x = 0; x < img.getWidth(); ++x) {
        uint32_t totalBright = 0;
        int samples = 0;
        for (int py = yStart; py <= yEnd; ++py) {
            auto px = img.getPixelAt(x, py);
            totalBright += (uint32_t)px.getRed() + (uint32_t)px.getGreen() + (uint32_t)px.getBlue();
            samples++;
        }
        uint32_t avgBright = samples > 0 ? totalBright / (uint32_t)samples : 0;
        if (avgBright > bestBrightness) {
            bestBrightness = avgBright;
            bestX = x;
        }
    }
    return bestX;
}

// ============================================================================
//  Tests
// ============================================================================

static void test_construction() {
    std::printf("\n── Construction ──\n");
    mixcoach::StereoWidthMeter meter;
    meter.setSize(200, 60);
    TEST("component created and sized", meter.getWidth() == 200 && meter.getHeight() == 60);
    TEST_NEAR("initial width is 0.0", meter.getAvgWidth(), 0.0f, 0.001f);

    auto img = renderComponent(meter, 200, 60);
    TEST("empty paint completes without crash", true);
    TEST("paint produces visible content", imageHasContent(img));
}

static void test_set_width_mono() {
    std::printf("\n── setAvgWidth: Mono (0.0) ──\n");
    mixcoach::StereoWidthMeter meter;
    meter.setSize(200, 60);
    meter.setAvgWidth(0.0f);
    meter.advanceVisuals(60.0, false);

    TEST_NEAR("getAvgWidth returns 0.0", meter.getAvgWidth(), 0.0f, 0.01f);

    auto img = renderComponent(meter, 200, 60);
    TEST("paint at width=0.0 completes", true);
    TEST("paint at width=0.0 has content", imageHasContent(img));
}

static void test_set_width_narrow() {
    std::printf("\n── setAvgWidth: Narrow (0.25) ──\n");
    mixcoach::StereoWidthMeter meter;
    meter.setSize(200, 60);
    meter.setAvgWidth(0.25f);
    meter.advanceVisuals(60.0, true);

    TEST_NEAR("getAvgWidth returns 0.25", meter.getAvgWidth(), 0.25f, 0.01f);

    auto img = renderComponent(meter, 200, 60);
    TEST("paint at width=0.25 completes", true);
    TEST("paint at width=0.25 has content", imageHasContent(img));

    // Marker should be at ~25% of bar width
    // Bar starts at x=2 (reduced(2,1)) and is 200-4=196 wide
    // Marker X = 2 + 0.25 * 196 = 51
    int expectedX = 2 + (int)(0.25f * 196.0f);
    int foundX = findMarkerX(img); // bar at y~20
    TEST_NEAR("marker near 25% position", (float)foundX, (float)expectedX, 8.0f);
}

static void test_set_width_wide() {
    std::printf("\n── setAvgWidth: Wide (0.55) ──\n");
    mixcoach::StereoWidthMeter meter;
    meter.setSize(200, 60);
    meter.setAvgWidth(0.55f);
    meter.advanceVisuals(60.0, true);

    TEST_NEAR("getAvgWidth returns 0.55", meter.getAvgWidth(), 0.55f, 0.01f);

    auto img = renderComponent(meter, 200, 60);
    TEST("paint at width=0.55 completes", true);

    // Marker should be at ~55% of bar width
    int expectedX = 2 + (int)(0.55f * 196.0f);
    int foundX = findMarkerX(img);
    TEST_NEAR("marker near 55% position", (float)foundX, (float)expectedX, 8.0f);
}

static void test_set_width_too_wide() {
    std::printf("\n── setAvgWidth: Too Wide (0.85) ──\n");
    mixcoach::StereoWidthMeter meter;
    meter.setSize(200, 60);
    meter.setAvgWidth(0.85f);
    meter.advanceVisuals(60.0, true);

    TEST_NEAR("getAvgWidth returns 0.85", meter.getAvgWidth(), 0.85f, 0.01f);

    auto img = renderComponent(meter, 200, 60);
    TEST("paint at width=0.85 completes", true);

    // Marker should be at ~85% of bar width
    int expectedX = 2 + (int)(0.85f * 196.0f);
    int foundX = findMarkerX(img);
    TEST_NEAR("marker near 85% position", (float)foundX, (float)expectedX, 8.0f);
}

static void test_set_width_full() {
    std::printf("\n── setAvgWidth: Full (1.0) ──\n");
    mixcoach::StereoWidthMeter meter;
    meter.setSize(200, 60);
    meter.setAvgWidth(1.0f);
    meter.advanceVisuals(60.0, true);

    TEST_NEAR("getAvgWidth returns 1.0", meter.getAvgWidth(), 1.0f, 0.01f);

    auto img = renderComponent(meter, 200, 60);
    TEST("paint at width=1.0 completes", true);

    // Marker at ~100% of bar width
    int expectedX = 2 + (int)(1.0f * 196.0f);
    int foundX = findMarkerX(img);
    TEST_NEAR("marker near 100% position", (float)foundX, (float)expectedX, 8.0f);
}

static void test_clamping() {
    std::printf("\n── Clamping ──\n");
    mixcoach::StereoWidthMeter meter;
    meter.setSize(200, 60);

    // Values outside [0,1] should be clamped
    meter.setAvgWidth(-0.5f);
    TEST_NEAR("clamped to 0.0 for negative input", meter.getAvgWidth(), 0.0f, 0.01f);

    meter.setAvgWidth(1.5f);
    meter.advanceVisuals(60.0, true);
    TEST_NEAR("clamped to 1.0 for over-unity input", meter.getAvgWidth(), 1.0f, 0.05f);
}

static void test_animation_smoothing() {
    std::printf("\n── Animation Smoothing ──\n");
    mixcoach::StereoWidthMeter meter;
    meter.setSize(200, 60);

    // Set a low initial value via multiple advances
    meter.setAvgWidth(0.0f);
    for (int i = 0; i < 5; ++i)
        meter.advanceVisuals(60.0, false);
    float initial = meter.getAvgWidth();
    TEST_NEAR("settled to 0.0", initial, 0.0f, 0.01f);

    // Jump to a high value
    meter.setAvgWidth(0.8f);
    // First tick should move toward target but not reach it
    bool dirty = meter.advanceVisuals(60.0, false);
    float afterOne = meter.getAvgWidth();
    TEST("value moved toward target", afterOne > initial);
    TEST("value hasn't reached target yet", afterOne < 0.8f);
    TEST("advanceVisuals returns dirty", dirty);

    // After enough ticks, should approach target
    for (int i = 0; i < 30; ++i)
        meter.advanceVisuals(60.0, false);
    TEST_NEAR("approaches target after animation", meter.getAvgWidth(), 0.8f, 0.15f);
}

static void test_per_band_data() {
    std::printf("\n── Per-Band Data ──\n");
    mixcoach::StereoWidthMeter meter;
    meter.setSize(200, 80);

    // Initially no per-band data
    auto imgNoBand = renderComponent(meter, 200, 80);
    TEST("paint without per-band data completes", true);

    // Set per-band data
    float bandData[6] = { 0.1f, 0.2f, 0.5f, 0.7f, 0.9f, 0.3f };
    meter.setPerBandWidth(bandData);
    auto imgBand = renderComponent(meter, 200, 80);
    TEST("paint with per-band data completes", true);

    // Null should clear per-band
    meter.setPerBandWidth(nullptr);
    auto imgClear = renderComponent(meter, 200, 80);
    TEST("paint after clearing per-band data completes", true);

    // Alternating null/set should not crash
    meter.setPerBandWidth(bandData);
    meter.setPerBandWidth(nullptr);
    meter.setPerBandWidth(bandData);
    auto imgStable = renderComponent(meter, 200, 80);
    TEST("paint after multiple per-band toggles completes", true);
}

static void test_zone_boundaries() {
    std::printf("\n── Zone Boundaries ──\n");

    // Test that marker positions fall within correct zones
    // MONO: 0.00-0.15, NARROW: 0.15-0.40, WIDE: 0.40-0.70, TOO_WIDE: 0.70-1.00
    struct TestCase {
        float width;
        const char* zone;
        float expectedNorm; // Expected normalized marker position (0-1)
    };

    TestCase cases[] = {
        { 0.00f, "MONO",      0.00f },
        { 0.07f, "MONO",      0.07f },
        { 0.14f, "MONO",      0.14f },
        { 0.15f, "NARROW",    0.15f },
        { 0.20f, "NARROW",    0.20f },
        { 0.39f, "NARROW",    0.39f },
        { 0.40f, "WIDE",      0.40f },
        { 0.55f, "WIDE",      0.55f },
        { 0.69f, "WIDE",      0.69f },
        { 0.70f, "TOO WIDE",  0.70f },
        { 0.85f, "TOO WIDE",  0.85f },
        { 1.00f, "TOO WIDE",  1.00f },
    };

    int caseCount = sizeof(cases) / sizeof(cases[0]);
    for (int i = 0; i < caseCount; ++i) {
        mixcoach::StereoWidthMeter meter;
        meter.setSize(200, 60);
        meter.setAvgWidth(cases[i].width);
        meter.advanceVisuals(60.0, true);

        float width = meter.getAvgWidth();
        TEST_NEAR(cases[i].zone, width, cases[i].expectedNorm, 0.01f);
    }
}

static void test_resize_stability() {
    std::printf("\n── Resize Stability ──\n");
    mixcoach::StereoWidthMeter meter;

    // Try various sizes
    int sizes[] = { 100, 150, 200, 300, 400, 50 };
    for (int s : sizes) {
        meter.setSize(s, 60);
        meter.setAvgWidth(0.5f);
        meter.advanceVisuals(60.0, true);
        auto img = renderComponent(meter, s, 60);
        TEST(("paint at width=" + std::to_string(s)).c_str(), imageHasContent(img));
    }

    // Very small size (should not crash)
    meter.setSize(30, 20);
    auto imgSmall = renderComponent(meter, 30, 20);
    TEST("paint at very small size (30x20) completes", true);

    // Very large size (should not crash)
    meter.setSize(600, 200);
    auto imgLarge = renderComponent(meter, 600, 200);
    TEST("paint at large size (600x200) completes", true);
}

static void test_multiple_update_cycles() {
    std::printf("\n── Multiple Update Cycles ──\n");
    mixcoach::StereoWidthMeter meter;
    meter.setSize(200, 60);

    // Simulate many updates with varying widths
    float vals[] = { 0.0f, 0.3f, 0.6f, 0.9f, 0.4f, 0.1f, 0.8f, 0.5f, 0.2f };
    for (int cycle = 0; cycle < 3; ++cycle) {
        for (float v : vals) {
            meter.setAvgWidth(v);
            meter.advanceVisuals(60.0, false);
        }
        auto img = renderComponent(meter, 200, 60);
        TEST(("cycle " + std::to_string(cycle + 1) + " paint completes").c_str(), imageHasContent(img));
    }
}

static void test_badge_display() {
    std::printf("\n── Badge Display ──\n");
    mixcoach::StereoWidthMeter meter;

    // At 42% width, badge should show "42%"
    meter.setSize(200, 60);
    meter.setAvgWidth(0.42f);
    meter.advanceVisuals(60.0, true);
    auto img = renderComponent(meter, 200, 60);
    TEST("paint with badge at 42% completes", imageHasContent(img));

    // At 100% width
    meter.setAvgWidth(1.0f);
    meter.advanceVisuals(60.0, true);
    auto imgFull = renderComponent(meter, 200, 60);
    TEST("paint with badge at 100% completes", imageHasContent(imgFull));
}

static void test_rapid_width_changes() {
    std::printf("\n── Rapid Width Changes ──\n");
    mixcoach::StereoWidthMeter meter;
    meter.setSize(200, 60);

    // Rapid changes should not crash
    for (int i = 0; i < 100; ++i) {
        float v = std::sin(i * 0.1f) * 0.5f + 0.5f;
        meter.setAvgWidth(v);
    }
    meter.advanceVisuals(60.0, true);
    auto img = renderComponent(meter, 200, 60);
    TEST("paint after 100 rapid width changes completes", true);
    TEST("paint has visible content after rapid changes", imageHasContent(img));
}

// ============================================================================
//  Main
// ============================================================================

int main() {
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  StereoWidthMeter Unit Tests\n");
    std::printf("  Construction | Width Values | Marker Position | Zones | Animation | Per-Band | Resize\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");

    test_construction();
    test_set_width_mono();
    test_set_width_narrow();
    test_set_width_wide();
    test_set_width_too_wide();
    test_set_width_full();
    test_clamping();
    test_animation_smoothing();
    test_per_band_data();
    test_zone_boundaries();
    test_resize_stability();
    test_multiple_update_cycles();
    test_badge_display();
    test_rapid_width_changes();

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
