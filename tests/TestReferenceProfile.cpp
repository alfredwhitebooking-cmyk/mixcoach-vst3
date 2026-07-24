// ═══════════════════════════════════════════════════════════════════════════
//  TestReferenceProfile.cpp — Unit tests para ReferenceProfile completo
//
//  Cubre:
//    • Raw metrics builders (computeFromData con LUFS, truePeak, etc.)
//    • 5 high-level scores (subBalance, punchScore, densityScore, etc.)
//    • Serialización JSON (toJson → fromJson round-trip)
//    • Cache (save/load/has/remove/clearAll)
//    • Text helpers
//
//  Compilado via CMake:
//    cmake --build build --config Debug --target TestReferenceProfile
//    ./build/tests/Debug/TestReferenceProfile.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>

#include <juce_core/juce_core.h>

#include "MixCoach/engine/ReferenceProfile.h"
#include "MixCoach/engine/DifferenceProfile.h"

static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define TEST(name, expr) do {                                                  \
    if (!(expr)) {                                                             \
        std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: %s (%s:%d)\n",              \
                     name, __FILE__, __LINE__);                                \
        std::fflush(stderr);                                                   \
        gTestsFailed++;                                                        \
    } else {                                                                   \
        std::printf("  \xe2\x9c\x85 PASS: %s\n", name);                        \
        std::fflush(stdout);                                                   \
        gTestsPassed++;                                                        \
    }                                                                          \
} while(0)

#define TEST_NEAR(name, val, expected, tol) \
    TEST(name, std::abs((val) - (expected)) < (tol))

// ─── Helper: array de regiones para tests ─────────────────────────────────
static float balancedRegions[6]  = {-12.0f, -10.0f, -10.0f, -11.0f, -13.0f, -16.0f};
static float bassHeavyRegions[6] = {-6.0f, -4.0f, -14.0f, -16.0f, -18.0f, -20.0f};
static float brightRegions[6]   = {-20.0f, -18.0f, -14.0f, -10.0f, -6.0f, -4.0f};

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: Raw Metrics (computeFromData con nuevos campos)
// ═══════════════════════════════════════════════════════════════════════════
static void test_raw_metrics()
{
    std::printf("\n── Raw Metrics ──\n");
    std::fflush(stdout);

    // ─── computeFromData con todos los campos ─────────────────────────────
    {
        auto p = mixcoach::ReferenceProfile::computeFromData(
            10.0f,    // crestFactor
            8.0f,     // loudnessRange
            4500.0f,  // spectralCentroidHz
            balancedRegions,
            -10.0f,   // integratedLUFS
            -1.5f,    // truePeak
            0.65f,    // stereoWidth
            0.85f);   // correlation

        TEST_NEAR("raw integratedLUFS", p.integratedLUFS, -10.0f, 0.01f);
        TEST_NEAR("raw truePeakDBTP",   p.truePeakDBTP,   -1.5f, 0.01f);
        TEST_NEAR("raw stereoWidth",    p.stereoWidth,     0.65f, 0.01f);
        TEST_NEAR("raw correlation",    p.correlation,     0.85f, 0.01f);
        TEST_NEAR("raw crestFactor",    p.crestFactor,     10.0f, 0.01f);
        TEST_NEAR("raw loudnessRange",  p.loudnessRange,   8.0f, 0.01f);
        TEST_NEAR("raw centroid",       p.spectralCentroidHz, 4500.0f, 1.0f);
        TEST(    "raw regionEnergy[0]", std::abs(p.regionEnergy[0] - (-12.0f)) < 0.01f);
        TEST(    "raw regionEnergy[5]", std::abs(p.regionEnergy[5] - (-16.0f)) < 0.01f);
        TEST(    "raw valid",           p.valid);
    }

    // ─── computeFromData sin raw metrics (valores por defecto) ────────────
    {
        auto p = mixcoach::ReferenceProfile::computeFromData(
            12.0f, 6.0f, 5000.0f, balancedRegions);

        TEST_NEAR("default integratedLUFS = -100", p.integratedLUFS, -100.0f, 0.01f);
        TEST_NEAR("default truePeak = -100",       p.truePeakDBTP,   -100.0f, 0.01f);
        TEST_NEAR("default stereoWidth = 0",       p.stereoWidth,     0.0f, 0.01f);
        TEST_NEAR("default correlation = 0",       p.correlation,     0.0f, 0.01f);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: computeSubBalance
// ═══════════════════════════════════════════════════════════════════════════
static void test_subBalance()
{
    std::printf("\n── SubBalance ──\n");
    std::fflush(stdout);

    // Balanceado
    TEST_NEAR("subBalance balanced ~0.75",
        mixcoach::ReferenceProfile::computeSubBalance(balancedRegions), 0.75f, 0.05f);

    // Bass-heavy
    TEST_NEAR("subBalance bass-heavy ~1.0",
        mixcoach::ReferenceProfile::computeSubBalance(bassHeavyRegions), 1.0f, 0.05f);

    // Thin bass
    {
        float thin[6] = {-24.0f, -22.0f, -6.0f, -8.0f, -10.0f, -12.0f};
        TEST_NEAR("subBalance thin bass ~0.0",
            mixcoach::ReferenceProfile::computeSubBalance(thin), 0.0f, 0.05f);
    }

    // All silence → neutral
    {
        float silent[6] = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};
        TEST_NEAR("subBalance silence ~0.5",
            mixcoach::ReferenceProfile::computeSubBalance(silent), 0.5f, 0.01f);
    }

    // Single band → neutral
    {
        float single[6] = {-10.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};
        TEST_NEAR("subBalance single band ~0.5",
            mixcoach::ReferenceProfile::computeSubBalance(single), 0.5f, 0.01f);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: computePunchScore
// ═══════════════════════════════════════════════════════════════════════════
static void test_punchScore()
{
    std::printf("\n── PunchScore ──\n");
    std::fflush(stdout);

    TEST_NEAR("punch 10dB crest ~0.5",
        mixcoach::ReferenceProfile::computePunchScore(10.0f), 0.5f, 0.05f);
    TEST_NEAR("punch 4dB crest = 0.0",
        mixcoach::ReferenceProfile::computePunchScore(4.0f), 0.0f, 0.01f);
    TEST_NEAR("punch 16dB crest = 1.0",
        mixcoach::ReferenceProfile::computePunchScore(16.0f), 1.0f, 0.01f);
    TEST_NEAR("punch 8dB crest ~0.25",
        mixcoach::ReferenceProfile::computePunchScore(8.0f), 0.25f, 0.02f);
    TEST_NEAR("punch 12dB crest ~0.75",
        mixcoach::ReferenceProfile::computePunchScore(12.0f), 0.75f, 0.02f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: computeDensityScore
// ═══════════════════════════════════════════════════════════════════════════
static void test_densityScore()
{
    std::printf("\n── DensityScore ──\n");
    std::fflush(stdout);

    // Muy denso
    {
        float flat[6] = {-10.0f, -10.0f, -10.0f, -10.0f, -10.0f, -10.0f};
        TEST_NEAR("density very dense ~1.0",
            mixcoach::ReferenceProfile::computeDensityScore(6.0f, 4.0f, flat), 1.0f, 0.05f);
    }

    // Muy hueco
    {
        float hollow[6] = {-6.0f, -8.0f, -12.0f, -16.0f, -22.0f, -30.0f};
        TEST_NEAR("density very hollow ~0.0",
            mixcoach::ReferenceProfile::computeDensityScore(14.0f, 12.0f, hollow), 0.0f, 0.05f);
    }

    // Moderado
    {
        float mod[6] = {-8.0f, -10.0f, -12.0f, -14.0f, -16.0f, -18.0f};
        TEST_NEAR("density moderate ~0.55",
            mixcoach::ReferenceProfile::computeDensityScore(10.0f, 8.0f, mod), 0.55f, 0.05f);
    }

    // Silence
    {
        float silent[6] = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};
        TEST_NEAR("density silence ~0.5",
            mixcoach::ReferenceProfile::computeDensityScore(10.0f, 8.0f, silent), 0.5f, 0.05f);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: computeBrightnessScore
// ═══════════════════════════════════════════════════════════════════════════
static void test_brightnessScore()
{
    std::printf("\n── BrightnessScore ──\n");
    std::fflush(stdout);

    // Muy brillante
    TEST_NEAR("bright very bright ~1.0",
        mixcoach::ReferenceProfile::computeBrightnessScore(8000.0f, brightRegions), 1.0f, 0.05f);

    // Muy oscuro
    {
        float dark[6] = {-6.0f, -8.0f, -12.0f, -18.0f, -22.0f, -26.0f};
        TEST_NEAR("bright very dark ~0.0",
            mixcoach::ReferenceProfile::computeBrightnessScore(2000.0f, dark), 0.0f, 0.05f);
    }

    // Balanceado
    {
        float bal[6] = {-12.0f, -10.0f, -10.0f, -11.0f, -12.0f, -13.0f};
        TEST_NEAR("bright balanced ~0.44",
            mixcoach::ReferenceProfile::computeBrightnessScore(5000.0f, bal), 0.44f, 0.05f);
    }

    // Silence
    {
        float silent[6] = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};
        TEST_NEAR("bright silence ~0.5",
            mixcoach::ReferenceProfile::computeBrightnessScore(5000.0f, silent), 0.5f, 0.05f);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: computeDynamicScore
// ═══════════════════════════════════════════════════════════════════════════
static void test_dynamicScore()
{
    std::printf("\n── DynamicScore ──\n");
    std::fflush(stdout);

    TEST_NEAR("dynamic very dynamic ~1.0",
        mixcoach::ReferenceProfile::computeDynamicScore(14.0f, 12.0f), 1.0f, 0.05f);
    TEST_NEAR("dynamic very compressed ~0.0",
        mixcoach::ReferenceProfile::computeDynamicScore(6.0f, 4.0f), 0.0f, 0.05f);
    TEST_NEAR("dynamic moderate ~0.5",
        mixcoach::ReferenceProfile::computeDynamicScore(10.0f, 8.0f), 0.5f, 0.05f);
    TEST_NEAR("dynamic mixed ~0.5",
        mixcoach::ReferenceProfile::computeDynamicScore(12.0f, 6.0f), 0.5f, 0.05f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: computeFromData (integración de raw metrics + scores)
// ═══════════════════════════════════════════════════════════════════════════
static void test_computeFromData()
{
    std::printf("\n── computeFromData ──\n");
    std::fflush(stdout);

    // Balanced mix
    {
        auto p = mixcoach::ReferenceProfile::computeFromData(
            10.0f, 8.0f, 4500.0f, balancedRegions, -10.0f, -1.5f, 0.65f, 0.85f);
        TEST_NEAR("balanced subBalance",  p.subBalance,      0.75f, 0.05f);
        TEST_NEAR("balanced punchScore",  p.punchScore,      0.5f,  0.05f);
        TEST_NEAR("balanced dynamicScore", p.dynamicScore,    0.5f,  0.05f);
        TEST_NEAR("balanced LUFS",        p.integratedLUFS, -10.0f, 0.01f);
        TEST_NEAR("balanced width",       p.stereoWidth,     0.65f, 0.01f);
    }

    // Compressed mix
    {
        float compressed[6] = {-8.0f, -8.0f, -9.0f, -9.0f, -10.0f, -11.0f};
        auto p = mixcoach::ReferenceProfile::computeFromData(
            6.0f, 4.0f, 6000.0f, compressed, -8.0f, -0.5f, 0.30f, 0.95f);
        TEST_NEAR("compressed subBalance",   p.subBalance,    0.76f, 0.05f);
        TEST_NEAR("compressed punchScore",   p.punchScore,    0.0f,  0.01f);
        TEST_NEAR("compressed densityScore", p.densityScore,  1.0f,  0.10f);
        TEST_NEAR("compressed dynamicScore", p.dynamicScore,  0.0f,  0.01f);
        TEST_NEAR("compressed LUFS",         p.integratedLUFS, -8.0f, 0.01f);
        TEST_NEAR("compressed width",        p.stereoWidth,   0.30f, 0.01f);
    }

    // Dynamic mix
    {
        float dynamic[6] = {-16.0f, -14.0f, -10.0f, -12.0f, -18.0f, -22.0f};
        auto p = mixcoach::ReferenceProfile::computeFromData(
            16.0f, 14.0f, 3000.0f, dynamic, -14.0f, -2.5f, 0.80f, 0.60f);
        TEST_NEAR("dynamic subBalance",   p.subBalance,      0.69f, 0.05f);
        TEST_NEAR("dynamic punchScore",   p.punchScore,      1.0f,  0.01f);
        TEST_NEAR("dynamic dynamicScore", p.dynamicScore,    1.0f,  0.01f);
        TEST_NEAR("dynamic LUFS",         p.integratedLUFS, -14.0f, 0.01f);
        TEST_NEAR("dynamic width",        p.stereoWidth,     0.80f, 0.01f);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: Serialización (toJson → fromJson round-trip)
// ═══════════════════════════════════════════════════════════════════════════
static void test_serialization()
{
    std::printf("\n── Serialization ──\n");
    std::fflush(stdout);

    // ─── Crear perfil de prueba ──────────────────────────────────────────
    auto original = mixcoach::ReferenceProfile::computeFromData(
        12.0f, 6.0f, 5500.0f, balancedRegions,
        -12.5f, -2.0f, 0.72f, 0.80f);
    original.referenceName = "Test Reference.wav";
    original.filePath      = "/path/to/test.wav";
    original.genreGuess    = "Reggaeton";
    original.durationSeconds = 180.0;
    original.sampleRate    = 44100;
    original.timestampUs   = 1234567890;

    // ─── toJson → fromJson round-trip ────────────────────────────────────
    juce::DynamicObject obj;
    original.toJson(obj);
    auto loaded = mixcoach::ReferenceProfile::fromJson(obj);

    TEST("serialization valid", loaded.valid);
    TEST_NEAR("serialization subBalance",  loaded.subBalance,      original.subBalance,      0.001f);
    TEST_NEAR("serialization punchScore",  loaded.punchScore,      original.punchScore,      0.001f);
    TEST_NEAR("serialization densityScore", loaded.densityScore,   original.densityScore,    0.001f);
    TEST_NEAR("serialization brightness",  loaded.brightnessScore, original.brightnessScore,  0.001f);
    TEST_NEAR("serialization dynamic",     loaded.dynamicScore,    original.dynamicScore,     0.001f);

    TEST_NEAR("serialization LUFS",        loaded.integratedLUFS,  original.integratedLUFS,  0.001f);
    TEST_NEAR("serialization truePeak",    loaded.truePeakDBTP,    original.truePeakDBTP,    0.001f);
    TEST_NEAR("serialization width",       loaded.stereoWidth,     original.stereoWidth,     0.001f);
    TEST_NEAR("serialization correlation", loaded.correlation,     original.correlation,     0.001f);
    TEST_NEAR("serialization crest",       loaded.crestFactor,     original.crestFactor,     0.001f);
    TEST_NEAR("serialization LRA",         loaded.loudnessRange,   original.loudnessRange,   0.001f);
    TEST_NEAR("serialization centroid",    loaded.spectralCentroidHz, original.spectralCentroidHz, 0.001f);

    TEST("serialization referenceName", loaded.referenceName == original.referenceName);
    TEST("serialization filePath",      loaded.filePath      == original.filePath);
    TEST("serialization genreGuess",    loaded.genreGuess    == original.genreGuess);
    TEST_NEAR("serialization duration", loaded.durationSeconds, original.durationSeconds, 0.001);
    TEST(    "serialization sampleRate", loaded.sampleRate   == original.sampleRate);

    for (int i = 0; i < 6; ++i) {
        TEST_NEAR(juce::String("serialization region[") + juce::String(i) + "]",
                  loaded.regionEnergy[i], original.regionEnergy[i], 0.001f);
    }

    // ─── saveToFile → loadFromFile round-trip ────────────────────────────
    {
        juce::File tempFile = juce::File::createTempFile("ref_profile_test");
        bool saved = original.saveToFile(tempFile);
        TEST("saveToFile returned true", saved);

        auto fromFile = mixcoach::ReferenceProfile::loadFromFile(tempFile);
        TEST("loadFromFile valid", fromFile.valid);
        TEST_NEAR("file subBalance", fromFile.subBalance, original.subBalance, 0.001f);
        TEST_NEAR("file LUFS",       fromFile.integratedLUFS, original.integratedLUFS, 0.001f);
        TEST("file referenceName",   fromFile.referenceName == original.referenceName);

        tempFile.deleteFile();
    }

    // ─── loadFromFile con archivo inexistente → inválido ─────────────────
    {
        auto invalid = mixcoach::ReferenceProfile::loadFromFile(
            juce::File("/nonexistent/path/profile.json"));
        TEST("loadFromFile nonexistent returns invalid", !invalid.valid);
    }

    // ─── fromJson con objeto vacío → defaults pero válido ────────────────
    {
        juce::DynamicObject emptyObj;
        auto fromEmpty = mixcoach::ReferenceProfile::fromJson(emptyObj);
        // Sin propiedad "valid" → false por defecto (rd returns 0.0)
        TEST("fromJson empty valid=false", !fromEmpty.valid);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: ReferenceProfileCache
// ═══════════════════════════════════════════════════════════════════════════
static void test_cache()
{
    std::printf("\n── Cache ──\n");
    std::fflush(stdout);

    // Crear un directorio temporal para el cache
    juce::File tempDir = juce::File::createTempFile("ref_profile_cache_test");
    tempDir.deleteFile(); // Es un archivo temporal, lo queremos como directorio
    tempDir.createDirectory();

    mixcoach::ReferenceProfileCache cache(tempDir);

    // ─── makeKey: mismo path produce mismo hash ──────────────────────────
    {
        juce::String key1 = mixcoach::ReferenceProfileCache::makeKey("/path/to/song.wav");
        juce::String key2 = mixcoach::ReferenceProfileCache::makeKey("/path/to/song.wav");
        juce::String key3 = mixcoach::ReferenceProfileCache::makeKey("/different/song.wav");
        TEST("makeKey consistent", key1 == key2);
        TEST("makeKey different for different paths", key1 != key3);
        TEST("makeKey not empty", key1.isNotEmpty());
        TEST("makeKey hex (16 chars)", key1.length() == 16);
    }

    // ─── Guardar y cargar ────────────────────────────────────────────────
    {
        auto original = mixcoach::ReferenceProfile::computeFromData(
            10.0f, 8.0f, 4500.0f, balancedRegions,
            -10.0f, -1.5f, 0.65f, 0.85f);

        juce::String key = "test_key_123";
        TEST("cache.has before save", !cache.has(key));
        cache.save(key, original);
        TEST("cache.has after save", cache.has(key));

        auto loaded = cache.load(key);
        TEST("cache.load valid", loaded.valid);
        TEST_NEAR("cache subBalance",  loaded.subBalance,  original.subBalance,  0.001f);
        TEST_NEAR("cache LUFS",        loaded.integratedLUFS, original.integratedLUFS, 0.001f);
        TEST_NEAR("cache stereoWidth", loaded.stereoWidth, original.stereoWidth, 0.001f);
    }

    // ─── Cargar key inexistente → inválido ───────────────────────────────
    {
        auto loaded = cache.load("nonexistent_key");
        TEST("cache load nonexistent", !loaded.valid);
    }

    // ─── Eliminar ────────────────────────────────────────────────────────
    {
        juce::String key = "delete_test";
        auto p = mixcoach::ReferenceProfile::computeFromData(10.0f, 8.0f, 4500.0f, balancedRegions);
        cache.save(key, p);
        TEST("cache has before remove", cache.has(key));
        cache.remove(key);
        TEST("cache has after remove", !cache.has(key));
    }

    // ─── clearAll ────────────────────────────────────────────────────────
    {
        cache.save("key_a", mixcoach::ReferenceProfile::computeFromData(10.0f, 8.0f, 4500.0f, balancedRegions));
        cache.save("key_b", mixcoach::ReferenceProfile::computeFromData(12.0f, 6.0f, 5000.0f, balancedRegions));
        TEST("cache has before clear", cache.has("key_a") && cache.has("key_b"));
        cache.clearAll();
        TEST("cache has after clear", !cache.has("key_a") && !cache.has("key_b"));
    }

    // ─── Guardar perfil inválido no crea cache ───────────────────────────
    {
        mixcoach::ReferenceProfile invalid;
        cache.save("invalid_key", invalid);
        TEST("cache save invalid does not create", !cache.has("invalid_key"));
    }

    // Cleanup
    tempDir.deleteRecursively();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: Text Helpers
// ═══════════════════════════════════════════════════════════════════════════
static void test_text_helpers()
{
    std::printf("\n── Text Helpers ──\n");
    std::fflush(stdout);

    auto profile = mixcoach::ReferenceProfile::computeFromData(
        10.0f, 8.0f, 4500.0f, balancedRegions,
        -10.0f, -1.5f, 0.65f, 0.85f);
    profile.referenceName = "Test Ref.wav";

    juce::String shortText = profile.toShortText();
    TEST("toShortText not empty", shortText.isNotEmpty());
    TEST("toShortText contains SubBal", shortText.contains("SubBal"));
    TEST("toShortText contains Punch", shortText.contains("Punch"));
    TEST("toShortText contains Density", shortText.contains("Density"));
    TEST("toShortText contains Bright", shortText.contains("Bright"));
    TEST("toShortText contains Dynamic", shortText.contains("Dynamic"));

    juce::String verboseText = profile.toVerboseText();
    TEST("toVerboseText not empty", verboseText.isNotEmpty());
    TEST("toVerboseText contains REFERENCE PROFILE", verboseText.contains("REFERENCE PROFILE"));
    TEST("toVerboseText contains Raw Metrics", verboseText.contains("Raw Metrics"));
    TEST("toVerboseText contains High-Level Scores", verboseText.contains("High-Level Scores"));
    TEST("toVerboseText contains Integrated LUFS", verboseText.contains("Integrated LUFS"));
    TEST("toVerboseText contains Stereo Width", verboseText.contains("Stereo Width"));
    TEST("toVerboseText contains Sub Balance", verboseText.contains("Sub Balance"));
    TEST("toVerboseText contains Brightness", verboseText.contains("Brightness"));
    TEST("toVerboseText contains reference name", verboseText.contains("Test Ref.wav"));
    TEST("toVerboseText contains labels like Balanced",
         verboseText.contains("Balanced") || verboseText.contains("Moderate"));
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: Metadata defaults
// ═══════════════════════════════════════════════════════════════════════════
static void test_metadata_defaults()
{
    std::printf("\n── Metadata Defaults ──\n");
    std::fflush(stdout);

    mixcoach::ReferenceProfile p;
    TEST("default valid = false",       !p.valid);
    TEST("default timestamp = 0",       p.timestampUs == 0);
    TEST("default name empty",          p.referenceName.isEmpty());
    TEST("default path empty",          p.filePath.isEmpty());
    TEST("default genre empty",         p.genreGuess.isEmpty());
    TEST("default duration = 0",        p.durationSeconds == 0.0);
    TEST("default sampleRate = 0",      p.sampleRate == 0);
    TEST("default LUFS = -100",         p.integratedLUFS == -100.0f);
    TEST("default truePeak = -100",     p.truePeakDBTP == -100.0f);
    TEST("default crest = 0",           p.crestFactor == 0.0f);
    TEST("default width = 0",           p.stereoWidth == 0.0f);
    TEST("default correlation = 0",     p.correlation == 0.0f);
    TEST("default LRA = 0",             p.loudnessRange == 0.0f);
    TEST("default centroid = 0",        p.spectralCentroidHz == 0.0f);
    TEST("default subBalance = 0.5",    std::abs(p.subBalance - 0.5f) < 0.01f);
    TEST("default punchScore = 0.5",    std::abs(p.punchScore - 0.5f) < 0.01f);

    // regionEnergy defaults
    for (int i = 0; i < 6; ++i) {
        TEST(juce::String("default region[") + juce::String(i) + "] = -100",
             std::abs(p.regionEnergy[i] - (-100.0f)) < 0.01f);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════
int main()
{
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  ReferenceProfile Full Test Suite\n");
    std::printf("  Raw Metrics | 5 Scores | Serialization | Cache | Text\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n\n");
    std::fflush(stdout);

    test_raw_metrics();
    test_subBalance();
    test_punchScore();
    test_densityScore();
    test_brightnessScore();
    test_dynamicScore();
    test_computeFromData();
    test_serialization();
    test_cache();
    test_text_helpers();
    test_metadata_defaults();

    std::printf("\n\n\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::fflush(stdout);

    return gTestsFailed > 0 ? 1 : 0;
}
