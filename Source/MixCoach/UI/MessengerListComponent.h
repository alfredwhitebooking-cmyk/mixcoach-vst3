#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include "MixCoachTheme.h"
#include "../../Common/types/Types.h"
#include "../../Common/types/Constants.h"
#include "../../Common/memory/SlotRegistry.h"

namespace mixcoach {

// ─── Entry de Messenger en la lista ─────────────────────────────────────────
struct MessengerEntry {
    SlotInfo    info;
    float       peakLeft      = -100.0f;
    float       peakRight     = -100.0f;
    float       rmsAvg        = -100.0f;
    float       peakHold      = -100.0f;
    uint32_t    peakHoldTimeMs = 0;
    bool        hasSignal     = false;
    juce::String aiSuggestion;

    // ─── Nivel animado con decaimiento fijo (render loop independiente) ─
    //  El decaimiento es CONSTANTE (kDecayDbPerFrame dB/frame) hacia
    //  -infinito, NO depende del target. Ataca instantáneamente cuando
    //  rawPeak > barLevel. Así las barras SIEMPRE se mueven aunque los
    //  datos de telemetría no cambien entre frames.
    float       barLevel      = -80.0f;  // nivel animado (decae fijo hacia -inf)
    float       rmsSmooth     = -80.0f;  // RMS animado

    // ─── Peak hold rendering ─────────────────────────────────────────
    float peakHoldAlpha = 0.0f;     // alpha para el triángulo (fade out)

    // ─── Fade-in animation ────────────────────────────────────────────
    float fadeAlpha = 1.0f;         // 0.0 -> 1.0 durante fade-in
    uint32_t fadeStartMs = 0;       // timestamp de creación (ms)
    static constexpr float kFadeDurationMs = 350.0f; // duración del fade
};

// ─── Grupo de buses ─────────────────────────────────────────────────────────
struct BusGroup {
    int count = 0;
    std::array<int, SlotRegistry::kMaxSlots> slotIndices{};
};

// Layout constants — card-based track list
static constexpr int kCardHeight    = 34;
static constexpr int kCardGap       = 3;
static constexpr int kHeaderHeight  = 22;
static constexpr int kTitleHeight   = 22;
static constexpr int kCardCorner    = 6;

// ═══════════════════════════════════════════════════════════════════════════
//  MessengerListComponent — Track list with VU meters and AI suggestions
//  ═══ PERSISTENCIA ESTÁTICA: Los datos sobreviven recreaciones del editor ═══
// ═══════════════════════════════════════════════════════════════════════════
class MessengerListComponent : public juce::Component,
                              private juce::Timer {
public:
    MessengerListComponent();
    ~MessengerListComponent() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void updateMessengers(SlotRegistry& registry);
    /** Solo telemetría + cache (60 Hz); no repinta ni reordena la lista. */
    void refreshTelemetryFromRegistry(SlotRegistry& registry);
    void smoothMeters();
    void restoreFromPersistent();

    [[nodiscard]] int getPreferredHeight() const;

    void setSelectedSlot(int slotIndex);
    [[nodiscard]] int getSelectedSlot() const noexcept { return selectedSlot_; }
    std::function<void(int slotIndex)> onSlotSelected;

    // ─── Grouping mode ─────────────────────────────────────────────────────
    enum class GroupingMode { Type, Colour, Bus };
    void setGroupingMode(GroupingMode mode);
    [[nodiscard]] GroupingMode getGroupingMode() const noexcept { return groupingMode_; }

    // ─── Group collapse/expand ────────────────────────────────────────────
    void collapseAll();
    void expandAll();
    [[nodiscard]] bool isGroupCollapsed(int busIdx) const noexcept { return collapsedGroups_[busIdx]; }

private:
    void timerCallback() override;
    void visibilityChanged() override;
    void drawBusHeader(juce::Graphics& g, juce::Rectangle<int>& bounds,
                       int busIdx, int count);
    void drawTrackCard(juce::Graphics& g, juce::Rectangle<int> bounds,
                       const MessengerEntry& entry, int index);

    void syncTelemetryFromRegistry(SlotRegistry& registry, bool& anyDataOut);
    /** Reconstruye busGroups_ desde messengers_ actuales. Se llama depsués de syncTelemetryFromRegistry() para reflejar cambios de bus en tiempo real. */
    void rebuildBusGroups();

    void mouseDown(const juce::MouseEvent& e) override;

    // ─── Datos locales ────────────────────────────────────────────────────
    std::array<MessengerEntry, SlotRegistry::kMaxSlots> messengers_;
    std::array<BusGroup, kNumBuses + 1> busGroups_;
    std::array<bool, kNumBuses + 1> collapsedGroups_{};
    GroupingMode groupingMode_ = GroupingMode::Bus;
    int activeMessengerCount_{0};
    int selectedSlot_ = -1;
    bool isPaused_{false};

    // ─── Cache de telemetría para smoothMeters() ──────────────────────────
    struct TelemetrySnapshot {
        float peakLeft  = -100.0f;
        float peakRight = -100.0f;
        float rmsAvg    = -100.0f;
        bool  hasSignal = false;
        bool  active    = false;
    };
    std::array<TelemetrySnapshot, SlotRegistry::kMaxSlots> telemetryCache_{};
    juce::Label emptyLabel_;

    // ─── Datos PERSISTENTES (sobreviven recreación del editor) ────────────
    static std::array<MessengerEntry, SlotRegistry::kMaxSlots> s_persistentData_;
    static std::array<BusGroup, kNumBuses + 1> s_persistentGroups_;
    static int  s_persistentCount_;
    static bool s_persistentReady_;

    // ─── Constantes de decaimiento fijo (independiente del target) ────
    // Este decay se aplica SIEMPRE en smoothMeters(), incluso si los datos
    // de telemetría no han cambiado. Cada frame la barra decae hacia
    // -infinito. Cuando llegan datos nuevos (de TelemetryBuffer o bg
    // thread), el attack instantáneo la sube de nuevo.
    // smoothMeters() se llama desde editor timer (60fps) + timer interno (120fps)
    // Frecuencia combinada ≈ 180 fps. 30 dB/sec / 180 = 0.167 dB/frame.
    static constexpr float kDecayDbPerFrame = 0.167f;  // ~30 dB/sec combinado

    static constexpr int kMaxRowsPerBus = 1000;
};

} // namespace mixcoach
