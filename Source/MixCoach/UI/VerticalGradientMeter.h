#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"

namespace mixcoach {

// Utilidades de pintado para medidores verticales (Sesión 2 – Meter / Messenger)
struct VerticalGradientMeter
{
    static constexpr float kMinDb = -60.0f;
    static constexpr float kMaxDb = 0.0f;

    static float dbToNorm(float db) noexcept;
    static float normToY(float norm, juce::Rectangle<float> meterBounds) noexcept;
    static juce::Colour colourForDb(float db) noexcept;

    static void drawDbScale(juce::Graphics& g, juce::Rectangle<float> bounds,
                            bool topIsZero = true);

    static void drawGradientBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                                float levelDb, float radius = 2.5f);

    static void drawSolidBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                             float levelDb, juce::Colour colour, float radius = 2.0f);

    static void drawPeakTriangle(juce::Graphics& g, juce::Rectangle<float> scaleBounds,
                                 float peakHoldDb, juce::Colour colour);

    static void drawPeakReadout(juce::Graphics& g, juce::Rectangle<float> bounds,
                                float peakDb, juce::Colour colour);
};

// Peak hold + suavizado por canal (UI thread)
struct MeterChannelBallistics
{
    float displayDb = -100.0f;
    float peakHoldDb = -100.0f;
    int   peakHoldFrames = 0;

    void setLevelDb(float db) noexcept;
    void tickHold() noexcept;
};

} // namespace mixcoach
