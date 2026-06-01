#include "VUMeter.h"

namespace mixcoach {

VUMeter::VUMeter()
{
    setOpaque(false);
}

void VUMeter::setLevel(float levelDb)
{
    // Smooth ballistics: attack rápido, release lento (VU style)
    auto newLevel = juce::jlimit(-80.0f, 6.0f, levelDb);

    // Attack: subida instantánea
    if (newLevel > currentLevel_) {
        currentLevel_ = newLevel;
    }
    // Release: caída suave (decaimiento como VU meter)
    else {
        currentLevel_ += (newLevel - currentLevel_) * 0.15f;
    }

    // Peak hold con decay
    if (newLevel > peakLevel_) {
        peakLevel_ = newLevel;
        peakHoldTimer_ = 30; // frames que se mantiene el peak
    } else if (peakHoldTimer_ > 0) {
        peakHoldTimer_--;
    } else {
        peakLevel_ += (-80.0f - peakLevel_) * 0.03f; // decay lento
    }

    if (std::abs(newLevel - lastDrawnLevel_) > 0.05f ||
        std::abs(peakLevel_ - lastDrawnPeak_) > 0.05f) {
        repaint();
        lastDrawnLevel_ = currentLevel_;
        lastDrawnPeak_ = peakLevel_;
    }
}

void VUMeter::setBarColour(juce::Colour col)
{
    barColour_ = col;
}

void VUMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);

    // ─── Fondo oscuro ─────────────────────────────────────────────────
    g.setColour(juce::Colour(0xFF0A0A15));
    g.fillRoundedRectangle(bounds, 3.0f);

    // ─── Grid de referencia ────────────────────────────────────────────
    g.setColour(juce::Colour(0xFF1A1A2E).withAlpha(0.5f));
    float gridLevels[] = { -18.0f, -12.0f, -6.0f, 0.0f };
    for (float gridDb : gridLevels) {
        float gridY = levelToY(gridDb, bounds);
        g.drawHorizontalLine((int)gridY, bounds.getX() + 2, bounds.getRight() - 2);
    }

    // ─── Labels de referencia ──────────────────────────────────────────
    g.setFont(juce::Font(juce::FontOptions(7.0f)));
    g.setColour(juce::Colour(0xFF555566));
    for (float gridDb : gridLevels) {
        float gridY = levelToY(gridDb, bounds);
        g.drawText(juce::String((int)gridDb),
                   juce::Rectangle<float>(bounds.getX() + 2, gridY - 6, 16, 8),
                   juce::Justification::centredLeft);
    }

    // ─── Barra principal (gradiente verde → amarillo → rojo) ───────────
    float barTop = levelToY(currentLevel_, bounds);
    auto barRect = bounds.withTop(barTop);

    if (barRect.getHeight() > 1.0f) {
        juce::ColourGradient barGrad(
            getLevelColour(currentLevel_, 1.0f),
            juce::Point<float>(0.0f, barRect.getY()),
            getLevelColour(currentLevel_, 1.0f).withAlpha(0.3f),
            juce::Point<float>(0.0f, barRect.getBottom()),
            false);
        g.setGradientFill(barGrad);
        g.fillRoundedRectangle(barRect, 2.0f);

        // Brillito en la parte superior de la barra
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        auto shineRect = barRect.withHeight(juce::jmax(2.0f, barRect.getHeight() * 0.1f));
        g.fillRoundedRectangle(shineRect, 1.0f);
    }

    // ─── Peak hold line ────────────────────────────────────────────────
    float peakY = levelToY(peakLevel_, bounds);
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.fillRoundedRectangle(
        juce::Rectangle<float>(bounds.getX() + 2, peakY - 1.0f, bounds.getWidth() - 4, 2.5f),
        1.0f);

    // ─── Borde ─────────────────────────────────────────────────────────
    g.setColour(juce::Colour(0xFF2C2C3E));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

    // ─── Clip indicator (si > -0.5dB) ──────────────────────────────────
    if (currentLevel_ > -0.5f) {
        g.setColour(juce::Colour(0xFFE74C3C).withAlpha(0.6f));
        g.fillRoundedRectangle(bounds, 3.0f);
    }
}

float VUMeter::levelToY(float levelDb, juce::Rectangle<float> bounds) const
{
    // Normalizar: -60dB → bottom, +6dB → top
    float norm = juce::jlimit(0.0f, 1.0f, (levelDb + 60.0f) / 66.0f);
    return bounds.getBottom() - norm * bounds.getHeight();
}

juce::Colour VUMeter::getLevelColour(float levelDb, float alpha) const
{
    if (levelDb > -6.0f)
        return juce::Colour(0xFFE74C3C).withAlpha(alpha);    // rojo
    else if (levelDb > -12.0f)
        return juce::Colour(0xFFF39C12).withAlpha(alpha);    // amarillo
    else if (levelDb > -18.0f)
        return juce::Colour(0xFF2ECC71).withAlpha(alpha);    // verde claro
    else
        return barColour_.withAlpha(alpha * 0.5f);           // color tenue
}

} // namespace mixcoach
