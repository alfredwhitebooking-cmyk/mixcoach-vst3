#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Common/types/Constants.h"
#include "MixCoachTheme.h"

namespace mixcoach {

// ─── Buses Virtuales — Rediseño visual ──────────────────────────────────────
class VirtualBusesComponent : public juce::Component
{
public:
    VirtualBusesComponent();
    ~VirtualBusesComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    juce::Label titleLabel_;
    juce::Label infoLabel_;

    struct BusInfo {
        juce::String name;
        juce::Colour colour;
        float level = -60.0f;
        int  trackCount = 0;
    };

    std::vector<BusInfo> buses_;
};

} // namespace mixcoach
