#include "CircularGauge.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constants de dibujo — usamos uint32_t (tipo simple) para evitar el bug
    //  de MSVC \"most vexing parse\" donde const Colour se interpreta como
    //  declaración de función.
    // ═══════════════════════════════════════════════════════════════════════════
    constexpr uint32_t kArcTrackARGB     = 0xFF2A2B3E;
    constexpr uint32_t kNeedleColourARGB = 0xFF8B5CF6; // violeta
    constexpr uint32_t kNeedleGlowARGB   = 0xFFA78BFA;
    constexpr uint32_t kTickColourARGB   = 0xFF6B6E82;
    constexpr uint32_t kTickMajorARGB    = 0xFF8B8FA3;
    constexpr uint32_t kTextDimARGB      = 0xFF8B8FA3;
    constexpr uint32_t kTextBrightARGB   = 0xFFF1F1F6;
    constexpr uint32_t kEdgeGlowARGB     = 0xFFC4B5FD;

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor
    // ═══════════════════════════════════════════════════════════════════════════
    CircularGauge::CircularGauge()
    {
        smoothValue_.reset(0.0f);
        smoothValue_.setBallistics(4.0, 35.0);

        // ─── Title label ─────────────────────────────────────────────────────
        titleLabel_.setText("GAIN REDUCTION", juce::dontSendNotification);
        titleLabel_.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
        titleLabel_.setJustificationType(juce::Justification::centred);
        titleLabel_.setColour(juce::Label::textColourId, juce::Colour(kTextDimARGB));
        addAndMakeVisible(titleLabel_);

        // ─── Value label ─────────────────────────────────────────────────────
        valueLabel_.setText("0.0 dB", juce::dontSendNotification);
        valueLabel_.setFont(juce::Font(juce::FontOptions(16.0f)).boldened());
        valueLabel_.setJustificationType(juce::Justification::centred);
        valueLabel_.setColour(juce::Label::textColourId, juce::Colour(kNeedleColourARGB));
        addAndMakeVisible(valueLabel_);

        startTimerHz(60);
    }

    CircularGauge::~CircularGauge()
    {
        stopTimer();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Setters
    // ═══════════════════════════════════════════════════════════════════════════
    void CircularGauge::setTitle(const juce::String& title)
    {
        titleLabel_.setText(title, juce::dontSendNotification);
    }

    void CircularGauge::setValue(float valueDb)
    {
        targetValue_ = juce::jlimit(minDb_, maxDb_, valueDb);
    }

    void CircularGauge::setRange(float minDb, float maxDb)
    {
        minDb_ = minDb;
        maxDb_ = maxDb;
    }

    void CircularGauge::setLabelPrefix(const juce::String& prefix)
    {
        labelPrefix_ = prefix;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Timer
    // ═══════════════════════════════════════════════════════════════════════════
    void CircularGauge::timerCallback()
    {
        if (!isVisible()) return;

        smoothValue_.setTargetValue(targetValue_);
        if (!smoothValue_.advance(60.0)) return;

        const float current = smoothValue_.getCurrent();

        // ─── Actualizar value label ───────────────────────────────────────
        juce::String valStr;
        if (current < minDb_ + 0.5f) valStr = "--.-";
        else
            valStr = juce::String(current, 1);

        // Color del texto: neutro cerca de 0, violeta brillante si > 0
        juce::Colour needleColour(kNeedleColourARGB);
        juce::Colour edgeGlow(kEdgeGlowARGB);
        juce::Colour textDim(kTextDimARGB);

        juce::Colour textColour = needleColour;
        if (current < -3.0f) textColour = textDim;
        else if (current > 0.0f)
            textColour = edgeGlow; // más brillante

        valueLabel_.setColour(juce::Label::textColourId, textColour);
        valueLabel_.setText(valStr + " dB", juce::dontSendNotification);

        repaint();
    }

    void CircularGauge::visibilityChanged()
    {
        if (isVisible()) startTimerHz(60);
        else
            stopTimer();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Layout
    // ═══════════════════════════════════════════════════════════════════════════
    void CircularGauge::resized()
    {
        auto area = getLocalBounds().reduced(2);

        // Title at top
        titleLabel_.setBounds(area.removeFromTop(14));

        // Value label at bottom
        valueLabel_.setBounds(area.removeFromBottom(20));
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  valueToAngle — Mapea valor dB a ángulo del needle (radianes)
    //
    //  El arco va de 135° (startAngle, abajo-izquierda) a 45° (endAngle+2π,
    //  abajo-derecha), pasando por 180° (izquierda), 270° (arriba) y
    //  360°/0° (derecha).
    //
    //  -12 dB → 135° (inicio del arco, izquierda)
    //    0 dB → 270° (arriba, centro)
    //   +12 dB → 45° (405°, final del arco, derecha)
    // ═══════════════════════════════════════════════════════════════════════════
    float CircularGauge::valueToAngle(float valueDb) const
    {
        float norm = (valueDb - minDb_) / (maxDb_ - minDb_);
        norm       = juce::jlimit(0.0f, 1.0f, norm);
        return kArcStartAngle + norm * kArcRange;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Paint
    // ═══════════════════════════════════════════════════════════════════════════
    void CircularGauge::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        // ─── El arco ocupa el espacio entre title y value label ────────────
        auto arcBounds = bounds;
        arcBounds.removeFromTop(14.0f);    // title
        arcBounds.removeFromBottom(20.0f); // value label

        float cx     = arcBounds.getCentreX();
        float cy     = arcBounds.getCentreY() + 2.0f; // ligero offset vertical
        float radius = std::min(arcBounds.getWidth(), arcBounds.getHeight()) * 0.42f;

        // ─── Fondo transparente — NO pintar fondo, solo el arco ───────────
        drawArc(g, {cx - radius - 4, cy - radius - 4, radius * 2 + 8, radius * 2 + 8});
        drawScale(g, {cx - radius - 4, cy - radius - 4, radius * 2 + 8, radius * 2 + 8});
        drawNeedle(g, {cx - radius - 4, cy - radius - 4, radius * 2 + 8, radius * 2 + 8});
        drawCenterDot(g, {cx - radius - 4, cy - radius - 4, radius * 2 + 8, radius * 2 + 8});
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawArc — Arco semicircular con gradiente y glow
    // ═══════════════════════════════════════════════════════════════════════════
    void CircularGauge::drawArc(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        float cx     = bounds.getCentreX();
        float cy     = bounds.getCentreY();
        float outerR = std::min(bounds.getWidth(), bounds.getHeight()) * 0.45f;
        float innerR = outerR * 0.85f;

        juce::Colour needleColour(kNeedleColourARGB);
        juce::Colour edgeGlow(kEdgeGlowARGB);
        juce::Colour textDim(kTextDimARGB);

        // ─── Arco exterior glow (efecto neón) ────────────────────────────
        {
            juce::Path glowPath;
            glowPath.addArc(cx - outerR,
                            cy - outerR,
                            outerR * 2,
                            outerR * 2,
                            kArcStartAngle,
                            kArcEndAngle + juce::MathConstants<float>::twoPi,
                            true);
            g.setColour(juce::Colour::fromFloatRGBA(139.0f / 255.0f, 92.0f / 255.0f, 246.0f / 255.0f, 0.04f));
            g.strokePath(glowPath, juce::PathStrokeType(outerR * 0.25f));
        }

        // ─── Arco track (fondo) ─────────────────────────────────────────
        {
            juce::Path trackPath;
            trackPath.addArc(cx - outerR,
                             cy - outerR,
                             outerR * 2,
                             outerR * 2,
                             kArcStartAngle,
                             kArcEndAngle + juce::MathConstants<float>::twoPi,
                             true);
            g.setColour(juce::Colour(kArcTrackARGB));
            g.strokePath(trackPath, juce::PathStrokeType(outerR - innerR));
        }

        // ─── Arco activo hasta el valor actual ──────────────────────────────
        float current = smoothValue_.getCurrent();

        if (current > minDb_ + 0.5f) {
            float endAngle = valueToAngle(current);
            // Asegurar que endAngle > startAngle sumando 2π si es necesario
            if (endAngle < kArcStartAngle) endAngle += juce::MathConstants<float>::twoPi;

            juce::Colour activeColour = textDim;
            if (current < -6.0f) {
                activeColour = textDim;
            }
            else if (current < -2.0f) {
                activeColour = needleColour.interpolatedWith(juce::Colour(0xFFFFFFFFu), 0.3f);
            }
            else if (current < 2.0f) {
                activeColour = needleColour.interpolatedWith(edgeGlow, 0.5f);
            }
            else {
                activeColour = edgeGlow;
            }

            juce::Path activePath;
            activePath.addArc(cx - outerR, cy - outerR, outerR * 2, outerR * 2, kArcStartAngle, endAngle, true);
            g.setColour(activeColour.withAlpha(0.35f));
            g.strokePath(activePath, juce::PathStrokeType(outerR - innerR - 1.0f));
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawScale — Marcas de escala y labels numéricos
    // ═══════════════════════════════════════════════════════════════════════════
    void CircularGauge::drawScale(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        float cx         = bounds.getCentreX();
        float cy         = bounds.getCentreY();
        float outerR     = std::min(bounds.getWidth(), bounds.getHeight()) * 0.45f;
        float tickInnerR = outerR * 0.78f;
        float tickOuterR = outerR * 0.92f;
        float labelR     = outerR * 1.05f; // Ajuste para columna GR 109px

        juce::Colour tickColour(kTickColourARGB);
        juce::Colour tickMajor(kTickMajorARGB);
        juce::Colour textDim(kTextDimARGB);

        // ─── Marcas principales ───────────────────────────────────────────
        static constexpr float markValues[] = {-12.0f, -6.0f, 0.0f, 6.0f, 12.0f};
        static const char* markLabels[]     = {"-12", "-6", "0", "+6", "+12"};
        static constexpr int numMarks       = 5;

        for (int i = 0; i < numMarks; ++i) {
            float angle = valueToAngle(markValues[i]);

            bool isCenter   = (std::abs(markValues[i]) < 0.5f);
            float tickWidth = isCenter ? 1.5f : 1.0f;

            // ─── Tick line ────────────────────────────────────────────────
            float x1 = cx + std::cos(angle) * tickInnerR;
            float y1 = cy + std::sin(angle) * tickInnerR;
            float x2 = cx + std::cos(angle) * tickOuterR;
            float y2 = cy + std::sin(angle) * tickOuterR;

            g.setColour(isCenter ? tickMajor : tickColour);
            g.drawLine(x1, y1, x2, y2, tickWidth);

            // ─── Label ────────────────────────────────────────────────────
            float lx = cx + std::cos(angle) * labelR;
            float ly = cy + std::sin(angle) * labelR;

            float labelW = 20.0f;
            float labelH = 10.0f;

            // Para marcas laterales, desplazar para no solaparse con el arco
            if (markValues[i] < -9.0f) {
                lx -= labelW * 0.5f;
            }
            else if (markValues[i] > 9.0f) {
                lx += labelW * 0.5f;
            }

            g.setFont(juce::Font(juce::FontOptions(7.0f)).boldened());
            g.setColour(textDim);
            g.drawText(juce::String(markLabels[i]),
                       juce::Rectangle<float>(lx - labelW * 0.5f, ly - labelH * 0.5f, labelW, labelH),
                       juce::Justification::centred);

            // ─── Subtick at -3 and +3 ────────────────────────────────────
            if (!isCenter && std::abs(markValues[i]) > 3.5f) {
                float subVal   = markValues[i] + ((markValues[i] < 0.0f) ? 3.0f : -3.0f);
                float subAngle = valueToAngle(subVal);
                float sx1      = cx + std::cos(subAngle) * (tickInnerR + tickOuterR) * 0.5f;
                float sy1      = cy + std::sin(subAngle) * (tickInnerR + tickOuterR) * 0.5f;
                float sx2      = cx + std::cos(subAngle) * tickOuterR;
                float sy2      = cy + std::sin(subAngle) * tickOuterR;
                g.setColour(tickColour.withAlpha(0.4f));
                g.drawLine(sx1, sy1, sx2, sy2, 0.7f);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawNeedle — Aguja violeta con glow progresivo
    // ═══════════════════════════════════════════════════════════════════════════
    void CircularGauge::drawNeedle(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        float cx     = bounds.getCentreX();
        float cy     = bounds.getCentreY();
        float outerR = std::min(bounds.getWidth(), bounds.getHeight()) * 0.45f;

        float current = smoothValue_.getCurrent();
        float angle   = valueToAngle(current);

        juce::Colour needleColour(kNeedleColourARGB);
        juce::Colour needleGlow(kNeedleGlowARGB);

        // ─── Tip of the needle ────────────────────────────────────────────
        float tipLen = outerR * 0.82f;
        float tipX   = cx + std::cos(angle) * tipLen;
        float tipY   = cy + std::sin(angle) * tipLen;

        // ─── Tail (detrás del centro, más corto) ─────────────────────────
        float tailLen   = outerR * 0.15f;
        float tailAngle = angle + juce::MathConstants<float>::pi; // opposite
        float tailX     = cx + std::cos(tailAngle) * tailLen;
        float tailY     = cy + std::sin(tailAngle) * tailLen;

        // ─── Glow exterior (4 capas decrecientes) ─────────────────────────
        for (int layer = 3; layer >= 0; --layer) {
            float glowWidth = 3.0f + static_cast<float>(layer) * 2.5f;
            float alpha     = 0.08f / static_cast<float>(layer + 1);
            g.setColour(needleColour.withAlpha(alpha));

            juce::Path glowPath;
            glowPath.startNewSubPath(tailX, tailY);
            float perpAngle = angle + juce::MathConstants<float>::halfPi;
            float perpLen   = glowWidth;

            float gx1 = cx + std::cos(perpAngle) * perpLen;
            float gy1 = cy + std::sin(perpAngle) * perpLen;
            float gx2 = cx + std::cos(perpAngle + juce::MathConstants<float>::pi) * perpLen;
            float gy2 = cy + std::sin(perpAngle + juce::MathConstants<float>::pi) * perpLen;

            glowPath.lineTo(gx1, gy1);
            glowPath.lineTo(tipX + std::cos(perpAngle) * glowWidth * 0.3f,
                            tipY + std::sin(perpAngle) * glowWidth * 0.3f);
            glowPath.lineTo(tipX, tipY);
            glowPath.lineTo(tipX - std::cos(perpAngle) * glowWidth * 0.3f,
                            tipY - std::sin(perpAngle) * glowWidth * 0.3f);
            glowPath.lineTo(gx2, gy2);
            glowPath.closeSubPath();
            g.fillPath(glowPath);
        }

        // ─── Needle principal (violeta sólido) ────────────────────────────
        {
            juce::Path needlePath;
            needlePath.startNewSubPath(tailX, tailY);

            float perpAngle = angle + juce::MathConstants<float>::halfPi;
            float perpLen   = 2.0f;

            float nx1 = cx + std::cos(perpAngle) * perpLen;
            float ny1 = cy + std::sin(perpAngle) * perpLen;
            float nx2 = cx + std::cos(perpAngle + juce::MathConstants<float>::pi) * perpLen;
            float ny2 = cy + std::sin(perpAngle + juce::MathConstants<float>::pi) * perpLen;

            needlePath.lineTo(nx1, ny1);
            needlePath.lineTo(tipX, tipY); // tip
            needlePath.lineTo(nx2, ny2);
            needlePath.closeSubPath();

            // Fill with gradient from base to tip
            juce::ColourGradient needleGrad(needleColour.brighter(0.3f),
                                            juce::Point<float>(tipX, tipY),
                                            needleColour,
                                            juce::Point<float>(cx, cy),
                                            false);
            g.setGradientFill(needleGrad);
            g.fillPath(needlePath);

            // Highlight on one side
            g.setColour(juce::Colour(0xFFFFFFFFu).withAlpha(0.15f));
            g.strokePath(needlePath, juce::PathStrokeType(0.5f));
        }

        // ─── Tip glow (punto brillante en la punta) ───────────────────────
        {
            g.setColour(needleGlow.withAlpha(0.6f));
            g.fillEllipse(tipX - 2.0f, tipY - 2.0f, 4.0f, 4.0f);
            g.setColour(juce::Colour(0xFFFFFFFFu).withAlpha(0.3f));
            g.fillEllipse(tipX - 1.0f, tipY - 1.0f, 2.0f, 2.0f);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawCenterDot — Centro del gauge con glow
    // ═══════════════════════════════════════════════════════════════════════════
    void CircularGauge::drawCenterDot(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();

        juce::Colour needleColour(kNeedleColourARGB);
        juce::Colour arcTrack(kArcTrackARGB);

        // ─── Outer glow ──────────────────────────────────────────────────
        juce::ColourGradient glowGrad(needleColour.withAlpha(0.15f),
                                      juce::Point<float>(cx, cy),
                                      needleColour.withAlpha(0.0f),
                                      juce::Point<float>(cx + 12.0f, cy),
                                      false);
        g.setGradientFill(glowGrad);
        g.fillEllipse(cx - 12.0f, cy - 12.0f, 24.0f, 24.0f);

        // ─── Center ring (outer) ─────────────────────────────────────────
        g.setColour(arcTrack);
        g.fillEllipse(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);

        // ─── Center dot ──────────────────────────────────────────────────
        g.setColour(needleColour);
        g.fillEllipse(cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);

        // ─── Highlight ───────────────────────────────────────────────────
        g.setColour(juce::Colour(0xFFFFFFFFu).withAlpha(0.25f));
        g.fillEllipse(cx - 1.5f, cy - 1.5f, 3.0f, 3.0f);
    }

} // namespace mixcoach
