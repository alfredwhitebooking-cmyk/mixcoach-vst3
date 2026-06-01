// ═══════════════════════════════════════════════════════════════════════════
//  TestIPCIntegration.cpp — Integration test for the IPC pipeline
//  Tests the complete data flow: SlotRegistry → Backup Files → Shared Memory
// ═══════════════════════════════════════════════════════════════════════════
//
// Pipeline under test:
//   Messenger writes → saveSlotToBackupFile() + updateSlotBackupTelemetry()
//                    → backup file on disk (slot_N.bin)
//   MixCoach reads  → loadSlotsFromBackupFiles() → SlotRegistry
//   IPC real-time   → SharedMemoryManager (CreateFileMappingW)
//
// NOTE: SlotRegistry is large (~11MB due to TelemetryBuffer arrays).
// Always allocate on heap (std::make_unique) to avoid stack overflow.

#include <cstdio>
#include <cmath>
#include <cstring>
#include <memory>

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

// ─── Helpers ────────────────────────────────────────────────────────────────
static juce::File getBackupDir() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("MixCoach").getChildFile("SlotBackup");
}

static void cleanupBackupFiles() {
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

// ═══════════════════════════════════════════════════════════════════════════
//  Test 1: Backup File Roundtrip
//  Writes a slot via saveSlotToBackupFile(), reads it back via
//  loadSlotsFromBackupFiles(), verifies ALL fields match.
// ═══════════════════════════════════════════════════════════════════════════
static void test_backup_file_roundtrip() {
    std::printf("\n── Test 1: Backup File Roundtrip ──\n");
    std::fflush(stdout);
    cleanupBackupFiles();

    auto registry = std::make_unique<mixcoach::SlotRegistry>();

    mixcoach::SlotInfo original;
    original.slotIndex = 0;
    original.active = true;
    original.bus = mixcoach::BusType::Drums;
    original.colour = juce::Colour(0xFFFF5000);
    original.setTrackName("Bateria");
    mixcoach::SlotRegistry::saveSlotToBackupFile(0, original);

    auto backupFile = getBackupDir().getChildFile("slot_0.bin");
    TEST("Backup file exists after save", backupFile.existsAsFile());
    TEST("Backup file has V2 size (>= 140 bytes)",
         backupFile.getSize() >= static_cast<juce::int64>(140));

    int found = registry->loadSlotsFromBackupFiles(true);
    TEST("loadSlotsFromBackupFiles found the slot", found >= 1);

    auto loaded = registry->getSlotInfo(0);
    TEST("Slot active", loaded.active);
    TEST("Slot index", loaded.slotIndex == 0);
    TEST("Track name: Bateria",
         juce::String(loaded.trackName).trim() == "Bateria");
    TEST("Bus: Drums", loaded.bus == mixcoach::BusType::Drums);
    TEST("Colour matches",
         loaded.colour.getARGB() == juce::Colour(0xFFFF5000).getARGB());

    registry.reset();
    cleanupBackupFiles();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 2: Telemetry Update in Backup Files
//  Writes slot → updates telemetry → reloads → verifies telemetry values
// ═══════════════════════════════════════════════════════════════════════════
static void test_backup_telemetry_update() {
    std::printf("\n── Test 2: Backup Telemetry Update ──\n");
    std::fflush(stdout);
    cleanupBackupFiles();

    auto registry = std::make_unique<mixcoach::SlotRegistry>();

    // Write initial slot
    mixcoach::SlotInfo slot;
    slot.slotIndex = 1;
    slot.active = true;
    slot.bus = mixcoach::BusType::Bass;
    slot.colour = juce::Colour(0xFF3498DB);
    slot.setTrackName("Bajo");
    mixcoach::SlotRegistry::saveSlotToBackupFile(1, slot);

    // Update telemetry (simulating Messenger processBlock)
    mixcoach::SlotRegistry::updateSlotBackupTelemetry(1,
        -6.0f, -8.0f, -18.0f, -20.0f, 0.85f, 12.0f,
        0.25f, -0.15f, -14.0f, -16.0f, -12.0f, -8.0f, 6.0f);

    // Load from backup with forceOverwrite
    int found = registry->loadSlotsFromBackupFiles(true);
    TEST("loadSlotsFromBackupFiles found telemetry slot", found >= 1);

    // Metadata should survive
    auto loaded = registry->getSlotInfo(1);
    TEST("Telemetry slot name preserved: Bajo",
         juce::String(loaded.trackName).trim() == "Bajo");
    TEST("Telemetry slot bus preserved: Bass",
         loaded.bus == mixcoach::BusType::Bass);

    // Telemetry values should be in the ring buffer
    auto& telem = registry->getTelemetry(1);
    int64_t sz = telem.size();
    TEST("Telemetry buffer has entries", sz > 0);
    if (sz > 0) {
        auto latest = telem.latest();
        TEST("Telemetry peakLeft matches (-6.0f)",
             std::fabs(latest.peakLeft - (-6.0f)) < 0.001f);
        TEST("Telemetry peakRight matches (-8.0f)",
             std::fabs(latest.peakRight - (-8.0f)) < 0.001f);
        TEST("Telemetry rmsLeft matches (-18.0f)",
             std::fabs(latest.rmsLeft - (-18.0f)) < 0.001f);
        TEST("Telemetry rmsRight matches (-20.0f)",
             std::fabs(latest.rmsRight - (-20.0f)) < 0.001f);
        TEST("Telemetry correlation matches (0.85f)",
             std::fabs(latest.correlation - 0.85f) < 0.001f);
        TEST("Telemetry lufsIntegrated roughly matches (-14.0f)",
             std::fabs(latest.lufsIntegrated - (-14.0f)) < 1.0f);
    }

    registry.reset();
    cleanupBackupFiles();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 3: Remove Backup File
//  Saves → removes → verifies file gone
// ═══════════════════════════════════════════════════════════════════════════
static void test_remove_backup_file() {
    std::printf("\n── Test 3: Remove Backup File ──\n");
    std::fflush(stdout);
    cleanupBackupFiles();

    mixcoach::SlotInfo slot;
    slot.slotIndex = 2;
    slot.active = true;
    slot.setTrackName("TempTrack");
    mixcoach::SlotRegistry::saveSlotToBackupFile(2, slot);

    auto backupFile = getBackupDir().getChildFile("slot_2.bin");
    TEST("Backup file exists before remove", backupFile.existsAsFile());

    mixcoach::SlotRegistry::removeSlotBackupFile(2);
    TEST("Backup file removed after removeSlotBackupFile",
         !backupFile.existsAsFile());

    cleanupBackupFiles();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 4: SharedMemoryManager Basic Operations
//  Creates SharedMemoryManager → registers slot → reads back → releases
//  Uses unique map name per run to avoid conflicts.
// ═══════════════════════════════════════════════════════════════════════════
static void test_shared_memory_basic() {
    std::printf("\n── Test 4: SharedMemory Basic ──\n");
    std::fflush(stdout);

    juce::String testMapName = "Local\\MixCoach_Test_" +
        juce::String(juce::Time::getMillisecondCounter());

    mixcoach::SharedMemoryManager shm;
    bool initialized = shm.initialize(testMapName);
    TEST("SharedMemoryManager initializes", initialized);
    TEST("SharedMemoryManager reports initialized", shm.isInitialized());

    if (!initialized) {
        std::printf("  \xe2\x9a\xa0\xef\xb8\x8f SKIP: remaining shared memory tests\n");
        std::fflush(stdout);
        return;
    }

    auto* block = shm.getBlock();
    TEST("Shared memory block is accessible", block != nullptr);
    if (!block) { shm.close(); return; }

    TEST("Header initialized flag set", block->header.initialized == 1);
    TEST("Struct version matches",
         block->header.structVersion == mixcoach::SharedMemoryHeader::kCurrentStructVersion);
    TEST("Change count starts at 0", shm.getChangeCount() == 0);

    // Register a slot
    mixcoach::SharedSlotEntry entry;
    entry.active = 1;
    entry.bus = 2;
    entry.colourARGB = 0xFF2ECC71;
    std::strncpy(entry.trackName, "Guitarra", sizeof(entry.trackName) - 1);
    entry.peakLeft = -12.0f;
    entry.peakRight = -14.0f;
    entry.rmsLeft = -22.0f;
    entry.rmsRight = -24.0f;
    entry.correlation = 0.92f;

    int assignedSlot = shm.registerSlot(entry);
    TEST("registerSlot returns valid index", assignedSlot >= 0);
    TEST("registerSlot increments changeCount", shm.getChangeCount() > 0);

    // Read back
    mixcoach::SharedSlotEntry readBack;
    bool readOk = shm.readSlot(assignedSlot, readBack);
    TEST("readSlot succeeds", readOk);
    if (readOk) {
        TEST("Read slot is active", readBack.active == 1);
        TEST("Read slot index matches", readBack.slotIndex == assignedSlot);
        TEST("Read trackName: Guitarra",
             std::strncmp(readBack.trackName, "Guitarra", 64) == 0);
        TEST("Read bus matches", readBack.bus == 2);
        TEST("Read colour matches", readBack.colourARGB == 0xFF2ECC71);
        TEST("Read peakLeft matches (-12.0f)",
             std::fabs(readBack.peakLeft - (-12.0f)) < 0.001f);
        TEST("Read correlation matches (0.92f)",
             std::fabs(readBack.correlation - 0.92f) < 0.001f);
    }

    // Release
    shm.releaseSlot(assignedSlot);
    mixcoach::SharedSlotEntry afterRelease;
    shm.readSlot(assignedSlot, afterRelease);
    TEST("Slot is inactive after release", afterRelease.active == 0);

    TEST("Health check passes", shm.healthCheck());
    shm.close();
    TEST("After close, block is null", shm.getBlock() == nullptr);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 5: Full Slot Registration → Backup Pipeline
//  Simulates Messenger → backup → MixCoach reload with property updates
// ═══════════════════════════════════════════════════════════════════════════
static void test_slot_registration_pipeline() {
    std::printf("\n── Test 5: Slot Registration Pipeline ──\n");
    std::fflush(stdout);
    cleanupBackupFiles();

    // Phase 1: "Messenger" registers slots (auto-creates backup files)
    auto messengerReg = std::make_unique<mixcoach::SlotRegistry>();
    int s0 = messengerReg->registerSlot("Bateria",
        juce::Colour(0xFFE74C3C), mixcoach::BusType::Drums);
    int s1 = messengerReg->registerSlot("Bajo",
        juce::Colour(0xFF3498DB), mixcoach::BusType::Bass);
    int s2 = messengerReg->registerSlot("Guitarra",
        juce::Colour(0xFF2ECC71), mixcoach::BusType::Guitars);

    TEST("registerSlot returns valid indices", s0 >= 0 && s1 >= 0 && s2 >= 0);
    TEST("activeCount matches after registration", messengerReg->activeCount() == 3);

    // Verify backup files created automatically
    juce::Array<juce::File> backupFiles;
    getBackupDir().findChildFiles(backupFiles, juce::File::findFiles, false, "slot_*.bin");
    TEST("Backup files created for all 3 slots", backupFiles.size() >= 3);

    // Phase 2: "MixCoach" reads from backup files
    auto coachReg = std::make_unique<mixcoach::SlotRegistry>();
    int found = coachReg->loadSlotsFromBackupFiles(true);
    TEST("loadSlotsFromBackupFiles found all 3 slots", found >= 3);

    auto i0 = coachReg->getSlotInfo(0);
    auto i1 = coachReg->getSlotInfo(1);
    auto i2 = coachReg->getSlotInfo(2);

    TEST("Slot 0 active", i0.active);
    TEST("Slot 0 name: Bateria", juce::String(i0.trackName).trim() == "Bateria");
    TEST("Slot 0 bus: Drums", i0.bus == mixcoach::BusType::Drums);
    TEST("Slot 0 colour", i0.colour.getARGB() == juce::Colour(0xFFE74C3C).getARGB());

    TEST("Slot 1 name: Bajo", juce::String(i1.trackName).trim() == "Bajo");
    TEST("Slot 1 bus: Bass", i1.bus == mixcoach::BusType::Bass);

    TEST("Slot 2 name: Guitarra", juce::String(i2.trackName).trim() == "Guitarra");
    TEST("Slot 2 bus: Guitars", i2.bus == mixcoach::BusType::Guitars);

    // Phase 3: Update slot properties (simulating user editing in Messenger)
    messengerReg->updateSlotName(1, "Bajo Electrico");
    messengerReg->updateSlotColour(1, juce::Colour(0xFF1F618D));
    messengerReg->updateSlotBus(1, mixcoach::BusType::Keys);

    found = coachReg->loadSlotsFromBackupFiles(true);
    auto u1 = coachReg->getSlotInfo(1);
    TEST("Updated slot name: Bajo Electrico",
         juce::String(u1.trackName).trim() == "Bajo Electrico");
    TEST("Updated slot bus: Keys",
         u1.bus == mixcoach::BusType::Keys);
    TEST("Updated slot colour matches",
         u1.colour.getARGB() == juce::Colour(0xFF1F618D).getARGB());

    // Phase 4: Release slot → backup removed
    messengerReg->releaseSlot(2);
    auto releasedBackup = getBackupDir().getChildFile("slot_2.bin");
    TEST("Backup file removed after releaseSlot", !releasedBackup.existsAsFile());

    // Messenger's local state sees the release immediately
    auto messengerSlot2 = messengerReg->getSlotInfo(2);
    TEST("Messenger sees released slot as inactive", !messengerSlot2.active);

    // Coach reloads, but loadSlotsFromBackupFiles only OVERWRITES slots
    // that have backup files — it doesn't clear slots whose backup was
    // removed (since slot indices can be reused by the Messenger).
    // The coach will learn about the release on its next periodic poll
    // when the Messenger reuses slot 2 for a new track.
    coachReg->loadSlotsFromBackupFiles(true);

    // Phase 5: Multiple telemetry updates
    for (int t = 0; t < 5; ++t) {
        float peak = -10.0f + static_cast<float>(t) * 0.5f;
        mixcoach::SlotRegistry::updateSlotBackupTelemetry(
            0, peak, peak - 2.0f, -20.0f + static_cast<float>(t),
            -22.0f, 0.9f - static_cast<float>(t) * 0.02f,
            10.0f + static_cast<float>(t), 0.1f, -0.05f,
            -14.0f, -16.0f, -12.0f, -8.0f, 6.0f);
    }

    found = coachReg->loadSlotsFromBackupFiles(true);
    auto s0AfterTelemetry = coachReg->getSlotInfo(0);
    TEST("Slot still active after multiple telemetry updates", s0AfterTelemetry.active);
    TEST("Slot name preserved after telemetry updates",
         juce::String(s0AfterTelemetry.trackName).trim() == "Bateria");

    auto& buf0 = coachReg->getTelemetry(0);
    int64_t bufSize = buf0.size();
    TEST("Telemetry buffer has entries after 5 updates", bufSize > 0);
    if (bufSize > 0) {
        auto latest = buf0.latest();
        float expectedPeak = -10.0f + 4.0f * 0.5f; // = -8.0f
        TEST("Latest telemetry peakLeft reflects last update",
             std::fabs(latest.peakLeft - expectedPeak) < 0.001f);
    }

    messengerReg.reset();
    coachReg.reset();
    cleanupBackupFiles();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 6: Concurrent Backup Files (multiple Messengers)
//  Simulates 10 Messengers writing independent backup files
// ═══════════════════════════════════════════════════════════════════════════
static void test_concurrent_backup_files() {
    std::printf("\n── Test 6: Concurrent Backup Files ──\n");
    std::fflush(stdout);
    cleanupBackupFiles();

    constexpr int numSlots = 10;
    for (int i = 0; i < numSlots; ++i) {
        mixcoach::SlotInfo slot;
        slot.slotIndex = i;
        slot.active = true;
        slot.bus = static_cast<mixcoach::BusType>(i % 6);
        slot.colour = juce::Colour::fromHSV(
            static_cast<float>(i) / static_cast<float>(numSlots), 0.8f, 0.7f, 1.0f);
        slot.setTrackName("Track_" + std::to_string(i));
        mixcoach::SlotRegistry::saveSlotToBackupFile(i, slot);
    }

    juce::Array<juce::File> files;
    getBackupDir().findChildFiles(files, juce::File::findFiles, false, "slot_*.bin");
    TEST("All 10 backup files exist", files.size() >= numSlots);

    auto registry = std::make_unique<mixcoach::SlotRegistry>();
    int found = registry->loadSlotsFromBackupFiles(true);
    TEST("All 10 slots found by loadSlotsFromBackupFiles", found >= numSlots);
    TEST("activeCount matches 10", registry->activeCount() == numSlots);

    for (int i = 0; i < numSlots; ++i) {
        auto info = registry->getSlotInfo(i);
        juce::String expectedName = "Track_" + std::to_string(i);
        std::string testName = "Slot " + std::to_string(i) + " name: " + expectedName.toStdString();
        TEST(testName.c_str(),
             juce::String(info.trackName).trim() == expectedName);
    }

    // Cleanup all
    for (int i = 0; i < numSlots; ++i)
        mixcoach::SlotRegistry::removeSlotBackupFile(i);

    files.clear();
    getBackupDir().findChildFiles(files, juce::File::findFiles, false, "slot_*.bin");
    TEST("All backup files removed after cleanup", files.size() == 0);

    registry.reset();
    cleanupBackupFiles();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 7: SlotRegistry + SharedMemory Integration
//  Full pipeline: register via SlotRegistry → shared memory → second process reads
// ═══════════════════════════════════════════════════════════════════════════
static void test_registry_with_shared_memory() {
    std::printf("\n── Test 7: SlotRegistry + SharedMemory ──\n");
    std::fflush(stdout);
    cleanupBackupFiles();

    juce::String testMapName = "Local\\MixCoach_Test_Reg_" +
        juce::String(juce::Time::getMillisecondCounter());

    // Create SharedMemoryManager
    auto shm = std::make_unique<mixcoach::SharedMemoryManager>();
    if (!shm->initialize(testMapName)) {
        std::printf("  \xe2\x9a\xa0\xef\xb8\x8f SKIP: Shared memory not available\n");
        std::fflush(stdout);
        return;
    }

    // Create SlotRegistry with shared memory
    auto registry = std::make_unique<mixcoach::SlotRegistry>();
    registry->setSharedMemory(shm.get());

    // Register slots
    int s0 = registry->registerSlot("Vocal",
        juce::Colour(0xFF9B59B6), mixcoach::BusType::Vocals);
    int s1 = registry->registerSlot("Bateria",
        juce::Colour(0xFFE74C3C), mixcoach::BusType::Drums);
    TEST("registerSlot with shared memory returns valid indices",
         s0 >= 0 && s1 >= 0);

    // Verify data is in shared memory
    mixcoach::SharedSlotEntry shmEntry;
    bool readOk = shm->readSlot(0, shmEntry);
    TEST("Slot 0 readable from shared memory", readOk);
    if (readOk) {
        TEST("Slot 0 in shared memory name: Vocal",
             std::strncmp(shmEntry.trackName, "Vocal", 64) == 0);
        TEST("Slot 0 in shared memory bus: Vocals",
             shmEntry.bus == static_cast<int>(mixcoach::BusType::Vocals));
        TEST("Slot 0 in shared memory is active", shmEntry.active == 1);
    }

    // Update telemetry via shared memory
    registry->updateSharedTelemetry(0,
        -6.0f, -7.0f, -18.0f, -19.0f, 0.95f, 12.0f,
        0.3f, -0.1f, nullptr, -14.0f, -16.0f, -12.0f, -8.0f, 6.0f);

    shm->readSlot(0, shmEntry);
    TEST("Telemetry peakLeft in shared memory matches (-6.0f)",
         std::fabs(shmEntry.peakLeft - (-6.0f)) < 0.001f);
    TEST("Telemetry correlation in shared memory matches (0.95f)",
         std::fabs(shmEntry.correlation - 0.95f) < 0.001f);
    TEST("Telemetry lufsIntegrated in shared memory matches (-14.0f)",
         std::fabs(shmEntry.lufsIntegrated - (-14.0f)) < 0.001f);

    // Second registry (simulating MixCoach) attaches to same shared memory
    auto shm2 = std::make_unique<mixcoach::SharedMemoryManager>();
    if (shm2->initialize(testMapName)) {
        auto registry2 = std::make_unique<mixcoach::SlotRegistry>();
        registry2->setSharedMemory(shm2.get());

        bool synced = registry2->syncFromShared();
        TEST("Second registry syncs from shared memory", synced);

        auto info0 = registry2->getSlotInfo(0);
        TEST("Second registry sees slot 0 active", info0.active);
        TEST("Second registry sees slot 0 name: Vocal",
             juce::String(info0.trackName).trim() == "Vocal");

        int found = registry2->forceFullSync();
        TEST("forceFullSync finds slots via shared memory", found >= 2);

        registry2.reset();
    }

    registry.reset();
    shm->close();
    if (shm2) shm2->close();
    cleanupBackupFiles();
}

// ─── Main ───────────────────────────────────────────────────────────────────
int main() {
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("\xf0\x9f\x94\x8c  IPC Integration Tests\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n\n");
    std::fflush(stdout);

    test_backup_file_roundtrip();
    test_backup_telemetry_update();
    test_remove_backup_file();
    test_shared_memory_basic();
    test_slot_registration_pipeline();
    test_concurrent_backup_files();
    test_registry_with_shared_memory();

    std::printf("\n\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::fflush(stdout);

    return gTestsFailed > 0 ? 1 : 0;
}
