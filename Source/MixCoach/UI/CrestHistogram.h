#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <deque>
#include "MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  CrestHistogram — Histograma dinámico de Crest Factor
// ═══════════════════════════════════════════════════════════════════════════
class CrestHistogram : public juce::Component {
public:
    CrestHistogram();
    ~CrestHistogram() override = default;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void pushCrest(float peakDb, float rmsDb);

private:
    static constexpr int kNumBins = 20;
    static constexpr int kMaxSamples = 500;
    std::array<int, kNumBins> histogram_{};
    int totalSamples_ = 0;
    std::deque<float> recentCrest_;
    juce::Label titleLabel_;
    juce::Label avgLabel_;
    juce::Label maxLabel_;
};

} // namespace mixcoach
