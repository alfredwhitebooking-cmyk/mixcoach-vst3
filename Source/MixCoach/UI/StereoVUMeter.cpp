#include "StereoVUMeter.h"

namespace mixcoach {

StereoVUMeter::StereoVUMeter()
{
    titleLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x93\x88 Level (RMS + Peak)"),
                        juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(titleLabel_);

    leftLabel_.setText("L: --.- dB", juce::dontSendNotification);
    leftLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    leftLabel_.setJustificationType(juce::Justification::centred);
    leftLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    addAndMakeVisible(leftLabel_);

    rightLabel_.setText("R: --.- dB", juce::dontSendNotification);
    rightLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    rightLabel_.setJustificationType(juce::Justification::centred);
    rightLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    addAndMakeVisible(rightLabel_);
}

void StereoVUMeter::resized()
{
    auto area = getLocalBounds().reduced(2);
    titleLabel_.setBounds(area.removeFromTop(18));
    auto content = area.reduced(2, 0);
    auto halfWidth = content.getWidth() / 2;
    leftLabel_.setBounds(content.removeFromLeft(halfWidth).removeFromBottom(14));
    rightLabel_.setBounds(content.removeFromLeft(halfWidth).removeFromBottom(14));
}

void StereoVUMeter::setLevels(float leftRMS, float rightRMS,
                               float leftPeak, float rightPeak)
{
    leftRMS_.setTarget(leftRMS);
    rightRMS_.setTarget(rightRMS);
    leftPeak_.setTarget(leftPeak);
    rightPeak_.setTarget(rightPeak);

    if (leftPeak > leftPeakHold_) {
        leftPeakHold_ = leftPeak;
        leftHoldTimer_ = 30;
    } else if (leftHoldTimer_ > 0) {
        leftHoldTimer_--;
    } else {
        leftPeakHold_ += (-80.0f - leftPeakHold_) * 0.03f;
    }

    if (rightPeak > rightPeakHold_) {
        rightPeakHold_ = rightPeak;
        rightHoldTimer_ = 30;
    } else if (rightHoldTimer_ > 0) {
        rightHoldTimer_--;
    } else {
        rightPeakHold_ += (-80.0f - rightPeakHold_) * 0.03f;
    }

    leftLabel_.setText("L: " + juce::String(leftPeak, 1) + " dB", juce::dontSendNotification);
    rightLabel_.setText("R: " + juce::String(rightPeak, 1) + " dB", juce::dontSendNotification);
    repaint();
}

void StereoVUMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(20);
    area.removeFromBottom(14);

    auto content = area.reduced(2, 0);
    auto halfWidth = content.getWidth() / 2;

    drawChannelMeter(g, content.removeFromLeft(halfWidth).toFloat().reduced(2, 0),
                     leftRMS_.getCurrent(), leftPeak_.getCurrent(), leftPeakHold_,
                     "L", leftColour_);
    drawChannelMeter(g, content.removeFromLeft(halfWidth).toFloat().reduced(2, 0),
                     rightRMS_.getCurrent(), rightPeak_.getCurrent(), rightPeakHold_,
                     "R", rightColour_);
}

void StereoVUMeter::drawChannelMeter(juce::Graphics& g, juce::Rectangle<float> bounds,
                                      float rms, float peak, float peakHold,
                                      const juce::String& channelLabel,
                                      juce::Colour colour)
{
    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(MixCoachTheme::textDim());
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
    auto labelArea = bounds.removeFromTop(14).reduced(1, 0);
    g.drawText(channelLabel, labelArea, juce::Justification::centred);

    g.setColour(MixCoachTheme::border().withAlpha(0.3f));
    float gridLevels[] = { -18.0f, -12.0f, -6.0f, 0.0f };
    for (float gl : gridLevels) {
        float glNorm = juce::jlimit(0.0f, 1.0f, (gl + 60.0f) / 66.0f);
        float glY = bounds.getBottom() - bounds.getHeight() * glNorm;
        g.drawHorizontalLine((int)glY, bounds.getX() + 2, bounds.getRight() - 2);
        g.setFont(juce::Font(juce::FontOptions(6.5f)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.4f));
        g.drawText(juce::String((int)gl),
                   juce::Rectangle<float>(bounds.getX() + 2, glY - 5, 14, 8),
                   juce::Justification::centredLeft);
    }

    float norm = juce::jlimit(0.0f, 1.0f, (rms + 60.0f) / 66.0f);
    if (norm > 0.01f) {
        auto fillBounds = bounds.withTop(bounds.getBottom() - bounds.getHeight() * norm);
        juce::Colour fillColour;
        if (rms > -6.0f)      fillColour = MixCoachTheme::meterRed();
        else if (rms > -12.0f) fillColour = MixCoachTheme::meterOrange();
        else if (rms > -18.0f) fillColour = MixCoachTheme::meterYellow();
        else                   fillColour = colour;

        juce::ColourGradient barGrad(
            fillColour.withAlpha(0.9f),
            juce::Point<float>(0.0f, fillBounds.getY()),
            fillColour.withAlpha(0.2f),
            juce::Point<float>(0.0f, fillBounds.getBottom()),
            false);
        g.setGradientFill(barGrad);
        g.fillRoundedRectangle(fillBounds, 3.0f);
    }

    if (peak > -60.0f) {
        float peakNorm = juce::jlimit(0.0f, 1.0f, (peak + 60.0f) / 66.0f);
        float peakY = bounds.getBottom() - bounds.getHeight() * peakNorm;
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.fillEllipse(bounds.getCentreX() - 3.0f, peakY - 2.0f, 6.0f, 4.0f);
    }

    if (peakHold > -60.0f) {
        float holdNorm = juce::jlimit(0.0f, 1.0f, (peakHold + 60.0f) / 66.0f);
        float holdY = bounds.getBottom() - bounds.getHeight() * holdNorm;
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.fillRect(bounds.getX() + 2, holdY - 0.5f, bounds.getWidth() - 4, 1.5f);
    }
}

} // namespace mixcoach
