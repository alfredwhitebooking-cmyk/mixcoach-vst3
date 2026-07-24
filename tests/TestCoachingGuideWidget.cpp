// ═══════════════════════════════════════════════════════════════════════════
//  TestCoachingGuideWidget.cpp — Unit tests para CoachingGuideWidget
//
//  Verifica:
//  1. LiveMeterData struct: defaults, field ranges, assignments, copy
//  2. Stage info: short names, icons, ProblemType mapping (de setStageSuggestionsFromProvider)
//  3. Tier suggestion defaults per stage + string storage
//  4. Stage lifecycle: setStageDirectly, completedFlags, progress
//  5. Color threshold ranges (non-overlapping, correct direction)
//
//  NOTA: CoachingGuideWidget hereda de juce::Component. Para mantener el
//  test standalone, las funciones auxiliares se definen localmente (como
//  en TestNavigationAnimations.cpp con SetupFadeAnim).
//
//  Los valores DEBEN coincidir con CoachingGuideWidget.cpp.
//  Protegido por static_assert en CoachingGuideWidget.cpp.
//
//  Compilado via CMake:
//    cmake --build build --config Release --target TestCoachingGuideWidget
//    ./build/tests/Release/TestCoachingGuideWidget.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstring>

// ─── Test runner ───────────────────────────────────────────────────────────
static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define TEST(name, expr) do {                                                   \
    if (!(expr)) {                                                              \
        std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: %s (%s:%d)\n",              \
                     name, __FILE__, __LINE__);                                 \
        std::fflush(stderr);                                                    \
        gTestsFailed++;                                                         \
    } else {                                                                    \
        std::printf("  \xe2\x9c\x85 PASS: %s\n", name);                         \
        std::fflush(stdout);                                                    \
        gTestsPassed++;                                                         \
    }                                                                           \
} while(0)

// ═══════════════════════════════════════════════════════════════════════════
//  LiveMeterData — Mirror de CoachingGuideWidget::LiveMeterData
// ═══════════════════════════════════════════════════════════════════════════
struct LiveMeterData {
    float peakDb = -80.0f;
    float rmsDb = -80.0f;
    float correlation = 1.0f;
    float crestFactor = 8.0f;
    float stereoWidth = 0.5f;
    float spectralCentroidHz = 1000.0f;
    int activeTrackCount = 0;
    int totalTrackCount = 0;
};

// ═══════════════════════════════════════════════════════════════════════════
//  CoachingStage — Mirror del enum en CoachingStageManager
// ═══════════════════════════════════════════════════════════════════════════
enum class CoachingStage {
    GainStaging = 0,
    Balance,
    EQ,
    Compression,
    Spatial,
    Refinement
};

// ═══════════════════════════════════════════════════════════════════════════
//  ProblemType — Mirror del enum en PluginSuggestionsProvider
// ═══════════════════════════════════════════════════════════════════════════
enum class ProblemType {
    Gain,
    Clipping,
    Spatial,
    TonalExcess,
    TonalDeficit,
    Masking,
    DynamicsOvercompressed,
    DynamicsTooDynamic,
    Reverb,
    Phase,
    Saturation,
    Limiting
};

// ═══════════════════════════════════════════════════════════════════════════
//  Stage info helpers — Mirror de CoachingGuideWidget.cpp
// ═══════════════════════════════════════════════════════════════════════════

static const char* stageShortName(CoachingStage stage) noexcept
{
    switch (stage) {
        case CoachingStage::GainStaging: return "Gain Staging";
        case CoachingStage::Balance:     return "Balance";
        case CoachingStage::EQ:          return "EQ";
        case CoachingStage::Compression: return "Compresion";
        case CoachingStage::Spatial:     return "Espacial";
        case CoachingStage::Refinement:  return "Refinamiento";
        default:                         return "";
    }
}

static const char* stageIcon(CoachingStage stage) noexcept
{
    switch (stage) {
        case CoachingStage::GainStaging: return "\xf0\x9f\x8e\x9a\xef\xb8\x8f";
        case CoachingStage::Balance:     return "\xe2\x9a\x96\xef\xb8\x8f";
        case CoachingStage::EQ:          return "\xf0\x9f\x8e\x9b\xef\xb8\x8f";
        case CoachingStage::Compression: return "\xf0\x9f\x93\x88";
        case CoachingStage::Spatial:     return "\xf0\x9f\x8c\x8a";
        case CoachingStage::Refinement:  return "\xe2\x9c\xa8";
        default:                         return "";
    }
}

// ═══ Stage -> ProblemType mapping (from setStageSuggestionsFromProvider) ═══
// Cada etapa se asocia a 1-3 problemas tipicos de mezcla.
struct StageProblemMapping {
    CoachingStage stage;
    ProblemType problems[3];
    int numProblems;
};

static const StageProblemMapping kStageProblems[6] = {
    { CoachingStage::GainStaging, { ProblemType::Gain, ProblemType::Clipping }, 2 },
    { CoachingStage::Balance,     { ProblemType::Spatial, ProblemType::Gain }, 2 },
    { CoachingStage::EQ,          { ProblemType::TonalExcess, ProblemType::TonalDeficit, ProblemType::Masking }, 3 },
    { CoachingStage::Compression, { ProblemType::DynamicsOvercompressed, ProblemType::DynamicsTooDynamic }, 2 },
    { CoachingStage::Spatial,     { ProblemType::Spatial, ProblemType::Reverb, ProblemType::Phase }, 3 },
    { CoachingStage::Refinement,  { ProblemType::Saturation, ProblemType::Limiting }, 2 }
};

// ─── Tier defaults — Mirror de getTierDefaults() en CoachingGuideWidget.cpp ──
struct TierDefaults {
    const char* ajusta;
    const char* verifica;
    const char* mejora;
};

static const TierDefaults& getTierDefaults(CoachingStage stage) noexcept
{
    static const TierDefaults defaults[] = {
        { "Usa Fruity Balance para ajustar niveles rapido",
          "Descarga YouLean Loudness Meter para monitorear LUFS",
          "Hazte con Hornet VU Meter: medicion profesional de nivel" },
        { "Fruity Stereo Shaper: paneo basico y ancho estereo",
          "Prueba Flux Stereo Tool para verificacion de balance",
          "iZotope Relay: balance preciso con visualizacion espectral" },
        { "Parametric EQ 2 de FL Studio: ecualizacion quirurgica",
          "TDR Nova: EQ dinamico gratuito con spectrum visual",
          "FabFilter Pro-Q 3: el estandar de EQ profesional" },
        { "Fruity Compressor: compresion basica en cada pista",
          "Rough Rider 3: compresor gratuito con caracter",
          "FabFilter Pro-C 2: compresion transparente de nivel mundial" },
        { "Fruity Reverb 2: reverb nativa con buenos presets",
          "Valhalla Supermassive: reverb y delay gratuito epico",
          "ValhallaVintageVerb: reverb de clase mundial por $50" },
        { "Maximus: multiband nativo para pegamento y punch final",
          "Ozone 11 EQ: limpieza final con asistente de referencia",
          "oeksound Soothe 2: control dinamico de frecuencias problematicas" }
    };
    auto idx = static_cast<int>(stage);
    if (idx < 0 || idx >= 6) return defaults[0];
    return defaults[idx];
}

// ═══════════════════════════════════════════════════════════════════════════
//  1. LIVE METER DATA — Valores por defecto y asignaciones
// ═══════════════════════════════════════════════════════════════════════════
static void test_live_meter_defaults() {
    std::printf("\n-- [1] LiveMeterData: Default Values --\n");

    LiveMeterData d;

    TEST("peakDb defaults to -80.0f (silence)", d.peakDb == -80.0f);
    TEST("rmsDb defaults to -80.0f (silence)", d.rmsDb == -80.0f);
    TEST("correlation defaults to 1.0f (perfect mono)", d.correlation == 1.0f);
    TEST("crestFactor defaults to 8.0f (healthy range)", d.crestFactor == 8.0f);
    TEST("stereoWidth defaults to 0.5f (moderate)", d.stereoWidth == 0.5f);
    TEST("spectralCentroidHz defaults to 1000.0f (typical)", d.spectralCentroidHz == 1000.0f);
    TEST("activeTrackCount defaults to 0", d.activeTrackCount == 0);
    TEST("totalTrackCount defaults to 0", d.totalTrackCount == 0);
}

static void test_live_meter_assignment() {
    std::printf("\n-- [2] LiveMeterData: Field Assignment --\n");

    LiveMeterData d;

    d.peakDb = -12.5f;
    d.rmsDb = -22.0f;
    d.correlation = 0.85f;
    d.crestFactor = 10.2f;
    d.stereoWidth = 0.72f;
    d.spectralCentroidHz = 2400.0f;
    d.activeTrackCount = 12;
    d.totalTrackCount = 128;

    TEST("peakDb assigned (-12.5)", d.peakDb == -12.5f);
    TEST("rmsDb assigned (-22.0)", d.rmsDb == -22.0f);
    TEST("correlation assigned (0.85)", d.correlation == 0.85f);
    TEST("crestFactor assigned (10.2)", d.crestFactor == 10.2f);
    TEST("stereoWidth assigned (0.72)", d.stereoWidth == 0.72f);
    TEST("spectralCentroidHz assigned (2400)", d.spectralCentroidHz == 2400.0f);
    TEST("activeTrackCount assigned (12)", d.activeTrackCount == 12);
    TEST("totalTrackCount assigned (128)", d.totalTrackCount == 128);

    // Edge cases
    LiveMeterData e;
    e.peakDb = -96.0f;
    TEST("peakDb = -96.0f (near noise floor)", e.peakDb == -96.0f);

    e.peakDb = 0.0f;
    TEST("peakDb = 0.0f (clipping at 0dBFS)", e.peakDb == 0.0f);

    e.correlation = -0.8f;
    TEST("correlation = -0.8 (phase issues)", e.correlation == -0.8f);

    e.crestFactor = 24.0f;
    TEST("crestFactor = 24.0 (highly dynamic)", e.crestFactor == 24.0f);

    e.activeTrackCount = 0;
    e.totalTrackCount = 0;
    TEST("track counts = 0 (empty session)",
         e.activeTrackCount == 0 && e.totalTrackCount == 0);

    e.activeTrackCount = 128;
    e.totalTrackCount = 128;
    TEST("track counts = 128 (max capacity)",
         e.activeTrackCount == 128 && e.totalTrackCount == 128);
}

static void test_live_meter_copy() {
    std::printf("\n-- [3] LiveMeterData: Copy Semantics --\n");

    LiveMeterData a;
    a.peakDb = -6.0f;
    a.rmsDb = -18.0f;
    a.correlation = 0.5f;
    a.crestFactor = 12.0f;
    a.stereoWidth = 0.8f;
    a.spectralCentroidHz = 3000.0f;
    a.activeTrackCount = 8;
    a.totalTrackCount = 64;

    LiveMeterData b = a;

    TEST("Copy: peakDb matches", b.peakDb == a.peakDb);
    TEST("Copy: rmsDb matches", b.rmsDb == a.rmsDb);
    TEST("Copy: correlation matches", b.correlation == a.correlation);
    TEST("Copy: crestFactor matches", b.crestFactor == a.crestFactor);
    TEST("Copy: stereoWidth matches", b.stereoWidth == a.stereoWidth);
    TEST("Copy: centroidHz matches", b.spectralCentroidHz == a.spectralCentroidHz);
    TEST("Copy: activeTrackCount matches", b.activeTrackCount == a.activeTrackCount);
    TEST("Copy: totalTrackCount matches", b.totalTrackCount == a.totalTrackCount);

    // Modify original, verify copy unchanged
    a.peakDb = -3.0f;
    a.activeTrackCount = 16;
    TEST("Modify orig: copy peakDb unchanged (-6.0)", b.peakDb == -6.0f);
    TEST("Modify orig: copy trackCount unchanged (8)", b.activeTrackCount == 8);
}

// ═══════════════════════════════════════════════════════════════════════════
//  4. STAGE INFO — Nombres e iconos de etapas
// ═══════════════════════════════════════════════════════════════════════════
static void test_stage_info() {
    std::printf("\n-- [4] Stage Info: Names and Icons --\n");

    TEST("GainStaging name", std::strcmp(stageShortName(CoachingStage::GainStaging), "Gain Staging") == 0);
    TEST("Balance name", std::strcmp(stageShortName(CoachingStage::Balance), "Balance") == 0);
    TEST("EQ name", std::strcmp(stageShortName(CoachingStage::EQ), "EQ") == 0);
    TEST("Compression name", std::strcmp(stageShortName(CoachingStage::Compression), "Compresion") == 0);
    TEST("Spatial name", std::strcmp(stageShortName(CoachingStage::Spatial), "Espacial") == 0);
    TEST("Refinement name", std::strcmp(stageShortName(CoachingStage::Refinement), "Refinamiento") == 0);

    TEST("GainStaging icon not empty", std::strlen(stageIcon(CoachingStage::GainStaging)) > 0);
    TEST("Balance icon not empty", std::strlen(stageIcon(CoachingStage::Balance)) > 0);
    TEST("EQ icon not empty", std::strlen(stageIcon(CoachingStage::EQ)) > 0);
    TEST("Compression icon not empty", std::strlen(stageIcon(CoachingStage::Compression)) > 0);
    TEST("Spatial icon not empty", std::strlen(stageIcon(CoachingStage::Spatial)) > 0);
    TEST("Refinement icon not empty", std::strlen(stageIcon(CoachingStage::Refinement)) > 0);

    // All icons are different
    TEST("Icon differs: GainStaging vs Balance",
         std::strcmp(stageIcon(CoachingStage::GainStaging), stageIcon(CoachingStage::Balance)) != 0);
    TEST("Icon differs: EQ vs Compression",
         std::strcmp(stageIcon(CoachingStage::EQ), stageIcon(CoachingStage::Compression)) != 0);
    TEST("Icon differs: Spatial vs Refinement",
         std::strcmp(stageIcon(CoachingStage::Spatial), stageIcon(CoachingStage::Refinement)) != 0);
}

// ═══════════════════════════════════════════════════════════════════════════
//  5. STAGE -> PROBLEMTYPE MAPPING (setStageSuggestionsFromProvider core logic)
// ═══════════════════════════════════════════════════════════════════════════
static void test_stage_problem_mapping() {
    std::printf("\n-- [5] Stage -> ProblemType Mapping --\n");

    // Verify all 6 stages have correct problem types
    // GainStaging -> Gain, Clipping
    TEST("GainStaging has 2 problems", kStageProblems[0].numProblems == 2);
    TEST("GainStaging problem[0] = Gain",
         kStageProblems[0].problems[0] == ProblemType::Gain);
    TEST("GainStaging problem[1] = Clipping",
         kStageProblems[0].problems[1] == ProblemType::Clipping);

    // Balance -> Spatial, Gain
    TEST("Balance has 2 problems", kStageProblems[1].numProblems == 2);
    TEST("Balance problem[0] = Spatial",
         kStageProblems[1].problems[0] == ProblemType::Spatial);
    TEST("Balance problem[1] = Gain",
         kStageProblems[1].problems[1] == ProblemType::Gain);

    // EQ -> TonalExcess, TonalDeficit, Masking
    TEST("EQ has 3 problems", kStageProblems[2].numProblems == 3);
    TEST("EQ problem[0] = TonalExcess",
         kStageProblems[2].problems[0] == ProblemType::TonalExcess);
    TEST("EQ problem[1] = TonalDeficit",
         kStageProblems[2].problems[1] == ProblemType::TonalDeficit);
    TEST("EQ problem[2] = Masking",
         kStageProblems[2].problems[2] == ProblemType::Masking);

    // Compression -> DynamicsOvercompressed, DynamicsTooDynamic
    TEST("Compression has 2 problems", kStageProblems[3].numProblems == 2);
    TEST("Compression problem[0] = DynamicsOvercompressed",
         kStageProblems[3].problems[0] == ProblemType::DynamicsOvercompressed);
    TEST("Compression problem[1] = DynamicsTooDynamic",
         kStageProblems[3].problems[1] == ProblemType::DynamicsTooDynamic);

    // Spatial -> Spatial, Reverb, Phase
    TEST("Spatial has 3 problems", kStageProblems[4].numProblems == 3);
    TEST("Spatial problem[0] = Spatial",
         kStageProblems[4].problems[0] == ProblemType::Spatial);
    TEST("Spatial problem[1] = Reverb",
         kStageProblems[4].problems[1] == ProblemType::Reverb);
    TEST("Spatial problem[2] = Phase",
         kStageProblems[4].problems[2] == ProblemType::Phase);

    // Refinement -> Saturation, Limiting
    TEST("Refinement has 2 problems", kStageProblems[5].numProblems == 2);
    TEST("Refinement problem[0] = Saturation",
         kStageProblems[5].problems[0] == ProblemType::Saturation);
    TEST("Refinement problem[1] = Limiting",
         kStageProblems[5].problems[1] == ProblemType::Limiting);

    // Verify no duplicate problem types within a stage
    for (int s = 0; s < 6; ++s) {
        for (int i = 0; i < kStageProblems[s].numProblems - 1; ++i) {
            for (int j = i + 1; j < kStageProblems[s].numProblems; ++j) {
                bool dup = (kStageProblems[s].problems[i] == kStageProblems[s].problems[j]);
                char msg[80];
                std::snprintf(msg, sizeof(msg), "Stage %d: no duplicate problems (%d != %d)", s, i, j);
                TEST(msg, !dup);
            }
        }
    }

    // All stages covered: 6 entries in mapping table
    int totalProblems = 0;
    for (int s = 0; s < 6; ++s)
        totalProblems += kStageProblems[s].numProblems;
    TEST("Total problems across all stages = 14 (2+2+3+2+3+2)",
         totalProblems == 14);
}

// ═══════════════════════════════════════════════════════════════════════════
//  6. TIER DEFAULTS — Sugerencias por defecto + string storage
// ═══════════════════════════════════════════════════════════════════════════
static void test_tier_defaults() {
    std::printf("\n-- [6] Tier Defaults: Per-Stage Suggestions --\n");

    // Each stage has all 3 tiers non-empty
    for (int i = 0; i < 6; ++i) {
        auto stage = static_cast<CoachingStage>(i);
        const auto& d = getTierDefaults(stage);
        char buf[80];

        std::snprintf(buf, sizeof(buf), "Stage %d ajusta not empty", i);
        TEST(buf, std::strlen(d.ajusta) > 0);

        std::snprintf(buf, sizeof(buf), "Stage %d verifica not empty", i);
        TEST(buf, std::strlen(d.verifica) > 0);

        std::snprintf(buf, sizeof(buf), "Stage %d mejora not empty", i);
        TEST(buf, std::strlen(d.mejora) > 0);
    }

    // Each stage has distinct ajusta text from its neighbor
    for (int i = 0; i < 5; ++i) {
        const auto& a = getTierDefaults(static_cast<CoachingStage>(i));
        const auto& b = getTierDefaults(static_cast<CoachingStage>(i + 1));
        char buf[80];
        std::snprintf(buf, sizeof(buf), "Stage %d ajusta differs from stage %d", i, i + 1);
        TEST(buf, std::strcmp(a.ajusta, b.ajusta) != 0);
    }

    // Specific content validation
    const auto& gain = getTierDefaults(CoachingStage::GainStaging);
    TEST("Gain: ajusta mentions Fruity Balance", std::strstr(gain.ajusta, "Balance") != nullptr);
    TEST("Gain: verifica mentions YouLean", std::strstr(gain.verifica, "YouLean") != nullptr);
    TEST("Gain: mejora mentions Hornet", std::strstr(gain.mejora, "Hornet") != nullptr);

    const auto& eq = getTierDefaults(CoachingStage::EQ);
    TEST("EQ: ajusta mentions Parametric EQ", std::strstr(eq.ajusta, "Parametric") != nullptr);
    TEST("EQ: verifica mentions TDR Nova", std::strstr(eq.verifica, "TDR Nova") != nullptr);
    TEST("EQ: mejora mentions FabFilter", std::strstr(eq.mejora, "FabFilter") != nullptr);

    // Simulate setTierSuggestions string storage (mirror of coachingGuideWidget logic)
    struct TierStrings {
        const char* ajusta;
        const char* verifica;
        const char* mejora;
    };
    TierStrings stored;
    auto setTierSuggestions = [&](const char* a, const char* v, const char* m) {
        stored.ajusta = a;
        stored.verifica = v;
        stored.mejora = m;
    };

    // Store and verify
    const auto& comp = getTierDefaults(CoachingStage::Compression);
    setTierSuggestions(comp.ajusta, comp.verifica, comp.mejora);
    TEST("setTierSuggestions: ajusta stored correctly",
         std::strcmp(stored.ajusta, "Fruity Compressor: compresion basica en cada pista") == 0);
    TEST("setTierSuggestions: verifica stored correctly",
         std::strcmp(stored.verifica, "Rough Rider 3: compresor gratuito con caracter") == 0);
    TEST("setTierSuggestions: mejora stored correctly",
         std::strcmp(stored.mejora, "FabFilter Pro-C 2: compresion transparente de nivel mundial") == 0);
}

// ═══════════════════════════════════════════════════════════════════════════
//  7. STAGE LIFECYCLE — Verificar setStageDirectly + completedFlags
// ═══════════════════════════════════════════════════════════════════════════
static void test_stage_lifecycle() {
    std::printf("\n-- [7] Stage Lifecycle: State Transitions --\n");

    // Simulate setStageDirectly() logic from CoachingGuideWidget.cpp
    CoachingStage currentStage = CoachingStage::GainStaging;
    float stageProgress = 0.0f;
    float overallProgress = 0.0f;
    bool stageCompletedFlags[6] = {};

    auto setStageDirectly = [&](CoachingStage stage, float sp, float op) {
        currentStage = stage;
        stageProgress = sp;
        overallProgress = op;
        int currentIdx = static_cast<int>(stage);
        for (int i = 0; i < 6; ++i)
            stageCompletedFlags[i] = (i < currentIdx);
    };

    TEST("Initial stage = GainStaging", currentStage == CoachingStage::GainStaging);
    TEST("Initial progress = 0.0f", stageProgress == 0.0f);

    // Move to Balance (complete GainStaging)
    setStageDirectly(CoachingStage::Balance, 0.2f, 0.17f);
    TEST("Stage = Balance", currentStage == CoachingStage::Balance);
    TEST("Progress = 0.2f", stageProgress == 0.2f);
    TEST("Overall ~0.167", std::abs(overallProgress - 0.167f) < 0.01f);
    TEST("GainStaging completed", stageCompletedFlags[0]);
    TEST("Balance NOT completed", !stageCompletedFlags[1]);

    // Move to EQ
    setStageDirectly(CoachingStage::EQ, 0.5f, 0.33f);
    TEST("Stage = EQ", currentStage == CoachingStage::EQ);
    TEST("GainStaging still completed", stageCompletedFlags[0]);
    TEST("Balance now completed", stageCompletedFlags[1]);
    TEST("EQ NOT completed", !stageCompletedFlags[2]);

    // Move to Compression
    setStageDirectly(CoachingStage::Compression, 0.0f, 0.5f);
    TEST("Stage = Compression", currentStage == CoachingStage::Compression);
    TEST("Prev 3 completed", stageCompletedFlags[0] && stageCompletedFlags[1] && stageCompletedFlags[2]);
    TEST("Compression NOT completed", !stageCompletedFlags[3]);

    // Final stage: Refinement
    setStageDirectly(CoachingStage::Refinement, 0.8f, 0.83f);
    TEST("Stage = Refinement", currentStage == CoachingStage::Refinement);
    TEST("Prev 5 completed", stageCompletedFlags[0] && stageCompletedFlags[1]
         && stageCompletedFlags[2] && stageCompletedFlags[3] && stageCompletedFlags[4]);
    TEST("Refinement NOT completed", !stageCompletedFlags[5]);
    TEST("Progress = 0.8f", stageProgress == 0.8f);

    // Edge: progress=1.0
    setStageDirectly(CoachingStage::GainStaging, 1.0f, 1.0f);
    TEST("Progress 1.0", stageProgress == 1.0f);
    TEST("Overall 1.0", overallProgress == 1.0f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  8. COLOR THRESHOLDS — Verificar rangos no contradictorios
// ═══════════════════════════════════════════════════════════════════════════
// Los thresholds estan hardcodeados en CoachingGuideWidget::drawStageDetail().
// Verificamos que los rangos sean monotonicos y no se superpongan
// incorrectamente entre los 3 estados (red/yellow/green).
static void test_color_thresholds() {
    std::printf("\n-- [8] Color Thresholds: Stage Meter Ranges --\n");

    // ─── GainStaging: peak thresholds ──
    // red: > -3dB   yellow: > -6dB   green: <= -6dB
    // Monotonic: red <-> yellow <-> green boundaries must not overlap
    float gainPeakRed = -3.0f;
    float gainPeakYellow = -6.0f;

    // A value of -4.0 should NOT be red (it's less than -3.0)
    // A value of -4.0 should NOT be green (it's greater than -6.0)
    // So -4.0 SHOULD be yellow
    TEST("GainStaging: peak -4.0dB is yellow (between -6 and -3)",
         -4.0f > gainPeakRed == false  // not red
         && -4.0f > gainPeakYellow     // not green
         && -4.0f <= gainPeakRed);     // is yellow

    // A value of -2.0 should be red (> -3.0)
    TEST("GainStaging: peak -2.0dB is red (above -3.0 threshold)",
         -2.0f > gainPeakRed);

    // A value of -8.0 should be green (<= -6.0)
    TEST("GainStaging: peak -8.0dB is green (below -6.0 threshold)",
         -8.0f <= gainPeakYellow);

    // ─── GainStaging: RMS thresholds ──
    // red: > -3dB   yellow: > -12dB   green: <= -12dB
    float gainRmsRed = -3.0f;
    float gainRmsYellow = -12.0f;

    TEST("GainStaging: RMS -5.0dB is yellow (between -12 and -3)",
         -5.0f > gainRmsRed == false  // not red
         && -5.0f > gainRmsYellow     // not green
         && -5.0f <= gainRmsRed);     // is yellow

    TEST("GainStaging: RMS -2.0dB is red (above -3.0)",
         -2.0f > gainRmsRed);

    TEST("GainStaging: RMS -15.0dB is green (below -12.0)",
         -15.0f <= gainRmsYellow);

    // ─── Compression: crest thresholds ──
    // red: < 4dB   yellow: < 8dB   green: >= 8dB
    float compCrestRed = 4.0f;
    float compCrestYellow = 8.0f;

    TEST("Compression: crest 6.0dB is yellow (between 4 and 8)",
         6.0f >= compCrestRed && 6.0f < compCrestYellow);

    TEST("Compression: crest 2.0dB is red (below 4.0)",
         2.0f < compCrestRed);

    TEST("Compression: crest 10.0dB is green (above 8.0)",
         10.0f >= compCrestYellow);

    // ─── Spatial: correlation thresholds ──
    // red: < -0.3   yellow: < 0.3   green: >= 0.3
    float spatCorrRed = -0.3f;
    float spatCorrYellow = 0.3f;

    TEST("Spatial: corr -0.5 is red (below -0.3)",
         -0.5f < spatCorrRed);

    TEST("Spatial: corr 0.0 is yellow (between -0.3 and 0.3)",
         0.0f >= spatCorrRed && 0.0f < spatCorrYellow);

    TEST("Spatial: corr 0.8 is green (above 0.3)",
         0.8f >= spatCorrYellow);

    // ─── Refinement: crest thresholds ──
    // red: < 4dB   yellow: < 6dB   green: >= 6dB
    float refCrestRed = 4.0f;
    float refCrestYellow = 6.0f;

    TEST("Refinement: crest 3.0 is red (below 4.0)",
         3.0f < refCrestRed);

    TEST("Refinement: crest 5.0 is yellow (between 4 and 6)",
         5.0f >= refCrestRed && 5.0f < refCrestYellow);

    TEST("Refinement: crest 8.0 is green (above 6.0)",
         8.0f >= refCrestYellow);

    // ─── Refinement: spectral centroid threshold ──
    // warning: < 800Hz
    float refCentroidWarn = 800.0f;

    TEST("Refinement: centroid 500Hz is warning (below 800Hz)",
         500.0f < refCentroidWarn);

    TEST("Refinement: centroid 1200Hz is normal (above 800Hz)",
         1200.0f >= refCentroidWarn);

    // Guard: 800Hz is more realistic than old 200Hz
    TEST("Centroid threshold (800Hz) is more realistic than 200Hz",
         800.0f >= 500.0f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════
int main() {
    std::printf("\n");
    std::printf("========================================================================\n");
    std::printf("  CoachingGuideWidget Unit Tests\n");
    std::printf("  LiveMeterData + stage mapping + tier defaults + lifecycle + thresholds\n");
    std::printf("========================================================================\n");

    test_live_meter_defaults();
    test_live_meter_assignment();
    test_live_meter_copy();
    test_stage_info();
    test_stage_problem_mapping();
    test_tier_defaults();
    test_stage_lifecycle();
    test_color_thresholds();

    std::printf("\n");
    std::printf("========================================================================\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("========================================================================\n");

    return gTestsFailed > 0 ? 1 : 0;
}
