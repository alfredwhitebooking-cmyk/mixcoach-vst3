// ═══════════════════════════════════════════════════════════════════════════
//  TestSlotRegistry.cpp — Unit tests para SlotRegistry (modo local)
//  Cubre: register/release, activeCount, getters, updates, forEach, buffers
// ═══════════════════════════════════════════════════════════════════════════
//
// NOTA: SlotRegistry es grande (~11MB por TelemetryBuffer arrays).
// Siempre asignar en heap (std::make_unique) para evitar stack overflow.
//
//  Build: cmake --build build --config Release --target TestSlotRegistry
//  Run:   build/tests/Release/TestSlotRegistry.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstring>
#include <memory>

// ─── Dependencias del proyecto ──────────────────────────────────────────────
#include "Common/memory/SlotRegistry.h"
#include "Common/types/Types.h"
#include "Common/types/Constants.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─── Test runner ───────────────────────────────────────────────────────────
static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        std::fprintf(stderr, "  \xe2\x9d\x8c FAIL: %s (%s:%d)\n", name, __FILE__, __LINE__); \
        gTestsFailed++; \
    } else { \
        std::printf("  \xe2\x9c\x85 PASS: %s\n", name); \
        gTestsPassed++; \
    } \
} while(0)

#define TEST_NEAR(name, a, b, eps) TEST(name, std::fabs((a) - (b)) < (eps))

// ─── Helpers para backup files ──────────────────────────────────────────────
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

// ============================================================================
//  1. Estado inicial
// ============================================================================
static void test_initial_state()
{
    std::printf("\n── Initial State ──\n");
    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    TEST("activeCount = 0 initially", reg->activeCount() == 0);
    TEST("totalSlots = kMaxSlots", reg->totalSlots() == mixcoach::SlotRegistry::kMaxSlots);

    // SlotInfo por defecto para slot no registrado
    auto info = reg->getSlotInfo(0);
    TEST("unregistered slot has slotIndex -1", info.slotIndex == -1);
    TEST("unregistered slot is not active", !info.active);

    // getSlotColour returns grey for unregistered slot
    auto grey = reg->getSlotColour(0);
    TEST("unregistered slot colour is grey",
         grey.getARGB() == juce::Colours::grey.getARGB());

    // ChangeCount en modo local
    TEST("local changeCount starts at 0", reg->getChangeCount() == 0);
}

// ============================================================================
//  2. Registrar un slot
// ============================================================================
static void test_register_one()
{
    std::printf("\n── Register One ──\n");
    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    int idx = reg->registerSlot("Bateria",
        juce::Colour(0xFFE74C3C), mixcoach::BusType::Drums);
    TEST("register returns index 0", idx == 0);
    TEST("activeCount = 1", reg->activeCount() == 1);
    TEST("changeCount incremented", reg->getChangeCount() > 0);

    auto info = reg->getSlotInfo(0);
    TEST("slot active", info.active);
    TEST("slotIndex = 0", info.slotIndex == 0);
    TEST("trackName: Bateria", juce::String(info.trackName).trim() == "Bateria");
    TEST("bus: Drums", info.bus == mixcoach::BusType::Drums);
    TEST("colour ARGB matches",
         info.colour.getARGB() == juce::Colour(0xFFE74C3C).getARGB());

    // getSlotColour helper
    auto c = reg->getSlotColour(0);
    TEST("getSlotColour returns correct colour",
         c.getARGB() == juce::Colour(0xFFE74C3C).getARGB());
}

// ============================================================================
//  3. Registrar múltiples slots
// ============================================================================
static void test_register_multiple()
{
    std::printf("\n── Register Multiple ──\n");
    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    int i0 = reg->registerSlot("Bateria",  juce::Colour(0xFFE74C3C), mixcoach::BusType::Drums);
    int i1 = reg->registerSlot("Bajo",     juce::Colour(0xFF3498DB), mixcoach::BusType::Bass);
    int i2 = reg->registerSlot("Guitarra", juce::Colour(0xFF2ECC71), mixcoach::BusType::Guitars);
    int i3 = reg->registerSlot("Vocal",    juce::Colour(0xFF9B59B6), mixcoach::BusType::Vocals);

    TEST("slot 0 = 0", i0 == 0);
    TEST("slot 1 = 1", i1 == 1);
    TEST("slot 2 = 2", i2 == 2);
    TEST("slot 3 = 3", i3 == 3);
    TEST("activeCount = 4", reg->activeCount() == 4);

    // Verificar cada slot
    auto c0 = reg->getSlotInfo(0);
    auto c1 = reg->getSlotInfo(1);
    auto c2 = reg->getSlotInfo(2);
    TEST("slot 0 name: Bateria",  juce::String(c0.trackName).trim() == "Bateria");
    TEST("slot 1 name: Bajo",     juce::String(c1.trackName).trim() == "Bajo");
    TEST("slot 2 name: Guitarra", juce::String(c2.trackName).trim() == "Guitarra");
    TEST("slot 3 name: Vocal",    juce::String(reg->getSlotInfo(3).trackName).trim() == "Vocal");
}

// ============================================================================
//  4. Release slot
// ============================================================================
static void test_release()
{
    std::printf("\n── Release ──\n");
    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    reg->registerSlot("Bateria",  juce::Colour(0xFFE74C3C), mixcoach::BusType::Drums);
    reg->registerSlot("Bajo",     juce::Colour(0xFF3498DB), mixcoach::BusType::Bass);
    TEST("activeCount = 2 before release", reg->activeCount() == 2);

    reg->releaseSlot(0);
    TEST("activeCount = 1 after release", reg->activeCount() == 1);
    auto info = reg->getSlotInfo(0);
    TEST("released slot is inactive", !info.active);
    TEST("released slotIndex = -1", info.slotIndex == -1);
    TEST("slot 1 still active", reg->getSlotInfo(1).active);

    // Registrar de nuevo — debe reusar slot 0
    int newIdx = reg->registerSlot("Teclados",
        juce::Colour(0xFFF39C12), mixcoach::BusType::Keys);
    TEST("new register reuses released index 0", newIdx == 0);
    TEST("activeCount = 2 again", reg->activeCount() == 2);
    auto reused = reg->getSlotInfo(0);
    TEST("reused slot name: Teclados",
         juce::String(reused.trackName).trim() == "Teclados");
    TEST("reused slot bus: Keys", reused.bus == mixcoach::BusType::Keys);
}

// ============================================================================
//  5. setActive toggle
// ============================================================================
static void test_set_active()
{
    std::printf("\n── Set Active ──\n");
    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    reg->registerSlot("Bateria", juce::Colour(0xFFE74C3C), mixcoach::BusType::Drums);
    TEST("activeCount = 1 initially", reg->activeCount() == 1);

    reg->setActive(0, false);
    TEST("activeCount = 0 after setActive(false)", reg->activeCount() == 0);
    auto info = reg->getSlotInfo(0);
    TEST("slot is inactive", !info.active);

    reg->setActive(0, true);
    TEST("activeCount = 1 after setActive(true)", reg->activeCount() == 1);
    info = reg->getSlotInfo(0);
    TEST("slot is active again", info.active);
}

// ============================================================================
//  6. Update slot properties
// ============================================================================
static void test_updates()
{
    std::printf("\n── Updates ──\n");
    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    reg->registerSlot("Bajo", juce::Colour(0xFF3498DB), mixcoach::BusType::Bass);

    // Update name
    reg->updateSlotName(0, "Bajo Electrico");
    auto n1 = reg->getSlotInfo(0);
    TEST("updateSlotName: Bajo Electrico",
         juce::String(n1.trackName).trim() == "Bajo Electrico");

    // Update colour
    reg->updateSlotColour(0, juce::Colour(0xFF1F618D));
    auto n2 = reg->getSlotInfo(0);
    TEST("updateSlotColour: #1F618D",
         n2.colour.getARGB() == juce::Colour(0xFF1F618D).getARGB());

    // Update bus
    reg->updateSlotBus(0, mixcoach::BusType::Keys);
    auto n3 = reg->getSlotInfo(0);
    TEST("updateSlotBus: Keys", n3.bus == mixcoach::BusType::Keys);

    // changeCount debe incrementar con cada update
    TEST("changeCount > 0 after updates", reg->getChangeCount() > 0);
}

// ============================================================================
//  7. forEachActive
// ============================================================================
static void test_for_each_active()
{
    std::printf("\n── forEachActive ──\n");
    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    reg->registerSlot("Bateria",  juce::Colour(0xFFE74C3C), mixcoach::BusType::Drums);
    reg->registerSlot("Bajo",     juce::Colour(0xFF3498DB), mixcoach::BusType::Bass);
    reg->registerSlot("Guitarra", juce::Colour(0xFF2ECC71), mixcoach::BusType::Guitars);

    int count = 0;
    reg->forEachActive([&](const mixcoach::SlotInfo& info) {
        count++;
        TEST("forEachActive slot is active", info.active);
    });
    TEST("forEachActive visited 3 slots", count == 3);

    // Release one, forEach debería visitar 2
    reg->releaseSlot(1);
    count = 0;
    reg->forEachActive([&](const mixcoach::SlotInfo&) { count++; });
    TEST("forEachActive visited 2 slots after release", count == 2);
}

// ============================================================================
//  8. Telemetry buffer access
// ============================================================================
static void test_telemetry_buffer()
{
    std::printf("\n── Telemetry Buffer ──\n");
    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    reg->registerSlot("Vocal", juce::Colour(0xFF9B59B6), mixcoach::BusType::Vocals);

    auto& buf = reg->getTelemetry(0);
    TEST("telemetry buffer size is 0 initially", buf.size() == 0);

    // Push telemetry data
    mixcoach::TrackTelemetry t1;
    t1.timestamp = 1000;
    t1.peakLeft = -6.0f;
    t1.peakRight = -8.0f;
    t1.rmsLeft = -18.0f;
    t1.correlation = 0.95f;
    t1.lufsIntegrated = -14.0f;
    buf.push(t1);

    TEST("telemetry buffer size = 1 after push", buf.size() == 1);

    auto latest = buf.latest();
    TEST_NEAR("latest peakLeft", latest.peakLeft, -6.0f, 0.001f);
    TEST_NEAR("latest peakRight", latest.peakRight, -8.0f, 0.001f);
    TEST_NEAR("latest correlation", latest.correlation, 0.95f, 0.001f);
    TEST_NEAR("latest lufsIntegrated", latest.lufsIntegrated, -14.0f, 0.001f);

    // Const ref también funciona
    const auto& constBuf = reg->getTelemetry(0);
    TEST("const telemetry buffer has 1 entry", constBuf.size() == 1);
}

// ============================================================================
//  9. Audio ring buffer access
// ============================================================================
static void test_audio_buffer()
{
    std::printf("\n── Audio Ring Buffer ──\n");
    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    reg->registerSlot("Guitarra", juce::Colour(0xFF2ECC71), mixcoach::BusType::Guitars);

    auto& buf = reg->getAudioBuffer(0);
    TEST("audio buffer initially empty", !buf.hasData());
    TEST("audio buffer available = 0", buf.available() == 0);

    // Escribir samples de audio
    float testData[] = {0.5f, -0.3f, 0.1f, -0.7f, 0.9f};
    buf.write(testData, 5);
    TEST("audio buffer has data after write", buf.hasData());
    TEST("audio buffer available = 5", buf.available() == 5);

    // Leer back
    float readback[8] = {};
    int read = buf.read(readback, 3);
    TEST("read returns 3 samples", read == 3);
    TEST_NEAR("readback[0]", readback[0], 0.5f, 0.001f);
    TEST_NEAR("readback[1]", readback[1], -0.3f, 0.001f);
    TEST_NEAR("readback[2]", readback[2], 0.1f, 0.001f);
    TEST("audio buffer available = 2 after read", buf.available() == 2);

    // Peek (sin consumir)
    float peekBuf[8] = {};
    int peeked = buf.peek(peekBuf, 2);
    TEST("peek returns 2 samples", peeked == 2);
    TEST_NEAR("peek[0]", peekBuf[0], -0.7f, 0.001f);
    TEST_NEAR("peek[1]", peekBuf[1], 0.9f, 0.001f);
    TEST("audio buffer available still 2 after peek", buf.available() == 2);

    // Reset
    buf.reset();
    TEST("audio buffer empty after reset", !buf.hasData());
    TEST("audio buffer available = 0 after reset", buf.available() == 0);

    // Const ref
    const auto& constBuf = reg->getAudioBuffer(0);
    TEST("const audio buffer exists", true);
}

// ============================================================================
// 10. SlotInfo para slot inexistente
// ============================================================================
static void test_invalid_slot()
{
    std::printf("\n── Invalid Slot ──\n");
    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    // SlotInfo para índice -1 (nunca registrado)
    auto info = reg->getSlotInfo(-1);
    TEST("getSlotInfo(-1) returns default (slotIndex -1)", info.slotIndex == -1);
    TEST("getSlotInfo(-1) not active", !info.active);

    // SlotInfo para índice más allá del límite
    auto info2 = reg->getSlotInfo(mixcoach::SlotRegistry::kMaxSlots);
    TEST("getSlotInfo(kMaxSlots) returns default", info2.slotIndex == -1);

    // getSlotColour para índices inválidos
    auto c1 = reg->getSlotColour(-1);
    TEST("getSlotColour(-1) returns grey",
         c1.getARGB() == juce::Colours::grey.getARGB());

    auto c2 = reg->getSlotColour(mixcoach::SlotRegistry::kMaxSlots);
    TEST("getSlotColour(kMaxSlots) returns grey",
         c2.getARGB() == juce::Colours::grey.getARGB());

    // setActive en índice inválido no debe crashear
    reg->setActive(-1, true);
    TEST("setActive(-1, true) does not crash", true);
    reg->setActive(mixcoach::SlotRegistry::kMaxSlots, false);
    TEST("setActive(kMaxSlots, false) does not crash", true);

    // releaseSlot en índice inválido
    reg->releaseSlot(-1);
    TEST("releaseSlot(-1) does not crash", true);
    reg->releaseSlot(mixcoach::SlotRegistry::kMaxSlots);
    TEST("releaseSlot(kMaxSlots) does not crash", true);

    // updateSlotBus en índice inválido
    reg->updateSlotBus(-1, mixcoach::BusType::Drums);
    TEST("updateSlotBus(-1) does not crash", true);
}

// ============================================================================
// 11. Registrar todos los slots (full registry)
// ============================================================================
static void test_full_slots()
{
    std::printf("\n── Full Slots ──\n");
    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    // Llenar todos los slots disponibles
    int lastIdx = -1;
    for (int i = 0; i < mixcoach::SlotRegistry::kMaxSlots; ++i) {
        std::string name = "Track_" + std::to_string(i);
        float hue = static_cast<float>(i) / mixcoach::SlotRegistry::kMaxSlots;
        auto colour = juce::Colour::fromHSV(hue, 0.8f, 0.7f, 1.0f);
        auto bus = static_cast<mixcoach::BusType>(i % mixcoach::kNumBuses);
        int idx = reg->registerSlot(name, colour, bus);
        TEST("registerSlot returns valid index when filling",
             idx >= 0 && idx < mixcoach::SlotRegistry::kMaxSlots);
        lastIdx = idx;
    }
    TEST("last registered index = kMaxSlots - 1",
         lastIdx == mixcoach::SlotRegistry::kMaxSlots - 1);
    TEST("activeCount = kMaxSlots", reg->activeCount() == mixcoach::SlotRegistry::kMaxSlots);

    // Overflow
    int overflow = reg->registerSlot("Overflow",
        juce::Colours::red, mixcoach::BusType::None);
    // Sin shared memory, registerSlot busca secuencialmente - debería ser -1
    // (en modo local, no hay shared memory que asigne, y todos los slots locales están activos)
    TEST("registerSlot returns -1 when full", overflow == -1);

    // Liberar uno y registrar de nuevo
    reg->releaseSlot(5);
    int reuse = reg->registerSlot("NewTrack",
        juce::Colour(0xFF00FFFF), mixcoach::BusType::FX);
    TEST("registerSlot reuses released slot index 5", reuse == 5);
    TEST("activeCount = kMaxSlots again", reg->activeCount() == mixcoach::SlotRegistry::kMaxSlots);

    auto reused = reg->getSlotInfo(5);
    TEST("reused slot name: NewTrack",
         juce::String(reused.trackName).trim() == "NewTrack");
}

// ============================================================================
// 12. getChangeCount en modo local (sin shared memory)
// ============================================================================
static void test_local_change_count()
{
    std::printf("\n── Local Change Count ──\n");
    auto reg = std::make_unique<mixcoach::SlotRegistry>();

    uint64_t cc0 = reg->getChangeCount();
    TEST("cc starts at 0", cc0 == 0);

    reg->registerSlot("Track1", juce::Colours::red, mixcoach::BusType::None);
    uint64_t cc1 = reg->getChangeCount();
    TEST("cc incremented after register", cc1 > cc0);

    reg->updateSlotName(0, "Renamed");
    uint64_t cc2 = reg->getChangeCount();
    TEST("cc incremented after updateName", cc2 > cc1);

    reg->releaseSlot(0);
    uint64_t cc3 = reg->getChangeCount();
    TEST("cc incremented after release", cc3 > cc2);
}

// ============================================================================
//  Main
// ============================================================================

int main()
{
    // Limpiar backup files residuales de tests anteriores
    cleanupBackupFiles();

    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  SlotRegistry Unit Tests (Local Mode)\n");
    std::printf("  Register  |  Release  |  Updates  |  Iterate  |  Buffers  |  Boundaries\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");

    test_initial_state();
    test_register_one();
    test_register_multiple();
    test_release();
    test_set_active();
    test_updates();
    test_for_each_active();
    test_telemetry_buffer();
    test_audio_buffer();
    test_invalid_slot();
    test_full_slots();
    test_local_change_count();

    std::printf("\n\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");

    // Cleanup final
    cleanupBackupFiles();

    return gTestsFailed > 0 ? 1 : 0;
}
