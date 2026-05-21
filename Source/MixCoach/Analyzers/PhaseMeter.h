#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace mixcoach {

// ─── Phase / Correlation Meter ──────────────────────────────────────────────
class PhaseMeter : public juce::Component
{
public:
    PhaseMeter();
    ~PhaseMeter() override = default;

    void paint(juce::Graphics& g) override;
    void updateCorrelation(float correlation);

private:
    float correlation_ = 1.0f;
    juce::Label valueLabel_;
};

} // namespace mixcoach
