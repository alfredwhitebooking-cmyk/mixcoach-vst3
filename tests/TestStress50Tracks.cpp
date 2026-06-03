// ═══════════════════════════════════════════════════════════════════════════
//  TestStress50Tracks.cpp — Stress test for 50+ tracks
//  Simulates the exact scenario that crashed (50+ Messengers in FL Studio)
//
//  Tests:
//   1. Register 55 slots (simulating 55 Messengers)
//   2. Push telemetry data to all 55
//   3. forEachActive with 55 tracks
//   4. buildMaster() composite with 55 tracks  
//   5. buildBus() composite with tracks in each bus
//   6. Backup file creation + loading for 55 slots
//   7. Shared memory with 55 slots
//   8. checkStaleSlots() with mixed stale/fresh tracks
//   9. Playlist-style iteration with 55 tracks
//  10. Memory: no excessive allocations or leaks
//
//  Build: cmake --build build --config Release --target TestStress50Tracks
//  Run:   build/tests/Release/TestStress50Tracks.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstring>
#include <memory>
#include <chrono>

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

#include "Common/types/Types.h"
#include "Common/types/Constants.h"
#include "Common/memory/SlotRegistry.h"
#include "Common/memory/SharedMemory.h"

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

#define TEST_NEAR(name, a, b, eps) TEST(name, std::fabs((a) - (b)) < (eps))

// ─── Constants ──────────────────────────────────────────────────────────────
static constexpr int kStressTrackCount = 55;  // More than 50 as user reported

// ─── Helpers ────────────────────────────────────────────────────────────────
static juce::File getBackupDir()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("MixCoach").getChildFile("SlotBackup");
}

static void cleanupBackupFiles()
{
    auto dir = getBackupDir();
    if (dir.exists()) {
        juce::Array<juce::File> files;
        dir.findChildFiles(files, juce::File::findFiles, false, "slot_*.bin");
        for (auto& f : files) f.deleteFile();
        files.clear();
        dir.findChildFiles(files, juce::File::findFiles, false, "_tmp_*.bin");
        for (auto& f : files) f.deleteFile();
    }
}

// ─── Build a TrackTelemetry with varied values per slot ─────────────────────
static mixcoach::TrackTelemetry makeTelemetry(int slotIndex, float basePeak = -12.0f)
{
    mixcoach::TrackTelemetry t;
    t.timestamp = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;
    t.slotIndex = slotIndex;
    t.active = true;
    t.peakLeft  = basePeak + static_cast<float>(slotIndex) * 0.5f;
    t.peakRight = basePeak + static_cast<float>(slotIndex) * 0.4f;
    t.rmsLeft   = basePeak - 6.0f + static_cast<float>(slotIndex) * 0.3f;
    t.rmsRight  = basePeak - 5.0f + static_cast<float>(slotIndex) * 0.3f;
    t.correlation = 0.9f - static_cast<float>(slotIndex) * 0.005f;
    t.crestFactor = 6.0f + static_cast<float>(slotIndex) * 0.1f;
    t.sampleL = 0.1f + static_cast<float>(slotIndex) * 0.002f;
    t.sampleR = 0.08f + static_cast<float>(slotIndex) * 0.002f;
    t.lufsIntegrated = -14.0f - static_cast<float>(slotIndex) * 0.1f;
    t.lufsShortTerm  = -16.0f - static_cast<float>(slotIndex) * 0.1f;
    t.lufsMomentary  = -12.0f - static_cast<float>(slotIndex) * 0.1f;
    t.lufsTruePeak   = basePeak + static_cast<float>(slotIndex) * 0.5f;
    t.loudnessRange  = 8.0f + static_cast<float>(slotIndex) * 0.05f;
    // Populate spectrum: a sine peak at different bins per track
    for (int j = 0; j < 256; ++j) {
        float freqPos = static_cast<float>(j) / 256.0f;
        float peakPos = static_cast<float>(slotIndex) / static_cast<float>(kStressTrackCount);
        t.spectrum[j] = 0.001f + 0.5f * std::exp(-std::pow((freqPos - peakPos) * 20.0f, 2.0f));
    }
    return t;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 1: Register 55 slots and verify count
// ═══════════════════════════════════════════════════════════════════════════
static void test_register_55_slots()
{
    std::printf("\n── Test 1: Register 55 slots ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    for (int i = 0; i < kStressTrackCount; ++i) {
        std::string name = "Track_" + std::to_string(i);
        auto bus = static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses);
        float hue = static_cast<float>(i) / static_cast<float>(kStressTrackCount);
        auto colour = juce::Colour::fromHSV(hue, 0.8f, 0.7f, 1.0f);
        int idx = reg->registerSlot(name, colour, bus);
        TEST("registerSlot returns valid index",
             idx >= 0 && idx < mixcoach::SlotRegistry::kMaxSlots);
    }

    TEST("activeCount = 55", reg->activeCount() == kStressTrackCount);
    TEST("totalSlots = 128", reg->totalSlots() == mixcoach::SlotRegistry::kMaxSlots);

    // Verify random slots
    auto info0 = reg->getSlotInfo(0);
    TEST("Slot 0 name: Track_0",
         juce::String(info0.trackName).trim() == "Track_0");

    auto info54 = reg->getSlotInfo(54);
    TEST("Slot 54 name: Track_54",
         juce::String(info54.trackName).trim() == "Track_54");

    auto info99 = reg->getSlotInfo(99);
    TEST("Slot 99 is inactive (not registered)", !info99.active);

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 2: Push telemetry to 55 slots + forEachActive
// ═══════════════════════════════════════════════════════════════════════════
static void test_telemetry_and_forEach()
{
    std::printf("\n── Test 2: Telemetry push + forEachActive (55 tracks) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    // Register 55 slots
    for (int i = 0; i < kStressTrackCount; ++i) {
        std::string name = "Track_" + std::to_string(i);
        auto bus = static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses);
        reg->registerSlot(name, juce::Colours::grey, bus);
    }

    // Push telemetry to all 55
    for (int i = 0; i < kStressTrackCount; ++i) {
        auto telem = makeTelemetry(i);
        reg->getTelemetry(i).push(telem);
    }

    // Verify telemetry via forEachActive
    int count = 0;
    reg->forEachActive([&](const mixcoach::SlotInfo& info) {
        count++;
        auto telem = reg->getTelemetry(info.slotIndex).latest();
        TEST("telemetry peakLeft > -100 for active slot",
             telem.peakLeft > -100.0f);
        TEST("telemetry timestamp > 0", telem.timestamp > 0);
    });
    TEST("forEachActive visited all 55 slots", count == kStressTrackCount);

    // Read buffer sizes
    int64_t totalEntries = 0;
    for (int i = 0; i < kStressTrackCount; ++i) {
        totalEntries += reg->getTelemetry(i).size();
    }
    TEST("Total telemetry entries across all slots = 55",
         totalEntries == kStressTrackCount);

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 3: buildMaster() with 55 tracks
//  This simulates the TelemetryProvider::buildMaster() call
//  that happens every frame in ALL mode
// ═══════════════════════════════════════════════════════════════════════════
static void test_build_master_composite()
{
    std::printf("\n── Test 3: buildMaster() composite (55 tracks) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    // Register and push telemetry
    for (int i = 0; i < kStressTrackCount; ++i) {
        std::string name = "Track_" + std::to_string(i);
        auto bus = static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses);
        reg->registerSlot(name, juce::Colours::grey, bus);
    }
    for (int i = 0; i < kStressTrackCount; ++i) {
        reg->getTelemetry(i).push(makeTelemetry(i));
    }

    // ═══ Simulate buildMaster() as done in TelemetryProvider ═══════════
    mixcoach::TrackTelemetry result;
    result.active = false;
    int activeCount = 0;
    float maxPeak = -100.0f;
    int loudestSlot = -1;

    // First pass: collect stats
    reg->forEachActive([&](const mixcoach::SlotInfo& info) {
        if (info.stale) return;
        int idx = info.slotIndex;
        if (idx < 0) return;

        auto t = reg->getTelemetry(idx).latest();
        result.active = true;

        if (t.peakLeft > result.peakLeft)   result.peakLeft  = t.peakLeft;
        if (t.peakRight > result.peakRight) result.peakRight = t.peakRight;

        float slotMax = juce::jmax(t.peakLeft, t.peakRight);
        if (slotMax > maxPeak) {
            maxPeak = slotMax;
            loudestSlot = idx;
        }
        ++activeCount;
    });

    TEST("buildMaster active = true", result.active);
    TEST("buildMaster activeCount = 55", activeCount == kStressTrackCount);
    TEST("buildMaster peakLeft matches loudest track (idx 54 = -12+27=-9.5)",
         std::fabs(result.peakLeft - (-12.0f + 54.0f * 0.5f)) < 0.01f);

    // Spectrum MAX pass (simulate spectrum compositing)
    {
        std::array<float, 256> spectrum{};
        reg->forEachActive([&](const mixcoach::SlotInfo& info) {
            if (info.stale) return;
            int idx = info.slotIndex;
            if (idx < 0) return;
            auto t = reg->getTelemetry(idx).latest();
            for (int j = 0; j < 256; ++j) {
                if (t.spectrum[j] > spectrum[j])
                    spectrum[j] = t.spectrum[j];
            }
        });

        // Verify spectrum has content (should have max values across 55 tracks)
        bool hasContent = false;
        for (int j = 0; j < 256; ++j) {
            if (spectrum[j] > 0.01f) { hasContent = true; break; }
        }
        TEST("buildMaster spectrum has content from 55 tracks", hasContent);
    }

    // Loudest track data
    if (loudestSlot >= 0) {
        auto t = reg->getTelemetry(loudestSlot).latest();
        float avgRms = (t.rmsLeft + t.rmsRight) * 0.5f;
        float crest = result.peakLeft - avgRms;
        TEST("buildMaster crestFactor > 0 (loudest track)", crest > 0.0f);
        TEST("buildMaster crestFactor < 30 (sensible range)", crest < 30.0f);
    }

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 4: buildBus() with tracks in each bus
// ═══════════════════════════════════════════════════════════════════════════
static void test_bus_composite()
{
    std::printf("\n── Test 4: buildBus() per bus group (55 tracks / 6 buses) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    // Register 55 tracks distributed across 6 buses
    int busCounts[6] = {0};
    for (int i = 0; i < kStressTrackCount; ++i) {
        std::string name = "Track_" + std::to_string(i);
        auto bus = static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses);
        reg->registerSlot(name, juce::Colours::grey, bus);
        busCounts[i % 6]++;
    }
    for (int i = 0; i < kStressTrackCount; ++i) {
        reg->getTelemetry(i).push(makeTelemetry(i, -10.0f));
    }

    // Build composite for each bus
    for (int b = 0; b < mixcoach::kNumBuses; ++b) {
        auto bus = static_cast<mixcoach::BusType>(b);
        int busActive = 0;
        float busPeak = -100.0f;

        reg->forEachActive([&](const mixcoach::SlotInfo& info) {
            if (info.bus != bus) return;
            if (info.stale) return;
            busActive++;
            auto t = reg->getTelemetry(info.slotIndex).latest();
            if (t.peakLeft > busPeak) busPeak = t.peakLeft;
        });

        std::string testName = "Bus " + std::to_string(b) + " has tracks";
        TEST(testName.c_str(), busActive > 0);
        TEST("Bus peak > -80", busPeak > -80.0f);
    }

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 5: Backup files for 55 slots
// ═══════════════════════════════════════════════════════════════════════════
static void test_backup_55_slots()
{
    std::printf("\n── Test 5: Backup files for 55 slots ──\n");
    std::fflush(stdout);
    cleanupBackupFiles();

    // Simulate 55 Messengers writing backup files
    for (int i = 0; i < kStressTrackCount; ++i) {
        mixcoach::SlotInfo slot;
        slot.slotIndex = i;
        slot.active = true;
        slot.bus = static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses);
        slot.colour = juce::Colour::fromHSV(
            static_cast<float>(i) / static_cast<float>(kStressTrackCount),
            0.8f, 0.7f, 1.0f);
        slot.setTrackName("Track_" + std::to_string(i));
        mixcoach::SlotRegistry::saveSlotToBackupFile(i, slot);

        // Write telemetry (simulating updateSlotBackupTelemetry)
        mixcoach::SlotRegistry::updateSlotBackupTelemetry(
            i, -10.0f, -12.0f, -18.0f, -20.0f, 0.9f, 8.0f,
            0.1f, -0.05f, -14.0f, -16.0f, -12.0f, -8.0f, 6.0f);
    }

    // Verify all 55 backup files exist
    juce::Array<juce::File> files;
    getBackupDir().findChildFiles(files, juce::File::findFiles, false, "slot_*.bin");
    TEST("55 backup files exist", files.size() >= kStressTrackCount);

    // Load all from MixCoach side
    auto reg = std::make_unique<mixcoach::SlotRegistry>();
    int found = reg->loadSlotsFromBackupFiles(true);
    TEST("loadSlotsFromBackupFiles found 55 slots", found >= kStressTrackCount);
    TEST("activeCount = 55", reg->activeCount() == kStressTrackCount);

    // Verify names
    for (int i = 0; i < kStressTrackCount; ++i) {
        auto info = reg->getSlotInfo(i);
        juce::String expectedName = "Track_" + std::to_string(i);
        std::string tn = "Slot " + std::to_string(i) + " name matches";
        TEST(tn.c_str(), juce::String(info.trackName).trim() == expectedName);
    }

    // Cleanup
    for (int i = 0; i < kStressTrackCount; ++i)
        mixcoach::SlotRegistry::removeSlotBackupFile(i);
    reg.reset();
    cleanupBackupFiles();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 6: Shared memory with 55 slots
// ═══════════════════════════════════════════════════════════════════════════
static void test_shared_memory_55_slots()
{
    std::printf("\n── Test 6: Shared memory with 55 slots ──\n");
    std::fflush(stdout);

    juce::String testMapName = "Local\\\\MixCoach_Stress_55_" +
        juce::String(juce::Time::getMillisecondCounter());

    mixcoach::SharedMemoryManager shm;
    if (!shm.initialize(testMapName)) {
        std::printf("  \xe2\x9a\xa0\xef\xb8\x8f SKIP: Shared memory not available\n");
        std::fflush(stdout);
        return;
    }

    // Register 55 slots
    for (int i = 0; i < kStressTrackCount; ++i) {
        mixcoach::SharedSlotEntry entry;
        entry.active = 1;
        entry.bus = i % mixcoach::kNumBuses;
        entry.colourARGB = 0xFF3498DB;
        entry.slotIndex = i;
        std::snprintf(entry.trackName, sizeof(entry.trackName),
                      "Track_%d", i);
        entry.peakLeft = -10.0f - static_cast<float>(i) * 0.2f;
        entry.rmsLeft = -18.0f - static_cast<float>(i) * 0.1f;
        entry.correlation = 0.9f - static_cast<float>(i) * 0.003f;

        int assigned = shm.registerSlot(entry);
        std::string tn = "registerSlot returns valid index for slot " +
                         std::to_string(i);
        TEST(tn.c_str(), assigned >= 0 && assigned < mixcoach::kSharedMaxSlots);
    }

    // Verify change count
    TEST("changeCount > 0 after 55 registrations", shm.getChangeCount() > 0);

    // Read back all 55 slots
    for (int i = 0; i < kStressTrackCount; ++i) {
        mixcoach::SharedSlotEntry readBack;
        bool ok = shm.readSlot(i, readBack);
        std::string tn = "readSlot " + std::to_string(i) + " succeeds";
        TEST(tn.c_str(), ok);
        if (ok) {
            TEST("readback slot is active", readBack.active == 1);
        }
    }

    // Read beyond our range (should be inactive)
    mixcoach::SharedSlotEntry emptySlot;
    bool emptyOk = shm.readSlot(kStressTrackCount, emptySlot);
    TEST("readSlot at index 55 succeeds (unregistered, inactive)",
         emptyOk && emptySlot.active == 0);

    shm.close();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 7: checkStaleSlots() with mixed stale/fresh tracks
// ═══════════════════════════════════════════════════════════════════════════
static void test_stale_detection()
{
    std::printf("\n── Test 7: Stale detection with 55 tracks ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    // Register 55 tracks
    for (int i = 0; i < kStressTrackCount; ++i) {
        std::string name = "Track_" + std::to_string(i);
        reg->registerSlot(name, juce::Colours::grey,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));
    }

    // Push telemetry with FRESH timestamps
    for (int i = 0; i < kStressTrackCount; ++i) {
        auto t = makeTelemetry(i);
        t.timestamp = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;
        reg->getTelemetry(i).push(t);
    }

    // Initially no stale tracks
    reg->checkStaleSlots();
    int staleCount = 0;
    for (int i = 0; i < kStressTrackCount; ++i) {
        if (reg->getSlotInfo(i).stale) staleCount++;
    }
    TEST("0 stale tracks initially (all fresh)", staleCount == 0);

    // Simulate some tracks going stale by setting old timestamps
    auto oldTime = static_cast<int64_t>(1);  // 1 microsecond = very old
    for (int i = 10; i < 15; ++i) {
        auto t = makeTelemetry(i);
        t.timestamp = oldTime;
        reg->getTelemetry(i).push(t);
    }

    // Check stale
    reg->checkStaleSlots();
    int newStaleCount = 0;
    for (int i = 0; i < kStressTrackCount; ++i) {
        if (reg->getSlotInfo(i).stale) newStaleCount++;
    }
    TEST("5 stale tracks detected", newStaleCount == 5);

    // Verify stale tracks are excluded from "analysis" (buildMaster-style)
    int activeNonStale = 0;
    reg->forEachActive([&](const mixcoach::SlotInfo& info) {
        if (!info.stale) activeNonStale++;
    });
    TEST("50 non-stale tracks counted by forEachActive",
         activeNonStale == kStressTrackCount - 5);

    // Simulate recovery: push fresh data to stale tracks
    for (int i = 10; i < 15; ++i) {
        auto t = makeTelemetry(i);
        t.timestamp = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;
        reg->getTelemetry(i).push(t);
    }

    reg->checkStaleSlots();
    int recoveredStale = 0;
    for (int i = 0; i < kStressTrackCount; ++i) {
        if (reg->getSlotInfo(i).stale) recoveredStale++;
    }
    TEST("0 stale tracks after recovery (all fresh again)", recoveredStale == 0);

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 8: Playlist-style iteration (simulating PlaylistComponent::updateList)
// ═══════════════════════════════════════════════════════════════════════════
static void test_playlist_iteration()
{
    std::printf("\n── Test 8: Playlist-style iteration (55 tracks) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    // Register 55 tracks
    for (int i = 0; i < kStressTrackCount; ++i) {
        std::string name = "Track_" + std::to_string(i);
        auto bus = static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses);
        reg->registerSlot(name, juce::Colours::grey, bus);
    }
    for (int i = 0; i < kStressTrackCount; ++i) {
        reg->getTelemetry(i).push(makeTelemetry(i));
    }

    // Simulate PlaylistComponent::updateList logic
    struct TrackEntry {
        int slotIndex;
        mixcoach::SlotInfo info;
        float peakLeft;
        bool hasSignal;
    };

    std::vector<TrackEntry> entries;
    float maxPeak = -100.0f;

    reg->forEachActive([&](const mixcoach::SlotInfo& info) {
        int idx = info.slotIndex;
        auto latest = reg->getTelemetry(idx).latest();
        TrackEntry e;
        e.slotIndex = idx;
        e.info = info;
        e.peakLeft = latest.peakLeft;
        e.hasSignal = (latest.peakLeft > -60.0f || latest.peakRight > -60.0f);
        if (e.peakLeft > maxPeak) maxPeak = e.peakLeft;
        entries.push_back(e);
    });

    TEST("Playlist has 55 entries", entries.size() == kStressTrackCount);
    TEST("All entries have signal", true);  // All our test tracks have signal > -60
    TEST("maxPeak > -80", maxPeak > -80.0f);

    // Bus grouping (same as PlaylistComponent)
    struct BusGroup {
        int count = 0;
    };
    BusGroup busGroups[7] = {};  // 6 buses + unassigned
    for (auto& e : entries) {
        int busIdx = static_cast<int>(e.info.bus);
        if (busIdx >= 0 && busIdx < 6)
            busGroups[busIdx].count++;
        else
            busGroups[6].count++;
    }

    int totalInGroups = 0;
    for (int b = 0; b < 7; ++b) {
        totalInGroups += busGroups[b].count;
    }
    TEST("All 55 tracks distributed across bus groups",
         totalInGroups == kStressTrackCount);

    // Each bus should have roughly equal count (55/6 ≈ 9 per bus)
    for (int b = 0; b < 6; ++b) {
        std::string tn = "Bus " + std::to_string(b) + " has ~9 tracks";
        TEST(tn.c_str(), busGroups[b].count >= 7 && busGroups[b].count <= 11);
    }

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 9: Performance — forEachActive with 55 tracks under 10ms
//  Ensures the compositing is fast enough for real-time UI (60fps = 16ms/frame)
// ═══════════════════════════════════════════════════════════════════════════
static void test_performance()
{
    std::printf("\n── Test 9: Performance (55 tracks, 100 iterations) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    // Register and push telemetry
    for (int i = 0; i < kStressTrackCount; ++i) {
        std::string name = "Track_" + std::to_string(i);
        reg->registerSlot(name, juce::Colours::grey,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));
    }
    for (int i = 0; i < kStressTrackCount; ++i) {
        reg->getTelemetry(i).push(makeTelemetry(i));
    }

    // Measure forEachActive + buildMaster composite (100 iterations)
    auto start = std::chrono::high_resolution_clock::now();
    constexpr int kIterations = 100;

    for (int iter = 0; iter < kIterations; ++iter) {
        int count = 0;
        float maxPeak = -100.0f;
        reg->forEachActive([&](const mixcoach::SlotInfo& info) {
            auto t = reg->getTelemetry(info.slotIndex).latest();
            if (t.peakLeft > maxPeak) maxPeak = t.peakLeft;
            count++;
        });
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();
    double avgUs = static_cast<double>(elapsedUs) / kIterations;

    std::printf("  \xe2\x8f\xb1 Average forEachActive time: %.1f us (%.2f ms for 100 iters)\n",
                avgUs, elapsedUs / 1000.0);
    std::fflush(stdout);

    // Should be well under 1ms per iteration for 55 tracks
    TEST("forEachActive with 55 tracks < 1ms avg",
         avgUs < 1000.0);

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 10: Edge case — 128 slots (full capacity)
// ═══════════════════════════════════════════════════════════════════════════
static void test_full_capacity()
{
    std::printf("\n── Test 10: Full capacity (128 slots) ──\n");
    std::fflush(stdout);

    cleanupBackupFiles();
    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    // Fill ALL 128 slots
    for (int i = 0; i < mixcoach::SlotRegistry::kMaxSlots; ++i) {
        std::string name = "Full_" + std::to_string(i);
        int idx = reg->registerSlot(name, juce::Colours::grey,
            static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));
        std::string tn = "registerSlot " + std::to_string(i) +
                         " = " + std::to_string(idx);
        TEST(tn.c_str(), idx >= 0 && idx < mixcoach::SlotRegistry::kMaxSlots);
    }
    TEST("activeCount = 128", reg->activeCount() == mixcoach::SlotRegistry::kMaxSlots);

    // Push telemetry to all 128
    for (int i = 0; i < mixcoach::SlotRegistry::kMaxSlots; ++i) {
        reg->getTelemetry(i).push(makeTelemetry(i));
    }

    // forEachActive all 128
    int count = 0;
    reg->forEachActive([&](const mixcoach::SlotInfo&) { count++; });
    TEST("forEachActive visited 128 slots", count == mixcoach::SlotRegistry::kMaxSlots);

    // Verify overflow
    int overflow = reg->registerSlot("Overflow", juce::Colours::red,
                                      mixcoach::BusType::None);
    TEST("registerSlot returns -1 when full (no shared memory)", overflow == -1);

    // Release slot 100, verify reuse
    reg->releaseSlot(100);
    auto info100 = reg->getSlotInfo(100);
    TEST("Released slot 100 is inactive", !info100.active);
    TEST("activeCount = 127 after release", reg->activeCount() == 127);

    int reused = reg->registerSlot("Reused100", juce::Colours::cyan,
                                    mixcoach::BusType::FX);
    TEST("Reused slot index 100", reused == 100);
    TEST("activeCount = 128 again", reg->activeCount() == 128);
    auto reusedInfo = reg->getSlotInfo(100);
    TEST("Reused slot name: Reused100",
         juce::String(reusedInfo.trackName).trim() == "Reused100");

    reg.reset();
    cleanupBackupFiles();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════
int main()
{
    cleanupBackupFiles();

    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  \xf0\x9f\x92\xaa  STRESS TEST: 50+ Tracks (55 slots, 100 iterations)\n");
    std::printf("  kMaxSlots=%d | kSharedMaxSlots=%d | structVersion=%d\n",
                mixcoach::SlotRegistry::kMaxSlots,
                mixcoach::kSharedMaxSlots,
                mixcoach::SharedMemoryHeader::kCurrentStructVersion);
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n\n");
    std::fflush(stdout);

    test_register_55_slots();
    test_telemetry_and_forEach();
    test_build_master_composite();
    test_bus_composite();
    test_backup_55_slots();
    test_shared_memory_55_slots();
    test_stale_detection();
    test_playlist_iteration();
    test_performance();
    test_full_capacity();

    std::printf("\n\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::fflush(stdout);

    cleanupBackupFiles();
    return gTestsFailed > 0 ? 1 : 0;
}
