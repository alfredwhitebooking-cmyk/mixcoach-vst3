#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <cmath>
#include <array>
#include "MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  VectorscopeComponent — Vectorscopio circular
// ═══════════════════════════════════════════════════════════════════════════
class VectorscopeComponent : public juce::Component {
public:
    VectorscopeComponent();
    ~VectorscopeComponent() override = default;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void pushSample(float left, float right);

private:
    static constexpr int kTraceLen = 256;
    static constexpr int kPhosphorDecay = 8;

    struct Point {
        float x = 0.0f, y = 0.0f;
        float alpha = 0.0f;
    };

    std::array<Point, kTraceLen> trace_{};
    int writePos_ = 0;
    juce::Label titleLabel_;
    juce::Label corrLabel_;

    void drawGrid(juce::Graphics& g, juce::Rectangle<float> area);
};

} // namespace mixcoach
