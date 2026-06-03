#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <functional>
#include "MixCoachTheme.h"
#include "SmoothValue.h"
#include "../../Common/types/Types.h"
#include "../../Common/types/Constants.h"
#include "../../Common/memory/SlotRegistry.h"

namespace mixcoach {

// Layout constants para el playlist agrupado por buses
static constexpr int kPlBusHeaderH  = 18;
static constexpr int kPlCardHeight  = 26;
static constexpr int kPlCardGap     = 2;
static constexpr int kPlLEDStripH   = 5;
static constexpr int kPlLabelH      = 18;

// ═══════════════════════════════════════════════════════════════════════════
//  PlaylistComponent — Lista de Messengers agrupada por buses
//  Muestra todos los slots activos organizados por bus virtual,
//  con cabeceras de bus + track cards compactas.
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
    [[nodiscard]] int getPreferredHeight() const;
    [[nodiscard]] int getActiveCount() const noexcept { return activeCount_; }

    std::function<void(int slotIndex)> onSlotSelected;
    /** Callback para el botón "+ GRUPO". Si no se setea, no hace nada. */
    std::function<void()> onAddGroupRequested;

    /** Asigna el Viewport padre para auto-scroll al seleccionar un track. */
    void setViewport(juce::Viewport* vp) noexcept { playlistViewport_ = vp; }

private:
    struct TrackEntry {
        int slotIndex = -1;
        SlotInfo info;
        float peakLeft = -80.0f;
        float peakRight = -80.0f;
        bool hasSignal = false;
        bool selected = false;
    };

    struct PlBusGroup {
        int  count = 0;
        std::array<int, SlotRegistry::kMaxSlots> slotIndices{};
    };

    static constexpr int kMaxEntries = 128;
    std::array<TrackEntry, kMaxEntries> entries_{};
    std::array<PlBusGroup, kNumBuses + 1> busGroups_{};
    int activeCount_ = 0;
    int selectedSlot_ = -1;

    void drawBusHeader(juce::Graphics& g, const juce::Rectangle<int>& area,
                        int busIdx, int count);
    void drawTrackCard(juce::Graphics& g, juce::Rectangle<int> bounds,
                        const TrackEntry& entry);
    void mouseDown(const juce::MouseEvent& e) override;

    // ─── Sub-header interactive bounds ───────────────────────────────
    juce::Rectangle<int> subHeaderGrupoBounds_;

    juce::Label headerLabel_;

    // ─── Viewport padre (para auto-scroll) ────────────────────────────
    juce::Viewport* playlistViewport_ = nullptr;

    /** Calcula la coordenada Y de un slot en la lista. Retorna -1 si no se encuentra. */
    [[nodiscard]] int getSlotY(int slotIndex) const noexcept;

    // ─── LED strip animado ───────────────────────────────────────────
    static constexpr int kLEDCount = 24;
    float ledLevel_{0.0f};
    SmoothValue ledSmooth_{ 0.0f, 30.0f, 400.0f };
    void animateLEDStrip(juce::Graphics& g, juce::Rectangle<int> bounds);
};

} // namespace mixcoach
