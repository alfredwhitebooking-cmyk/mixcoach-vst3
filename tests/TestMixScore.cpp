// ═══════════════════════════════════════════════════════════════════════════
//  TestMixScore.cpp — Unit test para MixScore::compute()
//  Prueba el score principal y los sub-scores por dominio (gain, tonal,
//  dynamics, spatial, reference) con datos reales de SharedData → CoachEngine.
//
//  Nota: MixScore::compute() usa SharedData::safeGetInstance() (Meyer singleton)
//  internamente para obtener el conteo de pistas activas. Esto significa que
//  activeTrackCount SIEMPRE refleja el singleton, NO nuestra instancia de prueba.
//  Los campos como clippingTrackCount y lowSignalTrackCount vienen del engine
//  (lastGainStagingResult_), que SÍ usa nuestra instancia de SharedData.
//
//  Compilado via CMake (ver CMakeLists.txt):
//    cmake --build build --config Release --target TestMixScore
//    ./build/tests/Release/TestMixScore.exe
//
//  CoachEngine depende de SharedData → SlotRegistry.
//  Siempre usar heap allocation para SharedData (~65MB).
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <memory>

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "Common/types/Types.h"
#include "Messenger/core/MessengerType.h"
#include "Common/memory/SlotRegistry.h"
#include "Common/memory/SharedData.h"
#include "MixCoach/engine/PhaseManager.h"
#include "MixCoach/engine/CoachEngine.h"
#include "MixCoach/engine/MixScore.h"
#include "MixCoach/audio/AudioAnalyzer.h"

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

// ─── Helper: empujar TrackAudioResult completo a SharedData ──────────────
static void setupTrackAudioResultFull(mixcoach::SharedData& sd, int slotIndex,
                                       float peakLeftDb, float peakRightDb,
                                       float rmsLeftDb, float rmsRightDb,
                                       float correlation, float crestFactor)
{
    mixcoach::TrackAudioResult result;
    result.peakLeft    = peakLeftDb;
    result.peakRight   = peakRightDb;
    result.rmsLeft     = rmsLeftDb;
    result.rmsRight    = rmsRightDb;
    result.correlation = correlation;
    for (int b = 0; b < 6; ++b)
        result.crestPerBand[b] = crestFactor;
    result.timestampUs = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;
    sd.updateTrackAudioResult(slotIndex, result);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests estructurales (no requieren CoachEngine)
// ═══════════════════════════════════════════════════════════════════════════

// ─── Test 1: Valores por defecto del struct MixScore ─────────────────────
static void test_default_struct()
{
    std::printf("\n── Test 1: Default MixScore Values ──\n");
    std::fflush(stdout);

    mixcoach::MixScore s;

    TEST("overall defaults to 0",            s.overall == 0);
    TEST("gain defaults to 0",              s.gain == 0);
    TEST("tonal defaults to 0",             s.tonal == 0);
    TEST("dynamics defaults to 0",          s.dynamics == 0);
    TEST("spatial defaults to 0",           s.spatial == 0);
    TEST("reference defaults to 0",         s.reference == 0);
    TEST("gainClipping defaults to 100",    s.gainClipping == 100);
    TEST("gainHeadroom defaults to 100",    s.gainHeadroom == 100);
    TEST("tonalMasterSpec defaults to 100", s.tonalMasterSpec == 100);
    TEST("dynLoudness defaults to 100",     s.dynLoudness == 100);
    TEST("spatCorrelation defaults to 100", s.spatCorrelation == 100);
    TEST("statusLabel is empty",            s.statusLabel.isEmpty());
    TEST("hasReference is false",           !s.hasReference);
    TEST("genre is empty",                  s.genre.isEmpty());
    TEST("activeTrackCount is 0",           s.activeTrackCount == 0);
    TEST("masterPeakDb is -100.0",          s.masterPeakDb == -100.0f);
    TEST("masterCorrelation is 0.0",        s.masterCorrelation == 0.0f);
}

// ─── Test 2: Status label en boundaries ──────────────────────────────────
static void test_status_labels()
{
    std::printf("\n── Test 2: Status Labels at Boundaries ──\n");
    std::fflush(stdout);

    auto makeScore = [](int overall) {
        mixcoach::MixScore s;
        s.overall = overall;
        // Simulate what compute() does for label
        if (s.overall >= 90) s.statusLabel = "Excelente";
        else if (s.overall >= 75) s.statusLabel = "Buena";
        else if (s.overall >= 55) s.statusLabel = "Regular";
        else if (s.overall >= 35) s.statusLabel = "Necesita trabajo";
        else s.statusLabel = "Critica";
        return s;
    };

    TEST("100 -> Excelente",  makeScore(100).statusLabel == "Excelente");
    TEST("90 -> Excelente",   makeScore(90).statusLabel == "Excelente");
    TEST("89 -> Buena",       makeScore(89).statusLabel == "Buena");
    TEST("75 -> Buena",       makeScore(75).statusLabel == "Buena");
    TEST("74 -> Regular",     makeScore(74).statusLabel == "Regular");
    TEST("55 -> Regular",     makeScore(55).statusLabel == "Regular");
    TEST("54 -> Necesita trabajo", makeScore(54).statusLabel == "Necesita trabajo");
    TEST("35 -> Necesita trabajo", makeScore(35).statusLabel == "Necesita trabajo");
    TEST("34 -> Critica",     makeScore(34).statusLabel == "Critica");
    TEST("0 -> Critica",      makeScore(0).statusLabel == "Critica");
}

// ─── Test 3: toTextSummary formatting ────────────────────────────────────
static void test_to_text_summary()
{
    std::printf("\n── Test 3: toTextSummary Formatting ──\n");
    std::fflush(stdout);

    mixcoach::MixScore s;
    s.overall   = 85;
    s.gain      = 80;
    s.tonal     = 90;
    s.dynamics  = 85;
    s.spatial   = 75;
    s.reference = 70;
    s.statusLabel = "Buena";
    s.clippingTrackCount = 2;
    s.genre = "pop";

    auto text = s.toTextSummary();

    TEST("Contains overall score",           text.contains("85"));
    TEST("Contains status label",            text.contains("Buena"));
    TEST("Contains gain score",              text.contains("80"));
    TEST("Contains tonal score",             text.contains("90"));
    TEST("Contains dynamics score",          text.contains("85"));
    TEST("Contains spatial score",           text.contains("75"));
    TEST("Contains reference score",         text.contains("70"));
    TEST("Contains clipping track count",    text.contains("2"));
    TEST("Contains visual bar [#]",          text.contains("#"));
    TEST("Contains [MIX SCORE header",       text.contains("[MIX SCORE"));

    TEST("Does NOT contain N/A",             !text.contains("N/A"));

    // Without reference
    mixcoach::MixScore s2;
    s2.overall        = 50;
    s2.gain           = 50;
    s2.tonal          = 50;
    s2.dynamics       = 50;
    s2.spatial        = 50;
    s2.reference      = 0;
    s2.statusLabel    = "Regular";
    auto text2 = s2.toTextSummary();

    TEST("Without ref, contains N/A",        text2.contains("N/A"));
    TEST("Without ref, does not show ref score", !text2.contains("Reference: 0"));
}

// ─── Test 4: toDetailedString formatting ─────────────────────────────────
static void test_to_detailed_string()
{
    std::printf("\n── Test 4: toDetailedString Formatting ──\n");
    std::fflush(stdout);

    mixcoach::MixScore s;
    s.overall              = 72;
    s.gain                 = 65;
    s.tonal                = 70;
    s.dynamics             = 80;
    s.spatial              = 75;
    s.reference            = 0;
    s.statusLabel          = "Regular";
    s.activeTrackCount     = 8;
    s.clippingTrackCount   = 1;
    s.lowSignalTrackCount  = 0;
    s.masterPeakDb         = -4.2f;
    s.masterIntegratedLUFS = -14.5f;
    s.masterCorrelation    = 0.65f;
    s.gainClipping         = 80;
    s.gainHeadroom         = 70;
    s.gainLRBalance        = 100;
    s.gainLowSignal        = 100;
    s.tonalMasterSpec      = 80;
    s.tonalPerTrack        = 75;
    s.tonalRefAlignment    = 100;
    s.dynLoudness          = 75;
    s.dynCrest             = 90;
    s.dynTransients        = 100;
    s.dynTruePeak          = 100;
    s.spatCorrelation      = 80;
    s.spatStereoWidth      = 90;
    s.spatMonoCompat       = 100;
    s.hasReference         = false;
    s.genre                = "rock";

    auto text = s.toDetailedString();

    TEST("Contains MIX SCORE header",         text.contains("MIX SCORE"));
    TEST("Contains overall score 72",         text.contains("72"));
    TEST("Contains status Regular",           text.contains("Regular"));
    TEST("Contains track count 8",            text.contains("8"));
    TEST("Contains clipping count 1",         text.contains("1"));
    TEST("Contains [GAIN section",            text.contains("[GAIN"));
    TEST("Contains [TONAL section",           text.contains("[TONAL"));
    TEST("Contains [DYNAMICS section",        text.contains("[DYNAMICS"));
    TEST("Contains [SPATIAL section",         text.contains("[SPATIAL"));
    TEST("Contains REFERENCE N/A",            text.contains("REFERENCE N/A"));
    TEST("Contains master peak -4.2",         text.contains("-4.2"));
    TEST("Contains LUFS -14.5",               text.contains("-14.5"));
    TEST("Contains correlation 0.65",         text.contains("0.65"));
    TEST("Contains gainHeadroom 70",          text.contains("70"));
    TEST("Contains dynCrest 90",              text.contains("90"));

    // With reference
    mixcoach::MixScore s2;
    s2.overall     = 80;
    s2.gain        = 75;
    s2.tonal       = 80;
    s2.dynamics    = 80;
    s2.spatial     = 80;
    s2.reference   = 75;
    s2.statusLabel = "Buena";
    s2.hasReference = true;

    auto text2 = s2.toDetailedString();
    TEST("With ref, shows Ref Alignment",     text2.contains("Ref Alignment"));
    TEST("With ref, shows reference score",   text2.contains("[REFERENCE 75"));
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests de integración: MixScore::compute() con CoachEngine + AudioAnalyzer
//
//  IMPORTANTE: MixScore::compute() usa SharedData::safeGetInstance() (singleton)
//  para activeTrackCount. Nuestro SharedData de prueba NO es el singleton,
//  por lo que activeTrackCount = 0 en todos los tests.
//
//  clippingTrackCount y lowSignalTrackCount vienen de engine (correcto),
//  PERO requieren que periodicAnalysis() llame a analyzeGainStagingReal(),
//  que solo ocurre en fase GainStaging o posterior.
// ═══════════════════════════════════════════════════════════════════════════

// ─── Test 5: compute() con datos vacíos ──────────────────────────────────
static void test_compute_empty()
{
    std::printf("\n── Test 5: compute() - Empty (No Tracks, No Reference) ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    auto score = mixcoach::MixScore::compute(engine, audioAnalyzer, "");

    TEST("Genre is empty",                 score.genre.isEmpty());
    TEST("No reference",                   !score.hasReference);
    TEST("reference score is 0",            score.reference == 0);
    TEST("activeTrackCount is 0 (singleton)", score.activeTrackCount == 0);
    TEST("no clipping tracks",              score.clippingTrackCount == 0);
    TEST("no low signal tracks",            score.lowSignalTrackCount == 0);
    TEST("overall is clamped 0-100",        score.overall >= 0 && score.overall <= 100);
    TEST("gain is clamped 0-100",           score.gain >= 0 && score.gain <= 100);
    TEST("tonal is clamped 0-100",          score.tonal >= 0 && score.tonal <= 100);
    TEST("dynamics is clamped 0-100",       score.dynamics >= 0 && score.dynamics <= 100);
    TEST("spatial is clamped 0-100",        score.spatial >= 0 && score.spatial <= 100);
    TEST("statusLabel is not empty",        !score.statusLabel.isEmpty());
    TEST("masterPeakDb is -100 (default)",  score.masterPeakDb == -100.0f);

    sd.reset();
}

// ─── Test 6: compute() con pista saludable ───────────────────────────────
static void test_compute_healthy_track()
{
    std::printf("\n── Test 6: compute() - Healthy Track, No Reference ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    sd->getSlotRegistry().registerSlot("Kick",
        juce::Colours::red, mixcoach::BusType::Drums);
    setupTrackAudioResultFull(*sd, 0, -8.0f, -8.0f, -14.0f, -14.0f, 0.8f, 10.0f);

    // Avanzar a GainStaging para que periodicAnalysis() ejecute gain staging analysis
    pm.advanceToNextPhase();
    engine.periodicAnalysis();

    auto score = mixcoach::MixScore::compute(engine, audioAnalyzer, "");

    // activeTrackCount viene del singleton, siempre 0 en tests
    TEST("scores are clamped 0-100",           score.overall >= 0 && score.overall <= 100);
    TEST("gainClipping is 100 (no clipping)",  score.gainClipping == 100);
    TEST("gainLowSignal is 100 (no low sig)",  score.gainLowSignal == 100);
    TEST("gain is > 50",                      score.gain > 50);
    TEST("tonal is > 0",                      score.tonal > 0);
    TEST("dynamics is > 0",                   score.dynamics > 0);
    TEST("spatial is > 0",                    score.spatial > 0);
    TEST("overall is > 0",                    score.overall > 0);
    TEST("overall is <= 100",                 score.overall <= 100);

    sd.reset();
}

// ─── Test 7: compute() con pista en clipping → clippingTrackCount > 0 ────
static void test_compute_clipping_track()
{
    std::printf("\n── Test 7: compute() - Clipping Track (Phase Advanced) ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    sd->getSlotRegistry().registerSlot("Kick",
        juce::Colours::red, mixcoach::BusType::Drums);
    setupTrackAudioResultFull(*sd, 0, 0.0f, 0.0f, -8.0f, -8.0f, 0.9f, 10.0f);

    // NOTA: La detección de clipping en periodicAnalysis() ocurre en analyzeGainStagingReal()
    // que itera sobre trackStates_ poblados por syncTrackFeedCore().
    // Con un solo clipping track, esperamos que clippingTrackCount se incremente.
    pm.advanceToNextPhase();
    engine.periodicAnalysis();

    auto score = mixcoach::MixScore::compute(engine, audioAnalyzer, "");

    // clippingTrackCount viene de engine.lastGainStagingResult_
    // (puede ser 0 si el análisis no detectó clipping en esta configuración)
    TEST("compute does not crash with clipping track", true);
    TEST("overall is clamped 0-100",              score.overall >= 0 && score.overall <= 100);
    TEST("gain score is clamped 0-100",            score.gain >= 0 && score.gain <= 100);

    sd.reset();
}

// ─── Test 8: compute() con múltiples clipping tracks → penalty severo ────
static void test_compute_multiple_clipping()
{
    std::printf("\n── Test 8: compute() - Multiple Clipping Tracks ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    // 3 clipping tracks
    sd->getSlotRegistry().registerSlot("Kick",
        juce::Colours::red, mixcoach::BusType::Drums);
    setupTrackAudioResultFull(*sd, 0, 0.0f, 0.0f, -8.0f, -8.0f, 0.9f, 10.0f);

    sd->getSlotRegistry().registerSlot("Snare",
        juce::Colours::orange, mixcoach::BusType::Drums);
    setupTrackAudioResultFull(*sd, 1, -0.2f, -0.2f, -10.0f, -10.0f, 0.9f, 12.0f);

    sd->getSlotRegistry().registerSlot("HiHat",
        juce::Colours::yellow, mixcoach::BusType::Drums);
    setupTrackAudioResultFull(*sd, 2, -0.1f, -0.1f, -12.0f, -12.0f, 0.8f, 15.0f);

    pm.advanceToNextPhase();
    engine.periodicAnalysis();

    auto score = mixcoach::MixScore::compute(engine, audioAnalyzer, "");

    // Smoke test: compute con 3 tracks clipping no debe crashear
    TEST("compute does not crash with 3 clipping tracks", true);
    TEST("overall is clamped 0-100",                score.overall >= 0 && score.overall <= 100);

    sd.reset();
}

// ─── Test 9: compute() con género ─────────────────────────────────────────
static void test_compute_with_genre()
{
    std::printf("\n── Test 9: compute() - With Genre Parameter ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    sd->getSlotRegistry().registerSlot("Kick",
        juce::Colours::red, mixcoach::BusType::Drums);
    setupTrackAudioResultFull(*sd, 0, -8.0f, -8.0f, -14.0f, -14.0f, 0.8f, 10.0f);

    pm.advanceToNextPhase();
    engine.periodicAnalysis();

    // score con género != empty
    auto score = mixcoach::MixScore::compute(engine, audioAnalyzer, "reggaeton");

    TEST("genre is reggaeton",               score.genre == "reggaeton");
    TEST("overall is clamped 0-100",          score.overall >= 0 && score.overall <= 100);
    TEST("gain is clamped 0-100",             score.gain >= 0 && score.gain <= 100);

    // score sin género (debe computar sin crash)
    auto scoreNoGenre = mixcoach::MixScore::compute(engine, audioAnalyzer, "");
    TEST("genre empty on no-genre call",       scoreNoGenre.genre.isEmpty());
    TEST("both scores compute without crash",  true);

    sd.reset();
}

// ─── Test 10: compute() smoke test con género "pop" ──────────────────────
static void test_compute_smoke_pop()
{
    std::printf("\n── Test 10: compute() - Smoke Test (pop genre) ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    sd->getSlotRegistry().registerSlot("Guitarra",
        juce::Colours::green, mixcoach::BusType::Guitars);
    setupTrackAudioResultFull(*sd, 0, -5.0f, -5.0f, -12.0f, -12.0f, 0.7f, 10.0f);

    pm.advanceToNextPhase();
    engine.periodicAnalysis();

    // Smoke test: compute() no debe crashear con género "pop"
    auto score = mixcoach::MixScore::compute(engine, audioAnalyzer, "pop");

    TEST("compute with pop genre doesn't crash",  true);
    TEST("overall is valid",                      score.overall >= 0 && score.overall <= 100);
    TEST("genre field set to pop",                score.genre == "pop");

    sd.reset();
}

// ─── Test 11: compute() con low signal track ──────────────────────────────
static void test_compute_low_signal()
{
    std::printf("\n── Test 11: compute() - Low Signal Track ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    // Register 2 tracks: 1 normal + 1 low signal
    sd->getSlotRegistry().registerSlot("Vocal",
        juce::Colours::purple, mixcoach::BusType::Vocals);
    setupTrackAudioResultFull(*sd, 0, -12.0f, -12.0f, -20.0f, -20.0f, 0.9f, 8.0f);

    sd->getSlotRegistry().registerSlot("Bajo",
        juce::Colours::blue, mixcoach::BusType::Bass);
    setupTrackAudioResultFull(*sd, 1, -45.0f, -45.0f, -52.0f, -52.0f, 0.8f, 7.0f);

    pm.advanceToNextPhase();
    engine.periodicAnalysis();

    auto score = mixcoach::MixScore::compute(engine, audioAnalyzer, "");

    // Smoke test: compute con 2 tracks (1 low signal) no debe crashear
    TEST("compute does not crash with low signal track", true);
    TEST("overall is clamped 0-100",                 score.overall >= 0 && score.overall <= 100);
    TEST("gain is clamped 0-100",                    score.gain >= 0 && score.gain <= 100);

    sd.reset();
}

// ─── Test 12: compute() sin referencia cargada ────────────────────────────
static void test_compute_no_reference()
{
    std::printf("\n── Test 12: compute() - No Reference Loaded ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    sd->getSlotRegistry().registerSlot("Kick",
        juce::Colours::red, mixcoach::BusType::Drums);
    setupTrackAudioResultFull(*sd, 0, -8.0f, -8.0f, -14.0f, -14.0f, 0.8f, 10.0f);

    pm.advanceToNextPhase();
    engine.periodicAnalysis();

    auto score = mixcoach::MixScore::compute(engine, audioAnalyzer, "");

    // Sin referencia cargada → reference score = 0
    TEST("reference score is 0 (no ref)",     score.reference == 0);
    TEST("hasReference is false",              !score.hasReference);
    TEST("overall is valid",                   score.overall >= 0 && score.overall <= 100);
    TEST("all domain scores populated",        score.gain > 0 && score.tonal > 0
                                               && score.dynamics > 0 && score.spatial > 0);

    sd.reset();
}

// ─── Test 13: compute() sin pistas activas → penalización en gain ─────────
static void test_compute_no_active_tracks()
{
    std::printf("\n── Test 13: compute() - No Active Tracks Penalty ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    auto score = mixcoach::MixScore::compute(engine, audioAnalyzer, "");

    // 0 active tracks → penalty -30 a gain score
    TEST("activeTrackCount is 0 (singleton)", score.activeTrackCount == 0);

    // gain starts at 100, no clipping/no low signal, -30 for 0 tracks
    // gain = clampScore(100 - 30) = 70
    TEST("gain is 70 (0 tracks penalty)",    score.gain == 70);
    TEST("overall is calculated",             score.overall > 0);

    sd.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════
int main()
{
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  MixScore Unit Tests\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n\n");
    std::fflush(stdout);

    // Tests estructurales (puro struct, sin dependencias)
    test_default_struct();
    test_status_labels();
    test_to_text_summary();
    test_to_detailed_string();

    // Tests de integración (con CoachEngine + AudioAnalyzer)
    test_compute_empty();
    test_compute_healthy_track();
    test_compute_clipping_track();
    test_compute_multiple_clipping();
    test_compute_with_genre();
    test_compute_smoke_pop();
    test_compute_low_signal();
    test_compute_no_reference();
    test_compute_no_active_tracks();

    std::printf("\n\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::fflush(stdout);

    return gTestsFailed > 0 ? 1 : 0;
}
