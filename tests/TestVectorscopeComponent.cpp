// ═══════════════════════════════════════════════════════════════════════════
//  TestVectorscopeComponent.cpp — Unit tests para VectorscopeComponent
//  (vectorscopio circular con detección de correlación de fase)
// ═══════════════════════════════════════════════════════════════════════════
//
//  Build: cmake --build build --config Release --target TestVectorscopeComponent
//  Run:   build/tests/Release/TestVectorscopeComponent.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/MixCoach/UI/VectorscopeComponent.h"

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
    mixcoach::VectorscopeComponent vec;
    vec.setSize(300, 300);
    TEST("component created and sized", vec.getWidth() == 300 && vec.getHeight() == 300);

    auto img = renderComponent(vec, 300, 300);
    TEST("empty paint completes without crash", true);
    TEST("grid is drawn on empty vectorscope", imageHasContent(img));
}

static void test_push_sample_identical() {
    std::printf("\n── pushSample: Identical L/R ──\n");
    mixcoach::VectorscopeComponent vec;
    vec.setSize(300, 300);

    // Push identical L/R samples — correlation should trend toward +1.0
    for (int i = 0; i < 256; ++i)
        vec.pushSample(0.5f, 0.5f);

    auto img = renderComponent(vec, 300, 300);
    TEST("paint after identical L/R samples completes", true);
    TEST("identical L/R produces visual trace", imageHasContent(img));
}

static void test_push_sample_opposite() {
    std::printf("\n── pushSample: Opposite L/R ──\n");
    mixcoach::VectorscopeComponent vec;
    vec.setSize(300, 300);

    // Push opposite L/R samples — correlation should trend toward -1.0
    for (int i = 0; i < 256; ++i)
        vec.pushSample(0.5f, -0.5f);

    auto img = renderComponent(vec, 300, 300);
    TEST("paint after opposite L/R samples completes", true);
}

static void test_push_sample_left_only() {
    std::printf("\n── pushSample: Left Only ──\n");
    mixcoach::VectorscopeComponent vec;
    vec.setSize(300, 300);

    // Push only left channel — trace should be horizontal line
    for (int i = 0; i < 256; ++i)
        vec.pushSample(1.0f, 0.0f);

    auto img = renderComponent(vec, 300, 300);
    TEST("paint after left-only input completes", true);
}

static void test_push_sample_right_only() {
    std::printf("\n── pushSample: Right Only ──\n");
    mixcoach::VectorscopeComponent vec;
    vec.setSize(300, 300);

    // Push only right channel — trace should be vertical line
    for (int i = 0; i < 256; ++i)
        vec.pushSample(0.0f, 1.0f);

    auto img = renderComponent(vec, 300, 300);
    TEST("paint after right-only input completes", true);
}

static void test_push_sample_silence() {
    std::printf("\n── pushSample: Silence ──\n");
    mixcoach::VectorscopeComponent vec;
    vec.setSize(300, 300);

    // Silence — trace should be at center
    for (int i = 0; i < 256; ++i)
        vec.pushSample(0.0f, 0.0f);

    auto img = renderComponent(vec, 300, 300);
    TEST("paint after silence completes", true);
}

static void test_push_sample_full_scale() {
    std::printf("\n── pushSample: Full Scale ──\n");
    mixcoach::VectorscopeComponent vec;
    vec.setSize(300, 300);

    // Push full-scale L/R that change polarity — stress test clamping
    for (int i = 0; i < 256; ++i) {
        float phase = std::sin((float)i * 0.1f);
        vec.pushSample(phase, phase * 0.5f);
    }

    auto img = renderComponent(vec, 300, 300);
    TEST("paint after full-scale input completes", true);
    TEST("full-scale produces visual trace", imageHasContent(img));
}

static void test_push_sample_beyond_limits() {
    std::printf("\n── pushSample: Beyond Limits ──\n");
    mixcoach::VectorscopeComponent vec;
    vec.setSize(300, 300);

    // Values outside [-1, 1] should be clamped internally
    for (int i = 0; i < 256; ++i)
        vec.pushSample(2.0f, -2.0f);

    auto img = renderComponent(vec, 300, 300);
    TEST("paint after out-of-range input completes without crash", true);
}

static void test_ring_buffer_wrap() {
    std::printf("\n── Ring Buffer Wrap ──\n");
    mixcoach::VectorscopeComponent vec;
    vec.setSize(300, 300);

    // Push more than kTraceLen (256) samples to test ring buffer wrapping
    for (int i = 0; i < 512; ++i)
        vec.pushSample(0.3f, 0.3f);

    auto img = renderComponent(vec, 300, 300);
    TEST("paint after ring buffer wrap completes", true);
    TEST("trace persists after full buffer wrap", imageHasContent(img));
}

static void test_alpha_decay() {
    std::printf("\n── Alpha Decay ──\n");
    mixcoach::VectorscopeComponent vec;
    vec.setSize(300, 300);

    // Push some samples
    for (int i = 0; i < 50; ++i)
        vec.pushSample(0.5f, 0.5f);

    auto imgAfterPush = renderComponent(vec, 300, 300);
    TEST("paint after samples has content", imageHasContent(imgAfterPush));

    // Push many silence samples — alpha should decay to near-zero
    for (int i = 0; i < 500; ++i)
        vec.pushSample(0.0f, 0.0f);

    auto imgAfterDecay = renderComponent(vec, 300, 300);
    TEST("paint after alpha decay completes without crash", true);
}

static void test_resize() {
    std::printf("\n── Resize ──\n");
    mixcoach::VectorscopeComponent vec;
    vec.setSize(300, 300);

    for (int i = 0; i < 100; ++i)
        vec.pushSample(0.5f * std::sin(i * 0.1f), 0.5f * std::sin(i * 0.1f + 1.0f));

    // Try different sizes
    for (int s = 100; s <= 400; s += 50) {
        vec.setSize(s, s);
        renderComponent(vec, s, s);
    }
    TEST("multiple resize + paint cycles complete without crash", true);
}

// ============================================================================
//  Main
// ============================================================================

int main() {
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  VectorscopeComponent Unit Tests\n");
    std::printf("  Construction | L/R | Grid | Ring Buffer | Alpha Decay | Resize\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");

    test_construction();
    test_push_sample_identical();
    test_push_sample_opposite();
    test_push_sample_left_only();
    test_push_sample_right_only();
    test_push_sample_silence();
    test_push_sample_full_scale();
    test_push_sample_beyond_limits();
    test_ring_buffer_wrap();
    test_alpha_decay();
    test_resize();

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
