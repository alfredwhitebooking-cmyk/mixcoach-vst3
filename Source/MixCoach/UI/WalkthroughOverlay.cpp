#include "WalkthroughOverlay.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constants
    // ═══════════════════════════════════════════════════════════════════════════
    namespace WalkthroughColours {
        inline juce::Colour dimBg()       { return juce::Colours::black.withAlpha(0.65f); }
        inline juce::Colour tooltipBg()   { return juce::Colour(0xFF1E1B2E).withAlpha(0.95f); }
        inline juce::Colour tooltipBorder() { return MixCoachTheme::accent().withAlpha(0.35f); }
        inline juce::Colour titleText()   { return juce::Colours::white.withAlpha(0.95f); }
        inline juce::Colour bodyText()    { return juce::Colours::white.withAlpha(0.75f); }
        inline juce::Colour accentBar()   { return MixCoachTheme::accent().withAlpha(0.60f); }
        inline juce::Colour dotActive()   { return MixCoachTheme::accent(); }
        inline juce::Colour dotInactive() { return MixCoachTheme::textMuted().withAlpha(0.30f); }
        inline juce::Colour skipText()    { return MixCoachTheme::textMuted().withAlpha(0.50f); }
        inline juce::Colour skipHover()   { return MixCoachTheme::textPrimary(); }
    }

    namespace WalkthroughLayout {
        constexpr int kTooltipW      = 280;
        constexpr int kTooltipH      = 160;
        constexpr float kCornerRadius = 10.0f;
        constexpr float kArrowSize   = 10.0f;
        constexpr float kTitleFont   = 15.0f;
        constexpr float kBodyFont    = 12.0f;
        constexpr float kIconFont    = 28.0f;
        constexpr float kDotSize     = 7.0f;
        constexpr float kDotGap      = 10.0f;
        constexpr int   kDotBottomY  = 30;
        constexpr float kSkipFont    = 11.0f;
        constexpr float kHighlightCorner = 6.0f;
        constexpr float kHighlightGlow   = 3.0f;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor — Define los 5 pasos del tutorial
    // ═══════════════════════════════════════════════════════════════════════════
    WalkthroughOverlay::WalkthroughOverlay()
    {
        setInterceptsMouseClicks(true, false);
        setVisible(false);
        setOpaque(false);

        // ─── Paso 0: El Coach (avatar + chat) ─────────────────────────────
        steps_[0].title      = "El Coach";
        steps_[0].description = "Este es tu ingeniero de mezcla personal. "
                                "Te guiará durante toda la sesi\\xC3\\xB3n, "
                                "detectar\\xC3\\xA1 problemas y te dar\\xC3\\xA1 "
                                "recomendaciones paso a paso.";
        steps_[0].icon       = "\\xF0\\x9F\\xA4\\x96";
        steps_[0].tooltipAnchorX = 0.5f;
        steps_[0].tooltipAnchorY = 1.0f; // Abajo del highlight

        // ─── Paso 1: El Mapa de Sesión (MixMap) ────────────────────────────
        steps_[1].title      = "Mapa de Sesi\\xC3\\xB3n";
        steps_[1].description = "Aqu\\xC3\\xAD ver\\xC3\\xA1s el mapa completo de tu "
                                "sesi\\xC3\\xB3n con todas las pistas, sus roles, "
                                "niveles y colores. Revisa que el ruteo sea "
                                "correcto antes de empezar.";
        steps_[1].icon       = "\\xF0\\x9F\\x97\\xBA";
        steps_[1].tooltipAnchorX = 0.5f;
        steps_[1].tooltipAnchorY = 1.0f;

        // ─── Paso 2: Recomendaciones (TrackProblemCard) ─────────────────────
        steps_[2].title      = "Recomendaciones";
        steps_[2].description = "El coach te mostrar\\xC3\\xA1 problemas detectados en "
                                "tarjetas como esta. Haz clic en \\\"Aplicado\\\" "
                                "cuando hagas el ajuste, o \\\"Explicar\\\" para "
                                "m\\xC3\\xA1s detalles t\\xC3\\xA9cnicos.";
        steps_[2].icon       = "\\xF0\\x9F\\x93\\x8C";
        steps_[2].tooltipAnchorX = 0.5f;
        steps_[2].tooltipAnchorY = 1.0f;

        // ─── Paso 3: Referencia Profesional ─────────────────────────────────
        steps_[3].title      = "Referencia";
        steps_[3].description = "Puedes cargar una canci\\xC3\\xB3n de referencia "
                                "para comparar tu mezcla. El coach te mostrar\\xC3\\xA1 "
                                "en qu\\xC3\\xA9 porcentaje te est\\xC3\\xA1s acercando "
                                "al sonido profesional que eliges.";
        steps_[3].icon       = "\\xF0\\x9F\\x8E\\xB5";
        steps_[3].tooltipAnchorX = 0.5f;
        steps_[3].tooltipAnchorY = 1.0f;

        // ─── Paso 4: Comandos Rápidos ──────────────────────────────────────
        steps_[4].title      = "Comandos R\\xC3\\xA1pidos";
        steps_[4].description = "Usa /ok, /skip, /why o los atajos de teclado "
                                "(Ctrl+Enter) para interactuar sin escribir. "
                                "\\xC2\\xA1El coach responde al instante!";
        steps_[4].icon       = "\\xE2\\x8C\\xA8";
        steps_[4].tooltipAnchorX = 0.5f;
        steps_[4].tooltipAnchorY = 0.0f; // Arriba del highlight
    }

    WalkthroughOverlay::~WalkthroughOverlay()
    {
        stopTimer();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  startWalkthrough — Inicia desde el paso 0
    // ═══════════════════════════════════════════════════════════════════════════
    void WalkthroughOverlay::startWalkthrough()
    {
        currentStep_ = 0;
        animProgress_ = 0.0f;
        animating_ = true;
        setAlpha(0.0f);
        setVisible(true);
        toFront(true);
        startTimerHz(60);
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  advanceStep — Siguiente paso o completar
    // ═══════════════════════════════════════════════════════════════════════════
    void WalkthroughOverlay::advanceStep()
    {
        if (currentStep_ < 0) return;

        if (currentStep_ >= kNumSteps - 1) {
            // Último paso → completar
            currentStep_ = -1;
            setVisible(false);
            animating_ = false;
            stopTimer();
            if (onComplete)
                onComplete();
            return;
        }

        // Avanzar con fade
        currentStep_++;
        animProgress_ = 0.0f;
        animating_ = true;
        startTimerHz(60);
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  previousStep — Retrocede un paso
    // ═══════════════════════════════════════════════════════════════════════════
    void WalkthroughOverlay::previousStep()
    {
        if (currentStep_ <= 0) return;
        currentStep_--;
        animProgress_ = 0.0f;
        animating_ = true;
        startTimerHz(60);
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  skipWalkthrough — Salta todo el tutorial
    // ═══════════════════════════════════════════════════════════════════════════
    void WalkthroughOverlay::skipWalkthrough()
    {
        currentStep_ = -1;
        setVisible(false);
        animating_ = false;
        stopTimer();
        if (onSkipped)
            onSkipped();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  restartWalkthrough — Vuelve al paso 0
    // ═══════════════════════════════════════════════════════════════════════════
    void WalkthroughOverlay::restartWalkthrough()
    {
        startWalkthrough();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateStepPositions — Actualiza áreas de highlight según el layout
    // ═══════════════════════════════════════════════════════════════════════════
    void WalkthroughOverlay::updateStepPositions(juce::Rectangle<int> parentBounds)
    {
        auto bounds = parentBounds;
        int w = bounds.getWidth();
        int h = bounds.getHeight();

        // Paso 0: El Coach — esquina superior izquierda (avatar + chat header)
        steps_[0].highlightArea = juce::Rectangle<int>(
            bounds.getX() + 10, bounds.getY() + 10,
            juce::jmin(180, w - 20), 60);
        steps_[0].tooltipArea = juce::Rectangle<int>(
            bounds.getCentreX() - WalkthroughLayout::kTooltipW / 2,
            steps_[0].highlightArea.getBottom() + 20,
            WalkthroughLayout::kTooltipW, WalkthroughLayout::kTooltipH);

        // Paso 1: Mapa de Sesión — centro izquierda (MixMap panel)
        steps_[1].highlightArea = juce::Rectangle<int>(
            bounds.getX() + 10, bounds.getCentreY() - 40,
            juce::jmin(200, w - 20), 80);
        steps_[1].tooltipArea = juce::Rectangle<int>(
            bounds.getCentreX() - WalkthroughLayout::kTooltipW / 2,
            steps_[1].highlightArea.getBottom() + 20,
            WalkthroughLayout::kTooltipW, WalkthroughLayout::kTooltipH);

        // Paso 2: Recomendaciones — centro (track problem cards)
        steps_[2].highlightArea = juce::Rectangle<int>(
            bounds.getCentreX() - 100, bounds.getCentreY() - 30,
            200, 60);
        steps_[2].tooltipArea = juce::Rectangle<int>(
            bounds.getCentreX() - WalkthroughLayout::kTooltipW / 2,
            steps_[2].highlightArea.getBottom() + 20,
            WalkthroughLayout::kTooltipW, WalkthroughLayout::kTooltipH);

        // Paso 3: Referencia — esquina superior derecha
        steps_[3].highlightArea = juce::Rectangle<int>(
            bounds.getRight() - 160, bounds.getY() + 10,
            150, 40);
        steps_[3].tooltipArea = juce::Rectangle<int>(
            bounds.getCentreX() - WalkthroughLayout::kTooltipW / 2,
            steps_[3].highlightArea.getBottom() + 20,
            WalkthroughLayout::kTooltipW, WalkthroughLayout::kTooltipH);

        // Paso 4: Comandos Rápidos — abajo (footer/chat input)
        steps_[4].highlightArea = juce::Rectangle<int>(
            bounds.getCentreX() - 120, bounds.getBottom() - 50,
            240, 40);
        steps_[4].tooltipArea = juce::Rectangle<int>(
            bounds.getCentreX() - WalkthroughLayout::kTooltipW / 2,
            steps_[4].highlightArea.getY() - WalkthroughLayout::kTooltipH - 20,
            WalkthroughLayout::kTooltipW, WalkthroughLayout::kTooltipH);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  visibilityChanged — Timer guard
    // ═══════════════════════════════════════════════════════════════════════════
    void WalkthroughOverlay::visibilityChanged()
    {
        if (!isVisible() && animating_) {
            animating_ = false;
            stopTimer();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  resized — Actualizar posiciones de los pasos
    // ═══════════════════════════════════════════════════════════════════════════
    void WalkthroughOverlay::resized()
    {
        updateStepPositions(getLocalBounds());
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  mouseDown — Clic avanza paso / skip si clic fuera
    // ═══════════════════════════════════════════════════════════════════════════
    void WalkthroughOverlay::mouseDown(const juce::MouseEvent& e)
    {
        if (currentStep_ < 0) return;

        auto pos = e.getPosition();

        // Check si hizo clic en "Saltar tutorial"
        auto bounds = getLocalBounds();
        auto skipArea = juce::Rectangle<int>(
            bounds.getCentreX() - 60,
            bounds.getBottom() - WalkthroughLayout::kDotBottomY - 15,
            120, 25);
        if (skipArea.contains(pos)) {
            skipWalkthrough();
            return;
        }

        // Clic en cualquier lado → avanzar
        advanceStep();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  timerCallback — 60fps: animación de fade entre pasos
    // ═══════════════════════════════════════════════════════════════════════════
    void WalkthroughOverlay::timerCallback()
    {
        if (!animating_) {
            stopTimer();
            return;
        }

        animProgress_ += WalkthroughLayout::kCornerRadius > 0 ? kFadeStep : kFadeStep;
        if (animProgress_ >= 1.0f) {
            animProgress_ = 1.0f;
            animating_ = false;
            stopTimer();
        }

        float eased = easeOutQuad(animProgress_);
        setAlpha(eased);
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Renderiza el overlay completo
    // ═══════════════════════════════════════════════════════════════════════════
    void WalkthroughOverlay::paint(juce::Graphics& g)
    {
        if (currentStep_ < 0) return;

        // ═══ 1. Fondo oscuro con cutout ═══════════════════════════════════
        drawDimOverlay(g);

        // ═══ 2. Cutout highlight + glow ══════════════════════════════════
        drawHighlightCutout(g);

        // ═══ 3. Tooltip del paso actual ═══════════════════════════════════
        drawStepTooltip(g);

        // ═══ 4. Indicador de progreso (dots) ═════════════════════════════
        drawStepDots(g);

        // ═══ 5. Botón "Saltar tutorial" ═══════════════════════════════════
        drawSkipButton(g);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawDimOverlay — Fondo oscuro semi-transparente con even-odd cutout
    // ═══════════════════════════════════════════════════════════════════════════
    void WalkthroughOverlay::drawDimOverlay(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        auto& step = steps_[currentStep_];
        auto highlight = step.highlightArea.toFloat();

        // Crear path con even-odd fill: outer (oscuro), inner (transparente)
        juce::Path path;
        path.setUsingNonZeroWinding(false);
        path.addRectangle(bounds);
        path.addRoundedRectangle(highlight, WalkthroughLayout::kHighlightCorner);

        float alpha = getAlpha();
        g.setColour(WalkthroughColours::dimBg().withAlpha(alpha));
        g.fillPath(path);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawHighlightCutout — Borde glow alrededor del área destacada
    // ═══════════════════════════════════════════════════════════════════════════
    void WalkthroughOverlay::drawHighlightCutout(juce::Graphics& g)
    {
        if (currentStep_ < 0) return;
        auto& step = steps_[currentStep_];
        auto highlight = step.highlightArea.toFloat();
        float alpha = getAlpha();

        // Outer glow (más ancho, más tenue)
        auto outerBounds = highlight.expanded(WalkthroughLayout::kHighlightGlow + 2.0f,
                                               WalkthroughLayout::kHighlightGlow + 2.0f);
        g.setColour(MixCoachTheme::accent().withAlpha(0.20f * alpha));
        g.drawRoundedRectangle(outerBounds,
                               WalkthroughLayout::kHighlightCorner + 2.0f,
                               2.0f);

        // Inner glow (más fino, más brillante)
        g.setColour(MixCoachTheme::accent().withAlpha(0.45f * alpha));
        g.drawRoundedRectangle(highlight,
                               WalkthroughLayout::kHighlightCorner,
                               1.5f);

        // White core
        auto innerBounds = highlight.reduced(0.5f, 0.5f);
        g.setColour(juce::Colours::white.withAlpha(0.12f * alpha));
        g.drawRoundedRectangle(innerBounds,
                               WalkthroughLayout::kHighlightCorner - 0.5f,
                               0.8f);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawStepTooltip — Tooltip con título, descripción, icono
    // ═══════════════════════════════════════════════════════════════════════════
    void WalkthroughOverlay::drawStepTooltip(juce::Graphics& g)
    {
        if (currentStep_ < 0) return;
        auto& step = steps_[currentStep_];
        auto area = step.tooltipArea;
        float alpha = getAlpha();

        // ─── Sombra ───────────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.35f * alpha));
        g.fillRoundedRectangle(area.toFloat().translated(0.0f, 3.0f),
                               WalkthroughLayout::kCornerRadius);

        // ─── Fondo del tooltip ────────────────────────────────────────────
        juce::ColourGradient grad(
            WalkthroughColours::tooltipBg().withAlpha(alpha),
            (float)area.getCentreX(), (float)area.getY(),
            WalkthroughColours::tooltipBg().darker(0.15f).withAlpha(alpha),
            (float)area.getCentreX(), (float)area.getBottom(),
            false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(area.toFloat(), WalkthroughLayout::kCornerRadius);

        // ─── Border sutil ─────────────────────────────────────────────────
        g.setColour(WalkthroughColours::tooltipBorder().withAlpha(alpha * 0.8f));
        g.drawRoundedRectangle(area.toFloat(), WalkthroughLayout::kCornerRadius, 0.8f);

        // ─── Accent bar (left edge) ───────────────────────────────────────
        g.setColour(WalkthroughColours::accentBar().withAlpha(alpha));
        g.fillRoundedRectangle(
            juce::Rectangle<float>((float)area.getX() + 2.0f,
                                   (float)area.getY() + 6.0f,
                                   2.5f,
                                   (float)area.getHeight() - 12.0f),
            1.5f);

        // ─── Layout interno ───────────────────────────────────────────────
        auto innerArea = area.reduced(16, 12);
        int iconSize = 32;

        // Icono (si existe)
        if (step.icon.isNotEmpty()) {
            auto iconArea = juce::Rectangle<int>(
                innerArea.getX(), innerArea.getY(),
                iconSize, iconSize);
            g.setFont(juce::Font(juce::FontOptions(WalkthroughLayout::kIconFont)));
            g.setColour(juce::Colours::white.withAlpha(alpha));
            g.drawText(step.icon, iconArea, juce::Justification::centred);

            // Texto después del icono
            innerArea.removeFromLeft(iconSize + 8);
        }

        // Título
        auto titleArea = innerArea.removeFromTop(22);
        g.setFont(juce::Font(juce::FontOptions(WalkthroughLayout::kTitleFont)).boldened());
        g.setColour(WalkthroughColours::titleText().withAlpha(alpha));
        g.drawText(step.title, titleArea, juce::Justification::centredLeft);

        // Separador delgado
        auto sepArea = innerArea.removeFromTop(6);
        g.setColour(MixCoachTheme::border().withAlpha(0.20f * alpha));
        g.fillRect(sepArea.removeFromTop(1));

        // Descripción
        g.setFont(juce::Font(juce::FontOptions(WalkthroughLayout::kBodyFont)));
        g.setColour(WalkthroughColours::bodyText().withAlpha(alpha));
        g.drawText(step.description, innerArea, juce::Justification::topLeft);

        // ─── Flecha direccional (triángulo apuntando al highlight) ────────
        // Calcular dirección según tooltipAnchorY
        float arrowX = (float)area.getCentreX();
        float arrowY;
        float arrowDir;

        if (step.tooltipAnchorY >= 0.5f) {
            // Tooltip debajo del highlight → flecha apunta hacia arriba
            arrowY = (float)area.getY() - WalkthroughLayout::kArrowSize;
            arrowDir = -1.0f;
        } else {
            // Tooltip arriba del highlight → flecha apunta hacia abajo
            arrowY = (float)area.getBottom();
            arrowDir = 1.0f;
        }

        juce::Path arrowPath;
        float as = WalkthroughLayout::kArrowSize;
        arrowPath.addTriangle(arrowX, arrowY,
                              arrowX - as, arrowY - arrowDir * as * 0.866f,
                              arrowX + as, arrowY - arrowDir * as * 0.866f);
        g.setColour(WalkthroughColours::tooltipBg().withAlpha(alpha));
        g.fillPath(arrowPath);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawStepDots — Indicador de progreso (●●●○○)
    // ═══════════════════════════════════════════════════════════════════════════
    void WalkthroughOverlay::drawStepDots(juce::Graphics& g)
    {
        if (currentStep_ < 0) return;
        float alpha = getAlpha();

        auto bounds = getLocalBounds();
        int totalW = (int)(kNumSteps * (WalkthroughLayout::kDotSize + WalkthroughLayout::kDotGap));
        int startX = bounds.getCentreX() - totalW / 2;
        int dotY = bounds.getBottom() - WalkthroughLayout::kDotBottomY;

        for (int i = 0; i < kNumSteps; ++i) {
            float cx = (float)(startX + i * (int)(WalkthroughLayout::kDotSize + WalkthroughLayout::kDotGap));
            float cy = (float)dotY;

            // Dot activo vs inactivo
            bool active = (i == currentStep_);
            float radius = active ? WalkthroughLayout::kDotSize * 0.6f
                                  : WalkthroughLayout::kDotSize * 0.4f;

            if (active) {
                // Active: glow + fill
                g.setColour(WalkthroughColours::dotActive().withAlpha(alpha));
                g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
                // Outer glow
                g.setColour(WalkthroughColours::dotActive().withAlpha(0.20f * alpha));
                float glowR = radius + 3.0f;
                g.fillEllipse(cx - glowR, cy - glowR, glowR * 2.0f, glowR * 2.0f);
            } else if (i < currentStep_) {
                // Completed: filled pero más tenue
                g.setColour(WalkthroughColours::dotActive().withAlpha(0.50f * alpha));
                g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
            } else {
                // Pending: solo borde
                g.setColour(WalkthroughColours::dotInactive().withAlpha(alpha));
                g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.0f);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawSkipButton — Texto "Saltar tutorial" en la parte inferior
    // ═══════════════════════════════════════════════════════════════════════════
    void WalkthroughOverlay::drawSkipButton(juce::Graphics& g)
    {
        if (currentStep_ < 0) return;
        float alpha = getAlpha();

        auto bounds = getLocalBounds();
        int buttonY = bounds.getBottom() - WalkthroughLayout::kDotBottomY - 28;
        auto skipArea = juce::Rectangle<int>(
            bounds.getCentreX() - 60, buttonY, 120, 20);

        g.setFont(juce::Font(juce::FontOptions(WalkthroughLayout::kSkipFont)));
        g.setColour(WalkthroughColours::skipText().withAlpha(alpha));
        g.drawText("Saltar tutorial \\xE2\\x86\\x92", skipArea,
                   juce::Justification::centred);
    }

} // namespace mixcoach
