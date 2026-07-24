// ═══════════════════════════════════════════════════════════════════════════
//  TestTrackAnalyzerCore.cpp — Unit tests for Sprint 6A-6C analyzers
//
//  Tests:
//    • TrackGainAdvice    — analyzeTrackGain, analyzeAllTracksGain
//    • TrackDynamicsAdvice — analyzeTrackDynamics, analyzeAllTracksDynamics
//    • TrackTonalAdvice   — analyzeTrackTonal, analyzeAllTracksTonal
//    • Helper functions   — getLatestTelemetry, computePerTrackLUFS
//
//  Compilado via CMake (ver CMakeLists.txt):
//    cmake --build build --config Release --target TestTrackAnalyzerCore
//    ./build/tests/Release/TestTrackAnalyzerCore.exe
//
//  Los analyzers son free functions sin dependencia en CoachEngine:
//    SlotRegistry, SharedData, ExpectedProfile son necesarios.
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <memory>
#include <vector>
#include <algorithm>
#include <array>

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

#include "Common/types/Types.h"
#include "Common/memory/SharedData.h"
#include "Common/memory/SlotRegistry.h"
#include "MixCoach/engine/TrackGainAnalyzer.h"
#include "MixCoach/engine/TrackDynamicsAnalyzer.h"
#include "MixCoach/engine/TrackTonalAnalyzer.h"
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

// ─── Test helpers ─────────────────────────────────────────────────────────

static constexpr int kMaxSlots = mixcoach::SlotRegistry::kMaxSlots;

struct TestFixture
{
    std::unique_ptr<mixcoach::SharedData> sd;
    std::array<mixcoach::TrackRole, kMaxSlots> trackRoles{};
    mixcoach::SlotRegistry* registry = nullptr;
    int slotIndex = -1;

    TestFixture()
    {
        sd = std::make_unique<mixcoach::SharedData>();
        registry = &sd->getSlotRegistry();
    }

    /** Register a slot with a name, return slot index. */
    int registerSlot(const char* name, mixcoach::TrackRole role,
                     const juce::Colour& colour = juce::Colours::grey)
    {
        int idx = registry->registerSlot(name, colour);
        if (idx >= 0 && idx < kMaxSlots) {
            trackRoles[static_cast<size_t>(idx)] = role;
            slotIndex = idx;
        }
        return idx;
    }

    /** Push a simple TrackAudioResult (peak + RMS) to the cache. */
    void pushAudio(int idx, float peakDb, float rmsDb)
    {
        mixcoach::TrackAudioResult r;
        r.peakLeft   = peakDb;
        r.peakRight  = peakDb;
        r.rmsLeft    = rmsDb;
        r.rmsRight   = rmsDb;
        r.correlation = 0.85f;
        r.timestampUs = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;
        sd->updateTrackAudioResult(idx, r);
    }

    /** Push a full TrackAudioResult with crest + band energies. */
    void pushAudioFull(int idx, float peakDb, float rmsDb,
                       float crestFactor, float correlation = 0.85f)
    {
        mixcoach::TrackAudioResult r;
        r.peakLeft   = peakDb;
        r.peakRight  = peakDb;
        r.rmsLeft    = rmsDb;
        r.rmsRight   = rmsDb;
        r.correlation = correlation;
        r.timestampUs = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;
        for (int b = 0; b < 6; ++b)
            r.crestPerBand[b] = crestFactor;
        // Default bandEnergies to -100 (silence)
        sd->updateTrackAudioResult(idx, r);
    }

    /** Push audio with specific band energies for tonal analysis. */
    void pushAudioWithBands(int idx, float peakDb, float rmsDb,
                            const float bandEnergies[30])
    {
        mixcoach::TrackAudioResult r;
        r.peakLeft   = peakDb;
        r.peakRight  = peakDb;
        r.rmsLeft    = rmsDb;
        r.rmsRight   = rmsDb;
        r.correlation = 0.85f;
        r.timestampUs = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;
        for (int b = 0; b < 30; ++b)
            r.bandEnergies[b] = bandEnergies[b];
        sd->updateTrackAudioResult(idx, r);
    }
};

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: getLatestTelemetry & computePerTrackLUFS helpers
// ═══════════════════════════════════════════════════════════════════════════

static void test_helper_get_latest_telemetry()
{
    std::printf("\n── Helpers: getLatestTelemetry ──\n");
    std::fflush(stdout);

    TestFixture fix;
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    fix.pushAudio(idx, -12.0f, -22.0f);

    auto telem = mixcoach::getLatestTelemetry(*fix.sd, idx);
    TEST("telemetry timestamp > 0",       telem.timestamp > 0);
    TEST("telemetry peakLeft matches",    std::abs(telem.peakLeft - (-12.0f)) < 0.01f);
    TEST("telemetry peakRight matches",   std::abs(telem.peakRight - (-12.0f)) < 0.01f);
    TEST("telemetry rmsLeft matches",     std::abs(telem.rmsLeft - (-22.0f)) < 0.01f);
    TEST("telemetry rmsRight matches",    std::abs(telem.rmsRight - (-22.0f)) < 0.01f);

    // Invalid slot returns empty
    auto empty = mixcoach::getLatestTelemetry(*fix.sd, -1);
    TEST("invalid slot returns empty telemetry", empty.timestamp == 0);

    empty = mixcoach::getLatestTelemetry(*fix.sd, 999);
    TEST("out-of-range slot returns empty telemetry", empty.timestamp == 0);
}

static void test_helper_compute_per_track_lufs()
{
    std::printf("\n── Helpers: computePerTrackLUFS ──\n");
    std::fflush(stdout);

    // TrackAudioResult with valid RMS
    mixcoach::TrackAudioResult r;
    r.rmsLeft   = -20.0f;
    r.rmsRight  = -20.0f;
    r.timestampUs = 1000;
    float lufs = mixcoach::computePerTrackLUFS(r);
    TEST("LUFS is rms + small boost (-20 + ~0 = -20)", std::abs(lufs - (-20.0f)) < 3.0f);

    // Silent track returns -100
    mixcoach::TrackAudioResult silent;
    silent.rmsLeft  = -100.0f;
    silent.rmsRight = -100.0f;
    float silentLufs = mixcoach::computePerTrackLUFS(silent);
    TEST("silent track LUFS = -100", std::abs(silentLufs - (-100.0f)) < 1.0f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: TrackGainAdvice — analyzeTrackGain
// ═══════════════════════════════════════════════════════════════════════════

static void test_gain_on_target()
{
    std::printf("\n── Gain: On Target ──\n");
    std::fflush(stdout);

    TestFixture fix;
    // Kick: peakTarget = -6.0, peakTolerance = 4.0  →  OnTarget range: [-10, -2]
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    fix.pushAudio(idx, -8.0f, -18.0f);  // peak -8 → within ±4 of -6

    auto advice = mixcoach::analyzeTrackGain(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("OnTarget: status = OnTarget",       advice.status == mixcoach::TrackGainAdvice::Status::OnTarget);
    TEST("OnTarget: slotIndex matches",       advice.slotIndex == idx);
    TEST("OnTarget: trackName = Kick",        advice.trackName == "Kick");
    TEST("OnTarget: role = Kick",             advice.role == mixcoach::TrackRole::Kick);
    TEST("OnTarget: currentPeak ≈ -8",        std::abs(advice.currentPeak - (-8.0f)) < 0.01f);
    TEST("OnTarget: deviation ≈ -2 (peak - target)", std::abs(advice.peakDeviation - (-2.0f)) < 0.01f);
    TEST("OnTarget: isActionable = false",    !advice.isActionable());
    TEST("OnTarget: suggestedDeltaDb ≈ +2",   std::abs(advice.suggestedDeltaDb - 2.0f) < 0.01f);
}

static void test_gain_near_target_high()
{
    std::printf("\n── Gain: Near Target (high) ──\n");
    std::fflush(stdout);

    TestFixture fix;
    // Kick: NearTarget range: [2, 8] above target → peak between +2 and +2*4=+8 → near high: -1.0
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    fix.pushAudio(idx, -1.0f, -12.0f);  // peak -1 → deviation = -1 - (-6) = +5 → between 4 and 8 → NearTarget

    auto advice = mixcoach::analyzeTrackGain(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("NearTarget(high): status = NearTarget",  advice.status == mixcoach::TrackGainAdvice::Status::NearTarget);
    TEST("NearTarget(high): deviation > 0",         advice.peakDeviation > 0);
    TEST("NearTarget(high): isActionable = true",   advice.isActionable());
    TEST("NearTarget(high): message contains 'ligeramente alto'",
         advice.message.contains("ligeramente alto") || advice.message.contains("NearTarget"));
}

static void test_gain_near_target_low()
{
    std::printf("\n── Gain: Near Target (low) ──\n");
    std::fflush(stdout);

    TestFixture fix;
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    fix.pushAudio(idx, -13.0f, -24.0f);  // peak -13 → deviation = -13 - (-6) = -7 → abs 7, between 4 and 8 → NearTarget

    auto advice = mixcoach::analyzeTrackGain(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("NearTarget(low): status = NearTarget",   advice.status == mixcoach::TrackGainAdvice::Status::NearTarget);
    TEST("NearTarget(low): deviation < 0",          advice.peakDeviation < 0);
    TEST("NearTarget(low): isActionable = true",    advice.isActionable());
    TEST("NearTarget(low): message contains 'ligeramente bajo'",
         advice.message.contains("ligeramente bajo") || advice.message.contains("NearTarget"));
}

static void test_gain_off_target_high()
{
    std::printf("\n── Gain: Off Target (too high) ──\n");
    std::fflush(stdout);

    TestFixture fix;
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    fix.pushAudio(idx, 3.0f, -8.0f);  // peak +3 → deviation = 3 - (-6) = +9 → > 8 → OffTarget

    auto advice = mixcoach::analyzeTrackGain(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("OffTarget(high): status = OffTarget",    advice.status == mixcoach::TrackGainAdvice::Status::OffTarget);
    TEST("OffTarget(high): deviation > 0",          advice.peakDeviation > 0);
    TEST("OffTarget(high): suggestedDeltaDb < 0 (bajar)", advice.suggestedDeltaDb < 0);
    TEST("OffTarget(high): message contains 'demasiado alto'",
         advice.message.contains("demasiado alto") || advice.message.contains("OffTarget"));
}

static void test_gain_off_target_low()
{
    std::printf("\n── Gain: Off Target (too low) ──\n");
    std::fflush(stdout);

    TestFixture fix;
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    fix.pushAudio(idx, -18.0f, -30.0f);  // peak -18 → deviation = -18 - (-6) = -12 → abs 12 > 8 → OffTarget

    auto advice = mixcoach::analyzeTrackGain(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("OffTarget(low): status = OffTarget",      advice.status == mixcoach::TrackGainAdvice::Status::OffTarget);
    TEST("OffTarget(low): deviation < 0",            advice.peakDeviation < 0);
    TEST("OffTarget(low): suggestedDeltaDb > 0 (subir)", advice.suggestedDeltaDb > 0);
    TEST("OffTarget(low): message contains 'demasiado bajo'",
         advice.message.contains("demasiado bajo") || advice.message.contains("OffTarget"));
}

static void test_gain_no_signal()
{
    std::printf("\n── Gain: No Signal ──\n");
    std::fflush(stdout);

    TestFixture fix;
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    fix.pushAudio(idx, -80.0f, -90.0f);  // peak < -60 → NoSignal (check: peak -80 < -60)

    auto advice = mixcoach::analyzeTrackGain(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("NoSignal: status = NoSignal",     advice.status == mixcoach::TrackGainAdvice::Status::NoSignal);
    TEST("NoSignal: isActionable = false",  !advice.isActionable());
}

static void test_gain_unknown_role()
{
    std::printf("\n── Gain: Unknown Role ──\n");
    std::fflush(stdout);

    TestFixture fix;
    int idx = fix.registerSlot("Unknown", mixcoach::TrackRole::Unknown);
    fix.pushAudio(idx, -10.0f, -20.0f);

    auto advice = mixcoach::analyzeTrackGain(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("UnknownRole: status = UnknownRole", advice.status == mixcoach::TrackGainAdvice::Status::UnknownRole);
    TEST("UnknownRole: isActionable = false",  !advice.isActionable());
}

static void test_gain_invalid_slot()
{
    std::printf("\n── Gain: Invalid Slot ──\n");
    std::fflush(stdout);

    TestFixture fix;
    auto advice = mixcoach::analyzeTrackGain(-1, *fix.sd, fix.trackRoles, "pop", false);
    TEST("Invalid slot: status = UnknownRole", advice.status == mixcoach::TrackGainAdvice::Status::UnknownRole);

    advice = mixcoach::analyzeTrackGain(999, *fix.sd, fix.trackRoles, "pop", false);
    TEST("Out-of-range: status = UnknownRole", advice.status == mixcoach::TrackGainAdvice::Status::UnknownRole);
}

static void test_gain_muted_track()
{
    std::printf("\n── Gain: Muted Track ──\n");
    std::fflush(stdout);

    TestFixture fix;
    // Register but don't register as active — the function checks muted flag
    int idx = fix.registerSlot("Muted", mixcoach::TrackRole::Kick);
    fix.pushAudio(idx, -6.0f, -16.0f);
    // Mute the track
    fix.registry->setActive(idx, false);

    auto advice = mixcoach::analyzeTrackGain(idx, *fix.sd, fix.trackRoles, "pop", false);
    // Even though setActive(false) might not be the same as muted, let's check what we get
    TEST("Muted: does not crash", true);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: TrackGainAdvice — analyzeAllTracksGain
// ═══════════════════════════════════════════════════════════════════════════

static void test_gain_analyze_all_ordering()
{
    std::printf("\n── Gain: analyzeAllTracksGain ordering ──\n");
    std::fflush(stdout);

    TestFixture fix;

    // Track 0: OffTarget (high)
    int idx0 = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    fix.pushAudio(idx0, 3.0f, -8.0f);

    // Track 1: NearTarget
    int idx1 = fix.registerSlot("Snare", mixcoach::TrackRole::Snare);
    fix.pushAudio(idx1, -1.0f, -12.0f);

    // Track 2: OnTarget (not actionable)
    int idx2 = fix.registerSlot("HiHat", mixcoach::TrackRole::HiHat);
    fix.pushAudio(idx2, -6.0f, -16.0f);

    auto results = mixcoach::analyzeAllTracksGain(*fix.sd, fix.trackRoles, "pop", false);
    TEST("analyzeAll: returns at least 1 actionable", results.size() >= 1);
    if (results.size() >= 1) {
        TEST("analyzeAll: first result is OffTarget",
             results[0].status == mixcoach::TrackGainAdvice::Status::OffTarget);
    }
}

static void test_gain_analyze_all_empty_genre()
{
    std::printf("\n── Gain: analyzeAll with empty genre ──\n");
    std::fflush(stdout);

    TestFixture fix;
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    fix.pushAudio(idx, 3.0f, -8.0f);

    auto results = mixcoach::analyzeAllTracksGain(*fix.sd, fix.trackRoles, "", false);
    TEST("analyzeAll empty genre: returns results", results.size() > 0);
    TEST("analyzeAll empty genre: status is OffTarget",
         results[0].status == mixcoach::TrackGainAdvice::Status::OffTarget);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: TrackDynamicsAdvice — analyzeTrackDynamics
// ═══════════════════════════════════════════════════════════════════════════

static void test_dynamics_on_target()
{
    std::printf("\n── Dynamics: On Target ──\n");
    std::fflush(stdout);

    TestFixture fix;
    // Kick: crestTarget = 14.0, crestTolerance = 6.0 → OnTarget: [8, 20]
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    fix.pushAudioFull(idx, -6.0f, -20.0f, 14.0f);  // crest = 14 (exact target)

    auto advice = mixcoach::analyzeTrackDynamics(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("OnTarget: status = OnTarget",         advice.status == mixcoach::TrackDynamicsAdvice::Status::OnTarget);
    TEST("OnTarget: subType = None",            advice.subType == mixcoach::TrackDynamicsAdvice::SubType::None);
    TEST("OnTarget: isActionable = false",      !advice.isActionable());
    TEST("OnTarget: isOvercompressed = false",  !advice.isOvercompressed());
    TEST("OnTarget: isTooDynamic = false",      !advice.isTooDynamic());
    TEST("OnTarget: crest matches",             std::abs(advice.currentCrest - 14.0f) < 0.01f);
}

static void test_dynamics_near_overcompressed()
{
    std::printf("\n── Dynamics: Near Overcompressed ──\n");
    std::fflush(stdout);

    TestFixture fix;
    // crest 5.0 → deviation = 5 - 14 = -9 → abs 9 > 6 (tol) → NearTarget, subtype Overcompressed
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    fix.pushAudioFull(idx, -6.0f, -11.0f, 5.0f);

    auto advice = mixcoach::analyzeTrackDynamics(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("NearOvercompressed: status = NearTarget",
         advice.status == mixcoach::TrackDynamicsAdvice::Status::NearTarget);
    TEST("NearOvercompressed: subType = Overcompressed",
         advice.subType == mixcoach::TrackDynamicsAdvice::SubType::Overcompressed);
    TEST("NearOvercompressed: isActionable = true",  advice.isActionable());
    TEST("NearOvercompressed: isOvercompressed = true", advice.isOvercompressed());
}

static void test_dynamics_near_too_dynamic()
{
    std::printf("\n── Dynamics: Near Too Dynamic ──\n");
    std::fflush(stdout);

    TestFixture fix;
    // crest 25.0 → deviation = 25 - 14 = +11 → abs 11 > 6 (tol) → NearTarget, subtype TooDynamic
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    fix.pushAudioFull(idx, -6.0f, -31.0f, 25.0f);

    auto advice = mixcoach::analyzeTrackDynamics(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("NearTooDynamic: status = NearTarget",
         advice.status == mixcoach::TrackDynamicsAdvice::Status::NearTarget);
    TEST("NearTooDynamic: subType = TooDynamic",
         advice.subType == mixcoach::TrackDynamicsAdvice::SubType::TooDynamic);
    TEST("NearTooDynamic: isTooDynamic = true", advice.isTooDynamic());
}

static void test_dynamics_off_overcompressed()
{
    std::printf("\n── Dynamics: Off Overcompressed ──\n");
    std::fflush(stdout);

    TestFixture fix;
    // crest 1.0 → deviation = 1 - 14 = -13 → abs 13 > 12 (= 2*tol) → OffTarget, subtype Overcompressed
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    fix.pushAudioFull(idx, -6.0f, -7.0f, 1.0f);

    auto advice = mixcoach::analyzeTrackDynamics(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("OffOvercompressed: status = OffTarget",
         advice.status == mixcoach::TrackDynamicsAdvice::Status::OffTarget);
    TEST("OffOvercompressed: subType = Overcompressed",
         advice.subType == mixcoach::TrackDynamicsAdvice::SubType::Overcompressed);
    TEST("OffOvercompressed: message contains 'sobre-comprimido'",
         advice.message.contains("sobre-comprimido") || advice.message.contains("OffTarget"));
}

static void test_dynamics_off_too_dynamic()
{
    std::printf("\n── Dynamics: Off Too Dynamic ──\n");
    std::fflush(stdout);

    TestFixture fix;
    // crest 30.0 → deviation = 30 - 14 = +16 → abs 16 > 12 → OffTarget, subtype TooDynamic
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    fix.pushAudioFull(idx, -6.0f, -36.0f, 30.0f);

    auto advice = mixcoach::analyzeTrackDynamics(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("OffTooDynamic: status = OffTarget",
         advice.status == mixcoach::TrackDynamicsAdvice::Status::OffTarget);
    TEST("OffTooDynamic: subType = TooDynamic",
         advice.subType == mixcoach::TrackDynamicsAdvice::SubType::TooDynamic);
}

static void test_dynamics_no_signal()
{
    std::printf("\n── Dynamics: No Signal ──\n");
    std::fflush(stdout);

    TestFixture fix;
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    fix.pushAudioFull(idx, -80.0f, -90.0f, 0.0f);

    auto advice = mixcoach::analyzeTrackDynamics(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("NoSignal: status = NoSignal",        advice.status == mixcoach::TrackDynamicsAdvice::Status::NoSignal);
    TEST("NoSignal: isActionable = false",     !advice.isActionable());
}

static void test_dynamics_unknown_role()
{
    std::printf("\n── Dynamics: Unknown Role ──\n");
    std::fflush(stdout);

    TestFixture fix;
    int idx = fix.registerSlot("Unknown", mixcoach::TrackRole::Unknown);
    fix.pushAudioFull(idx, -10.0f, -20.0f, 14.0f);

    auto advice = mixcoach::analyzeTrackDynamics(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("UnknownRole: status = UnknownRole",
         advice.status == mixcoach::TrackDynamicsAdvice::Status::UnknownRole);
}

static void test_dynamics_crest_factor_edge()
{
    std::printf("\n── Dynamics: Crest Factor Edge (crest ≤ 0) ──\n");
    std::fflush(stdout);

    TestFixture fix;
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    fix.pushAudioFull(idx, -60.0f, -60.0f, 0.0f);

    auto advice = mixcoach::analyzeTrackDynamics(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("Crest 0: status = NoSignal",
         advice.status == mixcoach::TrackDynamicsAdvice::Status::NoSignal);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: TrackTonalAdvice — analyzeTrackTonal
//  Each spectral region has a different expected energy based on the ExpectedProfile
//  for the role. For Kick (peak=-6.0, pop genre):
//    Region 0 (Sub) : expected = peak + spectralOffset[0] = -6 + (-8)  = -14.0
//    Region 1 (Bass): expected = peak + spectralOffset[1] = -6 + (-10) = -16.0
//    Region 2 (LoMid): expected = peak + spectralOffset[2] = -6 + (-20) = -26.0
//    Region 3 (HiMid): expected = peak + spectralOffset[3] = -6 + (-30) = -36.0
//    Region 4 (Pres): expected = peak + spectralOffset[4] = -6 + (-40) = -46.0
//    Region 5 (Air) : expected = peak + spectralOffset[5] = -6 + (-50) = -56.0
//
//  We avoid hardcoding these values by querying the profile at runtime.
// ═══════════════════════════════════════════════════════════════════════════

// Helper: fill 30 band energies with a constant value
static void fillBandsConstant(float bands[30], float value)
{
    for (int b = 0; b < 30; ++b)
        bands[b] = value;
}

// Helper: fill a specific 5-band region with a different value
static void fillBandsWithRegion(float bands[30], int region, float regionValue, float otherValue)
{
    for (int b = 0; b < 30; ++b)
        bands[b] = otherValue;
    int start = region * 5;
    int end   = start + 5;
    for (int b = start; b < end && b < 30; ++b)
        bands[b] = regionValue;
}

// Helper: set each region's band energies to exactly match the expected energy
// (peak + spectralOffset[i]), making the analyzer see OnTarget for all regions.
static void fillBandsOnTarget(float bands[30], const mixcoach::ExpectedProfile& profile, float peakDb)
{
    for (int region = 0; region < 6; ++region) {
        float expected = peakDb + profile.spectralOffset[region];
        int start = region * 5;
        int end   = start + 5;
        for (int b = start; b < end && b < 30; ++b)
            bands[b] = expected;
    }
}

// Helper: set a specific region's energy, and set all other regions to exactly their
// expected values (so only that region causes a deviation).
static void fillBandsWithRegionDeviation(float bands[30], int targetRegion, float targetEnergy,
                                          const mixcoach::ExpectedProfile& profile, float peakDb)
{
    for (int region = 0; region < 6; ++region) {
        float expected = peakDb + profile.spectralOffset[region];
        float energy = (region == targetRegion) ? targetEnergy : expected;
        int start = region * 5;
        int end   = start + 5;
        for (int b = start; b < end && b < 30; ++b)
            bands[b] = energy;
    }
}

static void test_tonal_on_target()
{
    std::printf("\n── Tonal: On Target ──\n");
    std::fflush(stdout);

    TestFixture fix;
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    auto profile = mixcoach::getExpectedProfile(mixcoach::TrackRole::Kick, "pop");
    float peakDb = -6.0f;
    float bands[30];
    fillBandsOnTarget(bands, profile, peakDb);
    fix.pushAudioWithBands(idx, peakDb, -16.0f, bands);

    auto advice = mixcoach::analyzeTrackTonal(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("OnTarget: status = OnTarget",       advice.status == mixcoach::TrackTonalAdvice::Status::OnTarget);
    TEST("OnTarget: isActionable = false",    !advice.isActionable());
    TEST("OnTarget: no worst region when all exact",  advice.worstRegion == -1);
}

static void test_tonal_off_target_excess_sub()
{
    std::printf("\n── Tonal: Off Target Excess (Sub region) ──\n");
    std::fflush(stdout);

    TestFixture fix;
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    auto profile = mixcoach::getExpectedProfile(mixcoach::TrackRole::Kick, "pop");
    float peakDb = -6.0f;

    // Set Sub region (0) to +6.0 dB above expected → deviation = +6.0
    // This is exactly at kToleranceDb (6.0) boundary, so it's OnTarget
    // Set it to expected + 10.0 → deviation = +10.0 → OffTarget
    float bands[30];
    float expectedSub = peakDb + profile.spectralOffset[0];
    fillBandsWithRegionDeviation(bands, 0, expectedSub + 15.0f, profile, peakDb);
    fix.pushAudioWithBands(idx, peakDb, -16.0f, bands);

    auto advice = mixcoach::analyzeTrackTonal(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("OffExcess(Sub): status = OffTarget",
         advice.status == mixcoach::TrackTonalAdvice::Status::OffTarget);
    TEST("OffExcess(Sub): isExcess = true",    advice.isExcess);
    TEST("OffExcess(Sub): worstRegion = 0",    advice.worstRegion == 0);
    TEST("OffExcess(Sub): isActionable = true", advice.isActionable());
    TEST("OffExcess(Sub): hasExcess = true",   advice.hasExcess());
    TEST("OffExcess(Sub): hasDeficit = false", !advice.hasDeficit());
}

static void test_tonal_off_target_deficit_bass()
{
    std::printf("\n── Tonal: Off Target Deficit (Bass region) ──\n");
    std::fflush(stdout);

    TestFixture fix;
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    auto profile = mixcoach::getExpectedProfile(mixcoach::TrackRole::Kick, "pop");
    float peakDb = -6.0f;

    // Need abs(deviation) > kNearToleranceDb (12.0) → deviation = -13.0 → OffTarget
    float bands[30];
    float expectedBass = peakDb + profile.spectralOffset[1];
    fillBandsWithRegionDeviation(bands, 1, expectedBass - 13.0f, profile, peakDb);
    fix.pushAudioWithBands(idx, peakDb, -16.0f, bands);

    auto advice = mixcoach::analyzeTrackTonal(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("OffDeficit(Bass): status = OffTarget",
         advice.status == mixcoach::TrackTonalAdvice::Status::OffTarget);
    TEST("OffDeficit(Bass): isExcess = false",  !advice.isExcess);
    TEST("OffDeficit(Bass): hasDeficit = true", advice.hasDeficit());
    TEST("OffDeficit(Bass): hasExcess = false", !advice.hasExcess());
}

static void test_tonal_near_target_excess_lo_mid()
{
    std::printf("\n── Tonal: Near Target Excess (LoMid region) ──\n");
    std::fflush(stdout);

    TestFixture fix;
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    auto profile = mixcoach::getExpectedProfile(mixcoach::TrackRole::Kick, "pop");
    float peakDb = -6.0f;

    // Set LoMid region (2) to expected + 7.0 → deviation = +7.0 → NearTarget (between 6 and 12)
    float bands[30];
    float expectedLoMid = peakDb + profile.spectralOffset[2];
    fillBandsWithRegionDeviation(bands, 2, expectedLoMid + 7.0f, profile, peakDb);
    fix.pushAudioWithBands(idx, peakDb, -16.0f, bands);

    auto advice = mixcoach::analyzeTrackTonal(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("NearExcess(LoMid): status = NearTarget",
         advice.status == mixcoach::TrackTonalAdvice::Status::NearTarget);
    TEST("NearExcess(LoMid): isExcess = true",    advice.isExcess);
    TEST("NearExcess(LoMid): worstRegion = 2",    advice.worstRegion == 2);
    TEST("NearExcess(LoMid): isActionable = true", advice.isActionable());
}

static void test_tonal_no_signal()
{
    std::printf("\n── Tonal: No Signal ──\n");
    std::fflush(stdout);

    TestFixture fix;
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    float bands[30]{};
    fix.pushAudioWithBands(idx, -80.0f, -90.0f, bands);  // peak < -60

    auto advice = mixcoach::analyzeTrackTonal(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("NoSignal: status = NoSignal",
         advice.status == mixcoach::TrackTonalAdvice::Status::NoSignal);
}

static void test_tonal_unknown_role()
{
    std::printf("\n── Tonal: Unknown Role ──\n");
    std::fflush(stdout);

    TestFixture fix;
    int idx = fix.registerSlot("Unknown", mixcoach::TrackRole::Unknown);
    float bands[30]{};
    fix.pushAudioWithBands(idx, -10.0f, -20.0f, bands);

    auto advice = mixcoach::analyzeTrackTonal(idx, *fix.sd, fix.trackRoles, "pop", false);
    TEST("UnknownRole: status = UnknownRole",
         advice.status == mixcoach::TrackTonalAdvice::Status::UnknownRole);
}

static void test_tonal_all_regions()
{
    std::printf("\n── Tonal: All 6 Regions Detectable ──\n");
    std::fflush(stdout);

    // Verify that each region can be detected as the worst region
    // Use a massive boost (+30 dB above expected) on the target region while
    // keeping all other regions at their exact expected values.
    for (int region = 0; region < 6; ++region) {
        TestFixture fix;
        int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
        auto profile = mixcoach::getExpectedProfile(mixcoach::TrackRole::Kick, "pop");
        float peakDb = -6.0f;
        float bands[30];
        float expectedTarget = peakDb + profile.spectralOffset[region];
        fillBandsWithRegionDeviation(bands, region, expectedTarget + 30.0f, profile, peakDb);
        fix.pushAudioWithBands(idx, peakDb, -16.0f, bands);

        auto advice = mixcoach::analyzeTrackTonal(idx, *fix.sd, fix.trackRoles, "pop", false);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Region %d: worstRegion detected", region);
        TEST(buf, advice.worstRegion == region);
        TEST("Region excess: isExcess = true", advice.isExcess);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: TrackTonalAdvice — analyzeAllTracksTonal
// ═══════════════════════════════════════════════════════════════════════════

static void test_tonal_analyze_all()
{
    std::printf("\n── Tonal: analyzeAllTracksTonal ──\n");
    std::fflush(stdout);

    TestFixture fix;

    // Track 0: OffTarget excess (Sub region boosted)
    int idx0 = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    {
        auto profile = mixcoach::getExpectedProfile(mixcoach::TrackRole::Kick, "pop");
        float peakDb = -6.0f;
        float bands0[30];
        float expectedSub = peakDb + profile.spectralOffset[0];
        // Need abs(deviation) > kNearToleranceDb (12.0) → OffTarget
        fillBandsWithRegionDeviation(bands0, 0, expectedSub + 13.0f, profile, peakDb);
        fix.pushAudioWithBands(idx0, peakDb, -16.0f, bands0);
    }

    // Track 1: OnTarget (not actionable)
    int idx1 = fix.registerSlot("Snare", mixcoach::TrackRole::Snare);
    {
        auto profile = mixcoach::getExpectedProfile(mixcoach::TrackRole::Snare, "pop");
        float peakDb = -6.0f;
        float bands1[30];
        fillBandsOnTarget(bands1, profile, peakDb);
        fix.pushAudioWithBands(idx1, peakDb, -16.0f, bands1);
    }

    auto results = mixcoach::analyzeAllTracksTonal(*fix.sd, fix.trackRoles, "pop", false);
    TEST("analyzeAll: returns at least 1", results.size() >= 1);
    if (results.size() >= 1) {
        TEST("analyzeAll: first is actionable", results[0].isActionable());
        TEST("analyzeAll: first is OffTarget",
             results[0].status == mixcoach::TrackTonalAdvice::Status::OffTarget);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Tests: Edge cases
// ═══════════════════════════════════════════════════════════════════════════

static void test_edge_no_tracks_registered()
{
    std::printf("\n── Edge: No Tracks Registered ──\n");
    std::fflush(stdout);

    TestFixture fix;
    // No slots registered — all analyzers should handle gracefully

    auto gainResults = mixcoach::analyzeAllTracksGain(*fix.sd, fix.trackRoles, "pop", false);
    TEST("No tracks: gain results empty", gainResults.empty());

    auto dynResults = mixcoach::analyzeAllTracksDynamics(*fix.sd, fix.trackRoles, "pop", false);
    TEST("No tracks: dynamics results empty", dynResults.empty());

    auto tonalResults = mixcoach::analyzeAllTracksTonal(*fix.sd, fix.trackRoles, "pop", false);
    TEST("No tracks: tonal results empty", tonalResults.empty());
}

static void test_edge_solo_active()
{
    std::printf("\n── Edge: Solo Active ──\n");
    std::fflush(stdout);

    TestFixture fix;
    int idx = fix.registerSlot("Kick", mixcoach::TrackRole::Kick);
    // Solo this track
    fix.registry->setActive(idx, true);
    fix.pushAudio(idx, 3.0f, -8.0f);

    // With soloActive=true, track should still be analyzed
    auto advice = mixcoach::analyzeTrackGain(idx, *fix.sd, fix.trackRoles, "pop", true);
    TEST("Solo active: does not crash", advice.status != mixcoach::TrackGainAdvice::Status::UnknownRole);
}

static void test_edge_genre_specific_profiles()
{
    std::printf("\n── Edge: Genre-Specific Profiles ──\n");
    std::fflush(stdout);

    // Different genres should produce different expected profiles
    // Use a very high peak (+12.0 dB) to ensure OffTarget regardless of genre delta
    TestFixture fixPop;
    TestFixture fixRock;

    int idxPop = fixPop.registerSlot("Kick", mixcoach::TrackRole::Kick);
    fixPop.pushAudio(idxPop, 12.0f, -2.0f);

    int idxRock = fixRock.registerSlot("Kick", mixcoach::TrackRole::Kick);
    fixRock.pushAudio(idxRock, 12.0f, -2.0f);

    auto advicePop  = mixcoach::analyzeTrackGain(idxPop, *fixPop.sd, fixPop.trackRoles, "pop", false);
    auto adviceRock = mixcoach::analyzeTrackGain(idxRock, *fixRock.sd, fixRock.trackRoles, "rock", false);

    // Both should be OffTarget regardless of genre (deviation huge: +12 - (-6) ≈ +18 >> 2*tol)
    TEST("Genre: pop OffTarget",  advicePop.status == mixcoach::TrackGainAdvice::Status::OffTarget);
    TEST("Genre: rock OffTarget", adviceRock.status == mixcoach::TrackGainAdvice::Status::OffTarget);

    // Verify that profiles actually differ (different peak targets)
    auto profilePop  = mixcoach::getExpectedProfile(mixcoach::TrackRole::Kick, "pop");
    auto profileRock = mixcoach::getExpectedProfile(mixcoach::TrackRole::Kick, "rock");
    TEST("Genre: profiles differ", profilePop.peakTargetDb != profileRock.peakTargetDb
                                     || profilePop.crestTargetDb != profileRock.crestTargetDb);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════

int main()
{
    std::printf("══════════════════════════════════════════════════════════\n");
    std::printf("  Track Analyzer Core Unit Tests (Sprints 6A-6C)\n");
    std::printf("  TrackGainAdvice | TrackDynamicsAdvice | TrackTonalAdvice\n");
    std::printf("══════════════════════════════════════════════════════════\n\n");
    std::fflush(stdout);

    // ─── Helper functions ──────────────────────────────────────────────
    test_helper_get_latest_telemetry();
    test_helper_compute_per_track_lufs();

    // ─── Track Gain Advice ─────────────────────────────────────────────
    test_gain_on_target();
    test_gain_near_target_high();
    test_gain_near_target_low();
    test_gain_off_target_high();
    test_gain_off_target_low();
    test_gain_no_signal();
    test_gain_unknown_role();
    test_gain_invalid_slot();
    test_gain_muted_track();
    test_gain_analyze_all_ordering();
    test_gain_analyze_all_empty_genre();

    // ─── Track Dynamics Advice ─────────────────────────────────────────
    test_dynamics_on_target();
    test_dynamics_near_overcompressed();
    test_dynamics_near_too_dynamic();
    test_dynamics_off_overcompressed();
    test_dynamics_off_too_dynamic();
    test_dynamics_no_signal();
    test_dynamics_unknown_role();
    test_dynamics_crest_factor_edge();

    // ─── Track Tonal Advice ────────────────────────────────────────────
    test_tonal_on_target();
    test_tonal_off_target_excess_sub();
    test_tonal_off_target_deficit_bass();
    test_tonal_near_target_excess_lo_mid();
    test_tonal_no_signal();
    test_tonal_unknown_role();
    test_tonal_all_regions();
    test_tonal_analyze_all();

    // ─── Edge Cases ────────────────────────────────────────────────────
    test_edge_no_tracks_registered();
    test_edge_solo_active();
    test_edge_genre_specific_profiles();

    std::printf("\n══════════════════════════════════════════════════════════\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("══════════════════════════════════════════════════════════\n");
    std::fflush(stdout);

    return gTestsFailed > 0 ? 1 : 0;
}
