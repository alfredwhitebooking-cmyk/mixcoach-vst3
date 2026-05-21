#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Common/Types.h"
#include "../../Common/SlotRegistry.h"
#include "MixCoachTheme.h"

namespace mixcoach {

// ─── Dashboard de todas las pistas ──────────────────────────────────────────
class TrackDashboardComponent : public juce::Component
{
public:
    TrackDashboardComponent();
    ~TrackDashboardComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void updateTrackData(const SlotInfo& info, const TrackTelemetry& telemetry);

private:
    void drawTrackRow(juce::Graphics& g, juce::Rectangle<int> bounds,
                      const SlotInfo& info, const TrackTelemetry& telemetry);

    juce::Label titleLabel_;
    juce::Label infoLabel_;

    std::array<SlotInfo, SlotRegistry::kMaxSlots> trackInfo_{};
    std::array<TrackTelemetry, SlotRegistry::kMaxSlots> trackTelemetry_{};
};

} // namespace mixcoach
