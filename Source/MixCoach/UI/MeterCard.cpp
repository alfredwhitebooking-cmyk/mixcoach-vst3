#include "MeterCard.h"

namespace mixcoach {

void MeterCard::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(MixCoachTheme::bgCard());
    g.fillRoundedRectangle(b, 4.0f);
    g.setColour(accent_.withAlpha(0.15f));
    g.drawRoundedRectangle(b.reduced(0.5f), 4.0f, 0.5f);

    auto labelArea = b.removeFromTop(b.getHeight() * 0.3f).reduced(2, 0);
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened());
    g.setColour(accent_.withAlpha(0.8f));
    g.drawText(label_, labelArea, juce::Justification::centred);

    auto numArea = b.removeFromTop(b.getHeight() * 0.55f);
    g.setFont(juce::Font(juce::FontOptions(18.0f)).boldened());
    g.setColour(MixCoachTheme::textBright());
    juce::String numStr;
    if (value_ > -60.0f)
        numStr = juce::String(value_, 2);
    else
        numStr = "--.-";
    g.drawText(numStr, numArea, juce::Justification::centred);

    auto unitArea = b;
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)));
    g.setColour(MixCoachTheme::textMuted());
    g.drawText(unit_, unitArea, juce::Justification::centred);
}

} // namespace mixcoach
