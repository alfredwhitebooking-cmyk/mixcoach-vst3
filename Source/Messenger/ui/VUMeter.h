#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  VUMeter — Medidor VU estilo profesional con peak triangle ◀
//  Escala: +6 a -60 dB | Gradiente: verde → rojo (bottom → top)
//  Peak triangle a la izquierda, escala a la derecha
// ═══════════════════════════════════════════════════════════════════════════
class VUMeter : public juce::Component
{
public:
    VUMeter();

    void setLevel(float levelDb);
    void setBarColour(juce::Colour col);

    void paint(juce::Graphics& g) override;

private:
    float levelToY(float levelDb, juce::Rectangle<float> bounds) const;
    juce::Colour getLevelColour(float levelDb, float alpha) const;

    float currentLevel_ = -80.0f;
    float peakLevel_ = -80.0f;
    int peakHoldTimer_ = 0;
    float lastDrawnLevel_ = -80.0f;
    float lastDrawnPeak_ = -80.0f;
    juce::Colour barColour_{0xFF3498DB};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VUMeter)
};

} // namespace mixcoach
