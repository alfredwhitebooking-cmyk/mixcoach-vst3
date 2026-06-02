#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  StereoMeter — VU Meter estéreo compacto con peak+scale compartidos
//
//  Layout interno:
//    peakZone(5px) | gap | L bar(10px) | gap(4px) | R bar(10px) | gap | scaleZone(13px)
//
//  Las barras L y R comparten:
//    - Peak zone (triángulos L y R lado a lado)
//    - Fondo unificado detrás de ambas barras
//    - Grid lines continuas
//    - Escala numérica a la derecha
// ═══════════════════════════════════════════════════════════════════════════
class StereoMeter : public juce::Component
{
public:
    StereoMeter();

    void setLevels(float leftDb, float rightDb);
    void setBarColours(juce::Colour leftCol, juce::Colour rightCol);

    void paint(juce::Graphics& g) override;

private:
    float levelToY(float levelDb, juce::Rectangle<float> bounds) const;
    void drawBar(juce::Graphics& g, juce::Rectangle<float> barBounds, float level);

    float leftLevel_    = -80.0f;
    float rightLevel_   = -80.0f;
    float leftPeak_     = -80.0f;
    float rightPeak_    = -80.0f;
    int   leftHoldTimer_  = 0;
    int   rightHoldTimer_ = 0;

    juce::Colour barColourL_{0xFF60A5FA};  // Azul para L
    juce::Colour barColourR_{0xFF34D399};  // Verde para R

    // ─── Clip indicator ─────────────────────────────────────────────────
    bool clipActive_ = false;
    int  clipHoldTimer_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoMeter)
};

} // namespace mixcoach
