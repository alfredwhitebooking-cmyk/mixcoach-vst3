#pragma once
#include <cstdint>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <string>
#include <functional>
#include "../types/Types.h"
#include "../types/TelemetryData.h"
#include "SharedMemory.h"

namespace mixcoach {

// ─── Registro de slots (pistas) ─────────────────────────────────────────────
// Soporta dos modos:
//   1. Local (standalone/sin shared memory) — datos en memoria local
//   2. Shared (IPC entre plugins) — lecture/escritura en SharedMemoryManager
// En modo shared, cada escritura va a local + shared; la sincronización
// desde shared → local se hace via syncFromShared().
class SlotRegistry {
public:
    static constexpr int kMaxSlots = 128;

    SlotRegistry();

    // Registrar / liberar slots
    int  registerSlot(const std::string& trackName, const juce::Colour& colour, BusType bus = BusType::None);
    void releaseSlot(int slotIndex);
    void setActive(int slotIndex, bool active);

    // Consultas
    [[nodiscard]] int          activeCount() const noexcept;
    [[nodiscard]] int          totalSlots()  const noexcept { return kMaxSlots; }
    [[nodiscard]] SlotInfo     getSlotInfo(int slotIndex) const;
    [[nodiscard]] juce::Colour getSlotColour(int slotIndex) const;

    // Iterar slots activos
    void forEachActive(std::function<void(const SlotInfo&)> callback) const;

    // Actualizar propiedades del slot (desde Messenger UI)
    void updateSlotName(int slotIndex, const std::string& name);
    void updateSlotColour(int slotIndex, const juce::Colour& colour);
    void updateSlotBus(int slotIndex, BusType bus);

    // Acceso a telemetría (local, historia por proceso)
    [[nodiscard]] TelemetryBuffer& getTelemetry(int slotIndex);
    [[nodiscard]] const TelemetryBuffer& getTelemetry(int slotIndex) const;

    // Acceso a buffers de audio (local, por proceso)
    [[nodiscard]] AudioRingBuffer& getAudioBuffer(int slotIndex);
    [[nodiscard]] const AudioRingBuffer& getAudioBuffer(int slotIndex) const;

    // Escribir telemetría más reciente al shared memory slot entry
    // (llamado desde Messenger::processBlock para que MixCoach vea los niveles)
    void updateSharedTelemetry(int slotIndex,
                               float peakLeft, float peakRight,
                               float rmsLeft, float rmsRight,
                               float correlation, float crestFactor,
                               float sampleL, float sampleR,
                               const float* fftMagnitudes = nullptr,
                               float lufsIntegrated = -100.0f,
                               float lufsShortTerm  = -100.0f,
                               float lufsMomentary  = -100.0f,
                               float lufsTruePeak   = -100.0f,
                               float loudnessRange  = 0.0f);

    // Obtener el nombre de pista por defecto
    [[nodiscard]] static juce::String defaultTrackName();

    // ─── Shared Memory (IPC) ──────────────────────────────────────────────
    void setSharedMemory(SharedMemoryManager* shm) noexcept { shm_ = shm; }

    // Sincronizar desde memoria compartida → caché local
    // Retorna true si hubo cambios
    bool syncFromShared();

    /** Lee peaks/RMS/FFT/LUFS desde SHM → TelemetryBuffer (sin changeCount). UI 60 Hz. */
    void pollTelemetryFromShared();

    /** Fallback cuando SHM no está disponible: lee telemetría V2 del backup file. */
    void pollTelemetryFromBackups();

    // Forzar sincronización completa (ignora changeCount).
    // Usar cuando MixCoach se conecta por primera vez a una shared memory
    // que ya tiene Messengers registrados. Retorna número de slots encontrados.
    int forceFullSync();

    // Contador de cambios (shared memory si está disponible, sino local)
    [[nodiscard]] uint64_t getChangeCount() const noexcept;

    // True si ya se hizo al menos una sincronización exitosa desde shared memory
    [[nodiscard]] bool hasEverSynced() const noexcept { return everSynced_; }

    // ─── Stale data detection ────────────────────────────────────────────
    /** Timeout: si un slot no recibe telemetría por >3s, se marca stale. */
    static constexpr int64_t kStaleTimeoutUs = 3 * 1000 * 1000;  // 3 segundos

    /**
     * Escanea todos los slots activos y marca como stale aquellos cuyo
     * último timestamp de telemetría supere kStaleTimeoutUs.
     * Si un slot stale recibe datos nuevos, se limpia la flag.
     * Llamar periódicamente desde el background worker (~1s).
     */
    void checkStaleSlots();

    // ─── Observer callbacks ───────────────────────────────────────────────
    std::function<void(int slotIndex)> onSlotChanged{nullptr};
    std::function<void(int slotIndex)> onSlotRegistered{nullptr};
    std::function<void(int slotIndex)> onSlotReleased{nullptr};

    // ─── File-based backup (COMUNICACIÓN PRINCIPAL entre plugins) ──
    // Como cada DLL (MixCoach.vst3 / Messenger.vst3) tiene su propio singleton
    // SharedData, la comunicación via CreateFileMapping puede ser frágil
    // entre procesos separados de FL Studio. Los backup files son el mecanismo
    // de comunicación GARANTIZADO entre plugins.
    //
    // Cada Messenger escribe un backup file cuando:
    //   - Se registra un slot (registra: nombre, color, bus)
    //   - Se actualiza la telemetría (actualiza: peaks, RMS, LUFS)
    //
    // MixCoach lee TODOS los backup files periódicamente para detectar
    // Messengers independientemente del estado de shared memory.
    //
    // FORMATO:
    //   V1 (legacy): magic + version + slotIndex + active + bus + colourARGB + trackName[64]
    //   V2: V1 + peakLeft + peakRight + rmsLeft + rmsRight +
    //       correlation + crestFactor + sampleL + sampleR +
    //       lufsIntegrated + lufsShortTerm + lufsMomentary + lufsTruePeak + loudnessRange
    //   V3 (actual): V2 + fftData[512] (magnitudes FFT, 2048 bytes)
    static void saveSlotToBackupFile(int slotIndex, const SlotInfo& info);
    static void removeSlotBackupFile(int slotIndex);
    // Lee todos los slots desde archivos de backup.
    // Retorna cantidad encontrada.
    // forceOverwrite=true: sobrescribe datos locales aunque el slot ya esté activo.
    int loadSlotsFromBackupFiles(bool forceOverwrite = true);
    // Actualiza SOLO la telemetría en un backup file existente (rápido, no toca nombre/color/bus)
    // Llamado desde el audio thread del Messenger
    // fftData: opcional (nullptr = saltar), array de kNumSpectrumBins floats
    static void updateSlotBackupTelemetry(int slotIndex,
                                           float peakLeft, float peakRight,
                                           float rmsLeft, float rmsRight,
                                           float correlation, float crestFactor,
                                           float sampleL, float sampleR,
                                           float lufsIntegrated, float lufsShortTerm,
                                           float lufsMomentary, float lufsTruePeak,
                                           float loudnessRange,
                                           const float* fftData = nullptr);

private:
    std::array<SlotInfo, kMaxSlots>        slots_{};
    std::array<TelemetryBuffer, kMaxSlots> telemetry_{};
    std::array<AudioRingBuffer, kMaxSlots> audioBuffers_{};
    int                                    nextSlot_{0};
    uint64_t                               localChangeCount_{0};

    // Shared memory (opcional, nullptr = modo local-only)
    SharedMemoryManager* shm_ = nullptr;

    // Último changeCount conocido de shared memory
    uint64_t lastSharedChangeCount_{0};

    // Flag: si ya se hizo al menos una sincronización exitosa
    bool everSynced_{false};

    // Cache de timestamps del meta file (slot_N.meta) para evitar I/O
    // innecesario. El meta file SOLO se actualiza cuando cambian metadatos
    // (updateSlotName/Colour/Bus → saveSlotToBackupFile), NUNCA por
    // telemetría. Así pollTelemetryFromBackups() puede checkear si hubo
    // cambios de metadatos sin abrir el backup file (que se actualiza
    // constantemente por telemetría cada ~32ms).
    std::array<int64_t, kMaxSlots> lastMetaModTimeMs_{};

    static const juce::Colour          kSlotColours[8];

    // Helpers
    void readFromShared(int slotIndex);
    // Sincronización completa desde shared memory (ignora changeCount)
    // Usado por forceFullSync() antes del fallback a archivos
    int forceFullSyncFromShm();
};

} // namespace mixcoach
