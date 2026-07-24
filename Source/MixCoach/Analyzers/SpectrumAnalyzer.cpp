#include "SpectrumAnalyzer.h"
#include <cmath>

namespace mixcoach {

SpectrumAnalyzer::SpectrumAnalyzer()
{
    bins_.fill(0.0f);
    referenceBins_.fill(0.0f);

    titleLabel_.setFont(juce::Font(juce::FontOptions(9.0f)));
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel_);
}

void SpectrumAnalyzer::updateSpectrum(const float* data, int numBins)
{
    int copyCount = juce::jmin(numBins, kNumBins);
    for (int i = 0; i < copyCount; ++i)
        bins_[i] = juce::jlimit(0.0f, 1.0f, data[i]);
    repaint();
}

void SpectrumAnalyzer::setReferenceCurve(const float* data, int numBins)
{
    int copyCount = juce::jmin(numBins, kNumBins);
    for (int i = 0; i < copyCount; ++i)
        referenceBins_[i] = juce::jlimit(0.0f, 1.0f, data[i]);
    hasReference_ = true;
    repaint();
}

void SpectrumAnalyzer::clearReferenceCurve()
{
    referenceBins_.fill(0.0f);
    hasReference_ = false;
    repaint();
}

void SpectrumAnalyzer::setHighlightFreq(float freqHz, float gainDb)
{
    highlightFreq_ = freqHz;
    highlightDb_ = gainDb;
    repaint();
}

void SpectrumAnalyzer::setTitle(const juce::String& title)
{
    title_ = title;
    titleLabel_.setText(title_, juce::dontSendNotification);
}

void SpectrumAnalyzer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float cr = 4.0f;

    // ─── Fondo oscuro ──────────────────────────────────────────────────
    g.setColour(MixCoachTheme::bgPanel());
    g.fillRoundedRectangle(bounds, cr);
    g.setColour(MixCoachTheme::bgDarker().withAlpha(0.3f));
    g.drawRoundedRectangle(bounds, cr, 0.5f);

    // ─── Área del gráfico ───────────────────────────────────────────────
    auto graphArea = bounds.reduced(6, 4);
    graphArea.removeFromTop(14.0f); // título
    graphArea.removeFromBottom(16.0f); // labels de frecuencia

    drawGrid(g, graphArea);
    if (hasReference_)
        drawReference(g, graphArea);
    drawSpectrum(g, graphArea);
    drawHighlight(g, graphArea);
}

void SpectrumAnalyzer::drawGrid(juce::Graphics& g, juce::Rectangle<float> area)
{
    // ─── Líneas horizontales (niveles) ──────────────────────────────────
    g.setFont(juce::Font(juce::FontOptions(7.5f)));
    for (int i = 0; i <= 4; ++i) {
        float y = area.getY() + area.getHeight() * (float)i / 4.0f;
        g.setColour(juce::Colours::white.withAlpha(0.04f));
        g.drawHorizontalLine((int)y, area.getX(), area.getRight());

        // Label dB
        g.setColour(MixCoachTheme::textDim().withAlpha(0.4f));
        g.drawText(juce::String(-i * 12) + "dB",
                   area.getX() - 2.0f, y - 6.0f, 30.0f, 12.0f,
                   juce::Justification::centredLeft);
    }

    // ─── Líneas verticales (frecuencias) ────────────────────────────────
    const float kMinFreq = 20.0f;
    const float kMaxFreq = 20000.0f;
    const float freqLabels[] = { 100.0f, 1000.0f, 10000.0f };

    g.setFont(juce::Font(juce::FontOptions(7.5f)));
    for (float freq : freqLabels) {
        float normalized = std::log(freq / kMinFreq) / std::log(kMaxFreq / kMinFreq);
        float x = area.getX() + normalized * area.getWidth();

        g.setColour(juce::Colours::white.withAlpha(0.04f));
        g.drawVerticalLine((int)x, area.getY(), area.getBottom());

        g.setColour(MixCoachTheme::textDim().withAlpha(0.5f));
        juce::String label;
        if (freq >= 1000.0f)
            label = juce::String((int)(freq / 1000.0f)) + "k";
        else
            label = juce::String((int)freq);
        g.drawText(label, x - 12.0f, area.getBottom() + 2.0f, 24.0f, 12.0f,
                   juce::Justification::centred);
    }
}

void SpectrumAnalyzer::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> area)
{
    float w = area.getWidth();
    float h = area.getHeight();
    float binW = w / (float)kNumBins;

    // ─── Dibujar barras ─────────────────────────────────────────────────
    for (int i = 0; i < kNumBins; ++i) {
        float x = area.getX() + (float)i * binW;
        float val = bins_[i];
        if (val < 0.01f) continue;

        float barH = h * val;
        float y = area.getBottom() - barH;

        // Color: cyan con opacidad según altura
        float alpha = juce::jmap(val, 0.0f, 1.0f, 0.15f, 0.85f);
        g.setColour(MixCoachTheme::accentCyan().withAlpha(alpha));
        g.fillRect(x, y, binW * 0.8f, barH);
    }

    // ─── Línea de contorno (path) ───────────────────────────────────────
    juce::Path path;
    path.startNewSubPath(area.getX(), area.getBottom());
    for (int i = 0; i < kNumBins; ++i) {
        float x = area.getX() + (float)i * binW + binW * 0.5f;
        float val = bins_[i];
        float y = area.getBottom() - h * val;
        path.lineTo(x, y);
    }
    path.lineTo(area.getRight(), area.getBottom());
    path.closeSubPath();

    g.setColour(MixCoachTheme::accentCyan().withAlpha(0.06f));
    g.fillPath(path);

    // Línea del contorno
    juce::Path contour;
    contour.startNewSubPath(area.getX(), area.getBottom());
    for (int i = 0; i < kNumBins; ++i) {
        float x = area.getX() + (float)i * binW + binW * 0.5f;
        float val = bins_[i];
        float y = area.getBottom() - h * val;
        contour.lineTo(x, y);
    }
    g.setColour(MixCoachTheme::accentCyan().withAlpha(0.5f));
    g.strokePath(contour, juce::PathStrokeType(1.2f));
}

void SpectrumAnalyzer::drawReference(juce::Graphics& g, juce::Rectangle<float> area)
{
    if (!hasReference_) return;

    float w = area.getWidth();
    float h = area.getHeight();
    float binW = w / (float)kNumBins;

    juce::Path refPath;
    bool first = true;

    for (int i = 0; i < kNumBins; ++i) {
        float x = area.getX() + (float)i * binW + binW * 0.5f;
        float val = referenceBins_[i];
        if (val < 0.01f) continue;

        float y = area.getBottom() - h * val;
        if (first) {
            refPath.startNewSubPath(x, y);
            first = false;
        } else {
            refPath.lineTo(x, y);
        }
    }

    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.strokePath(refPath, juce::PathStrokeType(1.0f));
}

void SpectrumAnalyzer::drawHighlight(juce::Graphics& g, juce::Rectangle<float> area)
{
    if (highlightFreq_ <= 0.0f) return;

    const float kMinFreq = 20.0f;
    const float kMaxFreq = 20000.0f;
    float normalized = std::log(highlightFreq_ / kMinFreq) / std::log(kMaxFreq / kMinFreq);
    float x = area.getX() + normalized * area.getWidth();

    // ─── Línea vertical de resalte ──────────────────────────────────────
    g.setColour(MixCoachTheme::accent().withAlpha(0.5f));
    g.drawVerticalLine((int)x, area.getY(), area.getBottom());

    // ─── Círculo en el punto de intersección ────────────────────────────
    int binIdx = (int)(normalized * kNumBins);
    float val = bins_[juce::jlimit(0, kNumBins - 1, binIdx)];
    float markerY = area.getBottom() - area.getHeight() * val;

    g.setColour(MixCoachTheme::accent().withAlpha(0.7f));
    g.fillEllipse(x - 3.0f, markerY - 3.0f, 6.0f, 6.0f);

    // Label del highlight
    g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
    g.setColour(MixCoachTheme::accent());
    juce::String hlLabel = juce::String((int)highlightFreq_) + "Hz";
    if (highlightDb_ != 0.0f)
        hlLabel += " " + juce::String(highlightDb_, 1) + "dB";
    g.drawText(hlLabel, x + 6.0f, markerY - 8.0f, 80.0f, 14.0f, juce::Justification::centredLeft);
}

void SpectrumAnalyzer::resized()
{
    titleLabel_.setBounds(getLocalBounds().removeFromTop(14).reduced(4, 0));
}

} // namespace mixcoach
