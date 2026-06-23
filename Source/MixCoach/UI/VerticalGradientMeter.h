#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"

namespace mixcoach {

// ===========================================================================
//  VERTICAL GRADIENT METER — UTILIDADES DE RENDERIZADO DIGITAL ULTRA-SUAVE
// ===========================================================================
struct VerticalGradientMeter
{
    static constexpr float kMinDb = -60.0f;
    static constexpr float kMaxDb = 6.0f; // Calibrado a +6dB para emparejar con la escala master

    static float dbToNorm(float db) noexcept;
    static float normToY(float norm, juce::Rectangle<float> meterBounds) noexcept;
    static juce::Colour colourForDb(float db) noexcept;

    static void drawDbScale(juce::Graphics& g, juce::Rectangle<float> bounds,
                            bool topIsZero = true);

    static void drawGradientBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                                float levelDb, float radius = 1.5f);

    static void drawSolidBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                             float levelDb, juce::Colour colour, float radius = 1.5f);

    static void drawPeakTriangle(juce::Graphics& g, juce::Rectangle<float> scaleBounds,
                                 float peakHoldDb, juce::Colour colour, bool alignLeft = true);

    static void drawPeakReadout(juce::Graphics& g, juce::Rectangle<float> bounds,
                                float peakDb, juce::Colour colour);
};

// ===========================================================================
//  BALLISTICS ENGINE — RETENCIÓN LOGARÍTMICA Y CAÍDA LINEAL EN DB
// ===========================================================================
struct MeterChannelBallistics
{
    float displayDb = -60.0f;
    float peakHoldDb = -60.0f;
    int   peakHoldFrames = 0;

    void setLevelDb(float db) noexcept;
    void tickHold() noexcept;
};

} // namespace mixcoach
