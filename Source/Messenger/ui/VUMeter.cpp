#include "VUMeter.h"

namespace mixcoach {

// ─── Constantes de color del gradiente (visual_design.md exacto) ───────────
constexpr uint32_t kGreen   = 0xFF22C55E;
constexpr uint32_t kLime    = 0xFF84CC16;
constexpr uint32_t kYellow  = 0xFFEAB308;
constexpr uint32_t kOrange  = 0xFFF97316;
constexpr uint32_t kRed     = 0xFFEF4444;

constexpr uint32_t kBgBar   = 0xFF0A0A0F;
constexpr uint32_t kBorder  = 0xFF2C2C3E;
constexpr uint32_t kGrid    = 0xFF1A1A2E;
constexpr uint32_t kScaleText = 0xFF6B7280;

// Escala exacta del visual design (top → bottom)
static constexpr float kScaleValues[] = {
    6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f,
    -30.0f, -36.0f, -42.0f, -48.0f, -60.0f
};
static constexpr int kNumScaleMarks = 11;

VUMeter::VUMeter()
{
    setOpaque(false);
}

void VUMeter::setLevel(float levelDb)
{
    auto newLevel = juce::jlimit(-80.0f, 6.0f, levelDb);

    // Attack: subida instantánea
    if (newLevel > currentLevel_) {
        currentLevel_ = newLevel;
    }
    // Release: caída suave (VU ballistic style)
    else {
        currentLevel_ += (newLevel - currentLevel_) * 0.15f;
    }

    // Peak hold con decay
    if (newLevel > peakLevel_) {
        peakLevel_ = newLevel;
        peakHoldTimer_ = 30;
    } else if (peakHoldTimer_ > 0) {
        peakHoldTimer_--;
    } else {
        peakLevel_ += (-80.0f - peakLevel_) * 0.03f;
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
    auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty()) return;

    // ─── Zonas internas ─────────────────────────────────────────────────
    //  peakZone(5px) | sidePad | bar(~10px) | sidePad | scaleZone(13px)
    const float peakW = 5.0f;
    const float scaleW = 13.0f;
    const float barW = 10.0f;  // Barra delgada estilo referencia

    float available = bounds.getWidth() - peakW - scaleW;
    float sidePad = (available - barW) * 0.5f;
    if (sidePad < 0.0f) sidePad = 0.0f;

    auto barBounds  = bounds.withTrimmedLeft(peakW + sidePad).withTrimmedRight(scaleW + sidePad);
    auto peakZone   = bounds.withWidth(peakW);
    auto scaleZone  = bounds.withTrimmedLeft(bounds.getWidth() - scaleW);

    const float radius = 0.8f;  // Esquinas sutiles para barra delgada

    // ─── Fondo del bar ──────────────────────────────────────────────────
    g.setColour(juce::Colour(kBgBar));
    g.fillRoundedRectangle(barBounds, radius);

    // ─── Grid lines ─────────────────────────────────────────────────────
    g.setColour(juce::Colour(kGrid).withAlpha(0.45f));
    for (float db : { -18.0f, -12.0f, -6.0f, 0.0f }) {
        float y = levelToY(db, barBounds);
        g.drawHorizontalLine((int)y, barBounds.getX() + 1.0f, barBounds.getRight() - 1.0f);
    }

    // ─── Barra rellena con gradiente (verde → rojo, bottom → top) ───────
    float norm = juce::jlimit(0.0f, 1.0f, (currentLevel_ + 60.0f) / 66.0f);
    if (norm > 0.005f) {
        float fillTop = barBounds.getBottom() - norm * barBounds.getHeight();
        auto fillRect = barBounds.withTop(fillTop);

        juce::ColourGradient grad(
            juce::Colour(kRed),                                  // top: rojo
            juce::Point<float>(fillRect.getCentreX(), fillRect.getY()),
            juce::Colour(kGreen),                                // bottom: verde
            juce::Point<float>(fillRect.getCentreX(), fillRect.getBottom()),
            false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(fillRect, radius);

        // Shine en la parte superior del fill
        auto shine = fillRect.withHeight(juce::jmax(1.5f, fillRect.getHeight() * 0.08f));
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.fillRoundedRectangle(shine, radius);
    }

    // ─── Peak triangle ◀ a la izquierda del bar ─────────────────────────
    if (peakLevel_ > -60.0f) {
        float peakY = levelToY(peakLevel_, barBounds);
        peakY = juce::jlimit(barBounds.getY(), barBounds.getBottom(), peakY);

        juce::Path tri;
        tri.addTriangle(
            peakZone.getX(), peakY,
            peakZone.getRight(), peakY - 3.0f,
            peakZone.getRight(), peakY + 3.0f);
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.fillPath(tri);
    }

    // ─── Escala completa a la derecha: 6, 0, -6, -12, ..., -60 ──────────
    g.setFont(juce::Font(juce::FontOptions(6.0f)));
    g.setColour(juce::Colour(kScaleText).withAlpha(0.7f));
    for (int i = 0; i < kNumScaleMarks; ++i) {
        float y = levelToY(kScaleValues[i], barBounds);
        g.drawText(juce::String((int)kScaleValues[i]),
                   juce::Rectangle<float>(scaleZone.getX(), y - 4.0f, scaleZone.getWidth(), 8.0f),
                   juce::Justification::centredRight);
    }

    // ─── Borde ──────────────────────────────────────────────────────────
    g.setColour(juce::Colour(kBorder).withAlpha(0.5f));
    g.drawRoundedRectangle(barBounds, radius, 1.0f);

    // ─── Clip indicator (> -0.5 dB) ─────────────────────────────────────
    if (currentLevel_ > -0.5f) {
        g.setColour(juce::Colour(kRed).withAlpha(0.55f));
        g.fillRoundedRectangle(barBounds, radius);
    }
}

float VUMeter::levelToY(float levelDb, juce::Rectangle<float> bounds) const
{
    // -60 dB → bottom, +6 dB → top
    float norm = juce::jlimit(0.0f, 1.0f, (levelDb + 60.0f) / 66.0f);
    return bounds.getBottom() - norm * bounds.getHeight();
}

juce::Colour VUMeter::getLevelColour(float levelDb, float alpha) const
{
    if (levelDb > -6.0f)
        return juce::Colour(kRed).withAlpha(alpha);
    else if (levelDb > -12.0f)
        return juce::Colour(kOrange).withAlpha(alpha);
    else if (levelDb > -18.0f)
        return juce::Colour(kYellow).withAlpha(alpha);
    else if (levelDb > -24.0f)
        return juce::Colour(kLime).withAlpha(alpha);
    else
        return juce::Colour(kGreen).withAlpha(alpha);
}

} // namespace mixcoach
