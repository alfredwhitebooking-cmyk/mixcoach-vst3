#include "SlotRegistry.h"
#include <juce_graphics/juce_graphics.h>
#include <atomic>

namespace mixcoach {

const juce::Colour SlotRegistry::kSlotColours[8] = {
    juce::Colour(0xFFFF0000), // red
    juce::Colour(0xFF0000FF), // blue
    juce::Colour(0xFF00FF00), // green
    juce::Colour(0xFFFFA500), // orange
    juce::Colour(0xFF800080), // purple
    juce::Colour(0xFF00FFFF), // cyan
    juce::Colour(0xFFFFFF00), // yellow
    juce::Colour(0xFFFF00FF)  // magenta
};

SlotRegistry::SlotRegistry()
    : nextSlot_{0}
{
    for (auto& slot : slots_) {
        slot.slotIndex = -1;
        slot.active    = false;
    }
}

int SlotRegistry::registerSlot(const std::string& trackName, const juce::Colour& colour)
{
    for (int i = 0; i < kMaxSlots; ++i) {
        if (!slots_[i].active) {
            slots_[i].slotIndex = i;
            slots_[i].trackName = trackName;
            slots_[i].colour    = colour;
            slots_[i].active    = true;
            return i;
        }
    }
    return -1; // No hay espacio
}

void SlotRegistry::releaseSlot(int slotIndex)
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots) {
        slots_[slotIndex].active    = false;
        slots_[slotIndex].slotIndex = -1;
        slots_[slotIndex].trackName.clear();
        slots_[slotIndex].colour = juce::Colours::grey;
        audioBuffers_[slotIndex].reset();
    }
}

void SlotRegistry::setActive(int slotIndex, bool active)
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots)
        slots_[slotIndex].active = active;
}

int SlotRegistry::activeCount() const noexcept
{
    int count = 0;
    for (const auto& slot : slots_) {
        if (slot.active) ++count;
    }
    return count;
}

SlotInfo SlotRegistry::getSlotInfo(int slotIndex) const
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots)
        return slots_[slotIndex];
    return SlotInfo{};
}

juce::Colour SlotRegistry::getSlotColour(int slotIndex) const
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots)
        return slots_[slotIndex].colour;
    return juce::Colours::grey;
}

void SlotRegistry::updateSlotName(int slotIndex, const std::string& name)
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots)
        slots_[slotIndex].trackName = name;
}

void SlotRegistry::updateSlotColour(int slotIndex, const juce::Colour& colour)
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots)
        slots_[slotIndex].colour = colour;
}

void SlotRegistry::forEachActive(std::function<void(const SlotInfo&)> callback) const
{
    for (const auto& slot : slots_) {
        if (slot.active) callback(slot);
    }
}

TelemetryBuffer& SlotRegistry::getTelemetry(int slotIndex)
{
    return telemetry_[slotIndex];
}

const TelemetryBuffer& SlotRegistry::getTelemetry(int slotIndex) const
{
    return telemetry_[slotIndex];
}

AudioRingBuffer& SlotRegistry::getAudioBuffer(int slotIndex)
{
    return audioBuffers_[slotIndex];
}

const AudioRingBuffer& SlotRegistry::getAudioBuffer(int slotIndex) const
{
    return audioBuffers_[slotIndex];
}

juce::String SlotRegistry::defaultTrackName()
{
    static std::atomic<int> counter{0};
    return "Pista " + juce::String(++counter);
}

} // namespace mixcoach
