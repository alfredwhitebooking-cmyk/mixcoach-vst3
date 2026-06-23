#include "CrestPanel.h"
#include <cmath>

namespace mixcoach {

    CrestPanel::CrestPanel()
    {
        setOpaque(false);
    }

    void CrestPanel::resized() {}

    void CrestPanel::setValues(float peak, float rms)
    {
        rawPeak_ = peak;
        rawRms_  = rms;
        peak_.setTargetValue(peak);
        rms_.setTargetValue(rms);

        float c = (rms > -60.0f && peak > -60.0f) ? juce::jmax(0.0f, peak - rms) : 0.0f;
        crest_.setTargetValue(c);
    }

    bool CrestPanel::advanceVisuals(double sampleRateHz, bool allowRepaint)
    {
        bool dirty = false;
        dirty |= peak_.advance(sampleRateHz);
        dirty |= rms_.advance(sampleRateHz);
        dirty |= crest_.advance(sampleRateHz);
        if (dirty && allowRepaint) repaint();
        return dirty;
    }

    void CrestPanel::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds();
        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), 6.0f);

        // ─── Borde del panel con glow ─────────────────────────────────────────
        g.setColour(juce::Colour(0xFF1A2A44).withAlpha(0.30f));
        g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 6.0f, 1.0f);
        g.setColour(juce::Colour(0xFFA855F7).withAlpha(0.04f));
        g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 6.0f, 2.0f);

        auto area = bounds.reduced(5, 3);

        // ─── Header: "CREST FACTOR" con glow morado ───────────────────────────
        {
            auto headerArea = area.removeFromTop(18);
            g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
            g.setColour(juce::Colour(0xFFC084FC));
            g.drawText("CREST FACTOR", headerArea, juce::Justification::centred);

            auto underline = headerArea.withTop(headerArea.getBottom() - 1).toFloat();
            juce::ColourGradient glow(juce::Colour(0xFFC084FC).withAlpha(0.25f),
                                      underline.getX(),
                                      underline.getY(),
                                      juce::Colour(0xFFC084FC).withAlpha(0.01f),
                                      underline.getRight(),
                                      underline.getY(),
                                      false);
            glow.addColour(0.35f, juce::Colour(0xFFC084FC).withAlpha(0.10f));
            g.setGradientFill(glow);
            g.fillRect(underline.withHeight(1.0f));
        }

        // ─── Gauge semicircular (~68% del espacio restante) ──────────────────
        auto gaugeArea = area.removeFromTop(juce::jmax(72, area.getHeight() * 68 / 100)).toFloat();
        drawGauge(g, gaugeArea);

        // ─── Lectura digital (~15%) ──────────────────────────────────────────
        float crest      = juce::jmax(0.0f, crest_.getCurrent());
        auto digitalArea = area.removeFromTop(juce::jmax(28, area.getHeight() * 15 / 100)).toFloat();
        drawDigitalReading(g, digitalArea, crest);

        // ─── Separator con glow ─────────────────────────────────────────────
        auto sepArea = area.removeFromTop(juce::jmax(6, area.getHeight() * 3 / 100));
        {
            float sy = sepArea.getCentreY();
            juce::ColourGradient sepGrad(juce::Colour(0xFFA855F7).withAlpha(0.15f),
                                         (float)sepArea.getX(),
                                         sy,
                                         juce::Colour(0xFFA855F7).withAlpha(0.02f),
                                         (float)sepArea.getRight(),
                                         sy,
                                         false);
            sepGrad.addColour(0.35f, juce::Colour(0xFFA855F7).withAlpha(0.06f));
            g.setGradientFill(sepGrad);
            g.drawHorizontalLine((int)sy, sepArea.getX() + 10, sepArea.getRight() - 10);

            g.setColour(MixCoachTheme::divider().withAlpha(0.15f));
            g.drawHorizontalLine((int)(sy + 2), sepArea.getX() + 20, sepArea.getRight() - 20);
        }

        // ─── Metrics table (~17% restante) ───────────────────────────────────
        auto metricsArea = area.toFloat();
        drawMetricsTable(g, metricsArea);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Gauge semicircular con gradiente continuo de alta calidad
    // ═══════════════════════════════════════════════════════════════════════════

    void CrestPanel::drawGauge(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        // ─── Fondo del gauge con gradiente radial ────────────────────────────
        {
            auto gaugeBg = bounds;
            juce::ColourGradient radialBg(juce::Colour(0xFF0E1520),
                                          gaugeBg.getCentreX(),
                                          gaugeBg.getCentreY(),
                                          juce::Colour(0xFF06080E),
                                          gaugeBg.getX(),
                                          gaugeBg.getY(),
                                          true);
            radialBg.addColour(0.6f, juce::Colour(0xFF0A0E18));
            g.setGradientFill(radialBg);
            g.fillRoundedRectangle(gaugeBg, 5.0f);

            // Inner glow subtle
            g.setColour(juce::Colour(0xFF1A2A44).withAlpha(0.08f));
            g.fillRoundedRectangle(gaugeBg.reduced(2.0f), 4.0f);
        }

        // ─── Geometría del arco (semicírculo contenido) ─────────────────────
        auto arcArea = bounds.reduced(10, 4);
        float cx     = arcArea.getCentreX();
        float radius = juce::jmin(arcArea.getWidth() * 0.44f, arcArea.getHeight() * 0.45f);
        radius       = juce::jmax(radius, 20.0f);
        float cy     = arcArea.getBottom() - radius; // centro exacto en el borde inferior
        float halfR  = radius;

        // ─── Arco de fondo (track) con glow interno ──────────────────────────
        {
            juce::Path bgArc;
            bgArc.addArc(cx - radius, cy - halfR, radius * 2.0f, halfR * 2.0f, kGaugeStart, kGaugeEnd, true);
            // Outer glow
            g.setColour(juce::Colour(0xFF1A2A44).withAlpha(0.08f));
            g.strokePath(bgArc, juce::PathStrokeType(10.0f));
            // Track
            g.setColour(juce::Colour(0xFF1A2A44).withAlpha(0.40f));
            g.strokePath(bgArc, juce::PathStrokeType(7.0f));
            // Inner highlight
            g.setColour(juce::Colour(0xFF2A3A55).withAlpha(0.15f));
            g.strokePath(bgArc, juce::PathStrokeType(3.0f));
        }

        // ─── Arco multizona con gradiente continuo ───────────────────────────
        // Mapea color en función de la posición angular para transición suave
        constexpr int kArcSegments = 40;
        for (int i = 0; i < kArcSegments; ++i) {
            float t0 = (float)i / (float)kArcSegments;
            float t1 = (float)(i + 1) / (float)kArcSegments;

            float aStart = kGaugeStart + t0 * kGaugeRange;
            float aEnd   = kGaugeStart + t1 * kGaugeRange;

            // Color gradient continuo: Verde → Amarillo → Naranja → Rojo
            juce::Colour segCol;
            if (t1 < 0.30f) {
                // Verde puro (#44CC66)
                segCol = juce::Colour(0xFF44CC66);
            }
            else if (t1 < 0.50f) {
                // Transición Verde → Amarillo
                float mix = (t1 - 0.30f) / 0.20f;
                segCol    = juce::Colour(0xFF44CC66).interpolatedWith(juce::Colour(0xFFFFCC44), mix);
            }
            else if (t1 < 0.70f) {
                // Transición Amarillo → Naranja
                float mix = (t1 - 0.50f) / 0.20f;
                segCol    = juce::Colour(0xFFFFCC44).interpolatedWith(juce::Colour(0xFFFF8833), mix);
            }
            else {
                // Transición Naranja → Rojo
                float mix = (t1 - 0.70f) / 0.30f;
                segCol    = juce::Colour(0xFFFF8833).interpolatedWith(juce::Colour(0xFFEE3333), juce::jmin(1.0f, mix));
            }

            juce::Path segArc;
            segArc.addArc(cx - radius, cy - halfR, radius * 2.0f, halfR * 2.0f, aStart, aEnd, true);

            // Zona brillante
            g.setColour(segCol.withAlpha(0.85f));
            g.strokePath(segArc, juce::PathStrokeType(5.5f));

            // Glow detrás
            g.setColour(segCol.withAlpha(0.07f));
            g.strokePath(segArc, juce::PathStrokeType(10.0f));
        }

        // ─── Crest fill dinámico sobre el arco ───────────────────────────────
        float crest     = juce::jmax(0.0f, crest_.getCurrent());
        float crestNorm = juce::jlimit(0.0f, 1.0f, crest / kMaxCrest);

        if (crestNorm > 0.01f) {
            float fillEnd = kGaugeStart + crestNorm * kGaugeRange;

            juce::Path fillArc;
            fillArc.addArc(cx - radius, cy - halfR, radius * 2.0f, halfR * 2.0f, kGaugeStart, fillEnd, true);

            // Color dinámico con transición suave
            juce::Colour fillCol;
            if (crestNorm < 0.30f) fillCol = juce::Colour(0xFF44CC66);
            else if (crestNorm < 0.50f)
                fillCol =
                    juce::Colour(0xFF44CC66).interpolatedWith(juce::Colour(0xFFFFCC44), (crestNorm - 0.30f) / 0.20f);
            else if (crestNorm < 0.70f)
                fillCol =
                    juce::Colour(0xFFFFCC44).interpolatedWith(juce::Colour(0xFFFF8833), (crestNorm - 0.50f) / 0.20f);
            else
                fillCol =
                    juce::Colour(0xFFFF8833).interpolatedWith(juce::Colour(0xFFEE3333), (crestNorm - 0.70f) / 0.30f);

            // Fill glow exterior
            g.setColour(fillCol.withAlpha(0.25f));
            g.strokePath(fillArc, juce::PathStrokeType(12.0f));

            // Fill principal
            g.setColour(fillCol.withAlpha(0.95f));
            g.strokePath(fillArc, juce::PathStrokeType(5.5f));

            // Bright core
            g.setColour(juce::Colours::white.withAlpha(0.15f));
            g.strokePath(fillArc, juce::PathStrokeType(2.0f));
        }

        // ─── Escala con marcas ───────────────────────────────────────────────
        drawScale(g, cx, cy, radius);

        // ─── Aguja ───────────────────────────────────────────────────────────
        drawNeedle(g, cx, cy, radius, crestNorm);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Escala: marcas principales (0, 5, 10, 15, 20, 24) + secundarias + dB label
    // ═══════════════════════════════════════════════════════════════════════════

    void CrestPanel::drawScale(juce::Graphics& g, float cx, float cy, float radius)
    {
        float halfR = radius;

        // ─── Marcas principales ─────────────────────────────────────────────
        struct MajorMark
        {
            float val;
            const char* text;
        };

        MajorMark majorMarks[] = {{0, "0"}, {5, "5"}, {10, "10"}, {15, "15"}, {20, "20"}, {24, "24"}};

        for (auto& m : majorMarks) {
            float norm = juce::jlimit(0.0f, 1.0f, m.val / kMaxCrest);
            float a    = kGaugeStart + norm * kGaugeRange;
            float ca   = std::cos(a);
            float sa   = std::sin(a);

            // Tick externo más largo para principales
            float tickLen = (m.val == 0.0f || m.val == 24.0f) ? 8.0f : 6.0f;
            float tx1     = cx + ca * (radius - 2.0f);
            float ty1     = cy + sa * (halfR - 1.0f);
            float tx2     = cx + ca * (radius + tickLen);
            float ty2     = cy + sa * (halfR + tickLen * 0.5f);

            float alpha = (m.val == 0.0f || m.val == 24.0f) ? 0.85f : 0.70f;
            g.setColour(juce::Colour(0xFFE0E4E8).withAlpha(alpha));
            g.drawLine(tx1, ty1, tx2, ty2, 1.8f);

            // Tick glow más visible
            g.setColour(juce::Colour(0xFFA855F7).withAlpha(0.06f));
            g.drawLine(tx1, ty1, tx2, ty2, 4.0f);

            // Texto más grande
            float lx = cx + ca * (radius + 15.0f);
            float ly = cy + sa * (halfR + 8.0f);
            g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
            g.setColour(juce::Colour(0xFFE0E4E8));
            g.drawText(
                juce::String(m.text), juce::Rectangle<float>(lx - 11, ly - 5, 22, 10), juce::Justification::centred);
        }

        // ─── Marcas secundarias entre principales ───────────────────────────
        struct MinorRange
        {
            float start, end, step;
        };

        MinorRange minorRanges[] = {
            {1.0f, 4.0f, 1.0f},
            {6.0f, 9.0f, 1.0f},
            {12.0f, 14.0f, 1.0f},
            {16.0f, 19.0f, 1.0f},
            {22.0f, 23.0f, 1.0f},
        };

        for (auto& r : minorRanges) {
            for (float v = r.start; v <= r.end + 0.1f; v += r.step) {
                float norm = juce::jlimit(0.0f, 1.0f, v / kMaxCrest);
                float a    = kGaugeStart + norm * kGaugeRange;
                float ca   = std::cos(a);
                float sa   = std::sin(a);

                float tx1 = cx + ca * (radius - 0.5f);
                float ty1 = cy + sa * (halfR - 0.3f);
                float tx2 = cx + ca * (radius + 4.0f);
                float ty2 = cy + sa * (halfR + 2.0f);

                g.setColour(juce::Colour(0xFF888888).withAlpha(0.40f));
                g.drawLine(tx1, ty1, tx2, ty2, 1.0f);
            }
        }

        // ─── "dB" label centrada debajo del arco ─────────────────────────────
        float labelY = cy + halfR + 20.0f;
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
        g.setColour(juce::Colour(0xFF8899AA).withAlpha(0.55f));
        g.drawText("dB", juce::Rectangle<float>(cx - 12, labelY, 24, 9), juce::Justification::centred);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Aguja cónica premium (Path) — ancha en base, fina en punta, contra-peso
    // ═══════════════════════════════════════════════════════════════════════════

    void CrestPanel::drawNeedle(juce::Graphics& g, float cx, float cy, float radius, float crestNorm)
    {
        float halfR       = radius;
        float needleAngle = kGaugeStart + crestNorm * kGaugeRange;

        float ca = std::cos(needleAngle);
        float sa = std::sin(needleAngle);

        // Largo de la aguja: ~85% del radio
        float needleLen = radius * 0.85f;
        float nx        = cx + ca * needleLen;
        float ny        = cy + sa * (needleLen * (halfR / radius));

        // Contra-peso trasero
        float cwLen = radius * 0.18f;
        float cwx   = cx - ca * cwLen;
        float cwy   = cy - sa * (cwLen * (halfR / radius));

        // Vector dirección normalizado para perpendicular
        float dx  = nx - cx;
        float dy  = ny - cy;
        float len = std::sqrt(dx * dx + dy * dy);

        float perpX = 0.0f, perpY = 1.0f;
        if (len > 0.01f) {
            perpX = -dy / len;
            perpY = dx / len;
        }

        // ─── Sombra de la aguja (Path desplazado 2px) ────────────────────────
        float shadowOff = 2.0f;
        float snx       = nx + shadowOff;
        float sny       = ny + shadowOff * (halfR / radius);
        float scx       = cx + shadowOff;
        float scy       = cy + shadowOff * (halfR / radius);
        float scwx      = cwx + shadowOff;
        float scwy      = cwy + shadowOff * (halfR / radius);

        // Sombra cuerpo
        {
            juce::Path shadowPath;
            float sBaseW = 5.5f;
            float sTipW  = 0.5f;
            shadowPath.startNewSubPath(scx + perpX * sBaseW * 0.5f, scy + perpY * sBaseW * 0.5f);
            shadowPath.lineTo(snx + perpX * sTipW * 0.5f, sny + perpY * sTipW * 0.5f);
            shadowPath.lineTo(snx - perpX * sTipW * 0.5f, sny - perpY * sTipW * 0.5f);
            shadowPath.lineTo(scx - perpX * sBaseW * 0.5f, scy - perpY * sBaseW * 0.5f);
            shadowPath.closeSubPath();
            g.setColour(juce::Colour(0xFF000000).withAlpha(0.20f));
            g.fillPath(shadowPath);
        }

        // ─── Cuerpo principal de la aguja (gradiente metálico cónico) ────────
        {
            juce::Path needlePath;
            float baseW = 5.5f;
            float tipW  = 0.5f;
            needlePath.startNewSubPath(cx + perpX * baseW * 0.5f, cy + perpY * baseW * 0.5f);
            needlePath.lineTo(nx + perpX * tipW * 0.5f, ny + perpY * tipW * 0.5f);
            needlePath.lineTo(nx - perpX * tipW * 0.5f, ny - perpY * tipW * 0.5f);
            needlePath.lineTo(cx - perpX * baseW * 0.5f, cy - perpY * baseW * 0.5f);
            needlePath.closeSubPath();

            juce::ColourGradient needleGrad(juce::Colour(0xFF8A9AAA),
                                            nx,
                                            ny, // gris claro brillante (punta)
                                            juce::Colour(0xFF1A2233),
                                            cx,
                                            cy, // gris oscuro profundo (base)
                                            true);
            needleGrad.addColour(0.4f, juce::Colour(0xFF5A6A7A));
            g.setGradientFill(needleGrad);
            g.fillPath(needlePath);
        }

        // ─── Highlight lateral (efecto 3D metálico) ──────────────────────────
        float hOffX = perpX * 1.0f;
        float hOffY = perpY * 1.0f;
        {
            juce::Path highlightPath;
            float hlBase = 1.5f;
            float hlTip  = 0.3f;
            highlightPath.startNewSubPath(cx + hOffX + perpX * hlBase * 0.5f, cy + hOffY + perpY * hlBase * 0.5f);
            highlightPath.lineTo(nx + hOffX + perpX * hlTip * 0.5f, ny + hOffY + perpY * hlTip * 0.5f);
            highlightPath.lineTo(nx + hOffX - perpX * hlTip * 0.5f, ny + hOffY - perpY * hlTip * 0.5f);
            highlightPath.lineTo(cx + hOffX - perpX * hlBase * 0.5f, cy + hOffY - perpY * hlBase * 0.5f);
            highlightPath.closeSubPath();
            g.setColour(juce::Colour(0xFFCCDDEE).withAlpha(0.20f));
            g.fillPath(highlightPath);
        }

        // ─── Contra-peso cónico ───────────────────────────────────────────────
        {
            juce::Path cwPath;
            float cwBase = 2.5f;
            float cwTip  = 0.3f;
            cwPath.startNewSubPath(cx + perpX * cwBase * 0.5f, cy + perpY * cwBase * 0.5f);
            cwPath.lineTo(cwx + perpX * cwTip * 0.5f, cwy + perpY * cwTip * 0.5f);
            cwPath.lineTo(cwx - perpX * cwTip * 0.5f, cwy - perpY * cwTip * 0.5f);
            cwPath.lineTo(cx - perpX * cwBase * 0.5f, cy - perpY * cwBase * 0.5f);
            cwPath.closeSubPath();
            g.setColour(juce::Colour(0xFF3A4A5A).withAlpha(0.50f));
            g.fillPath(cwPath);
        }

        // ─── Pivote mecánico premium ─────────────────────────────────────────
        {
            // Anillo exterior más grande
            g.setColour(juce::Colour(0xFF0A0E14));
            g.fillEllipse(cx - 5.5f, cy - 5.5f, 11.0f, 11.0f);

            // Anillo metálico intermedio
            juce::ColourGradient pivotGrad(
                juce::Colour(0xFF6A7A8A), cx - 4.0f, cy - 4.0f, juce::Colour(0xFF2A3344), cx + 4.0f, cy + 4.0f, true);
            g.setGradientFill(pivotGrad);
            g.fillEllipse(cx - 4.0f, cy - 4.0f, 8.0f, 8.0f);

            // Anillo fino
            g.setColour(juce::Colour(0xFF8A9AAA).withAlpha(0.5f));
            g.drawEllipse(cx - 4.0f, cy - 4.0f, 8.0f, 8.0f, 0.6f);

            // Centro oscuro
            g.setColour(juce::Colour(0xFF05080D));
            g.fillEllipse(cx - 1.8f, cy - 1.8f, 3.6f, 3.6f);

            // Punto de luz
            g.setColour(juce::Colour(0xFFFFFFFF).withAlpha(0.12f));
            g.fillEllipse(cx - 0.5f, cy - 0.5f, 1.0f, 1.0f);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Lectura digital premium: "12.5 dB" con glow dinámico
    //  Diseño: valor enorme centrado, unidad integrada, color adaptativo
    // ═══════════════════════════════════════════════════════════════════════════

    void CrestPanel::drawDigitalReading(juce::Graphics& g, juce::Rectangle<float> bounds, float crest)
    {
        juce::String valStr  = juce::String(crest, 1);
        juce::String fullStr = valStr + " dB";

        // Color según nivel con transición suave
        juce::Colour valCol;
        float glowAlpha;
        if (crest > 24.0f) {
            valCol    = juce::Colour(0xFFEE3333);
            glowAlpha = 0.12f;
        }
        else if (crest > 18.0f) {
            float t   = (crest - 18.0f) / 6.0f;
            valCol    = juce::Colour(0xFFFF8833).interpolatedWith(juce::Colour(0xFFEE3333), t);
            glowAlpha = 0.06f + t * 0.06f;
        }
        else if (crest > 12.0f) {
            float t   = (crest - 12.0f) / 6.0f;
            valCol    = juce::Colour(0xFFFFCC44).interpolatedWith(juce::Colour(0xFFFF8833), t);
            glowAlpha = 0.03f + t * 0.03f;
        }
        else if (crest > 6.0f) {
            float t   = (crest - 6.0f) / 6.0f;
            valCol    = juce::Colour(0xFF44CC66).interpolatedWith(juce::Colour(0xFFFFCC44), t);
            glowAlpha = 0.02f;
        }
        else {
            valCol    = juce::Colour(0xFF44CC66);
            glowAlpha = 0.0f;
        }

        // ─── Glow detrás del valor ───────────────────────────────────────────
        if (glowAlpha > 0.01f) {
            auto glowBounds = bounds.expanded(8.0f, 3.0f);
            g.setColour(valCol.withAlpha(glowAlpha));
            g.fillRoundedRectangle(glowBounds, 5.0f);

            // Second outer glow
            g.setColour(valCol.withAlpha(glowAlpha * 0.3f));
            g.fillRoundedRectangle(glowBounds.expanded(4.0f, 2.0f), 6.0f);
        }

        // ─── Sombra del texto completo ───────────────────────────────────────
        auto textBounds = bounds.reduced(2, 0);
        g.setFont(juce::Font(juce::FontOptions(24.0f)).boldened());
        g.setColour(juce::Colour(0xFF000000).withAlpha(0.25f));
        g.drawText(fullStr, textBounds.translated(1.0f, 1.0f), juce::Justification::centred);

        // ─── Valor principal glow ────────────────────────────────────────────
        g.setColour(valCol.withAlpha(0.15f));
        g.drawText(fullStr, textBounds, juce::Justification::centred);

        // ─── Valor principal ────────────────────────────────────────────────
        g.setColour(valCol);
        g.drawText(fullStr, textBounds, juce::Justification::centred);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Tabla de métricas premium: 3 filas (PEAK, RMS, CREST)
    //  Con glow en fila activa, mejor espaciado, y color dinámico
    // ═══════════════════════════════════════════════════════════════════════════

    void CrestPanel::drawMetricsTable(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        struct MetricRow
        {
            const char* label;
            float value;
            const char* unit;
            juce::Colour accent;
            bool highlight;
        };

        float cval = juce::jmax(0.0f, crest_.getCurrent());

        // CREST color dinámico según valor
        juce::Colour crestCol;
        if (cval > 24.0f) crestCol = juce::Colour(0xFFEE3333);
        else if (cval > 18.0f)
            crestCol = juce::Colour(0xFFFF8833);
        else if (cval > 12.0f)
            crestCol = juce::Colour(0xFFFFCC44);
        else
            crestCol = juce::Colour(0xFF44CC66);

        MetricRow rows[] = {
            {"PEAK", rawPeak_, "dBFS", juce::Colour(0xFFFFCC44), false},
            {"RMS", rawRms_, "dBFS", juce::Colour(0xFF44BBFF), false},
            {"CREST", cval, "dB", crestCol, true},
        };

        const int numRows = 3;
        const float rowH  = bounds.getHeight() / numRows;

        for (int i = 0; i < numRows; ++i) {
            auto rowArea =
                juce::Rectangle<float>(bounds.getX(), bounds.getY() + i * rowH - 0.5f, bounds.getWidth(), rowH + 1.0f);

            // ─── Background highlight para CREST ─────────────────────────────
            if (rows[i].highlight) {
                auto bgBounds = rowArea.reduced(2, 1);
                juce::ColourGradient hlGrad(rows[i].accent.withAlpha(0.06f),
                                            bgBounds.getX(),
                                            bgBounds.getY(),
                                            rows[i].accent.withAlpha(0.01f),
                                            bgBounds.getRight(),
                                            bgBounds.getY(),
                                            false);
                g.setGradientFill(hlGrad);
                g.fillRoundedRectangle(bgBounds, 3.0f);

                // Left accent bar
                g.setColour(rows[i].accent.withAlpha(0.25f));
                g.fillRect(bgBounds.getX(), bgBounds.getY() + 2, 2.0f, bgBounds.getHeight() - 4);
            }
            else if (i % 2 == 0) {
                // Background sutil para filas pares
                g.setColour(juce::Colour(0x06FFFFFF));
                g.fillRoundedRectangle(rowArea.reduced(3, 0), 2.0f);
            }

            // ─── Label (izquierda) ───────────────────────────────────────────
            auto labelArea = rowArea.removeFromLeft(rowArea.getWidth() * 0.38f).reduced(5, 0);
            g.setFont(juce::Font(juce::FontOptions(8.5f)).boldened());
            g.setColour(rows[i].accent.withAlpha(0.85f));
            g.drawText(rows[i].label, labelArea, juce::Justification::centredLeft);

            // ─── Value + Unit (derecha) ─────────────────────────────────────
            auto valueArea = rowArea.reduced(5, 0);

            juce::String valStr;
            if (rows[i].value > -60.0f) valStr = juce::String(rows[i].value, 1);
            else
                valStr = "--.-";

            juce::String unitStr = rows[i].unit;

            // Value (big number)
            auto numArea = valueArea.removeFromRight(valueArea.getWidth() * 0.60f);
            g.setFont(juce::Font(juce::FontOptions(10.5f)).boldened());

            // Sombra
            g.setColour(juce::Colour(0xFF000000).withAlpha(0.15f));
            g.drawText(valStr, numArea.translated(1, 1), juce::Justification::centredRight);

            // Valor con color dinámico
            juce::Colour valCol = rows[i].highlight ? rows[i].accent : juce::Colour(0xFFF0F4F8);
            g.setColour(valCol);
            g.drawText(valStr, numArea, juce::Justification::centredRight);

            // Unit (small text)
            auto unitArea = valueArea;
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)));
            g.setColour(juce::Colour(0xFF8899AA).withAlpha(0.7f));
            g.drawText(unitStr, unitArea, juce::Justification::centredLeft);

            // ─── Línea separadora entre filas ────────────────────────────────
            if (i < numRows - 1) {
                float sy = rowArea.getY() + rowArea.getHeight();
                juce::ColourGradient sepGrad(MixCoachTheme::divider().withAlpha(0.12f),
                                             bounds.getX() + 15,
                                             sy,
                                             MixCoachTheme::divider().withAlpha(0.04f),
                                             bounds.getRight() - 15,
                                             sy,
                                             false);
                g.setGradientFill(sepGrad);
                g.drawHorizontalLine(juce::roundToInt(sy), bounds.getX() + 15, bounds.getRight() - 15);
            }
        }
    }

} // namespace mixcoach
