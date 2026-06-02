#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "SmoothValue.h"
#include "VerticalGradientMeter.h"
#include "../../Common/types/TelemetryData.h"

namespace mixcoach {

// SESIÓN 2 – METER (Tab Analyzers): VU L/R | PEAK/RMS/LUFS/DR | LUFS MDR L/R
class MeterComponent : public juce::Component
{
public:
    MeterComponent();
    ~MeterComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void updateData(const TrackTelemetry& telem);

    /** Timer 60 Hz: avanza suavizado y repinta solo si hubo cambio visible. */
    bool advanceVisuals(double sampleRateHz = 60.0, bool allowRepaint = true);

private:
    SmoothValue leftBar_{ -80.0f, 1.5f, 35.0f };
    SmoothValue rightBar_{ -80.0f, 1.5f, 35.0f };
    SmoothValue lufsLeftBar_{ -30.0f, 3.0f, 45.0f };
    SmoothValue lufsRightBar_{ -30.0f, 3.0f, 45.0f };

    SmoothValue peakSmooth_{ -80.0f, 3.0f, 40.0f };
    SmoothValue rmsSmooth_{ -80.0f, 4.0f, 50.0f };
    SmoothValue lufsSmooth_{ -30.0f, 6.0f, 60.0f };
    SmoothValue drSmooth_{ 0.0f, 12.0f, 80.0f };

    MeterChannelBallistics leftPeak_;
    MeterChannelBallistics rightPeak_;
    MeterChannelBallistics lufsLeftHold_;
    MeterChannelBallistics lufsRightHold_;

    float lufsReadoutLeft_  = -100.0f;
    float lufsReadoutRight_ = -100.0f;

    void paintStereoColumn(juce::Graphics& g, juce::Rectangle<int> area);
    void paintNumericColumn(juce::Graphics& g, juce::Rectangle<int> area);
    void paintLufsMdrColumn(juce::Graphics& g, juce::Rectangle<int> area);

    void drawMetricRow(juce::Graphics& g, juce::Rectangle<float> row,
                       const juce::String& label, float valueDb,
                       const juce::String& suffix, bool isLufs = false);
};

} // namespace mixcoach
