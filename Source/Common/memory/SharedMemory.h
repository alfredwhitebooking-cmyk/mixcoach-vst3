#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <juce_core/juce_core.h>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  SharedMemory — Memoria compartida entre procesos via CreateFileMapping
//  Todos los plugins MixCoach y Messenger ven los mismos datos en tiempo real.
// ═══════════════════════════════════════════════════════════════════════════

// ─── Constantes ─────────────────────────────────────────────────────────────
static constexpr int kSharedMaxSlots = 64;
static constexpr int kSharedTrackNameLen = 64;

// ─── Slot data en shared memory (POD-only, sin constructores/destructores) ──
#pragma pack(push, 8)
struct SharedSlotEntry {
    int      slotIndex = -1;
    char     trackName[kSharedTrackNameLen] = {0};
    uint32_t colourARGB = 0xFF808080;
    int      active  = 0;     // bool como int para POD
    int      bus     = -1;    // BusType como int
    // Última telemetría (cached para acceso rápido desde MixCoach)
    int64_t  telemetryTimestamp = 0;
    float    peakLeft    = -100.0f;
    float    peakRight   = -100.0f;
    float    rmsLeft     = -100.0f;
    float    rmsRight    = -100.0f;
    float    correlation = 1.0f;
    float    crestFactor = 0.0f;
    // Última muestra de audio para vectorscope
    float    sampleL     = 0.0f;
    float    sampleR     = 0.0f;
    // FFT magnitudes para espectrograma (cada ~100ms)
    float    fftMagnitudes[512]{};
    int64_t  fftTimestamp = 0;
    // LUFS (EBU R128 / ITU BS.1770) — calculado en Messenger, leído por Brain
    float    lufsIntegrated = -100.0f;
    float    lufsShortTerm  = -100.0f;
    float    lufsMomentary  = -100.0f;
    float    lufsTruePeak   = -100.0f;
    float    loudnessRange  = 0.0f;
};

// ─── Header del bloque compartido ───────────────────────────────────────────
struct SharedMemoryHeader {
    // Spinlock para acceso exclusivo (0 = libre, 1 = bloqueado)
    // Usar InterlockedExchange para acceso atómico
    volatile long writeLock = 0;
    // Contador de cambios — incrementado en cada mutación
    volatile uint64_t changeCount = 0;
    // Flag de inicialización (1 = datos listos)
    int initialized = 0;
    // HMODULE owner para detectar si el creador sigue vivo (simple checksum)
    uint32_t ownerCheck = 0;
    // Version del struct (para detectar mismatches de tamaño)
    // Incrementar cada vez que SharedSlotEntry cambie de tamaño!
    static constexpr uint32_t kCurrentStructVersion = 3;
    uint32_t structVersion = kCurrentStructVersion;
};

// ─── Bloque completo de memoria compartida ─────────────────────────────────
struct SharedMemoryBlock {
    SharedMemoryHeader header;
    SharedSlotEntry    slots[kSharedMaxSlots];
};
#pragma pack(pop)

// ─── Version del struct para detección de mismatch ───────────────────────
// IMPORTANTE: Incrementar cada vez que se agreguen/remuevan campos del
// SharedSlotEntry para evitar access violations al abrir file mappings
// viejos de sesiones anteriores con structs de diferente tamaño.
static constexpr uint32_t kSharedMemoryStructVersion = 3;

// ─── Verificación de tamaño (no debe exceder ~1MB para mapeo eficiente) ─────
static_assert(sizeof(SharedMemoryBlock) < 1024 * 1024,
    "SharedMemoryBlock demasiado grande");

// ─── Gestor de memoria compartida (singleton por proceso) ──────────────────
class SharedMemoryManager {
public:
    SharedMemoryManager();
    ~SharedMemoryManager();

    // Inicializar: crea o abre la memoria compartida existente
    // Retorna true si se pudo abrir/crear exitosamente
    bool initialize(const juce::String& mapName = "Local\\MixCoachSharedMemV2");

    // Cerrar y liberar recursos
    void close();

    // Acceso al bloque compartido (thread-safe dentro del mismo proceso)
    SharedMemoryBlock* getBlock() const noexcept { return block_; }
    bool isInitialized() const noexcept { return block_ != nullptr; }

    // ─── Operaciones atómicas sobre shared memory ─────────────────────────

    // Lock para escritura exclusiva (spinlock con InterlockedExchange)
    bool acquireLock(int timeoutMs = 100);
    void releaseLock();

    // Leer changeCount de forma segura
    uint64_t getChangeCount() const noexcept;

    // Leer un slot (thread-safe)
    bool readSlot(int index, SharedSlotEntry& out) const noexcept;

    // Escribir un slot (requiere lock adquirido)
    void writeSlot(int index, const SharedSlotEntry& entry) noexcept;

    // Registrar nuevo slot (thread-safe, adquiere lock internamente)
    int registerSlot(const SharedSlotEntry& entry) noexcept;

    // Liberar slot (thread-safe, adquiere lock internamente)
    void releaseSlot(int index) noexcept;

    // ─── Health check y reconexión ───────────────────────────────────────
    // Verifica que el mapeo de memoria sigue siendo accesible.
    // Ideal para llamar periódicamente desde un timer.
    bool healthCheck() const noexcept;

    // Reintentar conexión: cierra el mapping actual y lo reabre.
    // Retorna true si la reconexión tuvo éxito.
    bool reconnect();

private:
    void*   fileMapping_ = nullptr;
    void*   fileView_    = nullptr;
    SharedMemoryBlock* block_ = nullptr;
    juce::String mapName_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SharedMemoryManager)
};

} // namespace mixcoach
