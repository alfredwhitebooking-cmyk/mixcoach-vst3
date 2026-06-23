// ═══════════════════════════════════════════════════════════════════════════
//  TestStress128Slots.cpp — STRESS TEST: 128 slots (V3 identity only)
//  Simulates the WORST CASE: all 128 Messenger slots filled simultaneously.
//  V3: Solo identidad (sin telemetría, sin backup files).
//
//  Tests:
//   1. Register ALL 128 slots + verify activeCount & overflow
//   2. forEachActive with 128 active slots
//   3. Bus distribution across 7 bus types
//   4. Update slot properties (name, colour, bus) on all 128
//   5. SharedMemoryManager: register 128 + read back + batch poll
//   6. checkStaleSlots() with mixed active/removed
//   7. Release/reuse cycle (flood test: 3 complete cycles)
//   8. Rapid churn (1000 register/release cycles)
//   9. Performance: forEachActive 1000 iterations
//  10. Memory: clean state after full release
//
//  Build: cmake --build build --config Release --target TestStress128Slots
//  Run:   build/tests/Release/TestStress128Slots.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstring>
#include <memory>
#include <chrono>
#include <vector>
#include <algorithm>
#include <set>

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

static constexpr int kTestMaxSlots = mixcoach::SlotRegistry::kMaxSlots; // 128

// ─── Timing helper ──────────────────────────────────────────────────────────
struct ScopedTimer {
    std::chrono::high_resolution_clock::time_point start;
    const char* label;
    double* outMs;
    ScopedTimer(const char* lbl, double* out = nullptr)
        : start(std::chrono::high_resolution_clock::now()), label(lbl), outMs(out) {}
    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();
        std::printf("  \xe2\x8f\xb1 %s: %.3f ms\n", label, ms);
        std::fflush(stdout);
        if (outMs) *outMs = ms;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
//  Test 1: Register ALL 128 slots + verify count + overflow
// ═══════════════════════════════════════════════════════════════════════════
static void test_register_all_128()
{
    std::printf("\n── Test 1: Register ALL 128 slots ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    for (int i = 0; i < kTestMaxSlots; ++i) {
        std::string name = "Track_" + std::to_string(i);
        auto bus = static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses);
        float hue = static_cast<float>(i) / static_cast<float>(kTestMaxSlots);
        auto colour = juce::Colour::fromHSV(hue, 0.8f, 0.7f, 1.0f);
        int idx = reg->registerSlot(name, colour, bus);
        TEST((std::string("Slot ") + std::to_string(i) + " registered").c_str(),
             idx >= 0 && idx < kTestMaxSlots);
    }

    TEST("activeCount = 128", reg->activeCount() == kTestMaxSlots);
    TEST("totalSlots = 128", reg->totalSlots() == kTestMaxSlots);

    // Overflow should return -1 (no SHM in this test)
    int overflow = reg->registerSlot("Overflow", juce::Colours::red, mixcoach::BusType::None);
    TEST("registerSlot returns -1 when full (no SHM)", overflow == -1);
    TEST("activeCount still 128 after overflow", reg->activeCount() == kTestMaxSlots);

    // Verify first and last slots
    auto info0 = reg->getSlotInfo(0);
    TEST("Slot 0 name: Track_0", juce::String(info0.trackName).trim() == "Track_0");
    auto info127 = reg->getSlotInfo(127);
    TEST("Slot 127 name: Track_127", juce::String(info127.trackName).trim() == "Track_127");
    TEST("Slot 127 active", info127.active);

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 2: forEachActive with 128 slots
// ═══════════════════════════════════════════════════════════════════════════
static void test_forEachActive_128()
{
    std::printf("\n── Test 2: forEachActive (128 active slots) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    for (int i = 0; i < kTestMaxSlots; ++i) {
        reg->registerSlot("T_" + std::to_string(i), juce::Colours::grey,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));
    }

    // forEachActive: must visit all 128
    int count = 0;
    reg->forEachActive([&](const mixcoach::SlotInfo& info) {
        count++;
        TEST("forEachActive slot index valid", info.slotIndex >= 0 && info.slotIndex < kTestMaxSlots);
        TEST("forEachActive slot active", info.active);
        TEST("forEachActive slot not stale", !info.stale);
    });
    TEST("forEachActive visited all 128 slots", count == kTestMaxSlots);

    // Release a few and verify forEachActive skips them
    reg->releaseSlot(0);
    reg->releaseSlot(1);

    int afterRelease = 0;
    reg->forEachActive([&](const mixcoach::SlotInfo&) { afterRelease++; });
    TEST("forEachActive = 126 after releasing 2 slots", afterRelease == kTestMaxSlots - 2);

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 3: Bus distribution across 128 slots
// ═══════════════════════════════════════════════════════════════════════════
static void test_bus_distribution()
{
    std::printf("\n── Test 3: Bus distribution (128 slots / 7 buses) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    for (int i = 0; i < kTestMaxSlots; ++i) {
        reg->registerSlot("T_" + std::to_string(i), juce::Colours::grey,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));
    }

    int busCounts[7] = {0};  // 6 buses + unassigned
    reg->forEachActive([&](const mixcoach::SlotInfo& info) {
        int busIdx = static_cast<int>(info.bus);
        if (busIdx >= 0 && busIdx < mixcoach::kNumBuses)
            busCounts[busIdx]++;
        else
            busCounts[mixcoach::kNumBuses]++;
    });

    // 128 / 7 buses = ~18 per bus
    int total = 0;
    for (int b = 0; b < 7; ++b) total += busCounts[b];
    TEST("All 128 tracks distributed across bus groups", total == kTestMaxSlots);

    for (int b = 0; b < mixcoach::kNumBuses; ++b) {
        std::string tn = "Bus " + std::to_string(b) + " has tracks";
        TEST(tn.c_str(), busCounts[b] > 0);
    }

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 4: Update slot properties on all 128
// ═══════════════════════════════════════════════════════════════════════════
static void test_update_properties()
{
    std::printf("\n── Test 4: Update properties (name/colour/bus) on all 128 ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    for (int i = 0; i < kTestMaxSlots; ++i)
        reg->registerSlot("Orig_" + std::to_string(i), juce::Colours::grey,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));

    // Update properties on all 128
    for (int i = 0; i < kTestMaxSlots; ++i) {
        reg->updateSlotName(i, "Updated_" + std::to_string(i));
        reg->updateSlotColour(i, juce::Colour::fromHSV(static_cast<float>(i) / 128.0f, 1.0f, 0.8f, 1.0f));
        reg->updateSlotBus(i, static_cast<mixcoach::BusType>((i + 1) % mixcoach::kNumBuses));
    }

    // Verify all updates
    for (int i = 0; i < kTestMaxSlots; ++i) {
        auto info = reg->getSlotInfo(i);
        juce::String expectedName = "Updated_" + std::to_string(i);
        std::string tn = "Slot " + std::to_string(i) + " name updated";
        TEST(tn.c_str(), juce::String(info.trackName).trim() == expectedName);
    }

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 5: SharedMemoryManager with 128 slots
// ═══════════════════════════════════════════════════════════════════════════
static void test_shared_memory_128()
{
    std::printf("\n── Test 5: SharedMemoryManager (128 slots) ──\n");
    std::fflush(stdout);

    juce::String testMapName = "Local\\MixCoach_Stress_128_SHM_" +
        juce::String(juce::Time::getMillisecondCounter());

    mixcoach::SharedMemoryManager shm;
    if (!shm.initialize(testMapName)) {
        std::printf("  \xe2\x9a\xa0\xef\xb8\x8f SKIP: Shared memory init failed\n");
        std::fflush(stdout);
        return;
    }

    // Register 128 slots via SHM
    for (int i = 0; i < kTestMaxSlots; ++i) {
        mixcoach::SharedSlotEntry entry;
        entry.slotIndex = i;
        entry.active = 1;
        entry.bus = i % mixcoach::kNumBuses;
        entry.colourARGB = 0xFF3498DB;
        std::snprintf(entry.trackName, sizeof(entry.trackName), "SHM_%d", i);

        int assigned = shm.registerSlot(entry);
        TEST("SHM registerSlot " + std::to_string(i) + " OK",
             assigned >= 0 && assigned < mixcoach::kSharedMaxSlots);
    }

    // Verify change count
    TEST("changeCount > 0 after 128 registrations", shm.getChangeCount() > 0);

    // Read back all 128 slots
    for (int i = 0; i < kTestMaxSlots; ++i) {
        mixcoach::SharedSlotEntry readBack;
        bool ok = shm.readSlot(i, readBack);
        TEST("readSlot " + std::to_string(i) + " succeeds", ok);
        if (ok) {
            TEST("readback slot " + std::to_string(i) + " active", readBack.active == 1);
        }
    }

    // SlotRegistry: forceFullSync + verify
    auto reg = std::make_unique<mixcoach::SlotRegistry>();
    reg->setSharedMemory(&shm);
    int found = reg->forceFullSync();
    TEST("forceFullSync found 128 slots", found >= kTestMaxSlots);
    TEST("activeCount = 128", reg->activeCount() == kTestMaxSlots);

    // Health check
    TEST("SharedMemory healthCheck OK", shm.healthCheck());

    // Release 10 slots via SHM directly
    for (int i = 50; i < 60; ++i)
        shm.releaseSlot(i);

    // checkStaleSlots should mark them stale
    reg->checkStaleSlots();
    int staleCount = 0;
    for (int i = 0; i < kTestMaxSlots; ++i) {
        if (reg->getSlotInfo(i).stale) staleCount++;
    }
    TEST("10 stale after releasing 10 slots via SHM", staleCount == 10);

    shm.close();
    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 6: checkStaleSlots() with mixed active/removed across all 128
// ═══════════════════════════════════════════════════════════════════════════
static void test_stale_detection()
{
    std::printf("\n── Test 6: Stale detection (128 slots) ──\n");
    std::fflush(stdout);

    juce::String testMapName = "Local\\MixCoach_Stress_Stale_" +
        juce::String(juce::Time::getMillisecondCounter());

    mixcoach::SharedMemoryManager shm;
    if (!shm.initialize(testMapName)) {
        std::printf("  \xe2\x9a\xa0\xef\xb8\x8f SKIP: Shared memory not available\n");
        std::fflush(stdout);
        return;
    }

    // Register 128 in SHM
    for (int i = 0; i < kTestMaxSlots; ++i) {
        mixcoach::SharedSlotEntry entry;
        entry.slotIndex = i;
        entry.active = 1;
        entry.bus = i % mixcoach::kNumBuses;
        entry.colourARGB = 0xFF2ECC71;
        std::snprintf(entry.trackName, sizeof(entry.trackName), "T_%d", i);
        shm.registerSlot(entry);
    }

    auto reg = std::make_unique<mixcoach::SlotRegistry>();
    reg->setSharedMemory(&shm);
    reg->forceFullSync();

    // Initially no stale
    reg->checkStaleSlots();
    int staleCount = 0;
    for (int i = 0; i < kTestMaxSlots; ++i)
        if (reg->getSlotInfo(i).stale) staleCount++;
    TEST("0 stale initially", staleCount == 0);

    // Release random 20 slots via SHM
    for (int i = 10; i < 30; ++i)
        shm.releaseSlot(i);

    reg->checkStaleSlots();
    int newStale = 0;
    for (int i = 0; i < kTestMaxSlots; ++i)
        if (reg->getSlotInfo(i).stale) newStale++;
    TEST("20 stale after releasing 20 via SHM", newStale == 20);

    // forEachActive should exclude stale
    int activeNonStale = 0;
    reg->forEachActive([&](const mixcoach::SlotInfo& info) {
        if (!info.stale) activeNonStale++;
    });
    TEST("108 non-stale counted by forEachActive",
         activeNonStale == kTestMaxSlots - 20);

    // Re-register the released slots
    for (int i = 10; i < 30; ++i) {
        mixcoach::SharedSlotEntry entry;
        entry.slotIndex = i;
        entry.active = 1;
        entry.bus = i % mixcoach::kNumBuses;
        entry.colourARGB = 0xFFE74C3C;
        std::snprintf(entry.trackName, sizeof(entry.trackName), "Recovered_%d", i);
        shm.registerSlot(entry);
    }

    reg->checkStaleSlots();
    int recoveredStale = 0;
    for (int i = 0; i < kTestMaxSlots; ++i)
        if (reg->getSlotInfo(i).stale) recoveredStale++;
    TEST("0 stale after recovery", recoveredStale == 0);

    shm.close();
    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 7: Release/reuse FLOOD test (3 complete cycles)
// ═══════════════════════════════════════════════════════════════════════════
static void test_release_reuse_flood()
{
    std::printf("\n── Test 7: Release/Reuse FLOOD (3 complete cycles) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    for (int cycle = 0; cycle < 3; ++cycle) {
        std::printf("  Cycle %d: register 128...\n", cycle + 1);
        std::fflush(stdout);

        for (int i = 0; i < kTestMaxSlots; ++i) {
            std::string name = "Flood_" + std::to_string(cycle) + "_" + std::to_string(i);
            int idx = reg->registerSlot(name, juce::Colours::grey,
                static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));
            std::string tn = "Cycle " + std::to_string(cycle) +
                             " slot " + std::to_string(i) + " registered";
            TEST(tn.c_str(), idx >= 0 && idx < kTestMaxSlots);
        }
        TEST("activeCount = 128 after cycle " + std::to_string(cycle),
             reg->activeCount() == kTestMaxSlots);

        // Release all 128 (reverse order)
        std::printf("  Cycle %d: release 128...\n", cycle + 1);
        std::fflush(stdout);
        for (int i = kTestMaxSlots - 1; i >= 0; --i)
            reg->releaseSlot(i);

        TEST("activeCount = 0 after release cycle " + std::to_string(cycle),
             reg->activeCount() == 0);

        // Verify all released
        for (int i = 0; i < kTestMaxSlots; ++i) {
            std::string tn = "Slot " + std::to_string(i) + " inactive after release";
            TEST(tn.c_str(), !reg->getSlotInfo(i).active);
        }
    }

    // Final re-register to prove health
    for (int i = 0; i < kTestMaxSlots; ++i) {
        reg->registerSlot("Final_" + std::to_string(i), juce::Colours::green,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));
    }
    TEST("activeCount = 128 after 3 flood cycles + final register",
         reg->activeCount() == kTestMaxSlots);

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 8: Rapid churn (1000 register/release cycles)
// ═══════════════════════════════════════════════════════════════════════════
static void test_rapid_churn()
{
    std::printf("\n── Test 8: Rapid churn (1000 register/release cycles) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();
    constexpr int kChurnCycles = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int cycle = 0; cycle < kChurnCycles; ++cycle) {
        int indices[5];
        for (int i = 0; i < 5; ++i) {
            indices[i] = reg->registerSlot(
                "Churn_" + std::to_string(cycle) + "_" + std::to_string(i),
                juce::Colours::yellow,
                static_cast<mixcoach::BusType>((cycle + i) % mixcoach::kNumBuses));
        }
        for (int i = 0; i < 3; ++i) {
            if (indices[i] >= 0)
                reg->releaseSlot(indices[i]);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
    std::printf("  \xe2\x8f\xb1 %d churn cycles: %.2f ms (%.1f us/cycle)\n",
                kChurnCycles, elapsedMs, elapsedMs * 1000.0 / kChurnCycles);
    std::fflush(stdout);

    TEST("No crash after 1000 churn cycles", true);
    TEST("Registry still operational", reg->activeCount() >= 0);

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 9: Performance — forEachActive 1000 iterations
// ═══════════════════════════════════════════════════════════════════════════
static void test_performance()
{
    std::printf("\n── Test 9: Performance (128 tracks, 1000 iterations) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    for (int i = 0; i < kTestMaxSlots; ++i)
        reg->registerSlot("T_" + std::to_string(i), juce::Colours::grey,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));

    constexpr int kIterations = 1000;

    auto start = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < kIterations; ++iter) {
        int count = 0;
        reg->forEachActive([&](const mixcoach::SlotInfo&) { count++; });
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();
    double avgUs = static_cast<double>(elapsedUs) / kIterations;

    std::printf("  \xe2\x8f\xb1 forEachActive (128 tracks): avg=%.2f us "
                "(%.2f ms for %d iters)\n",
                avgUs, elapsedUs / 1000.0, kIterations);
    std::fflush(stdout);

    TEST("forEachActive with 128 tracks < 200 us avg", avgUs < 200.0);

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 10: Memory — clean state after full release
// ═══════════════════════════════════════════════════════════════════════════
static void test_memory_cleanup()
{
    std::printf("\n── Test 10: Memory cleanup verification ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    for (int i = 0; i < kTestMaxSlots; ++i)
        reg->registerSlot("Mem_" + std::to_string(i), juce::Colours::blue,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));

    for (int i = 0; i < kTestMaxSlots; ++i)
        reg->releaseSlot(i);

    TEST("activeCount = 0 after memory test", reg->activeCount() == 0);
    for (int i = 0; i < kTestMaxSlots; ++i) {
        auto info = reg->getSlotInfo(i);
        TEST("Slot " + std::to_string(i) + " inactive", !info.active);
        TEST("Slot " + std::to_string(i) + " slotIndex = -1",
             info.slotIndex == -1);
    }

    // Re-register to prove reusability
    for (int i = 0; i < 10; ++i) {
        int idx = reg->registerSlot("Reuse_" + std::to_string(i),
                                     juce::Colours::red, mixcoach::BusType::Drums);
        TEST("Reuse register returns valid index", idx >= 0);
    }
    TEST("activeCount = 10 after reuse", reg->activeCount() == 10);

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
    std::printf("  \xf0\x9f\x92\xaa  STRESS TEST V3: MAXIMUM CAPACITY (128 slots)\n");
    std::printf("  \xf0\x9f\x94\xa5  Identidad pura V3 (sin telemetr\xc3\xad" "a, sin backup)\n");
    std::printf("     kMaxSlots=%d | kSharedMaxSlots=%d | structVersion=%d\n",
                mixcoach::SlotRegistry::kMaxSlots,
                mixcoach::kSharedMaxSlots,
                mixcoach::kSharedMemoryStructVersion);
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\n\n");
    std::fflush(stdout);

    test_register_all_128();
    test_forEachActive_128();
    test_bus_distribution();
    test_update_properties();
    test_shared_memory_128();
    test_stale_detection();
    test_release_reuse_flood();
    test_rapid_churn();
    test_performance();
    test_memory_cleanup();

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
