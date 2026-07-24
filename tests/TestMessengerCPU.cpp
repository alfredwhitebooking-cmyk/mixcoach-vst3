// ═══════════════════════════════════════════════════════════════════════════
//  TestMessengerCPU.cpp — CPU benchmark del Messenger processBlock()
//
//  Simula 100,000 llamadas a processBlock() midiendo el tiempo de CPU
//  de las operaciones críticas: audio passthrough, escritura a shared
//  memory, signal detection, y heartbeat atómico.
//
//  Objetivo: verificar que cada llamada toma <1μs en promedio.
//
//  Build: cmake --build build --config Release --target TestMessengerCPU
//  Run:   build/tests/Release/TestMessengerCPU.exe
// ═══════════════════════════════════════════════════════════════════════════

#include <cstdio>
#include <cstring>
#include <cmath>
#include <chrono>
#include <atomic>
#include <vector>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#pragma intrinsic(__rdtsc)
#endif

// ─── Constantes simuladas (matching Messenger real) ──────────────────────────
static constexpr int kBlockSize   = 512;   // Samples per block @ 48kHz
static constexpr int kNumChannels = 2;     // Stereo
static constexpr int kIterations  = 100000;
static constexpr int kRampUp      = 1000;  // Descartar primeros N para warm-up
static constexpr int kSignalCheckStride = 64; // 1 de cada 64 samples

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

// ─── High-resolution timer ─────────────────────────────────────────────────
struct Timer {
    using Clock = std::chrono::steady_clock;
    Clock::time_point start;

    Timer() : start(Clock::now()) {}

    double elapsedUs() const {
        auto end = Clock::now();
        return std::chrono::duration<double, std::micro>(end - start).count();
    }

    void reset() { start = Clock::now(); }
};

// ─── RDTCS cycle counter (x64) ─────────────────────────────────────────────
#ifdef _WIN32
static inline uint64_t rdtsc() {
    return __rdtsc();
}
#else
static inline uint64_t rdtsc() { return 0; }
#endif

// ═══════════════════════════════════════════════════════════════════════════
//  Benchmark 1: Audio passthrough + signal detection
//  Simula lo que hace Messenger en processBlock():
//    - juce::ScopedNoDenormals (CPU flag, no data touched)
//    - Audio signal detection (1/64 samples check)
//    - Audio write to shared memory (memcpy simulation)
//    - Atomic heartbeat (InterlockedExchange64)
// ═══════════════════════════════════════════════════════════════════════════
static void benchmark_processBlock()
{
    std::printf("\n── Benchmark: processBlock() — %d iteraciones ──\n", kIterations);

    // ─── Preparar buffers de audio simulados ─────────────────────────────
    // Simula el buffer que el DAW pasa a processBlock
    std::vector<float> audioBuffer(kBlockSize * kNumChannels, 0.0f);

    // Llenar con señal realista (seno a 440Hz + ruido)
    for (int i = 0; i < kBlockSize; ++i) {
        float t = (float)i / (float)kBlockSize;
        audioBuffer[i * 2]     = 0.3f * std::sin(2.0f * 3.14159f * 440.0f * t);
        audioBuffer[i * 2 + 1] = 0.3f * std::sin(2.0f * 3.14159f * 440.0f * t + 0.1f);
    }

    // ─── Buffers destino (simulando SharedAudioMemoryV2 ring buffer) ─────
    static constexpr int kRingBufferSize = 8192; // Matching kAudioBufferSize
    std::vector<float> ringBufferL(kRingBufferSize, 0.0f);
    std::vector<float> ringBufferR(kRingBufferSize, 0.0f);
    std::atomic<int64_t> writePosL{0};
    std::atomic<int64_t> writePosR{0};

    // ─── Heartbeat simulado ──────────────────────────────────────────────
    std::atomic<int64_t> heartbeat{0};

    // ─── Signal detection state ──────────────────────────────────────────
    std::atomic<int> signalState{0}; // 0 = Waiting, 1 = Detected

    // ─── Warm-up: descartar primeros kRampUp ────────────────────────────
    {
        float dummySum = 0.0f;
        for (int iter = 0; iter < kRampUp; ++iter) {
            const float* ch0 = audioBuffer.data();
            const float* ch1 = audioBuffer.data() + 1;

            // Signal detection (1/64 samples)
            if (signalState.load(std::memory_order_relaxed) == 0) {
                for (int i = 0; i < kBlockSize; i += kSignalCheckStride) {
                    if (std::abs(ch0[i * 2]) > 0.0005f) {
                        signalState.store(1, std::memory_order_relaxed);
                        break;
                    }
                }
            }

            // Audio write to ring buffer (core cost: memcpy float arrays)
            int64_t wpL = writePosL.load(std::memory_order_relaxed);
            int64_t wpR = writePosR.load(std::memory_order_relaxed);
            for (int i = 0; i < kBlockSize; ++i) {
                ringBufferL[(wpL + i) % kRingBufferSize] = ch0[i * 2];
                ringBufferR[(wpR + i) % kRingBufferSize] = ch1[i * 2];
            }
            writePosL.store(wpL + kBlockSize, std::memory_order_release);
            writePosR.store(wpR + kBlockSize, std::memory_order_release);

            // Heartbeat (InterlockedExchange64)
            int64_t now = (int64_t)iter;
            heartbeat.store(now, std::memory_order_relaxed);

            // Prevent compiler from optimizing away
            dummySum += ringBufferL[wpL % kRingBufferSize];
        }
        (void)dummySum;
    }

    // ─── Benchmark real ─────────────────────────────────────────────────
    double totalTimeUs = 0.0;
    uint64_t totalCycles = 0;
    int sampleCount = 0;
    double minTimeUs = 1e9;
    double maxTimeUs = 0.0;

    Timer timer;
    uint64_t tscStart = rdtsc();

    for (int iter = 0; iter < kIterations; ++iter) {
        Timer iterTimer;

        const float* ch0 = audioBuffer.data();
        const float* ch1 = audioBuffer.data() + 1;

        // ─── 1. ScopedNoDenormals (CPU flag, zero runtime cost) ──────────
        // En CPU modernas, DAZ/FTZ flags están siempre activos. No medible.

        // ─── 2. Audio signal detection (1/64 samples) ────────────────────
        if (signalState.load(std::memory_order_relaxed) == 0) {
            for (int i = 0; i < kBlockSize; i += kSignalCheckStride) {
                if (std::abs(ch0[i * 2]) > 0.0005f) {
                    signalState.store(1, std::memory_order_relaxed);
                    break;
                }
            }
        }

        // ─── 3. Audio write to ring buffer (main CPU cost) ───────────────
        int64_t wpL = writePosL.load(std::memory_order_relaxed);
        int64_t wpR = writePosR.load(std::memory_order_relaxed);
        for (int i = 0; i < kBlockSize; ++i) {
            ringBufferL[(wpL + i) % kRingBufferSize] = ch0[i * 2];
            ringBufferR[(wpR + i) % kRingBufferSize] = ch1[i * 2];
        }
        writePosL.store(wpL + kBlockSize, std::memory_order_release);
        writePosR.store(wpR + kBlockSize, std::memory_order_release);

        // ─── 4. Heartbeat (InterlockedExchange64) ────────────────────────
        heartbeat.store((int64_t)iter, std::memory_order_relaxed);

        double iterTime = iterTimer.elapsedUs();
        totalTimeUs += iterTime;
        if (iterTime < minTimeUs) minTimeUs = iterTime;
        if (iterTime > maxTimeUs) maxTimeUs = iterTime;
        sampleCount++;

        // Prevent compiler from optimizing away the writes
        if (iter == kIterations - 1) {
            volatile float sink = ringBufferL[wpL % kRingBufferSize];
            (void)sink;
        }
    }

    uint64_t tscEnd = rdtsc();
    totalCycles = tscEnd - tscStart;

    double avgTimeUs = totalTimeUs / sampleCount;
    double avgCycles = (double)totalCycles / sampleCount;
    double callsPerSec = 1.0e6 / avgTimeUs;

    std::printf("  Resultados (%d muestras, tras %d warm-up):\n", sampleCount, kRampUp);
    std::printf("    Tiempo promedio por llamada:  %.3f μs\n", avgTimeUs);
    std::printf("    Tiempo mínimo:                %.3f μs\n", minTimeUs);
    std::printf("    Tiempo máximo:                %.3f μs\n", maxTimeUs);
    std::printf("    Ciclos CPU promedio:          %.0f ciclos\n", avgCycles);
    std::printf("    Llamadas por segundo:         %.0f calls/s\n", callsPerSec);
    std::printf("    Throughput equivalente:       %.0f tracks @ 48kHz\n", callsPerSec / 93.75);

    // Verificar <1μs por llamada
    TEST("Cada llamada < 1.0 μs en promedio", avgTimeUs < 1.0);
    TEST("Mínimo < 0.5 μs (caché caliente)", minTimeUs < 0.5);
    TEST("Throughput > 1,000,000 calls/s", callsPerSec > 1.0e6);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Benchmark 2: writeStereoSamples() puro (sin overhead de processBlock)
//  Mide solo el costo de copiar float arrays al ring buffer.
// ═══════════════════════════════════════════════════════════════════════════
static void benchmark_writeStereoSamples()
{
    std::printf("\n── Benchmark: writeStereoSamples() puro — %d iteraciones ──\n", kIterations);

    static constexpr int kRingBufferSize = 8192;
    std::vector<float> ringBufferL(kRingBufferSize, 0.0f);
    std::vector<float> ringBufferR(kRingBufferSize, 0.0f);
    std::vector<float> srcL(kBlockSize, 0.5f);
    std::vector<float> srcR(kBlockSize, 0.3f);
    std::atomic<int64_t> writePosL{0};

    // Warm-up
    for (int i = 0; i < kRampUp; ++i) {
        int64_t wp = writePosL.load(std::memory_order_relaxed);
        for (int j = 0; j < kBlockSize; ++j) {
            ringBufferL[(wp + j) % kRingBufferSize] = srcL[j];
            ringBufferR[(wp + j) % kRingBufferSize] = srcR[j];
        }
        writePosL.store(wp + kBlockSize, std::memory_order_release);
    }

    Timer timer;
    for (int iter = 0; iter < kIterations; ++iter) {
        int64_t wp = writePosL.load(std::memory_order_relaxed);
        for (int j = 0; j < kBlockSize; ++j) {
            ringBufferL[(wp + j) % kRingBufferSize] = srcL[j];
            ringBufferR[(wp + j) % kRingBufferSize] = srcR[j];
        }
        writePosL.store(wp + kBlockSize, std::memory_order_release);
    }
    double totalUs = timer.elapsedUs();
    double avgUs = totalUs / kIterations;

    std::printf("  Tiempo promedio: %.3f μs (%.0f MB/s)\n", avgUs,
                (double)(kBlockSize * 2 * 4) / (avgUs * 1e-6) / 1.0e6);

    TEST("writeStereoSamples < 0.5 μs", avgUs < 0.5);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Benchmark 3: Heartbeat atómico puro (InterlockedExchange64)
// ═══════════════════════════════════════════════════════════════════════════
static void benchmark_heartbeat()
{
    std::printf("\n── Benchmark: Heartbeat atómico — %d iteraciones ──\n", kIterations);

    std::atomic<int64_t> heartbeat{0};

    Timer timer;
    for (int iter = 0; iter < kIterations; ++iter) {
        heartbeat.store((int64_t)iter, std::memory_order_relaxed);
    }
    double totalUs = timer.elapsedUs();
    double avgUs = totalUs / kIterations;

    std::printf("  Tiempo promedio: %.4f μs (%.0fM ops/s)\n", avgUs, 1.0e6 / avgUs / 1.0e6);

    TEST("Heartbeat < 0.05 μs", avgUs < 0.05);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Benchmark 4: Señal de audio (1/64 samples check)
// ═══════════════════════════════════════════════════════════════════════════
static void benchmark_signalDetection()
{
    std::printf("\n── Benchmark: Signal Detection (1/64 samples) — %d iteraciones ──\n", kIterations);

    std::vector<float> buffer(kBlockSize * 2, 0.001f); // Justo sobre el threshold
    std::atomic<int> signalState{0};

    Timer timer;
    for (int iter = 0; iter < kIterations; ++iter) {
        if (signalState.load(std::memory_order_relaxed) == 0) {
            const float* ch0 = buffer.data();
            for (int i = 0; i < kBlockSize; i += kSignalCheckStride) {
                if (std::abs(ch0[i * 2]) > 0.0005f) {
                    signalState.store(1, std::memory_order_relaxed);
                    break;
                }
            }
        }
    }
    double totalUs = timer.elapsedUs();
    double avgUs = totalUs / kIterations;

    std::printf("  Tiempo promedio: %.4f μs (solo 1ra iteración tiene costo real)\n", avgUs);

    // Después de la primera detección, solo hay un load atómico
    TEST("Signal detection overhead < 0.02 μs en steady state", avgUs < 0.02);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════════════
int main()
{
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");
    std::printf("  MixCoach Messenger CPU Benchmark\n");
    std::printf("  %d iteraciones  |  Block: %d samples  |  Stereo\n", kIterations, kBlockSize);
    std::printf("\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90"
                "\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\xe2\x95\x90\n");

    // CPU info
#ifdef _WIN32
    int cpuInfo[4] = {};
    __cpuid(cpuInfo, 0);
    char cpuBrand[49] = {};
    __cpuid(cpuInfo, 0x80000002);
    std::memcpy(cpuBrand, cpuInfo, sizeof(cpuInfo));
    __cpuid(cpuInfo, 0x80000003);
    std::memcpy(cpuBrand + 16, cpuInfo, sizeof(cpuInfo));
    __cpuid(cpuInfo, 0x80000004);
    std::memcpy(cpuBrand + 32, cpuInfo, sizeof(cpuInfo));
    std::printf("  CPU: %s\n", cpuBrand);
#endif

    // Prioridad alta para mediciones estables
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#endif

    // ─── Ejecutar benchmarks ────────────────────────────────────────────
    benchmark_processBlock();
    benchmark_writeStereoSamples();
    benchmark_heartbeat();
    benchmark_signalDetection();

    // ─── Resumen ────────────────────────────────────────────────────────
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
