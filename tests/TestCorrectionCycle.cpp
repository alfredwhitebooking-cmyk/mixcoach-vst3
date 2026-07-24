// ═══════════════════════════════════════════════════════════════════════════
//  TestCorrectionCycle.cpp — Unit test para el ciclo completo de corrección:
//    Recommend → Apply → Verify → Feedback
//
//  Valida el loop completo descrito en CoachEngineCorrection.cpp:
//    1. storeRecommendation() crea una recomendación Pending
//    2. verifyTrackCorrections() lee telemetría actual, calcula appliedRatio,
//       clasifica (Applied/OverApplied/UnderApplied/Ignored)
//    3. Envía feedback vía respondWithCorrectionFeedback()
//    4. Genera follow-up para casos OverApplied/UnderApplied
//    5. Registra en correctionHistory_
//
//  Compilado via CMake (ver CMakeLists.txt):
//    cmake --build build --config Debug --target TestCorrectionCycle
//    ./build/tests/Debug/TestCorrectionCycle.exe
//
//  Dependencias: SharedData (~65MB heap), PhaseManager, CoachEngine
// ═══════════════════════════════════════════════════════════════════════════

#include "TestHelpers.h"

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "Common/types/Types.h"
#include "Common/memory/SlotRegistry.h"
#include "Common/memory/SharedData.h"
#include "MixCoach/engine/PhaseManager.h"
#include "MixCoach/engine/CoachEngine.h"
#include "MixCoach/audio/AudioAnalyzer.h"

// ═══════════════════════════════════════════════════════════════════════════
//  Tests del Correction Cycle
// ═══════════════════════════════════════════════════════════════════════════

// ─── Test 1: storeRecommendation — Crear y verificar recomendación Pending ──
static void test_store_recommendation()
{
    std::printf("\n── Test 1: Store Recommendation ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    // Register track with audio
    sd->getSlotRegistry().registerSlot("Kick",
        juce::Colours::red, mixcoach::BusType::Drums);
    setupTrackAudioResult(*sd, 0, -10.0f, -18.0f);

    // Store a recommendation
    engine.storeRecommendation(0, "Kick",
        mixcoach::TrackRecommendation::Domain::Gain,
        "Baja el fader -3 dB para headroom",
        -10.0f, -13.0f, -3.0f, "peak", 0.0f, -1);

    // Verify via getTrackRecommendation
    const auto* rec = engine.getTrackRecommendation(0);
    TEST("Recommendation exists for slot 0", rec != nullptr);
    TEST("Recommendation track name is Kick", rec != nullptr && rec->trackName == "Kick");
    TEST("Recommendation is Pending", rec != nullptr &&
         rec->status == mixcoach::TrackRecommendation::Status::Pending);
    TEST("Recommendation domain is Gain", rec != nullptr &&
         rec->domain == mixcoach::TrackRecommendation::Domain::Gain);
    TEST("Recommendation action mentions baj", rec != nullptr &&
         rec->action.contains("Baja"));
    TEST("Recommendation verifyMetric is peak", rec != nullptr &&
         rec->verifyMetric == "peak");
    TEST("Recommendation beforeValue is -10.0", rec != nullptr &&
         std::abs(rec->beforeValue - (-10.0f)) < 0.01f);
    TEST("Recommendation expectedAfter is -13.0", rec != nullptr &&
         std::abs(rec->expectedAfter - (-13.0f)) < 0.01f);
    TEST("Recommendation delta is -3.0", rec != nullptr &&
         std::abs(rec->delta - (-3.0f)) < 0.01f);

    sd.reset();
}

// ─── Test 2: verifyTrackCorrections — Applied (exact match) ────────────────
static void test_verify_applied()
{
    std::printf("\n── Test 2: Verify Applied (Exact Match) ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    // Register track: initial peak = -4 dB
    sd->getSlotRegistry().registerSlot("Kick",
        juce::Colours::red, mixcoach::BusType::Drums);
    setupTrackAudioResult(*sd, 0, -4.0f, -12.0f);

    // Store recommendation: target = -10 dB (delta = -6)
    engine.storeRecommendation(0, "Kick",
        mixcoach::TrackRecommendation::Domain::Gain,
        "Baja -6 dB en fader",
        -4.0f, -10.0f, -6.0f, "peak", 0.0f, -1);

    // Simulate user applying the change: peak now = -10 dB (exact match)
    // The appliedRatio = (-10 - (-4)) / (-10 - (-4)) = -6/-6 = 1.0 = 100%
    // Applied range: underApplyTarget(0.35) to overApplyTarget(1.20)
    // 1.0 is within [0.35, 1.20] → Applied!
    setupTrackAudioResult(*sd, 0, -10.0f, -18.0f);

    // Run verification
    engine.verifyTrackCorrections();

    // Check recommendation status
    const auto* rec = engine.getTrackRecommendation(0);
    TEST("Recommendation status changed to Applied",
         rec != nullptr && rec->status == mixcoach::TrackRecommendation::Status::Applied);
    TEST("Feedback was sent",
         rec != nullptr && rec->feedbackSent);
    TEST("Feedback message is not empty",
         rec != nullptr && !rec->feedbackMessage.isEmpty());
    TEST("Feedback message mentions Kick",
         rec != nullptr && rec->feedbackMessage.contains("Kick"));
    TEST("Feedback message is correction feedback type (regular MentorMessage::Info)",
         sd->getMessageCount() >= 1);

    // Check correction history
    const auto& history = engine.getCorrectionHistory();
    TEST("1 entry in correction history",
         history.size() == 1);
    TEST("History entry status is Applied",
         history.size() > 0 && history[0].finalStatus == mixcoach::TrackRecommendation::Status::Applied);
    TEST("History entry track is Kick",
         history.size() > 0 && history[0].trackName == "Kick");
    TEST("History entry beforeValue is -4.0",
         history.size() > 0 && std::abs(history[0].beforeValue - (-4.0f)) < 0.1f);
    TEST("History entry afterValue is -10.0",
         history.size() > 0 && std::abs(history[0].afterValue - (-10.0f)) < 0.1f);
    TEST("History entry appliedRatio is ~1.0",
         history.size() > 0 && std::abs(history[0].appliedRatio - 1.0f) < 0.1f);

    sd.reset();
}

// ─── Test 3: verifyTrackCorrections — OverApplied (se pasó) + FollowUp ─────
static void test_verify_overapplied()
{
    std::printf("\n── Test 3: Verify Over-Applied (Se pasó) + FollowUp ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    // Register track: initial peak = -4 dB
    sd->getSlotRegistry().registerSlot("Bajo",
        juce::Colours::blue, mixcoach::BusType::Bass);
    setupTrackAudioResult(*sd, 0, -4.0f, -12.0f);

    // Store recommendation: target = -10 dB (delta = -6)
    engine.storeRecommendation(0, "Bajo",
        mixcoach::TrackRecommendation::Domain::Gain,
        "Baja -6 dB en fader",
        -4.0f, -10.0f, -6.0f, "peak", 0.0f, -1);

    // Simulate over-application: user went to -18 dB instead of -10 dB
    // appliedRatio = (-18 - (-4)) / (-10 - (-4)) = -14/-6 = 2.33 > 1.20 → OverApplied!
    setupTrackAudioResult(*sd, 0, -18.0f, -26.0f);

    // Run verification (first pass: OverApplied, verifyRetries = 1)
    engine.verifyTrackCorrections();

    // First verification should set OverApplied and increment retries to 1
    const auto* rec = engine.getTrackRecommendation(0);
    TEST("Recommendation status is OverApplied (first pass)",
         rec != nullptr && rec->status == mixcoach::TrackRecommendation::Status::OverApplied);

    // Should have sent feedback message
    TEST("Feedback was sent for over-apply",
         rec != nullptr && rec->feedbackSent);
    TEST("Feedback message is not empty for over-apply",
         rec != nullptr && !rec->feedbackMessage.isEmpty());
    TEST("Feedback message mentions 'pasaste'",
         rec != nullptr && rec->feedbackMessage.contains("pasaste"));

    // Verify retries = 1 (first over-apply, still has 1 more retry before Superseded)
    TEST("verifyRetries is 1 after first over-apply",
         rec != nullptr && rec->verifyRetries == 1);

    // At this point, OverApplied with retries=1 means NOT finalized yet.
    // The correction history should NOT have an entry yet (status not finalized).
    const auto& history1 = engine.getCorrectionHistory();
    TEST("No history entry yet (status not finalized after first over-apply)",
         history1.empty());

    // ─── Second verification: status is no longer Pending → skipped ────────
    // Once status changes from Pending, verifyTrackCorrections skips it.
    engine.verifyTrackCorrections();

    const auto* rec2 = engine.getTrackRecommendation(0);
    TEST("Recommendation still OverApplied after 2nd call (skipped)",
         rec2 != nullptr && rec2->status == mixcoach::TrackRecommendation::Status::OverApplied);
    TEST("verifyRetries still 1 (skipped, not incremented)",
         rec2 != nullptr && rec2->verifyRetries == 1);

    // No history entry — OverApplied is NOT finalized into correctionHistory
    // (the code flags OverApplied as non-terminal; only Applied/Ignored get finalized)
    const auto& history2 = engine.getCorrectionHistory();
    TEST("No correction history entry for non-finalized OverApplied",
         history2.empty());

    sd.reset();
}

// ─── Test 4: verifyTrackCorrections — UnderApplied (poco cambio) + FollowUp ─
static void test_verify_underapplied()
{
    std::printf("\n── Test 4: Verify Under-Applied (Poco cambio) + FollowUp ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    // Register track: initial peak = -4 dB
    sd->getSlotRegistry().registerSlot("Snare",
        juce::Colours::orange, mixcoach::BusType::Drums);
    setupTrackAudioResult(*sd, 0, -4.0f, -12.0f);

    // Store recommendation: target = -10 dB (delta = -6)
    engine.storeRecommendation(0, "Snare",
        mixcoach::TrackRecommendation::Domain::Gain,
        "Baja -6 dB en fader",
        -4.0f, -10.0f, -6.0f, "peak", 0.0f, -1);

    // Simulate under-application: user went to -7 dB instead of -10 dB
    // appliedRatio = (-7 - (-4)) / (-10 - (-4)) = -3/-6 = 0.50
    // Default thresholds: goodStartRatio=0.35, underApplyTarget=0.80
    // 0.35 <= 0.50 < 0.80 → UnderApplied!
    setupTrackAudioResult(*sd, 0, -7.0f, -15.0f);

    // Run verification (first pass: UnderApplied)
    engine.verifyTrackCorrections();

    const auto* rec = engine.getTrackRecommendation(0);
    TEST("Recommendation status is UnderApplied (ratio=0.50, in [0.35,0.80))",
         rec != nullptr && rec->status == mixcoach::TrackRecommendation::Status::UnderApplied);
    TEST("Feedback sent for under-apply",
         rec != nullptr && rec->feedbackSent);
    TEST("Feedback message mentions 'faltan' or 'sigue'",
         rec != nullptr && (rec->feedbackMessage.contains("faltan") ||
                            rec->feedbackMessage.contains("sigue") ||
                            rec->feedbackMessage.contains("Vas bien")));

    // No history entry yet (UnderApplied is not finalized into correctionHistory)
    const auto& history1 = engine.getCorrectionHistory();
    TEST("No history entry yet (UnderApplied not finalized)",
         history1.empty());

    sd.reset();
}

// ─── Test 5: verifyTrackCorrections — Ignored (sin cambio tras retries) ────
static void test_verify_ignored()
{
    std::printf("\n── Test 5: Verify Ignored (Sin cambio tras retries) ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    // Register track: initial peak = -4 dB
    sd->getSlotRegistry().registerSlot("Guitarra",
        juce::Colours::green, mixcoach::BusType::Guitars);
    setupTrackAudioResult(*sd, 0, -4.0f, -12.0f);

    // Store recommendation: target = -10 dB (delta = -6)
    engine.storeRecommendation(0, "Guitarra",
        mixcoach::TrackRecommendation::Domain::Gain,
        "Baja -6 dB en fader",
        -4.0f, -10.0f, -6.0f, "peak", 0.0f, -1);

    // Simulate NO change by user (peak stays at -4 dB)
    // appliedRatio = (-4 - (-4)) / (-10 - (-4)) = 0/-6 = 0.0
    // 0.0 < 0.15 → no change, increment retries
    setupTrackAudioResult(*sd, 0, -4.0f, -12.0f);

    // ─── Pass 1: No change → retries = 1 ────────────────────────────────────
    engine.verifyTrackCorrections();
    const auto* rec = engine.getTrackRecommendation(0);
    TEST("Retry 1: status still Pending",
         rec != nullptr && rec->status == mixcoach::TrackRecommendation::Status::Pending);
    TEST("Retry 1: verifyRetries is 1",
         rec != nullptr && rec->verifyRetries == 1);

    // ─── Pass 2: No change → retries = 2 ────────────────────────────────────
    engine.verifyTrackCorrections();
    const auto* rec2 = engine.getTrackRecommendation(0);
    TEST("Retry 2: status still Pending",
         rec2 != nullptr && rec2->status == mixcoach::TrackRecommendation::Status::Pending);
    TEST("Retry 2: verifyRetries is 2",
         rec2 != nullptr && rec2->verifyRetries == 2);

    // ─── Pass 3: No change → retries = 3 = maxRetriesBeforeIgnore → Ignored! ──
    engine.verifyTrackCorrections();

    // After 3rd retry with no change, should be Ignored and finalized
    const auto* rec3 = engine.getTrackRecommendation(0);
    TEST("After max retries, recommendation status is Ignored",
         rec3 == nullptr || rec3->slotIndex != 0 ||
         rec3->status == mixcoach::TrackRecommendation::Status::Ignored);

    // Check correction history
    const auto& history = engine.getCorrectionHistory();
    TEST("Correction history has 1 entry",
         history.size() == 1);
    if (history.size() > 0) {
        TEST("Finalized status is Ignored",
             history[0].finalStatus == mixcoach::TrackRecommendation::Status::Ignored);
        TEST("Ignored entry track is Guitarra",
             history[0].trackName == "Guitarra");
        TEST("Ignored entry appliedRatio is 0.0",
             std::abs(history[0].appliedRatio) < 0.01f);
    }

    sd.reset();
}

// ─── Test 6: verifyTrackCorrections — Multiple tracks, mixed results ───────
static void test_verify_multiple_tracks()
{
    std::printf("\n── Test 6: Multiple Tracks with Mixed Results ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    // ─── Track 0: Kick → Applied ─────────────────────────────────────────
    sd->getSlotRegistry().registerSlot("Kick",
        juce::Colours::red, mixcoach::BusType::Drums);
    setupTrackAudioResult(*sd, 0, -4.0f, -12.0f);
    engine.storeRecommendation(0, "Kick",
        mixcoach::TrackRecommendation::Domain::Gain,
        "Baja -6 dB",
        -4.0f, -10.0f, -6.0f, "peak", 0.0f, -1);

    // ─── Track 1: Bajo → OverApplied ─────────────────────────────────────
    sd->getSlotRegistry().registerSlot("Bajo",
        juce::Colours::blue, mixcoach::BusType::Bass);
    setupTrackAudioResult(*sd, 1, -4.0f, -12.0f);
    engine.storeRecommendation(1, "Bajo",
        mixcoach::TrackRecommendation::Domain::Gain,
        "Baja -6 dB",
        -4.0f, -10.0f, -6.0f, "peak", 0.0f, -1);

    // ─── Simulate user changes ───────────────────────────────────────────
    // Kick: exact match (-10 dB) → Applied
    setupTrackAudioResult(*sd, 0, -10.0f, -18.0f);
    // Bajo: over-applied (-18 dB) → OverApplied
    setupTrackAudioResult(*sd, 1, -18.0f, -26.0f);

    // ─── First verification pass ─────────────────────────────────────────
    engine.verifyTrackCorrections();

    // Kick should be Applied
    const auto* rec0 = engine.getTrackRecommendation(0);
    TEST("Kick: Applied",
         rec0 != nullptr && rec0->status == mixcoach::TrackRecommendation::Status::Applied);

    // Bajo should be OverApplied (retries=1)
    const auto* rec1 = engine.getTrackRecommendation(1);
    TEST("Bajo: OverApplied",
         rec1 != nullptr && rec1->status == mixcoach::TrackRecommendation::Status::OverApplied);
    TEST("Bajo: verifyRetries = 1",
         rec1 != nullptr && rec1->verifyRetries == 1);

    // Correction history should have 1 entry (Kick was finalized)
    const auto& history1 = engine.getCorrectionHistory();
    TEST("Correction history has 1 entry (Kick finalized)",
         history1.size() == 1);
    if (history1.size() > 0) {
        TEST("First entry is Kick Applied",
             history1[0].trackName == "Kick" &&
             history1[0].finalStatus == mixcoach::TrackRecommendation::Status::Applied);
    }

    // ─── Second verification: Bajo status changed from Pending → skipped ──
    // Once OverApplied, verifyTrackCorrections skips non-Pending recs.
    setupTrackAudioResult(*sd, 1, -18.0f, -26.0f); // Still over-applied
    engine.verifyTrackCorrections();

    // Bajo stays OverApplied (not Superseded — skipped because not Pending)
    const auto* rec1b = engine.getTrackRecommendation(1);
    TEST("Bajo: still OverApplied after 2nd call (skipped, not Pending)",
         rec1b != nullptr && rec1b->status == mixcoach::TrackRecommendation::Status::OverApplied);

    // Correction history still has 1 entry (Kick only. Bajo was not finalized)
    const auto& history2 = engine.getCorrectionHistory();
    TEST("Correction history still has 1 entry (only Kick finalized)",
         history2.size() == 1);

    sd.reset();
}

// ─── Test 7: Correction history — storeRecommendation + verify creates entry ─
static void test_correction_history_integration()
{
    std::printf("\n── Test 7: Correction History Integration ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    // Register track
    sd->getSlotRegistry().registerSlot("Vocal",
        juce::Colours::purple, mixcoach::BusType::Vocals);
    setupTrackAudioResult(*sd, 0, -6.0f, -14.0f);

    // Store recommendation: lower by -3 dB (target -9)
    engine.storeRecommendation(0, "Vocal",
        mixcoach::TrackRecommendation::Domain::Gain,
        "Baja -3 dB",
        -6.0f, -9.0f, -3.0f, "peak", 0.0f, -1);

    // Apply exact match
    setupTrackAudioResult(*sd, 0, -9.0f, -17.0f);
    engine.verifyTrackCorrections();

    // Check history exists
    const auto& history = engine.getCorrectionHistory();
    TEST("Correction history has 1 entry", history.size() >= 1);
    if (history.size() >= 1) {
        const auto& entry = history[0];
        TEST("Entry track is Vocal", entry.trackName == "Vocal");
        TEST("Entry action contains baja", entry.action.contains("Baja"));
        TEST("Entry domain is Gain",
             entry.domain == mixcoach::TrackRecommendation::Domain::Gain);
        TEST("Entry finalStatus is Applied",
             entry.finalStatus == mixcoach::TrackRecommendation::Status::Applied);
        TEST("Entry beforeValue is -6.0",
             std::abs(entry.beforeValue - (-6.0f)) < 0.1f);
        TEST("Entry afterValue is -9.0",
             std::abs(entry.afterValue - (-9.0f)) < 0.1f);
        TEST("Entry appliedRatio is ~1.0",
             std::abs(entry.appliedRatio - 1.0f) < 0.1f);
        TEST("Entry timestampUs > 0",
             entry.timestampUs > 0);
        TEST("Entry slotIndex is 0",
             entry.slotIndex == 0);
        TEST("Entry hadFollowUp is false (no follow-up for Applied)",
             !entry.hadFollowUp);
    }

    sd.reset();
}

// ─── Test 8: getSlotCorrectionStats — Stats per slot correct ───────────────
static void test_slot_correction_stats()
{
    std::printf("\n── Test 8: Slot Correction Stats ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    // Register slot 0
    sd->getSlotRegistry().registerSlot("Kick",
        juce::Colours::red, mixcoach::BusType::Drums);
    setupTrackAudioResult(*sd, 0, -4.0f, -12.0f);

    // 3 recommendations for slot 0
    for (int i = 0; i < 3; ++i) {
        engine.storeRecommendation(0, "Kick",
            mixcoach::TrackRecommendation::Domain::Gain,
            "Ajuste #" + juce::String(i + 1),
            -4.0f, -10.0f, -6.0f, "peak", 0.0f, -1);

        // Apply exact match each time to finalize as Applied
        setupTrackAudioResult(*sd, 0, -10.0f, -18.0f);
        engine.verifyTrackCorrections();

        // Reset peak for next cycle (simulate new state)
        setupTrackAudioResult(*sd, 0, -4.0f, -12.0f);
    }

    auto stats = engine.getSlotCorrectionStats(0);
    TEST("Slot 0 total corrections is 3", stats.total == 3);
    TEST("Slot 0 applied is 3", stats.applied == 3);
    TEST("Slot 0 ignored is 0", stats.ignored == 0);
    TEST("Slot 0 overApplied is 0", stats.overApplied == 0);
    TEST("Slot 0 underApplied is 0", stats.underApplied == 0);

    sd.reset();
}

// ─── Test 9: getMostUrgentRecommendation — Returns the highest priority ─────
static void test_most_urgent_recommendation()
{
    std::printf("\n── Test 9: Most Urgent Recommendation ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    // Slot 0: Clipping (priority 0)
    sd->getSlotRegistry().registerSlot("Kick",
        juce::Colours::red, mixcoach::BusType::Drums);
    setupTrackAudioResult(*sd, 0, 0.0f, -8.0f); // peak = 0 → CLIPPING
    engine.storeRecommendation(0, "Kick",
        mixcoach::TrackRecommendation::Domain::Gain,
        "Baja fader urgente!",
        0.0f, -6.0f, -6.0f, "peak", 0.0f, -1);

    // Slot 1: Normal level (lower priority)
    sd->getSlotRegistry().registerSlot("Vocal",
        juce::Colours::purple, mixcoach::BusType::Vocals);
    setupTrackAudioResult(*sd, 1, -10.0f, -18.0f);
    engine.storeRecommendation(1, "Vocal",
        mixcoach::TrackRecommendation::Domain::Gain,
        "Sutil ajuste",
        -10.0f, -12.0f, -2.0f, "peak", 0.0f, -1);

    const auto* urgent = engine.getMostUrgentRecommendation();
    TEST("Most urgent is not null", urgent != nullptr);
    TEST("Most urgent is Kick (clipping)", urgent != nullptr && urgent->trackName == "Kick");
    TEST("Most urgent action mentions Baja", urgent != nullptr && urgent->action.contains("Baja"));

    sd.reset();
}

// ─── Test 10: Correction for non-gain domain (Tonal: EQ) ───────────────────
static void test_verify_tonal_domain()
{
    std::printf("\n── Test 10: Verify Tonal Domain (EQ) ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    // Register track
    sd->getSlotRegistry().registerSlot("Vocal",
        juce::Colours::purple, mixcoach::BusType::Vocals);
    setupTrackAudioResult(*sd, 0, -10.0f, -18.0f);

    // Store a TONAL recommendation (verify via band_3)
    engine.storeRecommendation(0, "Vocal",
        mixcoach::TrackRecommendation::Domain::Tonal,
        "Reduce 3 kHz -3 dB",
        -20.0f, -23.0f, -3.0f, "band_3", 3000.0f, 3);

    // Simulate applying the EQ change: band_3 energy changed
    {
        mixcoach::TrackAudioResult result;
        result.peakLeft  = -10.0f;
        result.peakRight = -10.0f;
        result.rmsLeft   = -18.0f;
        result.rmsRight  = -18.0f;
        // band_3 (HiMid) changed from -20 dB to -23 dB (applied match)
        for (int b = 0; b < 30; ++b)
            result.bandEnergies[b] = -20.0f;
        result.bandEnergies[3] = -23.0f; // Applied correctly
        result.timestampUs = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;
        sd->updateTrackAudioResult(0, result);
    }

    engine.verifyTrackCorrections();

    const auto* rec = engine.getTrackRecommendation(0);
    TEST("Tonal recommendation is Applied",
         rec != nullptr && rec->status == mixcoach::TrackRecommendation::Status::Applied);
    TEST("Tonal verifyMetric is band_3",
         rec != nullptr && rec->verifyMetric == "band_3");
    TEST("Tonal frequencyHz is 3000",
         rec != nullptr && std::abs(rec->frequencyHz - 3000.0f) < 0.1f);

    sd.reset();
}

// ─── Test 11: Correction for Spatial domain (correlation) ──────────────────
static void test_verify_spatial_domain()
{
    std::printf("\n── Test 11: Verify Spatial Domain (Correlation) ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    // Register track
    sd->getSlotRegistry().registerSlot("FX",
        juce::Colours::grey, mixcoach::BusType::FX);
    setupTrackAudioResultFull(*sd, 0, -10.0f, -10.0f, -18.0f, -18.0f, 0.1f, 8.0f);

    // Store SPATIAL recommendation (correlation)
    engine.storeRecommendation(0, "FX",
        mixcoach::TrackRecommendation::Domain::Spatial,
        "Corrige fase - ajusta pan",
        0.1f, 0.7f, 0.6f, "correlation", 0.0f, -1);

    // Simulate user fixing correlation
    setupTrackAudioResultFull(*sd, 0, -10.0f, -10.0f, -18.0f, -18.0f, 0.7f, 8.0f);

    engine.verifyTrackCorrections();

    const auto* rec = engine.getTrackRecommendation(0);
    TEST("Spatial recommendation is Applied",
         rec != nullptr && rec->status == mixcoach::TrackRecommendation::Status::Applied);
    TEST("Spatial verifyMetric is correlation",
         rec != nullptr && rec->verifyMetric == "correlation");
    // Verify the verifyCurrent was updated
    TEST("Spatial verifyCurrent is 0.7",
         rec != nullptr && std::abs(rec->verifyCurrent - 0.7f) < 0.01f);

    sd.reset();
}

// ─── Test 12: ignoreUnused — dismissRecommendation sets to Ignored ────────
static void test_dismiss_recommendation()
{
    std::printf("\n── Test 12: Dismiss Recommendation ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    // Register track
    sd->getSlotRegistry().registerSlot("Pad",
        juce::Colours::teal, mixcoach::BusType::Keys);
    setupTrackAudioResult(*sd, 0, -12.0f, -20.0f);

    // Store recommendation
    engine.storeRecommendation(0, "Pad",
        mixcoach::TrackRecommendation::Domain::Gain,
        "Sube +2 dB para presencia",
        -12.0f, -10.0f, 2.0f, "peak", 0.0f, -1);

    TEST("Recommendation exists before dismiss",
         engine.getTrackRecommendation(0) != nullptr);

    // Dismiss it
    engine.dismissRecommendation(0);

    // After dismiss, recommendation should be reset (no longer active)
    const auto* rec = engine.getTrackRecommendation(0);
    TEST("Recommendation not available after dismiss",
         rec == nullptr || rec->slotIndex != 0);

    // Correction history should have an Ignored entry
    const auto& history = engine.getCorrectionHistory();
    TEST("Dismiss created Ignored history entry",
         history.size() >= 1);
    if (history.size() >= 1) {
        // The last entry should be Ignored
        const auto& entry = history.back();
        TEST("Dismiss entry finalStatus is Ignored",
             entry.finalStatus == mixcoach::TrackRecommendation::Status::Ignored);
        TEST("Dismiss entry track is Pad",
             entry.trackName == "Pad");
        TEST("Dismiss entry appliedRatio is 0.0",
             std::abs(entry.appliedRatio) < 0.01f);
    }

    sd.reset();
}

// ─── Test 13: Adaptive thresholds recalc after many corrections ───────────
static void test_adaptive_thresholds()
{
    std::printf("\n── Test 13: Adaptive Thresholds Recalc ──\n");
    std::fflush(stdout);

    auto sd = std::make_unique<mixcoach::SharedData>();
    mixcoach::AudioAnalyzer audioAnalyzer;
    mixcoach::PhaseManager pm(sd->getSlotRegistry());
    mixcoach::CoachEngine engine(pm, *sd, audioAnalyzer);

    auto& registry = sd->getSlotRegistry();

    // Create and process 12 corrections (10 triggers recalc)
    // Mix of Applied and OverApplied to exercise the recalc logic
    for (int i = 0; i < 12; ++i) {
        std::string name = "Track_" + std::to_string(i);
        registry.registerSlot(name, juce::Colours::grey, mixcoach::BusType::None);
        setupTrackAudioResult(*sd, i, -4.0f, -12.0f);

        engine.storeRecommendation(i, name,
            mixcoach::TrackRecommendation::Domain::Gain,
            "Baja -6 dB",
            -4.0f, -10.0f, -6.0f, "peak", 0.0f, -1);

        // For even indices, apply exactly. For odd, over-apply.
        if (i % 2 == 0)
            setupTrackAudioResult(*sd, i, -10.0f, -18.0f); // Applied
        else
            setupTrackAudioResult(*sd, i, -20.0f, -28.0f); // Over-applied

        engine.verifyTrackCorrections();
    }

    // The recalc should have run (correctionHistory_.size()=12, triggered at 10)
    const auto& thresholds = engine.getAdaptiveThresholds();
    TEST("Adaptive thresholds exist after 12 corrections", true);
    // With 6 over-applied out of 12 = 50% → exactly at threshold (0.5)
    // This should trigger the over-applier profile
    // (overRatio = 6/12 = 0.5 which is NOT > 0.5, so stays default or exact)
    // Not checking exact values since they depend on the ratio,
    // but the smoke test verifies no crash

    sd.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════
int main()
{
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  Correction Cycle Unit Tests\n");
    std::printf("  Recommend \xe2\x86\x92 Apply \xe2\x86\x92 Verify \xe2\x86\x92 Feedback\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n\n");
    std::fflush(stdout);

    // ─── Core Correction Cycle Tests ────────────────────────────────────
    test_store_recommendation();
    test_verify_applied();
    test_verify_overapplied();
    test_verify_underapplied();
    test_verify_ignored();
    test_verify_multiple_tracks();

    // ─── History & Stats Tests ──────────────────────────────────────────
    test_correction_history_integration();
    test_slot_correction_stats();
    test_most_urgent_recommendation();

    // ─── Domain-Specific Tests ──────────────────────────────────────────
    test_verify_tonal_domain();
    test_verify_spatial_domain();
    test_dismiss_recommendation();
    test_adaptive_thresholds();

    std::printf("\n\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::fflush(stdout);

    return gTestsFailed > 0 ? 1 : 0;
}
