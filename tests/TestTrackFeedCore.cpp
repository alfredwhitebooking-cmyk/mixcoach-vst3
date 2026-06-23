// ═══════════════════════════════════════════════════════════════════════════
//  TestTrackFeedCore.cpp — Unit test para TrackFeedCore
//  Testea: TrackState conversion, event generation, priority scoring,
//          cooldowns, health detection, edge cases
//
//  Compilado via CMake (ver CMakeLists.txt):
//    cmake --build build --config Release --target TestTrackFeedCore
//    ./build/tests/Release/TestTrackFeedCore.exe
//
//  TrackFeedCore es autonomo (no necesita SharedData/SlotRegistry en .cpp):
//    TrackAudioResult, SlotInfo son structs definidos en headers
//    kMaxSlots es static constexpr (inline C++17)
//    LogHelper::writeToLog es inline
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <memory>
#include <vector>
#include <algorithm>

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

#include "Common/types/Types.h"
#include "Common/memory/SharedData.h"
#include "Common/memory/SlotRegistry.h"
#include "MixCoach/engine/TrackFeedCore.h"
#include "MixCoach/engine/TrackState.h"
#include "MixCoach/engine/TrackRole.h"

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

// ─── Helpers ───────────────────────────────────────────────────────────────
// Timestamp base = uptime del sistema en microsegundos.
// Esto garantiza que los eventos NO sean filtrados por getRecentEvents()
// que usa (juce::Time::getMillisecondCounter() * 1000 - 60s) como cutoff.
static int64_t kTestTimestampUs = 0;

static mixcoach::TrackAudioResult makeResult(float peakDb, float rmsDb,
                                              float correlation = 0.0f,
                                              int64_t timestampUs = kTestTimestampUs,
                                              float transientRatio = 0.0f)
{
    mixcoach::TrackAudioResult r;
    r.peakLeft     = peakDb;
    r.peakRight    = peakDb;
    r.rmsLeft      = rmsDb;
    r.rmsRight     = rmsDb;
    r.correlation  = correlation;
    r.timestampUs  = timestampUs;
    r.transientRatio = transientRatio;
    return r;
}

static mixcoach::SlotInfo makeSlotInfo(int slotIndex, const char* name,
                                        mixcoach::BusType bus = mixcoach::BusType::None,
                                        bool active = true)
{
    mixcoach::SlotInfo info;
    info.slotIndex = slotIndex;
    info.setTrackName(name);
    info.active = active;
    info.bus = bus;
    info.colour = juce::Colours::grey;
    return info;
}

// Helper: contar eventos de un tipo especifico en el historial
static int countEventType(const std::vector<mixcoach::TrackEvent>& events,
                           mixcoach::TrackEventType type)
{
    return (int)std::count_if(events.begin(), events.end(),
        [type](const mixcoach::TrackEvent& e) { return e.type == type; });
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: TrackState conversion
// ═══════════════════════════════════════════════════════════════════════════

static void test_trackstate_conversion()
{
    std::printf("\n── TrackState: Conversion ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;

    auto result = makeResult(-12.0f, -22.0f, 0.85f, kTestTimestampUs);
    auto info   = makeSlotInfo(0, "Kick", mixcoach::BusType::Drums);

    feed.updateTrackState(0, result, info, mixcoach::TrackRole::Kick);
    auto state = feed.getTrackState(0);

    TEST("slotIndex = 0",            state.slotIndex == 0);
    TEST("trackName = Kick",         state.trackName == "Kick");
    TEST("bus = Drums",              state.bus == mixcoach::BusType::Drums);
    TEST("role = Kick",              state.role == mixcoach::TrackRole::Kick);
    TEST("active = true",            state.active == true);
    TEST("peakCombined = -12.0",     std::abs(state.peakCombined - (-12.0f)) < 0.01f);
    TEST("rmsCombined = -22.0",      std::abs(state.rmsCombined - (-22.0f)) < 0.01f);
    TEST("correlation = 0.85",       std::abs(state.correlation - 0.85f) < 0.01f);
    TEST("timestampUs = kTestTimestamp", state.timestampUs == kTestTimestampUs);
    TEST("peakLeft = -12.0",         std::abs(state.peakLeft - (-12.0f)) < 0.01f);
    TEST("peakRight = -12.0",        std::abs(state.peakRight - (-12.0f)) < 0.01f);
    TEST("rmsLeft = -22.0",          std::abs(state.rmsLeft - (-22.0f)) < 0.01f);
    TEST("rmsRight = -22.0",         std::abs(state.rmsRight - (-22.0f)) < 0.01f);
    TEST("crestFactor = 10.0",       std::abs(state.crestFactor - 10.0f) < 0.01f);
    TEST("hasSignal = true",         state.hasSignal() == true);
    TEST("isClipping = false",       state.isClipping() == false);
    TEST("isStale = false (3s timeout)", state.isStale(kTestTimestampUs + 1000000, 3000000) == false);
}

static void test_trackstate_invalid_index()
{
    std::printf("\n── TrackState: Invalid Index ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;
    auto state = feed.getTrackState(-1);
    TEST("Invalid index returns default state", state.slotIndex == -1);
    TEST("Invalid index: active = false",       state.active == false);
    TEST("Invalid index: health = Unknown",     state.health == mixcoach::TrackHealth::Unknown);

    state = feed.getTrackState(999);
    TEST("Out-of-range index returns default",   state.slotIndex == -1);
}

static void test_trackstate_crest_factor()
{
    std::printf("\n── TrackState: Crest Factor Edge Cases ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;

    // RMS muy bajo (silence) -> crestFactor = 0 (previene division por ~0)
    auto result = makeResult(-30.0f, -90.0f);
    auto info   = makeSlotInfo(0, "Silencio");
    feed.updateTrackState(0, result, info, mixcoach::TrackRole::Unknown);
    auto state = feed.getTrackState(0);
    TEST("Crest = 0 when RMS < -80", std::abs(state.crestFactor) < 0.01f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: Health detection
// ═══════════════════════════════════════════════════════════════════════════

static void test_health_clean()
{
    std::printf("\n── Health: Clean Signal ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;
    auto result = makeResult(-12.0f, -22.0f);
    auto info   = makeSlotInfo(0, "Vocal", mixcoach::BusType::Vocals);
    feed.updateTrackState(0, result, info, mixcoach::TrackRole::VozPrincipal);
    auto state = feed.getTrackState(0);
    TEST("Clean signal -> health = Clean", state.health == mixcoach::TrackHealth::Clean);
}

static void test_health_clipping()
{
    std::printf("\n── Health: Clipping ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;
    auto result = makeResult(-0.1f, -10.0f);
    auto info   = makeSlotInfo(0, "Kick");
    feed.updateTrackState(0, result, info, mixcoach::TrackRole::Unknown);
    auto state = feed.getTrackState(0);
    TEST("Clipping -> health = ClippingRisk",    state.health == mixcoach::TrackHealth::ClippingRisk);
    TEST("isClipping = true",                   state.isClipping() == true);
}

static void test_health_overcompressed()
{
    std::printf("\n── Health: Overcompressed ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;
    // crest = -6 - (-8) = 2, que es < 4 (kCrestOvercompressed)
    auto result = makeResult(-6.0f, -8.0f);
    auto info   = makeSlotInfo(0, "Bass");
    feed.updateTrackState(0, result, info, mixcoach::TrackRole::Unknown);
    auto state = feed.getTrackState(0);
    TEST("Crest low -> health = Overcompressed", state.health == mixcoach::TrackHealth::Overcompressed);
}

static void test_health_stereo_collapse()
{
    std::printf("\n── Health: Stereo Collapse ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;
    auto result = makeResult(-10.0f, -20.0f, -0.5f);
    auto info   = makeSlotInfo(0, "Pad");
    feed.updateTrackState(0, result, info, mixcoach::TrackRole::Unknown);
    auto state = feed.getTrackState(0);
    TEST("Corr < 0 -> health = StereoCollapse", state.health == mixcoach::TrackHealth::StereoCollapse);
}

static void test_health_needs_compression()
{
    std::printf("\n── Health: Needs Compression ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;
    // crest = -2 - (-30) = 28, que es > 24 (kCrestTooDynamic)
    auto result = makeResult(-2.0f, -30.0f);
    auto info   = makeSlotInfo(0, "Snare");
    feed.updateTrackState(0, result, info, mixcoach::TrackRole::Unknown);
    auto state = feed.getTrackState(0);
    TEST("High crest -> health = NeedsCompression", state.health == mixcoach::TrackHealth::NeedsCompression);
}

static void test_health_low_signal()
{
    std::printf("\n── Health: Low Signal ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;
    auto result = makeResult(-40.0f, -50.0f);
    auto info   = makeSlotInfo(0, "Ambience");
    feed.updateTrackState(0, result, info, mixcoach::TrackRole::Unknown);
    auto state = feed.getTrackState(0);
    TEST("Low peak -> health = LowSignal", state.health == mixcoach::TrackHealth::LowSignal);
}

static void test_health_silent()
{
    std::printf("\n── Health: Silent ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;
    auto result = makeResult(-100.0f, -100.0f, 0.0f, 0); // timestamp = 0 -> no signal
    auto info   = makeSlotInfo(0, "Silent");
    feed.updateTrackState(0, result, info, mixcoach::TrackRole::Unknown);
    auto state = feed.getTrackState(0);
    TEST("No signal -> health = Silent",       state.health == mixcoach::TrackHealth::Silent);
    TEST("hasSignal = false when peak < -60",  state.hasSignal() == false);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: Event generation
// ═══════════════════════════════════════════════════════════════════════════

static void test_event_track_appeared()
{
    std::printf("\n── Events: Track Appeared ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;

    // Primera llamada: pasa de inactive -> active -> debe generar TrackAppeared
    feed.updateTrackState(0, makeResult(-10.0f, -20.0f),
                           makeSlotInfo(0, "Guitarra"), mixcoach::TrackRole::Unknown);
    auto events = feed.getRecentEvents(0);
    bool found = countEventType(events, mixcoach::TrackEventType::TrackAppeared) > 0;
    TEST("TrackAppeared event generated on first update", found);
}

static void test_event_clipping_detected()
{
    std::printf("\n── Events: Clipping Detected ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;
    auto info = makeSlotInfo(0, "Kick");

    // Primera llamada: normal (no clipping)
    feed.updateTrackState(0, makeResult(-10.0f, -20.0f), info, mixcoach::TrackRole::Unknown);

    // Segunda llamada: clipping (peak > -0.5). timestamp mayor para transicion
    feed.updateTrackState(0, makeResult(-0.1f, -10.0f, 0.0f, kTestTimestampUs + 1000),
                           info, mixcoach::TrackRole::Unknown);

    auto events = feed.getRecentEvents(0);
    bool found = countEventType(events, mixcoach::TrackEventType::ClippingDetected) > 0;
    TEST("ClippingDetected event generated", found);
}

static void test_event_clipping_cleared()
{
    std::printf("\n── Events: Clipping Cleared ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;
    auto info = makeSlotInfo(0, "Kick");

    // Primera llamada: clipping
    feed.updateTrackState(0, makeResult(-0.1f, -10.0f), info, mixcoach::TrackRole::Unknown);

    // Segunda llamada: normal -> clipping cleared
    feed.updateTrackState(0, makeResult(-12.0f, -22.0f, 0.0f, kTestTimestampUs + 2000),
                           info, mixcoach::TrackRole::Unknown);

    auto events = feed.getRecentEvents(0);
    bool foundCleared = countEventType(events, mixcoach::TrackEventType::ClippingCleared) > 0;
    TEST("ClippingCleared event generated", foundCleared);
}

static void test_event_level_spike()
{
    std::printf("\n── Events: Level Spike ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;
    auto info = makeSlotInfo(0, "Snare");

    // Primera llamada: peak bajo (-20)
    feed.updateTrackState(0, makeResult(-20.0f, -30.0f), info, mixcoach::TrackRole::Unknown);

    // Segunda llamada: peak mucho mas alto (subio 10 dB -> > 6 dB threshold)
    feed.updateTrackState(0, makeResult(-10.0f, -20.0f, 0.0f, kTestTimestampUs + 1000),
                           info, mixcoach::TrackRole::Unknown);

    auto events = feed.getRecentEvents(0);
    bool found = countEventType(events, mixcoach::TrackEventType::LevelSpike) > 0;
    TEST("LevelSpike event generated (delta > 6dB)", found);
}

static void test_event_low_signal()
{
    std::printf("\n── Events: Low Signal ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;
    auto info = makeSlotInfo(0, "Ambience");

    // Primera llamada: normal
    feed.updateTrackState(0, makeResult(-15.0f, -25.0f), info, mixcoach::TrackRole::Unknown);

    // Segunda llamada: muy baja (peak < -30)
    feed.updateTrackState(0, makeResult(-40.0f, -50.0f, 0.0f, kTestTimestampUs + 1000),
                           info, mixcoach::TrackRole::Unknown);

    auto events = feed.getRecentEvents(0);
    bool found = countEventType(events, mixcoach::TrackEventType::LowSignal) > 0;
    TEST("LowSignal event generated (peak < -30dB)", found);
}

static void test_event_crest_too_low()
{
    std::printf("\n── Events: Crest Too Low ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;
    auto info = makeSlotInfo(0, "Bass");

    // Primera llamada: crest normal (peak -10, RMS -20 -> crest 10)
    feed.updateTrackState(0, makeResult(-10.0f, -20.0f), info, mixcoach::TrackRole::Unknown);

    // Segunda llamada: crest muy baja (peak -6, RMS -7 -> crest 1 -> < 4)
    feed.updateTrackState(0, makeResult(-6.0f, -7.0f, 0.0f, kTestTimestampUs + 1000),
                           info, mixcoach::TrackRole::Unknown);

    auto events = feed.getRecentEvents(0);
    bool found = countEventType(events, mixcoach::TrackEventType::CrestTooLow) > 0;
    TEST("CrestTooLow event generated (crest < 4dB)", found);
}

static void test_event_transient_detected()
{
    std::printf("\n── Events: Transient Detected ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;
    auto info = makeSlotInfo(0, "Kick");

    // Primera llamada: transientRatio normal
    feed.updateTrackState(0, makeResult(-10.0f, -20.0f, 0.0f, kTestTimestampUs, 1.0f),
                           info, mixcoach::TrackRole::Unknown);

    // Segunda llamada: transientRatio alto (> 3)
    feed.updateTrackState(0, makeResult(-10.0f, -20.0f, 0.0f, kTestTimestampUs + 1000, 5.0f),
                           info, mixcoach::TrackRole::Unknown);

    auto events = feed.getRecentEvents(0);
    bool found = countEventType(events, mixcoach::TrackEventType::TransientDetected) > 0;
    TEST("TransientDetected event generated (ratio > 3)", found);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: Cooldowns / Transitions
// ═══════════════════════════════════════════════════════════════════════════

static void test_clipping_transition_guard()
{
    std::printf("\n── Cooldowns: Clipping Transition Guard ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;
    auto info = makeSlotInfo(0, "Kick");

    // 1. Normal
    feed.updateTrackState(0, makeResult(-10.0f, -20.0f, 0.0f, kTestTimestampUs),
                           info, mixcoach::TrackRole::Unknown);
    int afterNormal = (int)feed.getTotalEventCount();
    TEST("Events after normal signal", afterNormal >= 1);  // TrackAppeared

    // 2. Clipping (normal->clipping) -> debe generar ClippingDetected
    feed.updateTrackState(0, makeResult(-0.1f, -10.0f, 0.0f, kTestTimestampUs + 1000),
                           info, mixcoach::TrackRole::Unknown);
    int afterClip = (int)feed.getTotalEventCount();
    TEST("New event after clipping transition", afterClip > afterNormal);

    // 3. Sigue clippeando -> NO debe generar otro ClippingDetected (transition guard)
    feed.updateTrackState(0, makeResult(-0.05f, -10.0f, 0.0f, kTestTimestampUs + 2000),
                           info, mixcoach::TrackRole::Unknown);
    int afterStillClip = (int)feed.getTotalEventCount();
    TEST("No duplicate clipping event (transition guard)", afterStillClip == afterClip);

    // 4. Deja de clipear -> debe generar ClippingCleared
    feed.updateTrackState(0, makeResult(-12.0f, -22.0f, 0.0f, kTestTimestampUs + 3000),
                           info, mixcoach::TrackRole::Unknown);
    int afterCleared = (int)feed.getTotalEventCount();
    TEST("New event after clipping cleared", afterCleared > afterStillClip);

    // 5. Vuelve a clipear -> debe generar otro ClippingDetected
    feed.updateTrackState(0, makeResult(-0.1f, -10.0f, 0.0f, kTestTimestampUs + 100000000),
                           info, mixcoach::TrackRole::Unknown);
    int afterReclip = (int)feed.getTotalEventCount();
    TEST("New event when clipping again after fix", afterReclip > afterCleared);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: Priority Scoring
// ═══════════════════════════════════════════════════════════════════════════

static void test_attention_clipping_vs_clean()
{
    std::printf("\n── Priority: Clipping vs Clean ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;

    feed.updateTrackState(0, makeResult(-0.1f, -10.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(0, "Kick", mixcoach::BusType::Drums),
                           mixcoach::TrackRole::Kick);
    feed.updateTrackState(1, makeResult(-12.0f, -22.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(1, "Vocal", mixcoach::BusType::Vocals),
                           mixcoach::TrackRole::VozPrincipal);

    float scoreClip  = feed.getAttentionScore(0);
    float scoreClean = feed.getAttentionScore(1);

    TEST("Clipping track has attention > 0",      scoreClip > 0.0f);
    TEST("Clean track has attention >= 0",          scoreClean >= 0.0f);
    TEST("Clipping attention > Clean attention",    scoreClip > scoreClean);
}

static void test_attention_overcompressed_vs_clean()
{
    std::printf("\n── Priority: Overcompressed vs Clean ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;

    feed.updateTrackState(0, makeResult(-6.0f, -7.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(0, "Bass", mixcoach::BusType::Bass),
                           mixcoach::TrackRole::BassSub);
    feed.updateTrackState(1, makeResult(-12.0f, -22.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(1, "Pad", mixcoach::BusType::Keys),
                           mixcoach::TrackRole::SynthPad);

    float scoreOver  = feed.getAttentionScore(0);
    float scoreClean = feed.getAttentionScore(1);

    TEST("Overcompressed attention > Clean attention", scoreOver > scoreClean);
}

static void test_attention_role_importance()
{
    std::printf("\n── Priority: Role Importance ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;

    // Mismo nivel de audio, roles diferentes
    feed.updateTrackState(0, makeResult(-12.0f, -22.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(0, "Kick", mixcoach::BusType::Drums),
                           mixcoach::TrackRole::Kick);
    feed.updateTrackState(1, makeResult(-12.0f, -22.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(1, "Riser", mixcoach::BusType::FX),
                           mixcoach::TrackRole::FxRiser);

    float scoreKick  = feed.getAttentionScore(0);
    float scoreRiser = feed.getAttentionScore(1);
    TEST("Kick attention > FX Riser attention (same audio)", scoreKick > scoreRiser);
}

static void test_priority_ordering()
{
    std::printf("\n── Priority: Track Ordering ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;

    feed.updateTrackState(0, makeResult(-0.1f, -10.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(0, "Kick"), mixcoach::TrackRole::Kick);
    feed.updateTrackState(1, makeResult(-6.0f, -7.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(1, "Bass"), mixcoach::TrackRole::BassSub);
    feed.updateTrackState(2, makeResult(-12.0f, -22.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(2, "Vocal"), mixcoach::TrackRole::VozPrincipal);

    auto priority = feed.getActiveTracksByPriority();
    TEST("3 tracks in priority list",       priority.size() == 3);

    bool firstIsClipping = (priority.size() >= 1 && priority[0] == 0);
    TEST("Highest priority = clipping track", firstIsClipping);

    bool lastIsClean = (priority.size() >= 3 && priority[2] == 2);
    TEST("Lowest priority = clean track", lastIsClean);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: Health Summary
// ═══════════════════════════════════════════════════════════════════════════

static void test_health_summary()
{
    std::printf("\n── Health Summary ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;

    feed.updateTrackState(0, makeResult(-12.0f, -22.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(0, "Vocal"), mixcoach::TrackRole::Unknown);           // clean
    feed.updateTrackState(1, makeResult(-0.1f, -10.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(1, "Kick"), mixcoach::TrackRole::Unknown);            // clipping -> critical
    feed.updateTrackState(2, makeResult(-6.0f, -7.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(2, "Bass"), mixcoach::TrackRole::Unknown);            // overcompressed -> critical
    feed.updateTrackState(3, makeResult(-2.0f, -30.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(3, "Snare"), mixcoach::TrackRole::Unknown);           // needs compression -> warning
    feed.updateTrackState(4, makeResult(-40.0f, -50.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(4, "Ambience"), mixcoach::TrackRole::Unknown);        // low signal -> silent

    auto summary = feed.getHealthSummary();
    TEST("HealthSummary: 1 clean",      summary.clean == 1);
    TEST("HealthSummary: 1 warning",    summary.warning == 1);
    TEST("HealthSummary: 2 critical",   summary.critical == 2);
    TEST("HealthSummary: 1 silent",     summary.silent == 1);
    TEST("HealthSummary: 0 unknown",    summary.unknown == 0);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: Track lifecycle
// ═══════════════════════════════════════════════════════════════════════════

static void test_mark_track_disappeared()
{
    std::printf("\n── Lifecycle: Track Disappeared ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;

    feed.updateTrackState(0, makeResult(-10.0f, -20.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(0, "Guitarra"), mixcoach::TrackRole::Unknown);
    feed.markTrackDisappeared(0);

    auto state = feed.getTrackState(0);
    TEST("Disappeared: active = false",  state.active == false);
    TEST("Disappeared: health = Silent", state.health == mixcoach::TrackHealth::Silent);

    auto events = feed.getRecentEvents(0);
    bool found = countEventType(events, mixcoach::TrackEventType::TrackDisappeared) > 0;
    TEST("TrackDisappeared event generated", found);
}

static void test_mark_track_disappeared_twice()
{
    std::printf("\n── Lifecycle: Disappear Twice (no-op) ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;

    feed.updateTrackState(0, makeResult(-10.0f, -20.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(0, "Guitarra"), mixcoach::TrackRole::Unknown);
    feed.markTrackDisappeared(0);
    int afterFirst = (int)feed.getTotalEventCount();

    feed.markTrackDisappeared(0);  // No deberia generar otro evento (ya inactive)
    int afterSecond = (int)feed.getTotalEventCount();
    TEST("Second disappeared call is no-op", afterSecond == afterFirst);
}

static void test_update_track_role()
{
    std::printf("\n── Lifecycle: Track Role Update ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;

    feed.updateTrackState(0, makeResult(-10.0f, -20.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(0, "Guitarra"), mixcoach::TrackRole::Unknown);
    feed.updateTrackRole(0, mixcoach::TrackRole::GuitarElectric);

    auto state = feed.getTrackState(0);
    TEST("Role updated to GuitarElectric",
         state.role == mixcoach::TrackRole::GuitarElectric);

    auto events = feed.getRecentEvents(0);
    bool found = countEventType(events, mixcoach::TrackEventType::RoleAssigned) > 0;
    TEST("RoleAssigned event generated", found);
}

static void test_update_track_role_same()
{
    std::printf("\n── Lifecycle: Same Role (no event) ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;

    // updateTrackState asigna el rol pero NO genera RoleAssigned event
    feed.updateTrackState(0, makeResult(-10.0f, -20.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(0, "Guitarra"), mixcoach::TrackRole::GuitarElectric);

    // updateTrackRole con el MISMO rol -> NO debe generar evento
    feed.updateTrackRole(0, mixcoach::TrackRole::GuitarElectric);
    auto events = feed.getRecentEvents(0);
    int roleEvents = countEventType(events, mixcoach::TrackEventType::RoleAssigned);

    // Solo updateTrackRole genera RoleAssigned events, y esta llamada
    // tiene el mismo rol -> 0 eventos de RoleAssigned
    TEST("No RoleAssigned event when same role", roleEvents == 0);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: Clear All
// ═══════════════════════════════════════════════════════════════════════════

static void test_clear_all()
{
    std::printf("\n── Clear All ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;

    feed.updateTrackState(0, makeResult(-10.0f, -20.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(0, "Kick"), mixcoach::TrackRole::Kick);
    feed.updateTrackState(1, makeResult(-12.0f, -22.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(1, "Vocal"), mixcoach::TrackRole::VozPrincipal);

    TEST("2 active tracks before clear", feed.getActiveTrackCount() == 2);
    TEST("Events exist before clear",    feed.getTotalEventCount() > 0);

    feed.clearAll();

    TEST("0 active tracks after clear",  feed.getActiveTrackCount() == 0);
    TEST("0 events after clear",         feed.getTotalEventCount() == 0);
    TEST("Global events empty after clear", feed.getGlobalEvents(10).empty());

    auto state = feed.getTrackState(0);
    TEST("State reset after clear: active = false",    state.active == false);
    TEST("State reset after clear: health = Unknown",  state.health == mixcoach::TrackHealth::Unknown);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: Global events
// ═══════════════════════════════════════════════════════════════════════════

static void test_global_events_order()
{
    std::printf("\n── Global Events: Severity Order ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;

    // Track 0: clipping (severity 1.0)
    feed.updateTrackState(0, makeResult(-0.1f, -10.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(0, "Kick"), mixcoach::TrackRole::Kick);

    // Track 1: level spike
    feed.updateTrackState(1, makeResult(-20.0f, -30.0f, 0.85f, kTestTimestampUs),
                           makeSlotInfo(1, "Snare"), mixcoach::TrackRole::Unknown);
    feed.updateTrackState(1, makeResult(-10.0f, -20.0f, 0.85f, kTestTimestampUs + 1000),
                           makeSlotInfo(1, "Snare"), mixcoach::TrackRole::Unknown);

    auto global = feed.getGlobalEvents(10);
    TEST("Global events > 0", global.size() > 0);
    if (global.size() >= 2) {
        TEST("First global event has highest severity",
             global[0].severity >= global[1].severity);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: Edge cases
// ═══════════════════════════════════════════════════════════════════════════

static void test_edge_invalid_slot_update()
{
    std::printf("\n── Edge Cases: Invalid Slot ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;

    auto result = makeResult(-10.0f, -20.0f);
    auto info   = makeSlotInfo(-1, "Invalid");
    feed.updateTrackState(-1, result, info, mixcoach::TrackRole::Unknown);
    TEST("updateTrackState with slot -1 does not crash", true);

    feed.updateTrackState(999, result, info, mixcoach::TrackRole::Unknown);
    TEST("updateTrackState with slot 999 does not crash", true);

    TEST("getAttentionScore(-1) = 0.0", std::abs(feed.getAttentionScore(-1)) < 0.01f);

    feed.markTrackDisappeared(-1);
    TEST("markTrackDisappeared(-1) does not crash", true);
}

static void test_edge_silent_track_no_events()
{
    std::printf("\n── Edge Cases: Silent Track ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;

    // Track sin timestamp (timestampUs = 0) -> no debe generar eventos de audio
    auto result = makeResult(-100.0f, -100.0f, 0.0f, 0);
    auto info   = makeSlotInfo(0, "Silent");
    feed.updateTrackState(0, result, info, mixcoach::TrackRole::Unknown);

    auto events = feed.getRecentEvents(0);
    int audioEvents = countEventType(events, mixcoach::TrackEventType::ClippingDetected)
                    + countEventType(events, mixcoach::TrackEventType::LevelSpike)
                    + countEventType(events, mixcoach::TrackEventType::LowSignal);
    TEST("No audio events for silent track", audioEvents == 0);
}

static void test_edge_negative_correlation()
{
    std::printf("\n── Edge Cases: Negative Correlation ──\n");
    std::fflush(stdout);

    mixcoach::TrackFeedCore feed;

    auto result = makeResult(-10.0f, -20.0f, -0.8f, kTestTimestampUs);
    auto info   = makeSlotInfo(0, "Problematic");
    feed.updateTrackState(0, result, info, mixcoach::TrackRole::Unknown);

    auto state = feed.getTrackState(0);
    TEST("Negative correlation stored correctly",
         std::abs(state.correlation - (-0.8f)) < 0.01f);
    TEST("Negative correlation -> StereoCollapse",
         state.health == mixcoach::TrackHealth::StereoCollapse);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════

int main()
{
    // Inicializar timestamp base con uptime del sistema en microsegundos
    // Esto garantiza que eventos queden dentro de la retention window (60s)
    kTestTimestampUs = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;

    std::printf("══════════════════════════════════════════════════════════\n");
    std::printf("  TrackFeedCore Unit Tests (V4)\n");
    std::printf("  TrackState conversion, event generation, priority scoring\n");
    std::printf("══════════════════════════════════════════════════════════\n\n");
    std::fflush(stdout);

    // TrackState conversion
    test_trackstate_conversion();
    test_trackstate_invalid_index();
    test_trackstate_crest_factor();

    // Health detection
    test_health_clean();
    test_health_clipping();
    test_health_overcompressed();
    test_health_stereo_collapse();
    test_health_needs_compression();
    test_health_low_signal();
    test_health_silent();

    // Event generation
    test_event_track_appeared();
    test_event_clipping_detected();
    test_event_clipping_cleared();
    test_event_level_spike();
    test_event_low_signal();
    test_event_crest_too_low();
    test_event_transient_detected();

    // Cooldowns / Transitions
    test_clipping_transition_guard();

    // Priority scoring
    test_attention_clipping_vs_clean();
    test_attention_overcompressed_vs_clean();
    test_attention_role_importance();
    test_priority_ordering();

    // Health Summary
    test_health_summary();

    // Track lifecycle
    test_mark_track_disappeared();
    test_mark_track_disappeared_twice();
    test_update_track_role();
    test_update_track_role_same();

    // Clear All
    test_clear_all();

    // Global events
    test_global_events_order();

    // Edge cases
    test_edge_invalid_slot_update();
    test_edge_silent_track_no_events();
    test_edge_negative_correlation();

    std::printf("\n══════════════════════════════════════════════════════════\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("══════════════════════════════════════════════════════════\n");
    std::fflush(stdout);

    return gTestsFailed > 0 ? 1 : 0;
}
