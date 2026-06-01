#include "PhaseCorrelationMeter.h"

namespace mixcoach {

PhaseCorrelationMeter::PhaseCorrelationMeter()
{
    titleLabel_.setText(juce::CharPointer_UTF8("\xCF\x86 Phase Correlation"),
                        juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(titleLabel_);

    valueLabel_.setText("+1.00", juce::dontSendNotification);
    valueLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)).boldened());
    valueLabel_.setJustificationType(juce::Justification::centred);
    valueLabel_.setColour(juce::Label::textColourId, MixCoachTheme::success());
    addAndMakeVisible(valueLabel_);
}

void PhaseCorrelationMeter::resized()
{
    auto area = getLocalBounds().reduced(2);
    titleLabel_.setBounds(area.removeFromTop(16));
    valueLabel_.setBounds(area.removeFromTop(18));
}

void PhaseCorrelationMeter::setCorrelation(float value)
{
    correlation_.setTarget(value);
    auto corr = correlation_.getCurrent();

    juce::Colour col;
    if (std::abs(corr) < 0.3f)
        col = MixCoachTheme::error();
    else if (corr < 0.0f)
        col = MixCoachTheme::warning();
    else
        col = MixCoachTheme::success();

    valueLabel_.setColour(juce::Label::textColourId, col);
    valueLabel_.setText(juce::String(corr, 2), juce::dontSendNotification);
    repaint();
}

void PhaseCorrelationMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(34);

    auto barBounds = area.reduced(8, 4).toFloat();

    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(barBounds, 4.0f);

    juce::ColourGradient bgGrad(
        MixCoachTheme::error().withAlpha(0.2f),
        juce::Point<float>(barBounds.getX(), 0.0f),
        MixCoachTheme::warning().withAlpha(0.15f),
        juce::Point<float>(barBounds.getCentreX(), 0.0f),
        false);
    bgGrad.addColour(0.75, MixCoachTheme::success().withAlpha(0.2f));
    g.setGradientFill(bgGrad);
    g.fillRoundedRectangle(barBounds, 4.0f);

    float corr = juce::jlimit(-1.0f, 1.0f, correlation_.getCurrent());
    float norm = (corr + 1.0f) * 0.5f;
    float markerX = barBounds.getX() + norm * barBounds.getWidth();

    juce::Colour indicatorColour;
    if (std::abs(corr) < 0.3f)
        indicatorColour = MixCoachTheme::error();
    else if (corr < 0.0f)
        indicatorColour = MixCoachTheme::warning();
    else
        indicatorColour = MixCoachTheme::success();

    juce::ColourGradient glow(
        indicatorColour.withAlpha(0.4f),
        juce::Point<float>(markerX, barBounds.getCentreY()),
        indicatorColour.withAlpha(0.0f),
        juce::Point<float>(markerX + 20.0f, barBounds.getCentreY()),
        false);
    g.setGradientFill(glow);
    g.fillEllipse(markerX - 10.0f, barBounds.getCentreY() - 6.0f, 20.0f, 12.0f);

    g.setColour(indicatorColour);
    g.fillEllipse(markerX - 5.0f, barBounds.getCentreY() - 5.0f, 10.0f, 10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.fillEllipse(markerX - 2.0f, barBounds.getCentreY() - 2.0f, 4.0f, 4.0f);

    g.setColour(MixCoachTheme::border().withAlpha(0.5f));
    g.drawRoundedRectangle(barBounds, 4.0f, 1.0f);

    g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    g.drawText("-1", juce::Rectangle<float>(barBounds.getX() - 2, barBounds.getBottom() + 2, 16, 12),
               juce::Justification::centred);
    g.drawText("0", juce::Rectangle<float>(barBounds.getCentreX() - 8, barBounds.getBottom() + 2, 16, 12),
               juce::Justification::centred);
    g.drawText("+1", juce::Rectangle<float>(barBounds.getRight() - 14, barBounds.getBottom() + 2, 16, 12),
               juce::Justification::centred);

    if (corr < -0.3f) {
        g.setColour(MixCoachTheme::error().withAlpha(0.5f));
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
        g.drawText("\xE2\x9A\xA0 Fuera de fase!", barBounds.withTop(barBounds.getBottom() - 22).toNearestInt(),
                   juce::Justification::centred);
    }
}

} // namespace mixcoach
