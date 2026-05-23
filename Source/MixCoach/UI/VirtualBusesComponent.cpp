#include "VirtualBusesComponent.h"

namespace mixcoach {

VirtualBusesComponent::VirtualBusesComponent()
{
    titleLabel_.setText(juce::String(juce::CharPointer_UTF8("\xF0\x9F\x94\x8C Buses Virtuales")), juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeHeader)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textBright());
    addAndMakeVisible(titleLabel_);

    infoLabel_.setText(juce::String(juce::CharPointer_UTF8("\xF0\x9F\x93\x8C Arrastra pistas aqui para agruparlas en familias de sonido.")), juce::dontSendNotification);
    infoLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)));
    infoLabel_.setJustificationType(juce::Justification::centred);
    infoLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
    addAndMakeVisible(infoLabel_);

    // Buses predefinidos con colores profesionales
    const char* busNames[] = { "Bateria", "Bajo", "Guitarras", "Teclados", "Voces", "FX" };
    for (int i = 0; i < 6; ++i) {
        buses_.push_back({ busNames[i], getBusColour(i), -60.0f, 0 });
    }
}

void VirtualBusesComponent::resized()
{
    auto area = getLocalBounds().reduced(8);
    titleLabel_.setBounds(area.removeFromTop(24));

    if (buses_.empty()) {
        infoLabel_.setBounds(area);
    }
}

void VirtualBusesComponent::paint(juce::Graphics& g)
{
    g.fillAll(MixCoachTheme::bgDark());

    auto area = getLocalBounds().reduced(8);
    auto contentArea = area;
    contentArea.removeFromTop(24);

    if (buses_.empty()) return;

    float busHeight = (float)contentArea.getHeight() / (float)buses_.size();

    for (int i = 0; i < (int)buses_.size(); ++i) {
        auto& bus = buses_[i];
        auto busArea = contentArea.removeFromTop((int)busHeight).reduced(6, 4);

        // Glass card background
        MixCoachTheme::fillGlassPanel(g, busArea.toFloat(), 6.0f);

        // Color accent bar (left)
        auto accentBar = busArea.removeFromLeft(5);
        g.setColour(bus.colour);
        g.fillRoundedRectangle(accentBar.toFloat(), 2.0f);

        busArea.removeFromLeft(8);

        // Bus icon
        auto iconArea = busArea.removeFromLeft(24);
        const char* icons[] = {
            "\xF0\x9F\xA5\x81",  // Bateria
            "\xF0\x9F\x8E\xB8",  // Bajo
            "\xF0\x9F\x8E\xA8",  // Guitarras
            "\xF0\x9F\x8E\xB9",  // Teclados
            "\xF0\x9F\x8E\xA4",  // Voces
            "\xF0\x9F\x94\x80"   // FX
        };
        g.setFont(juce::Font(juce::FontOptions(16.0f)));
        g.drawText(icons[i], iconArea, juce::Justification::centred);

        // Bus name
        g.setColour(MixCoachTheme::textPrimary());
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)).boldened());
        auto nameArea = busArea.removeFromLeft(100);
        g.drawText(bus.name, nameArea, juce::Justification::centredLeft);

        // Track count badge
        if (bus.trackCount > 0) {
            auto badgeArea = busArea.removeFromLeft(50).reduced(2, 4);
            g.setColour(bus.colour.withAlpha(0.2f));
            g.fillRoundedRectangle(badgeArea.toFloat(), 8.0f);
            g.setColour(bus.colour);
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
            g.drawText(juce::String(bus.trackCount) + " pistas",
                       badgeArea, juce::Justification::centred);
        }

        // Level meter
        auto meterArea = busArea.reduced(4, 8);
        g.setColour(MixCoachTheme::bgDarker());
        g.fillRoundedRectangle(meterArea.toFloat(), 3.0f);

        float norm = juce::jmap(bus.level, -60.0f, 0.0f, 0.0f, 1.0f);
        norm = juce::jlimit(0.0f, 1.0f, norm);

        if (norm > 0.0f) {
            g.setColour(bus.colour);
            auto fillBar = meterArea.withWidth((int)(meterArea.getWidth() * norm));
            if (fillBar.getWidth() > 1)
                g.fillRoundedRectangle(fillBar.toFloat(), 3.0f);

            // Glow
            juce::ColourGradient glow(
                bus.colour.withAlpha(0.2f),
                juce::Point<float>(fillBar.getCentreX(), fillBar.getY()),
                juce::Colour(0x00000000),
                juce::Point<float>(fillBar.getRight(), fillBar.getCentreY()),
                false);
            g.setGradientFill(glow);
            g.fillRect(fillBar);
        }
    }
}

} // namespace mixcoach
