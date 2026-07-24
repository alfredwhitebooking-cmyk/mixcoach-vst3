#include "QuickReplyBar.h"

namespace mixcoach {

    QuickReplyBar::QuickReplyBar()
    {
        setSize(200, kHeight);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setOpaque(false);
    }


    // ═══════════════════════════════════════════════════════════════════════════
    //  visibilityChanged — Pausa timer cuando el plugin no es visible
    // ═══════════════════════════════════════════════════════════════════════════

    void QuickReplyBar::visibilityChanged()
    {
        if (isShowing() && !isTimerRunning()) {
            startTimerHz(60);
        }
        // Nunca detener el timer — las animaciones de entrada/salida de
        // los chips de respuesta rápida deben seguir animándose.
    }

    // ═══ Bounce ease-out function ════════════════════════════════════════
    // Classic bounce: overshoots slightly and settles at t=1
    static float bounceOut(float t) noexcept
    {
        if (t < 1.0f / 2.75f) {
            return 7.5625f * t * t;
        } else if (t < 2.0f / 2.75f) {
            t -= 1.5f / 2.75f;
            return 7.5625f * t * t + 0.75f;
        } else if (t < 2.5f / 2.75f) {
            t -= 2.25f / 2.75f;
            return 7.5625f * t * t + 0.9375f;
        } else {
            t -= 2.625f / 2.75f;
            return 7.5625f * t * t + 0.984375f;
        }
    }

    void QuickReplyBar::startEntranceAnimation()
    {
        // Solo animar si hay replies y el componente está visible
        if (replies_.empty() || !isVisible()) return;

        animating_ = true;
        slideOffset_ = kSlidePx;
        currentAlpha_ = 0.0f;
        animStartMs_ = juce::Time::getMillisecondCounter();

        // Estado inicial: 20px abajo + invisible
        setTransform(juce::AffineTransform::translation(0.0f, slideOffset_));
        setAlpha(0.0f);

        startTimerHz(60);
        repaint();
    }

    void QuickReplyBar::timerCallback()
    {
        // ═══ Fade-out animation ═══════════════════════════════════════════
        if (fadingOut_) {
            int64_t elapsed = juce::Time::getMillisecondCounter() - fadeOutStartMs_;
            float t = juce::jmin(1.0f, (float)elapsed / (float)kFadeOutDurationMs);

            // Ease-in quad: t² (rápido al inicio, lento al final)
            float eased = t * t;
            float alpha = 1.0f - eased;

            setAlpha(juce::jmax(0.0f, alpha));

            if (t >= 1.0f) {
                // Fade-out completado — limpiar y notificar al padre
                fadingOut_ = false;
                replies_.clear();
                replyBounds_.clear();
                hoveredIndex_ = -1;
                setTransform(juce::AffineTransform());
                setAlpha(1.0f);
                stopTimer();

                repaint();

                // BUG #19: nullear callback antes de disparar para evitar double-fire
                if (onFadeOutComplete) { auto cb = onFadeOutComplete; onFadeOutComplete = nullptr; cb(); }
            } else {
                repaint();
            }
            return;
        }

        // ═══ Entrance animation (slide-up + fade-in) ═════════════════════
        if (!animating_ || replies_.empty()) {
            stopTimer();
            return;
        }

        int64_t elapsed = juce::Time::getMillisecondCounter() - animStartMs_;
        float t = juce::jmin(1.0f, (float)elapsed / (float)kAnimDurationMs);

        // Slide: bounce ease-out (overshoots + settles)
        float bounceVal = bounceOut(t);
        slideOffset_ = kSlidePx * (1.0f - bounceVal);

        // Alpha: ease-out quad (fade-in suave)
        float fadeVal = 1.0f - (1.0f - t) * (1.0f - t);
        currentAlpha_ = fadeVal;

        setTransform(juce::AffineTransform::translation(0.0f, slideOffset_));
        setAlpha(currentAlpha_);

        if (t >= 1.0f) {
            // Animación completa — resetear a estado final
            animating_ = false;
            setTransform(juce::AffineTransform());
            setAlpha(1.0f);
            stopTimer();
        }

        repaint();
    }

    void QuickReplyBar::setReplies(const std::vector<juce::String>& replies)
    {
        // Cancelar cualquier fade-out pendiente
        fadingOut_ = false;

        replies_ = replies;
        if ((int)replies_.size() > kMaxReplies)
            replies_.resize(kMaxReplies);
        replyBounds_.clear();
        resized();
        repaint();

        // Iniciar animación de entrada (slide-up + fade-in)
        startEntranceAnimation();
    }

    void QuickReplyBar::startFadeOut()
    {
        if (replies_.empty() || fadingOut_) return;

        // Cancelar entrance animation si está activa
        animating_ = false;

        fadingOut_ = true;
        fadeOutStartMs_ = juce::Time::getMillisecondCounter();

        // Resetear transform (cancelar slide) — solo fade-out
        setTransform(juce::AffineTransform());

        startTimerHz(60);
        repaint();
    }

    void QuickReplyBar::clearReplies()
    {
        replies_.clear();
        replyBounds_.clear();
        hoveredIndex_ = -1;

        // Resetear cualquier animación activa
        animating_ = false;
        fadingOut_ = false;
        setTransform(juce::AffineTransform());
        setAlpha(1.0f);
        stopTimer();

        repaint();
    }

    void QuickReplyBar::resized()
    {
        replyBounds_.clear();
        if (replies_.empty()) return;

        auto area = getLocalBounds().toFloat().reduced(4, 2);
        float gap = 6.0f;
        float totalW = 0.0f;
        int numReplies = (int)replies_.size();

        // Calculate total width for centering
        for (int i = 0; i < numReplies; ++i) {
            float textW = juce::GlyphArrangement::getStringWidthInt(
                juce::Font(juce::FontOptions(9.0f)).boldened(), replies_[i]);
            float btnW = textW + 20.0f;
            totalW += btnW;
            if (i < numReplies - 1) totalW += gap;
        }

        float startX = area.getX() + (area.getWidth() - totalW) * 0.5f;
        if (startX < area.getX()) startX = area.getX();

        for (int i = 0; i < numReplies; ++i) {
            float textW = juce::GlyphArrangement::getStringWidthInt(
                juce::Font(juce::FontOptions(9.0f)).boldened(), replies_[i]);
            float btnW = textW + 20.0f;
            float btnH = area.getHeight();
            replyBounds_.push_back({startX, area.getY(), btnW, btnH});
            startX += btnW + gap;
        }
    }

    void QuickReplyBar::paint(juce::Graphics& g)
    {
        if (replies_.empty()) return;

        auto bounds = getLocalBounds().toFloat();
        const float cr = 4.0f;

        // ─── Background glass panel ──────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.06f));
        g.fillRoundedRectangle(bounds.expanded(0, 2), cr);
        g.setColour(MixCoachTheme::bgPanel().withAlpha(0.85f));
        g.fillRoundedRectangle(bounds, cr);
        g.setColour(MixCoachTheme::border().withAlpha(0.2f));
        g.drawRoundedRectangle(bounds, cr, 0.5f);

        // ─── Draw each pill button ──────────────────────────────────────────
        for (int i = 0; i < (int)replyBounds_.size() && i < (int)replies_.size(); ++i) {
            auto& r = replyBounds_[i];
            bool hovered = (i == hoveredIndex_);
            const float pillCr = 10.0f;

            // Shadow
            g.setColour(juce::Colours::black.withAlpha(hovered ? 0.20f : 0.10f));
            g.fillRoundedRectangle(r.translated(0, hovered ? 1.5f : 1.0f), pillCr);

            // Background with gradient
            juce::ColourGradient bgGrad(
                MixCoachTheme::accent().withAlpha(hovered ? 0.20f : 0.10f),
                r.getX(), r.getY(),
                MixCoachTheme::accent().withAlpha(hovered ? 0.10f : 0.04f),
                r.getX(), r.getBottom(), false);
            g.setGradientFill(bgGrad);
            g.fillRoundedRectangle(r, pillCr);

            // Border
            g.setColour(MixCoachTheme::accent().withAlpha(hovered ? 0.45f : 0.20f));
            g.drawRoundedRectangle(r, pillCr, hovered ? 0.7f : 0.5f);

            // Text
            g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
            g.setColour(hovered ? MixCoachTheme::accentGlow() : MixCoachTheme::accentGlow().withAlpha(0.75f));
            g.drawText(replies_[i], r.toNearestInt(), juce::Justification::centred);
        }
    }

    void QuickReplyBar::mouseMove(const juce::MouseEvent& e)
    {
        auto pos = e.getPosition().toFloat();
        int oldHover = hoveredIndex_;
        hoveredIndex_ = -1;

        for (int i = 0; i < (int)replyBounds_.size(); ++i) {
            if (replyBounds_[i].contains(pos)) {
                hoveredIndex_ = i;
                break;
            }
        }

        if (hoveredIndex_ != oldHover) repaint();
    }

    void QuickReplyBar::mouseExit(const juce::MouseEvent&)
    {
        if (hoveredIndex_ >= 0) {
            hoveredIndex_ = -1;
            repaint();
        }
    }

    void QuickReplyBar::mouseDown(const juce::MouseEvent& e)
    {
        auto pos = e.getPosition().toFloat();
        for (int i = 0; i < (int)replyBounds_.size() && i < (int)replies_.size(); ++i) {
            if (replyBounds_[i].contains(pos)) {
                if (onReplySelected) onReplySelected(replies_[i]);
                break;
            }
        }
    }

} // namespace mixcoach
