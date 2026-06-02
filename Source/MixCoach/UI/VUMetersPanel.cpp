#include "VUMetersPanel.h"

namespace mixcoach {

VUMetersPanel::VUMetersPanel()
{
    headerLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x93\x8A VU Meters"),
                         juce::dontSendNotification);
    headerLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    headerLabel_.setJustificationType(juce::Justification::centredLeft);
    headerLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(headerLabel_);

    for (int i = 0; i < 4; ++i) {
        addAndMakeVisible(vuMeters_[i]);
    }
    // Subtle label colors — tonos grisáceo/neutros para no competir con la aguja
    vuMeters_[0].setLabel("L");
    vuMeters_[0].setMeterColour(MixCoachTheme::textDim());
    vuMeters_[1].setLabel("R");
    vuMeters_[1].setMeterColour(MixCoachTheme::textDim());
    vuMeters_[2].setLabel("M");
    vuMeters_[2].setMeterColour(MixCoachTheme::accentGlow().withAlpha(0.6f));
    vuMeters_[3].setLabel("S");
    vuMeters_[3].setMeterColour(MixCoachTheme::textDim());
}

void VUMetersPanel::resized()
{
    auto area = getLocalBounds().reduced(4, 2);
    headerLabel_.setBounds(area.removeFromTop(16));

    int vuGap = 4;
    int vuW = (area.getWidth() - vuGap) / 2;
    int vuH = (area.getHeight() - vuGap) / 2;

    vuMeters_[0].setBounds(area.getX(), area.getY(), vuW, vuH);
    vuMeters_[1].setBounds(area.getX() + vuW + vuGap, area.getY(), vuW, vuH);
    vuMeters_[2].setBounds(area.getX(), area.getY() + vuH + vuGap, vuW, vuH);
    vuMeters_[3].setBounds(area.getX() + vuW + vuGap, area.getY() + vuH + vuGap, vuW, vuH);
}

void VUMetersPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);
}

bool VUMetersPanel::advanceMeters(double sampleRateHz, bool allowRepaint)
{
    bool dirty = false;
    for (auto& m : vuMeters_)
        dirty |= m.advanceFrame(sampleRateHz, allowRepaint);
    return dirty;
}

} // namespace mixcoach
