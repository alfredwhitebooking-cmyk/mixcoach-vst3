#include "RobotAvatarComponent.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Utilities
    // ═══════════════════════════════════════════════════════════════════════════

    namespace {
        // Wave animation: ease-out bounce for natural feel
        static float waveBounce(float t) noexcept
        {
            // t: 0..1 over the wave phase
            // Returns -1..1 for the arm swing
            // Fast up, slow down = natural wave
            if (t < 0.5f) {
                float u = t * 2.0f;
                return -std::sin(u * juce::MathConstants<float>::pi * 0.5f);
            } else {
                float u = (t - 0.5f) * 2.0f;
                return -std::sin((0.5f + u * 0.5f) * juce::MathConstants<float>::pi);
            }
        }

        // Map wavePhase (0..1) to arm swing angle in radians
        // Returns angle from vertical (0 = arm down, positive = arm up)
        static float waveArmAngle(float phase) noexcept
        {
            // Oscillate between 20° and 130° from vertical
            constexpr float kMinAngle = juce::MathConstants<float>::pi * 20.0f / 180.0f;
            constexpr float kMaxAngle = juce::MathConstants<float>::pi * 130.0f / 180.0f;
            float swing = std::sin(phase * juce::MathConstants<float>::twoPi * 3.0f); // 3 waves per cycle
            return kMinAngle + (kMaxAngle - kMinAngle) * (0.5f + 0.5f * swing);
        }
    }

    static juce::Colour expressionGlowColour(AvatarExpression expression, float alpha = 1.0f) noexcept
    {
        switch (expression) {
            case AvatarExpression::Happy:       return juce::Colour(0xFF00E5A0).withAlpha(alpha); // Teal-green
            case AvatarExpression::Surprised:   return juce::Colour(0xFFFFD600).withAlpha(alpha); // Amber-yellow
            case AvatarExpression::Thinking:    return juce::Colour(0xFF448AFF).withAlpha(alpha); // Blue
            case AvatarExpression::Encouraging: return juce::Colour(0xFFFFAB40).withAlpha(alpha); // Warm orange
            case AvatarExpression::Serious:     return juce::Colour(0xFFFF5252).withAlpha(alpha); // Red
            case AvatarExpression::Celebrating: return juce::Colour(0xFF22D3A7).withAlpha(alpha); // Emerald green
            case AvatarExpression::Neutral:
            default:                            return juce::Colour(0xFFC8A8FF).withAlpha(alpha); // Purple glow
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor
    // ═══════════════════════════════════════════════════════════════════════════

    RobotAvatarComponent::RobotAvatarComponent()
    {
        setOpaque(false);
        startTimerHz(30); // Para animación de decay + pulso de glow
    }


    // ═══════════════════════════════════════════════════════════════════════════
    //  visibilityChanged — Pausa timer cuando el plugin no es visible
    // ═══════════════════════════════════════════════════════════════════════════

    void RobotAvatarComponent::visibilityChanged()
    {
        if (isShowing() && !isTimerRunning()) {
            startTimerHz(30);
        }
        // Nunca detener el timer — el robot debe seguir animando sus
        // expresiones faciales aunque el componente no sea visible.
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Expression management
    // ═══════════════════════════════════════════════════════════════════════════

    void RobotAvatarComponent::setExpression(AvatarExpression newExpression)
    {
        if (currentExpression_ == newExpression) return;

        currentExpression_   = newExpression;
        expressionChangeMs_  = juce::Time::getMillisecondCounter();
        expressionIntensity_ = 0.0f; // Start from 0, animate to 1
        expressionDecayMs_   = 0;    // Reset decay timer
        repaint();
    }

    void RobotAvatarComponent::setExpressionWithDecay(AvatarExpression exp, int64_t decayMs)
    {
        setExpression(exp);
        expressionDecayMs_ = decayMs;

        // Si decayMs > 0, el timer se encarga de volver a Neutral después
        if (decayMs > 0) {
            // TimerCallback hará el check
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Wave animation
    // ═══════════════════════════════════════════════════════════════════════════

    void RobotAvatarComponent::setWaveActive(bool active, int64_t durationMs)
    {
        if (active) {
            waveActive_ = true;
            waveStartMs_ = juce::Time::getMillisecondCounter();
            waveDurationMs_ = durationMs;
            wavePhase_ = 0.0f;
        } else {
            waveActive_ = false;
            wavePhase_ = 0.0f;
        }
        repaint();
    }

    void RobotAvatarComponent::triggerNod(int64_t durationMs)
    {
        nodActive_ = true;
        nodStartMs_ = juce::Time::getMillisecondCounter();
        nodDurationMs_ = durationMs;
        nodPhase_ = 0.0f;
        // Set expression to Happy for the duration of the nod
        setExpressionWithDecay(AvatarExpression::Happy, durationMs);
        repaint();
    }

    void RobotAvatarComponent::expressionTimerCallback()
    {
        int64_t now = juce::Time::getMillisecondCounter();
        bool needsRepaint = false;

        // ═══ Animar intensidad de expresión (fade in) ════════════════════════
        if (expressionIntensity_ < 1.0f && expressionChangeMs_ > 0) {
            float elapsed = (float)(now - expressionChangeMs_);
            expressionIntensity_ = juce::jmin(1.0f, elapsed / 400.0f); // 400ms fade-in
            needsRepaint = true;
        }

        // ═══ Decay: volver a Neutral después de un tiempo ═══════════════════
        if (expressionDecayMs_ > 0) {
            int64_t elapsed = now - expressionChangeMs_;
            if (elapsed > expressionDecayMs_ && currentExpression_ != AvatarExpression::Neutral) {
                currentExpression_   = AvatarExpression::Neutral;
                expressionChangeMs_  = now;
                expressionIntensity_ = 0.0f;
                expressionDecayMs_   = 0;
                needsRepaint = true;
            }
        }

        // ═══ Wave animation decay ═══════════════════════════════════════════
        if (waveActive_) {
            int64_t elapsed = now - waveStartMs_;
            if (elapsed >= waveDurationMs_) {
                waveActive_ = false;
                wavePhase_ = 0.0f;
            } else {
                wavePhase_ = (float)elapsed / (float)waveDurationMs_;
            }
            needsRepaint = true;
        }

        // ═══ Nod animation decay ════════════════════════════════════════════
        if (nodActive_) {
            int64_t elapsed = now - nodStartMs_;
            if (elapsed >= nodDurationMs_) {
                nodActive_ = false;
                nodPhase_ = 0.0f;
            } else {
                nodPhase_ = (float)elapsed / (float)nodDurationMs_;
            }
            needsRepaint = true;
        }

        // ═══ Blink animation — parpadeo automático cada ~4s ═══════════════
        blinkCounter_++;
        if (blinkActive_) {
            blinkHoldFrames_++;
            if (blinkHoldFrames_ >= kBlinkDuration) {
                blinkActive_ = false;
                blinkHoldFrames_ = 0;
                blinkCounter_ = 0;
            }
        } else if (blinkCounter_ >= kBlinkInterval) {
            blinkActive_ = true;
            blinkHoldFrames_ = 0;
        }
        needsRepaint = true;

        // ═══ Talk animation — boca LED animada cuando talkActive_ ════════
        if (talkActive_) {
            talkPhase_ += 0.15f; // ~4.5 ciclos/segundo
            if (talkPhase_ > juce::MathConstants<float>::twoPi)
                talkPhase_ -= juce::MathConstants<float>::twoPi;
        }

        if (needsRepaint)
            repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  timerCallback — Llama al helper de expresión cada tick
    // ═══════════════════════════════════════════════════════════════════════════

    void RobotAvatarComponent::timerCallback()
    {
        expressionTimerCallback();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  renderToImage — Renderiza el avatar a un Image ARGB
    // ═══════════════════════════════════════════════════════════════════════════

    juce::Image RobotAvatarComponent::renderToImage(int width, int height)
    {
        juce::Image result(juce::Image::ARGB, width, height, true);
        juce::Graphics g(result);

        auto bounds = result.getBounds().toFloat();
        float talkPhase = talkActive_ ? talkPhase_ : 0.0f;
        drawAvatarStatic(g, bounds, currentExpression_, 1.0f, 1.0f, gazeX_, gazeY_,
                          wavePhase_, nodPhase_, thinkActive_, listenActive_,
                          audioActive_, audioLevel_, talkPhase);

        return result;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Entry point, delega a drawAvatarStatic
    // ═══════════════════════════════════════════════════════════════════════════

    void RobotAvatarComponent::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        float talkPhase = talkActive_ ? talkPhase_ : 0.0f;
        drawAvatarStatic(g, bounds, currentExpression_, 1.0f, expressionIntensity_,
                          gazeX_, gazeY_, wavePhase_, nodPhase_, thinkActive_, listenActive_,
                          audioActive_, audioLevel_, talkPhase);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawAvatarStatic — Método estático para dibujar el avatar en bounds dados
    //  Usado por ChatMessagesComponent::drawWelcomeAnimation() y por paint()
    // ═══════════════════════════════════════════════════════════════════════════

    void RobotAvatarComponent::drawAvatarStatic(juce::Graphics& g,
                                                 juce::Rectangle<float> bounds,
                                                 AvatarExpression expression,
                                                 float scale,
                                                 float glowIntensity,
                                                 float gazeX,
                                                 float gazeY,
                                                 float wavePhase,
                                                 float nodPhase,
                                                 bool thinkActive,
                                                 bool listenActive,
                                                 bool audioActive,
                                                 float audioLevel,
                                                 float talkPhase)
    {
        // ─── Si bounds es muy pequeño, no dibujar ───────────────────────────
        float minDim = juce::jmin(bounds.getWidth(), bounds.getHeight());
        if (minDim < 16.0f) return;

        // ═══ Idle float offset: suave bobbing sinusoidal ═══════════════════
        // Aplicamos el transform para que TODO el avatar flote uniformemente
        float floatTime = (float)(juce::Time::getMillisecondCounter() % 3000) / 3000.0f;
        float floatOffset = std::sin(floatTime * juce::MathConstants<float>::twoPi) * 4.0f; // ±4px

        g.saveState();
        g.addTransform(juce::AffineTransform::translation(0.0f, floatOffset));

        // ─── Normalizar a un cuadrado unitario y escalar ────────────────────
        float dim    = minDim;
        float cx     = bounds.getCentreX();
        float cy     = bounds.getCentreY();
        float originX = cx - dim * 0.5f;
        float originY = cy - dim * 0.5f;

        auto unit = juce::Rectangle<float>(originX, originY, dim, dim);
        float s  = dim / 100.0f; // Factor de escala (diseñado para 100x100)

        // ═══ Easing de intensidad (para transiciones suaves) ═══════════════
        float intensity = juce::jmin(1.0f, glowIntensity);

        // ═══ Glow de fondo según expresión ═══════════════════════════════════
        juce::Colour glowCol = expressionGlowColour(expression, 0.5f * intensity);

        // Outer glow ring
        float outerGlowR = dim * 0.55f;
        juce::ColourGradient outerGlow(glowCol.withAlpha(0.20f * intensity),
                                       cx, cy,
                                       juce::Colours::transparentBlack,
                                       cx + outerGlowR, cy, true);
        g.setGradientFill(outerGlow);
        g.fillEllipse(cx - outerGlowR, cy - outerGlowR, outerGlowR * 2.0f, outerGlowR * 2.0f);

        // Inner glow ring
        float innerGlowR = dim * 0.42f;
        juce::ColourGradient innerGlow(glowCol.withAlpha(0.35f * intensity),
                                       cx, cy,
                                       juce::Colours::transparentBlack,
                                       cx + innerGlowR, cy, true);
        g.setGradientFill(innerGlow);
        g.fillEllipse(cx - innerGlowR, cy - innerGlowR, innerGlowR * 2.0f, innerGlowR * 2.0f);

        // ═══ Shoulders ════════════════════════════════════════════════════════
        {
            float shoulderY = unit.getBottom() - 16.0f * s;
            float shoulderH = 12.0f * s;
            float shoulderW = 72.0f * s;
            float shoulderX = cx - shoulderW * 0.5f;

            // Left shoulder
            g.setColour(silverMetallic());
            g.fillRoundedRectangle(shoulderX - 4.0f * s, shoulderY, shoulderW * 0.48f, shoulderH, 3.0f * s);
            g.setColour(silverShadow());
            g.drawRoundedRectangle(shoulderX - 4.0f * s, shoulderY, shoulderW * 0.48f, shoulderH, 3.0f * s, 0.5f * s);

            // Right shoulder
            g.setColour(silverMetallic());
            g.fillRoundedRectangle(shoulderX + shoulderW * 0.52f, shoulderY, shoulderW * 0.48f, shoulderH, 3.0f * s);
            g.setColour(silverShadow());
            g.drawRoundedRectangle(shoulderX + shoulderW * 0.52f, shoulderY, shoulderW * 0.48f, shoulderH, 3.0f * s, 0.5f * s);
        }

        // ═══ Body ═════════════════════════════════════════════════════════════
        {
            float bodyTop    = unit.getY() + 44.0f * s;
            float bodyBottom = unit.getBottom() - 18.0f * s;
            float bodyW      = 52.0f * s;
            float bodyX      = cx - bodyW * 0.5f;

            auto bodyRect = juce::Rectangle<float>(bodyX, bodyTop, bodyW, bodyBottom - bodyTop);

            g.setColour(juce::Colours::black.withAlpha(0.20f));
            g.fillRoundedRectangle(bodyRect.expanded(2.0f * s, 2.0f * s), 6.0f * s);

            juce::ColourGradient bodyGrad(bodyWhite(),
                                          bodyX, bodyTop,
                                          bodyShadow(),
                                          bodyX, bodyBottom, false);
            g.setGradientFill(bodyGrad);
            g.fillRoundedRectangle(bodyRect, 6.0f * s);

            g.setColour(silverShadow().withAlpha(0.3f));
            g.drawRoundedRectangle(bodyRect, 6.0f * s, 0.5f * s);

            // Chest logo (purple wave icon)
            float logoSize = 14.0f * s;
            float logoX = cx - logoSize * 0.5f;
            float logoY = bodyRect.getCentreY() - logoSize * 0.5f;
            auto logoBounds = juce::Rectangle<float>(logoX, logoY, logoSize, logoSize);

            juce::Colour logoCol = expressionGlowColour(expression, 0.9f);
            g.setColour(logoCol);
            juce::Path wavePath;
            float lx = logoBounds.getX();
            float ly = logoBounds.getCentreY();
            float lw = logoBounds.getWidth();
            float lh = logoBounds.getHeight() * 0.3f;
            wavePath.startNewSubPath(lx, ly);
            wavePath.quadraticTo(lx + lw * 0.25f, ly - lh, lx + lw * 0.5f, ly);
            wavePath.quadraticTo(lx + lw * 0.75f, ly + lh, lx + lw, ly);
            g.strokePath(wavePath, juce::PathStrokeType(1.5f * s));
        }

        // ═══ Nod rotation center: between neck and head ═════════════════
        float nodPivotY = unit.getY() + 38.0f * s; // neck top = pivot

        // ═══ Neck ═════════════════════════════════════════════════════════════
        {
            float neckTop   = unit.getY() + 38.0f * s;
            float neckH     = 8.0f * s;
            float neckW     = 16.0f * s;
            auto neckRect   = juce::Rectangle<float>(cx - neckW * 0.5f, neckTop, neckW, neckH);
            g.setColour(jointBlack());
            g.fillRect(neckRect);
        }

        // ═══ Head (with nod rotation applied) ═══════════════════════════════
        g.saveState();
        {
            // Aplicar rotación de nod alrededor del pivote del cuello
            if (nodPhase > 0.0f && nodPhase < 1.0f) {
                float nodAngle = (nodPhase < 0.5f)
                    ? nodPhase * 2.0f * -12.0f  // bajar (0→0.5) → 0° a -12°
                    : (1.0f - nodPhase) * 2.0f * -12.0f; // subir (0.5→1.0) → -12° a 0°
                float nodRad = juce::degreesToRadians(nodAngle);
                g.addTransform(juce::AffineTransform::rotation(nodRad, cx, nodPivotY));
            }

            float headTop    = unit.getY() + 4.0f * s;
            float headH      = 36.0f * s;
            float headW      = 46.0f * s;
            float headX      = cx - headW * 0.5f;
            auto headRect    = juce::Rectangle<float>(headX, headTop, headW, headH);

            // Enhanced shadow with softer edges
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            g.fillRoundedRectangle(headRect.expanded(2.5f * s, 3.0f * s), 10.0f * s);

            // Multi-stop gradient for metallic 3D effect
            juce::ColourGradient headGrad(silverLight(),
                                          headX, headTop,
                                          juce::Colour(0xFF8A92A0),
                                          headX, headRect.getBottom(), false);
            headGrad.addColour(0.3f, juce::Colour(0xFFD0D6DD));
            headGrad.addColour(0.7f, juce::Colour(0xFF9EA7B5));
            g.setGradientFill(headGrad);
            g.fillRoundedRectangle(headRect, 10.0f * s);

            // Top highlight for metallic sheen
            auto highlightRect = headRect.withHeight(headH * 0.25f);
            juce::ColourGradient highlightGrad(juce::Colours::white.withAlpha(0.15f),
                                               highlightRect.getX(), highlightRect.getY(),
                                               juce::Colours::transparentBlack,
                                               highlightRect.getX(), highlightRect.getBottom(), false);
            g.setGradientFill(highlightGrad);
            g.fillRoundedRectangle(highlightRect, 10.0f * s);

            // Subtle border with metallic edge
            g.setColour(juce::Colour(0xFF5A6270).withAlpha(0.5f));
            g.drawRoundedRectangle(headRect, 10.0f * s, 1.2f * s);

            // Add mechanical panel lines for realism
            g.setColour(juce::Colour(0xFF4A5260).withAlpha(0.3f));
            float panelY = headTop + headH * 0.65f;
            g.drawLine(headX + 6.0f * s, panelY, headX + headW - 6.0f * s, panelY, 0.8f * s);

            // Small screw details at corners
            float screwR = 1.2f * s;
            juce::Colour screwCol = juce::Colour(0xFF3A4250);
            g.setColour(screwCol);
            g.fillEllipse(headX + 8.0f * s - screwR, headTop + 8.0f * s - screwR, screwR * 2.0f, screwR * 2.0f);
            g.fillEllipse(headX + headW - 8.0f * s - screwR, headTop + 8.0f * s - screwR, screwR * 2.0f, screwR * 2.0f);
            g.fillEllipse(headX + 8.0f * s - screwR, headTop + headH - 8.0f * s - screwR, screwR * 2.0f, screwR * 2.0f);
            g.fillEllipse(headX + headW - 8.0f * s - screwR, headTop + headH - 8.0f * s - screwR, screwR * 2.0f, screwR * 2.0f);

            // ═══ Visor ═══════════════════════════════════════════════════════
            float visorTop    = headTop + 8.0f * s;
            float visorH      = 14.0f * s;
            float visorW      = 38.0f * s;
            float visorX      = cx - visorW * 0.5f;
            auto visorRect    = juce::Rectangle<float>(visorX, visorTop, visorW, visorH);

            // Visor shadow for depth
            g.setColour(juce::Colours::black.withAlpha(0.4f));
            g.fillRoundedRectangle(visorRect.expanded(1.0f * s, 1.5f * s), 5.0f * s);

            // Deep black visor background with subtle gradient
            juce::ColourGradient visorGrad(juce::Colour(0xFF0A0A12),
                                           visorX, visorTop,
                                           juce::Colour(0xFF151520),
                                           visorX, visorRect.getBottom(), false);
            g.setGradientFill(visorGrad);
            g.fillRoundedRectangle(visorRect, 4.0f * s);

            // Enhanced glass reflection with multiple layers
            auto glassReflect = visorRect.withHeight(visorH * 0.5f);
            juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.12f),
                                           glassReflect.getX(), glassReflect.getY(),
                                           juce::Colours::transparentBlack,
                                           glassReflect.getX(), glassReflect.getBottom(), false);
            g.setGradientFill(glassGrad);
            g.fillRoundedRectangle(glassReflect, 4.0f * s);

            // Secondary highlight for glass curvature
            auto highlightSpot = visorRect.withTrimmedLeft(visorW * 0.6f).withTrimmedBottom(visorH * 0.6f).withWidth(visorW * 0.25f);
            juce::ColourGradient spotGrad(juce::Colours::white.withAlpha(0.06f),
                                          highlightSpot.getX(), highlightSpot.getY(),
                                          juce::Colours::transparentBlack,
                                          highlightSpot.getRight(), highlightSpot.getBottom(), true);
            g.setGradientFill(spotGrad);
            g.fillRoundedRectangle(highlightSpot, 2.0f * s);

            // Visor bezel/frame for realism
            g.setColour(juce::Colour(0xFF2A2A35).withAlpha(0.7f));
            g.drawRoundedRectangle(visorRect, 4.0f * s, 1.5f * s);

            // Inner glow for active display effect
            juce::ColourGradient innerGlow(expressionGlowColour(expression, 0.15f),
                                          visorRect.getCentreX(), visorRect.getCentreY(),
                                          juce::Colours::transparentBlack,
                                          visorRect.getCentreX() + visorW * 0.4f, visorRect.getCentreY(), true);
            g.setGradientFill(innerGlow);
            g.fillRoundedRectangle(visorRect.reduced(2.0f * s), 3.0f * s);

            // ═══ Eyes (inside visor) ═════════════════════════════════════════
            float eyeY     = visorTop + 4.0f * s;
            float eyeAreaH = visorH - 8.0f * s;
            auto eyeBounds = juce::Rectangle<float>(visorX + 4.0f * s, eyeY, visorW - 8.0f * s, eyeAreaH);
            drawEyes(g, eyeBounds, expression, gazeX, gazeY);

            // ═══ Mouth ═══════════════════════════════════════════════════════
            float mouthTop = visorRect.getBottom() + 3.0f * s;
            float mouthH   = 8.0f * s;
            float mouthW   = 22.0f * s;
            float mouthX   = cx - mouthW * 0.5f;
            auto mouthBounds = juce::Rectangle<float>(mouthX, mouthTop, mouthW, mouthH);
            drawMouth(g, mouthBounds, expression, talkPhase);
        }
        g.restoreState();

        // ═══ LED indicators (headphone ear lights) + antenna light ═════
        // These are drawn SEPARATELY outside the saveState/restoreState so they
        // are NOT affected by the nod rotation (LEDs on the ears stay in place)
        {
            float headTop = unit.getY() + 4.0f * s;
            float headH   = 36.0f * s;
            float headW   = 46.0f * s;

            // LED dots on the headphones: small circles at ear level
            // Pulse continuo (idle) + se intensifica con think/listen
            float ledY = headTop + headH * 0.5f;
            float ledR = 2.5f * s;
            float ledSpacing = headW * 0.5f + 10.0f * s;

            // ─── Ear pulse: siempre visible, varía según estado ────────────
            float pulseTime = (float)(juce::Time::getMillisecondCounter() % 3000) / 3000.0f;
            float idlePulse = 0.2f + 0.15f * std::sin(pulseTime * juce::MathConstants<float>::twoPi);

            juce::Colour ledColour;
            float ledAlpha;

            if (thinkActive) {
                // Thinking: blinking LED (fast pulse on/off)
                ledAlpha = 0.5f + 0.5f * std::sin(juce::Time::getMillisecondCounter() * 0.008f);
                ledColour = juce::Colour(0xFF448AFF); // Blue
            } else if (listenActive) {
                // Listening: constant warm glow with subtle pulse
                ledAlpha = 0.75f + 0.10f * std::sin(pulseTime * juce::MathConstants<float>::twoPi);
                ledColour = juce::Colour(0xFF00E5A0); // Teal-green
            } else {
                // Idle: pulso suave púrpura
                ledAlpha = idlePulse;
                ledColour = purpleGlow();
            }

            // Left ear LED
            {
                juce::ColourGradient leftGlow(ledColour.withAlpha(ledAlpha * 0.3f),
                                              cx - ledSpacing, ledY,
                                              juce::Colours::transparentBlack,
                                              cx - ledSpacing + ledR * 3.0f, ledY, true);
                g.setGradientFill(leftGlow);
                g.fillEllipse(cx - ledSpacing - ledR * 2.0f, ledY - ledR * 2.0f, ledR * 4.0f, ledR * 4.0f);
                g.setColour(ledColour.withAlpha(ledAlpha));
                g.fillEllipse(cx - ledSpacing - ledR, ledY - ledR, ledR * 2.0f, ledR * 2.0f);
            }

            // Right ear LED
            {
                juce::ColourGradient rightGlow(ledColour.withAlpha(ledAlpha * 0.3f),
                                               cx + ledSpacing, ledY,
                                               juce::Colours::transparentBlack,
                                               cx + ledSpacing - ledR * 3.0f, ledY, true);
                g.setGradientFill(rightGlow);
                g.fillEllipse(cx + ledSpacing - ledR * 2.0f, ledY - ledR * 2.0f, ledR * 4.0f, ledR * 4.0f);
                g.setColour(ledColour.withAlpha(ledAlpha));
                g.fillEllipse(cx + ledSpacing - ledR, ledY - ledR, ledR * 2.0f, ledR * 2.0f);
            }

            // ═══ Antenna light: pulso continuo (más brillante con think) ═══
            float antennaAlpha;
            juce::Colour antennaColour;
            if (thinkActive) {
                antennaAlpha = 0.6f + 0.4f * std::sin(juce::Time::getMillisecondCounter() * 0.01f);
                antennaColour = juce::Colour(0xFF448AFF);
            } else if (listenActive) {
                antennaAlpha = 0.7f + 0.3f * std::sin(pulseTime * juce::MathConstants<float>::twoPi * 0.7f);
                antennaColour = juce::Colour(0xFF00E5A0);
            } else {
                antennaAlpha = 0.3f + 0.2f * std::sin(pulseTime * juce::MathConstants<float>::twoPi * 0.5f);
                antennaColour = purpleGlow();
            }

            float antennaX = cx;
            float antennaY = unit.getY() - 2.0f * s;
            float antennaR = 3.0f * s;
            // Antenna glow
            juce::ColourGradient antGlow(antennaColour.withAlpha(antennaAlpha * 0.2f),
                                         antennaX, antennaY,
                                         juce::Colours::transparentBlack,
                                         antennaX + antennaR * 4.0f, antennaY, true);
            g.setGradientFill(antGlow);
            g.fillEllipse(antennaX - antennaR * 3.0f, antennaY - antennaR * 3.0f,
                          antennaR * 6.0f, antennaR * 6.0f);
            g.setColour(antennaColour.withAlpha(antennaAlpha));
            g.fillEllipse(antennaX - antennaR, antennaY - antennaR,
                          antennaR * 2.0f, antennaR * 2.0f);
        }

        // ═══ Headphones ═══════════════════════════════════════════════════════
        {
            float headTop    = unit.getY() + 4.0f * s;
            float headH      = 36.0f * s;
            float headW      = 46.0f * s;
            float hpArcW     = headW + 14.0f * s;

            // Left headphone arc
            juce::Path leftHp;
            leftHp.startNewSubPath(cx - hpArcW * 0.5f, headTop + 6.0f * s);
            leftHp.quadraticTo(cx - hpArcW * 0.5f - 4.0f * s, headTop + headH * 0.5f,
                               cx - hpArcW * 0.5f, headTop + headH - 4.0f * s);
            g.setColour(headphoneBlack());
            g.strokePath(leftHp, juce::PathStrokeType(3.0f * s));

            // Right headphone arc
            juce::Path rightHp;
            rightHp.startNewSubPath(cx + hpArcW * 0.5f, headTop + 6.0f * s);
            rightHp.quadraticTo(cx + hpArcW * 0.5f + 4.0f * s, headTop + headH * 0.5f,
                                cx + hpArcW * 0.5f, headTop + headH - 4.0f * s);
            g.setColour(headphoneBlack());
            g.strokePath(rightHp, juce::PathStrokeType(3.0f * s));

            // Headband arc across top
            juce::Path headband;
            headband.startNewSubPath(cx - hpArcW * 0.5f, headTop + 2.0f * s);
            headband.quadraticTo(cx, headTop - 10.0f * s,
                                 cx + hpArcW * 0.5f, headTop + 2.0f * s);
            g.setColour(headphoneMatte());
            g.strokePath(headband, juce::PathStrokeType(3.5f * s));

            // ═══ Audio wave bars from headphone earcups ═══════════════════
            if (audioActive) {
                float waveTime = (float)(juce::Time::getMillisecondCounter() % 2000) / 2000.0f;
                int numBars = 4;
                float barSpacing = 4.0f * s;
                float maxBarLen = 14.0f * s * audioLevel;
                float barWidth = 2.0f * s;
                float pad = 2.0f * s;
                float centerY = headTop + headH * 0.5f;

                // Wave color: purple glow matching expression
                juce::Colour waveCol = expressionGlowColour(expression, 0.5f);

                auto drawWaveBars = [&](float legCenterX) {
                    for (int i = 0; i < numBars; ++i) {
                        float barPhase = (float)i / (float)numBars;
                        float barAnim = std::sin((waveTime * juce::MathConstants<float>::twoPi * 2.0f
                                                  + barPhase * juce::MathConstants<float>::pi) * 0.5f);
                        float barLen = maxBarLen * (0.3f + 0.7f * (0.5f + 0.5f * barAnim));
                        float barX = legCenterX + pad + (float)i * (barWidth + barSpacing);
                        g.setColour(waveCol.withAlpha(0.6f - 0.1f * (float)i));
                        g.fillRoundedRectangle(barX, centerY - barLen * 0.5f, barWidth, barLen, barWidth * 0.4f);
                    }
                };

                // Left ear waves (emanating outward to the left)
                drawWaveBars(cx - hpArcW * 0.5f - 4.0f * s);
                // Right ear waves (emanating outward to the right)
                drawWaveBars(cx + hpArcW * 0.5f + 2.0f * s);
            }
        }

        // ═══ Celebración: bounce vertical + scale ═════════════════════════
        if (expression == AvatarExpression::Celebrating) {
            float celebrateTime = (float)(juce::Time::getMillisecondCounter() % 1200) / 1200.0f;
            // Physical bounce: sube 15px, escala 1.05, con ease-out
            float bounceY = -std::abs(std::sin(celebrateTime * juce::MathConstants<float>::pi * 3.0f)) * 15.0f * s;
            float celebrateScale = 1.0f + 0.05f * std::sin(celebrateTime * juce::MathConstants<float>::twoPi * 3.0f);
            g.addTransform(juce::AffineTransform::translation(0.0f, bounceY)
                                              .scaled(celebrateScale, celebrateScale, cx, cy));

            // Emerald glow ring pulsante
            float pulseGlow = 0.1f + 0.08f * std::sin(celebrateTime * juce::MathConstants<float>::twoPi * 3.0f);
            float celebrateR = dim * 0.65f;
            juce::Colour celebrateGlow = juce::Colour(0xFF22D3A7).withAlpha(pulseGlow * intensity);
            juce::ColourGradient celebrateGrad(celebrateGlow,
                                               cx, cy,
                                               juce::Colours::transparentBlack,
                                               cx + celebrateR, cy, true);
            g.setGradientFill(celebrateGrad);
            g.fillEllipse(cx - celebrateR, cy - celebrateR, celebrateR * 2.0f, celebrateR * 2.0f);
        }

        // ═══ Waving arm (drawn on top of everything when wavePhase > 0) ══════
        if (wavePhase > 0.0f && wavePhase <= 1.0f) {
            drawWavingArm(g, unit, s, cx, cy, wavePhase);
        }

        g.restoreState(); // Restore float offset transform
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawEyes — Dibuja los ojos según la expresión
    // ═══════════════════════════════════════════════════════════════════════════

    void RobotAvatarComponent::drawEyes(juce::Graphics& g,
                                        juce::Rectangle<float> bounds,
                                        AvatarExpression expression,
                                        float gazeX,
                                        float gazeY)
    {
        float cx     = bounds.getCentreX();
        float cy     = bounds.getCentreY();
        float eyeW   = 6.0f;
        float eyeH   = 7.0f;
        float spacing = 12.0f;
        float baseY  = cy;
        float s      = bounds.getHeight() / 8.0f; // Scale relative to eye area

        // ═══ Blink detection: parpadeo automático cada ~4s ═════════════════
        // Usamos el tiempo global para que todas las instancias parpadeen juntas
        int64_t now = juce::Time::getMillisecondCounter();
        int blinkCycle = (int)(now / 4000);        // Ciclo de 4 segundos
        int blinkMs = (int)(now % 4000);            // Posición dentro del ciclo
        bool isBlinking = (blinkMs >= 3900 && blinkMs < 4030); // Últimos 100ms del ciclo

        // ═══ Auto pupil wander: micro-movimientos naturales ═══════════════
        float autoGazeTime = now * 0.0008f; // Lenta oscilación
        float autoGazeX = std::sin(autoGazeTime * 0.5f) * 0.15f;
        float autoGazeY = std::cos(autoGazeTime * 0.7f) * 0.10f;

        // ═══ Gaze offset combinado: mirada explícita + micro-movimientos ═══
        float combinedGazeX = gazeX + autoGazeX;
        float combinedGazeY = gazeY + autoGazeY;
        combinedGazeX = juce::jlimit(-1.0f, 1.0f, combinedGazeX);
        combinedGazeY = juce::jlimit(-1.0f, 1.0f, combinedGazeY);

        float gazeOffX = combinedGazeX * eyeW * 0.35f;
        float gazeOffY = combinedGazeY * eyeH * 0.35f;

        // ═══ Calcular ojos izquierdo y derecho según expresión ═══════════════
        float leftEyeX  = cx - spacing * 0.5f - eyeW * 0.5f;
        float rightEyeX = cx + spacing * 0.5f - eyeW * 0.5f;

        // Colores mejorados para realismo
        juce::Colour eyeCore   = juce::Colour(0xFF9B59B6).withAlpha(0.95f);  // Purple más rico
        juce::Colour eyeGlow   = juce::Colour(0xFF8E44AD).withAlpha(0.4f);   // Glow más intenso
        juce::Colour eyePupil  = juce::Colours::white.withAlpha(0.85f);       // Pupil más brillante
        juce::Colour eyeInner  = juce::Colour(0xFF6C3483).withAlpha(0.6f);   // Inner shadow
        juce::Colour expressionGlow = expressionGlowColour(expression, 0.25f);

        // ═══ Blink: reemplazar toda la expresión por ojos cerrados ═════════
        if (isBlinking) {
            // Ojos cerrados: dos líneas horizontales
            for (int side = 0; side < 2; ++side) {
                float ex = (side == 0) ? leftEyeX : rightEyeX;
                float ey = baseY;
                // Glow sutil
                g.setColour(expressionGlow);
                g.fillEllipse(ex - 2.0f, ey - eyeH * 0.5f - 2.0f, eyeW + 4.0f, eyeH + 4.0f);
                // Línea horizontal
                g.setColour(eyeCore);
                g.fillRect(ex, ey - 1.0f, eyeW, 2.0f);
            }
            return; // No dibujar la expresión normal
        }

        switch (expression) {


            case AvatarExpression::Celebrating:
            case AvatarExpression::Happy: {
                // ^ ^ — Arco feliz (Celebrating = mismo que Happy pero más intenso)
                float happyOffset = 1.5f;
                for (int side = 0; side < 2; ++side) {
                    float ex = (side == 0) ? leftEyeX : rightEyeX;
                    // Eye glow
                    g.setColour(expressionGlow);
                    g.fillEllipse(ex - 2.0f, baseY - eyeH + happyOffset - 2.0f, eyeW + 4.0f, eyeH + 4.0f);
                    // Eye arc (smiling shape)
                    g.setColour(eyeCore);
                    juce::Path happyEye;
                    happyEye.startNewSubPath(ex, baseY + happyOffset);
                    happyEye.quadraticTo(ex + eyeW * 0.5f, baseY - eyeH + happyOffset,
                                         ex + eyeW, baseY + happyOffset);
                    g.strokePath(happyEye, juce::PathStrokeType(1.8f * s));
                    // Small gleam (con gaze)
                    g.setColour(eyePupil.withAlpha(0.6f));
                    g.fillEllipse(ex + eyeW * 0.3f + gazeOffX, baseY - eyeH * 0.4f + happyOffset + gazeOffY, 2.0f * s, 2.0f * s);
                }
                break;
            }

            case AvatarExpression::Surprised: {
                // O O — Ojos muy abiertos
                float surpriseScale = 1.4f;
                float sw = eyeW * surpriseScale;
                float sh = eyeH * surpriseScale;
                for (int side = 0; side < 2; ++side) {
                    float ex = (side == 0) ? leftEyeX - (sw - eyeW) * 0.5f : rightEyeX - (sw - eyeW) * 0.5f;
                    float ey = baseY - sh * 0.5f;
                    // Big glow
                    g.setColour(expressionGlow);
                    g.fillEllipse(ex - 3.0f, ey - 3.0f, sw + 6.0f, sh + 6.0f);
                    // Eye circle
                    g.setColour(eyeCore);
                    g.fillEllipse(ex, ey, sw, sh);
                    // Pupil (con gaze)
                    g.setColour(eyePupil);
                    g.fillEllipse(ex + sw * 0.25f + gazeOffX, ey + sh * 0.25f + gazeOffY, sw * 0.5f, sh * 0.5f);
                }
                break;
            }

            case AvatarExpression::Thinking: {
                // > < — Ojos mirando hacia arriba, uno ligeramente entrecerrado
                for (int side = 0; side < 2; ++side) {
                    float ex = (side == 0) ? leftEyeX : rightEyeX;
                    float scaleX = (side == 0) ? 0.8f : 1.0f; // Left eye slightly squinted
                    float ew = eyeW * scaleX;
                    float eh = eyeH;
                    float offsetX = (side == 0) ? (eyeW - ew) * 0.5f : 0.0f;

                    // Glow
                    g.setColour(expressionGlow);
                    g.fillEllipse(ex - 2.0f + offsetX, baseY - eh * 0.5f - 2.0f, ew + 4.0f, eh + 4.0f);
                    // Eye
                    g.setColour(eyeCore);
                    g.fillEllipse(ex + offsetX, baseY - eh * 0.5f, ew, eh);
                    // Pupil shifted up and to the side
                    g.setColour(eyePupil);
                    float pupilOffsetX = (side == 0) ? -1.0f : 1.0f;
                    g.fillEllipse(ex + ew * 0.3f + pupilOffsetX * s + gazeOffX, baseY - eh * 0.4f + gazeOffY, ew * 0.4f, eh * 0.35f);
                }
                break;
            }

            case AvatarExpression::Encouraging: {
                // Warm smile eyes — slightly arched, warm glow
                for (int side = 0; side < 2; ++side) {
                    float ex = (side == 0) ? leftEyeX : rightEyeX;
                    g.setColour(expressionGlow);
                    g.fillEllipse(ex - 3.0f, baseY - eyeH * 0.5f - 3.0f, eyeW + 6.0f, eyeH + 6.0f);
                    // Soft arc
                    g.setColour(eyeCore);
                    juce::Path warmEye;
                    warmEye.startNewSubPath(ex, baseY);
                    warmEye.quadraticTo(ex + eyeW * 0.5f, baseY - eyeH * 0.7f,
                                        ex + eyeW, baseY);
                    g.strokePath(warmEye, juce::PathStrokeType(2.0f * s));
                    // Sparkle (con gaze)
                    g.setColour(juce::Colours::white.withAlpha(0.7f));
                    g.fillEllipse(ex + eyeW * 0.35f + gazeOffX, baseY - eyeH * 0.5f + gazeOffY, 2.5f * s, 2.5f * s);
                }
                break;
            }

            case AvatarExpression::Serious: {
                // __ — Líneas rectas, ligeramente inclinadas hacia abajo
                for (int side = 0; side < 2; ++side) {
                    float ex = (side == 0) ? leftEyeX : rightEyeX;
                    float tilt = (side == 0) ? 1.0f : -1.0f; // Slight inward tilt
                    g.setColour(expressionGlow);
                    g.fillEllipse(ex - 2.0f, baseY - eyeH * 0.5f - 2.0f, eyeW + 4.0f, eyeH + 4.0f);
                    // Straight line with slight downward angle
                    g.setColour(eyeCore);
                    juce::Path seriousEye;
                    seriousEye.startNewSubPath(ex, baseY + tilt * 0.5f);
                    seriousEye.lineTo(ex + eyeW, baseY - tilt * 0.5f);
                    g.strokePath(seriousEye, juce::PathStrokeType(2.0f * s));
                }
                break;
            }

            case AvatarExpression::Neutral:
            default: {
                // ● ● — Círculos mejorados con efectos 3D
                for (int side = 0; side < 2; ++side) {
                    float ex = (side == 0) ? leftEyeX : rightEyeX;
                    float ey = baseY - eyeH * 0.5f;
                    
                    // Outer glow layer
                    g.setColour(expressionGlow);
                    g.fillEllipse(ex - 4.0f, ey - 4.0f, eyeW + 8.0f, eyeH + 8.0f);
                    
                    // Inner glow for depth
                    juce::ColourGradient innerGlow(eyeGlow, ex + eyeW * 0.5f, ey + eyeH * 0.5f,
                                                  juce::Colours::transparentBlack,
                                                  ex + eyeW * 0.5f, ey + eyeH * 0.5f + eyeH * 0.5f, true);
                    g.setGradientFill(innerGlow);
                    g.fillEllipse(ex - 2.0f, ey - 2.0f, eyeW + 4.0f, eyeH + 4.0f);
                    
                    // Eye core with gradient for 3D sphere effect
                    juce::ColourGradient eyeGrad(eyeCore,
                                                  ex, ey - eyeH * 0.3f,
                                                  eyeInner,
                                                  ex, ey + eyeH * 0.7f, false);
                    g.setGradientFill(eyeGrad);
                    g.fillEllipse(ex, ey, eyeW, eyeH);
                    
                    // Top highlight for glossy effect
                    g.setColour(juce::Colours::white.withAlpha(0.4f));
                    g.fillEllipse(ex + eyeW * 0.2f, ey - eyeH * 0.2f, eyeW * 0.3f, eyeH * 0.25f);
                    
                    // Pupil con gaze y efecto de profundidad
                    g.setColour(eyePupil);
                    g.fillEllipse(ex + eyeW * 0.3f + gazeOffX, ey + eyeH * 0.25f + gazeOffY, eyeW * 0.4f, eyeH * 0.5f);
                }
                break;
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawMouth — Dibuja la boca según la expresión
    // ═══════════════════════════════════════════════════════════════════════════

    void RobotAvatarComponent::drawMouth(juce::Graphics& g,
                                         juce::Rectangle<float> bounds,
                                         AvatarExpression expression,
                                         float talkPhase)
    {
        float cx    = bounds.getCentreX();
        float cy    = bounds.getCentreY();
        float w     = bounds.getWidth();
        float h     = bounds.getHeight();
        float s     = h / 8.0f;
        juce::Colour mouthColour = purpleEye().withAlpha(0.85f);

        // ═══ Talk animation: animación de boca LED (pantalla LED 4×2) ═══
        if (talkPhase > 0.0f) {
            // Boca tipo LED grid animado: 3 columnas, 2 filas
            // Cada LED se ilumina según la fase de talk
            float ledW = 5.0f * s;
            float ledH = 3.0f * s;
            float gapX = 2.0f * s;
            float gapY = 2.0f * s;
            float totalW = 3.0f * ledW + 2.0f * gapX;
            float startX = cx - totalW * 0.5f;

            // La iluminación de los LEDs varía con sin(talkPhase), creando
            // un patrón de "boca hablando"
            for (int row = 0; row < 2; ++row) {
                for (int col = 0; col < 3; ++col) {
                    float ledAlpha = 0.2f + 0.8f * (0.5f + 0.5f * std::sin(
                        talkPhase + row * 1.5f + col * 2.0f));
                    // Los LEDs centrales parpadean más que los laterales
                    float intensity = (col == 1) ? 0.9f : 0.6f;
                    float alpha = ledAlpha * intensity;

                    g.setColour(mouthColour.withAlpha(alpha));
                    g.fillRoundedRectangle(
                        startX + (float)col * (ledW + gapX),
                        cy + (float)row * (ledH + gapY) - ledH,
                        ledW, ledH, 1.0f);
                }
            }
            return;
        }

        switch (expression) {
            case AvatarExpression::Celebrating:
            case AvatarExpression::Happy: {
                // Smile arc (Celebrating = big happy smile)
                g.setColour(mouthColour);
                juce::Path smile;
                smile.startNewSubPath(cx - w * 0.4f, cy + 1.0f);
                smile.quadraticTo(cx, cy + h * 0.6f,
                                  cx + w * 0.4f, cy + 1.0f);
                g.strokePath(smile, juce::PathStrokeType(2.0f * s));
                break;
            }

            case AvatarExpression::Surprised: {
                // O shape
                float mouthR = juce::jmin(w, h) * 0.3f;
                g.setColour(visorBlack());
                g.fillEllipse(cx - mouthR, cy - mouthR * 0.5f, mouthR * 2.0f, mouthR * 1.5f);
                g.setColour(mouthColour);
                g.drawEllipse(cx - mouthR, cy - mouthR * 0.5f, mouthR * 2.0f, mouthR * 1.5f, 1.5f * s);
                break;
            }

            case AvatarExpression::Thinking: {
                // Slight smirk — one side up
                g.setColour(mouthColour);
                juce::Path smirk;
                smirk.startNewSubPath(cx - w * 0.4f, cy + 1.0f);
                smirk.quadraticTo(cx, cy - h * 0.2f,
                                  cx + w * 0.4f, cy - h * 0.3f);
                g.strokePath(smirk, juce::PathStrokeType(2.0f * s));
                break;
            }

            case AvatarExpression::Encouraging: {
                // Warm wide smile
                g.setColour(mouthColour.brighter(0.3f));
                juce::Path warmSmile;
                warmSmile.startNewSubPath(cx - w * 0.4f, cy + 2.0f);
                warmSmile.quadraticTo(cx, cy + h * 0.5f,
                                      cx + w * 0.4f, cy + 2.0f);
                g.strokePath(warmSmile, juce::PathStrokeType(2.5f * s));
                // Small glow dot in center
                g.setColour(juce::Colours::white.withAlpha(0.3f));
                g.fillEllipse(cx - 1.5f * s, cy + h * 0.2f, 3.0f * s, 2.0f * s);
                break;
            }

            case AvatarExpression::Serious: {
                // Straight line / slight frown
                g.setColour(mouthColour.withAlpha(0.6f));
                juce::Path frown;
                frown.startNewSubPath(cx - w * 0.35f, cy + 1.0f);
                frown.quadraticTo(cx, cy - h * 0.1f,
                                  cx + w * 0.35f, cy + 1.0f);
                g.strokePath(frown, juce::PathStrokeType(2.0f * s));
                break;
            }

            case AvatarExpression::Neutral:
            default: {
                // Small gentle curve
                g.setColour(mouthColour);
                juce::Path neutralMouth;
                neutralMouth.startNewSubPath(cx - w * 0.3f, cy + 1.0f);
                neutralMouth.quadraticTo(cx, cy + h * 0.25f,
                                         cx + w * 0.3f, cy + 1.0f);
                g.strokePath(neutralMouth, juce::PathStrokeType(1.8f * s));
                break;
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawBackground — Degradado de fondo (implementación legacy, no usada)
    // ═══════════════════════════════════════════════════════════════════════════

    void RobotAvatarComponent::drawBackground(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        juce::ignoreUnused(g, bounds);
        // Background is now handled by drawAvatarStatic
    }

    void RobotAvatarComponent::drawHeadphones(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        juce::ignoreUnused(g, bounds);
        // Headphones handled in drawAvatarStatic
    }

    void RobotAvatarComponent::drawNeckAndBody(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        juce::ignoreUnused(g, bounds);
        // Neck and body handled in drawAvatarStatic
    }

    void RobotAvatarComponent::drawHead(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        juce::ignoreUnused(g, bounds);
        // Head handled in drawAvatarStatic
    }

    void RobotAvatarComponent::drawVisor(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        juce::ignoreUnused(g, bounds);
        // Visor handled in drawAvatarStatic
    }

    void RobotAvatarComponent::drawChestLogo(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        juce::ignoreUnused(g, bounds);
        // Chest logo handled in drawAvatarStatic
    }

    void RobotAvatarComponent::drawShoulders(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        juce::ignoreUnused(g, bounds);
        // Shoulders handled in drawAvatarStatic
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawWavingArm — Dibuja el brazo wave animado
    //  El brazo derecho se extiende desde el hombro derecho y oscila
    //  entre 20° y 130° para simular un saludo.
    // ═══════════════════════════════════════════════════════════════════════════

    void RobotAvatarComponent::drawWavingArm(juce::Graphics& g,
                                              juce::Rectangle<float> bounds,
                                              float s,
                                              float cx,
                                              float /*cy*/,
                                              float wavePhase)
    {
        juce::ignoreUnused(bounds);

        if (wavePhase <= 0.0f || wavePhase > 1.0f) return;

        // ═══ Calcular ángulo del brazo ═══════════════════════════════════════
        // Oscila entre 20° (casi abajo) y 130° (arriba) usando una
        // frecuencia de ~3 oscilaciones completas por ciclo de wave.
        constexpr float kMinAngleRad = juce::MathConstants<float>::pi * 20.0f / 180.0f;
        constexpr float kMaxAngleRad = juce::MathConstants<float>::pi * 130.0f / 180.0f;
        float swing = std::sin(wavePhase * juce::MathConstants<float>::twoPi * 3.0f);
        float angleRad = kMinAngleRad + (kMaxAngleRad - kMinAngleRad) * (0.5f + 0.5f * swing);

        // Ease-in/out para los extremos (más natural)
        float easeInOut = 1.0f - std::abs(swing) * 0.3f;

        // ═══ Punto de pivote: hombro derecho ══════════════════════════════════
        float shoulderY = bounds.getBottom() - 16.0f * s;
        float shoulderW = 72.0f * s;
        float pivotX = cx + shoulderW * 0.5f - 2.0f * s;  // Borde derecho del hombro
        float pivotY = shoulderY + 4.0f * s;               // Centro del hombro

        // ═══ Longitud del brazo ═══════════════════════════════════════════════
        float armLen = 28.0f * s;
        float armWidth = 5.0f * s;

        // ═══ Calcular extremo del brazo (antebrazo + mano) ═══════════════════
        // El ángulo se mide desde la vertical hacia arriba
        float dirX = std::sin(angleRad);
        float dirY = -std::cos(angleRad);  // Negativo = hacia arriba

        float elbowX = pivotX + dirX * armLen * 0.6f;
        float elbowY = pivotY + dirY * armLen * 0.6f;
        float handX = pivotX + dirX * armLen;
        float handY = pivotY + dirY * armLen;

        // ═══ Color del brazo (plateado metálico como los hombros) ════════════
        juce::Colour armColour = silverMetallic();
        juce::Colour armShadow = silverShadow();
        float alpha = 0.85f + 0.15f * easeInOut;

        g.saveState();

        // ═══ Sombra del brazo ════════════════════════════════════════════════
        g.setColour(juce::Colours::black.withAlpha(0.20f * alpha));
        juce::Path shadowPath;
        shadowPath.startNewSubPath(pivotX + 1.5f * s, pivotY + 1.5f * s);
        shadowPath.lineTo(elbowX + 1.5f * s, elbowY + 1.5f * s);
        shadowPath.lineTo(handX + 2.0f * s, handY + 2.0f * s);
        g.strokePath(shadowPath, juce::PathStrokeType(armWidth + 2.0f * s,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

        // ═══ Brazo superior (hombro → codo): rectángulo redondeado ═══════════
        {
            float upperLen = std::sqrt((elbowX - pivotX) * (elbowX - pivotX)
                                      + (elbowY - pivotY) * (elbowY - pivotY));
            float upperAngle = std::atan2(elbowY - pivotY, elbowX - pivotX);

            g.setColour(armColour.withAlpha(alpha));
            juce::Path upperArm;
            upperArm.addRoundedRectangle(0, -armWidth * 0.5f,
                                          upperLen, armWidth,
                                          armWidth * 0.4f);
            g.fillPath(upperArm, juce::AffineTransform::rotation(upperAngle, 0, 0)
                                               .translated(pivotX, pivotY));

            g.setColour(armShadow.withAlpha(0.3f * alpha));
            g.strokePath(upperArm, juce::PathStrokeType(0.5f * s),
                         juce::AffineTransform::rotation(upperAngle, 0, 0)
                                              .translated(pivotX, pivotY));
        }

        // ═══ Antebrazo (codo → mano): rectángulo redondeado ═══════════════════
        {
            float forearmLen = std::sqrt((handX - elbowX) * (handX - elbowX)
                                        + (handY - elbowY) * (handY - elbowY));
            float forearmAngle = std::atan2(handY - elbowY, handX - elbowX);

            g.setColour(armColour.withAlpha(alpha * 0.95f));
            juce::Path forearm;
            forearm.addRoundedRectangle(0, -armWidth * 0.45f,
                                         forearmLen, armWidth * 0.9f,
                                         armWidth * 0.35f);
            g.fillPath(forearm, juce::AffineTransform::rotation(forearmAngle, 0, 0)
                                               .translated(elbowX, elbowY));

            g.setColour(armShadow.withAlpha(0.25f * alpha));
            g.strokePath(forearm, juce::PathStrokeType(0.4f * s),
                         juce::AffineTransform::rotation(forearmAngle, 0, 0)
                                              .translated(elbowX, elbowY));
        }

        // ═══ Codo (articulación): círculo ═════════════════════════════════════
        float jointR = armWidth * 0.55f;
        g.setColour(jointBlack().withAlpha(alpha));
        g.fillEllipse(elbowX - jointR, elbowY - jointR, jointR * 2.0f, jointR * 2.0f);
        g.setColour(armColour.withAlpha(alpha * 0.6f));
        g.drawEllipse(elbowX - jointR, elbowY - jointR, jointR * 2.0f, jointR * 2.0f, 0.8f * s);

        // ═══ Mano (círculo más grande al final) ════════════════════════════════
        float handR = armWidth * 0.75f;
        // Mano: círculo con glow
        g.setColour(armColour.withAlpha(alpha));
        g.fillEllipse(handX - handR, handY - handR, handR * 2.0f, handR * 2.0f);

        // Anillo de la mano
        g.setColour(armColour.brighter(0.2f).withAlpha(alpha * 0.5f));
        g.drawEllipse(handX - handR, handY - handR, handR * 2.0f, handR * 2.0f, 1.0f * s);

        // Pequeño glow púrpura en la palma (como el logo del pecho)
        float glowR = handR * 0.6f;
        juce::Colour palmGlow = purpleLight().withAlpha(0.25f * alpha);
        juce::ColourGradient palmGrad(palmGlow, handX, handY,
                                       juce::Colours::transparentBlack,
                                       handX + glowR, handY, true);
        g.setGradientFill(palmGrad);
        g.fillEllipse(handX - glowR, handY - glowR, glowR * 2.0f, glowR * 2.0f);

        // ═══ Articulación del hombro ══════════════════════════════════════════
        float shoulderR = armWidth * 0.6f;
        g.setColour(jointBlack().withAlpha(alpha));
        g.fillEllipse(pivotX - shoulderR, pivotY - shoulderR,
                      shoulderR * 2.0f, shoulderR * 2.0f);
        g.setColour(armColour.withAlpha(alpha * 0.5f));
        g.drawEllipse(pivotX - shoulderR, pivotY - shoulderR,
                      shoulderR * 2.0f, shoulderR * 2.0f, 0.6f * s);

        g.restoreState();
    }

} // namespace mixcoach
