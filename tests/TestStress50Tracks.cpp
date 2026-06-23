// ═══════════════════════════════════════════════════════════════════════════
//  TestStress50Tracks.cpp — Stress test for 55+ slots (V3 identity only)
//  Simulates the exact scenario that previously crashed with 50+ Messengers.
//  V3: Solo identidad (sin telemetría, sin backup files).
//
//  Tests:
//   1. Register 55 slots + verify count
//   2. forEachActive with 55 slots
//   3. Bus distribution with tracks in each bus
//   4. Update slot properties on all 55
//   5. SharedMemoryManager with 55 slots
//   6. checkStaleSlots() with mixed active/removed
//   7. Playlist-style iteration (simulate PlaylistComponent::updateList)
//   8. Performance: forEachActive 100 iterations
//   9. Full capacity 128 + overflow
//
//  Build: cmake --build build --config Release --target TestStress50Tracks
//  Run:   build/tests/Release/TestStress50Tracks.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstring>
#include <memory>
#include <chrono>
#include <vector>
#include <algorithm>

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

static constexpr int kStressTrackCount = 55;

// ─── Timing helper ──────────────────────────────────────────────────────────
struct ScopedTimer {
    std::chrono::high_resolution_clock::time_point start;
    const char* label;
    ScopedTimer(const char* lbl) : start(std::chrono::high_resolution_clock::now()), label(lbl) {}
    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::printf("  \xe2\x8f\xb1 %s: %.3f ms\n", label, ms);
        std::fflush(stdout);
    }
};

// ═══════════════════════════════════════════════════════════════════════════
//  Test 1: Register 55 slots + verify count
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

    auto info0 = reg->getSlotInfo(0);
    TEST("Slot 0 name: Track_0", juce::String(info0.trackName).trim() == "Track_0");
    auto info54 = reg->getSlotInfo(54);
    TEST("Slot 54 name: Track_54", juce::String(info54.trackName).trim() == "Track_54");
    auto info99 = reg->getSlotInfo(99);
    TEST("Slot 99 is inactive (not registered)", !info99.active);

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 2: forEachActive with 55 slots
// ═══════════════════════════════════════════════════════════════════════════
static void test_forEachActive_55()
{
    std::printf("\n── Test 2: forEachActive (55 slots) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    for (int i = 0; i < kStressTrackCount; ++i)
        reg->registerSlot("T_" + std::to_string(i), juce::Colours::grey,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));

    int count = 0;
    reg->forEachActive([&](const mixcoach::SlotInfo& info) {
        count++;
        TEST("slot index valid", info.slotIndex >= 0 && info.slotIndex < mixcoach::SlotRegistry::kMaxSlots);
        TEST("slot active", info.active);
        TEST("slot not stale", !info.stale);
    });
    TEST("forEachActive visited all 55 slots", count == kStressTrackCount);

    // Release a few and re-check
    reg->releaseSlot(0);
    reg->releaseSlot(1);
    reg->releaseSlot(2);

    int afterRelease = 0;
    reg->forEachActive([&](const mixcoach::SlotInfo&) { afterRelease++; });
    TEST("forEachActive = 52 after releasing 3", afterRelease == kStressTrackCount - 3);

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 3: Bus distribution
// ═══════════════════════════════════════════════════════════════════════════
static void test_bus_distribution()
{
    std::printf("\n── Test 3: Bus distribution (55 slots / 7 buses) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    int busCounts[7] = {0};
    for (int i = 0; i < kStressTrackCount; ++i)
        reg->registerSlot("T_" + std::to_string(i), juce::Colours::grey,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));

    reg->forEachActive([&](const mixcoach::SlotInfo& info) {
        int busIdx = static_cast<int>(info.bus);
        if (busIdx >= 0 && busIdx < mixcoach::kNumBuses)
            busCounts[busIdx]++;
    });

    int total = 0;
    for (int b = 0; b < mixcoach::kNumBuses; ++b) total += busCounts[b];
    TEST("All 55 tracks distributed across bus groups", total == kStressTrackCount);

    for (int b = 0; b < mixcoach::kNumBuses; ++b) {
        std::string tn = "Bus " + std::to_string(b) + " has ~9 tracks";
        TEST(tn.c_str(), busCounts[b] >= 7 && busCounts[b] <= 11);
    }

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 4: Update slot properties on 55
// ═══════════════════════════════════════════════════════════════════════════
static void test_update_properties()
{
    std::printf("\n── Test 4: Update properties on 55 slots ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    for (int i = 0; i < kStressTrackCount; ++i)
        reg->registerSlot("Orig_" + std::to_string(i), juce::Colours::grey,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));

    for (int i = 0; i < kStressTrackCount; ++i) {
        reg->updateSlotName(i, "Updated_" + std::to_string(i));
        reg->updateSlotColour(i, juce::Colour::fromHSV(static_cast<float>(i) / 55.0f, 1.0f, 0.8f, 1.0f));
        reg->updateSlotBus(i, static_cast<mixcoach::BusType>((i + 1) % mixcoach::kNumBuses));
    }

    for (int i = 0; i < kStressTrackCount; ++i) {
        auto info = reg->getSlotInfo(i);
        juce::String expectedName = "Updated_" + std::to_string(i);
        TEST("Slot " + std::to_string(i) + " name updated",
             juce::String(info.trackName).trim() == expectedName);
    }

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 5: SharedMemoryManager with 55 slots
// ═══════════════════════════════════════════════════════════════════════════
static void test_shared_memory_55()
{
    std::printf("\n── Test 5: SharedMemoryManager (55 slots) ──\n");
    std::fflush(stdout);

    juce::String testMapName = "Local\\MixCoach_Stress_55_SHM_" +
        juce::String(juce::Time::getMillisecondCounter());

    mixcoach::SharedMemoryManager shm;
    if (!shm.initialize(testMapName)) {
        std::printf("  \xe2\x9a\xa0\xef\xb8\x8f SKIP: Shared memory not available\n");
        std::fflush(stdout);
        return;
    }

    for (int i = 0; i < kStressTrackCount; ++i) {
        mixcoach::SharedSlotEntry entry;
        entry.slotIndex = i;
        entry.active = 1;
        entry.bus = i % mixcoach::kNumBuses;
        entry.colourARGB = 0xFF3498DB;
        std::snprintf(entry.trackName, sizeof(entry.trackName), "SHM_%d", i);

        int assigned = shm.registerSlot(entry);
        TEST("SHM registerSlot " + std::to_string(i), assigned >= 0);
    }

    TEST("changeCount > 0", shm.getChangeCount() > 0);

    // Read back all
    for (int i = 0; i < kStressTrackCount; ++i) {
        mixcoach::SharedSlotEntry readBack;
        bool ok = shm.readSlot(i, readBack);
        TEST("readSlot " + std::to_string(i), ok && readBack.active == 1);
    }

    // SlotRegistry sync
    auto reg = std::make_unique<mixcoach::SlotRegistry>();
    reg->setSharedMemory(&shm);
    int found = reg->forceFullSync();
    TEST("forceFullSync found 55 slots", found >= kStressTrackCount);
    TEST("activeCount = 55", reg->activeCount() == kStressTrackCount);

    // Release 5 via SHM
    for (int i = 50; i < 55; ++i)
        shm.releaseSlot(i);

    reg->checkStaleSlots();
    int stale = 0;
    for (int i = 0; i < kStressTrackCount; ++i)
        if (reg->getSlotInfo(i).stale) stale++;
    TEST("5 stale after releasing 5 via SHM", stale == 5);

    shm.close();
    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 6: checkStaleSlots() with mixed active/removed
// ═══════════════════════════════════════════════════════════════════════════
static void test_stale_detection()
{
    std::printf("\n── Test 6: Stale detection (55 tracks) ──\n");
    std::fflush(stdout);

    juce::String testMapName = "Local\\MixCoach_Stress_55_Stale_" +
        juce::String(juce::Time::getMillisecondCounter());

    mixcoach::SharedMemoryManager shm;
    if (!shm.initialize(testMapName)) {
        std::printf("  \xe2\x9a\xa0\xef\xb8\x8f SKIP: Shared memory not available\n");
        std::fflush(stdout);
        return;
    }

    for (int i = 0; i < kStressTrackCount; ++i) {
        mixcoach::SharedSlotEntry entry;
        entry.slotIndex = i;
        entry.active = 1;
        entry.bus = i % mixcoach::kNumBuses;
        entry.colourARGB = 0xFF9B59B6;
        std::snprintf(entry.trackName, sizeof(entry.trackName), "T_%d", i);
        shm.registerSlot(entry);
    }

    auto reg = std::make_unique<mixcoach::SlotRegistry>();
    reg->setSharedMemory(&shm);
    reg->forceFullSync();

    reg->checkStaleSlots();
    int staleCount = 0;
    for (int i = 0; i < kStressTrackCount; ++i)
        if (reg->getSlotInfo(i).stale) staleCount++;
    TEST("0 stale initially", staleCount == 0);

    // Make 5 stale
    for (int i = 10; i < 15; ++i)
        shm.releaseSlot(i);

    reg->checkStaleSlots();
    int newStale = 0;
    for (int i = 0; i < kStressTrackCount; ++i)
        if (reg->getSlotInfo(i).stale) newStale++;
    TEST("5 stale detected", newStale == 5);

    // forEachActive excludes stale
    int nonStale = 0;
    reg->forEachActive([&](const mixcoach::SlotInfo& info) {
        if (!info.stale) nonStale++;
    });
    TEST("50 non-stale counted by forEachActive", nonStale == kStressTrackCount - 5);

    // Recover
    for (int i = 10; i < 15; ++i) {
        mixcoach::SharedSlotEntry entry;
        entry.slotIndex = i;
        entry.active = 1;
        entry.bus = i % mixcoach::kNumBuses;
        entry.colourARGB = 0xFFE74C3C;
        std::snprintf(entry.trackName, sizeof(entry.trackName), "Recovered_%d", i);
        shm.registerSlot(entry);
    }

    reg->checkStaleSlots();
    int recovered = 0;
    for (int i = 0; i < kStressTrackCount; ++i)
        if (reg->getSlotInfo(i).stale) recovered++;
    TEST("0 stale after recovery", recovered == 0);

    shm.close();
    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 7: Playlist-style iteration
// ═══════════════════════════════════════════════════════════════════════════
static void test_playlist_iteration()
{
    std::printf("\n── Test 7: Playlist-style iteration (55 tracks) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    for (int i = 0; i < kStressTrackCount; ++i)
        reg->registerSlot("Track_" + std::to_string(i), juce::Colours::grey,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));

    struct TrackEntry {
        int slotIndex;
        mixcoach::SlotInfo info;
    };

    std::vector<TrackEntry> entries;
    reg->forEachActive([&](const mixcoach::SlotInfo& info) {
        entries.push_back({info.slotIndex, info});
    });

    TEST("Playlist has 55 entries", entries.size() == kStressTrackCount);

    // Bus grouping
    int busGroups[7] = {0};
    for (auto& e : entries) {
        int busIdx = static_cast<int>(e.info.bus);
        if (busIdx >= 0 && busIdx < 6)
            busGroups[busIdx]++;
        else
            busGroups[6]++;
    }

    int total = 0;
    for (int b = 0; b < 7; ++b) total += busGroups[b];
    TEST("All 55 distributed across bus groups", total == kStressTrackCount);

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 8: Performance — forEachActive 100 iterations
// ═══════════════════════════════════════════════════════════════════════════
static void test_performance()
{
    std::printf("\n── Test 8: Performance (55 tracks, 100 iterations) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    for (int i = 0; i < kStressTrackCount; ++i)
        reg->registerSlot("T_" + std::to_string(i), juce::Colours::grey,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));

    constexpr int kIterations = 100;
    auto start = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < kIterations; ++iter) {
        int count = 0;
        reg->forEachActive([&](const mixcoach::SlotInfo&) { count++; });
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();
    double avgUs = static_cast<double>(elapsedUs) / kIterations;

    std::printf("  \xe2\x8f\xb1 forEachActive (55 tracks): avg=%.1f us (%.2f ms for %d iters)\n",
                avgUs, elapsedUs / 1000.0, kIterations);
    std::fflush(stdout);

    TEST("forEachActive with 55 tracks < 100 us avg", avgUs < 100.0);

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 9: Full capacity (128 slots) + overflow
// ═══════════════════════════════════════════════════════════════════════════
static void test_full_capacity()
{
    std::printf("\n── Test 9: Full capacity (128 slots) + overflow ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    for (int i = 0; i < mixcoach::SlotRegistry::kMaxSlots; ++i) {
        int idx = reg->registerSlot("Full_" + std::to_string(i), juce::Colours::grey,
            static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));
        TEST("registerSlot " + std::to_string(i), idx >= 0);
    }
    TEST("activeCount = 128", reg->activeCount() == mixcoach::SlotRegistry::kMaxSlots);

    int count = 0;
    reg->forEachActive([&](const mixcoach::SlotInfo&) { count++; });
    TEST("forEachActive visited 128", count == mixcoach::SlotRegistry::kMaxSlots);

    // Overflow
    int overflow = reg->registerSlot("Overflow", juce::Colours::red, mixcoach::BusType::None);
    TEST("registerSlot returns -1 when full (no SHM)", overflow == -1);

    // Release + reuse
    reg->releaseSlot(100);
    TEST("Released slot 100 inactive", !reg->getSlotInfo(100).active);
    TEST("activeCount = 127 after release", reg->activeCount() == 127);

    int reused = reg->registerSlot("Reused100", juce::Colours::cyan, mixcoach::BusType::FX);
    TEST("Reused slot index 100", reused == 100);
    TEST("activeCount = 128 again", reg->activeCount() == 128);
    TEST("Reused slot name: Reused100",
         juce::String(reg->getSlotInfo(100).trackName).trim() == "Reused100");

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════
int main()
{
    std::printf("\n"
        "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
        "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
        "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
        "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
        "\n");
    std::printf("  \xf0\x9f\x92\xaa  STRESS TEST V3: 55+ Tracks (identidad pura)\n");
    std::printf("  kMaxSlots=%d | kSharedMaxSlots=%d | structVersion=%d\n",
                mixcoach::SlotRegistry::kMaxSlots,
                mixcoach::kSharedMaxSlots,
                mixcoach::kSharedMemoryStructVersion);
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\n\n");
    std::fflush(stdout);

    test_register_55_slots();
    test_forEachActive_55();
    test_bus_distribution();
    test_update_properties();
    test_shared_memory_55();
    test_stale_detection();
    test_playlist_iteration();
    test_performance();
    test_full_capacity();

    std::printf("\n"
        "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
        "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
        "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
        "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
        "\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\n");
    std::fflush(stdout);

    return gTestsFailed > 0 ? 1 : 0;
}
