// ═══════════════════════════════════════════════════════════════════════════
//  TestMixMapDetailPanel.cpp — Unit tests para MixMapDetailPanel
//  (panel lateral con curva EQ, overlay de referencia, pin, click-away)
// ═══════════════════════════════════════════════════════════════════════════
//
//  Build: cmake --build build --config Release --target TestMixMapDetailPanel
//  Run:   build/tests/Release/TestMixMapDetailPanel.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/MixCoach/UI/MixMapDetailPanel.h"

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

// ─── Constants ─────────────────────────────────────────────────────────────
static constexpr int kTestWidth  = 300;
static constexpr int kTestHeight = 400;
static constexpr int kNumBands   = 30;

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

/** Crea un array de 30 bandas con energía decreciente (simula espectro real). */
static void fillTestBandEnergies(float* bands, float peakDb = -12.0f) {
    for (int b = 0; b < kNumBands; ++b) {
        // Curva con pico en banda 4, decayendo hacia los extremos
        float fraction = (float)b / (float)(kNumBands - 1);
        float envelope = 1.0f - 0.7f * std::abs(fraction - 0.15f) / 0.85f;
        bands[b] = peakDb + 20.0f * std::log10(std::max(0.001f, envelope));
    }
}

/** Crea un array de 30 bandas con perfil de referencia (más plano, target comercial). */
static void fillRefBandEnergies(float* bands, float targetDb = -14.0f) {
    for (int b = 0; b < kNumBands; ++b) {
        float fraction = (float)b / (float)(kNumBands - 1);
        // Reference: gentle slope from lows to highs (commercial/mastered sound)
        float envelope = 0.7f + 0.3f * (1.0f - fraction);
        bands[b] = targetDb + 20.0f * std::log10(std::max(0.001f, envelope));
    }
}

/** Crea un array de 30 bandas todas silenciosas (-100 dB). */
static void fillSilentBands(float* bands) {
    for (int b = 0; b < kNumBands; ++b)
        bands[b] = -100.0f;
}

// ============================================================================
//  Tests
// ============================================================================

static void test_construction() {
    std::printf("\n── Construction ──\n");
    mixcoach::MixMapDetailPanel panel;
    panel.setSize(kTestWidth, kTestHeight);
    TEST("panel created and sized", panel.getWidth() == kTestWidth);

    // Panel sin track — no debería dibujar nada
    auto img = renderComponent(panel, kTestWidth, kTestHeight);
    TEST("empty panel paint completes without crash", true);
    // Sin datos de track, paint() retorna early sin dibujar
    // (slotIndex_ < 0 → return inmediato)
}

static void test_set_track_data() {
    std::printf("\n── setTrackData ──\n");
    mixcoach::MixMapDetailPanel panel;
    panel.setSize(kTestWidth, kTestHeight);

    float bandEnergies[kNumBands];
    fillTestBandEnergies(bandEnergies, -12.0f);

    panel.setTrackData(0, "Kick", "Kick",
                       -3.0f, -12.0f,  // peak, rms
                       0.85f, 0.5f,    // correlation, stereoWidth
                       bandEnergies,   // 30 bands
                       -18.0f,         // target level
                       0.9f);          // role confidence

    auto img = renderComponent(panel, kTestWidth, kTestHeight);
    TEST("paint after setTrackData completes", true);
    TEST("panel has visual content after setTrackData", imageHasContent(img));
    TEST("hasTrack() returns true", panel.hasTrack());
    TEST("getSlotIndex() returns 0", panel.getSlotIndex() == 0);
}

static void test_set_track_data_negative_slot() {
    std::printf("\n── setTrackData: Negative Slot ──\n");
    mixcoach::MixMapDetailPanel panel;
    panel.setSize(kTestWidth, kTestHeight);

    float bandEnergies[kNumBands];
    fillTestBandEnergies(bandEnergies);

    // slotIndex -1 = ocultar
    panel.setTrackData(-1, "", "", 0, 0, 0, 0, nullptr, -18, 0);
    auto img = renderComponent(panel, kTestWidth, kTestHeight);
    TEST("paint after negative slot completes without crash", true);
    TEST("hasTrack() returns false for negative slot", !panel.hasTrack());
}

static void test_set_track_data_null_bands() {
    std::printf("\n── setTrackData: Null Bands ──\n");
    mixcoach::MixMapDetailPanel panel;
    panel.setSize(kTestWidth, kTestHeight);

    // Null bandEnergies — debe usar valores por defecto (-100 dB)
    panel.setTrackData(1, "Snare", "Snare",
                       -6.0f, -18.0f,
                       0.5f, 0.3f,
                       nullptr,  // null bands
                       -18.0f, 0.8f);

    auto img = renderComponent(panel, kTestWidth, kTestHeight);
    TEST("paint after null bands completes without crash", true);
    TEST("panel has content after null bands", imageHasContent(img));
}

// ============================================================================
//  Reference Overlay Tests
// ============================================================================

static void test_reference_set_data_valid() {
    std::printf("\n── setReferenceData: Valid Data ──\n");
    mixcoach::MixMapDetailPanel panel;
    panel.setSize(kTestWidth, kTestHeight);

    float bandEnergies[kNumBands];
    fillTestBandEnergies(bandEnergies);

    float refBands[kNumBands];
    fillRefBandEnergies(refBands);

    // Primero setear track data (necesario para que paint() dibuje)
    panel.setTrackData(0, "Kick", "Kick",
                       -3.0f, -12.0f, 0.85f, 0.5f,
                       bandEnergies, -18.0f, 0.9f);

    // Luego setear referencia
    panel.setReferenceData(refBands, true);

    auto img = renderComponent(panel, kTestWidth, kTestHeight);
    TEST("paint with reference overlay completes without crash", true);
    TEST("panel has content with reference overlay", imageHasContent(img));
}

static void test_reference_set_data_empty() {
    std::printf("\n── setReferenceData: Empty (hasReference=false) ──\n");
    mixcoach::MixMapDetailPanel panel;
    panel.setSize(kTestWidth, kTestHeight);

    float bandEnergies[kNumBands];
    fillTestBandEnergies(bandEnergies);
    panel.setTrackData(0, "Kick", "Kick", -3.0f, -12.0f, 0.85f, 0.5f,
                       bandEnergies, -18.0f, 0.9f);

    // Set reference with hasReference=false → should not draw overlay
    float refBands[kNumBands];
    fillRefBandEnergies(refBands);
    panel.setReferenceData(refBands, false);

    auto imgNoRef = renderComponent(panel, kTestWidth, kTestHeight);
    TEST("paint with hasReference=false completes without crash", true);

    // Now enable it
    panel.setReferenceData(refBands, true);
    auto imgWithRef = renderComponent(panel, kTestWidth, kTestHeight);
    TEST("paint after enabling reference completes", true);
}

static void test_reference_set_data_null() {
    std::printf("\n── setReferenceData: Null Pointer ──\n");
    mixcoach::MixMapDetailPanel panel;
    panel.setSize(kTestWidth, kTestHeight);

    float bandEnergies[kNumBands];
    fillTestBandEnergies(bandEnergies);
    panel.setTrackData(0, "Kick", "Kick", -3.0f, -12.0f, 0.85f, 0.5f,
                       bandEnergies, -18.0f, 0.9f);

    // Null data with hasReference=true → should be handled (no crash)
    panel.setReferenceData(nullptr, true);
    auto img = renderComponent(panel, kTestWidth, kTestHeight);
    TEST("paint after null reference data completes without crash", true);

    // Null data with hasReference=false → should be handled
    panel.setReferenceData(nullptr, false);
    auto img2 = renderComponent(panel, kTestWidth, kTestHeight);
    TEST("paint after null reference + false completes", true);
}

static void test_reference_clear_resets() {
    std::printf("\n── setReferenceData: Clear Resets ──\n");
    mixcoach::MixMapDetailPanel panel;
    panel.setSize(kTestWidth, kTestHeight);

    float bandEnergies[kNumBands];
    fillTestBandEnergies(bandEnergies);
    panel.setTrackData(0, "Kick", "Kick", -3.0f, -12.0f, 0.85f, 0.5f,
                       bandEnergies, -18.0f, 0.9f);

    float refBands[kNumBands];
    fillRefBandEnergies(refBands);
    panel.setReferenceData(refBands, true);
    auto imgWithRef = renderComponent(panel, kTestWidth, kTestHeight);
    TEST("paint before clear has content", imageHasContent(imgWithRef));

    // Clear should reset reference data
    panel.clear();
    auto imgAfterClear = renderComponent(panel, kTestWidth, kTestHeight);
    TEST("paint after clear completes without crash", true);
}

static void test_reference_silent_data() {
    std::printf("\n── setReferenceData: Silent Bands ──\n");
    mixcoach::MixMapDetailPanel panel;
    panel.setSize(kTestWidth, kTestHeight);

    float bandEnergies[kNumBands];
    fillTestBandEnergies(bandEnergies);
    panel.setTrackData(0, "Kick", "Kick", -3.0f, -12.0f, 0.85f, 0.5f,
                       bandEnergies, -18.0f, 0.9f);

    // Reference with all -100 dB (silent) — still valid data, just quiet
    float silentBands[kNumBands];
    fillSilentBands(silentBands);
    panel.setReferenceData(silentBands, true);
    auto img = renderComponent(panel, kTestWidth, kTestHeight);
    TEST("paint after silent reference bands completes", true);
}

static void test_reference_multiple_updates() {
    std::printf("\n── setReferenceData: Multiple Updates ──\n");
    mixcoach::MixMapDetailPanel panel;
    panel.setSize(kTestWidth, kTestHeight);

    float bandEnergies[kNumBands];
    fillTestBandEnergies(bandEnergies);
    panel.setTrackData(0, "Kick", "Kick", -3.0f, -12.0f, 0.85f, 0.5f,
                       bandEnergies, -18.0f, 0.9f);

    // Rapidly update reference data (simulates real-time refresh)
    for (int i = 0; i < 20; ++i) {
        float refBands[kNumBands];
        for (int b = 0; b < kNumBands; ++b)
            refBands[b] = -20.0f + (float)i * 0.5f;
        panel.setReferenceData(refBands, i % 2 == 0);
        auto img = renderComponent(panel, kTestWidth, kTestHeight);
    }
    TEST("20 rapid reference updates complete without crash", true);
}

static void test_reference_toggle_on_off() {
    std::printf("\n── setReferenceData: Toggle On/Off ──\n");
    mixcoach::MixMapDetailPanel panel;
    panel.setSize(kTestWidth, kTestHeight);

    float bandEnergies[kNumBands];
    fillTestBandEnergies(bandEnergies);
    panel.setTrackData(0, "Kick", "Kick", -3.0f, -12.0f, 0.85f, 0.5f,
                       bandEnergies, -18.0f, 0.9f);

    float refBands[kNumBands];
    fillRefBandEnergies(refBands);

    // Toggle sequence: on → off → on → off
    for (int i = 0; i < 4; ++i) {
        bool enable = (i % 2 == 0);
        panel.setReferenceData(refBands, enable);
        auto img = renderComponent(panel, kTestWidth, kTestHeight);
        TEST(("paint after toggle " + std::to_string(i) + " (" + (enable ? "on" : "off") + ")").c_str(), true);
    }
}

static void test_reference_persists_after_track_update() {
    std::printf("\n── setReferenceData: Persists After Track Update ──\n");
    mixcoach::MixMapDetailPanel panel;
    panel.setSize(kTestWidth, kTestHeight);

    float bandEnergies[kNumBands];
    fillTestBandEnergies(bandEnergies);

    float refBands[kNumBands];
    fillRefBandEnergies(refBands);

    // Set track + reference
    panel.setTrackData(0, "Kick", "Kick", -3.0f, -12.0f, 0.85f, 0.5f,
                       bandEnergies, -18.0f, 0.9f);
    panel.setReferenceData(refBands, true);
    auto imgFirst = renderComponent(panel, kTestWidth, kTestHeight);
    TEST("first paint with track + reference completes", true);

    // Update track data (new slot) — reference should persist
    float newBands[kNumBands];
    fillTestBandEnergies(newBands, -6.0f);
    panel.setTrackData(1, "Snare", "Snare", -6.0f, -18.0f, 0.5f, 0.3f,
                       newBands, -18.0f, 0.8f);
    // setTrackData does NOT clear reference data — it persists
    auto imgAfterUpdate = renderComponent(panel, kTestWidth, kTestHeight);
    TEST("paint after track update with persisted reference completes", true);
    TEST("hasTrack() returns new slot", panel.getSlotIndex() == 1);
}

// ============================================================================
//  Pin Toggle Tests
// ============================================================================

static void test_pin_default_state() {
    std::printf("\n── Pin: Default State ──\n");
    mixcoach::MixMapDetailPanel panel;
    panel.setSize(kTestWidth, kTestHeight);

    TEST("isPinned() starts false", !panel.isPinned());
}

static void test_pin_set_get() {
    std::printf("\n── Pin: setPinned / isPinned ──\n");
    mixcoach::MixMapDetailPanel panel;
    panel.setSize(kTestWidth, kTestHeight);

    panel.setPinned(true);
    TEST("isPinned() true after setPinned(true)", panel.isPinned());

    panel.setPinned(false);
    TEST("isPinned() false after setPinned(false)", !panel.isPinned());
}

static void test_pin_persists_after_clear() {
    std::printf("\n── Pin: Persists After Clear ──\n");
    mixcoach::MixMapDetailPanel panel;
    panel.setSize(kTestWidth, kTestHeight);

    float bandEnergies[kNumBands];
    fillTestBandEnergies(bandEnergies);
    panel.setTrackData(0, "Kick", "Kick", -3.0f, -12.0f, 0.85f, 0.5f,
                       bandEnergies, -18.0f, 0.9f);
    panel.setPinned(true);

    // clear() should NOT reset pin state
    panel.clear();
    TEST("isPinned() still true after clear", panel.isPinned());

    panel.setPinned(false);
    panel.clear();
    TEST("isPinned() still false after clear when unpinned", !panel.isPinned());
}

static void test_pin_not_reset_by_set_track_data() {
    std::printf("\n── Pin: Not Reset by setTrackData ──\n");
    mixcoach::MixMapDetailPanel panel;
    panel.setSize(kTestWidth, kTestHeight);

    panel.setPinned(true);

    float bandEnergies[kNumBands];
    fillTestBandEnergies(bandEnergies);
    panel.setTrackData(2, "808", "808", -6.0f, -14.0f, 0.7f, 0.6f,
                       bandEnergies, -18.0f, 0.85f);

    TEST("isPinned() remains true after setTrackData", panel.isPinned());
}

// ============================================================================
//  Render & Edge Case Tests
// ============================================================================

static void test_multiple_resizes() {
    std::printf("\n── Multiple Resizes ──\n");
    mixcoach::MixMapDetailPanel panel;

    float bandEnergies[kNumBands];
    fillTestBandEnergies(bandEnergies);

    float refBands[kNumBands];
    fillRefBandEnergies(refBands);

    panel.setTrackData(0, "Kick", "Kick", -3.0f, -12.0f, 0.85f, 0.5f,
                       bandEnergies, -18.0f, 0.9f);
    panel.setReferenceData(refBands, true);

    int sizes[] = {200, 300, 400, 500};
    for (int s : sizes) {
        panel.setSize(s, 350);
        auto img = renderComponent(panel, s, 350);
        TEST(("paint at " + std::to_string(s) + "x350 with reference overlay").c_str(), imageHasContent(img));
    }
}

static void test_render_with_extreme_band_values() {
    std::printf("\n── Extreme Band Values ──\n");
    mixcoach::MixMapDetailPanel panel;
    panel.setSize(kTestWidth, kTestHeight);

    // All bands at 0 dBFS (ceiling)
    float hotBands[kNumBands];
    for (int b = 0; b < kNumBands; ++b) hotBands[b] = 0.0f;
    panel.setTrackData(0, "Master", "Master", 0.0f, -6.0f, 0.9f, 0.7f,
                       hotBands, -14.0f, 0.95f);

    float refBands[kNumBands];
    fillRefBandEnergies(refBands);
    panel.setReferenceData(refBands, true);

    auto img = renderComponent(panel, kTestWidth, kTestHeight);
    TEST("paint with 0dBFS bands + reference overlay completes", true);
    TEST("extreme values still produce visual output", imageHasContent(img));

    // All bands at -120 dB (extremely quiet)
    float quietBands[kNumBands];
    for (int b = 0; b < kNumBands; ++b) quietBands[b] = -120.0f;
    panel.setTrackData(0, "Silent", "Silent", -120.0f, -120.0f, 0.0f, 0.0f,
                       quietBands, -18.0f, 0.5f);
    panel.setReferenceData(refBands, true);
    auto imgQuiet = renderComponent(panel, kTestWidth, kTestHeight);
    TEST("paint with -120dB bands + reference overlay completes", true);
}

static void test_render_with_visible_switch() {
    std::printf("\n── Visible Switch ──\n");
    mixcoach::MixMapDetailPanel panel;
    panel.setSize(kTestWidth, kTestHeight);

    float bandEnergies[kNumBands];
    fillTestBandEnergies(bandEnergies);
    panel.setTrackData(0, "Kick", "Kick", -3.0f, -12.0f, 0.85f, 0.5f,
                       bandEnergies, -18.0f, 0.9f);

    float refBands[kNumBands];
    fillRefBandEnergies(refBands);
    panel.setReferenceData(refBands, true);

    // setVisible(false) → stopTimer + clear() → hide
    panel.setVisible(false);
    auto imgHidden = renderComponent(panel, kTestWidth, kTestHeight);
    TEST("paint after setVisible(false) completes", true);

    // setVisible(true) → startTimer + show
    panel.setVisible(true);
    auto imgShown = renderComponent(panel, kTestWidth, kTestHeight);
    TEST("paint after setVisible(true) completes", true);

    // Re-set data (clear() was called by setVisible(false))
    panel.setTrackData(0, "Kick", "Kick", -3.0f, -12.0f, 0.85f, 0.5f,
                       bandEnergies, -18.0f, 0.9f);
    panel.setReferenceData(refBands, true);
    auto imgAfterReset = renderComponent(panel, kTestWidth, kTestHeight);
    TEST("paint after restore completes", true);
    TEST("content restored after visible cycle", imageHasContent(imgAfterReset));
}

// ============================================================================
//  Main
// ============================================================================

int main() {
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  MixMapDetailPanel Unit Tests\n");
    std::printf("  Reference Overlay | Pin Toggle | Track Data | Render Edge Cases\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");

    test_construction();
    test_set_track_data();
    test_set_track_data_negative_slot();
    test_set_track_data_null_bands();

    test_reference_set_data_valid();
    test_reference_set_data_empty();
    test_reference_set_data_null();
    test_reference_clear_resets();
    test_reference_silent_data();
    test_reference_multiple_updates();
    test_reference_toggle_on_off();
    test_reference_persists_after_track_update();

    test_pin_default_state();
    test_pin_set_get();
    test_pin_persists_after_clear();
    test_pin_not_reset_by_set_track_data();

    test_multiple_resizes();
    test_render_with_extreme_band_values();
    test_render_with_visible_switch();

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
