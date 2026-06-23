#include "CrestHistogram.h"

namespace mixcoach {

    constexpr uint32_t kNeedleDark  = 0xFF2D2D2D;
    constexpr uint32_t kNeedleLight = 0xFF4A4A4A;
    constexpr uint32_t kBezelFrame  = 0xFF1A1A1A;

    CrestHistogram::CrestHistogram()
    {
        crestSmooth_.reset(0.0f);
    }

    float CrestHistogram::valueToAngle(float crest) const
    {
        float norm = (crest - kCrestMin) / (kCrestMax - kCrestMin);
        norm       = juce::jlimit(0.0f, 1.0f, norm);
        return kArcStartAngle + norm * kArcRange;
    }

    void CrestHistogram::pushCrest(float peakDb, float rmsDb)
    {
        float crest = juce::jlimit(kCrestMin, kCrestMax, peakDb - rmsDb);
        recentCrest_.push_back(crest);
        if (recentCrest_.size() > (size_t)kMaxSamples) recentCrest_.pop_front();

        totalSamples_++;
        crestSmooth_.setTargetValue(crest);
        currentPeakDb_ = peakDb;
        currentRmsDb_  = rmsDb;
    }

    bool CrestHistogram::advanceFrame(double sampleRateHz, bool allowRepaint)
    {
        if (!crestSmooth_.advance(sampleRateHz)) return false;

        if (allowRepaint) repaint();

        return true;
    }

    void CrestHistogram::resized() {}

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawArc — Arco semicircular coloreado (verde→amarillo→rojo)
    // ═══════════════════════════════════════════════════════════════════════════
    void CrestHistogram::drawArc(juce::Graphics& g, juce::Rectangle<float> gaugeBounds)
    {
        float cx           = gaugeBounds.getCentreX();
        float cy           = gaugeBounds.getBottom();
        float outerR       = juce::jmin(gaugeBounds.getWidth(), gaugeBounds.getHeight() * 2.0f) * 0.48f;
        float innerR       = outerR * 0.78f;
        float midR         = (outerR + innerR) * 0.5f; // midpoint radius for arc path
        float arcThickness = outerR - innerR;

        // ─── Fondo oscuro del arco ──────────────────────────────────────────
        juce::Path bgArc;
        bgArc.addArc(cx - midR, cy - midR, midR * 2.0f, midR * 2.0f, kArcStartAngle, kArcEndAngle, true);
        g.setColour(MixCoachTheme::bgDarker().withAlpha(0.7f));
        g.strokePath(bgArc, juce::PathStrokeType(arcThickness));

        auto drawColoredArc = [&](float startNorm, float endNorm, juce::Colour colour, float alpha) {
            float startAngle = kArcStartAngle + startNorm * kArcRange;
            float endAngle   = kArcStartAngle + endNorm * kArcRange;
            if (endAngle <= startAngle) return;

            juce::Path segPath;
            segPath.addArc(cx - midR, cy - midR, midR * 2.0f, midR * 2.0f, startAngle, endAngle, true);
            g.setColour(colour.withAlpha(alpha));
            g.strokePath(segPath, juce::PathStrokeType(arcThickness));
        };

        // ─── Segmentos de color: verde 0-10, amarillo 10-20, rojo 20-30 ────
        float greenEnd  = 10.0f / 30.0f; // 33%
        float yellowEnd = 20.0f / 30.0f; // 67%

        drawColoredArc(0.0f, greenEnd, MixCoachTheme::meterGreen(), 0.85f);
        drawColoredArc(greenEnd, yellowEnd, MixCoachTheme::meterYellow(), 0.85f);
        drawColoredArc(yellowEnd, 1.0f, MixCoachTheme::meterRed(), 0.85f);

        // ─── Filled arc (current value overlay) ────────────────────────────
        float crestVal  = crestSmooth_.getCurrent();
        float crestNorm = (crestVal - kCrestMin) / (kCrestMax - kCrestMin);
        crestNorm       = juce::jlimit(0.0f, 1.0f, crestNorm);

        if (crestNorm > 0.01f) {
            juce::Colour fillColour;
            if (crestNorm <= greenEnd) fillColour = MixCoachTheme::meterGreen();
            else if (crestNorm <= yellowEnd)
                fillColour = MixCoachTheme::meterYellow();
            else
                fillColour = MixCoachTheme::meterRed();

            float fillEnd = kArcStartAngle + crestNorm * kArcRange;
            juce::Path fillArc;
            fillArc.addArc(cx - midR, cy - midR, midR * 2.0f, midR * 2.0f, kArcStartAngle, fillEnd, true);

            // Inner brighter fill
            g.setColour(fillColour.withAlpha(1.0f));
            g.strokePath(fillArc, juce::PathStrokeType(arcThickness));

            // Glow exterior
            g.setColour(fillColour.withAlpha(0.20f));
            g.strokePath(fillArc, juce::PathStrokeType(arcThickness + 4.0f));

            // White highlight on top of arc
            juce::ColourGradient topGlow(juce::Colours::white.withAlpha(0.12f),
                                         juce::Point<float>(cx - outerR, 0.0f),
                                         juce::Colours::white.withAlpha(0.0f),
                                         juce::Point<float>(cx + outerR, 0.0f),
                                         false);
            g.setGradientFill(topGlow);
            g.strokePath(fillArc, juce::PathStrokeType(arcThickness));
        }

        // ─── Borde exterior del arco ────────────────────────────────────────
        g.setColour(MixCoachTheme::border().withAlpha(0.2f));
        g.strokePath(bgArc, juce::PathStrokeType(0.5f));
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawScale — Marcas de escala + labels numéricos
    // ═══════════════════════════════════════════════════════════════════════════
    void CrestHistogram::drawScale(juce::Graphics& g, juce::Rectangle<float> gaugeBounds)
    {
        float cx     = gaugeBounds.getCentreX();
        float cy     = gaugeBounds.getBottom();
        float outerR = juce::jmin(gaugeBounds.getWidth(), gaugeBounds.getHeight() * 2.0f) * 0.48f;
        float innerR = outerR * 0.78f;
        float midR   = (outerR + innerR) * 0.5f;

        struct ScaleMark
        {
            float value;
            bool isMajor;
        };

        ScaleMark marks[] = {
            {0.0f, true}, {5.0f, false}, {10.0f, true}, {15.0f, false}, {20.0f, true}, {25.0f, false}, {30.0f, true}};

        for (auto& mark : marks) {
            float angle = valueToAngle(mark.value);

            // Tick line
            float tickInnerR = innerR + (outerR - innerR) * 0.2f;
            float tickOuterR = outerR - (outerR - innerR) * 0.2f;
            float tickWidth  = mark.isMajor ? 1.2f : 0.6f;

            float x1 = cx + std::cos(angle) * tickInnerR;
            float y1 = cy + std::sin(angle) * tickInnerR;
            float x2 = cx + std::cos(angle) * tickOuterR;
            float y2 = cy + std::sin(angle) * tickOuterR;

            g.setColour(MixCoachTheme::textDim().withAlpha(mark.isMajor ? 0.6f : 0.3f));
            g.drawLine(x1, y1, x2, y2, tickWidth);

            // Label
            if (mark.isMajor) {
                float labelR = outerR + 10.0f;
                float lx     = cx + std::cos(angle) * labelR;
                float ly     = cy + std::sin(angle) * labelR;

                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened());
                g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
                g.drawText(juce::String((int)mark.value),
                           juce::Rectangle<float>(lx - 10.0f, ly - 5.0f, 20.0f, 10.0f),
                           juce::Justification::centred);
            }
        }

        // ─── Label de unidad ────────────────────────────────────────────────
        float labelR   = outerR + 12.0f;
        float midAngle = (kArcStartAngle + kArcEndAngle) * 0.5f;
        float ux       = cx + std::cos(midAngle) * labelR;
        float uy       = cy + std::sin(midAngle) * labelR;
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizePico)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.35f));
        g.drawText("dB", juce::Rectangle<float>(ux - 8.0f, uy - 5.0f, 16.0f, 10.0f), juce::Justification::centred);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawNeedle — Aguja oscura para el valor actual de Crest
    // ═══════════════════════════════════════════════════════════════════════════
    void CrestHistogram::drawNeedle(juce::Graphics& g, juce::Rectangle<float> gaugeBounds, float angle)
    {
        float cx        = gaugeBounds.getCentreX();
        float cy        = gaugeBounds.getBottom();
        float outerR    = juce::jmin(gaugeBounds.getWidth(), gaugeBounds.getHeight() * 2.0f) * 0.48f;
        float needleLen = outerR * 0.75f;

        float tipX = cx + std::cos(angle) * needleLen;
        float tipY = cy + std::sin(angle) * needleLen;

        float tailLen   = outerR * 0.12f;
        float tailAngle = angle + juce::MathConstants<float>::pi;
        float tailX     = cx + std::cos(tailAngle) * tailLen;
        float tailY     = cy + std::sin(tailAngle) * tailLen;

        // ─── Sombra de la aguja ──────────────────────────────────────────
        juce::Path shadowPath;
        shadowPath.startNewSubPath(tailX + 1.0f, tailY + 1.0f);
        float perpAngle = angle + juce::MathConstants<float>::halfPi;
        float perpLen   = 1.5f;
        float sx1       = cx + std::cos(perpAngle) * perpLen + 1.0f;
        float sy1       = cy + std::sin(perpAngle) * perpLen + 1.0f;
        float sx2       = cx + std::cos(perpAngle + juce::MathConstants<float>::pi) * perpLen + 1.0f;
        float sy2       = cy + std::sin(perpAngle + juce::MathConstants<float>::pi) * perpLen + 1.0f;
        shadowPath.lineTo(sx1, sy1);
        shadowPath.lineTo(tipX + 1.0f, tipY + 1.0f);
        shadowPath.lineTo(sx2, sy2);
        shadowPath.closeSubPath();
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.fillPath(shadowPath);

        // ─── Cuerpo de la aguja ──────────────────────────────────────────
        juce::Path needlePath;
        needlePath.startNewSubPath(tailX, tailY);
        float nx1 = cx + std::cos(perpAngle) * perpLen;
        float ny1 = cy + std::sin(perpAngle) * perpLen;
        float nx2 = cx + std::cos(perpAngle + juce::MathConstants<float>::pi) * perpLen;
        float ny2 = cy + std::sin(perpAngle + juce::MathConstants<float>::pi) * perpLen;
        needlePath.lineTo(nx1, ny1);
        needlePath.lineTo(tipX, tipY);
        needlePath.lineTo(nx2, ny2);
        needlePath.closeSubPath();

        juce::ColourGradient needleGrad(juce::Colour(kNeedleLight),
                                        juce::Point<float>(cx, cy),
                                        juce::Colour(kNeedleDark),
                                        juce::Point<float>(tipX, tipY),
                                        false);
        g.setGradientFill(needleGrad);
        g.fillPath(needlePath);

        // ─── Pivot dot ───────────────────────────────────────────────────
        g.setColour(juce::Colour(kBezelFrame));
        g.fillEllipse(cx - 3.5f, cy - 3.5f, 7.0f, 7.0f);
        g.setColour(juce::Colour(kNeedleLight));
        g.fillEllipse(cx - 1.5f, cy - 1.5f, 3.0f, 3.0f);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawSubValues — PEAK, RMS, CREST en texto debajo del gauge
    // ═══════════════════════════════════════════════════════════════════════════
    void CrestHistogram::drawSubValues(juce::Graphics& g, juce::Rectangle<float> gaugeBounds)
    {
        auto infoArea = gaugeBounds.withTop(gaugeBounds.getBottom() + 4.0f).withHeight(36.0f).toNearestInt();

        float crestVal = crestSmooth_.getCurrent();
        float avgCrest = 0.0f;
        if (!recentCrest_.empty()) {
            float sum = 0.0f;
            for (auto c : recentCrest_) sum += c;
            avgCrest = sum / (float)recentCrest_.size();
        }

        // ─── Crest value (grande, centrado) ─────────────────────────────
        auto crestArea = infoArea.removeFromTop(14);
        g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());

        juce::Colour crestColour;
        if (crestVal < 10.0f) crestColour = MixCoachTheme::meterGreen();
        else if (crestVal < 20.0f)
            crestColour = MixCoachTheme::meterYellow();
        else
            crestColour = MixCoachTheme::meterRed();

        juce::String crestStr = (totalSamples_ > 0) ? juce::String(crestVal, 1) + " dB" : "--.- dB";
        g.setColour(crestColour);
        g.drawText(crestStr, crestArea, juce::Justification::centred);

        // ─── Sub-info: PEAK / RMS / CREST avg ──────────────────────────
        auto subArea = infoArea.reduced(4, 0);
        int subH     = subArea.getHeight() / 3;

        auto drawSubLine = [&](const juce::String& label, float value, juce::Colour col, juce::Rectangle<int> area) {
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)));
            auto labelArea = area.removeFromLeft(32);
            g.setColour(MixCoachTheme::textDim().withAlpha(0.6f));
            g.drawText(label, labelArea, juce::Justification::centredLeft);

            g.setColour(col);
            juce::String valStr = (totalSamples_ > 0) ? juce::String(value, 1) : "--";
            g.drawText(valStr, area, juce::Justification::centredRight);
        };

        auto pkArea  = subArea.removeFromTop(subH);
        auto rmsArea = subArea.removeFromTop(subH);
        auto avgArea = subArea;

        drawSubLine("PEAK:", currentPeakDb_, MixCoachTheme::meterYellow(), pkArea);
        drawSubLine("RMS:", currentRmsDb_, MixCoachTheme::accentGlow(), rmsArea);
        drawSubLine("CREST:", avgCrest, MixCoachTheme::success(), avgArea);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint
    // ═══════════════════════════════════════════════════════════════════════════
    void CrestHistogram::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

        auto area        = getLocalBounds().reduced(4);
        auto gaugeBounds = area.toFloat();

        if (totalSamples_ == 0) {
            // Mensaje inicial
            auto msgArea = gaugeBounds;
            msgArea.removeFromBottom(40);
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.4f));
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)));
            g.drawText("Esperando datos...", msgArea.toNearestInt(), juce::Justification::centred);
            return;
        }

        float crestVal    = crestSmooth_.getCurrent();
        float needleAngle = valueToAngle(crestVal);

        drawArc(g, gaugeBounds);
        drawScale(g, gaugeBounds);
        drawNeedle(g, gaugeBounds, needleAngle);
        drawSubValues(g, gaugeBounds);
    }

} // namespace mixcoach
