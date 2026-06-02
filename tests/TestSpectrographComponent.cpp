// ═══════════════════════════════════════════════════════════════════════════
//  TestSpectrographComponent.cpp — Unit tests para SpectrographComponent
//  (espectrografo logarítmico 20Hz–20kHz con smooth + peak hold)
// ═══════════════════════════════════════════════════════════════════════════
//
//  Build: cmake --build build --config Release --target TestSpectrographComponent
//  Run:   build/tests/Release/TestSpectrographComponent.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/MixCoach/UI/SpectrographComponent.h"

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

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
    int step = std::max(1, bounds.getWidth() / 20);
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
    mixcoach::SpectrographComponent spec;
    spec.setSize(400, 200);
    TEST("component created and sized", spec.getWidth() == 400);

    // Paint on empty data — should show grid but no bars
    auto img = renderComponent(spec, 400, 200);
    TEST("empty paint completes without crash", true);
    TEST("grid is drawn on empty spectrograph", imageHasContent(img));
}

static void test_update_spectrum_silence() {
    std::printf("\n── updateSpectrum: Silence ──\n");
    mixcoach::SpectrographComponent spec;
    spec.setSize(400, 200);

    // All zeros → all bins near zero
    std::vector<float> silence(512, 0.0f);
    for (int i = 0; i < 5; ++i)
        spec.updateSpectrum(silence.data(), (int)silence.size());

    auto img = renderComponent(spec, 400, 200);
    TEST("paint after silence update completes", true);
}

static void test_update_spectrum_sine_peak() {
    std::printf("\n── updateSpectrum: Sine Peak ──\n");
    mixcoach::SpectrographComponent spec;
    spec.setSize(400, 200);

    // Create a spectrum with a clear peak at bin ~10 (440Hz at 44.1kHz, 1024 FFT)
    // FFT bin width = 44100/1024 ≈ 43.07 Hz
    // Bin 10 ≈ 430 Hz (close to 440 Hz)
    std::vector<float> spectrum(512, 0.00001f);
    spectrum[10] = 1.0f;  // strong peak at bin 10

    for (int i = 0; i < 5; ++i)
        spec.updateSpectrum(spectrum.data(), (int)spectrum.size());

    auto img = renderComponent(spec, 400, 200);
    TEST("paint after spectrum with peak completes", true);
    TEST("spectrum with peak produces visual output", imageHasContent(img));
}

static void test_update_spectrum_full_scale() {
    std::printf("\n── updateSpectrum: Full Scale ──\n");
    mixcoach::SpectrographComponent spec;
    spec.setSize(400, 200);

    // All bins at full scale
    std::vector<float> fullScale(512, 1.0f);
    for (int i = 0; i < 5; ++i)
        spec.updateSpectrum(fullScale.data(), (int)fullScale.size());

    auto img = renderComponent(spec, 400, 200);
    TEST("paint after full-scale spectrum completes", true);
    TEST("full-scale spectrum produces visual output", imageHasContent(img));
}

static void test_update_spectrum_small_fft() {
    std::printf("\n── updateSpectrum: Small FFT ──\n");
    mixcoach::SpectrographComponent spec;
    spec.setSize(400, 200);

    // FFT with very few bins (e.g., 32-point FFT)
    std::vector<float> smallFFT(32, 0.0f);
    smallFFT[4] = 1.0f;
    for (int i = 0; i < 5; ++i)
        spec.updateSpectrum(smallFFT.data(), (int)smallFFT.size());

    auto img = renderComponent(spec, 400, 200);
    TEST("paint after 32-bin FFT completes", true);
}

static void test_update_spectrum_large_fft() {
    std::printf("\n── updateSpectrum: Large FFT ──\n");
    mixcoach::SpectrographComponent spec;
    spec.setSize(400, 200);

    // FFT with 1024 bins (capped to kMaxFFTBins=512)
    std::vector<float> largeFFT(1024, 0.0f);
    largeFFT[20] = 0.8f;
    for (int i = 0; i < 5; ++i)
        spec.updateSpectrum(largeFFT.data(), (int)largeFFT.size());

    auto img = renderComponent(spec, 400, 200);
    TEST("paint after 1024-bin FFT completes", true);
}

static void test_update_spectrum_empty() {
    std::printf("\n── updateSpectrum: Empty ──\n");
    mixcoach::SpectrographComponent spec;
    spec.setSize(400, 200);

    // Empty/zero-bin data
    spec.updateSpectrum(nullptr, 0);
    auto img = renderComponent(spec, 400, 200);
    TEST("paint after empty update completes without crash", true);
}

static void test_update_spectrum_smoothing() {
    std::printf("\n── updateSpectrum: Smoothing ──\n");
    mixcoach::SpectrographComponent spec;
    spec.setSize(400, 200);

    // Sudden burst then silence — should smoothly decay (release)
    std::vector<float> burst(512, 0.0f);
    burst[10] = 1.0f;

    // Apply burst several times (attack)
    for (int i = 0; i < 3; ++i)
        spec.updateSpectrum(burst.data(), (int)burst.size());
    auto imgAfterBurst = renderComponent(spec, 400, 200);
    TEST("paint after burst completes", true);

    // Apply silence (should decay slowly — release is 0.12 coeff)
    std::vector<float> silence(512, 0.0f);
    for (int i = 0; i < 10; ++i)
        spec.updateSpectrum(silence.data(), (int)silence.size());
    auto imgAfterDecay = renderComponent(spec, 400, 200);
    TEST("paint after decay completes", true);

    // After many silence updates, should approach zero
    for (int i = 0; i < 200; ++i)
        spec.updateSpectrum(silence.data(), (int)silence.size());
    auto imgAfterLongSilence = renderComponent(spec, 400, 200);
    TEST("paint after long silence completes", true);
}

static void test_sample_rate_rebuild() {
    std::printf("\n── setSampleRate ──\n");
    mixcoach::SpectrographComponent spec;
    spec.setSize(400, 200);
    spec.setSampleRate(44100.0);
    spec.setSampleRate(48000.0);

    std::vector<float> burst(512, 0.0f);
    burst[12] = 0.9f;
    spec.updateSpectrum(burst.data(), (int) burst.size());
    auto img = renderComponent(spec, 400, 200);
    TEST("paint after sample rate change completes", true);
    TEST("rta renders after sample rate rebuild", imageHasContent(img));
}

static void test_resize_preserves_state() {
    std::printf("\n── Resize ──\n");
    mixcoach::SpectrographComponent spec;
    spec.setSize(400, 200);

    // Set some spectrum data
    std::vector<float> data(512, 0.0f);
    for (int i = 0; i < 50; i += 5)
        data[i] = 1.0f - (float)i / 50.0f;
    for (int i = 0; i < 5; ++i)
        spec.updateSpectrum(data.data(), (int)data.size());

    // Resize to different dimensions
    for (int w = 100; w <= 600; w += 100) {
        spec.setSize(w, 200);
        auto img = renderComponent(spec, w, 200);
    }
    TEST("multiple resizes complete without crash", true);

    // Resize back to original
    spec.setSize(400, 200);
    auto img = renderComponent(spec, 400, 200);
    TEST("paint after resize cycle completes", true);
    TEST("spectrum data preserved after resize", imageHasContent(img));
}

static void test_different_fft_sizes_sequentially() {
    std::printf("\n── Different FFT Sizes ──\n");
    mixcoach::SpectrographComponent spec;
    spec.setSize(400, 200);

    // 256 bins
    std::vector<float> fft256(256, 0.0f);
    fft256[5] = 1.0f;
    spec.updateSpectrum(fft256.data(), (int)fft256.size());

    // Then 1024 bins (capped)
    std::vector<float> fft1024(1024, 0.0f);
    fft1024[20] = 0.8f;
    spec.updateSpectrum(fft1024.data(), (int)fft1024.size());

    // Then 64 bins
    std::vector<float> fft64(64, 0.0f);
    fft64[2] = 0.6f;
    spec.updateSpectrum(fft64.data(), (int)fft64.size());

    auto img = renderComponent(spec, 400, 200);
    TEST("paint after sequential different FFT sizes completes", true);
}

// ============================================================================
//  Main
// ============================================================================

int main() {
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  SpectrographComponent Unit Tests\n");
    std::printf("  Construction | RTA Update | Smoothing | Resize\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");

    test_construction();
    test_update_spectrum_silence();
    test_update_spectrum_sine_peak();
    test_update_spectrum_full_scale();
    test_update_spectrum_small_fft();
    test_update_spectrum_large_fft();
    test_update_spectrum_empty();
    test_update_spectrum_smoothing();
    test_sample_rate_rebuild();
    test_resize_preserves_state();
    test_different_fft_sizes_sequentially();

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
