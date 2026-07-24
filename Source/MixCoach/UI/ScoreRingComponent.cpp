#include "ScoreRingComponent.h"
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor
    // ═══════════════════════════════════════════════════════════════════════════
    ScoreRingComponent::ScoreRingComponent()
    {
        setOpaque(false);
        setSize(kDefaultSize, kDefaultSize);
        startTimerHz(60);
    }


    // ═══════════════════════════════════════════════════════════════════════════
    //  visibilityChanged — Pausa timer cuando el plugin no es visible
    // ═══════════════════════════════════════════════════════════════════════════

    void ScoreRingComponent::visibilityChanged()
    {
        if (isShowing() && !isTimerRunning()) {
            startTimerHz(60);
        }
        // Nunca detener el timer — la animación del score ring (stroke,
        // número) debe seguir animándose aunque el componente no sea visible.
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setScore / startAnimation / setScoreImmediate
    // ═══════════════════════════════════════════════════════════════════════════
    void ScoreRingComponent::setScore(float score)
    {
        targetScore_ = juce::jlimit(0.0f, 100.0f, score);
        startAnimation();
    }

    void ScoreRingComponent::startAnimation()
    {
        animating_ = true;
        animStartMs_ = juce::Time::getMillisecondCounter();
        animatedScore_.reset(0.0f);
        animatedScore_.setTargetValue(targetScore_);
        repaint();
    }

    void ScoreRingComponent::setScoreImmediate(float score)
    {
        targetScore_ = juce::jlimit(0.0f, 100.0f, score);
        animating_ = false;
        animatedScore_.reset(targetScore_);
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  timerCallback — Avanza la animación SmoothValue
    // ═══════════════════════════════════════════════════════════════════════════
    void ScoreRingComponent::timerCallback()
    {
        bool changed = false;

        if (animating_) {
            if (animatedScore_.advance(60.0))
                changed = true;

            // Check if animation is complete (converged to target)
            if (std::abs(animatedScore_.getCurrent() - targetScore_) < 0.1f
                && animatedScore_.getTarget() == targetScore_) {
                animating_ = false;
                if (onAnimationComplete)
                    onAnimationComplete();
            }
        }

        // Even when not animating, we need to repaint for the glow pulse
        // on high scores. Check if score >= 85 for the celebration pulse.
        if (targetScore_ >= 85.0f)
            changed = true;

        if (changed)
            repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  resized
    // ═══════════════════════════════════════════════════════════════════════════
    void ScoreRingComponent::resized()
    {
        // No layout calculations needed — everything is drawn relative to center
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint
    // ═══════════════════════════════════════════════════════════════════════════
    void ScoreRingComponent::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        float dim = juce::jmin(bounds.getWidth(), bounds.getHeight());
        float r = dim * 0.5f - kStrokeWidth * 0.5f - 4.0f;

        if (r < 10.0f) return;

        float currentScore = animatedScore_.getCurrent();
        float pct = juce::jlimit(0.0f, 1.0f, currentScore / 100.0f);

        // ═══ Ángulos ═══════════════════════════════════════════════════════
        // JUCE addCentredArc toma: startAngleRadians, endAngleRadians
        // El arco va de 135° a 405° (270° de apertura)
        float startRad = degToRadians(kStartDeg);
        float endRad = degToRadians(kEndDeg);
        float rangeRad = endRad - startRad;
        float fillEndRad = startRad + rangeRad * pct;

        // ═══ Background arc (track) — gris oscuro ═════════════════════════
        {
            juce::Path bgArc;
            bgArc.addCentredArc(cx, cy, r, r, 0.0f, startRad, endRad, true);
            g.setColour(juce::Colour(0xFF1E293B));
            g.strokePath(bgArc, juce::PathStrokeType(kStrokeWidth,
                           juce::PathStrokeType::curved,
                           juce::PathStrokeType::rounded));
        }

        // ═══ Fill arc (progress) — degradado púrpura → verde ═════════════
        if (pct > 0.01f) {
            juce::Path fillArc;
            fillArc.addCentredArc(cx, cy, r, r, 0.0f, startRad, fillEndRad, true);

            // Crear degradado a lo largo del arco (de inicio a fin)
            // El color depende de la posición en el arco
            float midPct = juce::jmin(pct, 1.0f);
            juce::Colour startCol = scoreGradient(0.0f);   // Púrpura
            juce::Colour midCol = scoreGradient(midPct * 0.5f);  // Cian en la mitad del arco
            juce::Colour endCol = scoreGradient(pct);       // Verde (o cian si no es 100%)

            // Punto de inicio y fin del degradado
            float startX = cx + r * std::cos(startRad);
            float startY = cy + r * std::sin(startRad);
            float endX = cx + r * std::cos(fillEndRad);
            float endY = cy + r * std::sin(fillEndRad);

            juce::ColourGradient arcGrad(startCol, startX, startY,
                                         endCol, endX, endY, false);
            arcGrad.addColour(0.5f, midCol);

            g.setGradientFill(arcGrad);
            g.strokePath(fillArc, juce::PathStrokeType(kStrokeWidth,
                           juce::PathStrokeType::curved,
                           juce::PathStrokeType::rounded));
        }

        // ═══ Glow ring para scores altos (≥85, pulso de celebración) ═════
        if (targetScore_ >= 85.0f) {
            float glowTime = (float)(juce::Time::getMillisecondCounter() % 2000) / 2000.0f;
            float glowAlpha = 0.08f + 0.06f * std::sin(glowTime * juce::MathConstants<float>::twoPi);
            float glowR = r + kStrokeWidth * 0.5f + 6.0f;

            juce::Colour glowCol = scoreGradient(pct).withAlpha(glowAlpha);
            juce::ColourGradient glowGrad(glowCol,
                                           cx, cy,
                                           juce::Colours::transparentBlack,
                                           cx + glowR * 1.5f, cy, true);
            g.setGradientFill(glowGrad);
            g.fillEllipse(cx - glowR * 1.5f, cy - glowR * 1.5f,
                          glowR * 3.0f, glowR * 3.0f);
        }

        // ═══ Score number (centro) ══════════════════════════════════════
        {
            int displayScore = (int)std::round(currentScore);

            // Sombra del texto
            g.setColour(juce::Colours::black.withAlpha(0.3f));
            g.setFont(juce::Font(juce::FontOptions(28.0f)).boldened());
            g.drawText(juce::String(displayScore),
                       juce::Rectangle<float>(cx - 30.0f, cy - 22.0f, 60.0f, 30.0f)
                           .translated(1.0f, 1.0f).toNearestInt(),
                       juce::Justification::centred);

            // Texto principal
            g.setColour(MixCoachTheme::textBright().withAlpha(0.95f));
            g.drawText(juce::String(displayScore),
                       juce::Rectangle<float>(cx - 30.0f, cy - 22.0f, 60.0f, 30.0f).toNearestInt(),
                       juce::Justification::centred);
        }

        // ═══ Label "/100" debajo del score ═══════════════════════════════
        {
            g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.6f));
            g.drawText("/ 100",
                       juce::Rectangle<float>(cx - 20.0f, cy + 10.0f, 40.0f, 16.0f).toNearestInt(),
                       juce::Justification::centred);
        }

        // ═══ Pequeño badge de estado (arriba del ring) ═════════════════
        {
            juce::String statusText;
            juce::Colour statusColour;
            if (currentScore >= 90.0f) {
                statusText = "\xE2\x9C\xA8 Excelente";
                statusColour = MixCoachTheme::success();
            } else if (currentScore >= 75.0f) {
                statusText = "\xF0\x9F\x91\x8F Muy bien";
                statusColour = juce::Colour(0xFF06B6D4);
            } else if (currentScore >= 50.0f) {
                statusText = "\xF0\x9F\x91\x8C En camino";
                statusColour = MixCoachTheme::warning();
            } else if (currentScore > 0.0f) {
                statusText = "[SEARCH] Necesita trabajo";
                statusColour = MixCoachTheme::textMuted();
            } else {
                statusText = "";
            }

            if (statusText.isNotEmpty()) {
                float badgeY = cy - r - 16.0f;
                auto badgeArea = juce::Rectangle<float>(cx - 60.0f, badgeY, 120.0f, 16.0f);
                g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
                g.setColour(statusColour.withAlpha(0.85f));
                g.drawText(statusText, badgeArea.toNearestInt(), juce::Justification::centred);
            }
        }
    }

} // namespace mixcoach
