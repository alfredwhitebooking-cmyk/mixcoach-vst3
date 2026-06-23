#pragma once
#include <juce_graphics/juce_graphics.h>
#include "MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  drawAnalyzerHeader — Purple header with underline glow
// ═══════════════════════════════════════════════════════════════════════════
inline void drawAnalyzerHeader(juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title)
{
    g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
    g.setColour(MixCoachTheme::accentGlow());
    g.drawText(title, area, juce::Justification::centred);

    auto b = area.toFloat();
    float uy = b.getBottom() - 1.0f;
    float uw = b.getWidth();

    g.setColour(MixCoachTheme::divider().withAlpha(0.3f));
    g.drawHorizontalLine((int)uy + 1, b.getX(), b.getRight());

    float glowCx = b.getX() + uw * 0.5f;
    float glowW = juce::jmin(100.0f, uw * 0.6f);
    auto glowRect = juce::Rectangle<float>(glowCx - glowW * 0.5f, uy - 1.0f, glowW, 3.0f);
    juce::ColourGradient glow(
        MixCoachTheme::accent().withAlpha(0.25f), glowCx, uy,
        MixCoachTheme::accent().withAlpha(0.0f),  glowCx + glowW * 0.5f, uy, false);
    glow.addColour(0.5f, MixCoachTheme::accent().withAlpha(0.10f));
    g.setGradientFill(glow);
    g.fillRect(glowRect);
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawMetricCard — Individual metric card with label, value, unit
// ═══════════════════════════════════════════════════════════════════════════
inline void drawMetricCard(juce::Graphics& g, juce::Rectangle<int> bounds,
                           const juce::String& label, const juce::String& value,
                           const juce::String& unit, juce::Colour accent)
{
    auto b = bounds.toFloat();
    g.setColour(MixCoachTheme::bgInput().withAlpha(0.88f));
    g.fillRoundedRectangle(b, 5.0f);

    juce::ColourGradient top(
        juce::Colours::white.withAlpha(0.045f), b.getX(), b.getY(),
        juce::Colour(0x00000000), b.getX(), b.getCentreY(), false);
    g.setGradientFill(top);
    g.fillRoundedRectangle(b, 5.0f);

    g.setColour(accent.withAlpha(0.18f));
    g.drawRoundedRectangle(b.reduced(0.5f), 5.0f, 0.8f);

    auto content = bounds.reduced(5, 3);
    auto labelArea = content.removeFromTop(13);
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened());
    g.setColour(MixCoachTheme::textDim().withAlpha(0.78f));
    g.drawText(label, labelArea, juce::Justification::centred);

    auto valueArea = content;
    auto unitArea = valueArea.removeFromRight(22);
    g.setFont(juce::Font(juce::FontOptions(17.0f)).boldened());
    g.setColour(MixCoachTheme::textBright());
    g.drawText(value, valueArea, juce::Justification::centredRight);

    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)));
    g.setColour(MixCoachTheme::textDim());
    g.drawText(unit, unitArea, juce::Justification::centredLeft);
}

} // namespace mixcoach
