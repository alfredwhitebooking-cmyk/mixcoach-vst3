#include "VectorscopeComponent.h"

namespace mixcoach {

VectorscopeComponent::VectorscopeComponent()
{
    setOpaque(true);
    titleLabel_.setText(juce::CharPointer_UTF8("\xE2\xAD\x90 Vectorscope"),
                        juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(titleLabel_);

    corrLabel_.setText("\xCF\x86: +1.00", juce::dontSendNotification);
    corrLabel_.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
    corrLabel_.setJustificationType(juce::Justification::centred);
    corrLabel_.setColour(juce::Label::textColourId, MixCoachTheme::success());
    addAndMakeVisible(corrLabel_);
}

void VectorscopeComponent::resized()
{
    auto area = getLocalBounds().reduced(2);
    titleLabel_.setBounds(area.removeFromTop(14));
    corrLabel_.setBounds(area.removeFromBottom(14));
    gridCacheValid_ = false;
}

juce::Rectangle<float> VectorscopeComponent::plotCircleArea() const
{
    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(16);
    area.removeFromBottom(16);

    const int squareSize = juce::jmin(area.getWidth(), area.getHeight()) - 4;
    return juce::Rectangle<float>(0.0f, 0.0f, (float) squareSize, (float) squareSize)
        .withCentre(area.toFloat().getCentre());
}

void VectorscopeComponent::rebuildGridCache()
{
    const auto circleArea = plotCircleArea();
    if (circleArea.isEmpty())
    {
        gridCacheValid_ = false;
        return;
    }

    const auto bounds = getLocalBounds();
    gridCache_ = juce::Image(juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
    gridCache_.clear(gridCache_.getBounds());

    juce::Graphics cg(gridCache_);
    MixCoachTheme::fillGlassPanel(cg, bounds.toFloat(), 6.0f);

    cg.setColour(MixCoachTheme::bgDarker().withAlpha(0.6f));
    cg.fillEllipse(circleArea.reduced(2.0f));
    drawGrid(cg, circleArea);
    cg.setColour(MixCoachTheme::border().withAlpha(0.25f));
    cg.drawEllipse(circleArea.reduced(2.0f), 1.0f);

    gridCacheValid_ = true;
}

void VectorscopeComponent::pushSample(float left, float right)
{
    auto& pt = trace_[writePos_ % kTraceLen];
    pt.x = juce::jlimit(-1.0f, 1.0f, left);
    pt.y = juce::jlimit(-1.0f, 1.0f, right);
    pt.alpha = 1.0f;
    writePos_ = (writePos_ + 1) % kTraceLen;
}

void VectorscopeComponent::setDisplayCorrelation(float correlation)
{
    const float corr = juce::jlimit(-1.0f, 1.0f, correlation);
    if (std::abs(corr - lastCorrDisplayed_) < 0.02f)
        return;

    lastCorrDisplayed_ = corr;

    juce::Colour corrColour;
    if (std::abs(corr) < 0.3f)
        corrColour = MixCoachTheme::error();
    else if (corr < 0.0f)
        corrColour = MixCoachTheme::warning();
    else
        corrColour = MixCoachTheme::success();

    corrLabel_.setText("\xCF\x86: " + juce::String(corr, 2), juce::dontSendNotification);
    corrLabel_.setColour(juce::Label::textColourId, corrColour);
}

bool VectorscopeComponent::advanceFrame(double /*sampleRateHz*/, bool allowRepaint)
{
    bool hasTrace = false;
    for (auto& p : trace_)
    {
        if (p.alpha > 0.01f)
            hasTrace = true;
        p.alpha *= 0.965f;
    }

    if (hasTrace && allowRepaint)
        repaint();

    return hasTrace;
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawGrid — Círculos concéntricos, crosshairs, labels
// ═══════════════════════════════════════════════════════════════════════════
void VectorscopeComponent::drawGrid(juce::Graphics& g, juce::Rectangle<float> area)
{
    auto cx = area.getCentreX();
    auto cy = area.getCentreY();
    float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f - 4.0f;

    // ─── Círculos concéntricos ────────────────────────────────────────
    // 4 anillos: 25%, 50%, 75%, 100%
    float ringAlphas[] = { 0.08f, 0.10f, 0.12f, 0.25f };
    float ringRadii[]  = { 0.25f, 0.50f, 0.75f, 1.0f };

    for (int ri = 0; ri < 4; ++ri) {
        float r = radius * ringRadii[ri];
        auto ringBounds = juce::Rectangle<float>(cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour(MixCoachTheme::border().withAlpha(ringAlphas[ri]));
        g.drawEllipse(ringBounds, (ri == 3) ? 1.0f : 0.5f);
    }

    // ─── Crosshairs (horizontal y vertical) ────────────────────────────
    g.setColour(MixCoachTheme::border().withAlpha(0.15f));
    g.drawHorizontalLine((int)cy, area.getX() + 2, area.getRight() - 2);
    g.drawVerticalLine((int)cx, area.getY() + 2, area.getBottom() - 2);

    // ─── Diagonales (45°) ──────────────────────────────────────────────
    g.setColour(MixCoachTheme::border().withAlpha(0.08f));
    float d = radius * 0.707f;
    g.drawLine(cx - d, cy - d, cx + d, cy + d, 0.5f);
    g.drawLine(cx - d, cy + d, cx + d, cy - d, 0.5f);

    // ─── Center dot ────────────────────────────────────────────────────
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.2f));
    g.fillEllipse(cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);

    // ─── Labels en los ejes: M (top), L (left), R (right), S (bottom) ──
    g.setFont(juce::Font(juce::FontOptions(6.5f)).boldened());
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.35f));

    g.drawText("M", juce::Rectangle<float>(cx - 6, area.getY() + 2, 10, 10),
               juce::Justification::centred);
    g.drawText("L", juce::Rectangle<float>(area.getX() + 2, cy - 6, 10, 10),
               juce::Justification::centredLeft);
    g.drawText("R", juce::Rectangle<float>(area.getRight() - 12, cy - 6, 10, 10),
               juce::Justification::centredRight);
    g.drawText("S", juce::Rectangle<float>(cx - 6, area.getBottom() - 12, 10, 10),
               juce::Justification::centred);
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawTrace — Trazado anti-aliased con phosphor trail
//  Usa juce::Path con líneas para un trazado suave continuo
// ═══════════════════════════════════════════════════════════════════════════
void VectorscopeComponent::drawTrace(juce::Graphics& g, juce::Rectangle<float> circleArea)
{
    auto cx = circleArea.getCentreX();
    auto cy = circleArea.getCentreY();
    float radius = juce::jmin(circleArea.getWidth(), circleArea.getHeight()) * 0.5f - 4.0f;

    // ─── Phosphor trail: dibujar el trazado en múltiples capas ─────────
    // Los puntos más nuevos (alpha alto) se dibujan encima con color brillante
    // Los puntos más viejos (alpha bajo) se dibujan debajo con color tenue
    
    // Pasada 1: puntos viejos (phosphor glow, dibujados primero/debajo)
    juce::Path oldPath;
    bool oldStarted = false;
    for (int i = 0; i < kTraceLen; ++i) {
        int idx = (writePos_ + i) % kTraceLen;
        auto& pt = trace_[idx];
        if (pt.alpha < 0.01f) continue;
        if (pt.alpha > 0.5f) continue; // Los nuevos se dibujan en la pasada 2

        float sx = cx + pt.x * radius;
        float sy = cy + pt.y * radius;

        if (!oldStarted) {
            oldPath.startNewSubPath(sx, sy);
            oldStarted = true;
        } else {
            oldPath.lineTo(sx, sy);
        }
    }
    if (oldStarted) {
        g.setColour(juce::Colour(0xFF8B5CF6).withAlpha(0.15f));
        g.strokePath(oldPath, juce::PathStrokeType(2.5f));
    }

    // Pasada 2: puntos nuevos (trazo brillante, dibujados encima)
    juce::Path newPath;
    bool newStarted = false;
    for (int i = 0; i < kTraceLen; ++i) {
        int idx = (writePos_ + i) % kTraceLen;
        auto& pt = trace_[idx];
        if (pt.alpha < 0.3f) continue;

        float sx = cx + pt.x * radius;
        float sy = cy + pt.y * radius;

        if (!newStarted) {
            newPath.startNewSubPath(sx, sy);
            newStarted = true;
        } else {
            newPath.lineTo(sx, sy);
        }
    }
    if (newStarted) {
        // Color del trazo: violeta #8B5CF6 con glow
        g.setColour(MixCoachTheme::accent().withAlpha(0.7f));
        g.strokePath(newPath, juce::PathStrokeType(1.8f));

        // Glow exterior del trazo
        g.setColour(MixCoachTheme::accent().withAlpha(0.12f));
        g.strokePath(newPath, juce::PathStrokeType(4.0f));
    }

    // Pasada 3: puntos individuales (dots brillantes en las puntas)
    for (int i = 0; i < kTraceLen; ++i) {
        int idx = (writePos_ + i) % kTraceLen;
        auto& pt = trace_[idx];
        if (pt.alpha < 0.01f) continue;

        float sx = cx + pt.x * radius;
        float sy = cy + pt.y * radius;

        float dotSize = 1.5f + pt.alpha * 1.5f;
        g.setColour(MixCoachTheme::accentGlow().withAlpha(pt.alpha * 0.5f));
        g.fillEllipse(sx - dotSize * 0.5f, sy - dotSize * 0.5f, dotSize, dotSize);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  paint
// ═══════════════════════════════════════════════════════════════════════════
void VectorscopeComponent::paint(juce::Graphics& g)
{
    if (! gridCacheValid_)
        rebuildGridCache();

    if (gridCacheValid_)
        g.drawImageAt(gridCache_, 0, 0);
    else
        MixCoachTheme::fillGlassPanel(g, getLocalBounds().toFloat(), 6.0f);

    drawTrace(g, plotCircleArea());
}

} // namespace mixcoach
