#pragma once
#include <cstdint>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <string>
#include <functional>
#include "Types.h"
#include "TelemetryData.h"
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
    static constexpr int kMaxSlots = 64;

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

    // Contador de cambios (shared memory si está disponible, sino local)
    [[nodiscard]] uint64_t getChangeCount() const noexcept;

    // ─── Observer callbacks ───────────────────────────────────────────────
    std::function<void(int slotIndex)> onSlotChanged{nullptr};
    std::function<void(int slotIndex)> onSlotRegistered{nullptr};
    std::function<void(int slotIndex)> onSlotReleased{nullptr};

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

    static const juce::Colour          kSlotColours[8];

    // Helpers
    void readFromShared(int slotIndex);
};

} // namespace mixcoach
