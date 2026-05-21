#include "VirtualBusesComponent.h"

namespace mixcoach {

VirtualBusesComponent::VirtualBusesComponent()
{
    titleLabel_.setText("🔌 Buses Virtuales", juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(16.0f)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(titleLabel_);

    infoLabel_.setText("Arrastra pistas aquí para crear buses. Asígnale un color a cada familia.",
                       juce::dontSendNotification);
    infoLabel_.setFont(juce::Font(juce::FontOptions(13.0f)));
    infoLabel_.setJustificationType(juce::Justification::centred);
    infoLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    addAndMakeVisible(infoLabel_);

    // Buses predefinidos
    const char* busNames[] = { "Batería", "Bajo", "Guitarras", "Teclados", "Voces", "FX" };
    for (int i = 0; i < 6; ++i) {
        buses_.push_back({ busNames[i], kBusColours[i], -60.0f, 0 });
    }
}

void VirtualBusesComponent::resized()
{
    auto area = getLocalBounds().reduced(8);
    titleLabel_.setBounds(area.removeFromTop(24));

    if (buses_.empty()) {
        infoLabel_.setBounds(area);
        return;
    }

    float busHeight = (float)area.getHeight() / (float)buses_.size();
    for (int i = 0; i < (int)buses_.size(); ++i) {
        auto busArea = area.removeFromTop((int)busHeight).reduced(4, 2);
        // Pinta cada bus en paint()
        juce::ignoreUnused(busArea);
    }
}

void VirtualBusesComponent::paint(juce::Graphics& g)
{
    g.fillAll(MixCoachTheme::bgDark());

    auto area = getLocalBounds().reduced(8);
    auto contentArea = area;
    contentArea.removeFromTop(24); // title

    if (buses_.empty()) return;

    float busHeight = (float)contentArea.getHeight() / (float)buses_.size();

    for (int i = 0; i < (int)buses_.size(); ++i) {
        auto& bus = buses_[i];
        auto busArea = contentArea.removeFromTop((int)busHeight).reduced(4, 2);

        // Fondo del bus
        g.setColour(bus.colour.withAlpha(0.1f));
        g.fillRoundedRectangle(busArea.toFloat(), 4.0f);

        // Barra de color
        g.setColour(bus.colour);
        g.fillRoundedRectangle(busArea.removeFromLeft(4).toFloat(), 2.0f);

        // Nombre
        g.setColour(MixCoachTheme::textPrimary());
        g.setFont(juce::Font(juce::FontOptions(13.0f)).boldened());
        g.drawText(bus.name, busArea.removeFromLeft(100), juce::Justification::centredLeft);

        // Track count
        g.setColour(MixCoachTheme::textDim());
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText(juce::String(bus.trackCount) + " pistas",
                   busArea.removeFromLeft(80), juce::Justification::centredLeft);

        // Level meter simplificado
        auto meterArea = busArea.reduced(4, 4);
        g.setColour(MixCoachTheme::bgDarker());
        g.fillRoundedRectangle(meterArea.toFloat(), 2.0f);

        float norm = juce::jmap(bus.level, -60.0f, 0.0f, 0.0f, 1.0f);
        g.setColour(bus.colour);
        g.fillRoundedRectangle(meterArea.withWidth(meterArea.getWidth() * norm).toFloat(), 2.0f);
    }
}

} // namespace mixcoach
