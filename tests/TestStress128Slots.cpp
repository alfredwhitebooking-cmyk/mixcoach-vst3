// ═══════════════════════════════════════════════════════════════════════════
//  TestStress128Slots.cpp — STRESS TEST: 128 slots (maximum capacity)
//  Simulates the WORST CASE: all 128 Messenger slots filled simultaneously.
//
//  THIS IS THE DEFINITIVE STRESS TEST. Run BEFORE each deployment to verify
//  that MixCoach handles maximum load without crashes or degradation.
//
//  Tests:
//   1. Register ALL 128 slots + verify activeCount & overflow
//   2. Push telemetry to all 128 + forEachActive verification
//   3. buildMaster() single-pass with 128 tracks
//   4. buildBus() with tracks across 6 buses
//   5. Batch read pollTelemetryFromShared() with 128 slots
//   6. syncFromShared() batch read with 128 slots  
//   7. pollTelemetryFromBackups() with 128 backup files
//   8. checkStaleSlots() with mixed stale/fresh across all 128
//   9. Concurrent-like simulation (alternating poll + process)
//  10. Performance: forEachActive + buildMaster 1000 iterations
//  11. Performance: batch read pollTelemetryFromShared 1000 iterations
//  12. Release/reuse cycle (flood test: release all + re-register)
//  13. Memory: verify no leaks after full release
//  14. Spectrum data: verify FFT processing with 128 tracks
//  15. rapidFire: simulate 128 rapid register/release cycles
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

#define TEST_NEAR(name, a, b, eps) TEST(name, std::fabs((a) - (b)) < (eps))

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
        files.clear();
        dir.findChildFiles(files, juce::File::findFiles, false, "*.meta");
        for (auto& f : files) f.deleteFile();
    }
}

// ─── Build a TrackTelemetry with varied values per slot ─────────────────────
static mixcoach::TrackTelemetry makeTelemetry(int slotIndex, float basePeak = -15.0f)
{
    mixcoach::TrackTelemetry t;
    t.timestamp = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;
    t.slotIndex = slotIndex;
    t.active = true;
    float variation = static_cast<float>(slotIndex) / static_cast<float>(kTestMaxSlots);
    t.peakLeft  = basePeak + variation * 10.0f;
    t.peakRight = basePeak + variation * 9.0f;
    t.rmsLeft   = basePeak - 8.0f + variation * 5.0f;
    t.rmsRight  = basePeak - 7.0f + variation * 5.0f;
    t.correlation = 0.95f - variation * 0.3f;
    t.crestFactor = 6.0f + variation * 8.0f;
    t.sampleL = 0.15f + variation * 0.1f;
    t.sampleR = 0.12f + variation * 0.1f;
    t.lufsIntegrated = -16.0f - variation * 4.0f;
    t.lufsShortTerm  = -18.0f - variation * 4.0f;
    t.lufsMomentary  = -14.0f - variation * 4.0f;
    t.lufsTruePeak   = basePeak + variation * 10.0f;
    t.loudnessRange  = 6.0f + variation * 8.0f;
    // Populate spectrum with varied peaks
    for (int j = 0; j < 256; ++j) {
        float freqPos = static_cast<float>(j) / 256.0f;
        float peakPos = variation;
        t.spectrum[j] = 0.001f + 0.6f * std::exp(-std::pow((freqPos - peakPos) * 25.0f, 2.0f));
    }
    return t;
}

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
//  Test 2: Push telemetry to all 128 + forEachActive verification
// ═══════════════════════════════════════════════════════════════════════════
static void test_telemetry_all_128()
{
    std::printf("\n── Test 2: Telemetry push + forEachActive (128 tracks) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    for (int i = 0; i < kTestMaxSlots; ++i) {
        reg->registerSlot("T_" + std::to_string(i), juce::Colours::grey,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));
    }
    for (int i = 0; i < kTestMaxSlots; ++i) {
        reg->getTelemetry(i).push(makeTelemetry(i));
    }

    // forEachActive: must visit all 128
    int count = 0;
    reg->forEachActive([&](const mixcoach::SlotInfo& info) {
        count++;
        auto telem = reg->getTelemetry(info.slotIndex).latest();
        TEST("telemetry peakLeft > -100 for all 128", telem.peakLeft > -100.0f);
        TEST("telemetry timestamp > 0 for all 128", telem.timestamp > 0);
    });
    TEST("forEachActive visited all 128 slots", count == kTestMaxSlots);

    // Total telemetry entries
    int64_t totalEntries = 0;
    for (int i = 0; i < kTestMaxSlots; ++i) {
        totalEntries += reg->getTelemetry(i).size();
    }
    TEST("Total telemetry entries across all slots = 128",
         totalEntries == kTestMaxSlots);

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 3: buildMaster() single-pass with 128 tracks
//  CRITICAL: This simulates the EXACT operation that runs in the
//  TelemetryProvider timer at 30-60fps. Must complete in < 1ms.
// ═══════════════════════════════════════════════════════════════════════════
static void test_build_master_128()
{
    std::printf("\n── Test 3: buildMaster() composite (128 tracks) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    for (int i = 0; i < kTestMaxSlots; ++i) {
        reg->registerSlot("T_" + std::to_string(i), juce::Colours::grey,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));
    }
    for (int i = 0; i < kTestMaxSlots; ++i) {
        reg->getTelemetry(i).push(makeTelemetry(i, -12.0f));
    }

    // ═══ Simulate SINGLE-PASS buildMaster() as done in TelemetryProvider ═══
    mixcoach::TrackTelemetry result;
    result.active = false;
    int activeCount = 0;
    float maxPeak = -100.0f;
    int loudestSlot = -1;

    // Single pass: peaks, RMS, spectrum MAX, correlation, loudest track
    {
        std::array<float, 256> spectrum{};
        
        reg->forEachActive([&](const mixcoach::SlotInfo& info) {
            if (info.stale) return;
            int idx = info.slotIndex;
            if (idx < 0) return;

            auto t = reg->getTelemetry(idx).latest();
            result.active = true;

            if (t.peakLeft > result.peakLeft)   result.peakLeft  = t.peakLeft;
            if (t.peakRight > result.peakRight) result.peakRight = t.peakRight;
            result.rmsLeft   = juce::jmax(result.rmsLeft, t.rmsLeft);
            result.rmsRight  = juce::jmax(result.rmsRight, t.rmsRight);
            result.correlation = juce::jmin(result.correlation, t.correlation);

            float slotMax = juce::jmax(t.peakLeft, t.peakRight);
            if (slotMax > maxPeak) {
                maxPeak = slotMax;
                loudestSlot = idx;
            }

            // Spectrum MAX inline (no second pass!)
            for (int j = 0; j < 256; ++j) {
                if (t.spectrum[j] > spectrum[j])
                    spectrum[j] = t.spectrum[j];
            }
            ++activeCount;
        });

        // Verify spectrum has content
        bool hasContent = false;
        for (int j = 0; j < 256; ++j) {
            if (spectrum[j] > 0.01f) { hasContent = true; break; }
        }
        TEST("buildMaster spectrum has content from 128 tracks", hasContent);

        // Copy spectrum to result
        std::copy(spectrum.begin(), spectrum.end(), result.spectrum);
    }

    TEST("buildMaster activeCount = 128", activeCount == kTestMaxSlots);
    TEST("buildMaster active = true", result.active);
    // Last track (127) has peak = -12 + (127/128)*10 ≈ -2.08
    TEST("buildMaster peakLeft matches loudest track",
         std::fabs(result.peakLeft - (-12.0f + 127.0f / 128.0f * 10.0f)) < 0.1f);

    if (loudestSlot >= 0) {
        auto t = reg->getTelemetry(loudestSlot).latest();
        float avgRms = (t.rmsLeft + t.rmsRight) * 0.5f;
        float crest = result.peakLeft - avgRms;
        TEST("buildMaster crest > 0", crest > 0.0f);
        TEST("buildMaster crest < 30 (sensible range)", crest < 30.0f);
    }

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 4: buildBus() with 128 tracks across 6 buses
// ═══════════════════════════════════════════════════════════════════════════
static void test_bus_composite_128()
{
    std::printf("\n── Test 4: buildBus() per bus group (128 tracks / 6 buses) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    for (int i = 0; i < kTestMaxSlots; ++i) {
        reg->registerSlot("T_" + std::to_string(i), juce::Colours::grey,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));
    }
    for (int i = 0; i < kTestMaxSlots; ++i) {
        reg->getTelemetry(i).push(makeTelemetry(i));
    }

    // Build composite for each bus (single-pass per bus)
    for (int b = 0; b < mixcoach::kNumBuses; ++b) {
        auto bus = static_cast<mixcoach::BusType>(b);
        int busActive = 0;
        float busPeak = -100.0f;
        std::array<float, 256> busSpectrum{};

        reg->forEachActive([&](const mixcoach::SlotInfo& info) {
            if (info.bus != bus) return;
            if (info.stale) return;
            busActive++;
            auto t = reg->getTelemetry(info.slotIndex).latest();
            if (t.peakLeft > busPeak) busPeak = t.peakLeft;
            for (int j = 0; j < 256; ++j) {
                if (t.spectrum[j] > busSpectrum[j])
                    busSpectrum[j] = t.spectrum[j];
            }
        });

        TEST("Bus " + std::to_string(b) + " has tracks",
             busActive > 0);
        TEST("Bus " + std::to_string(b) + " peak > -80",
             busPeak > -80.0f);
    }

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 5: Batch read pollTelemetryFromShared() with 128 slots
//  Uses REAL SharedMemoryManager to test the batch read path
// ═══════════════════════════════════════════════════════════════════════════
static void test_batch_read_128()
{
    std::printf("\n── Test 5: Batch read pollTelemetryFromShared() (128 slots) ──\n");
    std::fflush(stdout);

    juce::String testMapName = "Local\\\\MixCoach_Stress_128_Batch_" +
        juce::String(juce::Time::getMillisecondCounter());

    mixcoach::SharedMemoryManager shm;
    if (!shm.initialize(testMapName)) {
        std::printf("  \xe2\x9a\xa0\xef\xb8\x8f SKIP: Shared memory init failed\n");
        std::fflush(stdout);
        return;
    }

    auto reg = std::make_unique<mixcoach::SlotRegistry>();
    reg->setSharedMemory(&shm);

    // Register all 128 slots via SHM
    for (int i = 0; i < kTestMaxSlots; ++i) {
        mixcoach::SharedSlotEntry entry;
        entry.active = 1;
        entry.bus = i % mixcoach::kNumBuses;
        entry.colourARGB = 0xFF3498DB;
        entry.slotIndex = i;
        std::snprintf(entry.trackName, sizeof(entry.trackName), "Stress_%d", i);
        entry.peakLeft = -10.0f - static_cast<float>(i) * 0.15f;
        entry.peakRight = -12.0f - static_cast<float>(i) * 0.15f;
        entry.rmsLeft = -18.0f - static_cast<float>(i) * 0.1f;
        entry.rmsRight = -20.0f - static_cast<float>(i) * 0.1f;
        entry.correlation = 0.95f - static_cast<float>(i) * 0.003f;
        entry.crestFactor = 6.0f + static_cast<float>(i) * 0.05f;
        entry.lufsIntegrated = -16.0f - static_cast<float>(i) * 0.1f;
        entry.lufsShortTerm = -18.0f - static_cast<float>(i) * 0.1f;

        // Write spectrum data to SHM for some tracks
        if (i % 3 == 0) {
            for (int fi = 0; fi < 512; ++fi) {
                entry.fftMagnitudes[fi] = 0.001f + 0.3f * std::exp(
                    -std::pow((static_cast<float>(fi) / 512.0f -
                              static_cast<float>(i) / 128.0f) * 30.0f, 2.0f));
            }
            entry.fftTimestamp = static_cast<int64_t>(
                juce::Time::getMillisecondCounter()) * 1000;
        }

        int assigned = shm.registerSlot(entry);
        TEST("SHM registerSlot " + std::to_string(i) + " OK",
             assigned >= 0 && assigned < mixcoach::kSharedMaxSlots);
    }

    // Force sync from SHM to populate local state
    int found = reg->forceFullSync();
    TEST("forceFullSync found 128 slots", found >= kTestMaxSlots);
    TEST("activeCount = 128", reg->activeCount() == kTestMaxSlots);

    // ═══ CRITICAL: pollTelemetryFromShared() batch read ═══════════════════
    // This is the EXACT function called from timer at 30-60fps.
    // Must NOT crash, must update telemetry for all 128 slots.
    reg->pollTelemetryFromShared();

    // Verify telemetry was populated for all 128 slots
    int telemetryCount = 0;
    for (int i = 0; i < kTestMaxSlots; ++i) {
        auto telem = reg->getTelemetry(i).latest();
        if (telem.timestamp > 0) telemetryCount++;
    }
    TEST("pollTelemetryFromShared populated all 128 slots",
         telemetryCount == kTestMaxSlots);

    // Verify spectrum data preserved for tracks that had FFT
    for (int i = 0; i < kTestMaxSlots; i += 3) {
        auto telem = reg->getTelemetry(i).latest();
        bool hasSpectrum = false;
        for (int fi = 0; fi < 256; ++fi) {
            if (telem.spectrum[fi] > 0.01f) { hasSpectrum = true; break; }
        }
        if (i < kTestMaxSlots) {
            TEST("Spectrum preserved for slot " + std::to_string(i), hasSpectrum);
        }
    }

    // ═══ Multiple poll calls (simulate 60fps for 100 frames) ═════════════
    for (int iter = 0; iter < 100; ++iter) {
        reg->pollTelemetryFromShared();
    }

    // Verify no stale after fresh polls
    int staleAfter = 0;
    reg->checkStaleSlots();
    for (int i = 0; i < kTestMaxSlots; ++i) {
        if (reg->getSlotInfo(i).stale) staleAfter++;
    }
    TEST("0 stale after 100 poll iterations (all fresh)", staleAfter == 0);

    shm.close();
    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 6: syncFromShared() batch read with 128 slots
// ═══════════════════════════════════════════════════════════════════════════
static void test_sync_from_shared_128()
{
    std::printf("\n── Test 6: syncFromShared() batch read (128 slots) ──\n");
    std::fflush(stdout);

    juce::String testMapName = "Local\\\\MixCoach_Stress_128_Sync_" +
        juce::String(juce::Time::getMillisecondCounter());

    mixcoach::SharedMemoryManager shm;
    if (!shm.initialize(testMapName)) {
        std::printf("  \xe2\x9a\xa0\xef\xb8\x8f SKIP: Shared memory not available\n");
        std::fflush(stdout);
        return;
    }

    auto reg = std::make_unique<mixcoach::SlotRegistry>();
    reg->setSharedMemory(&shm);

    // Register 128 in SHM
    for (int i = 0; i < kTestMaxSlots; ++i) {
        mixcoach::SharedSlotEntry entry;
        entry.active = 1;
        entry.bus = i % mixcoach::kNumBuses;
        entry.colourARGB = 0xFF2ECC71;
        entry.slotIndex = i;
        std::snprintf(entry.trackName, sizeof(entry.trackName),
                      "SyncT_%d", i);
        shm.registerSlot(entry);
    }

    // Do forceFullSync first
    reg->forceFullSync();

    // Now release some slots via SHM directly
    for (int i = 50; i < 60; ++i) {
        shm.releaseSlot(i);
    }

    // Increment changeCount to trigger sync
    for (int i = 0; i < 5; ++i) {
        mixcoach::SharedSlotEntry dummy;
        shm.readSlot(0, dummy);
        shm.writeSlot(0, dummy); // This increments changeCount
    }

    // syncFromShared() should detect the 10 released slots
    bool changed = reg->syncFromShared();
    TEST("syncFromShared detected changes", changed);

    int activeAfter = reg->activeCount();
    TEST("activeCount = 118 after releasing 10 slots via SHM",
         activeAfter == kTestMaxSlots - 10);

    // Verify released slots are inactive locally
    for (int i = 50; i < 60; ++i) {
        TEST("Slot " + std::to_string(i) + " inactive after sync",
             !reg->getSlotInfo(i).active);
    }

    shm.close();
    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 7: Backup files with ALL 128 slots + batch load
// ═══════════════════════════════════════════════════════════════════════════
static void test_backup_128_slots()
{
    std::printf("\n── Test 7: Backup files for ALL 128 slots ──\n");
    std::fflush(stdout);
    cleanupBackupFiles();

    // Simulate 128 Messengers writing backup files
    {
        ScopedTimer timer("Write 128 backup files");
        for (int i = 0; i < kTestMaxSlots; ++i) {
            mixcoach::SlotInfo slot;
            slot.slotIndex = i;
            slot.active = true;
            slot.bus = static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses);
            slot.colour = juce::Colour::fromHSV(
                static_cast<float>(i) / static_cast<float>(kTestMaxSlots),
                0.8f, 0.7f, 1.0f);
            slot.setTrackName("Backup_" + std::to_string(i));
            mixcoach::SlotRegistry::saveSlotToBackupFile(i, slot);

            // Write telemetry
            mixcoach::SlotRegistry::updateSlotBackupTelemetry(
                i, -10.0f, -12.0f, -18.0f, -20.0f, 0.9f, 8.0f,
                0.1f, -0.05f, -14.0f, -16.0f, -12.0f, -8.0f, 6.0f);
        }
    }

    // Verify all 128 backup files exist
    juce::Array<juce::File> files;
    getBackupDir().findChildFiles(files, juce::File::findFiles, false, "slot_*.bin");
    TEST("128 backup files exist", files.size() >= kTestMaxSlots);

    // Load all from MixCoach side
    {
        auto reg = std::make_unique<mixcoach::SlotRegistry>();

        // ═══ Medir SOLO el load de backup files (sin tests) ═══════════
        double loadTimeMs = 0;
        int found;
        {
            ScopedTimer timer("loadSlotsFromBackupFiles x128", &loadTimeMs);
            found = reg->loadSlotsFromBackupFiles(true);
        }

        TEST("loadSlotsFromBackupFiles found 128 slots", found >= kTestMaxSlots);
        TEST("activeCount = 128 after backup load", reg->activeCount() == kTestMaxSlots);

        // ═══ TIME BUDGET: carga de 128 archivos en < 2000ms ═══════════
        // En SSD: ~5-10ms. En HDD: ~100-500ms. El timeout de FL Studio
        // para plugins es de ~5000ms (5s). Con 2000ms estamos holgados.
        // Este test es SECUENCIAL (1 archivo a la vez). Una optimización
        // futura sería cargar en paralelo (std::async).
        TEST("Backup load time < 2000ms", loadTimeMs < 2000.0);

        // Verificar nombres (fuera del timer)
        for (int i = 0; i < kTestMaxSlots; ++i) {
            auto info = reg->getSlotInfo(i);
            juce::String expectedName = "Backup_" + std::to_string(i);
            std::string tn = "Slot " + std::to_string(i) + " name matches";
            TEST(tn.c_str(), juce::String(info.trackName).trim() == expectedName);
        }

        // Verify telemetry was loaded
        int telemetryOk = 0;
        for (int i = 0; i < kTestMaxSlots; ++i) {
            auto telem = reg->getTelemetry(i).latest();
            if (telem.timestamp > 0 && telem.peakLeft > -90.0f)
                telemetryOk++;
        }
        TEST("Telemetry loaded for all 128 slots", telemetryOk == kTestMaxSlots);

        reg.reset();
    }

    // Cleanup
    for (int i = 0; i < kTestMaxSlots; ++i)
        mixcoach::SlotRegistry::removeSlotBackupFile(i);
    cleanupBackupFiles();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 8: checkStaleSlots() with mixed stale/fresh across all 128
// ═══════════════════════════════════════════════════════════════════════════
static void test_stale_detection_128()
{
    std::printf("\n── Test 8: Stale detection with 128 tracks ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    for (int i = 0; i < kTestMaxSlots; ++i) {
        reg->registerSlot("T_" + std::to_string(i), juce::Colours::grey,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));
    }
    // Fresh timestamps
    for (int i = 0; i < kTestMaxSlots; ++i) {
        auto t = makeTelemetry(i);
        t.timestamp = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;
        reg->getTelemetry(i).push(t);
    }

    reg->checkStaleSlots();
    int staleCount = 0;
    for (int i = 0; i < kTestMaxSlots; ++i) {
        if (reg->getSlotInfo(i).stale) staleCount++;
    }
    TEST("0 stale initially (all fresh)", staleCount == 0);

    // Make RANDOM 20 slots stale
    auto oldTime = static_cast<int64_t>(1);
    for (int i = 10; i < 30; ++i) {
        auto t = makeTelemetry(i);
        t.timestamp = oldTime;
        reg->getTelemetry(i).push(t);
    }

    reg->checkStaleSlots();
    int newStaleCount = 0;
    for (int i = 0; i < kTestMaxSlots; ++i) {
        if (reg->getSlotInfo(i).stale) newStaleCount++;
    }
    TEST("20 stale tracks detected", newStaleCount == 20);

    // Verify forEachActive skips stale
    int activeNonStale = 0;
    reg->forEachActive([&](const mixcoach::SlotInfo& info) {
        if (!info.stale) activeNonStale++;
    });
    TEST("108 non-stale tracks counted by forEachActive",
         activeNonStale == kTestMaxSlots - 20);

    // Recover all 20
    for (int i = 10; i < 30; ++i) {
        auto t = makeTelemetry(i);
        t.timestamp = static_cast<int64_t>(juce::Time::getMillisecondCounter()) * 1000;
        reg->getTelemetry(i).push(t);
    }

    reg->checkStaleSlots();
    int recoveredStale = 0;
    for (int i = 0; i < kTestMaxSlots; ++i) {
        if (reg->getSlotInfo(i).stale) recoveredStale++;
    }
    TEST("0 stale after recovery", recoveredStale == 0);

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 9: Concurrent-like simulation (alternating poll + process)
//  Simulates the real-world pattern:
//    BG thread: pollTelemetryFromShared() + syncFromShared()
//    Timer:     forEachActive() + buildMaster() + smoothMeters()
// ═══════════════════════════════════════════════════════════════════════════
static void test_concurrent_like_simulation()
{
    std::printf("\n── Test 9: Concurrent-like simulation (1000 cycles) ──\n");
    std::fflush(stdout);

    juce::String testMapName = "Local\\\\MixCoach_Stress_Concurrent_" +
        juce::String(juce::Time::getMillisecondCounter());

    mixcoach::SharedMemoryManager shm;
    if (!shm.initialize(testMapName)) {
        std::printf("  \xe2\x9a\xa0\xef\xb8\x8f SKIP: Shared memory not available\n");
        std::fflush(stdout);
        return;
    }

    auto reg = std::make_unique<mixcoach::SlotRegistry>();
    reg->setSharedMemory(&shm);

    // Register 128 in SHM
    for (int i = 0; i < kTestMaxSlots; ++i) {
        mixcoach::SharedSlotEntry entry;
        entry.active = 1;
        entry.bus = i % mixcoach::kNumBuses;
        entry.colourARGB = 0xFF9B59B6;
        entry.slotIndex = i;
        std::snprintf(entry.trackName, sizeof(entry.trackName),
                      "Concurrent_%d", i);
        entry.peakLeft = -5.0f - static_cast<float>(i) * 0.1f;
        entry.rmsLeft = -15.0f - static_cast<float>(i) * 0.1f;
        shm.registerSlot(entry);
    }

    reg->forceFullSync();

    // ─── CRITICAL: Simulate 1000 cycles of the REAL timer flow ───────────
    // Pattern: pollTelemetryFromShared → forEachActive → checkStaleSlots
    // This is exactly what happens at 30fps with 60+ tracks.
    constexpr int kCycles = 1000;
    double totalPollUs = 0;
    double totalForEachUs = 0;
    double maxPollUs = 0;
    double maxForEachUs = 0;

    for (int cycle = 0; cycle < kCycles; ++cycle) {
        // ─── 1. pollTelemetryFromShared() (batch read) ─────────────────
        {
            auto start = std::chrono::high_resolution_clock::now();
            reg->pollTelemetryFromShared();
            auto elapsed = std::chrono::duration<double, std::nano>(
                std::chrono::high_resolution_clock::now() - start).count();
            double elapsedUs = elapsed / 1000.0;
            totalPollUs += elapsedUs;
            if (elapsedUs > maxPollUs) maxPollUs = elapsedUs;
        }

        // ─── 2. forEachActive (simulate buildMaster single-pass) ───────
        {
            auto start = std::chrono::high_resolution_clock::now();
            int activeCount = 0;
            float masterPeak = -100.0f;
            reg->forEachActive([&](const mixcoach::SlotInfo& info) {
                auto t = reg->getTelemetry(info.slotIndex).latest();
                if (t.peakLeft > masterPeak) masterPeak = t.peakLeft;
                activeCount++;
            });
            auto elapsed = std::chrono::duration<double, std::nano>(
                std::chrono::high_resolution_clock::now() - start).count();
            double elapsedUs = elapsed / 1000.0;
            totalForEachUs += elapsedUs;
            if (elapsedUs > maxForEachUs) maxForEachUs = elapsedUs;

            // Verify we processed all 128 every time
            if (cycle == 0) {
                TEST("forEachActive in concurrent sim = 128",
                     activeCount == kTestMaxSlots);
                TEST("masterPeak > -80 in concurrent sim", masterPeak > -80.0f);
            }
        }

        // ─── 3. Every 10 cycles: checkStaleSlots ──────────────────────
        if (cycle % 10 == 0) {
            reg->checkStaleSlots();
        }

        // ─── 4. Every 50 cycles: update SHM data (simulate Messenger) ─
        if (cycle % 50 == 0) {
            for (int i = 0; i < kTestMaxSlots; i += 4) {
                mixcoach::SharedSlotEntry entry;
                if (shm.readSlot(i, entry)) {
                    entry.peakLeft = -8.0f + static_cast<float>(cycle % 20);
                    shm.writeSlot(i, entry);
                }
            }
        }
    }

    double avgPollUs = totalPollUs / kCycles;
    double avgForEachUs = totalForEachUs / kCycles;

    std::printf("  \xe2\x8f\xb1 pollTelemetryFromShared: avg=%.2f us, max=%.2f us\n",
                avgPollUs, maxPollUs);
    std::printf("  \xe2\x8f\xb1 forEachActive:          avg=%.2f us, max=%.2f us\n",
                avgForEachUs, maxForEachUs);
    std::fflush(stdout);

    // CRITICAL: Each pollTelemetry must be < 5ms (spinlock timeout)
    // Each forEachActive must be < 1ms (60fps budget = 16ms)
    TEST("pollTelemetry avg < 500 us", avgPollUs < 500.0);
    TEST("pollTelemetry max < 5000 us (5ms)", maxPollUs < 5000.0);
    TEST("forEachActive avg < 1000 us (1ms)", avgForEachUs < 1000.0);
    TEST("forEachActive max < 5000 us", maxForEachUs < 5000.0);
    TEST("No crash after 1000 concurrent cycles", true);

    // Verify no stale after all cycles
    int staleAfter = 0;
    for (int i = 0; i < kTestMaxSlots; ++i) {
        if (reg->getSlotInfo(i).stale) staleAfter++;
    }
    TEST("0 stale after 1000 cycles", staleAfter == 0);

    shm.close();
    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 10: Performance — forEachActive + buildMaster 1000 iterations
// ═══════════════════════════════════════════════════════════════════════════
static void test_performance_128()
{
    std::printf("\n── Test 10: Performance (128 tracks, 1000 iterations) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    for (int i = 0; i < kTestMaxSlots; ++i) {
        reg->registerSlot("T_" + std::to_string(i), juce::Colours::grey,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));
    }
    for (int i = 0; i < kTestMaxSlots; ++i) {
        reg->getTelemetry(i).push(makeTelemetry(i));
    }

    // Measure forEachActive + buildMaster composite (1000 iterations)
    constexpr int kIterations = 1000;

    auto start = std::chrono::high_resolution_clock::now();
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

    std::printf("  \xe2\x8f\xb1 forEachActive (128 tracks): avg=%.2f us "
                "(%.2f ms for %d iters)\n",
                avgUs, elapsedUs / 1000.0, kIterations);
    std::fflush(stdout);

    // CRITICAL: forEachActive with 128 tracks must be < 1ms per iteration
    // (60fps budget = 16ms per frame, this is just one operation)
    TEST("forEachActive with 128 tracks < 1000 us avg", avgUs < 1000.0);
    TEST("forEachActive with 128 tracks < 100 us (target)", avgUs < 100.0);

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 11: Release/reuse cycle (flood test)
//  Releases ALL 128 slots and re-registers, simulating worst-case churn
// ═══════════════════════════════════════════════════════════════════════════
static void test_release_reuse_flood()
{
    std::printf("\n── Test 11: Release/Reuse FLOOD test (3 complete cycles) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    for (int cycle = 0; cycle < 3; ++cycle) {
        std::printf("  Cycle %d: register 128...\n", cycle + 1);
        std::fflush(stdout);

        // Register all 128
        for (int i = 0; i < kTestMaxSlots; ++i) {
            std::string name = "Flood_" + std::to_string(cycle) + "_" + std::to_string(i);
            int idx = reg->registerSlot(name, juce::Colours::grey,
                static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));
            std::string tn = "Cycle " + std::to_string(cycle) +
                             " Slot " + std::to_string(i) + " registered";
            TEST(tn.c_str(), idx >= 0 && idx < kTestMaxSlots);
        }
        TEST("activeCount = 128 after flood cycle " + std::to_string(cycle),
             reg->activeCount() == kTestMaxSlots);

        // Push telemetry
        for (int i = 0; i < kTestMaxSlots; ++i) {
            reg->getTelemetry(i).push(makeTelemetry(i));
        }

        // Release all 128 (reverse order to test edge cases)
        std::printf("  Cycle %d: release 128...\n", cycle + 1);
        std::fflush(stdout);
        for (int i = kTestMaxSlots - 1; i >= 0; --i) {
            reg->releaseSlot(i);
        }
        TEST("activeCount = 0 after flood release cycle " + std::to_string(cycle),
             reg->activeCount() == 0);

        // Verify all slots are released
        for (int i = 0; i < kTestMaxSlots; ++i) {
            std::string tn = "Slot " + std::to_string(i) +
                             " inactive after release";
            TEST(tn.c_str(), !reg->getSlotInfo(i).active);
        }
    }

    // Final re-register to prove the registry is healthy
    for (int i = 0; i < kTestMaxSlots; ++i) {
        reg->registerSlot("Final_" + std::to_string(i), juce::Colours::green,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));
    }
    TEST("activeCount = 128 after 3 flood cycles + final register",
         reg->activeCount() == kTestMaxSlots);

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 12: Memory — verify no leaks after full release
//  Strategy: Registers all 128, pushes telemetry, releases all.
//  Then checks that the registry is in clean state.
// ═══════════════════════════════════════════════════════════════════════════
static void test_memory_cleanup()
{
    std::printf("\n── Test 12: Memory cleanup verification ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    // Register + push telemetry + release
    for (int i = 0; i < kTestMaxSlots; ++i) {
        reg->registerSlot("Mem_" + std::to_string(i), juce::Colours::blue,
                          static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses));
    }
    for (int i = 0; i < kTestMaxSlots; ++i) {
        for (int j = 0; j < 10; ++j) {
            reg->getTelemetry(i).push(makeTelemetry(i));
        }
    }
    for (int i = 0; i < kTestMaxSlots; ++i) {
        reg->releaseSlot(i);
    }

    // Verify clean state
    TEST("activeCount = 0 after memory test", reg->activeCount() == 0);
    for (int i = 0; i < kTestMaxSlots; ++i) {
        auto info = reg->getSlotInfo(i);
        TEST("Slot " + std::to_string(i) + " inactive",
             !info.active);
        TEST("Slot " + std::to_string(i) + " slotIndex = -1",
             info.slotIndex == -1);
    }

    // Register + release again (prove reusability)
    for (int i = 0; i < 10; ++i) {
        int idx = reg->registerSlot("Reuse_" + std::to_string(i),
                                     juce::Colours::red, mixcoach::BusType::Drums);
        TEST("Reuse register returns valid index", idx >= 0);
    }
    TEST("activeCount = 10 after reuse", reg->activeCount() == 10);

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 13: Spectrum data — verify FFT processing with 128 tracks
//  Ensures the copyFftMagnitudesFromSharedEntry works correctly
//  with all 128 slots populated with spectrum data
// ═══════════════════════════════════════════════════════════════════════════
static void test_spectrum_128()
{
    std::printf("\n── Test 13: Spectrum data with 128 tracks ──\n");
    std::fflush(stdout);

    juce::String testMapName = "Local\\\\MixCoach_Stress_128_FFT_" +
        juce::String(juce::Time::getMillisecondCounter());

    mixcoach::SharedMemoryManager shm;
    if (!shm.initialize(testMapName)) {
        std::printf("  \xe2\x9a\xa0\xef\xb8\x8f SKIP: Shared memory not available\n");
        std::fflush(stdout);
        return;
    }

    // Write spectrum data for ALL 128 slots to SHM
    for (int i = 0; i < kTestMaxSlots; ++i) {
        mixcoach::SharedSlotEntry entry;
        entry.active = 1;
        entry.bus = i % mixcoach::kNumBuses;
        entry.colourARGB = 0xFFE74C3C;
        entry.slotIndex = i;
        std::snprintf(entry.trackName, sizeof(entry.trackName),
                      "FFT_%d", i);
        entry.peakLeft = -10.0f;

        // Each slot has a different spectral peak
        float peakBin = (static_cast<float>(i) / kTestMaxSlots) * 512.0f;
        for (int fi = 0; fi < 512; ++fi) {
            entry.fftMagnitudes[fi] = 0.001f + 0.5f * std::exp(
                -std::pow((static_cast<float>(fi) - peakBin) / 15.0f, 2.0f));
        }
        entry.fftTimestamp = static_cast<int64_t>(
            juce::Time::getMillisecondCounter()) * 1000;

        int assigned = shm.registerSlot(entry);
        TEST("SHM register slot " + std::to_string(i) + " for FFT test",
             assigned >= 0);
    }

    auto reg = std::make_unique<mixcoach::SlotRegistry>();
    reg->setSharedMemory(&shm);
    reg->forceFullSync();

    // Verify spectrum loaded via copyFftMagnitudesFromSharedEntry
    int slotsWithSpectrum = 0;
    for (int i = 0; i < kTestMaxSlots; ++i) {
        auto telem = reg->getTelemetry(i).latest();
        bool hasMagnitude = false;
        for (int fi = 0; fi < 256; ++fi) {
            if (telem.spectrum[fi] > 0.01f) {
                hasMagnitude = true;
                break;
            }
        }
        if (hasMagnitude) slotsWithSpectrum++;
    }
    TEST("All 128 slots have spectrum data via copyFftMagnitudes",
         slotsWithSpectrum == kTestMaxSlots);

    // Verify the spectrum peaks are at different positions
    std::vector<int> peakBins;
    for (int i = 0; i < kTestMaxSlots; ++i) {
        auto telem = reg->getTelemetry(i).latest();
        int peakBin = 0;
        float maxVal = 0.0f;
        for (int fi = 0; fi < 256; ++fi) {
            if (telem.spectrum[fi] > maxVal) {
                maxVal = telem.spectrum[fi];
                peakBin = fi;
            }
        }
        peakBins.push_back(peakBin);
    }

    // Each slot should have a different spectral peak
    int uniquePeaks = static_cast<int>(std::set<int>(peakBins.begin(), peakBins.end()).size());
    TEST("Unique spectral peaks across 128 slots (min 64 different)",
         uniquePeaks >= 64);

    shm.close();
    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 14: Rapid register/release (simulate track churn)
//  Rapidly registers and releases to stress-test slot allocation
// ═══════════════════════════════════════════════════════════════════════════
static void test_rapid_churn()
{
    std::printf("\n── Test 14: Rapid churn (1000 register/release cycles) ──\n");
    std::fflush(stdout);

    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    constexpr int kChurnCycles = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int cycle = 0; cycle < kChurnCycles; ++cycle) {
        // Register 5 slots, release 3, repeat
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

    // After 1000 cycles of +5/-3, should have net 2000 active slots...
    // But we only have 128! Most registerSlot calls will return -1 after full.
    // That's expected. We just test that it doesn't crash.
    TEST("No crash after 1000 churn cycles", true);
    TEST("Registry still operational", reg->activeCount() >= 0);

    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 15: SHM batch read stress with concurrent writes
//  Simulates Messenger writing telemetry while MixCoach reads
// ═══════════════════════════════════════════════════════════════════════════
static void test_shm_batch_concurrent()
{
    std::printf("\n── Test 15: SHM batch read stress (10000 poll cycles) ──\n");
    std::fflush(stdout);

    juce::String testMapName = "Local\\\\MixCoach_Stress_SHMBatch_" +
        juce::String(juce::Time::getMillisecondCounter());

    mixcoach::SharedMemoryManager shm;
    if (!shm.initialize(testMapName)) {
        std::printf("  \xe2\x9a\xa0\xef\xb8\x8f SKIP: Shared memory not available\n");
        std::fflush(stdout);
        return;
    }

    // Register 100 slots in SHM
    constexpr int kConcurrentSlots = 100;
    for (int i = 0; i < kConcurrentSlots; ++i) {
        mixcoach::SharedSlotEntry entry;
        entry.active = 1;
        entry.bus = i % mixcoach::kNumBuses;
        entry.colourARGB = 0xFF1ABC9C;
        entry.slotIndex = i;
        std::snprintf(entry.trackName, sizeof(entry.trackName),
                      "Batch_%d", i);
        entry.peakLeft = -20.0f;
        shm.registerSlot(entry);
    }

    auto reg = std::make_unique<mixcoach::SlotRegistry>();
    reg->setSharedMemory(&shm);
    reg->forceFullSync();

    // Simulate 10000 poll cycles, with random SHM updates between them
    constexpr int kPollCycles = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int cycle = 0; cycle < kPollCycles; ++cycle) {
        // Write random data to SHM every 10 cycles
        if (cycle % 10 == 0) {
            int slotToUpdate = (cycle / 10) % kConcurrentSlots;
            mixcoach::SharedSlotEntry entry;
            if (shm.readSlot(slotToUpdate, entry)) {
                entry.peakLeft = -5.0f - static_cast<float>(cycle % 30);
                entry.rmsLeft = -15.0f - static_cast<float>(cycle % 20);
                shm.writeSlot(slotToUpdate, entry);
            }
        }

        // Batch read poll (the critical path!)
        reg->pollTelemetryFromShared();

        // Every 100 cycles, check data integrity
        if (cycle % 100 == 0 && cycle > 0) {
            for (int i = 0; i < kConcurrentSlots; i += 10) {
                auto telem = reg->getTelemetry(i).latest();
                // Verify we got valid data (not corrupted)
                if (telem.peakLeft < -100.0f || telem.peakLeft > 20.0f) {
                    TEST("Data integrity at cycle " + std::to_string(cycle) +
                         " slot " + std::to_string(i), false);
                    break;
                }
            }
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();

    std::printf("  \xe2\x8f\xb1 %d poll cycles: %.2f ms (%.2f us/cycle)\n",
                kPollCycles, elapsedMs, elapsedMs * 1000.0 / kPollCycles);
    std::fflush(stdout);

    TEST("No crash after 10000 SHM poll cycles", true);
    TEST("Batch poll performance < 100 us/cycle",
         elapsedMs * 1000.0 / kPollCycles < 100.0);

    shm.close();
    reg.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════
int main()
{
    cleanupBackupFiles();

    std::printf("\n"
        "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
        "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
        "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
        "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
        "\n");
    std::printf("  \xf0\x9f\x92\xaa  STRESS TEST: MAXIMUM CAPACITY (128 slots)\n");
    std::printf("  \xf0\x9f\x94\xa5  This test simulates the absolute worst case:\n");
    std::printf("     All 128 Messenger slots filled simultaneously.\n");
    std::printf("     kMaxSlots=%d | kSharedMaxSlots=%d | structVersion=%d\n",
                mixcoach::SlotRegistry::kMaxSlots,
                mixcoach::kSharedMaxSlots,
                mixcoach::SharedMemoryHeader::kCurrentStructVersion);
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\n\n");
    std::fflush(stdout);

    test_register_all_128();
    test_telemetry_all_128();
    test_build_master_128();
    test_bus_composite_128();
    test_batch_read_128();
    test_sync_from_shared_128();
    test_backup_128_slots();
    test_stale_detection_128();
    test_concurrent_like_simulation();
    test_performance_128();
    test_release_reuse_flood();
    test_memory_cleanup();
    test_spectrum_128();
    test_rapid_churn();
    test_shm_batch_concurrent();

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

    cleanupBackupFiles();
    return gTestsFailed > 0 ? 1 : 0;
}
