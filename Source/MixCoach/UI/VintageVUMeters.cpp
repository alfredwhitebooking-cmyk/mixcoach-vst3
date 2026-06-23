#include "VintageVUMeters.h"
#include "AnalyzersPanelDrawing.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace mixcoach {

VintageVUMeters::VintageVUMeters() {}

void VintageVUMeters::resized()
{
    cacheValid_ = false;
    panelValid_ = false;
}

void VintageVUMeters::setLevels(float l, float r, float m, float s)
{
    float levels[] = { l, r, m, s };
    for (int i = 0; i < 4; ++i) {
        const float vu = levels[i] - kVuRefDbfs;
        channels_[i].level.setTargetValue(vu);
        if (vu > channels_[i].peakHold) {
            channels_[i].peakHold = vu;
            channels_[i].peakHoldTimer = 0.0f;
        }
    }
}

bool VintageVUMeters::advanceVisuals(double sr, bool allowRepaint)
{
    bool dirty = false;
    for (auto& ch : channels_) {
        dirty |= ch.level.advance(sr);

        if (ch.peakHold > -20.0f) {
            ch.peakHoldTimer += 1.0f / (float)sr;
            if (ch.peakHoldTimer > kPeakHoldSec) {
                float old = ch.peakHold;
                ch.peakHold -= kPeakDecay_dBps / (float)sr;
                float floor = juce::jmax(ch.level.getCurrent(), -20.0f);
                if (ch.peakHold < floor) ch.peakHold = floor;
                if (std::abs(ch.peakHold - old) > 0.01f) dirty = true;
            }
        }
    }
    if (dirty && allowRepaint) repaint();
    return dirty;
}

float VintageVUMeters::vuDbToNorm(float db) const noexcept
{
    db = juce::jlimit(-20.0f, 3.0f, db);
    for (int i = 1; i < 11; ++i) {
        if (db <= vuScale_[i].db) {
            float t = (db - vuScale_[i - 1].db) / (vuScale_[i].db - vuScale_[i - 1].db);
            return vuScale_[i - 1].pct + t * (vuScale_[i].pct - vuScale_[i - 1].pct);
        }
    }
    return 1.0f;
}

void VintageVUMeters::drawStaticDialFace(juce::Graphics& g, juce::Rectangle<float> bounds,
                                         float cx, float cy, float radius)
{
    scaleArcCx_ = cx;
    scaleArcCy_ = cy;
    scaleArcR_  = radius;

    // ── 1. Fondo ámbar retroiluminado ──────────────────────────────────────
    {
        const float glowCy = cy - radius * 0.50f;
        const float glowR  = radius * 1.30f;

        juce::ColourGradient amber(
            juce::Colour(0xFFFFF3C8), cx, glowCy,
            juce::Colour(0xFF8A5E1C), cx, glowCy + glowR,
            true);
        amber.addColour(0.20f, juce::Colour(0xFFF7DC6F));
        amber.addColour(0.50f, juce::Colour(0xFFE5B853));
        amber.addColour(0.78f, juce::Colour(0xFFC28A30));
        amber.addColour(0.92f, juce::Colour(0xFF9A6820));
        g.setGradientFill(amber);
        g.fillRect(bounds);
    }

    // Viñeta interna
    {
        juce::ColourGradient vign(
            juce::Colours::transparentBlack, cx, cy - radius * 0.4f,
            juce::Colour(0x60100800), bounds.getX(), bounds.getY(), true);
        g.setGradientFill(vign);
        g.fillRect(bounds);
    }

    // Sombra en bordes
    {
        juce::ColourGradient leftShadow(
            juce::Colour(0x30000000), bounds.getX(), bounds.getCentreY(),
            juce::Colours::transparentBlack, bounds.getX() + bounds.getWidth() * 0.25f, bounds.getCentreY(),
            false);
        g.setGradientFill(leftShadow);
        g.fillRect(bounds);

        juce::ColourGradient rightShadow(
            juce::Colour(0x30000000), bounds.getRight(), bounds.getCentreY(),
            juce::Colours::transparentBlack, bounds.getRight() - bounds.getWidth() * 0.25f, bounds.getCentreY(),
            false);
        g.setGradientFill(rightShadow);
        g.fillRect(bounds);
    }

    // ── 2. Arcos de la escala ──────────────────────────────────────────────
    const float arcR    = radius;
    const float aL      = kAngleLeft;
    const float aRange  = kAngleRange;
    const float zeroNorm = vuDbToNorm(0.0f);
    const float aZero   = aL + aRange * zeroNorm;

    {
        juce::Path blackArc;
        blackArc.addArc(cx - arcR, cy - arcR, arcR * 2.0f, arcR * 2.0f, aL, aZero, true);
        g.setColour(juce::Colour(0xFF111111));
        g.strokePath(blackArc, juce::PathStrokeType(1.8f));
    }

    {
        juce::Path redArc;
        redArc.addArc(cx - arcR, cy - arcR, arcR * 2.0f, arcR * 2.0f, aZero, aL + aRange, true);
        g.setColour(juce::Colour(0xFFCC1111));
        g.strokePath(redArc, juce::PathStrokeType(4.5f));

        g.setColour(juce::Colour(0x55FF6666));
        g.strokePath(redArc, juce::PathStrokeType(2.0f));
    }

    // ── 3. Ticks y labels de la escala ────────────────────────────────────
    struct Mark { float db; float tickLen; float fontSz; bool hasTick; };
    static const Mark scaleMarks[] = {
        { -20.0f, 11.0f, 9.5f,  true  },
        { -10.0f, 11.0f, 9.5f,  true  },
        {  -7.0f,  9.0f, 9.0f,  true  },
        {  -5.0f,  9.0f, 9.0f,  true  },
        {  -3.0f,  9.0f, 9.0f,  true  },
        {  -2.0f,  6.0f, 0.0f,  true  },
        {  -1.0f,  9.0f, 9.0f,  true  },
        {   0.0f, 13.0f, 9.5f,  true  },
        {   1.0f,  9.0f, 9.0f,  true  },
        {   2.0f,  9.0f, 9.0f,  true  },
        {   3.0f, 11.0f, 9.5f,  true  },
        { -15.0f,  5.0f, 0.0f, true },
        { -12.0f,  5.0f, 0.0f, true },
        {  -9.0f,  5.0f, 0.0f, true },
        {  -8.0f,  5.0f, 0.0f, true },
        {  -6.0f,  5.0f, 0.0f, true },
        {  -4.0f,  5.0f, 0.0f, true },
    };

    for (const auto& m : scaleMarks)
    {
        const float n     = vuDbToNorm(m.db);
        const float angle = aL + aRange * n;
        const float cosA  = std::cos(angle);
        const float sinA  = std::sin(angle);
        const bool  isRed = (m.db >= 0.0f);

        const float r1 = arcR;
        const float r2 = arcR + m.tickLen;

        if (m.hasTick)
        {
            juce::Path tick;
            tick.startNewSubPath(cx + cosA * r1, cy + sinA * r1);
            tick.lineTo(cx + cosA * r2, cy + sinA * r2);
            g.setColour(isRed ? juce::Colour(0xFFBB0000) : juce::Colour(0xFF111111));
            g.strokePath(tick, juce::PathStrokeType(m.fontSz > 0 ? 1.6f : 0.9f));
        }

        if (m.fontSz > 0.0f)
        {
            const float textR = r2 + 8.5f;
            const float tx    = cx + cosA * textR;
            const float ty    = cy + sinA * textR;

            juce::String lbl = (m.db > 0.0f ? "+" : "") + juce::String((int)m.db);
            g.setFont(juce::Font(juce::FontOptions(m.fontSz)).boldened());
            g.setColour(isRed ? juce::Colour(0xFFCC0000) : juce::Colour(0xFF0D0D0D));
            g.drawText(lbl,
                       juce::Rectangle<float>(tx - 15.0f, ty - 8.0f, 30.0f, 16.0f),
                       juce::Justification::centred);
        }
    }

    // ── 4. Label "VU" ─────────────────────────────────────────────────────
    {
        g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
        g.setColour(juce::Colour(0x501A1000));
        g.drawText("VU",
                   juce::Rectangle<float>(cx - 16.0f, cy - radius * 0.58f, 32.0f, 14.0f),
                   juce::Justification::centred);
    }

    // ── 5. Reflejo de cristal ──────────────────────────────────────────────
    {
        juce::ColourGradient glass(
            juce::Colours::white.withAlpha(0.13f), bounds.getCentreX(), bounds.getY(),
            juce::Colours::transparentWhite,       bounds.getCentreX(), bounds.getY() + bounds.getHeight() * 0.45f,
            false);
        g.setGradientFill(glass);
        g.fillRect(bounds.withHeight(bounds.getHeight() * 0.45f));
    }
}

void VintageVUMeters::drawNeedle(juce::Graphics& g, float cx, float cy,
                                 float radius, float level, float peakHold)
{
    juce::ignoreUnused(radius, peakHold);
    if (scaleArcR_ < 8.0f) return;

    const float norm   = juce::jlimit(0.0f, 1.0f, vuDbToNorm(level));
    const float angle  = kAngleLeft + kAngleRange * norm;
    const float cosA   = std::cos(angle);
    const float sinA   = std::sin(angle);

    const float needleLen = scaleArcR_ * 1.02f;
    const float tipX  = cx + cosA * needleLen;
    const float tipY  = cy + sinA * needleLen;

    const float tailLen  = scaleArcR_ * 0.10f;
    const float tailAngle = angle + juce::MathConstants<float>::pi;
    const float tailX = cx + std::cos(tailAngle) * tailLen;
    const float tailY = cy + std::sin(tailAngle) * tailLen;

    // ── Sombra proyectada ─────────────────────────────────────────────────
    {
        const float ox = scaleArcR_ * 0.018f;
        const float oy = scaleArcR_ * 0.025f;
        juce::Path shadow;
        shadow.startNewSubPath(tailX + ox, tailY + oy);
        shadow.lineTo(tipX + ox, tipY + oy);
        g.setColour(juce::Colours::black.withAlpha(0.22f));
        g.strokePath(shadow, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // ── Cuerpo principal de la aguja ──────────────────────────────────────
    {
        juce::Path needle;
        needle.startNewSubPath(tailX, tailY);
        needle.lineTo(tipX, tipY);

        g.setColour(juce::Colour(0xFF0A0A0A));
        g.strokePath(needle, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        g.setColour(juce::Colour(0xFF000000));
        g.strokePath(needle, juce::PathStrokeType(0.9f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }
}

VintageVUMeters::FaceGeom VintageVUMeters::computeGeom(juce::Rectangle<float> cell) const noexcept
{
    FaceGeom geo;
    geo.face    = cell.reduced(1.0f);
    geo.housing = geo.face.withTop(geo.face.getBottom());
    geo.cx      = geo.face.getCentreX();
    geo.cy      = geo.face.getBottom() - 5.0f;
    geo.radius  = (geo.face.getHeight() - 5.0f) * 0.72f;
    return geo;
}

void VintageVUMeters::computeCells(juce::Rectangle<int> bounds,
                                   juce::Rectangle<float> (&cells)[4]) const noexcept
{
    auto area = bounds;
    area.removeFromTop(20);
    auto b = area.toFloat().reduced(6.0f);
    const float halfW = b.getWidth()  * 0.5f;
    const float halfH = b.getHeight() * 0.5f;
    const float gap   = 6.0f;
    cells[0] = { b.getX(),               b.getY(),               halfW - gap * 0.5f, halfH - gap * 0.5f };
    cells[1] = { b.getX() + halfW + gap * 0.5f, b.getY(),               halfW - gap * 0.5f, halfH - gap * 0.5f };
    cells[2] = { b.getX(),               b.getY() + halfH + gap * 0.5f, halfW - gap * 0.5f, halfH - gap * 0.5f };
    cells[3] = { b.getX() + halfW + gap * 0.5f, b.getY() + halfH + gap * 0.5f, halfW - gap * 0.5f, halfH - gap * 0.5f };
}

void VintageVUMeters::drawBrushedPanel(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    juce::ColourGradient base(
        juce::Colour(0xFF1E1E20), bounds.getCentreX(), bounds.getY(),
        juce::Colour(0xFF0C0C0E), bounds.getCentreX(), bounds.getBottom(), false);
    base.addColour(0.4f, juce::Colour(0xFF161618));
    g.setGradientFill(base);
    g.fillRect(bounds);

    const int y0 = (int)bounds.getY();
    const int y1 = (int)bounds.getBottom();
    for (int y = y0; y < y1; ++y)
    {
        const unsigned int h = ((unsigned int)y * 2654435761u) >> 24;
        const float a = ((h & 0x0F) - 7.5f) / 7.5f;
        juce::Colour c = (a >= 0.0f)
            ? juce::Colours::white.withAlpha(0.014f * a)
            : juce::Colours::black.withAlpha(0.035f * -a);
        g.setColour(c);
        g.drawHorizontalLine(y, bounds.getX(), bounds.getRight());
    }

    {
        juce::ColourGradient topLit(
            juce::Colours::white.withAlpha(0.06f), bounds.getCentreX(), bounds.getY(),
            juce::Colours::transparentWhite,       bounds.getCentreX(), bounds.getY() + 16.0f, false);
        g.setGradientFill(topLit);
        g.fillRect(bounds.withHeight(16.0f));
    }
}

void VintageVUMeters::drawBevelFrame(juce::Graphics& g, juce::Rectangle<float> cell)
{
    const float corner = 5.0f;

    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.fillRoundedRectangle(cell.expanded(2.0f), corner + 1.5f);

    {
        juce::ColourGradient frameGrad(
            juce::Colour(0xFF030303), cell.getCentreX(), cell.getY(),
            juce::Colour(0xFF252527), cell.getCentreX(), cell.getBottom(), false);
        g.setGradientFill(frameGrad);
        g.fillRoundedRectangle(cell, corner);
    }

    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawLine(cell.getX() + corner, cell.getY() + 1.5f,
               cell.getRight() - corner, cell.getY() + 1.5f, 1.2f);

    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.drawLine(cell.getX() + corner, cell.getBottom() - 1.5f,
               cell.getRight() - corner, cell.getBottom() - 1.5f, 1.2f);

    auto inner = cell.reduced(4.0f);
    g.setColour(juce::Colours::black.withAlpha(0.50f));
    g.drawRoundedRectangle(inner.reduced(0.5f), corner * 0.5f, 2.0f);
}

void VintageVUMeters::drawHub(juce::Graphics& g, float cx, float cy, float faceH)
{
    const float r = juce::jlimit(5.0f, 12.0f, faceH * 0.090f);

    g.setColour(juce::Colours::black.withAlpha(0.40f));
    g.fillEllipse(cx - r * 1.4f, cy - r * 0.75f, r * 2.8f, r * 2.2f);

    {
        juce::ColourGradient capGrad(
            juce::Colour(0xFF303030), cx - r * 0.3f, cy - r * 0.55f,
            juce::Colour(0xFF050505), cx + r * 0.4f, cy + r * 0.65f,
            false);
        g.setGradientFill(capGrad);
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
    }

    g.setColour(juce::Colours::black.withAlpha(0.85f));
    g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.1f);

    const float br = r * 0.43f;
    {
        juce::ColourGradient brass(
            juce::Colour(0xFFEDD06E), cx - br * 0.35f, cy - br * 0.35f,
            juce::Colour(0xFF7A5B1A), cx + br * 0.5f,  cy + br * 0.5f,
            false);
        g.setGradientFill(brass);
        g.fillEllipse(cx - br, cy - br, br * 2.0f, br * 2.0f);
    }

    g.setColour(juce::Colours::white.withAlpha(0.60f));
    g.fillEllipse(cx - br * 0.52f, cy - br * 0.60f, br * 0.55f, br * 0.50f);

    g.setColour(juce::Colours::white.withAlpha(0.22f));
    g.fillEllipse(cx - r * 0.42f, cy - r * 0.58f, r * 0.38f, r * 0.32f);
}

void VintageVUMeters::paintMeterStatic(juce::Graphics& g, juce::Rectangle<float> cell, int idx)
{
    drawBevelFrame(g, cell);

    const auto geo = computeGeom(cell);

    const int cacheW = juce::jmax(1, (int)geo.face.getWidth());
    const int cacheH = juce::jmax(1, (int)geo.face.getHeight());

    if (!cacheValid_ || dialCache_.getWidth() != cacheW || dialCache_.getHeight() != cacheH) {
        dialCache_ = juce::Image(juce::Image::ARGB, cacheW, cacheH, true);
        juce::Graphics cg(dialCache_);
        auto local    = juce::Rectangle<float>(0.0f, 0.0f, (float)cacheW, (float)cacheH);
        float localCx = local.getCentreX();
        float localCy = local.getBottom() - 5.0f;
        drawStaticDialFace(cg, local, localCx, localCy, geo.radius);
        cacheValid_ = true;
    }
    g.drawImageAt(dialCache_, (int)geo.face.getX(), (int)geo.face.getY());

    {
        auto labelBounds = juce::Rectangle<float>(geo.cx - 22.0f, geo.cy - geo.radius * 0.36f, 44.0f, 15.0f);
        g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
        g.setColour(juce::Colour(0x6E120A00));
        g.drawText(juce::String(kLabels_[idx]), labelBounds, juce::Justification::centred);
    }

    drawHub(g, geo.cx, geo.cy, geo.face.getHeight());
}

void VintageVUMeters::rebuildPanelCache(juce::Rectangle<int> bounds)
{
    const int w = juce::jmax(1, bounds.getWidth());
    const int h = juce::jmax(1, bounds.getHeight());
    panelCache_ = juce::Image(juce::Image::ARGB, w, h, true);
    juce::Graphics cg(panelCache_);

    drawBrushedPanel(cg, juce::Rectangle<float>(0.0f, 0.0f, (float)w, (float)h));
    drawAnalyzerHeader(cg, juce::Rectangle<int>(0, 0, w, 20), "VU METERS");

    juce::Rectangle<float> cells[4];
    computeCells(bounds, cells);
    for (int i = 0; i < 4; ++i)
        paintMeterStatic(cg, cells[i], i);

    panelValid_ = true;
}

void VintageVUMeters::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    if (!panelValid_ || panelCache_.getWidth()  != bounds.getWidth()
                     || panelCache_.getHeight() != bounds.getHeight())
        rebuildPanelCache(bounds);
    g.drawImageAt(panelCache_, 0, 0);

    juce::Rectangle<float> cells[4];
    computeCells(bounds, cells);
    for (int i = 0; i < 4; ++i)
    {
        const auto geo = computeGeom(cells[i]);
        g.saveState();
        g.reduceClipRegion(geo.face.toNearestInt());
        drawNeedle(g, geo.cx, geo.cy, geo.radius,
                   channels_[i].level.getCurrent(),
                   channels_[i].peakHold);
        g.restoreState();
    }
}

} // namespace mixcoach
