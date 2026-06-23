#include "AnalogVUMeter.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Colores del estilo vintage VU
    // ═══════════════════════════════════════════════════════════════════════════
    constexpr uint32_t kBezelFrame     = 0xFF1A1A1A;
    constexpr uint32_t kBezelHighlight = 0xFF2A2A2A;
    constexpr uint32_t kPaperBase      = 0xFFF0DDB8; // Cream/yellowish warm base
    constexpr uint32_t kPaperBright    = 0xFFF8EDD0; // Brighter center
    constexpr uint32_t kPaperEdge      = 0xFFC4A878; // Darker warm edge
    constexpr uint32_t kPaperShadow    = 0xFFA08860; // Deepest edge shadow
    constexpr uint32_t kNeedleDark     = 0xFF2D2D2D;
    constexpr uint32_t kNeedleLight    = 0xFF4A4A4A;
    constexpr uint32_t kScaleText      = 0xFF1A1A1A;
    constexpr uint32_t kScaleTick      = 0xFF1A1A1A;
    constexpr uint32_t kRedZone        = 0xFFEF4444;
    constexpr uint32_t kLabelVu        = 0xFF8B7A5A;

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
        norm       = juce::jlimit(0.0f, 1.0f, norm);
        return kArcStartAngle + norm * kArcRange;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setLevel — Recibe dBFS, convierte a VU, actualiza smoothed + peak hold
    // ═══════════════════════════════════════════════════════════════════════════
    void AnalogVUMeter::setLevel(float levelDbFs)
    {
        const float vu = dbFsToVu(levelDbFs);
        smoothedVu_.setTargetValue(vu);

        uint32_t now                 = juce::Time::getMillisecondCounter();
        const float elapsedSincePeak = (now - peakHoldTimeMs_) * 0.001f;

        if (vu > peakHoldLevel_) {
            peakHoldLevel_  = vu;
            peakHoldTimeMs_ = now;
        }
        else if (elapsedSincePeak > 0.5f) {
            const float decay = 6.0f * (elapsedSincePeak - 0.5f);
            peakHoldLevel_    = juce::jmax(kVuMin, vu, peakHoldLevel_ - decay);
        }

        peakHoldVu_.setTargetValue(peakHoldLevel_);
    }

    bool AnalogVUMeter::advanceFrame(double sampleRateHz, bool allowRepaint)
    {
        const bool dirty = smoothedVu_.advance(sampleRateHz) | peakHoldVu_.advance(sampleRateHz);
        if (dirty && allowRepaint) repaint();
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
        if (bounds.isEmpty()) {
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
        auto faceBounds = bounds;

        drawBezel(g, faceBounds);
        drawPaperBackground(g, faceBounds);
        drawScale(g, faceBounds);
        drawVuLabel(g, faceBounds);
        drawChannelLabel(g, faceBounds);
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
        juce::ColourGradient innerShadow(juce::Colours::black.withAlpha(0.25f),
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
    //  drawPaperBackground — Papel vintage de VU meter
    //  Fondo crema amarillento cálido con gradiente radial:
    //  centro más brillante, bordes más oscuros y ligeramente sombreados
    // ═══════════════════════════════════════════════════════════════════════════
    void AnalogVUMeter::drawPaperBackground(juce::Graphics& g, juce::Rectangle<float> faceBounds) const
    {
        auto innerFace            = faceBounds.reduced(3.0f, 3.0f);
        juce::Point<float> centre = innerFace.getCentre();

        // ─── Base cream/yellowish ────────────────────────────────────────
        g.setColour(juce::Colour(kPaperBase));
        g.fillRoundedRectangle(innerFace, 3.0f);

        // ─── Radial gradient: centro brillante → bordes oscuros y cálidos ─
        juce::ColourGradient radialGrad(juce::Colour(kPaperBright), // Centro: brillante y cálido
                                        centre,
                                        juce::Colour(kPaperShadow), // Borde exterior: sombra profunda
                                        juce::Point<float>(innerFace.getX(), innerFace.getY()),
                                        true);
        radialGrad.addColour(0.40f, juce::Colour(kPaperBase)); // Zona media: base cream
        radialGrad.addColour(0.75f, juce::Colour(kPaperEdge)); // Cerca del borde: más oscuro
        g.setGradientFill(radialGrad);
        g.fillRoundedRectangle(innerFace, 3.0f);

        // ─── Brillito superior muy sutil (luz ambiental desde arriba) ────
        juce::ColourGradient topGlow(juce::Colours::white.withAlpha(0.08f),
                                     juce::Point<float>(0.0f, innerFace.getY()),
                                     juce::Colours::transparentBlack,
                                     juce::Point<float>(0.0f, innerFace.getY() + innerFace.getHeight() * 0.25f),
                                     false);
        g.setGradientFill(topGlow);
        g.fillRoundedRectangle(innerFace, 3.0f);

        // ─── Sombra interior en el borde superior (profundidad del bisel) ─
        auto topShadow = innerFace.withHeight(juce::jmin(8.0f, innerFace.getHeight() * 0.12f));
        juce::ColourGradient innerShadow(juce::Colours::black.withAlpha(0.12f),
                                         juce::Point<float>(0.0f, topShadow.getY()),
                                         juce::Colours::transparentBlack,
                                         juce::Point<float>(0.0f, topShadow.getBottom()),
                                         false);
        g.setGradientFill(innerShadow);
        g.fillRoundedRectangle(topShadow, 3.0f);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawScale — Escala impresa del VU meter
    //  Marcas: -20, -10, -7, -5, -3, -2, -1, 0, +1, +2, +3
    //  Zona roja: +1 a +3
    // ═══════════════════════════════════════════════════════════════════════════
    void AnalogVUMeter::drawScale(juce::Graphics& g, juce::Rectangle<float> faceBounds) const
    {
        auto innerFace     = faceBounds.reduced(3.0f, 3.0f);
        const float cx     = innerFace.getCentreX();
        const float pivotY = innerFace.getBottom() - kPivotOffsetY;
        const float scaleR = (innerFace.getHeight() - kPivotOffsetY) * kScaleRadiusFactor;

        // ─── Arco base completo (gris sutil) ──────────────────────────
        {
            juce::Path arcPath;
            arcPath.addArc(
                cx - scaleR, pivotY - scaleR, scaleR * 2.0f, scaleR * 2.0f, kArcStartAngle, kArcEndAngle, true);
            g.setColour(juce::Colour(kScaleTick).withAlpha(0.18f));
            g.strokePath(arcPath, juce::PathStrokeType(1.0f));
        }

        // ─── Arco rojo zona +1..+3 ─────────────────────────────────────
        {
            const float redStartAngle = valueToAngle(1.0f);
            const float redEndAngle   = valueToAngle(3.0f);
            juce::Path redArc;
            redArc.addArc(cx - scaleR, pivotY - scaleR, scaleR * 2.0f, scaleR * 2.0f, redStartAngle, redEndAngle, true);
            g.setColour(juce::Colour(kRedZone).withAlpha(0.85f));
            g.strokePath(redArc, juce::PathStrokeType(2.5f));
        }

        // ─── Marcas de la escala ───────────────────────────────────────
        struct ScaleMark
        {
            float vu;
            const char* label;
            bool isMajor;
            bool isRed;
        };

        static const ScaleMark marks[] = {
            {-20.0f, "-20", true, false},
            {-10.0f, "-10", true, false},
            {-7.0f, "-7", false, false},
            {-5.0f, "-5", true, false},
            {-3.0f, "-3", false, false},
            {-2.0f, "-2", false, false},
            {-1.0f, "-1", false, false},
            {0.0f, "0", true, false},
            {1.0f, "1", false, true},
            {2.0f, "2", false, true},
            {3.0f, "3", true, true},
        };

        for (const auto& mark : marks) {
            const float angle = valueToAngle(mark.vu);

            // Tick: major = largo, minor = corto
            const float tickOuterR = scaleR * 0.97f;
            const float tickInnerR = mark.isMajor ? scaleR * 0.80f : scaleR * 0.87f;
            const float tickW      = mark.isMajor ? 1.3f : 0.8f;

            const float x1 = cx + std::cos(angle) * tickInnerR;
            const float y1 = pivotY + std::sin(angle) * tickInnerR;
            const float x2 = cx + std::cos(angle) * tickOuterR;
            const float y2 = pivotY + std::sin(angle) * tickOuterR;

            if (mark.isRed) g.setColour(juce::Colour(kRedZone));
            else
                g.setColour(juce::Colour(kScaleTick).withAlpha(mark.isMajor ? 0.85f : 0.55f));

            g.drawLine(x1, y1, x2, y2, tickW);

            // Label numérico — todos los marks
            const float labelR = scaleR * 1.06f;
            float lx           = cx + std::cos(angle) * labelR;
            float ly           = pivotY + std::sin(angle) * labelR;

            const float labelW = 16.0f;
            const float labelH = 9.0f;

            // Empujar marcas extremas hacia afuera para no solaparse
            if (mark.vu <= -15.0f) lx -= 3.0f;
            if (mark.vu >= 2.0f) lx += 3.0f;

            g.setFont(juce::Font(
                juce::FontOptions(mark.isMajor ? MixCoachTheme::fontSizeMicro : MixCoachTheme::fontSizePico)));
            if (mark.isRed) g.setColour(juce::Colour(kRedZone));
            else
                g.setColour(juce::Colour(kScaleText).withAlpha(mark.isMajor ? 0.9f : 0.65f));

            // Mostrar TODOS los labels (no solo major)
            g.drawText(juce::String(mark.label),
                       juce::Rectangle<float>(lx - labelW * 0.5f, ly - labelH * 0.5f, labelW, labelH),
                       juce::Justification::centred);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawNeedle — Aguja oscura con efecto 3D
    // ═══════════════════════════════════════════════════════════════════════════
    void AnalogVUMeter::drawNeedle(juce::Graphics& g, juce::Rectangle<float> faceBounds, float angle) const
    {
        auto innerFace  = faceBounds.reduced(3.0f, 3.0f);
        float cx        = innerFace.getCentreX();
        float pivotY    = innerFace.getBottom() - kPivotOffsetY;
        float scaleR    = (innerFace.getHeight() - kPivotOffsetY) * kScaleRadiusFactor;
        float needleLen = scaleR * 0.80f;

        // ─── Puntas de la aguja ──────────────────────────────────────────
        float tipX = cx + std::cos(angle) * needleLen;
        float tipY = pivotY + std::sin(angle) * needleLen;

        // Cola de la aguja (detrás del pivote, más corta)
        float tailLen   = scaleR * 0.12f;
        float tailAngle = angle + juce::MathConstants<float>::pi;
        float tailX     = cx + std::cos(tailAngle) * tailLen;
        float tailY     = pivotY + std::sin(tailAngle) * tailLen;

        // ─── Sombra de la aguja ──────────────────────────────────────────
        {
            juce::Path shadowPath;
            shadowPath.startNewSubPath(tailX + 1.0f, tailY + 1.0f);

            float perpAngle = angle + juce::MathConstants<float>::halfPi;
            float perpLen   = 1.8f;

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
            float perpLen   = 1.8f;
            float tipWidth  = 0.3f;

            float nx1 = cx + std::cos(perpAngle) * perpLen;
            float ny1 = pivotY + std::sin(perpAngle) * perpLen;

            float nx2 = cx + std::cos(perpAngle + juce::MathConstants<float>::pi) * perpLen;
            float ny2 = pivotY + std::sin(perpAngle + juce::MathConstants<float>::pi) * perpLen;

            needlePath.lineTo(nx1, ny1);
            needlePath.lineTo(tipX + std::cos(perpAngle) * tipWidth, tipY + std::sin(perpAngle) * tipWidth);
            needlePath.lineTo(tipX, tipY);
            needlePath.lineTo(tipX - std::cos(perpAngle) * tipWidth, tipY - std::sin(perpAngle) * tipWidth);
            needlePath.lineTo(nx2, ny2);
            needlePath.closeSubPath();

            // Gradiente de la aguja (más claro en la base)
            juce::ColourGradient needleGrad(juce::Colour(kNeedleLight),
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
        float cx       = innerFace.getCentreX();
        float pivotY   = innerFace.getBottom() - kPivotOffsetY;
        float scaleR   = (innerFace.getHeight() - kPivotOffsetY) * kScaleRadiusFactor;

        // "VU" justo debajo del centro del arco
        float labelY = pivotY - scaleR * 0.32f;

        g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
        g.setColour(juce::Colour(kLabelVu).withAlpha(0.55f));
        g.drawText("VU", juce::Rectangle<float>(cx - 12.0f, labelY - 5.0f, 24.0f, 10.0f), juce::Justification::centred);
    }

    void AnalogVUMeter::drawChannelLabel(juce::Graphics& g, juce::Rectangle<float> faceBounds) const
    {
        if (labelText_.isEmpty()) return;

        auto innerFace = faceBounds.reduced(3.0f, 3.0f);
        float cx       = innerFace.getCentreX();
        float pivotY   = innerFace.getBottom() - kPivotOffsetY;
        float scaleR   = (innerFace.getHeight() - kPivotOffsetY) * kScaleRadiusFactor;

        // El canal (L/R/M/S) aparece a la derecha del centro, como en los VU analógicos clásicos
        float lx = cx + scaleR * 0.30f;
        float ly = pivotY - scaleR * 0.38f;

        g.setFont(juce::Font(juce::FontOptions(13.0f)).boldened());
        if (labelColourOverride_ != juce::Colour(0x00000000)) g.setColour(labelColourOverride_.withAlpha(0.75f));
        else
            g.setColour(juce::Colour(kLabelVu).withAlpha(0.75f));

        g.drawText(
            labelText_, juce::Rectangle<float>(lx - 14.0f, ly - 8.0f, 28.0f, 16.0f), juce::Justification::centred);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Dibuja el VU meter analógico vintage completo
    // ═══════════════════════════════════════════════════════════════════════════
    void AnalogVUMeter::paint(juce::Graphics& g)
    {
        if (!faceCacheValid_) rebuildFaceCache();

        if (faceCacheValid_) g.drawImageAt(faceCache_, 0, 0);

        // Dibujar aguja en tiempo real (no se cachea porque cambia con el audio)
        auto bounds     = getLocalBounds().toFloat();
        auto faceBounds = bounds;

        const float currentVu   = smoothedVu_.getCurrent();
        const float needleAngle = valueToAngle(currentVu);
        drawNeedle(g, faceBounds, needleAngle);

        // Peak hold triangle (dinámico)
        const float peakVu = peakHoldVu_.getCurrent();
        if (peakVu > kVuMin + 1.0f) {
            const float pkAngle = valueToAngle(peakVu);
            auto innerFace      = faceBounds.reduced(3.0f, 3.0f);
            const float cx      = innerFace.getCentreX();
            const float pivotY  = innerFace.getBottom() - kPivotOffsetY;
            const float scaleR  = (innerFace.getHeight() - kPivotOffsetY) * kScaleRadiusFactor;
            const float markerR = scaleR * 0.88f;

            const float mx = cx + std::cos(pkAngle) * markerR;
            const float my = pivotY + std::sin(pkAngle) * markerR;

            const float perpAngle = pkAngle + juce::MathConstants<float>::halfPi;
            const float triSize   = 3.5f;
            juce::Path pkTri;
            pkTri.addTriangle(mx + std::cos(perpAngle) * triSize,
                              my + std::sin(perpAngle) * triSize,
                              mx - std::cos(perpAngle) * triSize,
                              my - std::sin(perpAngle) * triSize,
                              mx - std::cos(pkAngle) * 3.0f,
                              my - std::sin(pkAngle) * 3.0f);
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            g.fillPath(pkTri);
        }
    }

} // namespace mixcoach
