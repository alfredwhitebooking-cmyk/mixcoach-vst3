#include "StereoMeter.h"

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

// ═══════════════════════════════════════════════════════════════════════════

StereoMeter::StereoMeter()
{
    setOpaque(false);
}

void StereoMeter::setLevels(float leftDb, float rightDb)
{
    auto newL = juce::jlimit(-80.0f, 6.0f, leftDb);
    auto newR = juce::jlimit(-80.0f, 6.0f, rightDb);

    // ─── Ballistics L ──────────────────────────────────────────────────
    if (newL > leftLevel_) leftLevel_ = newL;
    else leftLevel_ += (newL - leftLevel_) * 0.15f;

    // ─── Ballistics R ──────────────────────────────────────────────────
    if (newR > rightLevel_) rightLevel_ = newR;
    else rightLevel_ += (newR - rightLevel_) * 0.15f;

    // ─── Peak hold L ───────────────────────────────────────────────────
    if (newL > leftPeak_) { leftPeak_ = newL; leftHoldTimer_ = 30; }
    else if (leftHoldTimer_ > 0) leftHoldTimer_--;
    else leftPeak_ += (-80.0f - leftPeak_) * 0.03f;

    // ─── Peak hold R ───────────────────────────────────────────────────
    if (newR > rightPeak_) { rightPeak_ = newR; rightHoldTimer_ = 30; }
    else if (rightHoldTimer_ > 0) rightHoldTimer_--;
    else rightPeak_ += (-80.0f - rightPeak_) * 0.03f;

    // ─── Clip detection (> -0.5 dB) ────────────────────────────────────
    if (newL > -0.5f || newR > -0.5f) {
        clipActive_ = true;
        clipHoldTimer_ = 30;  // ~1 segundo a 30fps
    } else if (clipHoldTimer_ > 0) {
        clipHoldTimer_--;
    } else {
        clipActive_ = false;
    }

    repaint();
}

void StereoMeter::setBarColours(juce::Colour leftCol, juce::Colour rightCol)
{
    barColourL_ = leftCol;
    barColourR_ = rightCol;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Paint — Layout compartido: peak | L bar | R bar | scale
// ═══════════════════════════════════════════════════════════════════════════

void StereoMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty()) return;

    // ─── Layout interno ────────────────────────────────────────────────
    //  peakZone(5px) | sidePad | L bar(10px) | gap(4px) | R bar(10px) | sidePad | scaleZone(13px)
    const float peakW  = 5.0f;
    const float scaleW = 13.0f;
    const float barW   = 12.0f;
    const float barGap = 4.0f;

    float totalBarsW = barW * 2.0f + barGap;
    float sideContent = bounds.getWidth() - peakW - scaleW;
    float sidePad = (sideContent - totalBarsW) * 0.5f;
    if (sidePad < 0.0f) sidePad = 0.0f;

    float x0 = bounds.getX() + peakW + sidePad;

    auto lBarBounds = juce::Rectangle<float>(x0, bounds.getY(), barW, bounds.getHeight());
    auto rBarBounds = juce::Rectangle<float>(x0 + barW + barGap, bounds.getY(), barW, bounds.getHeight());
    auto barAreaBounds = lBarBounds.withRight(rBarBounds.getRight());

    auto peakZone  = bounds.withWidth(peakW);
    auto scaleZone = bounds.withTrimmedLeft(bounds.getWidth() - scaleW);

    const float radius = 0.8f;

    // ─── Fondo unificado para ambas barras ────────────────────────────
    g.setColour(juce::Colour(kBgBar));
    g.fillRoundedRectangle(barAreaBounds, radius);

    // ─── Grid lines (continuas a lo ancho de barArea) ────────────────
    g.setColour(juce::Colour(kGrid).withAlpha(0.45f));
    for (float db : { -18.0f, -12.0f, -6.0f, 0.0f }) {
        float y = levelToY(db, lBarBounds);
        g.drawHorizontalLine((int)y, barAreaBounds.getX() + 1.0f, barAreaBounds.getRight() - 1.0f);
    }

    // ─── Barras L y R ──────────────────────────────────────────────────
    drawBar(g, lBarBounds, leftLevel_);
    drawBar(g, rBarBounds, rightLevel_);

    // ─── Peak triangles L y R en peakZone compartido ───────────────────
    // L peak (azul, arriba)
    if (leftPeak_ > -60.0f) {
        float peakY = levelToY(leftPeak_, lBarBounds);
        peakY = juce::jlimit(lBarBounds.getY(), lBarBounds.getBottom(), peakY);

        juce::Path tri;
        tri.addTriangle(peakZone.getX(), peakY,
                        peakZone.getRight(), peakY - 3.0f,
                        peakZone.getRight(), peakY + 3.0f);
        g.setColour(barColourL_.withAlpha(0.85f));
        g.fillPath(tri);
    }

    // R peak (verde, ligeramente offset para visibilidad)
    if (rightPeak_ > -60.0f) {
        float peakY = levelToY(rightPeak_, rBarBounds);
        peakY = juce::jlimit(rBarBounds.getY(), rBarBounds.getBottom(), peakY);

        juce::Path tri;
        tri.addTriangle(peakZone.getX() + 1.0f, peakY + 0.5f,
                        peakZone.getRight() - 0.5f, peakY - 2.0f,
                        peakZone.getRight() - 0.5f, peakY + 3.0f);
        g.setColour(barColourR_.withAlpha(0.85f));
        g.fillPath(tri);
    }

    // ─── Escala compartida a la derecha ──────────────────────────────
    g.setFont(juce::Font(juce::FontOptions(6.0f)));
    g.setColour(juce::Colour(kScaleText).withAlpha(0.7f));
    for (int i = 0; i < kNumScaleMarks; ++i) {
        float y = levelToY(kScaleValues[i], lBarBounds);
        g.drawText(juce::String((int)kScaleValues[i]),
                   juce::Rectangle<float>(scaleZone.getX(), y - 4.0f, scaleZone.getWidth(), 8.0f),
                   juce::Justification::centredRight);
    }

    // ─── Clip indicator overlay ────────────────────────────────────────
    if (clipActive_) {
        // Overlay rojo semitransparente sobre toda el area de barras
        g.setColour(juce::Colour(kRed).withAlpha(0.20f));
        g.fillRoundedRectangle(barAreaBounds, radius);

        // Label "CLIP" pulsante (alpha varia con el timer)
        float pulseAlpha = 0.5f + 0.5f * std::sin(clipHoldTimer_ * 0.3f);
        g.setColour(juce::Colour(kRed).withAlpha(pulseAlpha));
        g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
        g.drawText("CLIP",
                   juce::Rectangle<float>(barAreaBounds.getX(), barAreaBounds.getY() + 2.0f,
                                          barAreaBounds.getWidth(), 12.0f),
                   juce::Justification::centred);
    }

    // ─── Borde unificado ─────────────────────────────────────────────
    g.setColour(juce::Colour(kBorder).withAlpha(0.5f));
    g.drawRoundedRectangle(barAreaBounds, radius, 1.0f);
}

// ─── Dibuja una barra individual con gradiente + shine ─────────────────────

void StereoMeter::drawBar(juce::Graphics& g, juce::Rectangle<float> barBounds, float level)
{
    const float radius = 0.8f;

    float norm = juce::jlimit(0.0f, 1.0f, (level + 60.0f) / 66.0f);
    if (norm > 0.005f) {
        float fillTop = barBounds.getBottom() - norm * barBounds.getHeight();
        auto fillRect = barBounds.withTop(fillTop);

        // ─── Gradiente multi-stop 5 colores (visual_design exacto) ─
        //  Top (rojo, clipping) → naranja → amarillo → lima → verde (seguro)
        juce::ColourGradient grad(
            juce::Colour(kRed),
            juce::Point<float>(fillRect.getCentreX(), fillRect.getY()),
            juce::Colour(kGreen),
            juce::Point<float>(fillRect.getCentreX(), fillRect.getBottom()),
            false);
        grad.addColour(0.25f, juce::Colour(kOrange));
        grad.addColour(0.50f, juce::Colour(kYellow));
        grad.addColour(0.75f, juce::Colour(kLime));
        g.setGradientFill(grad);
        g.fillRoundedRectangle(fillRect, radius);

        // Shine en parte superior del fill
        auto shine = fillRect.withHeight(juce::jmax(1.5f, fillRect.getHeight() * 0.08f));
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.fillRoundedRectangle(shine, radius);
    }
}

// ─── Convierte dB a posición Y ───────────────────────────────────────────

float StereoMeter::levelToY(float levelDb, juce::Rectangle<float> bounds) const
{
    float norm = juce::jlimit(0.0f, 1.0f, (levelDb + 60.0f) / 66.0f);
    return bounds.getBottom() - norm * bounds.getHeight();
}

} // namespace mixcoach
