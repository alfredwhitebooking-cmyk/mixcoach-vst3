#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <functional>
#include "MixCoachTheme.h"
#include "../../Common/types/Types.h"
#include "../../Common/memory/SlotRegistry.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  PlaylistComponent — Lista de Messengers conectados (25% izquierda)
//  Muestra todos los slots activos con color, nombre, indicador de señal.
//  Click para seleccionar.
// ═══════════════════════════════════════════════════════════════════════════
class PlaylistComponent : public juce::Component {
public:
    PlaylistComponent();
    ~PlaylistComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void updateList(SlotRegistry& registry);
    void setSelectedSlot(int slotIndex);
    [[nodiscard]] int getSelectedSlot() const noexcept { return selectedSlot_; }

    std::function<void(int slotIndex)> onSlotSelected;

private:
    struct TrackEntry {
        int slotIndex = -1;
        SlotInfo info;
        float peakLeft = -80.0f;
        float peakRight = -80.0f;
        bool hasSignal = false;
        bool selected = false;
    };

    static constexpr int kMaxEntries = 32;
    std::array<TrackEntry, kMaxEntries> entries_{};
    int activeCount_ = 0;
    int selectedSlot_ = -1;

    void drawEntry(juce::Graphics& g, juce::Rectangle<int> bounds,
                   const TrackEntry& entry, int index);
    void mouseDown(const juce::MouseEvent& e) override;

    juce::Label headerLabel_;
};

} // namespace mixcoach
