// ═══════════════════════════════════════════════════════════════════════════
//  TestSpectralBands.cpp — Unit test para computeSpectralBands (sample rate dinámico)
// ═══════════════════════════════════════════════════════════════════════════
//
//  Verifica que el cálculo dinámico de bins espectrales funcione correctamente
//  para distintos sample rates (44100, 48000, 96000) y que coincida con la
//  tabla hardcodeada kSpectralBandBins a 44100Hz/1024-FFT.
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstring>

// ─── Standalone test: include Constants directly ────────────────────────────
#include "../Source/Common/types/Constants.h"

// ─── Simple test framework ──────────────────────────────────────────────────
static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST(name, expr)                                                          \
    do {                                                                          \
        if (!(expr)) {                                                            \
            std::printf("  FAIL  %s (%s:%d)\n", name, __FILE__, __LINE__);        \
            ++g_testsFailed;                                                      \
        } else {                                                                  \
            std::printf("  PASS  %s\n", name);                                    \
            ++g_testsPassed;                                                      \
        }                                                                         \
    } while (false)

using namespace mixcoach;

// ═══════════════════════════════════════════════════════════════════════════════
//  Tests
// ═══════════════════════════════════════════════════════════════════════════════

static void testValidate44100()
{
    std::printf("\n── Validate at 44100Hz (matches hardcoded table) ──\n");

    TEST("validateSpectralBands44100() returns true", validateSpectralBands44100());
}

static void testComputeAt44100()
{
    std::printf("\n── Compute at 44100Hz / 1024-FFT ──\n");

    int bins[30][2];
    computeSpectralBands(44100.0, 1024, bins);

    // Cada banda debe coincidir con la tabla hardcodeada
    bool allMatch = true;
    for (int i = 0; i < kNumSpectralBands; ++i) {
        if (bins[i][0] != kSpectralBandBins[i][0] || bins[i][1] != kSpectralBandBins[i][1]) {
            std::printf("  Band %d mismatch: computed=[%d,%d] hardcoded=[%d,%d]\n",
                        i, bins[i][0], bins[i][1], kSpectralBandBins[i][0], kSpectralBandBins[i][1]);
            allMatch = false;
        }
    }
    TEST("All 30 bands match hardcoded at 44100Hz", allMatch);
}

static void testComputeAt48000()
{
    std::printf("\n── Compute at 48000Hz / 1024-FFT ──\n");

    int bins[30][2];
    computeSpectralBands(48000.0, 1024, bins);

    // A 48000Hz, bin frequency = bin * 48000 / 1024 ≈ bin * 46.875 Hz
    // La banda 0 debe ser ~0-1 (0-43Hz → 0-1 bin)
    TEST("Band 0 start = 0", bins[0][0] == 0);
    TEST("Band 0 end >= 1", bins[0][1] >= 1);

    // La banda 1 (43-86Hz) debe comenzar en bin ~1
    TEST("Band 1 start in range [1,2]", bins[1][0] >= 1 && bins[1][0] <= 2);

    // Nyquist a 48000Hz = 24000Hz, bin max = 512
    // La banda 29 (15294-16458Hz) debe estar dentro del rango
    TEST("Band 29 end <= 512", bins[29][1] <= 512);

    // Todas las bandas deben ser monótonamente crecientes (end > start)
    bool allMonotonic = true;
    for (int i = 0; i < kNumSpectralBands; ++i) {
        if (bins[i][1] <= bins[i][0]) { allMonotonic = false; break; }
    }
    TEST("All bands monotonic (end > start)", allMonotonic);
}

static void testComputeAt96000()
{
    std::printf("\n── Compute at 96000Hz / 1024-FFT ──\n");

    int bins[30][2];
    computeSpectralBands(96000.0, 1024, bins);

    // A 96000Hz, bin frequency = bin * 96000 / 1024 ≈ bin * 93.75 Hz
    // La banda 0 (0-43Hz) debe ser bin 0-0 o 0-1
    TEST("Band 0 start = 0", bins[0][0] == 0);

    // La banda 2 (86-129Hz) → bin ~1
    TEST("Band 2 start in range [0,2]", bins[2][0] >= 0 && bins[2][0] <= 2);

    // Nyquist a 96000Hz = 48000Hz, bin max = 512
    // Las frecuencias > 48000Hz no existen, pero las bandas llegan hasta 16458Hz
    // que es bien menor al Nyquist
    TEST("Band 29 end <= 512", bins[29][1] <= 512);
}

static void testRegionCoverage()
{
    std::printf("\n── Region Coverage (all 6 regions covered) ──\n");

    // Sin importar el sample rate, las 6 regiones deben cubrir las 30 bandas
    int totalBands = 0;
    for (int r = 0; r < kNumRegions; ++r) {
        totalBands += (kRegionBands[r][1] - kRegionBands[r][0]);
    }
    TEST("6 regions cover all 30 bands", totalBands == kNumSpectralBands);

    // Las regiones deben ser contiguas y sin huecos
    bool contiguous = true;
    for (int r = 1; r < kNumRegions; ++r) {
        if (kRegionBands[r][0] != kRegionBands[r - 1][1]) { contiguous = false; break; }
    }
    TEST("Regions are contiguous", contiguous);
}

static void testClamping()
{
    std::printf("\n── Clamping (edge cases) ──\n");

    // Sample rate extremadamente bajo: bins deben clamparse a al menos 1
    int bins[30][2];
    computeSpectralBands(8000.0, 1024, bins);

    bool allValid = true;
    for (int i = 0; i < kNumSpectralBands; ++i) {
        if (bins[i][1] <= bins[i][0] || bins[i][0] < 0 || bins[i][1] > 512) {
            allValid = false;
            break;
        }
    }
    TEST("All bands valid at 8000Hz (low sample rate)", allValid);

    // FFT size pequeño: bins deben no exceder maxBin
    computeSpectralBands(44100.0, 256, bins);
    bool withinBounds = true;
    for (int i = 0; i < kNumSpectralBands; ++i) {
        if (bins[i][1] > 128) { withinBounds = false; break; }
    }
    TEST("All bands within 128 bins at 256-FFT", withinBounds);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════════

int main()
{
    std::printf("╔══════════════════════════════════════════════════════════╗\n");
    std::printf("║        SpectralBands (Dynamic Sample Rate) Tests        ║\n");
    std::printf("╚══════════════════════════════════════════════════════════╝\n");

    testValidate44100();
    testComputeAt44100();
    testComputeAt48000();
    testComputeAt96000();
    testRegionCoverage();
    testClamping();

    int total = g_testsPassed + g_testsFailed;
    std::printf("\n══════════════════════════════════════════════════════════\n");
    std::printf("  Results: %d/%d passed, %d failed\n", g_testsPassed, total, g_testsFailed);
    std::printf("══════════════════════════════════════════════════════════\n");

    return g_testsFailed > 0 ? 1 : 0;
}
