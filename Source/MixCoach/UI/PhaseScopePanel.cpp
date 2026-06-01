#include "PhaseScopePanel.h"

namespace mixcoach {

PhaseScopePanel::PhaseScopePanel()
{
    headerLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x94\x84 Phase Scope"),
                         juce::dontSendNotification);
    headerLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    headerLabel_.setJustificationType(juce::Justification::centredLeft);
    headerLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(headerLabel_);

    addAndMakeVisible(vectorscope_);
    addAndMakeVisible(phaseMeter_);
    addAndMakeVisible(crestHistogram_);
}

void PhaseScopePanel::resized()
{
    auto area = getLocalBounds().reduced(4, 2);
    headerLabel_.setBounds(area.removeFromTop(16));

    int pcH = area.getHeight();
    int vecH = pcH * 45 / 100;
    int phaseH = pcH * 25 / 100;
    int crestH = pcH - vecH - phaseH;

    vectorscope_.setBounds(area.removeFromTop(vecH).reduced(1));
    phaseMeter_.setBounds(area.removeFromTop(phaseH).reduced(1));
    crestHistogram_.setBounds(area.reduced(1));
}

void PhaseScopePanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);
}

} // namespace mixcoach
