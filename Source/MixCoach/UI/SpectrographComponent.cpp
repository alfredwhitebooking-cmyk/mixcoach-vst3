#include "SpectrographComponent.h"

namespace mixcoach {

SpectrographComponent::SpectrographComponent()
{
    displayBins_.resize(kNumDisplayBins, 0.0f);
    rawBins_.resize(kNumDisplayBins, 0.0f);
    peakHoldBins_.resize(kNumDisplayBins, 0.0f);
    peakHoldTimers_.resize(kNumDisplayBins, 0);

    titleLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x93\xA1 Spectrum Analyzer"),
                        juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(titleLabel_);
}

void SpectrographComponent::buildFrequencyMap(int fftNumBins)
{
    if (fftNumBins <= 0) return;
    binFreqs_.resize(kNumDisplayBins);
    fftWeights_.resize(kNumDisplayBins);

    constexpr float sampleRate = 44100.0f;
    constexpr int fftSize = 1024;
    constexpr float binWidth = sampleRate / static_cast<float>(fftSize);

    for (int i = 0; i < kNumDisplayBins; ++i) {
        float freq = kMinFreq * std::pow(kMaxFreq / kMinFreq,
                                          static_cast<float>(i) / static_cast<float>(kNumDisplayBins - 1));
        binFreqs_[i] = freq;

        float fftPos = freq / binWidth;
        if (fftPos >= static_cast<float>(fftNumBins - 1))
            fftWeights_[i] = static_cast<float>(fftNumBins - 1);
        else if (fftPos < 0.0f)
            fftWeights_[i] = 0.0f;
        else
            fftWeights_[i] = fftPos;
    }
}

void SpectrographComponent::updateSpectrum(const float* data, int numBins)
{
    if (data == nullptr || numBins <= 0) return;

    int n = std::min(numBins, kMaxFFTBins);

    if (binFreqs_.empty()) {
        buildFrequencyMap(n);
    }

    for (int i = 0; i < kNumDisplayBins; ++i) {
        float fftPos = (i < static_cast<int>(fftWeights_.size())) ? fftWeights_[i] : 0.0f;

        int idxLow = juce::jlimit(0, n - 1, static_cast<int>(fftPos));
        int idxHigh = juce::jlimit(0, n - 1, idxLow + 1);
        float frac = fftPos - static_cast<float>(idxLow);

        float valLow = data[idxLow];
        float valHigh = data[idxHigh];
        float interpolated = valLow + (valHigh - valLow) * frac;

        float rawDb = 20.0f * std::log10(interpolated + 1e-8f);
        float rawNorm = juce::jmap(juce::jlimit(-80.0f, 0.0f, rawDb), -80.0f, 0.0f, 0.0f, 1.0f);

        rawBins_[i] = rawNorm;

        if (rawNorm > displayBins_[i])
            displayBins_[i] += (rawNorm - displayBins_[i]) * 0.55f;
        else
            displayBins_[i] += (rawNorm - displayBins_[i]) * 0.12f;

        if (rawNorm > peakHoldBins_[i]) {
            peakHoldBins_[i] = rawNorm;
            peakHoldTimers_[i] = 15;
        } else if (peakHoldTimers_[i] > 0) {
            peakHoldTimers_[i]--;
        } else {
            peakHoldBins_[i] += (0.0f - peakHoldBins_[i]) * 0.03f;
        }
    }
    repaint();
}

juce::Colour SpectrographComponent::getBinColour(float magnitude) const noexcept
{
    if (magnitude < 0.10f) {
        float t = magnitude / 0.10f;
        return juce::Colour::fromFloatRGBA(t * 0.2f, t * 0.3f, 0.3f + t * 0.5f, 1.0f);
    } else if (magnitude < 0.25f) {
        float t = (magnitude - 0.10f) / 0.15f;
        return juce::Colour::fromFloatRGBA(0.2f + t * 0.1f, 0.3f + t * 0.5f, 0.8f - t * 0.4f, 1.0f);
    } else if (magnitude < 0.40f) {
        float t = (magnitude - 0.25f) / 0.15f;
        return juce::Colour::fromFloatRGBA(0.3f + t * 0.2f, 0.8f + t * 0.2f, 0.4f - t * 0.3f, 1.0f);
    } else if (magnitude < 0.55f) {
        float t = (magnitude - 0.40f) / 0.15f;
        return juce::Colour::fromFloatRGBA(0.5f + t * 0.5f, 1.0f - t * 0.1f, 0.1f - t * 0.1f, 1.0f);
    } else if (magnitude < 0.70f) {
        float t = (magnitude - 0.55f) / 0.15f;
        return juce::Colour::fromFloatRGBA(1.0f, 0.9f - t * 0.3f, 0.0f, 1.0f);
    } else if (magnitude < 0.85f) {
        float t = (magnitude - 0.70f) / 0.15f;
        return juce::Colour::fromFloatRGBA(1.0f, 0.6f - t * 0.4f, 0.0f, 1.0f);
    } else {
        float t = (magnitude - 0.85f) / 0.15f;
        return juce::Colour::fromFloatRGBA(1.0f, 0.2f - t * 0.2f, 0.0f, 1.0f);
    }
}

void SpectrographComponent::resized()
{
    auto area = getLocalBounds().reduced(2);
    titleLabel_.setBounds(area.removeFromTop(16));
}

void SpectrographComponent::drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    auto w = bounds.getWidth();
    auto h = bounds.getHeight();

    g.setColour(MixCoachTheme::border().withAlpha(0.15f));
    float dbLevels[] = { -60.0f, -48.0f, -36.0f, -24.0f, -12.0f, -6.0f, 0.0f };
    for (float db : dbLevels) {
        float dbNorm = juce::jlimit(0.0f, 1.0f, (db + 80.0f) / 80.0f);
        float y = bounds.getBottom() - dbNorm * h;
        g.setColour(MixCoachTheme::border().withAlpha((db == 0.0f) ? 0.3f : 0.12f));
        g.drawHorizontalLine((int)y, bounds.getX() + 1, bounds.getRight() - 1);

        if (db == 0.0f || db == -12.0f || db == -24.0f || db == -48.0f) {
            g.setFont(juce::Font(juce::FontOptions(7.0f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.4f));
            g.drawText(juce::String((int)db),
                       juce::Rectangle<float>(bounds.getRight() - 22, y - 5, 20, 10),
                       juce::Justification::centredRight);
        }
    }

    struct FreqMarker { float freq; const char* label; };
    FreqMarker markers[] = {
        { 20.0f, "20" }, { 31.0f, "31" }, { 63.0f, "63" },
        { 100.0f, "100" }, { 125.0f, "125" },
        { 250.0f, "250" }, { 500.0f, "500" },
        { 1000.0f, "1k" }, { 2000.0f, "2k" },
        { 4000.0f, "4k" }, { 8000.0f, "8k" },
        { 10000.0f, "10k" }, { 16000.0f, "16k" }, { 20000.0f, "20k" }
    };

    for (auto& m : markers) {
        float xNorm = std::log2(m.freq / kMinFreq) / std::log2(kMaxFreq / kMinFreq);
        float x = bounds.getX() + xNorm * w;

        g.setColour(MixCoachTheme::border().withAlpha(
            (m.freq == 1000.0f) ? 0.35f : 0.15f));
        g.drawVerticalLine((int)x, bounds.getY() + 1, bounds.getBottom() - 1);
    }
}

void SpectrographComponent::drawFrequencyLabels(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    auto w = bounds.getWidth();

    struct FreqLabel { float freq; const char* text; };
    FreqLabel labels[] = {
        { 31.0f, "31" }, { 63.0f, "63" },
        { 125.0f, "125" }, { 250.0f, "250" },
        { 500.0f, "500" }, { 1000.0f, "1k" },
        { 2000.0f, "2k" }, { 4000.0f, "4k" },
        { 8000.0f, "8k" }, { 16000.0f, "16k" }
    };

    g.setFont(juce::Font(juce::FontOptions(7.0f)));
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));

    for (auto& lb : labels) {
        float xNorm = std::log2(lb.freq / kMinFreq) / std::log2(kMaxFreq / kMinFreq);
        float x = bounds.getX() + xNorm * w;
        g.drawText(juce::String(lb.text),
                   juce::Rectangle<float>(x - 12, bounds.getBottom() - 12, 24, 10),
                   juce::Justification::centred);
    }
}

void SpectrographComponent::drawSpectrumBars(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (displayBins_.empty() || binFreqs_.empty()) return;

    auto w = bounds.getWidth();
    auto h = bounds.getHeight();

    juce::Path fillPath;
    bool first = true;
    fillPath.preallocateSpace(kNumDisplayBins * 4);

    for (int i = 0; i < kNumDisplayBins; ++i) {
        float freq = binFreqs_[i];
        float xNorm = std::log2(freq / kMinFreq) / std::log2(kMaxFreq / kMinFreq);
        float x = bounds.getX() + xNorm * w;
        float normalized = juce::jlimit(0.0f, 1.0f, displayBins_[i]);
        float y = bounds.getBottom() - normalized * h;

        if (first) {
            fillPath.startNewSubPath(x, bounds.getBottom());
            fillPath.lineTo(x, y);
            first = false;
        } else {
            fillPath.lineTo(x, y);
        }
    }

    float lastX = bounds.getX() + w;
    fillPath.lineTo(lastX, bounds.getBottom());
    fillPath.closeSubPath();

    juce::ColourGradient spectrumGrad(
        juce::Colour(0xFF00B4D8).withAlpha(0.25f),
        juce::Point<float>(0.0f, bounds.getY()),
        juce::Colour(0xFFFF2D55).withAlpha(0.08f),
        juce::Point<float>(0.0f, bounds.getBottom()),
        false);
    spectrumGrad.addColour(0.5f, juce::Colour(0xFF00E676).withAlpha(0.12f));
    g.setGradientFill(spectrumGrad);
    g.fillPath(fillPath);

    for (int i = 1; i < kNumDisplayBins; ++i) {
        float freq1 = binFreqs_[i - 1];
        float freq2 = binFreqs_[i];
        float xNorm1 = std::log2(freq1 / kMinFreq) / std::log2(kMaxFreq / kMinFreq);
        float xNorm2 = std::log2(freq2 / kMinFreq) / std::log2(kMaxFreq / kMinFreq);
        float x1 = bounds.getX() + xNorm1 * w;
        float x2 = bounds.getX() + xNorm2 * w;
        float y1 = bounds.getBottom() - juce::jlimit(0.0f, 1.0f, displayBins_[i - 1]) * h;
        float y2 = bounds.getBottom() - juce::jlimit(0.0f, 1.0f, displayBins_[i]) * h;

        float avgMag = (displayBins_[i - 1] + displayBins_[i]) * 0.5f;

        g.setColour(getBinColour(avgMag).withAlpha(0.2f));
        g.drawLine(x1, y1, x2, y2, 4.0f);

        g.setColour(getBinColour(avgMag));
        g.drawLine(x1, y1, x2, y2, 1.5f);
    }
}

void SpectrographComponent::drawPeakHold(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (binFreqs_.empty()) return;

    auto w = bounds.getWidth();
    auto h = bounds.getHeight();

    g.setFont(juce::Font(juce::FontOptions(6.0f)));

    for (int i = 0; i < kNumDisplayBins; i += 4) {
        if (peakHoldBins_[i] < 0.01f) continue;

        float freq = binFreqs_[i];
        float xNorm = std::log2(freq / kMinFreq) / std::log2(kMaxFreq / kMinFreq);
        float x = bounds.getX() + xNorm * w;
        float y = bounds.getBottom() - juce::jlimit(0.0f, 1.0f, peakHoldBins_[i]) * h;

        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.fillRect(x - 0.5f, y - 1.0f, 2.0f, 3.0f);
    }
}

void SpectrographComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

    juce::ColourGradient topShadow(
        juce::Colours::black.withAlpha(0.15f),
        juce::Point<float>(0.0f, bounds.getY()),
        juce::Colour(0x00000000),
        juce::Point<float>(0.0f, bounds.getY() + 30.0f),
        false);
    g.setGradientFill(topShadow);
    g.fillRoundedRectangle(bounds, 6.0f);

    auto content = getLocalBounds().reduced(6).toFloat();
    content.removeFromTop(16);

    drawGrid(g, content);
    drawFrequencyLabels(g, content);
    drawSpectrumBars(g, content);
    drawPeakHold(g, content);
}

} // namespace mixcoach
