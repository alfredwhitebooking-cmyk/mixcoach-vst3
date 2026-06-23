#include "ColourSwatch.h"
#include "../../MixCoach/UI/MixCoachTheme.h"

namespace mixcoach {

    ColourSwatch::ColourSwatch()
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setOpaque(false);
    }

    void ColourSwatch::setColour(juce::Colour col)
    {
        colour_ = col;
        repaint();
    }

    void ColourSwatch::mouseDown(const juce::MouseEvent&)
    {
        if (onClick) onClick();
    }

    void ColourSwatch::mouseEnter(const juce::MouseEvent&)
    {
        repaint();
    }

    void ColourSwatch::mouseExit(const juce::MouseEvent&)
    {
        repaint();
    }

    void ColourSwatch::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        g.setColour(colour_);
        g.fillRoundedRectangle(bounds, 4.0f);

        if (colour_.getBrightness() < 0.3f) {
            g.setColour(juce::Colours::white.withAlpha(0.15f));
            g.fillRoundedRectangle(bounds.reduced(2.0f), 2.0f);
        }

        g.setColour(MixCoachTheme::bgDark());
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        if (isMouseOver()) {
            g.setColour(juce::Colours::white.withAlpha(0.3f));
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.drawText("\u25BC", bounds, juce::Justification::bottomRight);
        }
    }

} // namespace mixcoach
