#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "SmoothValue.h"
#include "MixCoachTheme.h"
#include "../../Common/types/TelemetryData.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  MeterComponent — Panel compacto de medidores (25% centro-arriba)
//  Izquierda: 2 barritas L/R | Centro: 4 numéricos (Peak,RMS,LUFS,DR)
//  | Derecha: 2 barritas LUFS MDR
// ═══════════════════════════════════════════════════════════════════════════
class MeterComponent : public juce::Component {
public:
    MeterComponent();
    ~MeterComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void updateData(const TrackTelemetry& telem);

private:
    // L/R bar meters
    SmoothValue leftLevel_{ -80.0f, 3.0f, 200.0f };
    SmoothValue rightLevel_{ -80.0f, 3.0f, 200.0f };

    // Numeric values
    float peakValue_ = -80.0f;
    float rmsValue_ = -80.0f;
    float lufsValue_ = -80.0f;
    float drValue_ = 0.0f;

    // LUFS MDR bars
    SmoothValue lufsMomentary_{ -30.0f, 5.0f, 150.0f };
    SmoothValue lufsRange_{ 0.0f, 50.0f, 400.0f };

    juce::Label headerLabel_;

    void drawLevelBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                      float level, juce::Colour colour, const juce::String& label);
    void drawNumericBox(juce::Graphics& g, juce::Rectangle<float> bounds,
                        float value, const juce::String& label,
                        const juce::String& unit, juce::Colour colour);
};

} // namespace mixcoach
