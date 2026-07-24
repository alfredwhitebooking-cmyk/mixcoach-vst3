#include "LevelMeter.h"

namespace mixcoach {

LevelMeter::LevelMeter()
{
    valueLabel_.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
    valueLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    valueLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(valueLabel_);
}

void LevelMeter::updateLevel(float levelDb) noexcept
{
    levelDb_ = levelDb;
    valueLabel_.setText(juce::String(levelDb, 1) + " dB", juce::dontSendNotification);
    repaint();
}

void LevelMeter::setTrackLabel(const juce::String& label)
{
    trackLabel_ = label;
    repaint();
}

void LevelMeter::reset() noexcept
{
    levelDb_ = -60.0f;
    valueLabel_.setText("-inf dB", juce::dontSendNotification);
    repaint();
}

juce::Colour LevelMeter::getBarColour(float db) const noexcept
{
    if (db > -3.0f)  return MixCoachTheme::error();       // Rojo — clipping
    if (db > -12.0f) return MixCoachTheme::warning();      // Amarillo — alto
    if (db > -24.0f) return MixCoachTheme::success();      // Verde — sano
    return MixCoachTheme::accentCyan().withMultipliedBrightness(0.7f); // Azul — bajo
}

void LevelMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float cr = 4.0f;

    // ─── Fondo oscuro ──────────────────────────────────────────────────
    g.setColour(MixCoachTheme::bgPanel());
    g.fillRoundedRectangle(bounds, cr);

    // ─── Barra de nivel vertical (de abajo hacia arriba) ───────────────
    const float padding = 4.0f;
    auto barArea = bounds.reduced(padding, padding + 16.0f); // espacio para label arriba
    float barH = barArea.getHeight();
    float normalized = juce::jmap(levelDb_, -60.0f, 0.0f, 0.0f, 1.0f);
    normalized = juce::jlimit(0.0f, 1.0f, normalized);

    float fillH = barH * normalized;

    // ─── Track de fondo de la barra ────────────────────────────────────
    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(barArea, 2.0f);

    // ─── Barra llena ──────────────────────────────────────────────────
    if (fillH > 1.0f) {
        auto fillArea = barArea.withTop(barArea.getBottom() - fillH);
        juce::Colour barColour = getBarColour(levelDb_);

        // Gradiente vertical para darle profundidad
        juce::ColourGradient grad(barColour.withAlpha(0.9f),
                                   fillArea.getX(), fillArea.getY(),
                                   barColour.withAlpha(0.5f),
                                   fillArea.getX(), fillArea.getBottom(),
                                   false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(fillArea, 2.0f);

        // ─── Brillo superior (highlight) ──────────────────────────────
        auto highlight = fillArea.withHeight(3.0f);
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.fillRoundedRectangle(highlight, 1.0f);
    }

    // ─── Líneas de referencia (0dB, -12dB, -24dB) ──────────────────────
    auto drawDbLine = [&](float db, float alpha) {
        float y = barArea.getBottom() - barH * juce::jmap(db, -60.0f, 0.0f, 0.0f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(alpha));
        g.drawHorizontalLine((int)y, barArea.getX(), barArea.getRight());
    };
    drawDbLine(-6.0f, 0.08f);
    drawDbLine(-12.0f, 0.06f);
    drawDbLine(-24.0f, 0.04f);

    // ─── Label del track ────────────────────────────────────────────────
    if (!trackLabel_.isEmpty()) {
        auto labelArea = bounds.removeFromTop(14.0f).reduced(2, 0);
        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.setColour(MixCoachTheme::textDim());
        g.drawText(trackLabel_, labelArea.toNearestInt(), juce::Justification::centred);
    }
}

void LevelMeter::resized()
{
    valueLabel_.setBounds(getLocalBounds().removeFromBottom(16));
}

} // namespace mixcoach
