#include "AnalogVUMeter.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  Colores fijos del estilo vintage VU
//  (usamos constexpr uint32_t para evitar MSVC "most vexing parse")
// ═══════════════════════════════════════════════════════════════════════════
constexpr uint32_t kBezelFrame      = 0xFF1A1A1A;
constexpr uint32_t kBezelHighlight  = 0xFF2A2A2A;
constexpr uint32_t kWoodBase        = 0xFFC4A35A;
constexpr uint32_t kWoodDark        = 0xFFA6843E;
constexpr uint32_t kWoodEdge        = 0xFF8B6F32;
constexpr uint32_t kNeedleDark      = 0xFF2D2D2D;
constexpr uint32_t kNeedleLight     = 0xFF4A4A4A;
constexpr uint32_t kScaleText       = 0xFF1A1A1A;
constexpr uint32_t kScaleTick       = 0xFF1A1A1A;
constexpr uint32_t kRedZone         = 0xFFEF4444;
constexpr uint32_t kLabelVu         = 0xFF4A3A20;

AnalogVUMeter::AnalogVUMeter()
{
    setOpaque(true);
    smoothedVu_.reset(kVuMin);
    peakHoldVu_.reset(kVuMin);
    peakHoldLevel_ = kVuMin;
}

// ═══════════════════════════════════════════════════════════════════════════
//  Conversiones
// ═══════════════════════════════════════════════════════════════════════════
float AnalogVUMeter::dbFsToVu(float dbFs) const
{
    return juce::jlimit(kVuMin, kVuMax, dbFs - kDbFsRef);
}

float AnalogVUMeter::valueToAngle(float vu) const
{
    float norm = (vu - kVuMin) / (kVuMax - kVuMin);
    norm = juce::jlimit(0.0f, 1.0f, norm);
    return kArcStartAngle + norm * kArcRange;
}


// ═══════════════════════════════════════════════════════════════════════════
//  setLevel — Recibe dBFS, convierte a VU, actualiza smoothed + peak hold
// ═══════════════════════════════════════════════════════════════════════════
void AnalogVUMeter::setLevel(float levelDbFs)
{
    const float vu = dbFsToVu(levelDbFs);
    smoothedVu_.setTargetValue(vu);

    uint32_t now = juce::Time::getMillisecondCounter();
    const float elapsedSincePeak = (now - peakHoldTimeMs_) * 0.001f;

    if (vu > peakHoldLevel_)
    {
        peakHoldLevel_ = vu;
        peakHoldTimeMs_ = now;
    }
    else if (elapsedSincePeak > 0.5f)
    {
        const float decay = 6.0f * (elapsedSincePeak - 0.5f);
        peakHoldLevel_ = juce::jmax(kVuMin, vu, peakHoldLevel_ - decay);
    }

    peakHoldVu_.setTargetValue(peakHoldLevel_);
}

bool AnalogVUMeter::advanceFrame(double sampleRateHz, bool allowRepaint)
{
    const bool dirty = smoothedVu_.advance(sampleRateHz) | peakHoldVu_.advance(sampleRateHz);
    if (dirty && allowRepaint)
        repaint();
    return dirty;
}

// ═══════════════════════════════════════════════════════════════════════════
//  resized
// ═══════════════════════════════════════════════════════════════════════════
void AnalogVUMeter::resized()
{
    faceCacheValid_ = false;
}

void AnalogVUMeter::rebuildFaceCache()
{
    const auto bounds = getLocalBounds();
    if (bounds.isEmpty())
    {
        faceCacheValid_ = false;
        return;
    }

    faceCache_ = juce::Image(juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
    faceCache_.clear(faceCache_.getBounds());

    juce::Graphics cg(faceCache_);
    paintStaticFace(cg, bounds.toFloat());
    faceCacheValid_ = true;
}

void AnalogVUMeter::paintStaticFace(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    float h = bounds.getHeight();
    auto labelArea = bounds.removeFromTop(juce::jmin(14.0f, h * 0.13f));
    bounds.removeFromBottom(juce::jmin(14.0f, h * 0.13f));
    auto faceBounds = bounds;

    drawBezel(g, faceBounds);
    drawWoodBackground(g, faceBounds);
    drawScale(g, faceBounds);
    drawVuLabel(g, faceBounds);

    if (! labelText_.isEmpty())
    {
        g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
        if (labelColourOverride_ != juce::Colour(0x00000000))
            g.setColour(labelColourOverride_);
        else
            g.setColour(MixCoachTheme::textDim());
        g.drawText(labelText_, labelArea.reduced(2, 0), juce::Justification::centred);
    }
}

void AnalogVUMeter::paintNeedleAndReadout(juce::Graphics& g, juce::Rectangle<float> faceBounds,
                                            juce::Rectangle<float> valArea) const
{
    const float currentVu = smoothedVu_.getCurrent();
    const float needleAngle = valueToAngle(currentVu);
    drawNeedle(g, faceBounds, needleAngle);

    const float peakVu = peakHoldVu_.getCurrent();
    if (peakVu > kVuMin + 1.0f)
    {
        const float pkAngle = valueToAngle(peakVu);
        auto innerFace = faceBounds.reduced(3.0f, 3.0f);
        const float cx = innerFace.getCentreX();
        const float pivotY = innerFace.getBottom() - 8.0f;
        const float scaleRadius = (innerFace.getHeight() - 8.0f) * 0.78f;
        const float markerR = scaleRadius * 0.88f;

        const float mx = cx + std::cos(pkAngle) * markerR;
        const float my = pivotY + std::sin(pkAngle) * markerR;

        const float perpAngle = pkAngle + juce::MathConstants<float>::halfPi;
        const float triSize = 4.0f;
        const float tx1 = mx + std::cos(perpAngle) * triSize;
        const float ty1 = my + std::sin(perpAngle) * triSize;
        const float tx2 = mx - std::cos(perpAngle) * triSize;
        const float ty2 = my - std::sin(perpAngle) * triSize;
        const float tx3 = mx - std::cos(pkAngle) * 3.0f;
        const float ty3 = my - std::sin(pkAngle) * 3.0f;

        juce::Path pkTri;
        pkTri.addTriangle(tx1, ty1, tx2, ty2, tx3, ty3);
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.fillPath(pkTri);
    }

    const float currentDb = currentVu + kDbFsRef;
    if (valArea.getHeight() <= 2.0f)
        return;

    juce::String valStr;
    if (currentDb < -50.0f)
        valStr = "--.-";
    else
        valStr = juce::String(currentDb, 1);

    juce::Colour valColour;
    if (currentVu > 0.0f)
        valColour = juce::Colour(kRedZone).withAlpha(0.9f);
    else if (currentVu > -6.0f)
        valColour = MixCoachTheme::meterOrange().withAlpha(0.8f);
    else if (currentVu > -12.0f)
        valColour = MixCoachTheme::meterYellow().withAlpha(0.7f);
    else
        valColour = MixCoachTheme::textDim();

    g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
    g.setColour(valColour);
    g.drawText(valStr, valArea, juce::Justification::centred);
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawBezel — Marco oscuro tipo bisel con inner shadow
// ═══════════════════════════════════════════════════════════════════════════
void AnalogVUMeter::drawBezel(juce::Graphics& g, juce::Rectangle<float> faceBounds) const
{
    // ─── Shadow exterior (sombra del meter sobre el fondo) ──────────
    auto shadowBounds = faceBounds.expanded(3.0f, 3.0f);
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillRoundedRectangle(shadowBounds, 5.0f);

    // ─── Cuerpo del bisel (multi-capa para efecto 3D) ──────────────
    // Capa exterior (oscura)
    auto outerBevel = faceBounds.expanded(1.5f, 1.5f);
    g.setColour(juce::Colour(kBezelHighlight));
    g.fillRoundedRectangle(outerBevel, 5.0f);

    // Capa interior (bisel principal)
    g.setColour(juce::Colour(kBezelFrame));
    g.fillRoundedRectangle(faceBounds, 4.0f);

    // ─── Inner shadow (oscuridad en el interior del bisel) ─────────
    juce::ColourGradient innerShadow(
        juce::Colours::black.withAlpha(0.25f),
        juce::Point<float>(faceBounds.getCentreX(), faceBounds.getY()),
        juce::Colours::transparentBlack,
        juce::Point<float>(faceBounds.getCentreX(), faceBounds.getY() + 10.0f),
        false);
    g.setGradientFill(innerShadow);
    g.fillRoundedRectangle(faceBounds, 4.0f);

    // ─── Separación sutil entre bisel y la cara del meter ──────────
    auto innerFace = faceBounds.reduced(3.0f, 3.0f);
    g.setColour(juce::Colour(kBezelFrame).withAlpha(0.5f));
    g.drawRoundedRectangle(innerFace, 3.0f, 0.5f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawWoodBackground — Fondo madera/beige envejecido
//  Simula madera clara vintage con gradientes y vetas sutiles
// ═══════════════════════════════════════════════════════════════════════════
void AnalogVUMeter::drawWoodBackground(juce::Graphics& g, juce::Rectangle<float> faceBounds) const
{
    auto innerFace = faceBounds.reduced(3.0f, 3.0f);
    juce::Point<float> centre = innerFace.getCentre();

    // ─── Base color madera ──────────────────────────────────────────
    g.setColour(juce::Colour(kWoodBase));
    g.fillRoundedRectangle(innerFace, 3.0f);

    // ─── Vignette radial (oscurece bordes) ──────────────────────────
    juce::ColourGradient vignette(
        juce::Colours::transparentBlack,
        centre,
        juce::Colour(kWoodEdge).withAlpha(0.35f),
        juce::Point<float>(innerFace.getX(), innerFace.getY()),
        true);
    g.setGradientFill(vignette);
    g.fillRoundedRectangle(innerFace, 3.0f);

    // ─── Vetas de madera sutiles (líneas horizontales) ──────────────
    g.setColour(juce::Colour(kWoodDark).withAlpha(0.08f));
    float grainY = innerFace.getY() + 8.0f;
    while (grainY < innerFace.getBottom()) {
        float alpha = 0.04f + 0.04f * std::sin(grainY * 0.3f);
        g.setColour(juce::Colour(kWoodDark).withAlpha(alpha));
        g.drawHorizontalLine((int)grainY, innerFace.getX() + 4.0f, innerFace.getRight() - 4.0f);
        grainY += 4.0f + 2.0f * std::abs(std::sin(grainY * 0.17f));
    }

    // ─── Brillo superior sutil ──────────────────────────────────────
    juce::ColourGradient topGlow(
        juce::Colours::white.withAlpha(0.06f),
        juce::Point<float>(0.0f, innerFace.getY()),
        juce::Colours::transparentBlack,
        juce::Point<float>(0.0f, innerFace.getY() + innerFace.getHeight() * 0.3f),
        false);
    g.setGradientFill(topGlow);
    g.fillRoundedRectangle(innerFace, 3.0f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawScale — Escala impresa del VU meter
//  Marcas: -20, -10, -7, -5, -3, -2, -1, 0, +1, +2, +3
//  Zona roja: +1 a +3
// ═══════════════════════════════════════════════════════════════════════════
void AnalogVUMeter::drawScale(juce::Graphics& g, juce::Rectangle<float> faceBounds) const
{
    auto innerFace = faceBounds.reduced(3.0f, 3.0f);
    float cx = innerFace.getCentreX();
    float pivotY = innerFace.getBottom() - 8.0f;  // moved up from -4 for better centering

    // ─── Radio del arco de escala (reducido de 0.88 a 0.78 para proporción) ──
    float scaleRadius = (innerFace.getHeight() - 8.0f) * 0.78f;

    // ─── Marcas de la escala ───────────────────────────────────────
    struct ScaleMark {
        float vu;
        const char* label;
        bool isMajor;
        bool isRed;
    };

    ScaleMark marks[] = {
        { -20.0f, "-20", true,  false },
        { -10.0f, "-10", true,  false },
        { -7.0f,  "-7",  false, false },
        { -5.0f,  "-5",  true,  false },
        { -3.0f,  "-3",  false, false },
        { -2.0f,  "-2",  false, false },
        { -1.0f,  "-1",  false, false },
        {  0.0f,   "0",  true,  false },
        {  1.0f,   "1",  false, true  },
        {  2.0f,   "2",  false, true  },
        {  3.0f,   "3",  true,  true  },
    };

    for (auto& mark : marks) {
        float angle = valueToAngle(mark.vu);

        // ─── Tick line (proporciones ajustadas) ────────────────────
        float tickInnerR = scaleRadius * 0.78f;
        float tickOuterR = scaleRadius * 0.95f;
        float tickWidth  = mark.isMajor ? 1.2f : 0.7f;

        float x1 = cx + std::cos(angle) * tickInnerR;
        float y1 = pivotY + std::sin(angle) * tickInnerR;
        float x2 = cx + std::cos(angle) * tickOuterR;
        float y2 = pivotY + std::sin(angle) * tickOuterR;

        if (mark.isRed)
            g.setColour(juce::Colour(kRedZone));
        else
            g.setColour(juce::Colour(kScaleTick).withAlpha(mark.isMajor ? 0.9f : 0.55f));

        g.drawLine(x1, y1, x2, y2, tickWidth);

        // ─── Label numérico (más cerca de la escala) ─────────────
        float labelR = scaleRadius * 1.02f;
        float lx = cx + std::cos(angle) * labelR;
        float ly = pivotY + std::sin(angle) * labelR;

        float labelW = 14.0f;
        float labelH = 8.0f;

        // Ajustar posición horizontal para marcas extremas
        if (mark.vu < -15.0f)  lx -= 4.0f;
        if (mark.vu >  2.0f)   lx += 4.0f;

        g.setFont(juce::Font(juce::FontOptions(6.5f)).boldened());
        if (mark.isRed)
            g.setColour(juce::Colour(kRedZone));
        else
            g.setColour(juce::Colour(kScaleTick));

        if (mark.isMajor) {
            g.drawText(juce::String(mark.label),
                       juce::Rectangle<float>(lx - labelW * 0.5f, ly - labelH * 0.5f, labelW, labelH),
                       juce::Justification::centred);
        }
    }

    // ─── Arco decorativo exterior (sutil) ──────────────────────────
    {
        juce::Path arcPath;
        arcPath.addArc(cx - scaleRadius, pivotY - scaleRadius,
                       scaleRadius * 2, scaleRadius * 2,
                       kArcStartAngle, kArcEndAngle, true);
        g.setColour(juce::Colour(kScaleTick).withAlpha(0.12f));
        g.strokePath(arcPath, juce::PathStrokeType(0.5f));
    }

    // ─── Línea guía central (decorativa) ──────────────────────────
    {
        float zeroAngle = valueToAngle(0.0f);
        float zeroR = scaleRadius * 0.88f;
        float zx = cx + std::cos(zeroAngle) * zeroR;
        float zy = pivotY + std::sin(zeroAngle) * zeroR;
        g.setColour(juce::Colour(kScaleTick).withAlpha(0.08f));
        g.drawLine(cx, pivotY, zx, zy, 0.4f);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawNeedle — Aguja oscura con efecto 3D
// ═══════════════════════════════════════════════════════════════════════════
void AnalogVUMeter::drawNeedle(juce::Graphics& g, juce::Rectangle<float> faceBounds,
                                float angle) const
{
    auto innerFace = faceBounds.reduced(3.0f, 3.0f);
    float cx = innerFace.getCentreX();
    float pivotY = innerFace.getBottom() - 8.0f;  // moved up

    float scaleRadius = (innerFace.getHeight() - 8.0f) * 0.78f;
    float needleLength = scaleRadius * 0.78f;

    // ─── Puntas de la aguja ──────────────────────────────────────────
    float tipX = cx + std::cos(angle) * needleLength;
    float tipY = pivotY + std::sin(angle) * needleLength;

    // Cola de la aguja (detrás del pivote, más corta)
    float tailLen = scaleRadius * 0.12f;
    float tailAngle = angle + juce::MathConstants<float>::pi;
    float tailX = cx + std::cos(tailAngle) * tailLen;
    float tailY = pivotY + std::sin(tailAngle) * tailLen;

    // ─── Sombra de la aguja ──────────────────────────────────────────
    {
        juce::Path shadowPath;
        shadowPath.startNewSubPath(tailX + 1.0f, tailY + 1.0f);

        float perpAngle = angle + juce::MathConstants<float>::halfPi;
        float perpLen = 1.8f;

        float sx1 = cx + std::cos(perpAngle) * perpLen + 1.0f;
        float sy1 = pivotY + std::sin(perpAngle) * perpLen + 1.0f;
        float sx2 = cx + std::cos(perpAngle + juce::MathConstants<float>::pi) * perpLen + 1.0f;
        float sy2 = pivotY + std::sin(perpAngle + juce::MathConstants<float>::pi) * perpLen + 1.0f;

        shadowPath.lineTo(sx1, sy1);
        shadowPath.lineTo(tipX + 1.5f, tipY + 1.5f);
        shadowPath.lineTo(sx2, sy2);
        shadowPath.closeSubPath();

        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillPath(shadowPath);
    }

    // ─── Cuerpo principal de la aguja (oscuro 3D) ────────────────────
    {
        juce::Path needlePath;
        needlePath.startNewSubPath(tailX, tailY);

        float perpAngle = angle + juce::MathConstants<float>::halfPi;
        float perpLen = 1.8f;
        float tipWidth = 0.3f;

        float nx1 = cx + std::cos(perpAngle) * perpLen;
        float ny1 = pivotY + std::sin(perpAngle) * perpLen;

        float nx2 = cx + std::cos(perpAngle + juce::MathConstants<float>::pi) * perpLen;
        float ny2 = pivotY + std::sin(perpAngle + juce::MathConstants<float>::pi) * perpLen;

        needlePath.lineTo(nx1, ny1);
        needlePath.lineTo(tipX + std::cos(perpAngle) * tipWidth,
                          tipY + std::sin(perpAngle) * tipWidth);
        needlePath.lineTo(tipX, tipY);
        needlePath.lineTo(tipX - std::cos(perpAngle) * tipWidth,
                          tipY - std::sin(perpAngle) * tipWidth);
        needlePath.lineTo(nx2, ny2);
        needlePath.closeSubPath();

        // Gradiente de la aguja (más claro en la base)
        juce::ColourGradient needleGrad(
            juce::Colour(kNeedleLight),
            juce::Point<float>(cx, pivotY),
            juce::Colour(kNeedleDark),
            juce::Point<float>(tipX, tipY),
            false);
        g.setGradientFill(needleGrad);
        g.fillPath(needlePath);

        // Highlight lateral sutil
        g.setColour(juce::Colour(kNeedleLight).withAlpha(0.3f));
        g.strokePath(needlePath, juce::PathStrokeType(0.3f));
    }

    // ─── Pivot dot (centro de rotación) ────────────────────────────
    {
        // Círculo exterior
        g.setColour(juce::Colour(kBezelFrame));
        g.fillEllipse(cx - 4.0f, pivotY - 4.0f, 8.0f, 8.0f);

        // Anillo metálico
        g.setColour(juce::Colour(kNeedleDark));
        g.drawEllipse(cx - 3.5f, pivotY - 3.5f, 7.0f, 7.0f, 0.5f);

        // Centro
        g.setColour(juce::Colour(kNeedleLight));
        g.fillEllipse(cx - 2.0f, pivotY - 2.0f, 4.0f, 4.0f);

        // Highlight
        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.fillEllipse(cx - 1.0f, pivotY - 1.0f, 2.0f, 2.0f);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawVuLabel — Label "VU" central en la cara del meter
// ═══════════════════════════════════════════════════════════════════════════
void AnalogVUMeter::drawVuLabel(juce::Graphics& g, juce::Rectangle<float> faceBounds) const
{
    auto innerFace = faceBounds.reduced(3.0f, 3.0f);
    float cx = innerFace.getCentreX();
    float pivotY = innerFace.getBottom() - 8.0f;
    float scaleRadius = (innerFace.getHeight() - 8.0f) * 0.78f;

    // Label "VU" justo debajo del centro del arco
    float labelY = pivotY - scaleRadius * 0.35f;

    g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
    g.setColour(juce::Colour(kLabelVu).withAlpha(0.7f));
    g.drawText("VU",
               juce::Rectangle<float>(cx - 10.0f, labelY - 5.0f, 20.0f, 10.0f),
               juce::Justification::centred);
}

// ═══════════════════════════════════════════════════════════════════════════
//  paint — Dibuja el VU meter analógico vintage completo
// ═══════════════════════════════════════════════════════════════════════════
void AnalogVUMeter::paint(juce::Graphics& g)
{
    if (! faceCacheValid_)
        rebuildFaceCache();

    if (faceCacheValid_)
        g.drawImageAt(faceCache_, 0, 0);

    auto bounds = getLocalBounds().toFloat();
    const float h = bounds.getHeight();
    bounds.removeFromTop(juce::jmin(14.0f, h * 0.13f));
    auto valArea = bounds.removeFromBottom(juce::jmin(14.0f, h * 0.13f)).reduced(2, 0);
    paintNeedleAndReadout(g, bounds, valArea);
}

} // namespace mixcoach
