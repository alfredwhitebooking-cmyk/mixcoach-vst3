#include "MeterComponent.h"

namespace mixcoach {

MeterComponent::MeterComponent()
{
    headerLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x93\x8A Meter"),
                         juce::dontSendNotification);
    headerLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    headerLabel_.setJustificationType(juce::Justification::centredLeft);
    headerLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(headerLabel_);
}

void MeterComponent::updateData(const TrackTelemetry& telem)
{
    leftLevel_.setTarget(telem.peakLeft);
    rightLevel_.setTarget(telem.peakRight);
    peakValue_ = telem.peakLeft;
    rmsValue_ = (telem.rmsLeft + telem.rmsRight) * 0.5f;
    lufsValue_ = (telem.lufsIntegrated > -99.0f) ? telem.lufsIntegrated : peakValue_ - 14.0f;
    drValue_ = telem.loudnessRange;
    lufsMomentary_.setTarget(telem.lufsMomentary);
    lufsRange_.setTarget(telem.loudnessRange);
    repaint();
}

void MeterComponent::resized()
{
    auto area = getLocalBounds().reduced(4, 2);
    headerLabel_.setBounds(area.removeFromTop(16));
}

void MeterComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

    auto area = getLocalBounds().reduced(4, 2);
    area.removeFromTop(18);

    auto totalWidth = area.getWidth();
    constexpr float kLeftPct  = 0.25f;
    constexpr float kRightPct = 0.25f;

    auto leftThird = area.removeFromLeft((int)(totalWidth * kLeftPct));
    auto rightThird = area.removeFromRight((int)(totalWidth * kRightPct));
    auto centerThird = area;

    auto leftContent = leftThird.reduced(2, 0).toFloat();
    float barW = leftContent.getWidth() * 0.45f;
    float barH = leftContent.getHeight() - 20.0f;

    auto lBar = juce::Rectangle<float>(leftContent.getX() + 3, leftContent.getY() + 18,
                                        barW, barH);
    drawLevelBar(g, lBar, leftLevel_.getCurrent(), MixCoachTheme::channelLeft(), "L");

    auto rBar = juce::Rectangle<float>(leftContent.getRight() - 3 - barW, leftContent.getY() + 18,
                                        barW, barH);
    drawLevelBar(g, rBar, rightLevel_.getCurrent(), MixCoachTheme::channelRight(), "R");

    auto centerContent = centerThird.reduced(2, 0);
    auto numBoxes = centerContent.reduced(4, 0);
    int boxH = numBoxes.getHeight() / 4;

    auto peakBox = numBoxes.removeFromTop(boxH);
    auto rmsBox = numBoxes.removeFromTop(boxH);
    auto lufsBox = numBoxes.removeFromTop(boxH);
    auto drBox = numBoxes;

    drawNumericBox(g, peakBox.toFloat().reduced(2, 1),
                   peakValue_, "Peak", "dB", MixCoachTheme::meterYellow());
    drawNumericBox(g, rmsBox.toFloat().reduced(2, 1),
                   rmsValue_, "RMS", "dB", MixCoachTheme::accent());
    drawNumericBox(g, lufsBox.toFloat().reduced(2, 1),
                   lufsValue_, "LUFS", "", MixCoachTheme::lufsIntegrated());
    drawNumericBox(g, drBox.toFloat().reduced(2, 1),
                   drValue_, "DR", "LU", MixCoachTheme::success());

    auto rightContent = rightThird.reduced(2, 2).toFloat();
    float mdrBarW = rightContent.getWidth() * 0.7f;

    float mBarH = rightContent.getHeight() * 0.35f;
    auto mBarArea = juce::Rectangle<float>(
        rightContent.getCentreX() - mdrBarW * 0.5f,
        rightContent.getY() + 2,
        mdrBarW, mBarH);
    drawLevelBar(g, mBarArea, lufsMomentary_.getCurrent(), MixCoachTheme::lufsMomentary(), "M");

    auto drBarArea = juce::Rectangle<float>(
        rightContent.getCentreX() - mdrBarW * 0.5f,
        rightContent.getBottom() - mBarH - 2,
        mdrBarW, mBarH);
    drawLevelBar(g, drBarArea, lufsRange_.getCurrent(), MixCoachTheme::accent(), "DR");
}

void MeterComponent::drawLevelBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                                   float level, juce::Colour colour, const juce::String& label)
{
    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(bounds, 3.0f);

    auto labelArea = bounds.removeFromBottom(14).reduced(1, 0);
    g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
    g.setColour(MixCoachTheme::textDim());
    g.drawText(label, labelArea, juce::Justification::centred);

    g.setColour(MixCoachTheme::border().withAlpha(0.15f));
    float markers[] = { -24.0f, -18.0f, -12.0f, -6.0f, 0.0f };
    for (float m : markers) {
        float norm = juce::jlimit(0.0f, 1.0f, (m + 60.0f) / 66.0f);
        float y = bounds.getBottom() - bounds.getHeight() * norm;
        g.drawHorizontalLine((int)y, bounds.getX() + 1, bounds.getRight() - 1);
    }

    float norm = juce::jlimit(0.0f, 1.0f, (level + 60.0f) / 66.0f);
    if (norm > 0.01f) {
        auto fillBounds = bounds.withTop(bounds.getBottom() - bounds.getHeight() * norm);

        juce::Colour fillColour;
        if (level > -6.0f)      fillColour = MixCoachTheme::meterRed();
        else if (level > -12.0f) fillColour = MixCoachTheme::meterOrange();
        else if (level > -18.0f) fillColour = MixCoachTheme::meterYellow();
        else                     fillColour = colour;

        juce::ColourGradient barGrad(
            fillColour.withAlpha(0.85f),
            juce::Point<float>(0.0f, fillBounds.getY()),
            fillColour.withAlpha(0.15f),
            juce::Point<float>(0.0f, fillBounds.getBottom()),
            false);
        g.setGradientFill(barGrad);
        g.fillRoundedRectangle(fillBounds, 2.0f);

        auto glowRect = fillBounds.withHeight(juce::jmax(1.0f, fillBounds.getHeight() * 0.15f));
        juce::ColourGradient glow(
            juce::Colours::white.withAlpha(0.2f),
            juce::Point<float>(0.0f, glowRect.getY()),
            juce::Colour(0x00000000),
            juce::Point<float>(0.0f, glowRect.getBottom()),
            false);
        g.setGradientFill(glow);
        g.fillRoundedRectangle(glowRect, 2.0f);
    }
}

void MeterComponent::drawNumericBox(juce::Graphics& g, juce::Rectangle<float> bounds,
                                     float value, const juce::String& label,
                                     const juce::String& unit, juce::Colour colour)
{
    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(MixCoachTheme::border().withAlpha(0.3f));
    g.drawRoundedRectangle(bounds, 3.0f, 0.5f);

    auto labelArea = bounds.removeFromTop(bounds.getHeight() * 0.35f).reduced(4, 0);
    g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
    g.setColour(MixCoachTheme::textDim());
    g.drawText(label, labelArea, juce::Justification::centred);

    auto valArea = bounds.reduced(2, 0);
    juce::String valStr;
    if (value < -60.0f)
        valStr = "--.-";
    else
        valStr = juce::String(value, 1);

    if (!unit.isEmpty())
        valStr += " " + unit;

    g.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
    g.setColour(colour);
    g.drawText(valStr, valArea, juce::Justification::centred);
}

} // namespace mixcoach
