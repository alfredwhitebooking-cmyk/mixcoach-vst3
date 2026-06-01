#include "CrestHistogram.h"

namespace mixcoach {

CrestHistogram::CrestHistogram()
{
    titleLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x93\x88 Crest Factor"),
                        juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(titleLabel_);

    avgLabel_.setText("Avg: --.- dB", juce::dontSendNotification);
    avgLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    avgLabel_.setJustificationType(juce::Justification::centredLeft);
    avgLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    addAndMakeVisible(avgLabel_);

    maxLabel_.setText("Max: --.- dB", juce::dontSendNotification);
    maxLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    maxLabel_.setJustificationType(juce::Justification::centredRight);
    maxLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    addAndMakeVisible(maxLabel_);

    histogram_.fill(0);
}

void CrestHistogram::resized()
{
    auto area = getLocalBounds().reduced(2);
    titleLabel_.setBounds(area.removeFromTop(16));
    auto infoRow = area.removeFromBottom(16);
    avgLabel_.setBounds(infoRow.removeFromLeft(infoRow.getWidth() / 2));
    maxLabel_.setBounds(infoRow);
}

void CrestHistogram::pushCrest(float peakDb, float rmsDb)
{
    float crest = juce::jlimit(0.0f, 30.0f, peakDb - rmsDb);
    recentCrest_.push_back(crest);

    if (recentCrest_.size() > (size_t)kMaxSamples)
        recentCrest_.pop_front();

    totalSamples_++;
    int bin = juce::jlimit(0, kNumBins - 1,
                           (int)(crest / 30.0f * (float)kNumBins));
    histogram_[bin]++;

    float sum = 0.0f;
    float maxVal = 0.0f;
    for (auto c : recentCrest_) {
        sum += c;
        if (c > maxVal) maxVal = c;
    }
    float avg = recentCrest_.empty() ? 0.0f : sum / (float)recentCrest_.size();

    avgLabel_.setText("Avg: " + juce::String(avg, 1) + " dB", juce::dontSendNotification);
    maxLabel_.setText("Max: " + juce::String(maxVal, 1) + " dB", juce::dontSendNotification);

    repaint();
}

void CrestHistogram::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(18);
    area.removeFromBottom(18);

    auto histArea = area.toFloat().reduced(2, 4);

    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(histArea, 3.0f);

    if (totalSamples_ == 0) {
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
        g.drawText("Esperando datos...", histArea.toNearestInt(), juce::Justification::centred);
        return;
    }

    int maxBinCount = 1;
    for (int count : histogram_)
        if (count > maxBinCount) maxBinCount = count;

    float barWidth = histArea.getWidth() / (float)kNumBins;
    float maxHeight = histArea.getHeight() - 4.0f;

    for (int i = 0; i < kNumBins; ++i) {
        float height = ((float)histogram_[i] / (float)maxBinCount) * maxHeight;
        if (height < 1.0f) continue;

        float x = histArea.getX() + (float)i * barWidth + 1.0f;
        float w = barWidth - 2.0f;
        float y = histArea.getBottom() - 2.0f - height;

        float t = (float)i / (float)kNumBins;
        juce::Colour barColour;
        if (t < 0.33f)
            barColour = juce::Colour::fromFloatRGBA(t * 3.0f * 0.3f, 0.5f + t, 0.8f, 0.8f);
        else if (t < 0.66f)
            barColour = juce::Colour::fromFloatRGBA((t - 0.33f) * 3.0f, 0.9f, 0.1f, 0.8f);
        else
            barColour = juce::Colour::fromFloatRGBA(1.0f, 0.8f * (1.0f - (t - 0.66f) * 3.0f), 0.0f, 0.8f);

        juce::ColourGradient barGrad(
            barColour,
            juce::Point<float>(x, y),
            barColour.withAlpha(0.2f),
            juce::Point<float>(x, y + height),
            false);
        g.setGradientFill(barGrad);
        g.fillRoundedRectangle(juce::Rectangle<float>(x, y, w, height), 1.5f);
    }

    g.setFont(juce::Font(juce::FontOptions(7.0f)));
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.4f));
    g.drawText("0", juce::Rectangle<float>(histArea.getX(), histArea.getBottom() - 12, 20, 10),
               juce::Justification::centredLeft);
    g.drawText("15", juce::Rectangle<float>(histArea.getCentreX() - 10, histArea.getBottom() - 12, 20, 10),
               juce::Justification::centred);
    g.drawText("30dB", juce::Rectangle<float>(histArea.getRight() - 28, histArea.getBottom() - 12, 28, 10),
               juce::Justification::centredRight);

    float sum = 0.0f;
    for (auto c : recentCrest_) sum += c;
    if (!recentCrest_.empty()) {
        float avgCrest = sum / (float)recentCrest_.size();
        float avgX = histArea.getX() + (avgCrest / 30.0f) * histArea.getWidth();
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.drawVerticalLine((int)avgX, histArea.getY() + 2, histArea.getBottom() - 2);
    }
}

} // namespace mixcoach
