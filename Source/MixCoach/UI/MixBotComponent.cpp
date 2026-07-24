#include "MixBotComponent.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constants
    // ═══════════════════════════════════════════════════════════════════════════
    namespace {
        constexpr int kTimerHz = 60;   // ~16ms per tick (~60fps, not 150ms as plan says;
                                       //  60fps gives smooth head tilt & mouth)
        constexpr int kBlinkIntervalMinMs = 3000;  // Min ms between blinks
        constexpr int kBlinkIntervalMaxMs = 6000;  // Max ms between blinks
        constexpr int kBlinkDurationMs = 150;      // 150ms for one blink cycle
        constexpr float kMaxHeadTiltDeg = 8.0f;    // Max head tilt degrees
        constexpr float kHeadTiltPeriodMs = 3000.0f; // Complete tilt cycle
        constexpr float kMouthFrequency = 8.0f;    // Hz for talking mouth movement
        constexpr float kFloatAmplitude = 2.0f;    // Floating vertical amplitude (px)
        constexpr float kFloatPeriodMs = 2500.0f;  // Floating cycle ms
        constexpr float kBreathAmplitude = 0.015f; // Breath scale amplitude
        constexpr float kBreathPeriodMs = 3500.0f; // Breath cycle ms

        constexpr const char* kAssetSubdir = "MixCoach/Assets/";
        constexpr const char* kSpriteFilenames[] = {
            "robot_idle.png",
            "robot_thinking.png",
            "robot_talking.png",
            "robot_listening.png",
            "robot_celebrating.png"
        };
        constexpr int kNumSpriteFiles = 5;

        // Index mapping: MixBotState → kSpriteFilenames[]
        constexpr int stateToFileIndex(MixBotState state) noexcept
        {
            switch (state) {
                case MixBotState::Idle:        return 0;
                case MixBotState::Thinking:    return 1;
                case MixBotState::Talking:     return 2;
                case MixBotState::Listening:   return 3;
                case MixBotState::Celebrating: return 4;
                default:                       return 0;
            }
        }

        // Map AvatarExpression to closest MixBotState for sprite display
        constexpr MixBotState expressionToState(AvatarExpression expr) noexcept
        {
            switch (expr) {
                case AvatarExpression::Neutral:     return MixBotState::Idle;
                case AvatarExpression::Happy:       return MixBotState::Celebrating;
                case AvatarExpression::Surprised:   return MixBotState::Listening;
                case AvatarExpression::Thinking:    return MixBotState::Thinking;
                case AvatarExpression::Encouraging: return MixBotState::Celebrating;
                case AvatarExpression::Serious:     return MixBotState::Thinking;
                default:                            return MixBotState::Idle;
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor — Carga sprites e inicia timer
    // ═══════════════════════════════════════════════════════════════════════════
    MixBotComponent::MixBotComponent()
    {
        setOpaque(false);
        setSize(120, 120);
        hasSprites_ = loadSprites();
        lastAnimUpdate_ = juce::Time::getMillisecondCounter();
        startTimerHz(kTimerHz);
    }


    // ═══════════════════════════════════════════════════════════════════════════
    //  visibilityChanged — Pausa/restaura el timer según visibilidad
    // ═══════════════════════════════════════════════════════════════════════════
    //  BUG #10: El timer de animación (60fps) seguía corriendo aunque el
    //  componente no fuera visible, desperdiciando CPU innecesariamente.
    //  Ahora se detiene cuando !isShowing() y se reinicia al ser visible.

    void MixBotComponent::visibilityChanged()
    {
        if (isShowing()) {
            if (!isTimerRunning())
                startTimerHz(kTimerHz);
        } else {
            stopTimer();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  loadSprites — Intenta cargar los 5 PNGs desde múltiples ubicaciones
    //  Retorna true si AL MENOS UN sprite se cargó exitosamente.
    // ═══════════════════════════════════════════════════════════════════════════
    bool MixBotComponent::loadSprites()
    {
        // ═══ Buscar en Documents/MixCoach/Assets/ ═════════════════════════════
        juce::File docsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        juce::File assetsDir = docsDir.getChildFile(kAssetSubdir);

        // ═══ Fallback: buscar en el directorio del ejecutable ═════════════════
        juce::File exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                                .getParentDirectory()
                                .getChildFile("Assets");

        // ═══ Cargar cada sprite ═══════════════════════════════════════════════
        auto loadFromDirs = [this, &assetsDir, &exeDir](const juce::String& filename) -> juce::Image {
            // Try documents directory first
            juce::File f = assetsDir.getChildFile(filename);
            if (f.existsAsFile()) {
                auto img = juce::ImageFileFormat::loadFrom(f);
                if (img.isValid()) return img;
            }
            // Try executable directory
            f = exeDir.getChildFile(filename);
            if (f.existsAsFile()) {
                auto img = juce::ImageFileFormat::loadFrom(f);
                if (img.isValid()) return img;
            }
            return {};
        };

        idleSprite_        = loadFromDirs("robot_idle.png");
        thinkingSprite_    = loadFromDirs("robot_thinking.png");
        talkingSprite_     = loadFromDirs("robot_talking.png");
        listeningSprite_   = loadFromDirs("robot_listening.png");
        celebratingSprite_ = loadFromDirs("robot_celebrating.png");

        // Retorna true si al menos un sprite se cargó
        return idleSprite_.isValid()
            || thinkingSprite_.isValid()
            || talkingSprite_.isValid()
            || listeningSprite_.isValid()
            || celebratingSprite_.isValid();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getCurrentSprite — Retorna el sprite correspondiente al estado actual
    //  Si el sprite para el estado actual no se cargó, intenta fallback a idle.
    // ═══════════════════════════════════════════════════════════════════════════
    juce::Image MixBotComponent::getCurrentSprite() const
    {
        switch (currentState_) {
            case MixBotState::Idle:
                return idleSprite_.isValid() ? idleSprite_ : juce::Image{};
            case MixBotState::Thinking:
                return thinkingSprite_.isValid() ? thinkingSprite_
                      : (idleSprite_.isValid() ? idleSprite_ : juce::Image{});
            case MixBotState::Talking:
                return talkingSprite_.isValid() ? talkingSprite_
                      : (idleSprite_.isValid() ? idleSprite_ : juce::Image{});
            case MixBotState::Listening:
                return listeningSprite_.isValid() ? listeningSprite_
                      : (idleSprite_.isValid() ? idleSprite_ : juce::Image{});
            case MixBotState::Celebrating:
                return celebratingSprite_.isValid() ? celebratingSprite_
                      : (idleSprite_.isValid() ? idleSprite_ : juce::Image{});
            default:
                return idleSprite_;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Dibuja el avatar: sprite si available, procedural si no
    // ═══════════════════════════════════════════════════════════════════════════
    void MixBotComponent::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        if (hasSprites_) {
            drawSprite(g, bounds);
        } else {
            drawProcedural(g, bounds);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawSprite — Renderiza sprite PNG con transformaciones animadas
    // ═══════════════════════════════════════════════════════════════════════════
    void MixBotComponent::drawSprite(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        auto sprite = getCurrentSprite();
        if (! sprite.isValid()) {
            // Fallback a procedural si el sprite actual no es válido
            drawProcedural(g, bounds);
            return;
        }

        float minDim = juce::jmin(bounds.getWidth(), bounds.getHeight());
        if (minDim < 16.0f) return;

        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();

        g.saveState();

        // ═══ Aplicar transformaciones ════════════════════════════════════════
        // 1. Nod rotation (asentimiento)
        if (nodActive_ && nodPhase_ > 0.0f && nodPhase_ < 1.0f) {
            float nodAngle = (nodPhase_ < 0.5f)
                ? nodPhase_ * 2.0f * -12.0f  // bajar (0→0.5)
                : (1.0f - nodPhase_) * 2.0f * -12.0f; // subir (0.5→1.0)
            float nodRad = juce::degreesToRadians(nodAngle);
            g.addTransform(juce::AffineTransform::rotation(nodRad, cx, cy));
        }

        // 2. Head tilt (rotar alrededor del centro)
        float tiltRad = juce::degreesToRadians(headTiltAngle_);
        g.addTransform(juce::AffineTransform::rotation(tiltRad, cx, cy));

        // 3. Floating + breathing scale
        float scale = breathScale_;
        float floatY = floatOffset_;

        float spriteDim = minDim * 0.85f * scale;
        float sx = cx - spriteDim * 0.5f;
        float sy = cy - spriteDim * 0.5f + floatY;

        // ═══ Glow según expresión (halo colored) ═════════════════════════════
        juce::Colour glowCol;
        switch (currentExpression_) {
            case AvatarExpression::Happy:       glowCol = juce::Colour(0x4400E5A0); break;
            case AvatarExpression::Surprised:   glowCol = juce::Colour(0x44FFD600); break;
            case AvatarExpression::Thinking:    glowCol = juce::Colour(0x44448AFF); break;
            case AvatarExpression::Encouraging: glowCol = juce::Colour(0x44FFAB40); break;
            case AvatarExpression::Serious:     glowCol = juce::Colour(0x44FF5252); break;
            default:                            glowCol = juce::Colour(0x44C8A8FF); break;
        }

        if (glowCol.getAlpha() > 0) {
            float glowR = spriteDim * 0.35f;
            juce::ColourGradient glow(glowCol, cx, cy,
                                       juce::Colours::transparentBlack,
                                       cx + glowR, cy, true);
            g.setGradientFill(glow);
            g.fillEllipse(cx - glowR, cy - glowR, glowR * 2.0f, glowR * 2.0f);
        }

        // ═══ State transition crossfade ═══════════════════════════════════
        // If a transition is in progress, blend old and new sprites
        float alpha = juce::jmin(1.0f, expressionIntensity_);
        float transitionAlpha = 1.0f;

        if (stateTransitionProgress_ < 1.0f && prevSprite_.isValid()) {
            // Draw previous sprite with (1 - progress) alpha
            float prevAlpha = (1.0f - stateTransitionProgress_) * alpha;
            g.setColour(juce::Colours::white.withAlpha(prevAlpha));
            g.drawImage(prevSprite_,
                        (int)sx, (int)sy,
                        (int)spriteDim, (int)spriteDim,
                        0, 0, prevSprite_.getWidth(), prevSprite_.getHeight());
            // Current sprite uses progress as alpha
            transitionAlpha = stateTransitionProgress_ * alpha;
        }

        // ═══ Dibujar sprite actual ════════════════════════════════════════
        g.setColour(juce::Colours::white.withAlpha(transitionAlpha));

        g.drawImage(sprite,
                    (int)sx, (int)sy,
                    (int)spriteDim, (int)spriteDim,
                    0, 0, sprite.getWidth(), sprite.getHeight());

        g.restoreState();

        // ═══ Efecto de parpadeo (blink) ═════════════════════════════════════
        if (isBlinking_ && blinkPhase_ > 0) {
            float blinkAlpha = 0.0f;
            if (blinkPhase_ <= 4) {
                blinkAlpha = (float)blinkPhase_ / 4.0f; // Closing
            } else {
                blinkAlpha = (float)(8 - blinkPhase_) / 4.0f; // Opening
            }
            g.setColour(juce::Colours::black.withAlpha(blinkAlpha * 0.85f));
            float eyeCY = cy - spriteDim * 0.05f;
            float eyeH = spriteDim * 0.18f;
            float eyeW = spriteDim * 0.16f;
            float eyeSpacing = spriteDim * 0.12f;
            g.fillEllipse(cx - eyeSpacing - eyeW * 0.5f, eyeCY - eyeH * 0.5f, eyeW, eyeH);
            g.fillEllipse(cx + eyeSpacing - eyeW * 0.5f, eyeCY - eyeH * 0.5f, eyeW, eyeH);
        }

        // ═══ Talking mouth LED (sprite overlay) ═════════════════════════════
        // When in Talking state, draw a small LED bar below the sprite center
        // that opens/closes with mouthOpenAmount_.
        if (currentState_ == MixBotState::Talking && mouthOpenAmount_ > 0.01f) {
            float mouthCY = cy + spriteDim * 0.32f;
            float mouthW = spriteDim * 0.30f;
            float mouthH = spriteDim * 0.06f + spriteDim * 0.08f * mouthOpenAmount_;
            float mouthX = cx - mouthW * 0.5f;

            // Mouth glow (teal/purple LED)
            juce::Colour mouthLED = juce::Colour(0xFFA855F7).withAlpha(0.7f + 0.3f * mouthOpenAmount_);
            juce::ColourGradient mouthGrad(mouthLED, mouthX, mouthCY,
                                            juce::Colour(0xFF7C3AED).withAlpha(0.3f),
                                            mouthX + mouthW, mouthCY, false);
            g.setGradientFill(mouthGrad);
            g.fillRoundedRectangle(mouthX, mouthCY - mouthH * 0.5f, mouthW, mouthH, mouthH * 0.4f);

            // Center bright spot
            g.setColour(juce::Colour(0xFFC8A8FF).withAlpha(0.5f * mouthOpenAmount_));
            g.fillRoundedRectangle(mouthX + mouthW * 0.2f, mouthCY - mouthH * 0.3f,
                                    mouthW * 0.6f, mouthH * 0.6f, mouthH * 0.2f);
        }

        // ═══ Think/Listen LED indicators (ear-level dots) ═════════════════
        if (currentState_ == MixBotState::Thinking) {
            float ledY = cy - spriteDim * 0.25f;
            float ledR = spriteDim * 0.025f;
            float blinkAlpha = 0.5f + 0.5f * std::sin(juce::Time::getMillisecondCounter() * 0.008f);
            juce::Colour ledCol = juce::Colour(0xFF448AFF).withAlpha(blinkAlpha * 0.8f);
            g.setColour(ledCol);
            g.fillEllipse(cx - spriteDim * 0.35f - ledR, ledY - ledR, ledR * 2.0f, ledR * 2.0f);
            g.fillEllipse(cx + spriteDim * 0.35f - ledR, ledY - ledR, ledR * 2.0f, ledR * 2.0f);
        } else if (currentState_ == MixBotState::Listening) {
            float ledY = cy - spriteDim * 0.25f;
            float ledR = spriteDim * 0.025f;
            juce::Colour ledCol = juce::Colour(0xFF00E5A0).withAlpha(0.8f);
            g.setColour(ledCol);
            g.fillEllipse(cx - spriteDim * 0.35f - ledR, ledY - ledR, ledR * 2.0f, ledR * 2.0f);
            g.fillEllipse(cx + spriteDim * 0.35f - ledR, ledY - ledR, ledR * 2.0f, ledR * 2.0f);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawProcedural — Delega al drawAvatarStatic de RobotAvatarComponent
    // ═══════════════════════════════════════════════════════════════════════════
    void MixBotComponent::drawProcedural(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        RobotAvatarComponent::drawAvatarStatic(
            g, bounds,
            currentExpression_,
            1.0f,
            expressionIntensity_,
            gazeX_,
            gazeY_,
            waveActive_ ? wavePhase_ : 0.0f,
            nodActive_ ? nodPhase_ : 0.0f,
            currentState_ == MixBotState::Thinking,
            currentState_ == MixBotState::Listening,
            audioActive_,
            audioLevel_);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  timerCallback — 60fps: animaciones de sprite + decay de expresión
    // ═══════════════════════════════════════════════════════════════════════════
    void MixBotComponent::timerCallback()
    {
        int64_t now = juce::Time::getMillisecondCounter();
        float deltaMs = (float)(now - lastAnimUpdate_);
        lastAnimUpdate_ = now;

        // ═══ Expression decay (misma lógica que RobotAvatarComponent) ═════
        expressionTimerCallback();

        // ═══ Wave decay ════════════════════════════════════════════════════
        if (waveActive_) {
            int64_t elapsed = now - waveStartMs_;
            if (elapsed >= waveDurationMs_) {
                waveActive_ = false;
                wavePhase_ = 0.0f;
                if (hasSprites_) {
                    setMixBotState(MixBotState::Idle);
                }
            } else {
                wavePhase_ = (float)elapsed / (float)waveDurationMs_;
            }
            repaint();
        }

        // ═══ Nod decay ═════════════════════════════════════════════════════
        if (nodActive_) {
            int64_t elapsed = now - nodStartMs_;
            if (elapsed >= nodDurationMs_) {
                nodActive_ = false;
                nodPhase_ = 0.0f;
            } else {
                nodPhase_ = (float)elapsed / (float)nodDurationMs_;
            }
            repaint();
        }

        // ═══ State transition progress (300ms crossfade) ════════════════
        if (stateTransitionProgress_ < 1.0f) {
            int64_t elapsed = now - stateTransitionStartMs_;
            stateTransitionProgress_ = juce::jmin(1.0f, (float)elapsed / 300.0f);
            if (stateTransitionProgress_ >= 1.0f) {
                prevSprite_ = {}; // Release prev sprite memory
            }
            repaint();
        }

        // ═══ Animaciones de sprite (solo si hay sprites) ═══════════════════
        if (hasSprites_) {
            updateSpriteAnimations(now);

            // Force repaint if any animation is active
            if (isBlinking_
                || std::abs(headTiltAngle_) > 0.5f
                || std::abs(floatOffset_) > 0.1f
                || std::abs(breathScale_ - 1.0f) > 0.005f
                || mouthOpenAmount_ > 0.01f
                || waveActive_
                || nodActive_
                || stateTransitionProgress_ < 1.0f)
            {
                repaint();
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateSpriteAnimations — Blink, head tilt, float, breath, mouth
    // ═══════════════════════════════════════════════════════════════════════════
    void MixBotComponent::updateSpriteAnimations(int64_t now)
    {
        // ═══ Blink: aleatorio cada 3-6s ═════════════════════════════════════
        if (!isBlinking_) {
            // Random trigger for blink
            if (blinkTimer_ == 0) {
                blinkTimer_ = now + juce::Random::getSystemRandom().nextInt(
                    juce::Range<int>(kBlinkIntervalMinMs, kBlinkIntervalMaxMs));
            }
            if (now >= blinkTimer_) {
                triggerBlink();
            }
        } else {
            // Blink in progress: advance phase
            int64_t blinkElapsed = now - blinkTimer_;
            if (blinkElapsed >= kBlinkDurationMs) {
                isBlinking_ = false;
                blinkPhase_ = 0;
                blinkTimer_ = 0;
                repaint();
            } else {
                int newPhase = (int)(blinkElapsed * 8 / kBlinkDurationMs) + 1;
                if (newPhase != blinkPhase_) {
                    blinkPhase_ = juce::jmin(newPhase, 8);
                    repaint();
                }
            }
        }

        // ═══ Head tilt: suave oscilación sinusoidal ═════════════════════════
        float tiltPhase = (float)(now % (int)kHeadTiltPeriodMs) / kHeadTiltPeriodMs;
        float tiltTarget = 0.0f;

        switch (currentState_) {
            case MixBotState::Thinking:
                // Ladeo constante (curioso)
                tiltTarget = kMaxHeadTiltDeg * 0.6f;
                break;
            case MixBotState::Listening:
                // Ladeo sutil (atento)
                tiltTarget = kMaxHeadTiltDeg * 0.3f * std::sin(tiltPhase * juce::MathConstants<float>::twoPi);
                break;
            case MixBotState::Talking:
                // Movimiento rítmico
                tiltTarget = kMaxHeadTiltDeg * 0.2f * std::sin(tiltPhase * juce::MathConstants<float>::twoPi * 1.5f);
                break;
            case MixBotState::Celebrating:
                // Cabeza erguida, sin ladeo
                tiltTarget = 0.0f;
                break;
            case MixBotState::Idle:
            default:
                // Respiración suave
                tiltTarget = kMaxHeadTiltDeg * 0.15f * std::sin(tiltPhase * juce::MathConstants<float>::twoPi * 0.6f);
                break;
        }

        // ═══ Nod override: rápida oscilación vertical (asentimiento) ═════
        if (nodActive_ && nodPhase_ > 0.0f && nodPhase_ < 1.0f) {
            float nodProgress = nodPhase_ * juce::MathConstants<float>::twoPi * 2.0f; // 2 ciclos completos en 500ms
            float nodAngle = kMaxHeadTiltDeg * 1.2f * std::sin(nodProgress);
            // Decay envelope: fade out toward the end
            float envelope = 1.0f - nodPhase_ * nodPhase_ * 0.5f;
            tiltTarget = nodAngle * envelope;
        }

        // Suavizar hacia el target
        headTiltAngle_ += (tiltTarget - headTiltAngle_) * 0.12f;

        // Durante un nod, forzar más rápido (más responsivo)
        if (nodActive_) {
            headTiltAngle_ += (tiltTarget - headTiltAngle_) * 0.25f;
        }

        // ═══ Floating ═══════════════════════════════════════════════════════
        float floatPhase = (float)(now % (int)kFloatPeriodMs) / kFloatPeriodMs;
        floatOffset_ = kFloatAmplitude * std::sin(floatPhase * juce::MathConstants<float>::twoPi);

        // ═══ Breathing ══════════════════════════════════════════════════════
        float breathPhase = (float)(now % (int)kBreathPeriodMs) / kBreathPeriodMs;
        breathScale_ = 1.0f + kBreathAmplitude * std::sin(breathPhase * juce::MathConstants<float>::twoPi);

        // ═══ Mouth animation (solo Talking) ═════════════════════════════════
        if (currentState_ == MixBotState::Talking) {
            float mouthPhase = (float)(now % 125) / 125.0f; // ~8Hz
            mouthOpenAmount_ = 0.3f + 0.7f * (0.5f + 0.5f * std::sin(mouthPhase * juce::MathConstants<float>::twoPi));
        } else {
            mouthOpenAmount_ = 0.0f;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  triggerBlink — Inicia un ciclo de parpadeo
    // ═══════════════════════════════════════════════════════════════════════════
    void MixBotComponent::triggerBlink()
    {
        isBlinking_ = true;
        blinkPhase_ = 1;
        blinkTimer_ = juce::Time::getMillisecondCounter();
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Expression management (proxy a RobotAvatarComponent logic)
    // ═══════════════════════════════════════════════════════════════════════════
    void MixBotComponent::setExpression(AvatarExpression newExpression)
    {
        if (currentExpression_ == newExpression) return;

        currentExpression_   = newExpression;
        expressionChangeMs_  = juce::Time::getMillisecondCounter();
        expressionIntensity_ = 0.0f;
        expressionDecayMs_   = 0;

        // Sync state to expression if in sprite mode
        if (hasSprites_) {
            setMixBotState(expressionToState(newExpression));
        }

        repaint();
    }

    void MixBotComponent::setExpressionWithDecay(AvatarExpression exp, int64_t decayMs)
    {
        setExpression(exp);
        expressionDecayMs_ = decayMs;
    }

    void MixBotComponent::expressionTimerCallback()
    {
        // ═══ Animar intensidad de expresión (fade in) ════════════════════════
        if (expressionIntensity_ < 1.0f && expressionChangeMs_ > 0) {
            float elapsed = (float)(juce::Time::getMillisecondCounter() - expressionChangeMs_);
            expressionIntensity_ = juce::jmin(1.0f, elapsed / 400.0f); // 400ms fade-in
            repaint();
        }

        // ═══ Decay: volver a Neutral después de un tiempo ═══════════════════
        if (expressionDecayMs_ > 0) {
            int64_t elapsed = juce::Time::getMillisecondCounter() - expressionChangeMs_;
            if (elapsed > expressionDecayMs_ && currentExpression_ != AvatarExpression::Neutral) {
                currentExpression_   = AvatarExpression::Neutral;
                expressionChangeMs_  = juce::Time::getMillisecondCounter();
                expressionIntensity_ = 0.0f;
                expressionDecayMs_   = 0;

                if (hasSprites_) {
                    setMixBotState(MixBotState::Idle);
                }

                repaint();
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Wave animation
    // ═══════════════════════════════════════════════════════════════════════════
    void MixBotComponent::setWaveActive(bool active, int64_t durationMs)
    {
        if (active) {
            waveActive_ = true;
            waveStartMs_ = juce::Time::getMillisecondCounter();
            waveDurationMs_ = durationMs;
            wavePhase_ = 0.0f;
            // En sprite mode, cambiar a Celebrating para el wave
            if (hasSprites_) {
                setMixBotState(MixBotState::Celebrating);
            }
        } else {
            waveActive_ = false;
            wavePhase_ = 0.0f;
            if (hasSprites_) {
                setMixBotState(MixBotState::Idle);
            }
        }
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Nod animation — cabeceo rápido de asentimiento
    // ═══════════════════════════════════════════════════════════════════════════
    void MixBotComponent::triggerNod(int64_t durationMs)
    {
        nodActive_ = true;
        nodStartMs_ = juce::Time::getMillisecondCounter();
        nodDurationMs_ = durationMs;
        nodPhase_ = 0.0f;
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setMixBotState — Cambia el estado sprite-based
    // ═══════════════════════════════════════════════════════════════════════════
    void MixBotComponent::setMixBotState(MixBotState newState)
    {
        if (currentState_ == newState) return;

        // Capture previous state for crossfade transition
        if (hasSprites_) {
            prevSprite_ = getCurrentSprite();
        }
        currentState_ = newState;

        // Start 300ms crossfade transition
        stateTransitionStartMs_ = juce::Time::getMillisecondCounter();
        stateTransitionProgress_ = 0.0f;

        // Resetear animaciones al cambiar de estado
        if (hasSprites_) {
            // Reset blink timer to avoid immediate blink on state change
            blinkTimer_ = juce::Time::getMillisecondCounter()
                          + juce::Random::getSystemRandom().nextInt(
                              juce::Range<int>(1000, 3000));
        }

        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  renderToImage — Renderiza el avatar en un Image ARGB
    // ═══════════════════════════════════════════════════════════════════════════
    juce::Image MixBotComponent::renderToImage(int width, int height)
    {
        juce::Image result(juce::Image::ARGB, width, height, true);
        juce::Graphics g(result);

        auto bounds = result.getBounds().toFloat();

        if (hasSprites_) {
            drawSprite(g, bounds);
        } else {
            RobotAvatarComponent::drawAvatarStatic(
                g, bounds,
                currentExpression_,
                1.0f, 1.0f,
                gazeX_, gazeY_,
                waveActive_ ? wavePhase_ : 0.0f,
                nodActive_ ? nodPhase_ : 0.0f,
                currentState_ == MixBotState::Thinking,
                currentState_ == MixBotState::Listening,
                audioActive_,
                audioLevel_);
        }

        return result;
    }

} // namespace mixcoach
