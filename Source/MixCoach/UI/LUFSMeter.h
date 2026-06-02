#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "SmoothValue.h"
#include "MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  LUFSMeter — EBU R128 con target markers
// ═══════════════════════════════════════════════════════════════════════════
class LUFSMeter : public juce::Component {
public:
    LUFSMeter();
    ~LUFSMeter() override = default;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setIntegrated(float value)  { integrated_.setTarget(value);  repaint(); }
    void setShortTerm(float value)   { shortTerm_.setTarget(value);   repaint(); }
    void setMomentary(float value)   { momentary_.setTarget(value);   repaint(); }
    void setTruePeak(float value)    { truePeak_.setTarget(value);    repaint(); }
    void setRange(float value)       { range_.setTarget(value);       repaint(); }

private:
    // Ballistics DAW-smooth: attack rápido, release suave
    SmoothValue integrated_{ -30.0f, 5.0f,  200.0f };
    SmoothValue shortTerm_{  -30.0f, 3.0f,  200.0f };
    SmoothValue momentary_{  -30.0f, 2.0f,  150.0f };
    SmoothValue truePeak_{   -30.0f, 1.0f,  100.0f };
    SmoothValue range_{       0.0f,  10.0f, 300.0f };
    juce::Label titleLabel_;

    static constexpr float kTargetIntegrated = 23.0f;
    static constexpr float kTargetStreaming  = 14.0f;
    static constexpr float kTargetBroadcast  = 16.0f;

    void drawBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                 float value, const juce::String& label, const juce::String& unit,
                 juce::Colour colour, float targetLine = -1.0f);
};

} // namespace mixcoach
