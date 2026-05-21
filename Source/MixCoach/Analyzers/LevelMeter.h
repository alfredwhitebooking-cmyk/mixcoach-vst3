#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace mixcoach {

// ─── Level Meter con LUFS-style reduction ────────────────────────────────────
class LevelMeter : public juce::Component
{
public:
    LevelMeter()
        : label_("dB")
    {
        label_.setFont(juce::Font(juce::FontOptions(11.0f)));
        label_.setColour(juce::Label::textColourId, juce::Colours::white);
        label_.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label_);
    }

    void updateLevel(float newLevel) noexcept
    {
        activeLevel_ = newLevel;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto h = bounds.getHeight();
        auto w = bounds.getWidth();

        // Fondo oscuro
        g.setColour(juce::Colour(0xFF1A1A2E));
        g.fillRoundedRectangle(bounds, 3.0f);

        // Calcular altura de la barra
        float normalized = juce::jmap(activeLevel_, -60.0f, 0.0f, 0.0f, 1.0f);
        normalized = juce::jlimit(0.0f, 1.0f, normalized);

        auto barHeight = h * normalized;
        auto barBounds = bounds.removeFromBottom(barHeight);

        // Color según nivel
        juce::Colour barColour;
        if (activeLevel_ > -3.0f)
            barColour = juce::Colour(0xFFE74C3C); // Rojo (clipping)
        else if (activeLevel_ > -12.0f)
            barColour = juce::Colour(0xFFF1C40F); // Amarillo
        else if (activeLevel_ > -24.0f)
            barColour = juce::Colour(0xFF2ECC71); // Verde
        else
            barColour = juce::Colour(0xFF3498DB); // Azul (muy bajo)

        g.setColour(barColour);
        g.fillRoundedRectangle(barBounds.reduced(2, 0), 2.0f);

        // Texto del valor
        auto displayText = juce::String(activeLevel_, 1) + " dB";
        label_.setText(displayText, juce::dontSendNotification);
    }

    void resized() override
    {
        label_.setBounds(getLocalBounds().removeFromTop(14));
    }

private:
    float activeLevel_ = -60.0f;
    juce::Label label_;
};

} // namespace mixcoach
