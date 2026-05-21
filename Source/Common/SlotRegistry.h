#pragma once
#include <cstdint>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <string>
#include <functional>
#include "Types.h"
#include "TelemetryData.h"

namespace mixcoach {

// ─── Registro de slots (pistas) ─────────────────────────────────────────────
class SlotRegistry {
public:
    static constexpr int kMaxSlots = 64;

    SlotRegistry();

    // Registrar / liberar slots
    int  registerSlot(const std::string& trackName, const juce::Colour& colour);
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

    // Acceso a telemetría
    [[nodiscard]] TelemetryBuffer& getTelemetry(int slotIndex);
    [[nodiscard]] const TelemetryBuffer& getTelemetry(int slotIndex) const;

    // Acceso a buffers de audio compartidos (Messenger escribe, MixCoach lee)
    [[nodiscard]] AudioRingBuffer& getAudioBuffer(int slotIndex);
    [[nodiscard]] const AudioRingBuffer& getAudioBuffer(int slotIndex) const;

    // Obtener el nombre de pista por defecto
    [[nodiscard]] static juce::String defaultTrackName();

private:
    std::array<SlotInfo, kMaxSlots>        slots_{};
    std::array<TelemetryBuffer, kMaxSlots> telemetry_{};
    std::array<AudioRingBuffer, kMaxSlots> audioBuffers_{};
    int                                    nextSlot_{0};
    static const juce::Colour          kSlotColours[8];
};

} // namespace mixcoach
