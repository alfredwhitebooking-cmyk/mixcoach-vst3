// ═══════════════════════════════════════════════════════════════════════════
//  TestMeterComponent.cpp — Unit tests para MeterComponent
//  (panel compacto de medidores: barras L/R, numéricos Peak/RMS/LUFS/DR, LUFS MDR)
// ═══════════════════════════════════════════════════════════════════════════
//
//  Build: cmake --build build --config Release --target TestMeterComponent
//  Run:   build/tests/Release/TestMeterComponent.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/MixCoach/UI/MeterComponent.h"

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
    mixcoach::MeterComponent meter;
    meter.setSize(400, 200);
    TEST("component created and sized", meter.getWidth() == 400 && meter.getHeight() == 200);

    auto img = renderComponent(meter, 400, 200);
    TEST("empty paint completes without crash", true);
    TEST("default paint has content (meters + readouts)", imageHasContent(img));
}

static void test_update_data_basic() {
    std::printf("\n── updateData: Basic ──\n");
    mixcoach::MeterComponent meter;
    meter.setSize(400, 200);

    mixcoach::TrackTelemetry telem{};
    telem.peakLeft = -6.0f;
    telem.peakRight = -8.0f;
    telem.rmsLeft = -12.0f;
    telem.rmsRight = -14.0f;
    telem.lufsIntegrated = -10.0f;
    telem.lufsMomentary = -10.0f;
    telem.loudnessRange = 8.0f;

    for (int i = 0; i < 50; ++i)
        meter.updateData(telem);

    auto img = renderComponent(meter, 400, 200);
    TEST("paint after updateData completes", true);
    TEST("updateData produces visual output", imageHasContent(img));
}

static void test_update_data_silence() {
    std::printf("\n── updateData: Silence ──\n");
    mixcoach::MeterComponent meter;
    meter.setSize(400, 200);

    mixcoach::TrackTelemetry telem{};
    telem.peakLeft = -100.0f;
    telem.peakRight = -100.0f;
    telem.rmsLeft = -100.0f;
    telem.rmsRight = -100.0f;
    telem.lufsIntegrated = -100.0f;
    telem.lufsMomentary = -100.0f;
    telem.loudnessRange = 0.0f;

    for (int i = 0; i < 50; ++i)
        meter.updateData(telem);

    auto img = renderComponent(meter, 400, 200);
    TEST("paint after silence data completes", true);
}

static void test_update_data_full_scale() {
    std::printf("\n── updateData: Full Scale ──\n");
    mixcoach::MeterComponent meter;
    meter.setSize(400, 200);

    mixcoach::TrackTelemetry telem{};
    telem.peakLeft = 0.0f;
    telem.peakRight = 0.0f;
    telem.rmsLeft = -3.0f;
    telem.rmsRight = -3.0f;
    telem.lufsIntegrated = -6.0f;
    telem.lufsMomentary = -6.0f;
    telem.loudnessRange = 12.0f;

    for (int i = 0; i < 50; ++i)
        meter.updateData(telem);

    auto img = renderComponent(meter, 400, 200);
    TEST("paint after full-scale data completes", true);
    TEST("full-scale data produces clear output", imageHasContent(img));
}

static void test_update_data_asymmetric() {
    std::printf("\n── updateData: Asymmetric L/R ──\n");
    mixcoach::MeterComponent meter;
    meter.setSize(400, 200);

    mixcoach::TrackTelemetry telem{};
    telem.peakLeft = -3.0f;
    telem.peakRight = -24.0f;
    telem.rmsLeft = -8.0f;
    telem.rmsRight = -30.0f;
    telem.lufsIntegrated = -12.0f;
    telem.lufsMomentary = -14.0f;
    telem.loudnessRange = 20.0f;

    for (int i = 0; i < 50; ++i)
        meter.updateData(telem);

    auto img = renderComponent(meter, 400, 200);
    TEST("paint after asymmetric L/R data completes", true);
    TEST("asymmetric data produces visual output", imageHasContent(img));
}

static void test_update_data_with_lufs_fallback() {
    std::printf("\n── updateData: LUFS Fallback ──\n");
    mixcoach::MeterComponent meter;
    meter.setSize(400, 200);

    // When lufsIntegrated is invalid, short-term / readout may stay at floor
    mixcoach::TrackTelemetry telem{};
    telem.peakLeft = -12.0f;
    telem.peakRight = -12.0f;
    telem.rmsLeft = -18.0f;
    telem.rmsRight = -18.0f;
    telem.lufsIntegrated = -100.0f;  // below -99 threshold → fallback
    telem.lufsMomentary = -100.0f;
    telem.loudnessRange = 0.0f;

    for (int i = 0; i < 50; ++i)
        meter.updateData(telem);

    auto img = renderComponent(meter, 400, 200);
    TEST("paint after LUFS fallback completes", true);
}

static void test_update_data_extreme_values() {
    std::printf("\n── updateData: Extreme Values ──\n");
    mixcoach::MeterComponent meter;
    meter.setSize(400, 200);

    // Rapidly oscillating extremes — stress test
    for (int cycle = 0; cycle < 10; ++cycle) {
        mixcoach::TrackTelemetry hot{};
        hot.peakLeft = 3.0f;     // clipped/clamped
        hot.peakRight = 3.0f;
        hot.rmsLeft = 0.0f;
        hot.rmsRight = 0.0f;
        hot.lufsIntegrated = 0.0f;
        hot.loudnessRange = 60.0f;  // extreme range
        meter.updateData(hot);

        mixcoach::TrackTelemetry cold{};
        cold.peakLeft = -120.0f;  // far below floor
        cold.peakRight = -120.0f;
        cold.rmsLeft = -120.0f;
        cold.rmsRight = -120.0f;
        cold.lufsIntegrated = -120.0f;
        cold.loudnessRange = 0.0f;
        meter.updateData(cold);
    }

    auto img = renderComponent(meter, 400, 200);
    TEST("paint after extreme value oscillation completes without crash", true);
}

static void test_resize() {
    std::printf("\n── Resize ──\n");
    mixcoach::MeterComponent meter;
    meter.setSize(400, 200);

    mixcoach::TrackTelemetry telem{};
    telem.peakLeft = -6.0f;
    telem.peakRight = -6.0f;
    telem.rmsLeft = -12.0f;
    telem.rmsRight = -12.0f;

    for (int i = 0; i < 20; ++i)
        meter.updateData(telem);

    // Test various aspect ratios
    for (int w = 200; w <= 600; w += 100) {
        meter.setSize(w, 150);
        renderComponent(meter, w, 150);
    }
    for (int h = 100; h <= 300; h += 50) {
        meter.setSize(400, h);
        renderComponent(meter, 400, h);
    }
    TEST("multiple resizes + paints complete without crash", true);
}

static void test_consecutive_updates() {
    std::printf("\n── Consecutive Updates ──\n");
    mixcoach::MeterComponent meter;
    meter.setSize(400, 200);

    // Simulate a natural signal: silence → burst → sustain → silence
    mixcoach::TrackTelemetry silence{};
    silence.peakLeft = -90.0f;
    silence.peakRight = -90.0f;
    silence.rmsLeft = -96.0f;
    silence.rmsRight = -96.0f;

    mixcoach::TrackTelemetry burst{};
    burst.peakLeft = -1.0f;
    burst.peakRight = -2.0f;
    burst.rmsLeft = -6.0f;
    burst.rmsRight = -7.0f;
    burst.lufsIntegrated = -8.0f;
    burst.lufsMomentary = -8.0f;
    burst.loudnessRange = 10.0f;

    mixcoach::TrackTelemetry sustain{};
    sustain.peakLeft = -6.0f;
    sustain.peakRight = -6.0f;
    sustain.rmsLeft = -12.0f;
    sustain.rmsRight = -12.0f;
    sustain.lufsIntegrated = -14.0f;
    sustain.lufsMomentary = -14.0f;
    sustain.loudnessRange = 6.0f;

    // Phase 1: silence
    for (int i = 0; i < 5; ++i)
        meter.updateData(silence);
    auto img1 = renderComponent(meter, 400, 200);
    TEST("paint after silence phase completes", true);

    // Phase 2: sudden burst
    for (int i = 0; i < 3; ++i)
        meter.updateData(burst);
    auto img2 = renderComponent(meter, 400, 200);
    TEST("paint after burst phase completes", true);

    // Phase 3: sustain
    for (int i = 0; i < 20; ++i)
        meter.updateData(sustain);
    auto img3 = renderComponent(meter, 400, 200);
    TEST("paint after sustain phase completes", true);

    // Phase 4: back to silence
    for (int i = 0; i < 50; ++i)
        meter.updateData(silence);
    auto img4 = renderComponent(meter, 400, 200);
    TEST("paint after return to silence completes", true);
}

// ============================================================================
//  Main
// ============================================================================

int main() {
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  MeterComponent Unit Tests\n");
    std::printf("  Construction | Telemetry Data | LUFS Fallback | Consecutive | Resize\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");

    test_construction();
    test_update_data_basic();
    test_update_data_silence();
    test_update_data_full_scale();
    test_update_data_asymmetric();
    test_update_data_with_lufs_fallback();
    test_update_data_extreme_values();
    test_resize();
    test_consecutive_updates();

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
