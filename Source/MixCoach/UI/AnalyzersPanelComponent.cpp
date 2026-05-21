#include "AnalyzersPanelComponent.h"

namespace mixcoach {

AnalyzersPanelComponent::AnalyzersPanelComponent()
{
    titleLabel_.setText("Analizadores", juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(16.0f)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(titleLabel_);

    spectrumLabel_.setText("Espectro", juce::dontSendNotification);
    spectrumLabel_.setFont(juce::Font(juce::FontOptions(12.0f)));
    spectrumLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    addAndMakeVisible(spectrumLabel_);
    addAndMakeVisible(spectrumAnalyzer);

    phaseLabel_.setText("Fase", juce::dontSendNotification);
    phaseLabel_.setFont(juce::Font(juce::FontOptions(12.0f)));
    phaseLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    addAndMakeVisible(phaseLabel_);
    addAndMakeVisible(phaseMeter);

    levelLabel_.setText("Nivel", juce::dontSendNotification);
    levelLabel_.setFont(juce::Font(juce::FontOptions(12.0f)));
    levelLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    addAndMakeVisible(levelLabel_);
    addAndMakeVisible(masterLevelMeter);
    addAndMakeVisible(leftLevelMeter);
    addAndMakeVisible(rightLevelMeter);

    addAndMakeVisible(targetMarkers);
}

void AnalyzersPanelComponent::resized()
{
    auto area = getLocalBounds().reduced(6);
    titleLabel_.setBounds(area.removeFromTop(24));

    auto contentArea = area.reduced(2);
    auto spectrumArea = contentArea.removeFromLeft(contentArea.getWidth() * 0.4f);
    spectrumLabel_.setBounds(spectrumArea.removeFromTop(18));
    spectrumAnalyzer.setBounds(spectrumArea.reduced(2));

    auto phaseArea = contentArea.removeFromLeft(contentArea.getWidth() * 0.25f);
    phaseLabel_.setBounds(phaseArea.removeFromTop(18));
    phaseMeter.setBounds(phaseArea.reduced(2));

    auto levelArea = contentArea.reduced(2);
    levelLabel_.setBounds(levelArea.removeFromTop(18));

    auto meterRow = levelArea.reduced(2);
    auto leftMeterArea = meterRow.removeFromLeft(meterRow.getWidth() / 3);
    leftLevelMeter.setBounds(leftMeterArea);
    auto rightMeterArea = meterRow.removeFromRight(meterRow.getWidth() / 2);
    rightLevelMeter.setBounds(rightMeterArea);
    masterLevelMeter.setBounds(meterRow);
}

void AnalyzersPanelComponent::paint(juce::Graphics& g)
{
    g.fillAll(MixCoachTheme::bgPanel());
    g.setColour(MixCoachTheme::border());
    g.drawRect(getLocalBounds(), 1);
}

void AnalyzersPanelComponent::updateSpectrum(const float* data, int numBins)
{
    spectrumAnalyzer.updateSpectrum(data, numBins);
}

void AnalyzersPanelComponent::updatePhase(float correlation)
{
    phaseMeter.updateCorrelation(correlation);
}

void AnalyzersPanelComponent::updateLevels(float left, float right)
{
    leftLevelMeter.updateLevel(left);
    rightLevelMeter.updateLevel(right);
    masterLevelMeter.updateLevel((left + right) * 0.5f);
}

} // namespace mixcoach
