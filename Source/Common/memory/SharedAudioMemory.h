#pragma once
#include <cstdint>
#include <cstring>
#include <juce_core/juce_core.h>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  SharedAudioMemory — Memoria compartida para audio RAW entre procesos
//
//  Usa su propio CreateFileMappingW (separado de SharedMemory de identidad).
//  Cada slot tiene un ring buffer lock-free con Interlocked operations.
//
//  Messenger escribe samples RAW → MixCoach lee para análisis.
//  128 slots × 4096 samples × 4 bytes = ~2 MB.
// ═══════════════════════════════════════════════════════════════════════════

static constexpr int kAudioBufferSize = 4096;   // ~85ms @ 48kHz
static constexpr int kMaxAudioSlots  = 128;

#pragma pack(push, 8)
struct SharedAudioSlot {
    // Posiciones atómicas para productor/consumidor cross-process.
    // Usamos volatile int64_t directamente (no std::atomic) porque garantiza
    // operaciones atómicas entre procesos en memoria compartida en x64/x86.
    // En MSVC, volatile implícitamente tiene semántica de barrera de memoria.
    // Nota: Para ARM se requeriría InterlockedExchange64.
    volatile int64_t writePos;  // Solo el escritor (Messenger) incrementa
    volatile int64_t readPos;   // Solo el lector (MixCoach) incrementa
    float            buffer[kAudioBufferSize];  // Samples mono RAW
};
#pragma pack(pop)

// ─── Header del bloque de audio compartido ─────────────────────────────────
#pragma pack(push, 8)
struct SharedAudioHeader {
    int initialized;                // 1 si está listo
    int padding[7];                 // Alineación a 64 bytes
};
#pragma pack(pop)

// ─── Bloque completo ───────────────────────────────────────────────────────
#pragma pack(push, 8)
struct SharedAudioBlock {
    SharedAudioHeader header;
    SharedAudioSlot   slots[kMaxAudioSlots];
};
#pragma pack(pop)

static_assert(sizeof(SharedAudioBlock) == sizeof(SharedAudioHeader) + kMaxAudioSlots * sizeof(SharedAudioSlot),
    "SharedAudioBlock layout mismatch");

// ═══════════════════════════════════════════════════════════════════════════
//  Gestor de memoria compartida de audio
// ═══════════════════════════════════════════════════════════════════════════
class SharedAudioMemory {
public:
    SharedAudioMemory() = default;
    ~SharedAudioMemory();

    // Inicializar: crea o abre el file mapping de audio
    // Retorna true si se pudo abrir/crear exitosamente
    bool initialize(const juce::String& mapName = "Local\\MixCoachAudioMemV3");

    // Cerrar y liberar recursos
    void close();

    // ─── API para Messenger (escritor) ───────────────────────────────────
    // Escribe samples mono al ring buffer del slot. Lock-free.
    void writeSamples(int slotIndex, const float* data, int numSamples) noexcept;

    // ─── API para MixCoach (lector) ─────────────────────────────────────
    // Lee samples del ring buffer de un slot. Lock-free.
    // Retorna el número de samples leídos (0 si no hay datos).
    int readSamples(int slotIndex, float* outData, int maxSamples) noexcept;

    // Lee sin consumir (peek)
    int peekSamples(int slotIndex, float* outData, int maxSamples) const noexcept;

    // Consultas
    [[nodiscard]] bool hasData(int slotIndex) const noexcept;
    [[nodiscard]] int  available(int slotIndex) const noexcept;
    [[nodiscard]] bool isInitialized() const noexcept { return block_ != nullptr; }

    // Health check
    bool healthCheck() const noexcept;
    bool reconnect();

private:
    void*               fileMapping_ = nullptr;
    void*               fileView_    = nullptr;
    SharedAudioBlock*   block_       = nullptr;
    juce::String        mapName_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SharedAudioMemory)
};

} // namespace mixcoach
