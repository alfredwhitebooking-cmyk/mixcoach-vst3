#include "FocusOverlay.h"
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constants
    // ═══════════════════════════════════════════════════════════════════════════
    namespace FocusOverlayColours {
        inline juce::Colour dimBg()    { return juce::Colours::black.withAlpha(0.60f); }
        inline juce::Colour glowOuter(){ return MixCoachTheme::accent(); } // Purple glow
    }

    namespace FocusOverlayLayout {
        constexpr float kCornerRadius = 8.0f;
        constexpr float kGlowWidth    = 2.5f;
        constexpr float kGlowSpread   = 6.0f;  // Píxeles extras para el glow exterior
        constexpr int   kLabelGap     = 16;    // Gap entre cutout y label
        constexpr float kLabelFontSize = 11.0f;
        constexpr float kHintFontSize  = 8.0f;
        constexpr float kGlowAlpha     = 0.50f;
        constexpr float kInnerGlowAlpha = 0.20f;
        constexpr int   kMinCutoutW    = 80;   // Ancho mínimo del cutout
        constexpr int   kMinCutoutH    = 28;   // Alto mínimo del cutout
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor / Destructor
    // ═══════════════════════════════════════════════════════════════════════════
    FocusOverlay::FocusOverlay()
    {
        setOpaque(true);
        setMouseClickGrabsKeyboardFocus(false);
        setWantsKeyboardFocus(false);
        setVisible(false);
        setInterceptsMouseClicks(true, false);
    }

    FocusOverlay::~FocusOverlay()
    {
        stopTimer();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  focusGroup — Enfoca un grupo (bus) completo
    // ═══════════════════════════════════════════════════════════════════════════
    void FocusOverlay::focusGroup(BusType busType,
                                  const juce::String& busName,
                                  juce::Colour busColour,
                                  juce::Rectangle<int> focusBounds)
    {
        focusedBus_    = busType;
        focusedSlot_   = -1;
        focusMode_     = FocusMode::GroupFocus;
        targetFocusMode_ = FocusMode::GroupFocus;
        focusColour_   = busColour;
        focusLabel_    = busName.toUpperCase() + " BUS";

        // ─── Cutout bounds: expandir ligeramente para incluir glow ────────
        cutoutBounds_ = focusBounds.expanded(4, 4);
        targetCutoutBounds_ = cutoutBounds_;

        // ─── Iniciar animación fade-in ────────────────────────────────────
        setAlpha(0.0f);
        animProgress_ = 0.0f;
        animState_ = AnimState::FadingIn;
        setVisible(true);
        toFront(true);
        startTimerHz(60);

        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  focusTrack — Enfoca una pista individual
    // ═══════════════════════════════════════════════════════════════════════════
    void FocusOverlay::focusTrack(int slotIndex,
                                  const juce::String& trackName,
                                  juce::Colour trackColour,
                                  juce::Rectangle<int> focusBounds)
    {
        focusedBus_  = BusType::None;
        focusedSlot_ = slotIndex;
        focusMode_   = FocusMode::TrackFocus;
        targetFocusMode_ = FocusMode::TrackFocus;
        focusColour_ = trackColour;
        focusLabel_  = trackName;

        // ─── Cutout bounds: expandir ligeramente para incluir glow ────────
        cutoutBounds_ = focusBounds.expanded(4, 4);
        targetCutoutBounds_ = cutoutBounds_;

        // ─── Iniciar animación fade-in ────────────────────────────────────
        setAlpha(0.0f);
        animProgress_ = 0.0f;
        animState_ = AnimState::FadingIn;
        setVisible(true);
        toFront(true);
        startTimerHz(60);

        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  clearFocus — Limpia el enfoque con fade-out
    // ═══════════════════════════════════════════════════════════════════════════
    void FocusOverlay::clearFocus(float /*fadeMs*/)
    {
        if (focusMode_ == FocusMode::None) return;

        // ─── Iniciar animación fade-out ───────────────────────────────────
        animState_ = AnimState::FadingOut;
        animProgress_ = 0.0f;
        startTimerHz(60);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  visibilityChanged — Pausa timer cuando invisible
    // ═══════════════════════════════════════════════════════════════════════════
    void FocusOverlay::visibilityChanged()
    {
        if (!isVisible() && animState_ != AnimState::Idle) {
            animState_ = AnimState::Idle;
            stopTimer();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  resized — El overlay siempre cubre todo el parent
    // ═══════════════════════════════════════════════════════════════════════════
    void FocusOverlay::resized()
    {
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  mouseDown — Clic fuera del cutout → dismiss
    // ═══════════════════════════════════════════════════════════════════════════
    void FocusOverlay::mouseDown(const juce::MouseEvent& e)
    {
        if (focusMode_ == FocusMode::None || animState_ == AnimState::FadingOut)
            return;

        // ─── Si el clic NO está dentro del cutout, dismiss ─────────────────
        if (!cutoutBounds_.contains(e.getPosition())) {
            if (onDismiss) {
                // No llamar a clearFocus() directamente — dejar que el
                // NavigationShell maneje toda la orquestación
                onDismiss();
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  timerCallback — 60fps: controla fade-in/fade-out
    // ═══════════════════════════════════════════════════════════════════════════
    void FocusOverlay::timerCallback()
    {
        switch (animState_) {
            case AnimState::FadingIn: {
                animProgress_ += 1.0f / kFadeInFrames;
                if (animProgress_ >= 1.0f) {
                    animProgress_ = 1.0f;
                    setAlpha(1.0f);
                    animState_ = AnimState::Idle;
                    stopTimer();
                } else {
                    // Ease-out quad: suave al final
                    float eased = easeOutQuad(animProgress_);
                    setAlpha(eased);
                }
                repaint();
                break;
            }

            case AnimState::FadingOut: {
                animProgress_ += 1.0f / kFadeOutFrames;
                if (animProgress_ >= 1.0f) {
                    animProgress_ = 1.0f;
                    setAlpha(0.0f);
                    setVisible(false);
                    focusMode_ = FocusMode::None;
                    focusedBus_ = BusType::None;
                    focusedSlot_ = -1;
                    animState_ = AnimState::Idle;
                    stopTimer();
                } else {
                    // Ease-in quad: rápido al inicio, lento al final
                    float eased = easeInQuad(animProgress_);
                    setAlpha(1.0f - eased);
                }
                repaint();
                break;
            }

            case AnimState::Idle:
                // No hacer nada si está idle pero el timer está activo
                stopTimer();
                break;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Renderiza el overlay + cutout + glow + label
    // ═══════════════════════════════════════════════════════════════════════════
    void FocusOverlay::paint(juce::Graphics& g)
    {
        if (focusMode_ == FocusMode::None && animState_ != AnimState::FadingOut)
            return;

        auto bounds = getLocalBounds();

        // ═══ 1. Fondo oscuro con cutout ═══════════════════════════════════
        drawCutoutPath(g);

        // ═══ 2. Borde glow alrededor del cutout ══════════════════════════
        drawGlowBorder(g);

        // ═══ 3. Label del elemento enfocado ═══════════════════════════════
        drawFocusLabel(g);

        // ═══ 4. Hint text "Click fuera para quitar focus" ════════════════
        drawHintText(g);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawCutoutPath — Crea el efecto de "agujero de luz"
    //
    //  Usa even-odd fill rule: el rectángulo exterior se rellena, el interior
    //  (cutout) se sustrae, creando un agujero transparente.
    // ═══════════════════════════════════════════════════════════════════════════
    void FocusOverlay::drawCutoutPath(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        auto cutout = cutoutBounds_.toFloat();

        // ═══ Crear path con even-odd fill rule ════════════════════════════
        juce::Path path;
        path.setUsingNonZeroWinding(false);  // Enable even-odd fill

        // Outer: full overlay area (clockwise by default)
        path.addRectangle(bounds);

        // Inner: cutout (also clockwise, but with even-odd creates a hole)
        path.addRoundedRectangle(cutout, FocusOverlayLayout::kCornerRadius);

        // ═══ Fill con alpha según animación ═══════════════════════════════
        float bgAlpha = FocusOverlayColours::dimBg().getFloatAlpha();
        float currentAlpha = getAlpha();

        g.setColour(juce::Colours::black.withAlpha(bgAlpha * currentAlpha));
        g.fillPath(path);

        // ═══ Inner glow sutil dentro del cutout (ilumina el contenido) ═══
        g.setColour(focusColour_.withAlpha(FocusOverlayLayout::kInnerGlowAlpha * currentAlpha));
        g.fillRoundedRectangle(cutout.expanded(1.0f, 1.0f),
                               FocusOverlayLayout::kCornerRadius - 1.0f);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawGlowBorder — Borde con glow alrededor del cutout
    //
    //  Dibuja 2 círculos concéntricos de glow para simular un halo.
    //  Outer glow: más ancho, más tenue
    //  Inner glow: más brillante, más fino
    // ═══════════════════════════════════════════════════════════════════════════
    void FocusOverlay::drawGlowBorder(juce::Graphics& g)
    {
        auto cutout = cutoutBounds_.toFloat();
        float currentAlpha = getAlpha();

        // ═══ Outer glow (más ancho, más tenue) ════════════════════════════
        auto outerBounds = cutout.expanded(FocusOverlayLayout::kGlowSpread,
                                           FocusOverlayLayout::kGlowSpread);
        g.setColour(focusColour_.withAlpha(
            FocusOverlayLayout::kGlowAlpha * 0.35f * currentAlpha));
        g.drawRoundedRectangle(outerBounds,
                               FocusOverlayLayout::kCornerRadius + 3.0f,
                               FocusOverlayLayout::kGlowWidth + 1.5f);

        // ═══ Inner glow (más fino, más brillante) ═════════════════════════
        g.setColour(focusColour_.withAlpha(
            FocusOverlayLayout::kGlowAlpha * 0.85f * currentAlpha));
        g.drawRoundedRectangle(cutout,
                               FocusOverlayLayout::kCornerRadius,
                               FocusOverlayLayout::kGlowWidth);

        // ═══ White core (pico de brillo en el centro) ═════════════════════
        auto innerBounds = cutout.reduced(0.5f, 0.5f);
        g.setColour(juce::Colours::white.withAlpha(
            0.20f * currentAlpha));
        g.drawRoundedRectangle(innerBounds,
                               FocusOverlayLayout::kCornerRadius - 0.5f,
                               0.8f);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawFocusLabel — Texto descriptivo centrado debajo del cutout
    //
    //  "DRUM BUS" o "Kick"
    //  Si es TrackFocus y hay dominio, muestra "(gain)" "(tonal)" etc.
    // ═══════════════════════════════════════════════════════════════════════════
    void FocusOverlay::drawFocusLabel(juce::Graphics& g)
    {
        if (focusLabel_.isEmpty()) return;

        float currentAlpha = getAlpha();

        // ─── Posición: centrado debajo del cutout ─────────────────────────
        int labelY = cutoutBounds_.getBottom() + FocusOverlayLayout::kLabelGap;
        auto labelArea = juce::Rectangle<int>(
            0, labelY,
            getWidth(), static_cast<int>(FocusOverlayLayout::kLabelFontSize * 1.8f));

        // ─── Background pill semitransparente ─────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(FocusOverlayLayout::kLabelFontSize)).boldened());

        float textW = juce::GlyphArrangement::getStringWidthInt(
            g.getCurrentFont(), focusLabel_) + 24.0f;
        float textH = FocusOverlayLayout::kLabelFontSize + 10.0f;

        float pillX = (getWidth() - textW) * 0.5f;
        float pillY = (float)labelY;
        auto pillRect = juce::Rectangle<float>(pillX, pillY, textW, textH);

        // ─── Shadow ───────────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.30f * currentAlpha));
        g.fillRoundedRectangle(pillRect.translated(1.0f, 2.0f), 6.0f);

        // ─── Background glass ─────────────────────────────────────────────
        juce::ColourGradient pillGrad(
            focusColour_.withAlpha(0.18f * currentAlpha),
            pillRect.getCentreX(), pillRect.getY(),
            focusColour_.withAlpha(0.06f * currentAlpha),
            pillRect.getCentreX(), pillRect.getBottom(),
            false);
        g.setGradientFill(pillGrad);
        g.fillRoundedRectangle(pillRect, 6.0f);

        // ─── Border sutil ─────────────────────────────────────────────────
        g.setColour(focusColour_.withAlpha(0.35f * currentAlpha));
        g.drawRoundedRectangle(pillRect, 6.0f, 0.8f);

        // ─── Label accent bar (left edge) ─────────────────────────────────
        g.setColour(focusColour_.withAlpha(0.5f * currentAlpha));
        g.fillRoundedRectangle(
            juce::Rectangle<float>(pillRect.getX() + 1.0f, pillRect.getY() + 3.0f,
                                   2.5f, pillRect.getHeight() - 6.0f),
            1.5f);

        // ─── Text ─────────────────────────────────────────────────────────
        g.setColour(juce::Colours::white.withAlpha(0.95f * currentAlpha));
        g.drawText(focusLabel_, pillRect.toNearestInt(), juce::Justification::centred);

        // ─── Mode badge (GROUP o TRACK) — small tag above the label ───────
        {                    juce::String modeTag = (focusMode_ == FocusMode::GroupFocus)
                                   ? "\xF0\x9F\x94\xA6 GROUP"
                                   : "\xF0\x9F\x94\xA6 TRACK";

            float tagFontSize = 7.0f;
            g.setFont(juce::Font(juce::FontOptions(tagFontSize)).boldened());
            float tagW = juce::GlyphArrangement::getStringWidthInt(
                g.getCurrentFont(), modeTag) + 12.0f;
            float tagH = tagFontSize + 6.0f;
            float tagX = (getWidth() - tagW) * 0.5f;
            float tagY = pillRect.getY() - tagH - 4.0f;
            auto tagRect = juce::Rectangle<float>(tagX, tagY, tagW, tagH);

            // Tag background
            g.setColour(MixCoachTheme::bgPanel().withAlpha(0.85f * currentAlpha));
            g.fillRoundedRectangle(tagRect, 3.0f);
            g.setColour(MixCoachTheme::accent().withAlpha(0.30f * currentAlpha));
            g.drawRoundedRectangle(tagRect, 3.0f, 0.5f);

            g.setColour(MixCoachTheme::accentGlow().withAlpha(0.85f * currentAlpha));
            g.drawText(modeTag, tagRect.toNearestInt(), juce::Justification::centred);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawHintText — Texto de ayuda en la parte inferior
    // ═══════════════════════════════════════════════════════════════════════════
    void FocusOverlay::drawHintText(juce::Graphics& g)
    {
        float currentAlpha = getAlpha();
        if (currentAlpha < 0.15f) return;

        // Solo mostrar durante fade-in y estado idle
        if (animState_ == AnimState::FadingOut) return;

        auto bounds = getLocalBounds();
        int hintY = bounds.getBottom() - 40;

        auto hintArea = juce::Rectangle<int>(0, hintY, getWidth(), 20);
        g.setFont(juce::Font(juce::FontOptions(FocusOverlayLayout::kHintFontSize)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.40f * currentAlpha));
        g.drawText(
            "\xE2\x8C\xA8  Click fuera del \xC3\xA1rea iluminada para volver al chat",
                   hintArea, juce::Justification::centred);
    }

} // namespace mixcoach
