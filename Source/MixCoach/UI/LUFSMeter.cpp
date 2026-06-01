#include "LUFSMeter.h"

namespace mixcoach {

LUFSMeter::LUFSMeter()
{
    titleLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x94\x8A Loudness (LUFS)"),
                        juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(titleLabel_);
}

void LUFSMeter::resized()
{
    titleLabel_.setBounds(getLocalBounds().removeFromTop(18));
}

void LUFSMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

    auto area    = getLocalBounds().reduced(4);
    area.removeFromTop(20);

    auto content = area.reduced(2, 0);
    constexpr int kNumBars = 5;
    auto barWidth = content.getWidth() / kNumBars;

    auto barBounds = content;

    drawBar(g, barBounds.removeFromLeft(barWidth).toFloat().reduced(2, 0),
            integrated_.getCurrent(), "I", "LUFS",
            MixCoachTheme::lufsIntegrated(), kTargetIntegrated);
    drawBar(g, barBounds.removeFromLeft(barWidth).toFloat().reduced(2, 0),
            shortTerm_.getCurrent(), "S", "LUFS",
            MixCoachTheme::lufsShort(), kTargetStreaming);
    drawBar(g, barBounds.removeFromLeft(barWidth).toFloat().reduced(2, 0),
            momentary_.getCurrent(), "M", "LU",
            MixCoachTheme::lufsMomentary(), kTargetBroadcast);
    drawBar(g, barBounds.removeFromLeft(barWidth).toFloat().reduced(2, 0),
            truePeak_.getCurrent(), "TP", "dBTP",
            MixCoachTheme::truePeak());
    drawBar(g, barBounds.removeFromLeft(barWidth).toFloat().reduced(2, 0),
            range_.getCurrent(), "R", "LU",
            MixCoachTheme::accent());
}

void LUFSMeter::drawBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                         float value, const juce::String& label,
                         const juce::String& unit, juce::Colour colour,
                         float targetLine)
{
    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(MixCoachTheme::textDim());
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
    auto labelArea = bounds.removeFromTop(14).reduced(2, 0);
    g.drawText(label, labelArea, juce::Justification::centred);

    float norm = juce::jlimit(0.0f, 1.0f, (value + 40.0f) / 45.0f);
    auto barFill = bounds.withTop(bounds.getBottom() - bounds.getHeight() * norm);

    if (targetLine > 0.0f) {
        float targetNorm = juce::jlimit(0.0f, 1.0f, (targetLine + 40.0f) / 45.0f);
        float targetY = bounds.getBottom() - bounds.getHeight() * targetNorm;
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        for (float x = bounds.getX(); x < bounds.getRight(); x += 6.0f) {
            g.fillRect(x, targetY - 0.5f, 3.0f, 1.0f);
        }
    }

    if (barFill.getHeight() > 1.0f) {
        juce::ColourGradient barGrad(
            colour.withAlpha(0.9f),
            juce::Point<float>(0.0f, barFill.getY()),
            colour.withAlpha(0.2f),
            juce::Point<float>(0.0f, barFill.getBottom()),
            false);
        g.setGradientFill(barGrad);
        g.fillRoundedRectangle(barFill, 3.0f);

        auto glowRect = barFill.withHeight(juce::jmax(2.0f, barFill.getHeight() * 0.2f));
        juce::ColourGradient glow(
            juce::Colours::white.withAlpha(0.2f),
            juce::Point<float>(0.0f, glowRect.getY()),
            juce::Colour(0x00000000),
            juce::Point<float>(0.0f, glowRect.getBottom()),
            false);
        g.setGradientFill(glow);
        g.fillRoundedRectangle(glowRect, 3.0f);
    }

    g.setColour(MixCoachTheme::textBright());
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
    auto valStr = (unit == "LU" || unit == "dBTP")
        ? juce::String(value, 1) + " " + unit
        : juce::String(value, 1);
    auto valArea = bounds.removeFromBottom(16).reduced(1, 0);
    g.drawText(valStr, valArea, juce::Justification::centred);

    g.setColour(MixCoachTheme::border().withAlpha(0.2f));
    float gridLines[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f };
    for (float gl : gridLines) {
        float glNorm = juce::jlimit(0.0f, 1.0f, (gl + 40.0f) / 45.0f);
        float glY = bounds.getBottom() - bounds.getHeight() * glNorm;
        g.drawHorizontalLine((int)glY, bounds.getX() + 1, bounds.getRight() - 1);
    }
}

} // namespace mixcoach
