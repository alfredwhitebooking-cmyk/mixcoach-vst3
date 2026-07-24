// ═══════════════════════════════════════════════════════════════════════════
//  TestIPCIntegration.cpp — Integration test for the V3 IPC pipeline
//  Tests: SharedMemoryManager + SlotRegistry sync (identity only)
//
//  V3: SharedMemory V6 (solo identidad, sin telemetría).
//      SharedAudioMemory V1 (audio RAW cross-process, probado por separado).
//      Backup files ELIMINADOS.
//
// NOTE: SlotRegistry is large (~11MB due to TelemetryBuffer arrays).
// Always allocate on heap (std::make_unique) to avoid stack overflow.
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstring>
#include <memory>

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

#include "Common/types/Types.h"
#include "Common/types/Constants.h"
#include "Messenger/core/MessengerType.h"
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

// ═══════════════════════════════════════════════════════════════════════════
//  Test 1: SlotRegistry Basic Operations
//  Register, query, release, update properties
// ═══════════════════════════════════════════════════════════════════════════
static void test_slot_registry_basic()
{
    std::printf("\n── Test 1: SlotRegistry Basic Operations ──\n");
    std::fflush(stdout);

    auto registry = std::make_unique<mixcoach::SlotRegistry>();

    TEST("Initial active count = 0", registry->activeCount() == 0);

    // Register slots
    int s0 = registry->registerSlot("Bateria",
        juce::Colours::red, mixcoach::BusType::Drums);
    int s1 = registry->registerSlot("Bajo",
        juce::Colours::blue, mixcoach::BusType::Bass);
    int s2 = registry->registerSlot("Guitarra",
        juce::Colours::greenyellow, mixcoach::BusType::Guitars);

    TEST("registerSlot returns valid indices", s0 >= 0 && s1 >= 0 && s2 >= 0);
    TEST("activeCount = 3 after registration", registry->activeCount() == 3);

    // Query slot info
    auto i0 = registry->getSlotInfo(0);
    TEST("Slot 0 name: Bateria", juce::String(i0.trackName).trim() == "Bateria");
    TEST("Slot 0 bus: Drums", i0.bus == mixcoach::BusType::Drums);
    TEST("Slot 0 colour matches", i0.colour.getARGB() == juce::Colours::red.getARGB());
    TEST("Slot 0 is active", i0.active);

    auto i1 = registry->getSlotInfo(1);
    TEST("Slot 1 name: Bajo", juce::String(i1.trackName).trim() == "Bajo");
    TEST("Slot 1 bus: Bass", i1.bus == mixcoach::BusType::Bass);

    auto i2 = registry->getSlotInfo(2);
    TEST("Slot 2 name: Guitarra", juce::String(i2.trackName).trim() == "Guitarra");
    TEST("Slot 2 bus: Guitars", i2.bus == mixcoach::BusType::Guitars);

    // Update properties
    registry->updateSlotName(1, "Bajo Electrico");
    registry->updateSlotColour(1, juce::Colours::blue.darker(0.6f));
    registry->updateSlotBus(1, mixcoach::BusType::Keys);

    auto u1 = registry->getSlotInfo(1);
    TEST("Updated slot name: Bajo Electrico",
         juce::String(u1.trackName).trim() == "Bajo Electrico");
    TEST("Updated slot bus: Keys",
         u1.bus == mixcoach::BusType::Keys);
    TEST("Updated slot colour matches",
         u1.colour.getARGB() == juce::Colours::blue.darker(0.6f).getARGB());

    // Release slot
    registry->releaseSlot(2);
    auto r2 = registry->getSlotInfo(2);
    TEST("Released slot is inactive", !r2.active);
    TEST("activeCount = 2 after release", registry->activeCount() == 2);

    registry.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 2: SlotRegistry forEachActive
// ═══════════════════════════════════════════════════════════════════════════
static void test_slot_registry_for_each()
{
    std::printf("\n── Test 2: SlotRegistry forEachActive ──\n");
    std::fflush(stdout);

    auto registry = std::make_unique<mixcoach::SlotRegistry>();

    registry->registerSlot("Vocal", juce::Colours::purple, mixcoach::BusType::Vocals);
    registry->registerSlot("Bateria", juce::Colours::red, mixcoach::BusType::Drums);
    registry->registerSlot("Bajo", juce::Colours::blue, mixcoach::BusType::Bass);

    int count = 0;
    juce::String names;
    registry->forEachActive([&](const mixcoach::SlotInfo& info) {
        count++;
        if (names.isNotEmpty()) names += ",";
        names += juce::String(info.trackName).trim();
    });

    TEST("forEachActive found 3 tracks", count == 3);
    TEST("forEachActive found all names",
         names.contains("Vocal") && names.contains("Bateria") && names.contains("Bajo"));

    // Release one, verify iteration
    registry->releaseSlot(1);
    count = 0;
    registry->forEachActive([&](const mixcoach::SlotInfo&) { count++; });
    TEST("forEachActive count = 2 after release", count == 2);

    registry.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 3: SharedMemoryManager Basic Operations (V6 identity-only)
//  Creates SharedMemoryManager → registers slot → reads back → releases
//  V3: SharedMemory V6 = solo identidad (slotIndex, trackName, colourARGB, bus, active).
//      SIN campos de telemetría (peakLeft, peakRight, rmsLeft, rmsRight, etc.)
// ═══════════════════════════════════════════════════════════════════════════
static void test_shared_memory_basic()
{
    std::printf("\n── Test 3: SharedMemory Basic (V6 Identity) ──\n");
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
    TEST("Struct version is V6",
         block->header.structVersion == mixcoach::SharedMemoryHeader::kCurrentStructVersion);
    TEST("kCurrentStructVersion = 10",
         mixcoach::SharedMemoryHeader::kCurrentStructVersion == 10);
    TEST("Change count starts at 0", shm.getChangeCount() == 0);

    // Register a slot (identity only — V6)
    mixcoach::SharedSlotEntry entry;
    entry.active = 1;
    entry.bus = static_cast<int>(mixcoach::BusType::Guitars);
    entry.colourARGB = juce::Colours::greenyellow.getARGB();
    std::strncpy(entry.trackName, "Guitarra", sizeof(entry.trackName) - 1);

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
        TEST("Read bus matches (Guitars)",
             readBack.bus == static_cast<int>(mixcoach::BusType::Guitars));
        TEST("Read colour matches",
             readBack.colourARGB == juce::Colours::greenyellow.getARGB());
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
//  Test 4: SlotRegistry + SharedMemory Integration
//  Full pipeline V3: register via SlotRegistry → sync to shared memory →
//  second instance reads identity → release propagates
// ═══════════════════════════════════════════════════════════════════════════
static void test_registry_with_shared_memory()
{
    std::printf("\n── Test 4: SlotRegistry + SharedMemory (V3) ──\n");
    std::fflush(stdout);

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

    // Register slots (should auto-sync to shared memory)
    int s0 = registry->registerSlot("Vocal",
        juce::Colours::purple, mixcoach::BusType::Vocals);
    int s1 = registry->registerSlot("Bateria",
        juce::Colours::red, mixcoach::BusType::Drums);
    TEST("registerSlot with shared memory returns valid indices",
         s0 >= 0 && s1 >= 0);

    // Verify identity data is in shared memory
    mixcoach::SharedSlotEntry shmEntry;
    bool readOk = shm->readSlot(0, shmEntry);
    TEST("Slot 0 readable from shared memory", readOk);
    if (readOk) {
        TEST("Slot 0 in shared memory name: Vocal",
             std::strncmp(shmEntry.trackName, "Vocal", 64) == 0);
        TEST("Slot 0 in shared memory bus: Vocals",
             shmEntry.bus == static_cast<int>(mixcoach::BusType::Vocals));
        TEST("Slot 0 in shared memory is active", shmEntry.active == 1);
        TEST("Slot 0 colour matches",
             shmEntry.colourARGB == juce::Colours::purple.getARGB());
    }

    // Verify slot 1 identity
    shm->readSlot(1, shmEntry);
    TEST("Slot 1 in shared memory name: Bateria",
         std::strncmp(shmEntry.trackName, "Bateria", 64) == 0);
    TEST("Slot 1 in shared memory bus: Drums",
         shmEntry.bus == static_cast<int>(mixcoach::BusType::Drums));

    // Update slot properties and verify they propagate to shared memory
    registry->updateSlotName(0, "Vocal Lead");
    registry->updateSlotColour(0, juce::Colours::purple.darker(0.3f));

    shm->readSlot(0, shmEntry);
    TEST("Updated name propagates to shared memory: Vocal Lead",
         std::strncmp(shmEntry.trackName, "Vocal Lead", 64) == 0);
    TEST("Updated colour propagates to shared memory",
         shmEntry.colourARGB == juce::Colours::purple.darker(0.3f).getARGB());

    // Second registry (simulating MixCoach) attaches to same shared memory
    auto shm2 = std::make_unique<mixcoach::SharedMemoryManager>();
    if (shm2->initialize(testMapName)) {
        auto registry2 = std::make_unique<mixcoach::SlotRegistry>();
        registry2->setSharedMemory(shm2.get());

        int found = registry2->forceFullSync();
        TEST("Second registry syncs from shared memory", found >= 2);

        auto info0 = registry2->getSlotInfo(0);
        TEST("Second registry sees slot 0 active", info0.active);
        TEST("Second registry sees updated name: Vocal Lead",
             juce::String(info0.trackName).trim() == "Vocal Lead");
        TEST("Second registry sees updated colour",
             info0.colour.getARGB() == juce::Colours::purple.darker(0.3f).getARGB());

        // Already did forceFullSync above

        registry2.reset();
    }

    registry.reset();
    shm->close();
    if (shm2) shm2->close();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 5: Concurrent Slot Registration via Shared Memory
//  Simulates multiple Messengers registering slots
// ═══════════════════════════════════════════════════════════════════════════
static void test_concurrent_slot_registration()
{
    std::printf("\n── Test 5: Concurrent Slot Registration ──\n");
    std::fflush(stdout);

    juce::String testMapName = "Local\\MixCoach_Test_Conc_" +
        juce::String(juce::Time::getMillisecondCounter());

    mixcoach::SharedMemoryManager shm;
    if (!shm.initialize(testMapName)) {
        std::printf("  \xe2\x9a\xa0\xef\xb8\x8f SKIP: Shared memory not available\n");
        std::fflush(stdout);
        return;
    }

    constexpr int numSlots = 10;
    for (int i = 0; i < numSlots; ++i) {
        mixcoach::SharedSlotEntry entry;
        entry.active = 1;
        entry.bus = i % 6;
        entry.colourARGB = juce::Colour::fromHSV(
            static_cast<float>(i) / static_cast<float>(numSlots), 0.8f, 0.7f, 1.0f).getARGB();
        // Nota: fromHSV no tiene un nombre fijo, pero no es Colour(0xFF...) literal
        std::string name = "Track_" + std::to_string(i);
        std::strncpy(entry.trackName, name.c_str(), sizeof(entry.trackName) - 1);
        shm.registerSlot(entry);
    }

    // Verify all slots readable
    for (int i = 0; i < numSlots; ++i) {
        mixcoach::SharedSlotEntry entry;
        if (shm.readSlot(i, entry)) {
            juce::String expected = "Track_" + juce::String(i);
            juce::String testReadable = "Slot " + juce::String(i) + " readable";
            TEST(testReadable.toRawUTF8(), entry.active == 1);
            juce::String testNameMatch = "Slot " + juce::String(i) + " name matches";
            TEST(testNameMatch.toRawUTF8(),
                 std::strncmp(entry.trackName, expected.toRawUTF8(), 64) == 0);
        }
    }

    // Attach a second registry and forceFullSync
    auto registry = std::make_unique<mixcoach::SlotRegistry>();
    registry->setSharedMemory(&shm);
    int found = registry->forceFullSync();
    TEST("forceFullSync found all 10 slots", found >= numSlots);

    // Verify via SlotRegistry queries
    int active = registry->activeCount();
    TEST("activeCount matches 10", active == numSlots);

    // Verify some names
    for (int i = 0; i < numSlots; i += 5) {
        auto info = registry->getSlotInfo(i);
        juce::String expected = "Track_" + juce::String(i);
        juce::String testName = "Slot " + juce::String(i) + " name via registry: " + expected;
        TEST(testName.toRawUTF8(),
             juce::String(info.trackName).trim() == expected);
    }

    registry.reset();
    shm.close();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 6: Feedback Loop V9 — TrackType propagation through shared memory
//  Tests the bidirectional flow:
//    MixCoach writes TrackType via updateSlotTrackType() → SHM
//    Messenger reads TrackType via forceFullSync() from SHM
// ═══════════════════════════════════════════════════════════════════════════
static void test_feedback_loop_track_type_propagation()
{
    std::printf("\n── Test 6: Feedback Loop V9 — TrackType Propagation ──\n");
    std::fflush(stdout);

    juce::String testMapName = "Local\\MixCoach_Test_FLV9_" +
        juce::String(juce::Time::getMillisecondCounter());

    // ─── SharedMemory + Registry (simulating MixCoach) ──────────────
    auto shm = std::make_unique<mixcoach::SharedMemoryManager>();
    if (!shm->initialize(testMapName)) {
        std::printf("  \xe2\x9a\xa0\xef\xb8\x8f SKIP: Shared memory not available\n");
        std::fflush(stdout);
        return;
    }

    // ═══ PHASE 1: MixCoach writes TrackType to SHM ═══
    auto registryWriter = std::make_unique<mixcoach::SlotRegistry>();
    registryWriter->setSharedMemory(shm.get());

    int slot0 = registryWriter->registerSlot("Kick",
        juce::Colours::red, mixcoach::BusType::Drums);
    TEST("registerSlot returns valid index", slot0 >= 0);

    int slot1 = registryWriter->registerSlot("Bass 808",
        juce::Colours::blue, mixcoach::BusType::Bass);
    TEST("registerSlot returns valid index for slot 1", slot1 >= 0);

    // Verify initial trackType is -1 (None) after registration
    auto info0 = registryWriter->getSlotInfo(0);
    TEST("Initial trackType = -1 (None)", info0.trackType == -1);

    // ═══ Simulate MixCoach inferring roles (Feedback Loop V9) ═══
    // MixCoach infers TrackRole::Kick → writes TrackType::Kick (0)
    registryWriter->updateSlotTrackType(0, static_cast<int>(mixcoach::TrackType::Kick));

    // MixCoach infers TrackRole::Bass808 → writes TrackType::Bass808 (9)
    registryWriter->updateSlotTrackType(1, static_cast<int>(mixcoach::TrackType::Bass808));

    // Verify local state updated
    auto updated0 = registryWriter->getSlotInfo(0);
    auto updated1 = registryWriter->getSlotInfo(1);
    TEST("Slot 0 local trackType = Kick (0)",
         updated0.trackType == static_cast<int>(mixcoach::TrackType::Kick));
    TEST("Slot 1 local trackType = Bass808 (9)",
         updated1.trackType == static_cast<int>(mixcoach::TrackType::Bass808));

    // Verify SHM entry has trackType directly
    mixcoach::SharedSlotEntry shmEntry;
    bool readOk = shm->readSlot(0, shmEntry);
    TEST("Slot 0 readable from SHM", readOk);
    if (readOk) {
        TEST("Slot 0 SHM trackType = Kick (0)",
             shmEntry.trackType == static_cast<int>(mixcoach::TrackType::Kick));
        TEST("Slot 0 SHM name preserved",
             std::strncmp(shmEntry.trackName, "Kick", 64) == 0);
    }

    shm->readSlot(1, shmEntry);
    TEST("Slot 1 SHM trackType = Bass808 (9)",
         shmEntry.trackType == static_cast<int>(mixcoach::TrackType::Bass808));

    // ═══ PHASE 2: Second registry simulates Messenger reading TrackType ═══
    auto shm2 = std::make_unique<mixcoach::SharedMemoryManager>();
    if (shm2->initialize(testMapName)) {
        auto registryReader = std::make_unique<mixcoach::SlotRegistry>();
        registryReader->setSharedMemory(shm2.get());

        int found = registryReader->forceFullSync();
        TEST("Second registry syncs from SHM", found >= 2);

        auto readInfo0 = registryReader->getSlotInfo(0);
        TEST("Reader sees slot 0 active", readInfo0.active);
        TEST("Reader sees slot 0 trackType = Kick (0)",
             readInfo0.trackType == static_cast<int>(mixcoach::TrackType::Kick));
        TEST("Reader sees slot 0 name: Kick",
             juce::String(readInfo0.trackName).trim() == "Kick");

        auto readInfo1 = registryReader->getSlotInfo(1);
        TEST("Reader sees slot 1 trackType = Bass808 (9)",
             readInfo1.trackType == static_cast<int>(mixcoach::TrackType::Bass808));
        TEST("Reader sees slot 1 name: Bass 808",
             juce::String(readInfo1.trackName).trim() == "Bass 808");

        // ═══ PHASE 3: Update TrackType again → verify re-propagation ═══
        // Simulate MixCoach re-inferring slot 1 as SynthLead instead
        registryWriter->updateSlotTrackType(1, static_cast<int>(mixcoach::TrackType::SynthLead));

        // Force re-sync on reader
        int found2 = registryReader->forceFullSync();
        TEST("Re-sync finds updates", found2 >= 2);

        auto reReadInfo1 = registryReader->getSlotInfo(1);
        TEST("Reader sees updated trackType = SynthLead (13)",
             reReadInfo1.trackType == static_cast<int>(mixcoach::TrackType::SynthLead));

        // ═══ PHASE 4: Verify other properties NOT corrupted by trackType update ═══
        auto reReadInfo0 = registryReader->getSlotInfo(0);
        TEST("Slot 0 name still Kick after slot 1 update",
             juce::String(reReadInfo0.trackName).trim() == "Kick");
        TEST("Slot 0 trackType still Kick after slot 1 update",
             reReadInfo0.trackType == static_cast<int>(mixcoach::TrackType::Kick));
        TEST("Slot 0 bus still Drums after update",
             reReadInfo0.bus == mixcoach::BusType::Drums);
        TEST("Slot 0 colour preserved after update",
             reReadInfo0.colour.getARGB() == juce::Colours::red.getARGB());

        registryReader.reset();
    }

    registryWriter.reset();
    shm->close();
    if (shm2) shm2->close();
}

// ─── Main ───────────────────────────────────────────────────────────────────
int main()
{
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  IPC Integration Tests (V3)\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n\n");
    std::fflush(stdout);

    test_slot_registry_basic();
    test_slot_registry_for_each();
    test_shared_memory_basic();
    test_registry_with_shared_memory();
    test_concurrent_slot_registration();
    test_feedback_loop_track_type_propagation();

    std::printf("\n\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::fflush(stdout);

    return gTestsFailed > 0 ? 1 : 0;
}
