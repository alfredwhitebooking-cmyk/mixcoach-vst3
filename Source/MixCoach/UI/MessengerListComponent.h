#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "../../Common/types/Types.h"
#include "../../Common/types/Constants.h"
#include "../../Common/memory/SlotRegistry.h"

namespace mixcoach {

// ─── Entry de Messenger en la lista ─────────────────────────────────────────
struct MessengerEntry {
    SlotInfo    info;
    float       peakLeft   = -100.0f;
    float       peakRight  = -100.0f;
    float       rmsAvg     = -100.0f;
    bool        hasSignal  = false;
    juce::String aiSuggestion;
};

// ─── Grupo de buses ─────────────────────────────────────────────────────────
struct BusGroup {
    int count = 0;
    std::array<int, SlotRegistry::kMaxSlots> slotIndices{};
};

// ═══════════════════════════════════════════════════════════════════════════
//  MessengerListComponent — Panel derecho: lista de Messengers con AI
//  Agrupados por buses con cabeceras de sección coloreadas
// ═══════════════════════════════════════════════════════════════════════════
class MessengerListComponent : public juce::Component {
public:
    MessengerListComponent();
    ~MessengerListComponent() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void updateMessengers(SlotRegistry& registry);
    void setDisplayVersion(uint64_t version) { displayVersion_ = version; }
    [[nodiscard]] uint64_t getDisplayVersion() const { return displayVersion_; }

private:
    // No timer needed; scanning is triggered on visibility.
    // Scan is performed when the component becomes visible.
    void setRegistry (SlotRegistry* reg) noexcept;
    void visibilityChanged() override;
    void drawBusHeader(juce::Graphics& g, juce::Rectangle<int>& bounds,
                       int busIdx, int count);
    void drawMessengerRow(juce::Graphics& g, juce::Rectangle<int> bounds,
                          const MessengerEntry& entry, int index);

    // Pointer to SlotRegistry for visibility checks (not owned)
    SlotRegistry* registryPtr{ nullptr }; // not owned
    std::array<MessengerEntry, SlotRegistry::kMaxSlots> messengers_;
    std::array<BusGroup, kNumBuses + 1> busGroups_;
    int activeMessengerCount_{0};
    uint64_t displayVersion_{0};
    bool isPaused_{false}; // true when component hidden/minimized
    juce::Label titleLabel_;
    juce::Label emptyLabel_;

    static constexpr int kMaxRowsPerBus = 1000;
    static constexpr int kRowHeight = 20; // Height of each messenger row




};

} // namespace mixcoach
