// ═══════════════════════════════════════════════════════════════════════════
//  TestTelemetryDSP.cpp — Unit tests para el pipeline DSP de TelemetryCollector
//  Cubre: Peak, RMS, FFT, LUFS, Correlation, Crest Factor
// ═══════════════════════════════════════════════════════════════════════════
//
//  Build: cmake --build build --config Release --target TestTelemetryDSP
//  Run:   build/tests/Release/TestTelemetryDSP.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cassert>
#include <vector>
#include <algorithm>

// ─── Dependencias del proyecto (requiere JUCE) ──────────────────────────────
#include "../Source/Messenger/telemetry/TelemetryCollector.h"
#include "../Source/Common/types/Types.h"
#include "../Source/Common/types/Constants.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

// ─── Helpers para generar señales de test ──────────────────────────────────
static std::vector<float> generateSineWave(float amplitude, float freqHz,
                                            float sampleRate, int numSamples)
{
    std::vector<float> buffer(numSamples);
    for (int i = 0; i < numSamples; ++i)
        buffer[i] = amplitude * std::sin(2.0f * (float)M_PI * freqHz * i / sampleRate);
    return buffer;
}

static std::vector<float> generateSquareWave(float amplitude, float freqHz,
                                              float sampleRate, int numSamples)
{
    std::vector<float> buffer(numSamples);
    float period = sampleRate / freqHz;
    for (int i = 0; i < numSamples; ++i) {
        float phase = std::fmod((float)i, period) / period;
        buffer[i] = amplitude * (phase < 0.5f ? 1.0f : -1.0f);
    }
    return buffer;
}

static std::vector<float> generateSilence(int numSamples)
{
    return std::vector<float>(numSamples, 0.0f);
}

static juce::AudioBuffer<float> makeStereoBuffer(const std::vector<float>& left,
                                                  const std::vector<float>& right)
{
    int numSamples = (int)std::min(left.size(), right.size());
    juce::AudioBuffer<float> buffer(2, numSamples);
    buffer.copyFrom(0, 0, left.data(), numSamples);
    buffer.copyFrom(1, 0, right.data(), numSamples);
    return buffer;
}

static juce::AudioBuffer<float> makeMonoBuffer(const std::vector<float>& data)
{
    juce::AudioBuffer<float> buffer(1, (int)data.size());
    buffer.copyFrom(0, 0, data.data(), (int)data.size());
    return buffer;
}

// ============================================================================
//  Tests de TelemetryCollector DSP Pipeline
// ============================================================================

static void test_peak_silence() {
    std::printf("\n── Peak: Silence ──\n");
    mixcoach::TelemetryCollector tc;
    tc.prepare(44100.0, 512);

    auto silence = generateSilence(512);
    auto buffer = makeStereoBuffer(silence, silence);
    auto result = tc.collect(buffer);

    TEST_NEAR("peakLeft = -100 dB for silence", result.peakLeft, -100.0f, 0.5f);
    TEST_NEAR("peakRight = -100 dB for silence", result.peakRight, -100.0f, 0.5f);
}

static void test_peak_sine() {
    std::printf("\n── Peak: Sine Wave ──\n");
    mixcoach::TelemetryCollector tc;
    tc.prepare(44100.0, 512);

    auto sine = generateSineWave(0.5f, 440.0f, 44100.0f, 512);
    auto buffer = makeStereoBuffer(sine, sine);
    auto result = tc.collect(buffer);

    TEST_NEAR("peakLeft ~ -6.02 dB for 0.5 amplitude sine",
              result.peakLeft, -6.02f, 0.5f);
    TEST_NEAR("peakRight ~ -6.02 dB for 0.5 amplitude sine",
              result.peakRight, -6.02f, 0.5f);

    auto sine2 = generateSineWave(0.25f, 440.0f, 44100.0f, 512);
    auto buffer2 = makeStereoBuffer(sine2, sine2);
    auto result2 = tc.collect(buffer2);

    TEST_NEAR("peak ~ -12.04 dB for 0.25 amplitude sine",
              result2.peakLeft, -12.04f, 0.5f);
}

static void test_peak_fullscale() {
    std::printf("\n── Peak: Full Scale ──\n");
    mixcoach::TelemetryCollector tc;
    tc.prepare(44100.0, 512);

    auto sine = generateSineWave(1.0f, 440.0f, 44100.0f, 512);
    auto buffer = makeStereoBuffer(sine, sine);
    auto result = tc.collect(buffer);

    TEST_NEAR("peak ~ 0 dB for full-scale sine", result.peakLeft, 0.0f, 0.5f);
}

static void test_rms_silence() {
    std::printf("\n── RMS: Silence ──\n");
    mixcoach::TelemetryCollector tc;
    tc.prepare(44100.0, 512);

    auto silence = generateSilence(512);
    auto buffer = makeStereoBuffer(silence, silence);
    auto result = tc.collect(buffer);

    TEST_NEAR("rmsLeft = -100 dB for silence", result.rmsLeft, -100.0f, 0.5f);
    TEST_NEAR("rmsRight = -100 dB for silence", result.rmsRight, -100.0f, 0.5f);
}

static void test_rms_sine() {
    std::printf("\n── RMS: Sine Wave ──\n");
    mixcoach::TelemetryCollector tc;
    tc.prepare(44100.0, 512);

    auto sine = generateSineWave(0.5f, 440.0f, 44100.0f, 512);
    auto buffer = makeStereoBuffer(sine, sine);
    auto result = tc.collect(buffer);

    TEST_NEAR("rmsLeft ~ -9.03 dB for 0.5 amplitude sine",
              result.rmsLeft, -9.03f, 0.7f);
    TEST_NEAR("rmsRight ~ -9.03 dB for 0.5 amplitude sine",
              result.rmsRight, -9.03f, 0.7f);

    auto sine2 = generateSineWave(1.0f, 440.0f, 44100.0f, 512);
    auto buffer2 = makeStereoBuffer(sine2, sine2);
    auto result2 = tc.collect(buffer2);

    TEST_NEAR("rms ~ -3.01 dB for full-scale sine",
              result2.rmsLeft, -3.01f, 0.7f);
}

static void test_rms_square() {
    std::printf("\n── RMS: Square Wave ──\n");
    mixcoach::TelemetryCollector tc;
    tc.prepare(44100.0, 512);

    auto square = generateSquareWave(0.5f, 220.0f, 44100.0f, 512);
    auto buffer = makeStereoBuffer(square, square);
    auto result = tc.collect(buffer);

    TEST_NEAR("rms ~ -6.02 dB for 0.5 amplitude square wave",
              result.rmsLeft, -6.02f, 0.5f);
}

static void test_correlation_identical() {
    std::printf("\n── Correlation: Identical L/R ──\n");
    mixcoach::TelemetryCollector tc;
    tc.prepare(44100.0, 512);

    auto sine = generateSineWave(0.5f, 440.0f, 44100.0f, 512);
    auto buffer = makeStereoBuffer(sine, sine);
    auto result = tc.collect(buffer);

    TEST_NEAR("correlation ~ 1.0 for identical L/R", result.correlation, 1.0f, 0.01f);
}

static void test_correlation_opposite() {
    std::printf("\n── Correlation: Opposite L/R ──\n");
    mixcoach::TelemetryCollector tc;
    tc.prepare(44100.0, 512);

    auto sine = generateSineWave(0.5f, 440.0f, 44100.0f, 512);
    std::vector<float> invSine(sine.size());
    for (size_t i = 0; i < sine.size(); ++i) invSine[i] = -sine[i];

    auto buffer = makeStereoBuffer(sine, invSine);
    auto result = tc.collect(buffer);

    TEST_NEAR("correlation ~ -1.0 for opposite L/R", result.correlation, -1.0f, 0.01f);
}

static void test_correlation_mono() {
    std::printf("\n── Correlation: Mono ──\n");
    mixcoach::TelemetryCollector tc;
    tc.prepare(44100.0, 512);

    auto sine = generateSineWave(0.5f, 440.0f, 44100.0f, 512);
    auto buffer = makeMonoBuffer(sine);
    auto result = tc.collect(buffer);

    TEST_NEAR("correlation = 1.0 for mono (single channel)",
              result.correlation, 1.0f, 0.01f);
}

static void test_crest_factor() {
    std::printf("\n── Crest Factor ──\n");
    mixcoach::TelemetryCollector tc;
    tc.prepare(44100.0, 512);

    auto sine = generateSineWave(0.5f, 440.0f, 44100.0f, 512);
    auto buffer = makeStereoBuffer(sine, sine);
    auto result = tc.collect(buffer);

    TEST("crestFactor > 0 for sine wave", result.crestFactor > 0.0f);
    TEST_NEAR("crestFactor ~ 3 dB for sine wave", result.crestFactor, 3.0f, 1.5f);

    auto silence = generateSilence(512);
    auto buffer2 = makeStereoBuffer(silence, silence);
    auto result2 = tc.collect(buffer2);

    TEST_NEAR("crestFactor = 0 for silence", result2.crestFactor, 0.0f, 0.1f);
}

// ─── FFT: diagnostic test ─────────────────────────────────────────────────
// This test verifies that the FFT spectrum responds correctly to sine input
// and that silence produces a near-zero spectrum after enough empty blocks.
static void test_fft_spectrum() {
    std::printf("\n── FFT Spectrum ──\n");
    mixcoach::TelemetryCollector tc;
    tc.prepare(44100.0, 512);

    auto sine = generateSineWave(1.0f, 440.0f, 44100.0f, 512);
    auto silence = generateSilence(512);
    juce::AudioBuffer<float> buffer(2, 512);
    mixcoach::TrackTelemetry result;

    // ── Fill FFT rolling buffer with sine ─────────────────────────────────
    // FFT computed every 4th collect (kFFTInterval = 4). With 17 blocks,
    // block 16 % 4 = 0 triggers FFT. Rolling buffer (1024 samples) is fully
    // saturated with sine data after 17 * 512 = 8704 writes (> 8 full over-writes).
    for (int i = 0; i < 17; ++i) {
        buffer.copyFrom(0, 0, sine.data(), 512);
        buffer.copyFrom(1, 0, sine.data(), 512);
        result = tc.collect(buffer);
    }

    // Verify sine spectrum has a peak
    int peakBin = -1;
    float peakVal = -1.0f;
    for (int i = 0; i < 512; ++i) {
        if (result.spectrum[i] > peakVal) {
            peakVal = result.spectrum[i];
            peakBin = i;
        }
    }
    std::printf("  \xf0\x9f\x94\x8d Sine peak: bin=%d, value=%.4f\n", peakBin, peakVal);
    TEST("sine spectrum has a peak bin", peakBin >= 0);
    TEST_NEAR("peak bin ~ 10 (440Hz at 44.1kHz)", (float)peakBin, 10.0f, 3.0f);
    TEST("sine peak > 0.5 (strong signal)", peakVal > 0.5f);

    // ── Flush rolling buffer with silence ─────────────────────────────────
    // After sine, fill with silence. Rolling buffer is 1024 samples.
    // Need 3 full overwrites (3 * 1024 / 512 = 6 blocks) to be sure.
    // First collect after sine has blockCount_%4=0 so it triggers FFT on
    // the mixed sine+silence data. Collect more until we reach a clean
    // silence-only FFT.
    for (int i = 0; i < 8; ++i) {
        buffer.copyFrom(0, 0, silence.data(), 512);
        buffer.copyFrom(1, 0, silence.data(), 512);
        result = tc.collect(buffer);
    }

    // After 8 silence collects (blocks 17-24), the rolling buffer has been
    // overwritten 8*512 = 4096 times (> 4 full cycles).
    // Block 24 % 4 = 0 triggers the FFT, which should be all silence.
    float maxSilence = 0.0f;
    for (int i = 0; i < 512; ++i)
        maxSilence = std::max(maxSilence, result.spectrum[i]);

    std::printf("  \xf0\x9f\x94\x8d Silence max spectrum bin: %.6f\n", maxSilence);
    std::printf("  \xf0\x9f\x94\x8d Silence spectrum bins 0..15: ");
    for (int i = 0; i < 16; ++i)
        std::printf("%.4f ", result.spectrum[i]);
    std::printf("\n");
    TEST("silence spectrum near-zero", maxSilence < 0.1f);
}

// ─── FFT: Multi-frequency stress test ──────────────────────────────────────
// Generates sine waves at 5 different frequencies and verifies each
// produces a clear peak at the correct FFT bin.
//
// FFT parameters: 1024-point FFT at 44.1kHz → bin width ≈ 43.07 Hz/bin
//   freq  |  expected bin
//   ------+--------------
//   100   |  bin ~2.3
//   440   |  bin ~10.2
//   1000  |  bin ~23.2
//   5000  |  bin ~116.1
//   10000 |  bin ~232.2
static void test_fft_multi_freq_stress() {
    std::printf("\n── FFT Multi-Frequency Stress Test ──\n");

    struct FreqTest {
        const char* label;
        float freqHz;
        float expectedBin;
        float tolerance;
    };

    FreqTest tests[] = {
        {"100 Hz",   100.0f,   2.3f,   2.0f},
        {"440 Hz",   440.0f,  10.2f,   3.0f},
        {"1 kHz",   1000.0f,  23.2f,   3.0f},
        {"5 kHz",   5000.0f, 116.1f,   5.0f},
        {"10 kHz", 10000.0f, 232.2f,   8.0f},
    };

    constexpr float sampleRate = 44100.0f;
    constexpr int blockSize = 512;
    constexpr int numBlocks = 21; // blocks 0..20, FFT at 0,4,8,12,16,20

    juce::AudioBuffer<float> buffer(2, blockSize);

    for (auto t : tests) {
        std::printf("  \xf0\x9f\x94\x8a Testing %s (%.0f Hz)...\n", t.label, t.freqHz);

        mixcoach::TelemetryCollector tc;
        tc.prepare(sampleRate, blockSize);

        // Fill FFT rolling buffer with sine
        auto sine = generateSineWave(1.0f, t.freqHz, sampleRate, blockSize);
        mixcoach::TrackTelemetry result;

        for (int i = 0; i < numBlocks; ++i) {
            buffer.copyFrom(0, 0, sine.data(), blockSize);
            buffer.copyFrom(1, 0, sine.data(), blockSize);
            result = tc.collect(buffer);
        }

        // Find peak bin in spectrum
        int peakBin = -1;
        float peakVal = -1.0f;
        for (int i = 1; i < 256; ++i) { // skip DC (bin 0)
            if (result.spectrum[i] > peakVal) {
                peakVal = result.spectrum[i];
                peakBin = i;
            }
        }

        // Signal-to-noise: ratio of peak to average outside peak region
        float avgNoise = 0.0f;
        int noiseCount = 0;
        for (int i = 0; i < 256; ++i) {
            if (i < peakBin - 3 || i > peakBin + 3) {
                avgNoise += result.spectrum[i];
                noiseCount++;
            }
        }
        avgNoise /= (float)noiseCount;
        float snr = (avgNoise > 0.0001f) ? peakVal / avgNoise : 100.0f;

        std::printf("    peak bin=%d (expected ~%.1f), value=%.4f, SNR=%.1f:1\n",
                    peakBin, t.expectedBin, peakVal, snr);
        std::printf("    bins [%d..%d]: ", std::max(0, peakBin-2), std::min(255, peakBin+2));
        for (int i = std::max(0, peakBin-2); i <= std::min(255, peakBin+2); ++i)
            std::printf("%.4f ", result.spectrum[i]);
        std::printf("\n");

        // Assertions
        char testName[64];
        std::snprintf(testName, sizeof(testName), "%s: peak bin within tolerance", t.label);
        TEST(testName, std::fabs((float)peakBin - t.expectedBin) <= t.tolerance);

        std::snprintf(testName, sizeof(testName), "%s: peak value > 0.1", t.label);
        TEST(testName, peakVal > 0.1f);

        std::snprintf(testName, sizeof(testName), "%s: SNR > 2.5:1", t.label);
        TEST(testName, snr > 2.5f);
    }
}

// ─── LUFS: diagnostic test ────────────────────────────────────────────────
// EBU R128: full-scale 440Hz sine should give roughly -3 to -7 LUFS after
// K-weighting (440Hz is below the 1.5kHz shelf, so minimal boost).
static void test_lufs_basic() {
    std::printf("\n── LUFS: Basic ──\n");
    mixcoach::TelemetryCollector tc;
    tc.prepare(44100.0, 512);

    auto sine = generateSineWave(1.0f, 440.0f, 44100.0f, 512);
    juce::AudioBuffer<float> buffer(2, 512);

    // Fill momentary (400ms ~ 35 blocks) + short-term (3s ~ 259 blocks) queues.
    // 400 blocks x 512 samples = 204800 samples ~ 4.64s at 44.1kHz.
    for (int i = 0; i < 400; ++i) {
        buffer.copyFrom(0, 0, sine.data(), 512);
        buffer.copyFrom(1, 0, sine.data(), 512);
        auto r = tc.collect(buffer);
        juce::ignoreUnused(r);
    }

    buffer.copyFrom(0, 0, sine.data(), 512);
    buffer.copyFrom(1, 0, sine.data(), 512);
    auto result = tc.collect(buffer);

    std::printf("  \xf0\x9f\x94\x8d LUFS: momentary=%.2f short-term=%.2f integrated=%.2f\n",
                result.lufsMomentary, result.lufsShortTerm, result.lufsIntegrated);

    // Full-scale 440Hz sine after K-weighting: expect ~ -3.7 LUFS.
    // The K-weighting filter (pre-shelf + RLB HP 38Hz) attenuates 440Hz
    // slightly below 0 LUFS. Accept ±1.5 LU tolerance.
    TEST_NEAR("momentary LUFS ~ -3.7 for full-scale 440Hz sine",
              result.lufsMomentary, -3.7f, 3.0f);
    TEST_NEAR("short-term LUFS ~ -3.7 for full-scale 440Hz sine",
              result.lufsShortTerm, -3.7f, 3.0f);
    TEST_NEAR("integrated LUFS ~ -3.7 for full-scale 440Hz sine",
              result.lufsIntegrated, -3.7f, 3.0f);
}

static void test_lufs_silence() {
    std::printf("\n── LUFS: Silence ──\n");
    mixcoach::TelemetryCollector tc;
    tc.prepare(44100.0, 512);

    auto silence = generateSilence(512);
    juce::AudioBuffer<float> buffer(2, 512);

    for (int i = 0; i < 50; ++i) {
        buffer.copyFrom(0, 0, silence.data(), 512);
        buffer.copyFrom(1, 0, silence.data(), 512);
        auto r = tc.collect(buffer);
        juce::ignoreUnused(r);
    }

    buffer.copyFrom(0, 0, silence.data(), 512);
    buffer.copyFrom(1, 0, silence.data(), 512);
    auto result = tc.collect(buffer);

    TEST("momentary LUFS very low for silence",
         result.lufsMomentary < -70.0f);
    TEST("short-term LUFS very low for silence",
         result.lufsShortTerm < -70.0f);
}

static void test_multi_block_collect() {
    std::printf("\n── Multi-block Collection ──\n");
    mixcoach::TelemetryCollector tc;
    tc.prepare(44100.0, 512);

    auto sine = generateSineWave(0.5f, 440.0f, 44100.0f, 512);
    juce::AudioBuffer<float> buffer(2, 512);

    for (int block = 0; block < 10; ++block) {
        buffer.copyFrom(0, 0, sine.data(), 512);
        buffer.copyFrom(1, 0, sine.data(), 512);
        auto result = tc.collect(buffer);

        TEST("active = true in collect()", result.active == true);
        TEST("peakLeft > -60 for non-silence", result.peakLeft > -60.0f);
        TEST("rmsLeft > -60 for non-silence", result.rmsLeft > -60.0f);
    }
}

static void test_track_telemetry_fields() {
    std::printf("\n── TrackTelemetry Fields ──\n");
    mixcoach::TelemetryCollector tc;
    tc.prepare(44100.0, 512);

    auto sine = generateSineWave(0.5f, 440.0f, 44100.0f, 512);
    auto buffer = makeStereoBuffer(sine, sine);
    auto result = tc.collect(buffer);

    TEST("lufsMomentary is set", result.lufsMomentary > -101.0f);
    TEST("lufsShortTerm is set", result.lufsShortTerm > -101.0f);
    TEST("lufsIntegrated is set", result.lufsIntegrated > -101.0f);
    TEST("loudnessRange >= 0", result.loudnessRange >= 0.0f);
    TEST("sampleL is set", result.sampleL >= -1.0f && result.sampleL <= 1.0f);
    TEST("sampleR is set", result.sampleR >= -1.0f && result.sampleR <= 1.0f);
    TEST_NEAR("lufsTruePeak ~ peakLeft", result.lufsTruePeak, result.peakLeft, 0.1f);
}

static void test_different_amplitudes() {
    std::printf("\n── Different Amplitudes ──\n");
    // Peak detection is stateless (per-block), so reusing tc across
    // iterations is safe. The internal block count accumulates but
    // does not affect instantaneous peak readings.
    mixcoach::TelemetryCollector tc;
    tc.prepare(44100.0, 512);

    struct { float amp; float peakDb; } tests[] = {
        {1.0f, 0.0f}, {0.5f, -6.02f}, {0.25f, -12.04f},
        {0.125f, -18.06f}, {0.0625f, -24.08f}
    };

    for (auto t : tests) {
        auto sine = generateSineWave(t.amp, 440.0f, 44100.0f, 512);
        auto buffer = makeStereoBuffer(sine, sine);
        auto result = tc.collect(buffer);
        TEST_NEAR("peak scales with amplitude", result.peakLeft, t.peakDb, 0.5f);
    }
}

// ============================================================================
//  Main
// ============================================================================

int main() {
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  TelemetryCollector DSP Pipeline Tests\n");
    std::printf("  Peak  |  RMS  |  FFT  |  LUFS  |  Correlation  |  Crest\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");

    test_peak_silence();
    test_peak_sine();
    test_peak_fullscale();
    test_rms_silence();
    test_rms_sine();
    test_rms_square();
    test_correlation_identical();
    test_correlation_opposite();
    test_correlation_mono();
    test_crest_factor();
    test_fft_spectrum();
    test_fft_multi_freq_stress();
    test_lufs_basic();
    test_lufs_silence();
    test_multi_block_collect();
    test_track_telemetry_fields();
    test_different_amplitudes();

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
