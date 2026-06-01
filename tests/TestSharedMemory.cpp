// ═══════════════════════════════════════════════════════════════════════════
//  TestSharedMemory.cpp — Unit tests para SharedMemoryManager
//  Cubre: init/close, register/read/release, lock, changeCount, healthCheck
//
//  NOTA: Shared memory requires Windows CreateFileMapping API.
//  Si el entorno no lo soporta (ej. sandbox), los tests se skipean
//  automáticamente y solo se ejecuta el diagnóstico inicial.
//
//  Build: cmake --build build --config Release --target TestSharedMemory
//  Run:   build/tests/Release/TestSharedMemory.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstring>
#include <memory>

// ─── Dependencias del proyecto ──────────────────────────────────────────────
#include "Common/memory/SharedMemory.h"

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─── Test runner ───────────────────────────────────────────────────────────
static int gTestsPassed = 0;
static int gTestsFailed = 0;
static bool gSharedMemAvailable = false;
static int  gSharedMemLastError = 0;

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

// ─── Helpers ────────────────────────────────────────────────────────────────
// Genera un nombre único para el mapa de memoria compartida cada test run
static juce::String uniqueMapName(const char* prefix)
{
    auto ts = juce::Time::getMillisecondCounter();
    auto pid = (int)::GetCurrentProcessId();
    return "Local\\MixCoach_Test_" + juce::String(prefix) + "_"
         + juce::String(pid) + "_" + juce::String(ts);
}

// Prueba rápida de CreateFileMapping para diagnosticar el entorno
static bool probeSharedMemAvailability()
{
#ifdef _WIN32
    auto name = uniqueMapName("probe");
    HANDLE h = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
        PAGE_READWRITE, 0, 4096, name.toWideCharPointer());
    if (h == nullptr) {
        gSharedMemLastError = (int)GetLastError();
        return false;
    }
    CloseHandle(h);
    return true;
#else
    gSharedMemLastError = -1;
    return false;
#endif
}

// Helpers para tests que requieren shared memory
#define REQUIRE_SHM() do { \
    if (!gSharedMemAvailable) { \
        std::printf("  \xe2\x9a\xa0\xef\xb8\x8f SKIP: shared memory no disponible (error=%d)\n", gSharedMemLastError); \
        return; \
    } \
} while(0)

// ============================================================================
//  0. Diagnóstico del entorno
// ============================================================================
static void test_probe()
{
    std::printf("\n── Shared Memory Probe ──\n");
    if (gSharedMemAvailable) {
        std::printf("  \xe2\x9c\x85 Shared memory disponible\n");
    } else {
        std::printf("  \xe2\x9d\x8c Shared memory NO disponible (GetLastError=%d)\n", gSharedMemLastError);
#ifdef _WIN32
        // Traducir algunos errores comunes
        switch (gSharedMemLastError) {
            case 5:  std::printf("    → ERROR_ACCESS_DENIED: ejecutar como administrador?\n"); break;
            case 8:  std::printf("    → ERROR_NOT_ENOUGH_MEMORY\n"); break;
            case 87: std::printf("    → ERROR_INVALID_PARAMETER\n"); break;
            case 1450: std::printf("    → ERROR_NO_SYSTEM_RESOURCES\n"); break;
            default: break;
        }
#endif
    }
    TEST("shared memory diagnostics complete", true);
}

// ============================================================================
//  1. Constructor y estado inicial
// ============================================================================
static void test_initial_state()
{
    std::printf("\n── Initial State ──\n");
    mixcoach::SharedMemoryManager shm;

    TEST("not initialized after construction", !shm.isInitialized());
    TEST("block is null after construction", shm.getBlock() == nullptr);
    TEST("changeCount is 0 without init", shm.getChangeCount() == 0);
    TEST("healthCheck fails without init", !shm.healthCheck());
}

// ============================================================================
//  2. Inicializar y cerrar
// ============================================================================
static void test_init_close()
{
    std::printf("\n── Init / Close ──\n");
    REQUIRE_SHM();

    mixcoach::SharedMemoryManager shm;
    auto name = uniqueMapName("initclose");
    bool ok = shm.initialize(name);
    TEST("initialize succeeds", ok);
    if (!ok) { std::printf("  \xe2\x9a\xa0\xef\xb8\x8f SKIP: restantes tests\n"); return; }

    TEST("isInitialized after init", shm.isInitialized());
    TEST("block not null after init", shm.getBlock() != nullptr);

    auto* block = shm.getBlock();
    TEST("header initialized==1", block->header.initialized == 1);
    TEST("structVersion matches current",
         block->header.structVersion == mixcoach::SharedMemoryHeader::kCurrentStructVersion);
    TEST("ownerCheck non-zero (timestamp set)", block->header.ownerCheck != 0);

    shm.close();
    TEST("isInitialized false after close", !shm.isInitialized());
    TEST("block null after close", shm.getBlock() == nullptr);
}

// ============================================================================
//  3. Doble inicialización (idempotente)
// ============================================================================
static void test_double_init()
{
    std::printf("\n── Double Init ──\n");
    REQUIRE_SHM();

    mixcoach::SharedMemoryManager shm;
    auto name = uniqueMapName("doubleinit");
    bool ok1 = shm.initialize(name);
    TEST("first init succeeds", ok1);
    if (!ok1) return;

    bool ok2 = shm.initialize(name);
    TEST("second init with same name also succeeds (idempotent)", ok2);
    TEST("still initialized after double init", shm.isInitialized());
    shm.close();
}

// ============================================================================
//  4. Abrir memoria compartida existente (dos managers, mismo nombre)
// ============================================================================
static void test_open_existing()
{
    std::printf("\n── Open Existing ──\n");
    REQUIRE_SHM();

    auto name = uniqueMapName("openexist");
    mixcoach::SharedMemoryManager shm1;
    if (!shm1.initialize(name)) {
        std::printf("  \xe2\x9a\xa0\xef\xb8\x8f SKIP: no se pudo crear shared memory\n");
        return;
    }

    mixcoach::SharedMemoryManager shm2;
    bool ok = shm2.initialize(name);
    TEST("second manager opens existing shared memory", ok);
    if (ok) {
        TEST("second manager initialized", shm2.isInitialized());
        auto* b1 = shm1.getBlock();
        auto* b2 = shm2.getBlock();
        TEST("both managers see initialized==1",
             b1->header.initialized == 1 && b2->header.initialized == 1);
    }

    shm1.close();
    shm2.close();
}

// ============================================================================
//  5. Registrar / leer / liberar slots
// ============================================================================
static void test_register_read_release()
{
    std::printf("\n── Register / Read / Release ──\n");
    REQUIRE_SHM();

    auto name = uniqueMapName("regreadrel");
    mixcoach::SharedMemoryManager shm;
    if (!shm.initialize(name)) { std::printf("  SKIP\n"); return; }

    mixcoach::SharedSlotEntry entry;
    entry.active = 1;
    entry.bus = 0;
    entry.colourARGB = 0xFFE74C3C;
    entry.peakLeft = -6.0f;
    entry.rmsLeft = -18.0f;
    entry.correlation = 0.95f;
    entry.lufsIntegrated = -14.0f;
    std::strncpy(entry.trackName, "Bateria", sizeof(entry.trackName) - 1);
    entry.trackName[sizeof(entry.trackName) - 1] = '\0';

    uint64_t ccBefore = shm.getChangeCount();
    int idx = shm.registerSlot(entry);
    TEST("registerSlot returns index >= 0", idx >= 0);
    TEST("registerSlot on fresh block returns index 0", idx == 0);
    TEST("changeCount incremented after register", shm.getChangeCount() > ccBefore);

    mixcoach::SharedSlotEntry readback;
    bool readOk = shm.readSlot(idx, readback);
    TEST("readSlot succeeds", readOk);
    if (readOk) {
        TEST("readback slot active==1", readback.active == 1);
        TEST("readback slotIndex matches", readback.slotIndex == idx);
        TEST("readback trackName: Bateria",
             std::strncmp(readback.trackName, "Bateria", 64) == 0);
        TEST("readback bus==0 (Drums)", readback.bus == 0);
        TEST("readback colourARGB matches", readback.colourARGB == 0xFFE74C3C);
        TEST_NEAR("readback peakLeft -6.0f", readback.peakLeft, -6.0f, 0.001f);
        TEST_NEAR("readback rmsLeft -18.0f", readback.rmsLeft, -18.0f, 0.001f);
        TEST_NEAR("readback correlation 0.95f", readback.correlation, 0.95f, 0.001f);
        TEST_NEAR("readback lufsIntegrated -14.0f", readback.lufsIntegrated, -14.0f, 0.001f);
    }

    shm.releaseSlot(idx);
    mixcoach::SharedSlotEntry afterRelease;
    shm.readSlot(idx, afterRelease);
    TEST("slot inactive after release", afterRelease.active == 0);
    TEST("slotIndex reset to -1", afterRelease.slotIndex == -1);
    TEST("changeCount incremented after release", shm.getChangeCount() > ccBefore);

    shm.close();
}

// ============================================================================
//  6. Múltiples slots (registrar hasta llenar)
// ============================================================================
static void test_multiple_slots()
{
    std::printf("\n── Multiple Slots ──\n");
    REQUIRE_SHM();

    auto name = uniqueMapName("multislots");
    mixcoach::SharedMemoryManager shm;
    if (!shm.initialize(name)) { std::printf("  SKIP\n"); return; }

    auto makeEntry = [](int active, int bus, const char* name, uint32_t colour) {
        mixcoach::SharedSlotEntry e;
        e.active = active;
        e.bus = bus;
        e.colourARGB = colour;
        std::strncpy(e.trackName, name, sizeof(e.trackName) - 1);
        return e;
    };

    int i0 = shm.registerSlot(makeEntry(1, 0, "Bateria", 0xFFE74C3C));
    int i1 = shm.registerSlot(makeEntry(1, 1, "Bajo",    0xFF3498DB));
    int i2 = shm.registerSlot(makeEntry(1, 2, "Guitarra", 0xFF2ECC71));

    TEST("slot 0 index = 0", i0 == 0);
    TEST("slot 1 index = 1", i1 == 1);
    TEST("slot 2 index = 2", i2 == 2);

    shm.releaseSlot(1);
    int i3 = shm.registerSlot(makeEntry(1, 3, "Teclados", 0xFFF39C12));
    TEST("new slot reuses released index 1", i3 == 1);

    mixcoach::SharedSlotEntry r3;
    shm.readSlot(1, r3);
    TEST("reused slot name: Teclados",
         std::strncmp(r3.trackName, "Teclados", 64) == 0);
    TEST("reused slot bus==3 (Keys)", r3.bus == 3);

    shm.close();
}

// ============================================================================
//  7. WriteSlot (actualización directa)
// ============================================================================
static void test_write_slot()
{
    std::printf("\n── Write Slot ──\n");
    REQUIRE_SHM();

    auto name = uniqueMapName("writeslot");
    mixcoach::SharedMemoryManager shm;
    if (!shm.initialize(name)) { std::printf("  SKIP\n"); return; }

    mixcoach::SharedSlotEntry entry;
    entry.active = 1;
    entry.bus = 4;
    entry.colourARGB = 0xFF9B59B6;
    std::strncpy(entry.trackName, "Vocal", sizeof(entry.trackName) - 1);
    int idx = shm.registerSlot(entry);
    TEST("register returns valid index", idx >= 0);

    mixcoach::SharedSlotEntry update;
    update.active = 1;
    update.bus = 4;
    update.colourARGB = 0xFF9B59B6;
    update.peakLeft = -12.0f;
    update.peakRight = -14.0f;
    update.rmsLeft = -22.0f;
    update.lufsIntegrated = -16.0f;
    std::strncpy(update.trackName, "Vocal", sizeof(update.trackName) - 1);

    uint64_t ccBefore = shm.getChangeCount();
    shm.writeSlot(idx, update);

    mixcoach::SharedSlotEntry afterWrite;
    shm.readSlot(idx, afterWrite);
    TEST_NEAR("writeSlot updates peakLeft", afterWrite.peakLeft, -12.0f, 0.001f);
    TEST_NEAR("writeSlot updates rmsLeft", afterWrite.rmsLeft, -22.0f, 0.001f);
    TEST_NEAR("writeSlot updates lufsIntegrated", afterWrite.lufsIntegrated, -16.0f, 0.001f);
    TEST("writeSlot increments changeCount", shm.getChangeCount() > ccBefore);

    shm.close();
}

// ============================================================================
//  8. Lock acquire / release
// ============================================================================
static void test_locks()
{
    std::printf("\n── Lock Acquire / Release ──\n");
    REQUIRE_SHM();

    auto name = uniqueMapName("locks");
    mixcoach::SharedMemoryManager shm;
    if (!shm.initialize(name)) { std::printf("  SKIP\n"); return; }

    bool acquired = shm.acquireLock(500);
    TEST("acquireLock succeeds", acquired);

    bool reacquire = shm.acquireLock(100);
    TEST("acquireLock while already held returns false", !reacquire);

    shm.releaseLock();

    bool reacquired = shm.acquireLock(500);
    TEST("acquireLock succeeds after release", reacquired);
    shm.releaseLock();

    mixcoach::SharedMemoryManager shm2;
    bool noInit = shm2.acquireLock(10);
    TEST("acquireLock without init returns false", !noInit);

    shm.close();
}

// ============================================================================
//  9. Health check
// ============================================================================
static void test_health_check()
{
    std::printf("\n── Health Check ──\n");

    mixcoach::SharedMemoryManager shm;
    TEST("healthCheck false before init", !shm.healthCheck());
    REQUIRE_SHM();

    auto name = uniqueMapName("health");
    if (!shm.initialize(name)) { std::printf("  SKIP\n"); return; }

    TEST("healthCheck true after init", shm.healthCheck());
    shm.close();

    bool afterClose = shm.healthCheck();
    TEST("healthCheck false after close", !afterClose);
}

// ============================================================================
// 10. Reconnect
// ============================================================================
static void test_reconnect()
{
    std::printf("\n── Reconnect ──\n");
    REQUIRE_SHM();

    auto name = uniqueMapName("reconnect");
    mixcoach::SharedMemoryManager shm;
    if (!shm.initialize(name)) { std::printf("  SKIP\n"); return; }

    mixcoach::SharedSlotEntry entry;
    entry.active = 1;
    std::strncpy(entry.trackName, "Track1", sizeof(entry.trackName) - 1);
    int idx = shm.registerSlot(entry);
    TEST("slot registered before reconnect", idx >= 0);

    bool reconnected = shm.reconnect();
    TEST("reconnect succeeds", reconnected);
    TEST("isInitialized after reconnect", shm.isInitialized());
    if (reconnected) {
        TEST("healthCheck passes after reconnect", shm.healthCheck());
        auto* block = shm.getBlock();
        TEST("block is not null after reconnect", block != nullptr);
    }

    shm.close();
}

// ============================================================================
// 11. Límites: índices inválidos
// ============================================================================
static void test_boundaries()
{
    std::printf("\n── Boundaries ──\n");
    REQUIRE_SHM();

    auto name = uniqueMapName("boundaries");
    mixcoach::SharedMemoryManager shm;
    if (!shm.initialize(name)) { std::printf("  SKIP\n"); return; }

    mixcoach::SharedSlotEntry out;
    TEST("readSlot -1 returns false", !shm.readSlot(-1, out));
    TEST("readSlot kSharedMaxSlots returns false",
         !shm.readSlot(mixcoach::kSharedMaxSlots, out));

    mixcoach::SharedSlotEntry entry;
    entry.active = 1;
    shm.writeSlot(-1, entry);
    TEST("writeSlot -1 does not crash", true);
    shm.writeSlot(mixcoach::kSharedMaxSlots, entry);
    TEST("writeSlot kSharedMaxSlots does not crash", true);

    shm.releaseSlot(-1);
    TEST("releaseSlot -1 does not crash", true);
    shm.releaseSlot(mixcoach::kSharedMaxSlots);
    TEST("releaseSlot kSharedMaxSlots does not crash", true);

    mixcoach::SharedMemoryManager shm2;
    int noInit = shm2.registerSlot(entry);
    TEST("registerSlot without init returns -1", noInit == -1);

    shm.close();
}

// ============================================================================
// 12. Llenar todos los slots y verificar overflow
// ============================================================================
static void test_full_registry()
{
    std::printf("\n── Full Registry ──\n");
    REQUIRE_SHM();

    auto name = uniqueMapName("fullreg");
    mixcoach::SharedMemoryManager shm;
    if (!shm.initialize(name)) { std::printf("  SKIP\n"); return; }

    mixcoach::SharedSlotEntry entry;
    entry.active = 1;

    int lastIdx = -1;
    for (int i = 0; i < mixcoach::kSharedMaxSlots; ++i) {
        entry.bus = i % 6;
        std::snprintf(entry.trackName, sizeof(entry.trackName), "Track_%d", i);
        int idx = shm.registerSlot(entry);
        TEST("registerSlot returns valid index while filling",
             idx >= 0 && idx < mixcoach::kSharedMaxSlots);
        lastIdx = idx;
    }
    TEST("last registered index = kSharedMaxSlots - 1",
         lastIdx == mixcoach::kSharedMaxSlots - 1);

    int overflow = shm.registerSlot(entry);
    TEST("registerSlot returns -1 when full", overflow == -1);

    shm.releaseSlot(0);
    int reuse = shm.registerSlot(entry);
    TEST("registerSlot succeeds after release one slot", reuse >= 0);

    shm.close();
}

// ============================================================================
//  Main
// ============================================================================

int main()
{
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  SharedMemoryManager Unit Tests\n");
    std::printf("  Init  |  Register  |  Read  |  Write  |  Lock  |  Health  |  Boundaries\n");
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");

    // Detectar disponibilidad de shared memory al inicio
    gSharedMemAvailable = probeSharedMemAvailability();

    test_probe();
    test_initial_state();
    test_init_close();
    test_double_init();
    test_open_existing();
    test_register_read_release();
    test_multiple_slots();
    test_write_slot();
    test_locks();
    test_health_check();
    test_reconnect();
    test_boundaries();
    test_full_registry();

    std::printf("\n\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");

    return gTestsFailed > 0 ? 1 : 0;
}
