#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <deque>
#include "MixCoachTheme.h"
#include "SmoothValue.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  CrestHistogram — Gauge semicircular de Crest Factor (estilo referencia)
//  Arco coloreado verde→amarillo→rojo con aguja oscura
// ═══════════════════════════════════════════════════════════════════════════
class CrestHistogram : public juce::Component {
public:
    CrestHistogram();
    ~CrestHistogram() override = default;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void pushCrest(float peakDb, float rmsDb);
    bool advanceFrame(double sampleRateHz = 60.0, bool allowRepaint = true);

private:
    static constexpr int kMaxSamples = 500;
    std::deque<float> recentCrest_;
    int totalSamples_ = 0;

    SmoothValue crestSmooth_{ 0.0f, 8.0f, 60.0f };
    float currentPeakDb_ = -80.0f;
    float currentRmsDb_  = -80.0f;

    // ─── Geometría del arco semicircular (JUCE screen coords) ──────
    // El arco va desde ~210° (izquierda, 0 dB) hasta ~330° (derecha, 30 dB)
    // pasando por 270° (arriba, 15 dB) — arco de ~120° en la parte superior
    static constexpr float kArcStartAngle = juce::MathConstants<float>::pi * 1.1667f;  // ~210°
    static constexpr float kArcEndAngle   = juce::MathConstants<float>::pi * 1.8333f;  // ~330°
    static constexpr float kArcRange      = kArcEndAngle - kArcStartAngle;  // ~120°

    static constexpr float kCrestMin = 0.0f;
    static constexpr float kCrestMax = 30.0f;

    [[nodiscard]] float valueToAngle(float crest) const;
    void drawArc(juce::Graphics& g, juce::Rectangle<float> gaugeBounds);
    void drawScale(juce::Graphics& g, juce::Rectangle<float> gaugeBounds);
    void drawNeedle(juce::Graphics& g, juce::Rectangle<float> gaugeBounds, float angle);
    void drawSubValues(juce::Graphics& g, juce::Rectangle<float> gaugeBounds);
};

} // namespace mixcoach
