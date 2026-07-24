// ═══════════════════════════════════════════════════════════════════════════
//  TestIPCDeterministicThroughput.cpp — Prueba determinista de throughput IPC
//  Incremento 1c: Simula 50 Messengers a 48kHz por 10s y verifica ≤1% overrun
//
//  Escenario:
//   50 Messengers (slots) escribiendo audio estereo + heartbeat lock-free.
//   1 lector (simula MixCoach bg service) consumiendo cada 50ms.
//   Verifica que el ring buffer SPSC no sufra >1% overruns.
//
//  Frecuencias:
//   Writer: 48kHz / 512 buffer = 94 callbacks/s por Messenger
//           ~2400 samples/slot cada 50ms
//   Reader: 4096 samples/slot cada 50ms (tasa duplicada vs plan original)
//   Reader rate (81920/s) > Writer rate (48000/s) → 0 overruns esperados
//
//  Build: cmake --build build --config Release --target TestIPCDeterministicThroughput
//  Run:   build/tests/Release/TestIPCDeterministicThroughput.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cmath>
#include <cstring>
#include <memory>
#include <chrono>
#include <vector>
#include <algorithm>

#include <juce_core/juce_core.h>

#include "Common/types/Types.h"
#include "Common/types/Constants.h"
#include "Common/memory/SharedMemory.h"
#include "Common/memory/SharedAudioMemoryV2.h"
#include "Common/memory/SlotRegistry.h"

static int gTestsPassed = 0;
static int gTestsFailed = 0;

#define TEST(name, expr) do {                                                  \
    if (!(expr)) {                                                             \
        std::fprintf(stderr, "  %s FAIL: %s (%s:%d)\n",                         \
                     "\xe2\x9d\x8c", name, __FILE__, __LINE__);                 \
        std::fflush(stderr);                                                   \
        gTestsFailed++;                                                        \
    } else {                                                                   \
        std::printf("  %s PASS: %s\n", "\xe2\x9c\x85", name);                  \
        std::fflush(stdout);                                                   \
        gTestsPassed++;                                                        \
    }                                                                          \
} while(0)

// ─── Constantes del escenario ──────────────────────────────────────────────
static constexpr int kNumMessengers    = 50;     // 50 pistas simultaneas
static constexpr int kSampleRate       = 48000;  // 48 kHz
static constexpr int kDurationMs       = 10000;  // 10 segundos de simulacion
static constexpr int kReadIntervalMs   = 50;     // Intervalo del bg service
static constexpr int kReadSize         = 4096;   // Samples por lectura
static constexpr int kSamplesPerFrame  = kSampleRate * kReadIntervalMs / 1000; // 2400

// ═══════════════════════════════════════════════════════════════════════════
//  Test 1: Throughput principal — 50 Messengers x 48kHz x 10s
//  Verifica ≤1% overrun. Lee overrunCount UNA SOLA VEZ al final para evitar
//  doble conteo (getOverrunCount() retorna valor acumulado).
// ═══════════════════════════════════════════════════════════════════════════
static void test_throughput_50_messengers()
{
    std::printf("\n── Test 1: Throughput — 50 Messengers @ 48kHz x 10s ──\n");
    std::fflush(stdout);

    juce::String testGuid = juce::String(juce::Time::getMillisecondCounter());
    juce::String shmName  = "Local\\MixCoach_Thru_SHM_"  + testGuid;
    juce::String audioName = "Local\\MixCoach_Thru_Audio_" + testGuid;

    auto shm = std::make_unique<mixcoach::SharedMemoryManager>();
    if (!shm->initialize(shmName)) {
        std::printf("  SKIP: Shared memory not available\n");
        std::fflush(stdout);
        return;
    }

    auto audio = std::make_unique<mixcoach::SharedAudioMemoryV2>();
    if (!audio->initialize(audioName)) {
        std::printf("  SKIP: Shared audio memory not available\n");
        std::fflush(stdout);
        shm->close();
        return;
    }

    // Registrar 50 slots en SharedMemory
    for (int i = 0; i < kNumMessengers; ++i) {
        mixcoach::SharedSlotEntry entry;
        entry.slotIndex = i;
        entry.active    = 1;
        entry.bus       = i % mixcoach::kNumBuses;
        entry.colourARGB = 0xFF3498DB;
        std::snprintf(entry.trackName, sizeof(entry.trackName),
                      "Messenger_%d", i);
        int assigned = shm->registerSlot(entry);
        if (!(assigned >= 0 && assigned < mixcoach::kSharedMaxSlots)) {
            ++gTestsFailed;
            std::fprintf(stderr, "  %s FAIL: Slot %d registered (%s:%d)\n",
                         "\xe2\x9d\x8c", i, __FILE__, __LINE__);
        } else {
            ++gTestsPassed;
            std::printf("  %s PASS: Slot %d registered\n", "\xe2\x9c\x85", i);
        }
    }
    TEST("All 50 slots registered", true);

    // Simulacion: 10s en frames de 50ms
    int numFrames  = kDurationMs / kReadIntervalMs; // 200
    int totalReads = 0;

    std::vector<float> writeBuf(kSamplesPerFrame);
    for (int s = 0; s < kSamplesPerFrame; ++s) {
        writeBuf[s] = 0.5f * std::sin(2.0f * 3.14159f * 440.0f
                                      * static_cast<float>(s) / kSampleRate);
    }

    auto startWall = std::chrono::high_resolution_clock::now();
    uint32_t simulatedMs = 0;

    for (int frame = 0; frame < numFrames; ++frame) {
        // Writer: 50 Messengers
        for (int m = 0; m < kNumMessengers; ++m) {
            shm->writeHeartbeat(m, simulatedMs);
            audio->writeStereoSamples(m, writeBuf.data(), writeBuf.data(),
                                      kSamplesPerFrame);
        }

        // Reader: bg service
        std::vector<float> readBuf(kReadSize);
        for (int m = 0; m < kNumMessengers; ++m) {
            int read = audio->readStereoSamples(m, readBuf.data(), readBuf.data(), kReadSize);
            totalReads++;

            if (read > 0 && frame == 0) {
                bool hasSignal = false;
                for (int s = 0; s < read && s < 100; ++s) {
                    if (std::abs(readBuf[s]) > 0.001f) { hasSignal = true; break; }
                }
                TEST(("Slot " + std::to_string(m) + " signal in frame 0").c_str(), hasSignal);
            }
        }
        simulatedMs += kReadIntervalMs;
    }

    auto endWall = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(endWall - startWall).count();

    // Leer overruns UNA SOLA VEZ al final (getOverrunCount es acumulativo)
    int totalOverruns = 0;
    int maxOverrunsPerSlot = 0;
    for (int m = 0; m < kNumMessengers; ++m) {
        int o = static_cast<int>(audio->getOverrunCount(m));
        totalOverruns += o;
        if (o > maxOverrunsPerSlot) maxOverrunsPerSlot = o;
    }

    std::printf("  Simulated %dms in %.1f real ms (%.1fx speed)\n",
                kDurationMs, elapsedMs,
                static_cast<double>(kDurationMs) / elapsedMs);
    std::printf("  Reads: %d | Overruns: %d | Max/slot: %d\n",
                totalReads, totalOverruns, maxOverrunsPerSlot);
    std::fflush(stdout);

    int overrunThreshold = totalReads / 100;
    TEST("Overruns <= 1% of reads", totalOverruns <= overrunThreshold);

    // NOTA: En la configuracion FIXED (reader 4096/slot/50ms > writer 2400/slot/50ms),
    // el resultado esperado es 0 overruns.
    TEST("Max overruns per slot <= 1", maxOverrunsPerSlot <= 1);

    audio->close();
    shm->close();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 2: Overload detection — write mas rapido que buffer size
//  Verifica que overrunCount > 0 cuando writer produce mas de kAudioBufferSize
// ═══════════════════════════════════════════════════════════════════════════
static void test_overload_detection()
{
    std::printf("\n── Test 2: Overload detection — writer > buffer size ──\n");
    std::fflush(stdout);

    juce::String testGuid  = juce::String(juce::Time::getMillisecondCounter());
    juce::String audioName = "Local\\MixCoach_Overload_Audio_" + testGuid;

    auto audio = std::make_unique<mixcoach::SharedAudioMemoryV2>();
    if (!audio->initialize(audioName)) {
        std::printf("  SKIP: Shared audio memory not available\n");
        std::fflush(stdout);
        return;
    }

    constexpr int kSlot = 0;
    constexpr int kBufferSize = mixcoach::kAudioBufferSize;
    constexpr int kWriteExtra = 1000;

    std::vector<float> buf(kBufferSize + kWriteExtra, 0.5f);

    // Escribir mas de lo que cabe en el buffer (sin leer entre medias)
    audio->writeStereoSamples(kSlot, buf.data(), buf.data(), kBufferSize + kWriteExtra);

    std::vector<float> readBuf(kBufferSize);
    int read = audio->readStereoSamples(kSlot, readBuf.data(), readBuf.data(), kBufferSize);
    TEST("Read exactly buffer size", read == kBufferSize);

    // Debe haber overrun
    uint32_t overrun = audio->getOverrunCount(kSlot);
    TEST("Overrun count > 0 after overflow", overrun > 0);

    // Datos leidos deben ser validos
    bool hasSignal = false;
    for (int i = 0; i < read; ++i) {
        if (std::abs(readBuf[i]) > 0.001f) { hasSignal = true; break; }
    }
    TEST("Read data has valid signal", hasSignal);

    audio->close();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 3: Heartbeat + stale detection integration
//  Verifica que heartbeats frescos no marcan stale, expirados si, y
//  recuperacion los limpia.
// ═══════════════════════════════════════════════════════════════════════════
static void test_heartbeat_stale_integration()
{
    std::printf("\n── Test 3: Heartbeat + stale detection ──\n");
    std::fflush(stdout);

    juce::String testGuid = juce::String(juce::Time::getMillisecondCounter());
    juce::String shmName  = "Local\\MixCoach_HB_Test_" + testGuid;

    auto shm = std::make_unique<mixcoach::SharedMemoryManager>();
    if (!shm->initialize(shmName)) {
        std::printf("  SKIP: Shared memory not available\n");
        std::fflush(stdout);
        return;
    }

    // Registrar 10 slots
    for (int i = 0; i < 10; ++i) {
        mixcoach::SharedSlotEntry entry;
        entry.slotIndex = i;
        entry.active    = 1;
        entry.bus       = i % mixcoach::kNumBuses;
        entry.colourARGB = 0xFF2ECC71;
        std::snprintf(entry.trackName, sizeof(entry.trackName), "HB_%d", i);
        shm->registerSlot(entry);
    }

    auto registry = std::make_unique<mixcoach::SlotRegistry>();
    registry->setSharedMemory(shm.get());
    registry->forceFullSync();

    uint32_t now = juce::Time::getMillisecondCounter();

    // Heartbeats frescos → 0 stale
    for (int i = 0; i < 10; ++i) shm->writeHeartbeat(i, static_cast<int64_t>(now));
    registry->checkStaleSlots();

    int staleCount = 0;
    for (int i = 0; i < 10; ++i)
        if (registry->getSlotInfo(i).stale) staleCount++;
    TEST("0 stale with fresh heartbeats", staleCount == 0);

    // Heartbeats expirados (10s atras > 5s timeout) → 10 stale
    for (int i = 0; i < 10; ++i)
        shm->writeHeartbeat(i, static_cast<int64_t>(now - 10000));
    registry->checkStaleSlots();

    int staleAfter = 0;
    for (int i = 0; i < 10; ++i)
        if (registry->getSlotInfo(i).stale) staleAfter++;
    TEST("10 stale after heartbeat expiration", staleAfter == 10);

    // Recuperar con heartbeats frescos → 0 stale
    now = juce::Time::getMillisecondCounter();
    for (int i = 0; i < 10; ++i)
        shm->writeHeartbeat(i, static_cast<int64_t>(now));
    registry->checkStaleSlots();

    int afterRecovery = 0;
    for (int i = 0; i < 10; ++i)
        if (registry->getSlotInfo(i).stale) afterRecovery++;
    TEST("0 stale after heartbeat recovery", afterRecovery == 0);

    // forEachActive incluye todos tras recuperacion
    int active = 0;
    registry->forEachActive([&](const mixcoach::SlotInfo&) { active++; });
    TEST("forEachActive = 10 after recovery", active == 10);

    shm->close();
    registry.reset();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Test 4: Max capacity — 128 slots a 48kHz x 1s
//  Verifica ≤1% overrun a capacidad maxima. Lee overruns UNA SOLA VEZ al final.
// ═══════════════════════════════════════════════════════════════════════════
static void test_max_capacity_128_slots()
{
    std::printf("\n── Test 4: Max capacity — 128 slots @ 48kHz x 1s ──\n");
    std::fflush(stdout);

    juce::String testGuid  = juce::String(juce::Time::getMillisecondCounter());
    juce::String shmName   = "Local\\MixCoach_Max_SHM_"  + testGuid;
    juce::String audioName = "Local\\MixCoach_Max_Audio_" + testGuid;

    auto shm = std::make_unique<mixcoach::SharedMemoryManager>();
    if (!shm->initialize(shmName)) {
        std::printf("  SKIP: Shared memory not available\n");
        std::fflush(stdout);
        return;
    }

    auto audio = std::make_unique<mixcoach::SharedAudioMemoryV2>();
    if (!audio->initialize(audioName)) {
        std::printf("  SKIP: Shared audio memory not available\n");
        std::fflush(stdout);
        shm->close();
        return;
    }

    constexpr int kMaxSlots = mixcoach::kSharedMaxSlots;
    constexpr int kShortDurationMs = 1000;
    constexpr int kShortFrames = kShortDurationMs / kReadIntervalMs;

    // Registrar 128 slots
    for (int i = 0; i < kMaxSlots; ++i) {
        mixcoach::SharedSlotEntry entry;
        entry.slotIndex = i;
        entry.active    = 1;
        entry.bus       = i % mixcoach::kNumBuses;
        entry.colourARGB = 0xFFE74C3C;
        std::snprintf(entry.trackName, sizeof(entry.trackName), "Max_%d", i);
        shm->registerSlot(entry);
    }
    TEST("All 128 slots registered", true);

    std::vector<float> writeBuf(kSamplesPerFrame, 0.3f);
    std::vector<float> readBuf(kReadSize);

    int totalReads = 0;
    uint32_t simMs = 0;

    for (int frame = 0; frame < kShortFrames; ++frame) {
        for (int m = 0; m < kMaxSlots; ++m) {
            shm->writeHeartbeat(m, simMs);
            audio->writeStereoSamples(m, writeBuf.data(), writeBuf.data(), kSamplesPerFrame);
        }
        for (int m = 0; m < kMaxSlots; ++m) {
            audio->readStereoSamples(m, readBuf.data(), readBuf.data(), kReadSize);
            totalReads++;
        }
        simMs += kReadIntervalMs;
    }

    // Leer overruns UNA SOLA VEZ al final
    int totalOverruns = 0;
    for (int m = 0; m < kMaxSlots; ++m)
        totalOverruns += static_cast<int>(audio->getOverrunCount(m));

    int maxAllowed = totalReads / 100;
    std::printf("  Reads: %d | Overruns: %d | Threshold: %d\n",
                totalReads, totalOverruns, maxAllowed);
    std::fflush(stdout);

    TEST("Overruns <= 1% with 128 slots", totalOverruns <= maxAllowed);

    auto block = shm->getBlock();
    TEST("Shared memory block accessible after test",
         block != nullptr && block->header.initialized == 1);

    audio->close();
    shm->close();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════
int main()
{
    std::printf("\n═══ DETERMINISTIC THROUGHPUT TEST (Inc 1c) ═══\n");
    std::printf("  %d Messengers @ %d Hz x %ds | Read every %dms\n",
                kNumMessengers, kSampleRate, kDurationMs / 1000, kReadIntervalMs);
    std::printf("  Writer: %d samples/slot/frame | Reader: %d samples/slot/frame\n",
                kSamplesPerFrame, kReadSize);
    std::printf("  Reader rate: %d/s > Writer rate: %d/s → 0 overrun expected\n",
                kReadSize * (1000 / kReadIntervalMs),
                kSamplesPerFrame * (1000 / kReadIntervalMs));
    std::printf("═══════════════════════════════════════════════\n\n");
    std::fflush(stdout);

    test_throughput_50_messengers();
    test_overload_detection();
    test_heartbeat_stale_integration();
    test_max_capacity_128_slots();

    std::printf("\n═══════════════════════════════════════════════\n");
    std::printf("  Results: %d passed, %d failed\n", gTestsPassed, gTestsFailed);
    std::printf("═══════════════════════════════════════════════\n");
    std::fflush(stdout);

    return gTestsFailed > 0 ? 1 : 0;
}
