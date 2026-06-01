#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <cmath>
#include <vector>
#include "MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  SpectrographComponent — Espectrograma estilo IK Multimedia
//  Rango 20 Hz – 20 kHz, mapeo logarítmico, barras dinámicas con colores
//  profesionales, suavizado fluido por banda.
// ═══════════════════════════════════════════════════════════════════════════
class SpectrographComponent : public juce::Component {
public:
    SpectrographComponent();
    ~SpectrographComponent() override = default;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void updateSpectrum(const float* data, int numBins);

private:
    static constexpr int kNumDisplayBins = 256;
    static constexpr int kMaxFFTBins = 512;
    static constexpr float kMinFreq = 20.0f;
    static constexpr float kMaxFreq = 20000.0f;

    std::vector<float> displayBins_;      // smoothed display values
    std::vector<float> rawBins_;          // raw incoming values
    std::vector<float> peakHoldBins_;     // peak hold per bin
    std::vector<int>   peakHoldTimers_;   // decay timers per bin

    juce::Label titleLabel_;

    // Log-spaced frequency mapping table
    std::vector<float> binFreqs_;
    std::vector<float> fftWeights_;

    juce::Colour getBinColour(float magnitude) const noexcept;
    void buildFrequencyMap(int fftNumBins);
    void drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawSpectrumBars(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawPeakHold(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawFrequencyLabels(juce::Graphics& g, juce::Rectangle<float> bounds);
};

} // namespace mixcoach
