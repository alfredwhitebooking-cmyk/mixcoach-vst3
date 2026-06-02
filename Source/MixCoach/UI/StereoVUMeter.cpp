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
                     "L");
    drawChannelMeter(g, content.removeFromLeft(halfWidth).toFloat().reduced(2, 0),
                     rightRMS_.getCurrent(), rightPeak_.getCurrent(), rightPeakHold_,
                     "R");
}

void StereoVUMeter::drawChannelMeter(juce::Graphics& g, juce::Rectangle<float> bounds,
                                      float rms, float peak, float peakHold,
                                      const juce::String& channelLabel)
{
    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(MixCoachTheme::textDim());
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
    auto labelArea = bounds.removeFromTop(14).reduced(1, 0);
    g.drawText(channelLabel, labelArea, juce::Justification::centred);

    // ─── Scale labels (full range según visual design: 6, 0, -6, ..., -60) ──
    constexpr float kScaleVals[] = { 6.0f, 0.0f, -6.0f, -12.0f, -18.0f,
                                     -24.0f, -30.0f, -36.0f, -42.0f, -48.0f, -60.0f };
    constexpr int kNumScale = 11;
    g.setFont(juce::Font(juce::FontOptions(5.5f)));
    for (int i = 0; i < kNumScale; ++i) {
        float glNorm = juce::jlimit(0.0f, 1.0f, (kScaleVals[i] + 60.0f) / 66.0f);
        float glY = bounds.getBottom() - bounds.getHeight() * glNorm;
        g.setColour(MixCoachTheme::rowDivider().withAlpha(0.45f));
        g.drawHorizontalLine((int)glY, bounds.getX() + 1, bounds.getRight() - 1);
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
        g.drawText(juce::String((int)kScaleVals[i]),
                   juce::Rectangle<float>(bounds.getX() + 1, glY - 4.0f, 14, 8),
                   juce::Justification::centredLeft);
    }

    // ─── Gradient bar (multi-stop: green→lime→yellow→orange→red, bottom→top) ─
    float norm = juce::jlimit(0.0f, 1.0f, (rms + 60.0f) / 66.0f);
    if (norm > 0.01f) {
        auto fillBounds = bounds.withTop(bounds.getBottom() - bounds.getHeight() * norm);

        juce::ColourGradient barGrad;
        barGrad.isRadial = false;
        barGrad.point1 = juce::Point<float>(fillBounds.getCentreX(), fillBounds.getY());    // top
        barGrad.point2 = juce::Point<float>(fillBounds.getCentreX(), fillBounds.getBottom()); // bottom
        barGrad.addColour(0.00f, MixCoachTheme::meterRed());
        barGrad.addColour(0.05f, MixCoachTheme::meterRed());
        barGrad.addColour(0.15f, MixCoachTheme::meterOrange());
        barGrad.addColour(0.30f, MixCoachTheme::meterYellow());
        barGrad.addColour(0.50f, MixCoachTheme::meterLime());
        barGrad.addColour(1.00f, MixCoachTheme::meterGreen());

        g.setGradientFill(barGrad);
        g.fillRoundedRectangle(fillBounds, 3.0f);

        // Shine en la parte superior del fill
        auto shine = fillBounds.withHeight(juce::jmax(2.0f, fillBounds.getHeight() * 0.08f));
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.fillRoundedRectangle(shine, 3.0f);
    }

    // ─── Peak triangle ◀ a la izquierda ──────────────────────────────────
    if (peak > -60.0f) {
        float peakNorm = juce::jlimit(0.0f, 1.0f, (peak + 60.0f) / 66.0f);
        float peakY = bounds.getBottom() - bounds.getHeight() * peakNorm;
        
        juce::Path tri;
        tri.addTriangle(bounds.getX() - 1.0f, peakY,
                        bounds.getX() + 4.0f, peakY - 2.5f,
                        bounds.getX() + 4.0f, peakY + 2.5f);
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.fillPath(tri);
    }

    // ─── Peak hold line (horizontal bar) ─────────────────────────────────
    if (peakHold > -60.0f) {
        float holdNorm = juce::jlimit(0.0f, 1.0f, (peakHold + 60.0f) / 66.0f);
        float holdY = bounds.getBottom() - bounds.getHeight() * holdNorm;
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.fillRect(bounds.getX() + 2, holdY - 0.5f, bounds.getWidth() - 4, 2.0f);
    }
}

} // namespace mixcoach
