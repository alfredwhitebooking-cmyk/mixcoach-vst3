#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Common/types/Types.h"
#include "../../Common/types/Constants.h"
#include "../../Common/memory/SlotRegistry.h"
#include "../../Common/memory/SharedData.h"
#include "MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  TrackDashboardComponent — Dashboard de métricas agregadas
//
//  Muestra:
//    - Total de tracks activos + alertas de clipping
//    - Resumen por bus (track count, peak avg, RMS avg, crest)
//    - Barra de nivel horizontal por bus
//
//  Data source: SlotRegistry + SharedData via updateDashboard()
// ═══════════════════════════════════════════════════════════════════════════
class TrackDashboardComponent : public juce::Component
{
public:
    TrackDashboardComponent();
    ~TrackDashboardComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    /** Recibe SlotRegistry + SharedData, computa agregados y actualiza UI. */
    void updateDashboard(SlotRegistry& registry, SharedData& sharedData);

private:
    // ─── Estructuras de datos agregados ──────────────────────────────────
    struct BusSummary {
        int     busIndex    = -1;
        int     trackCount  = 0;
        float   avgPeakDb   = -100.0f;
        float   avgRmsDb    = -100.0f;
        float   crestDb     = 0.0f;   // avgPeak - avgRms
    };

    struct MasterSummary {
        int totalTracks    = 0;
        int clippingTracks = 0;   // peak > -0.5 dB
        int warningTracks  = 0;   // peak > -6 dB
        int signalTracks   = 0;   // peak > -60 dB
    };

    MasterSummary master_;
    std::array<BusSummary, kNumBuses + 1> buses_;  // +1 for UNASSIGNED

    // ─── Drawing helpers (centralized in MixCoachTheme) ─────────────────
    static constexpr int kCardHeight    = MixCoachTheme::cardHeight_dashboard;
    static constexpr int kCardGap       = MixCoachTheme::cardGap;
    static constexpr int kHeaderHeight  = MixCoachTheme::headerHeight;

    void drawHeader(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawBusCard(juce::Graphics& g, juce::Rectangle<int> bounds, const BusSummary& bus);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackDashboardComponent)
};

} // namespace mixcoach
