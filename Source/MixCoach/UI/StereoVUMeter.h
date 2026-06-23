#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "SmoothValue.h"
#include "MixCoachTheme.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  StereoVUMeter — RMS + Peak VU meter
    // ═══════════════════════════════════════════════════════════════════════════
    class StereoVUMeter : public juce::Component
    {
    public:
        StereoVUMeter();
        ~StereoVUMeter() override = default;
        void paint(juce::Graphics& g) override;
        void resized() override;
        void setLevels(float leftRMS, float rightRMS, float leftPeak, float rightPeak);

    private:
        SmoothValue leftRMS_{-80.0f, 5.0f, 300.0f};
        SmoothValue rightRMS_{-80.0f, 5.0f, 300.0f};
        SmoothValue leftPeak_{-80.0f, 1.0f, 100.0f};
        SmoothValue rightPeak_{-80.0f, 1.0f, 100.0f};
        float leftPeakHold_  = -80.0f;
        float rightPeakHold_ = -80.0f;
        int leftHoldTimer_   = 0;
        int rightHoldTimer_  = 0;
        juce::Label titleLabel_;
        juce::Label leftLabel_;
        juce::Label rightLabel_;

        void drawChannelMeter(juce::Graphics& g,
                              juce::Rectangle<float> bounds,
                              float rms,
                              float peak,
                              float peakHold,
                              bool isLeftChannel);
    };

} // namespace mixcoach
