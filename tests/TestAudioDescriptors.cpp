// ═══════════════════════════════════════════════════════════════════════════
//  TestAudioDescriptors.cpp — Unit tests para descriptores FFT de alto nivel
//  Valida transientRatio, crestPerBand[6] y stereoWidthPerBand[6] con
//  señales sintéticas conocidas (seno, ruido, transiente, estéreo)
//
//  Build:  cmake --build build --config Release --target TestAudioDescriptors
//  Run:    build/tests/Release/TestAudioDescriptors.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <array>
#include <vector>
#include <memory>
#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>

// ═══════════════════════════════════════════════════════════════════════════
//  CONSTANTES
// ═══════════════════════════════════════════════════════════════════════════

static constexpr int    kFftSize     = 256;
static constexpr int    kFftOrder    = 8;
static constexpr int    kNumChunks   = 4;
static constexpr int    kNumSamples  = kFftSize * kNumChunks; // 1024
static constexpr float  kSampleRate  = 48000.0f;
static constexpr int    kNumBands    = 6;
static constexpr double kPi          = 3.14159265358979323846;

// Bin ranges for each band (from PluginEditor.cpp)
static constexpr int kBandStarts[kNumBands] = { 1, 1, 2, 6, 19, 45 };
static constexpr int kBandEnds[  kNumBands] = { 1, 2, 6, 19, 45, 88 };

static constexpr const char* kBandLabels[kNumBands] = {
    "Sub", "Bass", "LoMid", "HiMid", "Pres", "Air"
};

// ═══════════════════════════════════════════════════════════════════════════
//  TEST RUNNER (mismo patrón que Tests existentes)
// ═══════════════════════════════════════════════════════════════════════════

static int gTestsPassed = 0;
static int gTestsFailed = 0;
static int gTestsSkipped = 0;

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

// ═══════════════════════════════════════════════════════════════════════════
//  SIGNAL GENERATORS
// ═══════════════════════════════════════════════════════════════════════════

static void fillSine(float* left, float* right, int n,
                     float freq, float sr, float amp = 0.5f)
{
    for (int i = 0; i < n; ++i) {
        float s = amp * std::sin(2.0f * float(kPi) * freq * i / sr);
        left[i] = s;
        right[i] = s; // mono
    }
}

static void fillNoise(float* left, float* right, int n, float amp = 0.5f)
{
    for (int i = 0; i < n; ++i) {
        float s = amp * (2.0f * (float)rand() / (float)RAND_MAX - 1.0f);
        left[i] = s;
        right[i] = s; // mono
    }
}

static void fillTransient(float* left, float* right, int n, float amp = 1.0f)
{
    std::memset(left,  0, n * sizeof(float));
    std::memset(right, 0, n * sizeof(float));
    // Impulso agresivo en sample 32 + cola silenciosa
    left[32]  = amp;
    right[32] = amp;
}

static void fillStereoSine(float* left, float* right, int n,
                           float freq, float sr, float phaseDiff,
                           float amp = 0.5f)
{
    for (int i = 0; i < n; ++i) {
        float phase = 2.0f * float(kPi) * freq * i / sr;
        left[i]  = amp * std::sin(phase);
        right[i] = amp * std::sin(phase + phaseDiff);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  DESCRIPTOR COMPUTATION (mismo algoritmo que PluginEditor.cpp)
// ═══════════════════════════════════════════════════════════════════════════

struct DescriptorResult {
    float peakCombined   = -100.0f;
    float rmsCombined    = -100.0f;
    float blockCrest     = 0.0f;    // dB (peak - rms)
    float transientRatio = 0.0f;    // adimensional
    float crestPerBand[kNumBands]      = { 0.0f };
    float stereoWidthPerBand[kNumBands]= { 0.0f };

    void print() const {
        std::printf("    peakCombined=%.1f dB  rmsCombined=%.1f dB  blockCrest=%.1f dB  transientRatio=%.2f\n",
                    peakCombined, rmsCombined, blockCrest, transientRatio);
        std::printf("    Crest per band (dB):  ");
        for (int b = 0; b < kNumBands; ++b)
            std::printf("%s:%.1f  ", kBandLabels[b], crestPerBand[b]);
        std::printf("\n    Stereo width (0-1):   ");
        for (int b = 0; b < kNumBands; ++b)
            std::printf("%s:%.3f  ", kBandLabels[b], stereoWidthPerBand[b]);
        std::printf("\n");
    }
};

// Pool de estado FFT (reutilizado entre llamadas, como en PluginEditor)
struct FFTState {
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> hann;
    bool prepared = false;
};

static FFTState s_fftState;

static void ensureFFT()
{
    if (s_fftState.prepared) return;
    s_fftState.fft = std::make_unique<juce::dsp::FFT>(kFftOrder);
    s_fftState.hann.resize(kFftSize);
    for (int i = 0; i < kFftSize; ++i)
        s_fftState.hann[i] = 0.5f * (1.0f - std::cos(2.0f * float(kPi) * i / (kFftSize - 1)));
    s_fftState.prepared = true;
}

// Helpers dB (evitan dependencia de juce_audio_basics)
static inline float gainToDb(float g) noexcept {
    return (g > 1e-10f) ? 20.0f * std::log10(g) : -100.0f;
}
static inline float dbToGain(float db) noexcept {
    return (db > -90.0f) ? std::pow(10.0f, db / 20.0f) : 0.0f;
}

// Replica exacta del algoritmo en PluginEditor.cpp backgroundRunLoop()
static DescriptorResult computeDescriptors(const float* left, const float* right, int n,
                                            float prevCrestAvg = -1.0f)
{
    DescriptorResult r;
    ensureFFT();

    // ─── 1. Peak / RMS (en dB) ──────────────────────────────────────────
    float peakL = 0.0f, peakR = 0.0f;
    double sumSqL = 0.0, sumSqR = 0.0;
    for (int i = 0; i < n; ++i) {
        float absL = std::abs(left[i]);
        float absR = std::abs(right[i]);
        if (absL > peakL) peakL = absL;
        if (absR > peakR) peakR = absR;
        sumSqL += (double)left[i] * left[i];
        sumSqR += (double)right[i] * right[i];
    }
    float peakDbL = gainToDb(peakL);
    float peakDbR = gainToDb(peakR);
    float rmsDbL  = gainToDb((float)std::sqrt(sumSqL / n));
    float rmsDbR  = gainToDb((float)std::sqrt(sumSqR / n));

    r.peakCombined = std::max(peakDbL, peakDbR);
    r.rmsCombined  = std::max(rmsDbL,  rmsDbR);
    r.blockCrest   = r.peakCombined - r.rmsCombined;

    // ─── 2. Transient Ratio (vs running average, como en el PluginEditor) ─
    if (r.blockCrest > 2.0f && prevCrestAvg > 0.0f)
        r.transientRatio = r.blockCrest / prevCrestAvg;
    else
        r.transientRatio = 0.0f;

    // ─── 3. FFT per-band analysis ──────────────────────────────────────
    if (n < kFftSize) return r;

    int numChunks = std::min(kNumChunks, n / kFftSize);
    float bandPeakMag[kNumBands]  = { 0.0f };
    float bandAvgMag[kNumBands]   = { 0.0f };
    float bandWidthSum[kNumBands] = { 0.0f };
    int bandCount[kNumBands]      = { 0 };
    int totalWindows = 0;

    for (int ch = 0; ch < numChunks; ++ch) {
        int off = ch * kFftSize;
        std::array<float, kFftSize * 2> mfft{}, lfft{}, rfft{};

        for (int i = 0; i < kFftSize; ++i) {
            int idx = off + i;
            float m = (left[idx] + right[idx]) * 0.5f;
            mfft[i*2]   = m * s_fftState.hann[i];
            mfft[i*2+1] = 0.0f;
            lfft[i*2]   = left[idx] * s_fftState.hann[i];
            lfft[i*2+1] = 0.0f;
            rfft[i*2]   = right[idx] * s_fftState.hann[i];
            rfft[i*2+1] = 0.0f;
        }

        s_fftState.fft->performRealOnlyForwardTransform(mfft.data());
        s_fftState.fft->performRealOnlyForwardTransform(lfft.data());
        s_fftState.fft->performRealOnlyForwardTransform(rfft.data());

        for (int b = 0; b < kNumBands; ++b) {
            // Crest per band (mid signal)
            float maxMag = 0.0f, sumMag = 0.0f;
            int bc = 0;
            for (int bin = kBandStarts[b]; bin < kBandEnds[b] && bin < 128; ++bin) {
                float re = mfft[bin*2], im = mfft[bin*2+1];
                float mag = std::sqrt(re*re + im*im);
                sumMag += mag; bc++;
                if (mag > maxMag) maxMag = mag;
            }
            if (bc > 0) {
                bandPeakMag[b] += maxMag;
                bandAvgMag[b]  += sumMag / (float)bc;
                bandCount[b]++;
            }

            // Stereo width per band (L vs R)
            float lSum = 0.0f, rSum = 0.0f;
            int wc = 0;
            for (int bin = kBandStarts[b]; bin < kBandEnds[b] && bin < 128; ++bin) {
                float lRe = lfft[bin*2], lIm = lfft[bin*2+1];
                float rRe = rfft[bin*2], rIm = rfft[bin*2+1];
                lSum += std::sqrt(lRe*lRe + lIm*lIm);
                rSum += std::sqrt(rRe*rRe + rIm*rIm);
                wc++;
            }
            if (wc > 0) {
                float lA = lSum / (float)wc;
                float rA = rSum / (float)wc;
                float mx = std::max(lA, rA);
                bandWidthSum[b] += (mx > 1e-10f) ? std::abs(lA - rA) / mx : 0.0f;
            }
        }
        totalWindows++;
    }

    // Aggregate across chunks
    for (int b = 0; b < kNumBands; ++b) {
        if (bandCount[b] > 0 && totalWindows > 0) {
            float avgPeak = bandPeakMag[b] / (float)bandCount[b];
            float avgAvg  = bandAvgMag[b]  / (float)bandCount[b];
            float crestRatio = (avgAvg > 1e-10f) ? avgPeak / avgAvg : 1.0f;
            if (crestRatio > 1.0f)
                r.crestPerBand[b] = 20.0f * std::log10(crestRatio);
            else
                r.crestPerBand[b] = 0.0f;

            r.stereoWidthPerBand[b] = bandWidthSum[b] / (float)totalWindows;
        }
    }

    return r;
}

// ═══════════════════════════════════════════════════════════════════════════
//  TESTS
// ═══════════════════════════════════════════════════════════════════════════

// ─── 1. Sine 1kHz mono ─────────────────────────────────────────────────────
//   - Energía concentrada en bandas 2-3 (LoMid, HiMid)
//   - crestPerBand alto en esas bandas (pico puro)
//   - stereoWidthPerBand ~0 (mono)
static void test_sine_mono()
{
    std::printf("\n── Sine 1kHz mono ──\n");
    float left[kNumSamples], right[kNumSamples];
    fillSine(left, right, kNumSamples, 1000.0f, kSampleRate);

    auto r = computeDescriptors(left, right, kNumSamples);
    r.print();

    // Band 2 (LoMid, bins 2-6): ~500-1125Hz → contiene la frecuencia 1kHz
    TEST("Sine 1kHz: crestPerBand[2] > 3 dB (LoMid has strong peak)",
         r.crestPerBand[2] > 3.0f);

    // Bandas Sub/Bass deben tener crest bajo o 0 (sin energía)
    TEST("Sine 1kHz: crestPerBand[0] < 2 dB (Sub has no energy)",
         r.crestPerBand[0] < 2.0f);
    TEST("Sine 1kHz: crestPerBand[1] < 5 dB (Bass has minimal energy)",
         r.crestPerBand[1] < 5.0f);

    // Mono → stereoWidth ~0
    for (int b = 0; b < kNumBands; ++b)
        TEST_NEAR("Sine 1kHz: stereoWidth[band] ~0 (mono)",
                  r.stereoWidthPerBand[b], 0.0f, 1e-4f);

    // Peak debe ser ~ -6 dB (amplitud 0.5)
    TEST_NEAR("Sine 1kHz: peakCombined ~ -6 dB", r.peakCombined, -6.0f, 1.0f);
}

// ─── 2. Sine 80Hz mono (sub-bass) ─────────────────────────────────────────
//   - Energía concentrada en bandas 0-1 (Sub, Bass)
//   - Las bandas altas deben tener crest bajo
static void test_sine_80hz()
{
    std::printf("\n── Sine 80Hz mono (sub-bass) ──\n");
    float left[kNumSamples], right[kNumSamples];
    fillSine(left, right, kNumSamples, 80.0f, kSampleRate);

    auto r = computeDescriptors(left, right, kNumSamples);
    r.print();

    // 80Hz → bin = 80 / (48000/256) = 0.427 → energía en bin 1 (Sub range: 1-1)
    TEST("Sine 80Hz: crestPerBand[0] < 2 dB (single-bin band, crest ≈ 0)",
         r.crestPerBand[0] < 2.0f);

    // Bandas altas deben tener crest bajo
    TEST("Sine 80Hz: crestPerBand[3] < 8 dB (HiDim noise floor)",
         r.crestPerBand[3] < 8.0f);
    TEST("Sine 80Hz: crestPerBand[4] < 6 dB (Pres noise floor)",
         r.crestPerBand[4] < 6.0f);
}

// ─── 3. White noise mono ───────────────────────────────────────────────────
//   - Crest bajo en todas las bandas (~3-8dB)
//   - stereoWidth ~0
static void test_noise_mono()
{
    std::printf("\n── White noise mono ──\n");
    float left[kNumSamples], right[kNumSamples];
    fillNoise(left, right, kNumSamples);

    auto r = computeDescriptors(left, right, kNumSamples);
    r.print();

    // Noise should have moderate crest across all bands (not extremely high like a sine)
    for (int b = 0; b < kNumBands; ++b) {
        char label[64];
        std::snprintf(label, sizeof(label),
            "Noise mono: crestPerBand[%d] < 12 dB", b);
        TEST(label, r.crestPerBand[b] < 12.0f);
    }

    // Mono → stereoWidth ~0
    for (int b = 0; b < kNumBands; ++b)
        TEST_NEAR("Noise mono: stereoWidth[band] ~0",
                  r.stereoWidthPerBand[b], 0.0f, 1e-4f);
}

// ─── 4. Transient (impulso) ───────────────────────────────────────────────
//   - blockCrest muy alto (peak 0dB, RMS bajo)
//   - transientRatio alto en segunda iteración
//   - crestPerBand alto en TODAS las bandas (impulso = todas las frecuencias)
static void test_transient()
{
    std::printf("\n── Transient (impulse) ──\n");
    float left[kNumSamples], right[kNumSamples];
    fillTransient(left, right, kNumSamples, 1.0f);

    // ─── Primera iteración: establece el running average ────────────────
    auto r1 = computeDescriptors(left, right, kNumSamples, -1.0f);
    std::printf("  Pass 1 (set avg):\n  ");
    r1.print();

    // blockCrest debe ser alto (peak ~0 dB, RMS ~ -20 a -15 dB)
    TEST("Transient: blockCrest > 10 dB in pass 1", r1.blockCrest > 10.0f);
    TEST("Transient: peakCombined > -3 dB (near 0 dBFS)",
         r1.peakCombined > -3.0f);

    // crestPerBand BAJO en todas las bandas (impulso = espectro plano = peak/avg ≈ 1.0)
    // Un impulso tiene magnitud FFT plana → todos los bins iguales → crest ≈ 0 dB
    bool allCrestLow = true;
    for (int b = 0; b < kNumBands; ++b)
        if (r1.crestPerBand[b] > 3.0f) allCrestLow = false;
    TEST("Transient: crestPerBand < 3 dB across all bands (flat spectrum)", allCrestLow);

    // ─── Segunda iteración: señal silenciosa (cola del impulso) ────────
    // El running average ya está caliente (~r1.blockCrest*0.1)
    // Ahora la señal post-impulso tiene blockCrest mucho menor
    float prevAvg = r1.blockCrest * 0.1f; // slotCrestAvgs update
    float silence[kNumSamples] = { 0.0f };
    auto r2 = computeDescriptors(silence, silence, kNumSamples, prevAvg);
    std::printf("  Pass 2 (post-impulse silence):\n  ");
    r2.print();

    // Silence after impulse: crestPerBand = 0 (no energy)
    // Transient ratio = 0 (blockCrest = 0, not > 2.0)
    bool postCrestZero = true;
    for (int b = 0; b < kNumBands; ++b)
        if (r2.crestPerBand[b] > 0.5f) postCrestZero = false;
    TEST("Transient: post-impulse silence has crestPerBand ~0", postCrestZero);
}

// ─── 5. Stereo sine (L/R fase diferente, 60°) ─────────────────────────────
//   - crestPerBand similar al sine mono
//   - stereoWidthPerBand > 0 en bandas con energía
static void test_stereo_sine()
{
    std::printf("\n── Stereo sine 500Hz (60° phase diff) ──\n");
    float left[kNumSamples], right[kNumSamples];
    fillStereoSine(left, right, kNumSamples, 500.0f, kSampleRate,
                   float(kPi) / 3.0f); // 60°

    auto r = computeDescriptors(left, right, kNumSamples);
    r.print();

    // 500Hz → bin = 500 / 187.5 ≈ 2.67 → energía en bandas 2-3
    TEST("Stereo sine: crestPerBand[2] > 5 dB (LoMid)",
         r.crestPerBand[2] > 5.0f);

    // Stereo width debe ser > 0 en bandas con energía (L≠R)
    float widthEnergy = r.stereoWidthPerBand[2] + r.stereoWidthPerBand[3];
    TEST("Stereo sine: stereoWidth > 0.01 in energetic bands (L/R phase diff)",
         widthEnergy > 0.01f);
}

// ─── 6. Hard panned sine (L only) ─────────────────────────────────────────
//   - stereoWidth = 1.0 en bandas con energía (solo un canal)
static void test_hard_panned()
{
    std::printf("\n── Hard-panned sine 200Hz (L only) ──\n");
    float left[kNumSamples], right[kNumSamples];
    fillSine(left, right, kNumSamples, 200.0f, kSampleRate); // fill mono
    // Zero out right channel
    std::memset(right, 0, kNumSamples * sizeof(float));

    auto r = computeDescriptors(left, right, kNumSamples);
    r.print();

    // Band 1-2 should have energy and stereoWidth should be very high (R=0)
    float totalWidth = 0.0f;
    for (int b = 0; b < kNumBands; ++b)
        totalWidth += r.stereoWidthPerBand[b];
    TEST("Hard-panned: stereoWidth sum > 0.5 across bands (one channel only)",
         totalWidth > 0.5f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  MAIN
// ═══════════════════════════════════════════════════════════════════════════

int main()
{
    std::printf("══════════════════════════════════════════════════════════\n");
    std::printf("  AudioDescriptor FFT Unit Tests                          \n");
    std::printf("  synthetic: sine 1kHz | sine 80Hz | noise | transient     \n");
    std::printf("             stereo sine | hard-panned                     \n");
    std::printf("══════════════════════════════════════════════════════════\n");

    test_sine_mono();
    test_sine_80hz();
    test_noise_mono();
    test_transient();
    test_stereo_sine();
    test_hard_panned();

    std::printf("\n══════════════════════════════════════════════════════════\n");
    std::printf("  Results: %d passed, %d failed, %d skipped\n",
                gTestsPassed, gTestsFailed, gTestsSkipped);
    std::printf("══════════════════════════════════════════════════════════\n");

    return gTestsFailed > 0 ? 1 : 0;
}
