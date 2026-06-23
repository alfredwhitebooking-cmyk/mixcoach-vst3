#include "RefToggle.h"
#include "MixCoachTheme.h"

namespace mixcoach {

    RefToggle::RefToggle() {}

    void RefToggle::paint(juce::Graphics& g)
    {
        auto btn   = getLocalBounds().toFloat();
        bool on    = toggled;
        bool hover = hovered;

        auto bgCol  = on ? juce::Colour(0x44FFD700) : juce::Colour(0x1A888888);
        auto fgCol  = on ? juce::Colour(0xCCFFD700) : juce::Colour(0x55999999);
        auto dotCol = on ? MixCoachTheme::warning() : juce::Colour(0x66999999);

        if (hover) {
            bgCol  = bgCol.brighter(0.6f);
            fgCol  = fgCol.brighter(0.4f);
            dotCol = dotCol.brighter(0.3f);
        }

        // Glow en hover
        if (hover) {
            g.setColour(on ? juce::Colour(0x22FFD700) : juce::Colour(0x11888888));
            g.fillRoundedRectangle(btn.expanded(2.0f, 2.0f), 4.0f);
        }

        // Background pill
        g.setColour(bgCol);
        g.fillRoundedRectangle(btn, 3.0f);
        g.setColour(fgCol);
        g.drawRoundedRectangle(btn, 3.0f, hover ? 1.2f : 0.7f);

        // Status dot (left side)
        float dotR = hover ? 3.5f : 3.0f;
        float dotX = btn.getX() + dotR + 2.0f;
        float dotY = btn.getCentreY();
        g.setColour(dotCol);
        g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);

        // Label
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)).boldened());
        g.setColour(fgCol);
        g.drawText("REF", btn, juce::Justification::centred);

        // Drawn tooltip en hover
        if (hover) {
            juce::String tipText = on ? juce::CharPointer_UTF8("Hide reference curve  ")
                                      : juce::CharPointer_UTF8("Show reference curve  ");

            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
            float tipW = (float)tipText.length() * 5.0f + 8.0f;
            float tipH = 14.0f;
            float tipX = btn.getX() - tipW - 2.0f;
            float tipY = btn.getCentreY() - tipH * 0.5f;

            if (tipX < 2.0f) tipX = btn.getRight() + 2.0f;

            auto tipRect = juce::Rectangle<float>(tipX, tipY, tipW, tipH);

            g.setColour(juce::Colour(0xEE0A0E1A));
            g.fillRoundedRectangle(tipRect, 3.0f);
            g.setColour(juce::Colour(0x55FFD700));
            g.drawRoundedRectangle(tipRect, 3.0f, 0.6f);

            g.setColour(juce::Colour(0xEEFFD700));
            g.drawText(tipText, tipRect, juce::Justification::centred);
        }
    }

    void RefToggle::mouseDown(const juce::MouseEvent&)
    {
        if (onClick) onClick();
    }

    void RefToggle::mouseEnter(const juce::MouseEvent&)
    {
        hovered = true;
        repaint();
    }

    void RefToggle::mouseExit(const juce::MouseEvent&)
    {
        hovered = false;
        repaint();
    }

    void RefToggle::mouseMove(const juce::MouseEvent&)
    {
        if (!hovered) {
            hovered = true;
            repaint();
        }
    }

} // namespace mixcoach
