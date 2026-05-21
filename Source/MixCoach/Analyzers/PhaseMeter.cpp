#include "PhaseMeter.h"

namespace mixcoach {

PhaseMeter::PhaseMeter()
{
    valueLabel_.setFont(juce::Font(juce::FontOptions(12.0f)));
    valueLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
    valueLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(valueLabel_);
}

void PhaseMeter::updateCorrelation(float correlation)
{
    correlation_ = correlation;
    valueLabel_.setText(juce::String(correlation, 2), juce::dontSendNotification);
    repaint();
}

void PhaseMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2);
    g.fillAll(juce::Colour(0xFF1A1A2E));

    // Fondo del medidor
    g.setColour(juce::Colour(0xFF16213E));
    g.fillRoundedRectangle(bounds, 4.0f);

    auto meterWidth = bounds.getWidth() - 20;
    auto meterBounds = bounds.reduced(10, bounds.getHeight() * 0.3f);

    // Centro = 0, izquierda = -1, derecha = +1
    float normalized = juce::jmap(correlation_, -1.0f, 1.0f, 0.0f, 1.0f);
    float markerX = meterBounds.getX() + normalized * meterBounds.getWidth();

    // Barra de fondo
    g.setColour(juce::Colour(0xFF2C2C3E));
    g.fillRoundedRectangle(meterBounds, 2.0f);

    // Indicador
    juce::Colour indicatorColour;
    if (std::abs(correlation_) < 0.3f)
        indicatorColour = juce::Colour(0xFFE74C3C); // Rojo - problemas de fase
    else if (correlation_ < 0.0f)
        indicatorColour = juce::Colour(0xFFF39C12); // Naranja - fuera de fase
    else
        indicatorColour = juce::Colour(0xFF2ECC71); // Verde - buena fase

    g.setColour(indicatorColour);
    g.fillRoundedRectangle(meterBounds.withWidth(meterBounds.getWidth() * normalized), 2.0f);

    // Marcador de centro
    float centerX = meterBounds.getX() + meterBounds.getWidth() * 0.5f;
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawVerticalLine((int)centerX, meterBounds.getY(), meterBounds.getBottom());

    // Etiquetas
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    g.setColour(juce::Colours::grey);
    g.drawText("Out", meterBounds.removeFromLeft(30).toNearestInt(), juce::Justification::centredLeft);
    g.drawText("In", meterBounds.removeFromRight(30).toNearestInt(), juce::Justification::centredRight);

    valueLabel_.setBounds(getLocalBounds().removeFromBottom(20));
}

} // namespace mixcoach
