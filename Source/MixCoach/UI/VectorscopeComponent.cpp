#include "VectorscopeComponent.h"

namespace mixcoach {

VectorscopeComponent::VectorscopeComponent()
{
    titleLabel_.setText(juce::CharPointer_UTF8("\xE2\xAD\x90 Vectorscope"),
                        juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(titleLabel_);

    corrLabel_.setText("\xCF\x86: +1.00", juce::dontSendNotification);
    corrLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
    corrLabel_.setJustificationType(juce::Justification::centred);
    corrLabel_.setColour(juce::Label::textColourId, MixCoachTheme::success());
    addAndMakeVisible(corrLabel_);
}

void VectorscopeComponent::resized()
{
    auto area = getLocalBounds().reduced(2);
    titleLabel_.setBounds(area.removeFromTop(16));
    corrLabel_.setBounds(area.removeFromBottom(16));
}

void VectorscopeComponent::pushSample(float left, float right)
{
    auto& pt = trace_[writePos_ % kTraceLen];
    pt.x = juce::jlimit(-1.0f, 1.0f, left);
    pt.y = juce::jlimit(-1.0f, 1.0f, right);
    pt.alpha = 1.0f;
    writePos_ = (writePos_ + 1) % kTraceLen;

    float corr = 0.0f;
    float sumL2 = 0.0f, sumR2 = 0.0f, sumLR = 0.0f;
    int count = 0;
    for (int i = 0; i < kTraceLen; ++i) {
        auto& p = trace_[i];
        if (p.alpha > 0.01f) {
            sumLR += p.x * p.y;
            sumL2 += p.x * p.x;
            sumR2 += p.y * p.y;
            count++;
        }
        p.alpha *= 0.96f;
    }
    if (count > 0 && sumL2 > 0.0f && sumR2 > 0.0f) {
        corr = sumLR / (std::sqrt(sumL2) * std::sqrt(sumR2));
    }

    juce::Colour corrColour;
    if (std::abs(corr) < 0.3f)
        corrColour = MixCoachTheme::error();
    else if (corr < 0.0f)
        corrColour = MixCoachTheme::warning();
    else
        corrColour = MixCoachTheme::success();

    corrLabel_.setText("\xCF\x86: " + juce::String(corr, 2), juce::dontSendNotification);
    corrLabel_.setColour(juce::Label::textColourId, corrColour);

    repaint();
}

void VectorscopeComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(18);
    area.removeFromBottom(18);

    auto squareSize = juce::jmin(area.getWidth(), area.getHeight()) - 4;
    auto circleArea = juce::Rectangle<float>(0, 0, (float)squareSize, (float)squareSize)
                          .withCentre(area.toFloat().getCentre());
    drawGrid(g, circleArea);

    auto cx = circleArea.getCentreX();
    auto cy = circleArea.getCentreY();
    auto radius = circleArea.getWidth() * 0.5f - 4.0f;

    for (int i = 0; i < kTraceLen; ++i) {
        int idx = (writePos_ + i) % kTraceLen;
        auto& pt = trace_[idx];
        if (pt.alpha < 0.01f) continue;

        float sx = cx + pt.x * radius;
        float sy = cy + pt.y * radius;

        g.setColour(juce::Colour(0xFF00E676).withAlpha(pt.alpha * 0.6f));
        g.fillEllipse(sx - 1.5f, sy - 1.5f, 3.0f, 3.0f);
    }
}

void VectorscopeComponent::drawGrid(juce::Graphics& g, juce::Rectangle<float> area)
{
    auto cx = area.getCentreX();
    auto cy = area.getCentreY();
    auto radius = area.getWidth() * 0.5f - 4.0f;

    g.setColour(MixCoachTheme::border().withAlpha(0.4f));
    g.drawEllipse(area.reduced(4.0f), 1.0f);

    juce::ColourGradient innerGrad(
        MixCoachTheme::border().withAlpha(0.15f),
        cx, cy,
        MixCoachTheme::border().withAlpha(0.0f),
        cx + radius * 0.5f, cy,
        false);
    g.setGradientFill(innerGrad);
    g.drawEllipse(juce::Rectangle<float>(cx - radius * 0.5f, cy - radius * 0.5f,
                                          radius, radius), 1.0f);

    g.setColour(MixCoachTheme::border().withAlpha(0.2f));
    g.drawHorizontalLine((int)cy, area.getX() + 2, area.getRight() - 2);
    g.drawVerticalLine((int)cx, area.getY() + 2, area.getBottom() - 2);

    float d = radius * 0.707f;
    g.drawLine(cx - d, cy - d, cx + d, cy + d, 0.5f);
    g.drawLine(cx - d, cy + d, cx + d, cy - d, 0.5f);

    g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
    g.fillEllipse(cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);

    g.setFont(juce::Font(juce::FontOptions(7.0f)));
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
    g.drawText("L", juce::Rectangle<float>(area.getX(), cy - 8, 12, 12),
               juce::Justification::centred);
    g.drawText("R", juce::Rectangle<float>(area.getRight() - 14, cy - 8, 12, 12),
               juce::Justification::centred);
    g.drawText("R", juce::Rectangle<float>(cx - 8, area.getY(), 12, 12),
               juce::Justification::centred);
    g.drawText("L", juce::Rectangle<float>(cx - 8, area.getBottom() - 14, 12, 12),
               juce::Justification::centred);
}

} // namespace mixcoach
