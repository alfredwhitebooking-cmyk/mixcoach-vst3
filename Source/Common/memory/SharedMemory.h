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
        // Nota: SharedMemory.cpp se modificó (healthCheck refactor a helper SEH),
        // pero SharedSlotEntry no cambió, por lo que kCurrentStructVersion sigue en 9.
        // V10: +slotHeartbeats (array de timestamps lock-free para audio thread)
        // Los Messengers escriben su heartbeat aquí desde processBlock() sin
        // adquirir el spinlock. MixCoach lee estos timestamps para detectar
        // slots vivos sin bloquear el audio thread.
        static constexpr uint32_t kCurrentStructVersion = 10;
        uint32_t structVersion                          = kCurrentStructVersion;
    };

    // ─── Bloque completo de memoria compartida ─────────────────────────────────
    // ═══ V10: slotHeartbeats — Array de timestamps lock-free ═══════════════
    // Cada Messenger escribe su heartbeat aquí desde processBlock() (~2ms
    // a 48kHz) usando InterlockedExchange64 — SIN adquirir el spinlock.
    // MixCoach lee estos timestamps para detectar slots activos.
    //
    // Los timestamps son milisegundos desde el startup del Messenger
    // (juce::Time::getMillisecondCounter()). Si un slot no actualiza su
    // heartbeat por > 5 segundos, se marca como stale.
    //
    // Esta separación elimina el spinlock del audio thread de Messenger,
    // que es la violación más crítica de tiempo real en la arquitectura.
    struct SharedMemoryBlock
    {
        SharedMemoryHeader header;
        // Lock-free heartbeats — escrito por Messenger audio thread,
        // leído por MixCoach bg service. Sin locks, sin I/O.
        // Usar InterlockedExchange64/CompareExchange64 para acceso.
        volatile int64_t slotHeartbeats[kSharedMaxSlots];
        SharedSlotEntry slots[kSharedMaxSlots];
    };

#pragma pack(pop)

    // ─── Version del struct para detección de mismatch ───────────────────────
    static constexpr uint32_t kSharedMemoryStructVersion = 10;

    // ─── Verificación de tamaño (no debe exceder ~1MB para mapeo eficiente) ─────
    static_assert(sizeof(SharedMemoryBlock) < 1024 * 1024, "SharedMemoryBlock demasiado grande");

    // ═══════════════════════════════════════════════════════════════════════════
    //  SessionDiscovery — Negociación de GUID de sesión entre Messenger y MixCoach
    // ═══════════════════════════════════════════════════════════════════════════
    // MixCoach crea una shared memory con nombre fijo que contiene el GUID de la
    // sesión actual. Los Messengers leen esta memoria para conectarse a la sesión
    // correcta, permitiendo que múltiples proyectos DAW convivan sin compartir slots.
    //
    // Formato del GUID: "MixCoach_<32-char-hex>" (ej: "MixCoach_A1B2C3D4E5F67890ABCDEF1234567890")
    //
    // Nombres de shared memory resultantes:
    //   Slots:  Local\MixCoach_<GUID>_Slots
    //   Audio:  Local\MixCoach_<GUID>_Audio

#pragma pack(push, 8)

    struct SessionDiscoveryBlock
    {
        char sessionGUID[48] = {};      // "MixCoach_<32-hex>" + null
        uint32_t timestampMs  = 0;       // Último heartbeat de MixCoach
        uint32_t active       = 0;       // 1 si MixCoach está vivo
        uint32_t protocolVer  = 1;       // Versión del protocolo de discovery
    };

#pragma pack(pop)

    // ─── Nombre fijo de la shared memory de discovery ────────────────────────
    static constexpr const char* kSessionDiscoveryName = "Local\\MixCoachSessionV3";

    // ─── Logger de sesión para shared memory fixa ────────────────────────────
    class SessionDiscovery
    {
    public:
        SessionDiscovery();
        ~SessionDiscovery();

        /** Crea o abre la shared memory de discovery. */
        bool initialize();

        /** Escribe un GUID de sesión (MixCoach). Marca active=true. */
        void publishGUID(const juce::String& guid);

        /** Lee el GUID de sesión actual (Messenger). Retorna vacío si no hay sesión. */
        juce::String readGUID() const;

        /** Marca active=false y limpia (MixCoach shutdown). */
        void closeSession();

        /** Libera el file mapping de descubrimiento. */
        void closeDiscovery();

        bool isInitialized() const noexcept { return block_ != nullptr; }

    private:
        void* fileMapping_  = nullptr;
        void* fileView_     = nullptr;
        SessionDiscoveryBlock* block_ = nullptr;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SessionDiscovery)
    };

    /** Genera un GUID único para la sesión: "MixCoach_<timestamp>_<random16>" */
    juce::String generateSessionGUID();

    /** Construye el nombre de shared memory para slots a partir de un GUID. */
    juce::String makeSlotShmName(const juce::String& guid);

    /** Construye el nombre de shared memory para audio a partir de un GUID. */
    juce::String makeAudioShmName(const juce::String& guid);

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

        // ═══ V10: Heartbeat lock-free — SIN spinlock ═════════════════════
        // Escribe un timestamp heartbeat para un slot específico.
        // Llamado desde el audio thread de Messenger (processBlock).
        // NO adquiere ningún lock — usa InterlockedExchange64 directo.
        // @param slotIndex  Índice del slot (0-127)
        // @param timestampMs  Valor del heartbeat (juce::Time::getMillisecondCounter())
        void writeHeartbeat(int slotIndex, int64_t timestampMs) noexcept;

        /** Lee el heartbeat timestamp de un slot.
            NO adquiere ningún lock — usa InterlockedCompareExchange64 directo.
            @param slotIndex  Índice del slot (0-127)
            @return Timestamp en ms, o 0 si el slot no es válido. */
        int64_t readHeartbeat(int slotIndex) const noexcept;

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
