#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <cmath>
#include <array>
#include "MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  VectorscopeComponent — Vectorscopio circular profesional
//  Grid concéntrico, trazado anti-aliased con phosphor trail,
//  crosshairs con labels, indicador de correlación
// ═══════════════════════════════════════════════════════════════════════════
class VectorscopeComponent : public juce::Component {
public:
    VectorscopeComponent();
    ~VectorscopeComponent() override = default;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void pushSample(float left, float right);
    void setDisplayCorrelation(float correlation);

    /** Decaimiento phosphor + repaint si hay traza visible. */
    bool advanceFrame(double sampleRateHz = 60.0, bool allowRepaint = true);

private:
    static constexpr int kTraceLen = 512;

    struct Point {
        float x = 0.0f, y = 0.0f;
        float alpha = 0.0f;
    };

    std::array<Point, kTraceLen> trace_{};
    int writePos_ = 0;
    juce::Label titleLabel_;
    juce::Label corrLabel_;

    void drawGrid(juce::Graphics& g, juce::Rectangle<float> area);
    void drawTrace(juce::Graphics& g, juce::Rectangle<float> circleArea);

    void rebuildGridCache();
    [[nodiscard]] juce::Rectangle<float> plotCircleArea() const;

    juce::Image gridCache_;
    bool gridCacheValid_ = false;
    float lastCorrDisplayed_ = 2.0f;
};

} // namespace mixcoach
