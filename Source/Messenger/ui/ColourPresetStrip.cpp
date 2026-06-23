#include "ColourPresetStrip.h"
#include "../../MixCoach/UI/MixCoachTheme.h"

namespace mixcoach {

    const juce::Colour ColourPresetStrip::presetColours_[8] = {
        MixCoachTheme::error(), // Rojo     — Drums
        MixCoachTheme::warning(), // Naranja  — Percusión
        MixCoachTheme::warning(), // Amarillo — Teclados
        MixCoachTheme::success(), // Verde    — Guitarras
        MixCoachTheme::roleFX(), // Turquesa — FX
        MixCoachTheme::info(), // Azul     — Bass
        MixCoachTheme::accent(), // Púrpura  — Voces
        MixCoachTheme::roleVocals(), // Rosa     — Coros
    };

    ColourPresetStrip::ColourPresetStrip()
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setOpaque(false);
    }

    void ColourPresetStrip::setActiveColour(juce::Colour col)
    {
        activeColour_ = col;
        repaint();
    }

    void ColourPresetStrip::mouseDown(const juce::MouseEvent& e)
    {
        auto bounds      = getLocalBounds().reduced(2);
        int numPresets   = getNumPresets();
        float dotSize    = (float)(bounds.getHeight() - 2) * 0.75f;
        float spacing    = (float)(bounds.getWidth() - 2) / (float)numPresets;
        float dotSpacing = juce::jmin(spacing, dotSize + 4.0f);
        float startX =
            bounds.getX() + ((float)bounds.getWidth() - dotSpacing * (float)(numPresets - 1) - dotSize) * 0.5f;

        for (int i = 0; i < numPresets; ++i) {
            float cx = startX + (float)i * dotSpacing + dotSize * 0.5f;
            float cy = bounds.getCentreY();
            float dx = e.x - cx;
            float dy = e.y - cy;
            if (dx * dx + dy * dy <= dotSize * dotSize * 0.25f) {
                if (onColourChosen) onColourChosen(presetColours_[i]);
                break;
            }
        }
    }

    void ColourPresetStrip::paint(juce::Graphics& g)
    {
        auto bounds      = getLocalBounds().reduced(2);
        int numPresets   = getNumPresets();
        float dotSize    = (float)(bounds.getHeight() - 2) * 0.75f;
        float spacing    = (float)(bounds.getWidth() - 2) / (float)numPresets;
        float dotSpacing = juce::jmin(spacing, dotSize + 4.0f);
        float startX =
            bounds.getX() + ((float)bounds.getWidth() - dotSpacing * (float)(numPresets - 1) - dotSize) * 0.5f;

        for (int i = 0; i < numPresets; ++i) {
            float cx       = startX + (float)i * dotSpacing;
            auto dotBounds = juce::Rectangle<float>(cx, (float)bounds.getCentreY() - dotSize * 0.5f, dotSize, dotSize);

            bool isActive = (presetColours_[i].getARGB() == activeColour_.getARGB());

            // Sombra exterior más prominente para el activo
            if (isActive) {
                g.setColour(juce::Colours::white.withAlpha(0.35f));
                g.fillEllipse(dotBounds.reduced(-3.0f));
                // Glow neón alrededor del activo
                g.setColour(presetColours_[i].withAlpha(0.3f));
                g.fillEllipse(dotBounds.reduced(-5.0f));
            }

            // Círculo de color principal
            g.setColour(presetColours_[i]);
            g.fillEllipse(dotBounds.reduced(isActive ? 1.0f : 2.0f));

            // Brillo interno (highlights)
            auto shineBounds = dotBounds.reduced(isActive ? 2.0f : 3.0f);
            shineBounds      = shineBounds.withHeight(shineBounds.getHeight() * 0.45f);
            juce::ColourGradient shine(juce::Colours::white.withAlpha(0.35f),
                                       shineBounds.getCentreX(),
                                       shineBounds.getY(),
                                       juce::Colour(0x00000000),
                                       shineBounds.getCentreX(),
                                       shineBounds.getBottom(),
                                       false);
            g.setGradientFill(shine);
            g.fillEllipse(shineBounds);

            // Borde más definido
            g.setColour(MixCoachTheme::bgDark().withAlpha(isActive ? 0.9f : 0.5f));
            g.drawEllipse(dotBounds.reduced(isActive ? 1.0f : 2.0f), 1.0f);
        }
    }

} // namespace mixcoach
