#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include "AnalogVUMeter.h"
#include "MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  VUMetersPanel — Contenedor con header "VU Meters"
//  Agrupa 4 AnalogVUMeter en grid 2×2
// ═══════════════════════════════════════════════════════════════════════════
class VUMetersPanel : public juce::Component {
public:
    VUMetersPanel();
    ~VUMetersPanel() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    std::array<AnalogVUMeter, 4>& getMeters() noexcept { return vuMeters_; }
    AnalogVUMeter& getMeter(int i) noexcept { return vuMeters_[i]; }

    void setLevel(int idx, float db) { if (idx >= 0 && idx < 4) vuMeters_[idx].setLevel(db); }

private:
    juce::Label headerLabel_;
    std::array<AnalogVUMeter, 4> vuMeters_;
};

} // namespace mixcoach
