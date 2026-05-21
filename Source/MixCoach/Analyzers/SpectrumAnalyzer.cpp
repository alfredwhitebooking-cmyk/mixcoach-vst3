#include "SpectrumAnalyzer.h"

namespace mixcoach {

SpectrumAnalyzer::SpectrumAnalyzer()
{
    bins_.resize(256, 0.0f);
}

void SpectrumAnalyzer::updateSpectrum(const float* data, int numBins)
{
    bins_.assign(data, data + std::min(numBins, (int)bins_.size()));
    repaint();
}

void SpectrumAnalyzer::setReferenceCurve(const float* data, int numBins)
{
    referenceBins_.assign(data, data + std::min(numBins, 256));
    repaint();
}

void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xFF1A1A2E));
    g.setColour(juce::Colour(0xFF16213E));
    g.drawRect(bounds, 1);

    if (bins_.empty()) return;

    auto w = bounds.getWidth();
    auto h = bounds.getHeight();
    float binWidth = w / (float)bins_.size();

    // Dibujar espectro
    juce::Path path;
    path.startNewSubPath(0.0f, h);

    for (size_t i = 0; i < bins_.size(); ++i) {
        float x = (float)i * binWidth;
        float normalized = juce::jlimit(0.0f, 1.0f, bins_[i] * 2.0f);
        float y = h - (normalized * h);
        path.lineTo(x, y);
    }

    path.lineTo(w, h);
    path.closeSubPath();

    g.setColour(juce::Colour(0x883498DB));
    g.fillPath(path);
    g.setColour(juce::Colour(0xFF3498DB));
    g.strokePath(path, juce::PathStrokeType(1.5f));

    // Líneas de referencia si existen
    if (!referenceBins_.empty()) {
        juce::Path refPath;
        refPath.startNewSubPath(0.0f, h);

        for (size_t i = 0; i < referenceBins_.size() && i < bins_.size(); ++i) {
            float x = (float)i * binWidth;
            float normalized = juce::jlimit(0.0f, 1.0f, referenceBins_[i] * 2.0f);
            float y = h - (normalized * h);
            refPath.lineTo(x, y);
        }

        g.setColour(juce::Colour(0x66FFFFFF));
        g.strokePath(refPath, juce::PathStrokeType(1.0f));
    }
}

} // namespace mixcoach
