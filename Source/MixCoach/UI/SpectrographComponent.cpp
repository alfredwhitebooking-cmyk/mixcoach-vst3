#include "SpectrographComponent.h"
#include <cmath>

namespace mixcoach {

namespace {

constexpr float kLabelFreqsHz[] = {
    20.0f, 30.0f, 50.0f, 70.0f, 100.0f,
    200.0f, 300.0f, 500.0f, 700.0f, 1000.0f,
    2000.0f, 3000.0f, 5000.0f, 7000.0f, 10000.0f, 20000.0f
};

juce::String formatFreqLabel(float hz)
{
    if (hz >= 1000.0f)
        return juce::String(hz / 1000.0f, hz >= 10000.0f ? 0 : 1) + "k";
    return juce::String((int) hz);
}

} // namespace

SpectrographComponent::SpectrographComponent()
{
    setOpaque(true);
    bandLevels_.reserve(kNumRtaBands);
    for (int i = 0; i < kNumRtaBands; ++i)
        bandLevels_.emplace_back(0.0f, 1.5f, 40.0f);

    rebuildBands();
}

void SpectrographComponent::setSampleRate(double sampleRate)
{
    if (sampleRate < 8000.0)
        return;

    if (std::abs(sampleRate_ - sampleRate) > 1.0)
    {
        sampleRate_ = sampleRate;
        rebuildBands();
    }
}

void SpectrographComponent::rebuildBands()
{
    bands_.resize((size_t) kNumRtaBands);

    std::vector<float> centers((size_t) kNumRtaBands);
    for (int i = 0; i < kNumRtaBands; ++i)
    {
        const float t = (kNumRtaBands <= 1) ? 0.0f
                        : (float) i / (float) (kNumRtaBands - 1);
        centers[(size_t) i] = kMinFreq * std::pow(kMaxFreq / kMinFreq, t);
    }

    for (int i = 0; i < kNumRtaBands; ++i)
    {
        auto& b = bands_[(size_t) i];
        b.centerHz = centers[(size_t) i];
        const float lowEdge  = (i == 0) ? kMinFreq
                             : std::sqrt(centers[(size_t) (i - 1)] * centers[(size_t) i]);
        const float highEdge = (i == kNumRtaBands - 1) ? kMaxFreq
                              : std::sqrt(centers[(size_t) i] * centers[(size_t) (i + 1)]);
        b.lowHz  = lowEdge;
        b.highHz = highEdge;
    }
}

void SpectrographComponent::invalidateStaticCache()
{
    staticCacheValid_ = false;
    staticCache_ = juce::Image();
    layout_.valid = false;
}

SpectrographComponent::PlotLayout SpectrographComponent::computePlotLayout() const
{
    PlotLayout L;
    if (plotArea_.isEmpty())
        return L;

    auto area = plotArea_.toFloat();
    auto dbAxis = area.removeFromLeft(22.0f);
    area.removeFromBottom(12.0f);
    area.removeFromTop(10.0f);

    L.plot = area;
    L.dbCol = dbAxis.withHeight(L.plot.getHeight()).withY(L.plot.getY());
    L.valid = ! L.plot.isEmpty();
    return L;
}

void SpectrographComponent::rebuildStaticCache()
{
    invalidateStaticCache();

    if (plotArea_.isEmpty())
        return;

    layout_ = computePlotLayout();
    if (! layout_.valid)
        return;

    const int w = plotArea_.getWidth();
    const int h = plotArea_.getHeight();
    if (w < 8 || h < 8)
        return;

    staticCache_ = juce::Image(juce::Image::ARGB, w, h, true);
    staticCache_.clear(staticCache_.getBounds());

    juce::Graphics cg(staticCache_);

    drawPlotBackground(cg, layout_.plot);
    drawGrid(cg, layout_.plot);
    drawDbAxis(cg, layout_.dbCol);
    drawFreqAxis(cg, layout_.plot);

    cg.setFont(juce::Font(juce::FontOptions(7.0f)).boldened());
    cg.setColour(MixCoachTheme::textDim().withAlpha(0.65f));
    cg.drawText("RTA",
                layout_.plot.getX() + 4.0f, layout_.plot.getY() + 2.0f, 28.0f, 10.0f,
                juce::Justification::centredLeft);

    staticCacheValid_ = true;
}

float SpectrographComponent::normToDb(float norm01) const noexcept
{
    return norm01 * 80.0f - 80.0f;
}

float SpectrographComponent::dbToDisplayNorm(float db) const noexcept
{
    return juce::jlimit(0.0f, 1.0f,
                        juce::jmap(db, kDisplayBottomDb, kDisplayTopDb, 0.0f, 1.0f));
}

float SpectrographComponent::freqToX(float freqHz, juce::Rectangle<float> plot) const noexcept
{
    freqHz = juce::jlimit(kMinFreq, kMaxFreq, freqHz);
    const float norm = std::log2(freqHz / kMinFreq) / std::log2(kMaxFreq / kMinFreq);
    return plot.getX() + norm * plot.getWidth();
}

void SpectrographComponent::aggregateBand(const float* data, int numBins,
                                          int bandIndex, float& outDb) const
{
    outDb = kDisplayBottomDb;
    if (data == nullptr || numBins <= 0 || bandIndex < 0 || bandIndex >= kNumRtaBands)
        return;

    const auto& band = bands_[(size_t) bandIndex];
    const float binWidth = (float) sampleRate_ / (float) kMessengerFftSize;

    int binLow  = (int) std::floor(band.lowHz / binWidth);
    int binHigh = (int) std::ceil(band.highHz / binWidth);
    binLow  = juce::jlimit(0, numBins - 1, binLow);
    binHigh = juce::jlimit(binLow, numBins - 1, binHigh);

    float maxNorm = 0.0f;
    for (int b = binLow; b <= binHigh; ++b)
        maxNorm = juce::jmax(maxNorm, data[b]);

    outDb = normToDb(maxNorm);
}

void SpectrographComponent::updateSpectrum(const float* data, int numBins)
{
    if (data == nullptr || numBins <= 0)
        return;

    if (bands_.empty())
        rebuildBands();

    for (int i = 0; i < kNumRtaBands; ++i)
    {
        float db = kDisplayBottomDb;
        aggregateBand(data, numBins, i, db);
        bandLevels_[(size_t) i].setTargetValue(dbToDisplayNorm(db));
    }
}

bool SpectrographComponent::smoothSpectrum(double sampleRateHz, bool allowRepaint)
{
    bool needsRepaint = false;
    for (int i = 0; i < kNumRtaBands; ++i)
        needsRepaint |= bandLevels_[(size_t) i].advance(sampleRateHz);

    if (needsRepaint && allowRepaint)
        repaint();

    return needsRepaint;
}

void SpectrographComponent::resized()
{
    auto bounds = getLocalBounds().reduced(3, 2);
    settingsButton_ = bounds.removeFromRight(22).removeFromTop(18);
    bounds.removeFromTop(2);
    plotArea_ = bounds.reduced(0, 1);
    rebuildStaticCache();
}

void SpectrographComponent::drawPlotBackground(juce::Graphics& g, juce::Rectangle<float> plot) const
{
    g.setColour(MixCoachTheme::bgCanvas());
    g.fillRoundedRectangle(plot, 3.0f);
}

void SpectrographComponent::drawGrid(juce::Graphics& g, juce::Rectangle<float> plot) const
{
    for (int db = 0; db >= (int) kDisplayBottomDb; db -= 5)
    {
        const float norm = juce::jmap((float) db, kDisplayBottomDb, kDisplayTopDb, 0.0f, 1.0f);
        const float y = plot.getBottom() - norm * plot.getHeight();
        g.setColour(MixCoachTheme::rowDivider().withAlpha(db == 0 ? 0.7f : 0.45f));
        g.drawHorizontalLine((int) y, plot.getX(), plot.getRight());
    }

    for (float freq : kLabelFreqsHz)
    {
        const float x = freqToX(freq, plot);
        g.setColour(MixCoachTheme::rowDivider().withAlpha(freq >= 1000.0f ? 0.55f : 0.35f));
        g.drawVerticalLine((int) x, plot.getY(), plot.getBottom());
    }
}

void SpectrographComponent::drawRtaBars(juce::Graphics& g, juce::Rectangle<float> plot) const
{
    if (bands_.empty())
        return;

    const auto cyanTop = MixCoachTheme::accentCyanBright();
    const auto blueBot = juce::Colour(0xFF0EA5E9);

    for (int i = 0; i < kNumRtaBands; ++i)
    {
        const auto& band = bands_[(size_t) i];
        const float x0 = freqToX(band.lowHz, plot);
        const float x1 = freqToX(band.highHz, plot);
        const float barW = juce::jmax(1.5f, (x1 - x0) * 0.92f);
        const float x = (x0 + x1) * 0.5f - barW * 0.5f;

        const float level = bandLevels_[(size_t) i].getCurrent();
        const float barH = level * plot.getHeight();
        if (barH < 0.5f)
            continue;

        auto bar = juce::Rectangle<float>(x, plot.getBottom() - barH, barW, barH);

        juce::ColourGradient grad(
            cyanTop.withAlpha(0.92f),
            juce::Point<float>(bar.getCentreX(), bar.getY()),
            blueBot.withAlpha(0.88f),
            juce::Point<float>(bar.getCentreX(), bar.getBottom()),
            false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bar, 1.5f);
    }
}

void SpectrographComponent::drawDbAxis(juce::Graphics& g, juce::Rectangle<float> labelCol) const
{
    g.setFont(juce::Font(juce::FontOptions(6.5f)));
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.8f));

    for (int db = 0; db >= (int) kDisplayBottomDb; db -= 5)
    {
        const float norm = juce::jmap((float) db, kDisplayBottomDb, kDisplayTopDb, 0.0f, 1.0f);
        const float y = labelCol.getBottom() - norm * labelCol.getHeight();
        g.drawText(juce::String(db),
                   juce::Rectangle<float>(labelCol.getX(), y - 5.0f, labelCol.getWidth(), 9.0f),
                   juce::Justification::centredRight);
    }
}

void SpectrographComponent::drawFreqAxis(juce::Graphics& g, juce::Rectangle<float> plot) const
{
    auto labelRow = juce::Rectangle<float>(plot.getX(), plot.getBottom() + 1.0f,
                                         plot.getWidth(), 11.0f);
    g.setFont(juce::Font(juce::FontOptions(6.5f)));
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.75f));

    for (float freq : kLabelFreqsHz)
    {
        const float x = freqToX(freq, plot);
        g.drawText(formatFreqLabel(freq),
                   juce::Rectangle<float>(x - 14.0f, labelRow.getY(), 28.0f, labelRow.getHeight()),
                   juce::Justification::centred);
    }
}

void SpectrographComponent::drawSettingsChrome(juce::Graphics& g) const
{
    if (settingsButton_.isEmpty())
        return;

    g.setColour(MixCoachTheme::textMuted().withAlpha(0.45f));
    g.drawEllipse(settingsButton_.toFloat().reduced(3.0f), 0.8f);
    auto c = settingsButton_.getCentre().toFloat();
    for (int i = 0; i < 6; ++i)
    {
        const float a = (float) i * juce::MathConstants<float>::twoPi / 6.0f;
        g.drawLine(c.x, c.y,
                   c.x + 5.0f * std::cos(a), c.y + 5.0f * std::sin(a), 0.8f);
    }
    g.fillEllipse(c.x - 2.0f, c.y - 2.0f, 4.0f, 4.0f);
}

void SpectrographComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::transparentBlack);

    if (plotArea_.isEmpty())
        return;

    if (! staticCacheValid_)
        rebuildStaticCache();

    if (staticCacheValid_)
        g.drawImageAt(staticCache_, plotArea_.getX(), plotArea_.getY());

    if (layout_.valid)
    {
        auto plotInComponent = layout_.plot.translated((float) plotArea_.getX(),
                                                     (float) plotArea_.getY());
        drawRtaBars(g, plotInComponent);
    }

    drawSettingsChrome(g);
}

} // namespace mixcoach
