#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace mixcoach {

// ─── Spectrum Analyzer visual ───────────────────────────────────────────────
class SpectrumAnalyzer : public juce::Component
{
public:
    SpectrumAnalyzer();
    ~SpectrumAnalyzer() override = default;

    void paint(juce::Graphics& g) override;
    void updateSpectrum(const float* data, int numBins);
    void setReferenceCurve(const float* data, int numBins);

private:
    std::vector<float> bins_;
    std::vector<float> referenceBins_;
    juce::Path spectrumPath_;
};

} // namespace mixcoach
