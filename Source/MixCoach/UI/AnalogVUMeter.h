#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <cmath>
#include "SmoothValue.h"
#include "MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  AnalogVUMeter — VU Meter con aguja analógica fluida
//  Aguja con suavizado juce::SmoothedValue, escala -20 a +3 dB
// ═══════════════════════════════════════════════════════════════════════════
class AnalogVUMeter : public juce::Component {
public:
    AnalogVUMeter();
    ~AnalogVUMeter() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setLevel(float levelDb);
    void setLabel(const juce::String& label) { labelText_ = label; repaint(); }
    void setMeterColour(juce::Colour c) { meterColour_ = c; repaint(); }

private:
    SmoothValue smoothedLevel_{ -80.0f, 5.0f, 200.0f };
    juce::String labelText_;
    juce::Colour meterColour_{ 0xFF00B4D8 };

    void drawScale(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawNeedle(juce::Graphics& g, juce::Rectangle<float> bounds);
};

} // namespace mixcoach
