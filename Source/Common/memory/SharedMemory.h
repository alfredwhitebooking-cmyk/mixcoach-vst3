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
    static constexpr int kSharedMaxSlots     = 128;
    static constexpr int kSharedTrackNameLen = 64;

// ─── Slot data en shared memory (POD-only, sin constructores/destructores) ──
// V3 SENSOR PURO: Solo identidad. Sin telemetría.
// Todo el análisis de audio se hace en el MASTER (MixCoach via AudioAnalyzer).
// El Messenger solo transmite: "Soy la pista X, mi color es Y, mi grupo es Z".
#pragma pack(push, 8)

    struct SharedSlotEntry
    {
        int slotIndex                       = -1;
        char trackName[kSharedTrackNameLen] = {0};        // Identidad: nombre
        uint32_t colourARGB                 = 0xFF808080; // Identidad: color
        int active                          = 0;          // bool como int para POD (señal de vida)
        int bus                             = -1;         // BusType como int (grupo/ruteo)
        int trackType                       = -1;         // TrackType como int (V7: identidad explícita del Messenger)
        int muted                           = 0;          // 1 = track muteado por el usuario (V8)
        int soloed                          = 0;          // 1 = track en solo (V8)
        float faderDb                       = 0.0f;       // Fader level en dB (V9)
        float panValue                      = 0.0f;       // Pan value -1.0 (izq) a +1.0 (der) (V9)
    };

    // ─── Header del bloque compartido ───────────────────────────────────────────
    struct SharedMemoryHeader
    {
        // Spinlock para acceso exclusivo (0 = libre, 1 = bloqueado)
        // Usar InterlockedExchange para acceso atómico
        volatile long writeLock = 0;
        // Contador de cambios — incrementado en cada mutación
        volatile uint64_t changeCount = 0;
        // Flag de inicialización (1 = datos listos)
        int initialized = 0;
        // HMODULE owner para detectar si el creador sigue vivo (simple checksum)
        uint32_t ownerCheck = 0;
        // Version del struct — incrementar si SharedSlotEntry cambia de tamaño.
        // V6: solo identidad (sin telemetría). Messenger es sensor puro.
        // V7: +trackType (identidad explícita del Messenger via TrackType).
        // V8: +muted + soloed (estado de mute/solo del channel strip).
        // V9: +faderDb + panValue (nivel de fader y paneo del channel strip).
        static constexpr uint32_t kCurrentStructVersion = 9;
        uint32_t structVersion                          = kCurrentStructVersion;
    };

    // ─── Bloque completo de memoria compartida ─────────────────────────────────
    struct SharedMemoryBlock
    {
        SharedMemoryHeader header;
        SharedSlotEntry slots[kSharedMaxSlots];
    };

#pragma pack(pop)

    // ─── Version del struct para detección de mismatch ───────────────────────
    static constexpr uint32_t kSharedMemoryStructVersion = 9;

    // ─── Verificación de tamaño (no debe exceder ~1MB para mapeo eficiente) ─────
    static_assert(sizeof(SharedMemoryBlock) < 1024 * 1024, "SharedMemoryBlock demasiado grande");

    // ─── Gestor de memoria compartida (singleton por proceso) ──────────────────
    class SharedMemoryManager
    {
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
        // ═══ FIX: timeout reducido de 100ms→5ms para no bloquear el message thread ═══
        // Con 60+ Messengers escribiendo a shared memory (audio thread ~11ms),
        // el lock puede estar ocupado cuando MixCoach intenta leer. Si el timer
        // del message thread se bloquea 100ms en el spinlock, FL Studio detecta
        // un plugin colgado y lo crashea. Con 5ms, si el lock no se libera rápido,
        // la lectura se omite y los datos quedan ligeramente desactualizados
        // (mejor que crashear).
        bool acquireLock(int timeoutMs = 5);
        void releaseLock();

        // Leer changeCount de forma segura
        uint64_t getChangeCount() const noexcept;

        // Leer un slot (thread-safe, adquiere lock internamente)
        bool readSlot(int index, SharedSlotEntry& out) noexcept;

        // Escribir un slot completo (requiere lock adquirido)
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
        void* fileMapping_        = nullptr;
        void* fileView_           = nullptr;
        SharedMemoryBlock* block_ = nullptr;
        juce::String mapName_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SharedMemoryManager)
    };

} // namespace mixcoach
