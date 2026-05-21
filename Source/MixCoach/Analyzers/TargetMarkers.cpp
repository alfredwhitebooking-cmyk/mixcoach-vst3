#include "TargetMarkers.h"

namespace mixcoach {

TargetMarkers::TargetMarkers()
{
    titleLabel_.setText("Referencias", juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(12.0f)));
    titleLabel_.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(titleLabel_);
}

void TargetMarkers::setTargets(const juce::StringArray& labels, const std::vector<float>& values)
{
    labels_ = labels;
    values_ = values;
    repaint();
}

void TargetMarkers::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll(juce::Colour(0xFF1A1A2E));

    titleLabel_.setBounds(bounds.removeFromTop(18).toNearestInt());

    if (values_.empty()) {
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.setColour(juce::Colours::grey);
        g.drawText("Sin referencias cargadas", bounds.toNearestInt(), juce::Justification::centred);
        return;
    }

    float rowHeight = bounds.getHeight() / (float)values_.size();
    for (size_t i = 0; i < values_.size(); ++i) {
        auto row = bounds.removeFromTop(rowHeight).reduced(4, 2);

        // Etiqueta
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.setColour(juce::Colours::lightgrey);
        g.drawText(labels_[i], row.removeFromLeft(row.getWidth() * 0.4f).toNearestInt(),
                   juce::Justification::centredLeft);

        // Barra de valor
        auto barBounds = row.reduced(2, 2);
        float norm = juce::jlimit(0.0f, 1.0f, values_[i] / 100.0f);

        g.setColour(juce::Colour(0xFF2C2C3E));
        g.fillRoundedRectangle(barBounds, 2.0f);

        g.setColour(juce::Colour(0xFF3498DB));
        g.fillRoundedRectangle(barBounds.withWidth(barBounds.getWidth() * norm), 2.0f);
    }
}

} // namespace mixcoach
