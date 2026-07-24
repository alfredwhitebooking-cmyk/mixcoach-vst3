#include "VectorscopeComponent.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Trace colors — using MixCoachTheme accentCyan variants
    // ═══════════════════════════════════════════════════════════════════════════
    // (Local PhaseColors namespace removed — now using theme canonical colors)

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor
    // ═══════════════════════════════════════════════════════════════════════════
    VectorscopeComponent::VectorscopeComponent()
    {
        setOpaque(true);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  resized
    // ═══════════════════════════════════════════════════════════════════════════
    void VectorscopeComponent::resized()
    {
        gridCacheValid_ = false;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  plotCircleArea — Área del vectorscope circular ocupando todo el espacio
    // ═══════════════════════════════════════════════════════════════════════════
    juce::Rectangle<float> VectorscopeComponent::plotCircleArea() const
    {
        auto area = getLocalBounds().toFloat();
        area.reduce(2.0f, 2.0f);

        const float squareSize = juce::jmin(area.getWidth(), area.getHeight());
        return juce::Rectangle<float>(0.0f, 0.0f, squareSize, squareSize).withCentre(area.getCentre());
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  rebuildGridCache
    // ═══════════════════════════════════════════════════════════════════════════
    void VectorscopeComponent::rebuildGridCache()
    {
        const auto circleArea = plotCircleArea();
        if (circleArea.isEmpty()) {
            gridCacheValid_ = false;
            return;
        }

        const auto bounds = getLocalBounds();
        gridCache_        = juce::Image(juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
        gridCache_.clear(gridCache_.getBounds());

        juce::Graphics cg(gridCache_);

        // ─── Glass panel background ────────────────────────────────────────
        auto panelArea = circleArea.expanded(8.0f, 8.0f);
        MixCoachTheme::fillGlassPanel(cg, panelArea, 6.0f);

        // ─── Black circle background ───────────────────────────────────────
        cg.setColour(juce::Colour(0xFF0A0E1A));
        cg.fillEllipse(circleArea.reduced(1.0f));

        // ─── Draw grid ─────────────────────────────────────────────────────
        drawGrid(cg, circleArea);

        // ─── Border ────────────────────────────────────────────────────────
        cg.setColour(juce::Colour(0xFF1A2A44).withAlpha(0.40f));
        cg.drawEllipse(circleArea.reduced(1.0f), 1.0f);

        gridCacheValid_ = true;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawGrid — Goniometer premium con círculos concéntricos, crosshairs,
    //             diagonales 45°, tick marks cada 30°, y labels L/R/M/S
    // ═══════════════════════════════════════════════════════════════════════════
    void VectorscopeComponent::drawGrid(juce::Graphics& g, juce::Rectangle<float> area)
    {
        auto cx      = area.getCentreX();
        auto cy      = area.getCentreY();
        float radius = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f - 3.0f;

        // ─── Círculos concéntricos (6 anillos) ─────────────────────────────
        struct RingDef
        {
            float norm;
            float alpha;
            float width;
        };

        RingDef rings[] = {
            {0.15f, 0.04f, 0.3f},
            {0.30f, 0.06f, 0.4f},
            {0.50f, 0.08f, 0.4f},
            {0.70f, 0.10f, 0.5f},
            {0.85f, 0.14f, 0.5f},
            {1.00f, 0.25f, 1.0f},
        };

        for (auto& ring : rings) {
            float r              = radius * ring.norm;
            auto ringBounds      = juce::Rectangle<float>(cx - r, cy - r, r * 2.0f, r * 2.0f);
            juce::Colour ringCol = (ring.norm > 0.99f) ? juce::Colour(0xFF8899AA).withAlpha(ring.alpha)
                                                       : juce::Colour(0xFF666666).withAlpha(ring.alpha);
            g.setColour(ringCol);
            g.drawEllipse(ringBounds, ring.width);

            // Outer ring glow
            if (ring.norm > 0.99f) {
                g.setColour(MixCoachTheme::accent().withAlpha(0.06f));
                g.drawEllipse(ringBounds, 2.0f);
            }
        }

        // ─── Crosshairs con centro sutil ──────────────────────────────────────
        juce::Colour crossColor = juce::Colour(0xFF8899AA).withAlpha(0.15f);
        g.setColour(crossColor);
        g.drawHorizontalLine(juce::roundToInt(cy), area.getX() + 2, area.getRight() - 2);
        g.drawVerticalLine(juce::roundToInt(cx), area.getY() + 2, area.getBottom() - 2);

        // ─── Tick marks cada 30° en el anillo exterior ────────────────────────
        {
            g.setColour(juce::Colour(0xFF8899AA).withAlpha(0.12f));
            float outerR = radius - 2.0f;
            float innerR = radius - 6.0f;
            for (int deg = 0; deg < 360; deg += 30) {
                float rad  = juce::MathConstants<float>::pi * (float)deg / 180.0f;
                float cosA = std::cos(rad);
                float sinA = std::sin(rad);
                g.drawLine(cx + cosA * outerR, cy + sinA * outerR, cx + cosA * innerR, cy + sinA * innerR, 0.6f);
            }
        }

        // ─── Diagonales (45°) ──────────────────────────────────────────────
        g.setColour(juce::Colour(0xFF8899AA).withAlpha(0.08f));
        float d = radius * 0.707f;
        g.drawLine(cx - d, cy - d, cx + d, cy + d, 0.5f);
        g.drawLine(cx - d, cy + d, cx + d, cy - d, 0.5f);

        // ─── Center dot con glow ───────────────────────────────────────────
        {
            g.setColour(MixCoachTheme::accent().withAlpha(0.06f));
            g.fillEllipse(cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);
            g.setColour(juce::Colour(0xFF888888).withAlpha(0.25f));
            g.fillEllipse(cx - 1.8f, cy - 1.8f, 3.6f, 3.6f);
            g.setColour(juce::Colour(0xFFCCCCCC).withAlpha(0.15f));
            g.fillEllipse(cx - 0.8f, cy - 0.8f, 1.6f, 1.6f);
        }

        // ─── Labels de goniómetro acordes a la rotación M/S: ───────────────────
        //   M arriba (vertical = mono/en fase), L/R en las diagonales a 45°
        //   (posición canónica de canal aislado), S en los extremos horizontales
        //   (eje de lados / anti-fase).
        g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
        const float diag = radius * 0.72f * 0.70710678f; // proyección sobre cada eje

        // M - Mid (arriba) - morado
        g.setColour(MixCoachTheme::accent().withAlpha(0.70f));
        g.drawText(
            "M", juce::Rectangle<float>(cx - 8.0f, area.getY() + 1.0f, 16.0f, 12.0f), juce::Justification::centred);
        // L - diagonal superior izquierda - azul claro
        g.setColour(juce::Colour(0xFF60A5FA).withAlpha(0.70f));
        g.drawText("L",
                   juce::Rectangle<float>(cx - diag - 8.0f, cy - diag - 6.0f, 16.0f, 12.0f),
                   juce::Justification::centred);
        // R - diagonal superior derecha - verde
        g.setColour(juce::Colour(0xFF34D399).withAlpha(0.70f));
        g.drawText("R",
                   juce::Rectangle<float>(cx + diag - 8.0f, cy - diag - 6.0f, 16.0f, 12.0f),
                   juce::Justification::centred);
        // S - extremos horizontales (eje lateral / anti-fase) - cyan
        g.setColour(juce::Colour(0xFF00B7FF).withAlpha(0.55f));
        g.drawText(
            "S", juce::Rectangle<float>(area.getX() + 1.0f, cy - 6.0f, 14.0f, 12.0f), juce::Justification::centred);
        g.drawText("S",
                   juce::Rectangle<float>(area.getRight() - 15.0f, cy - 6.0f, 14.0f, 12.0f),
                   juce::Justification::centred);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawTrace — Trazado vibrante con color dinámico por correlación,
    //              phosphor trail multicapa, glow y partículas
    // ═══════════════════════════════════════════════════════════════════════════
    void VectorscopeComponent::drawTrace(juce::Graphics& g, juce::Rectangle<float> circleArea)
    {
        auto cx      = circleArea.getCentreX();
        auto cy      = circleArea.getCentreY();
        float radius = juce::jmin(circleArea.getWidth(), circleArea.getHeight()) * 0.5f - 3.0f;

        if (radius < 8.0f) return;

        // ─── Color dinámico basado en correlación ─────────────────────────
        juce::Colour traceColor;
        juce::Colour traceGlowColor;
        juce::Colour traceCoreColor;

        float corr = juce::jlimit(-1.0f, 1.0f, correlation_);
        if (corr < 0.0f) {
            float t        = (corr + 1.0f);
            traceColor     = juce::Colour(0xFFFF4444).interpolatedWith(juce::Colour(0xFFFFCC44), t);
            traceGlowColor = juce::Colour(0xFFCC2222).interpolatedWith(juce::Colour(0xFFAA8833), t);
            traceCoreColor = juce::Colour(0xFFFF8888).interpolatedWith(juce::Colour(0xFFFFEE88), t);
        }
        else if (corr < 0.5f) {
            float t        = corr / 0.5f;
            traceColor     = juce::Colour(0xFFFFCC44).interpolatedWith(juce::Colour(0xFF55DDFF), t);
            traceGlowColor = juce::Colour(0xFFAA8833).interpolatedWith(juce::Colour(0xFF22AACC), t);
            traceCoreColor = juce::Colour(0xFFFFEE88).interpolatedWith(juce::Colour(0xFFAAEEFF), t);
        }
        else {
            float t        = (corr - 0.5f) / 0.5f;
            traceColor     = juce::Colour(0xFF55DDFF).interpolatedWith(juce::Colour(0xFF44CC66), t);
            traceGlowColor = juce::Colour(0xFF22AACC).interpolatedWith(juce::Colour(0xFF228844), t);
            traceCoreColor = juce::Colour(0xFFAAEEFF).interpolatedWith(juce::Colour(0xFFAAEEAA), t);
        }

        // ─── Build paths ───────────────────────────────────────────────────
        oldTracePath_.clear();
        newTracePath_.clear();
        bool oldStarted = false;
        bool newStarted = false;

        for (int i = 0; i < kTraceLen; ++i) {
            int idx  = (writePos_ + i) % kTraceLen;
            auto& pt = trace_[idx];
            if (pt.alpha < 0.01f) continue;

            float sx = cx + pt.x * radius;
            float sy = cy + pt.y * radius;

            if (pt.alpha > 0.5f) {
                if (!newStarted) {
                    newTracePath_.startNewSubPath(sx, sy);
                    newStarted = true;
                }
                else {
                    newTracePath_.lineTo(sx, sy);
                }
            }
            else {
                if (!oldStarted) {
                    oldTracePath_.startNewSubPath(sx, sy);
                    oldStarted = true;
                }
                else {
                    oldTracePath_.lineTo(sx, sy);
                }
            }
        }

        // ─── Pasada 1: phosphor trail (partículas viejas) ──────────────────
        // Solo los trazos (baratos): las elipses por punto eran casi invisibles
        // (alpha ~0.12) y costaban cientos de fillEllipse/frame.
        if (oldStarted) {
            g.setColour(traceGlowColor.withAlpha(0.05f));
            g.strokePath(oldTracePath_, juce::PathStrokeType(5.0f));

            g.setColour(traceColor.withAlpha(0.08f));
            g.strokePath(oldTracePath_, juce::PathStrokeType(2.0f));
        }

        // ─── Pasada 2: trazo brillante actual (3 capas: glow, trace, core) ──
        if (newStarted) {
            g.setColour(traceGlowColor.withAlpha(0.15f));
            g.strokePath(newTracePath_, juce::PathStrokeType(5.0f));

            g.setColour(traceColor.withAlpha(0.75f));
            g.strokePath(newTracePath_, juce::PathStrokeType(2.2f));

            g.setColour(traceCoreColor.withAlpha(0.50f));
            g.strokePath(newTracePath_, juce::PathStrokeType(1.0f));

            // Partículas solo en la "cabeza" reciente del trazo (comet head),
            // recorriendo hacia atrás desde writePos_ en vez de los 1024 puntos.
            constexpr int kHeadParticles = 120;
            for (int j = 1; j <= kHeadParticles; ++j) {
                int idx  = (writePos_ - j + kTraceLen) % kTraceLen;
                auto& pt = trace_[idx];
                if (pt.alpha < 0.5f) continue;

                float sx       = cx + pt.x * radius;
                float sy       = cy + pt.y * radius;
                float glowSize = 2.0f + pt.alpha * 2.0f;

                g.setColour(traceColor.withAlpha(pt.alpha * 0.10f));
                g.fillEllipse(sx - glowSize, sy - glowSize, glowSize * 2.0f, glowSize * 2.0f);

                g.setColour(traceCoreColor.withAlpha(pt.alpha * 0.65f));
                g.fillEllipse(sx - 0.8f, sy - 0.8f, 1.6f, 1.6f);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Solo dibuja el fondo + grid cacheado y la traza en vivo
    //  NOTA: El panel padre (PhaseScopePanel) dibuja header, correlation meter
    //  y métricas. Este componente SOLO es el círculo,
    //  más el overlay de fase si hay diagnóstico activo.
    // ═══════════════════════════════════════════════════════════════════════════
    void VectorscopeComponent::paint(juce::Graphics& g)
    {
        // ─── Cached grid background ────────────────────────────────────────
        if (!gridCacheValid_) rebuildGridCache();
        if (gridCacheValid_) g.drawImageAt(gridCache_, 0, 0);

        // ─── Trace (no cacheable) ──────────────────────────────────────────
        auto circleArea = plotCircleArea();
        drawTrace(g, circleArea);

        // ─── Phase diagnostic overlay ──────────────────────────────────────
        if (hasPhaseDiagnostic_) drawPhaseOverlay(g, circleArea);

        // ─── Target correlation overlay (ideal circle) ────────────────────
        if (hasTargetCorrelation_) drawTargetOverlay(g, circleArea);

        // ─── Stereo width badge overlay (si hay datos relevantes) ───────────
        if (stereoWidth_ > 0.01f) {
            auto badgeBounds =
                juce::Rectangle<float>(circleArea.getRight() - 52.0f, circleArea.getBottom() - 20.0f, 48.0f, 16.0f);

            // Badge background
            g.setColour(juce::Colour(0xBB000000));
            g.fillRoundedRectangle(badgeBounds, MixCoachTheme::cornerRadius_small);

            // Color según zona
            juce::Colour wCol;
            if (stereoWidth_ < 0.15f) wCol = juce::Colour(0xFF556677);
            else if (stereoWidth_ < 0.40f)
                wCol = juce::Colour(0xFF44BBFF);
            else if (stereoWidth_ < 0.70f)
                wCol = juce::Colour(0xFF44CC66);
            else
                wCol = juce::Colour(0xFFFF6633);

            g.setColour(wCol.withAlpha(0.10f));
            g.fillRoundedRectangle(badgeBounds, MixCoachTheme::cornerRadius_small);

            // Badge border
            g.setColour(wCol.withAlpha(0.30f));
            g.drawRoundedRectangle(badgeBounds, MixCoachTheme::cornerRadius_small, 0.5f);

            // Badge text
            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.setColour(wCol);
            g.drawText("W " + juce::String(static_cast<int>(stereoWidth_ * 100.0f)) + "%",
                       badgeBounds,
                       juce::Justification::centred);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  pushSample
    // ═══════════════════════════════════════════════════════════════════════════
    void VectorscopeComponent::pushSample(float left, float right)
    {
        // Goniómetro profesional: rotación de 45° a coordenadas Mid/Side.
        //   M = (L+R)/√2  → eje vertical (arriba = mono / en fase)
        //   S = (L-R)/√2  → eje horizontal (anti-fase / lados)
        // El factor 1/√2 mantiene una señal L-solo (o R-solo) full-scale justo
        // sobre el borde, en la diagonal a 45° (posición canónica L/R).
        //   pantalla_x = (R-L)/√2  → L hacia la izquierda, R hacia la derecha
        //   pantalla_y = -(L+R)/√2 → mono apunta hacia ARRIBA
        constexpr float kInvSqrt2 = 0.70710678f;
        const float mid           = (left + right) * kInvSqrt2;
        const float side          = (left - right) * kInvSqrt2;

        auto& pt  = trace_[writePos_ % kTraceLen];
        pt.x      = juce::jlimit(-1.0f, 1.0f, -side);
        pt.y      = juce::jlimit(-1.0f, 1.0f, -mid);
        pt.alpha  = 1.0f;
        writePos_ = (writePos_ + 1) % kTraceLen;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setDisplayCorrelation
    // ═══════════════════════════════════════════════════════════════════════════
    void VectorscopeComponent::setDisplayCorrelation(float correlation)
    {
        const float old = correlation_;
        correlation_    = juce::jlimit(-1.0f, 1.0f, correlation);
        if (std::abs(correlation_ - old) > 0.005f) repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setPhaseDiagnostic — Recibe un diagnóstico de fase opcional
    //  Además activa automáticamente el target correlation overlay para
    //  mostrar visualmente cómo debería verse la correlación ideal.
    // ═══════════════════════════════════════════════════════════════════════════
    void VectorscopeComponent::setPhaseDiagnostic(const PhaseDiagnostic* diagnostic)
    {
        if (diagnostic == nullptr) {
            if (hasPhaseDiagnostic_) {
                hasPhaseDiagnostic_ = false;
                // Limpiar también el target overlay cuando se limpia el diagnóstico
                clearTargetCorrelation();
                repaint();
            }
            return;
        }

        phaseDiagnostic_    = *diagnostic;
        hasPhaseDiagnostic_ = true;

        // ═══ Auto-activar target correlation overlay ════════════════════════════
        // Cuando se muestra una advertencia de fase, automáticamente mostramos
        // el círculo ideal para que el usuario vea hacia dónde ir.
        // Usamos 0.85 como target ideal (estéreo balanceado con buena correlación).
        if (!hasTargetCorrelation_) {
            juce::String targetLabel;
            if (diagnostic->isWarning)
                targetLabel = "Objetivo: corr > 0.85";
            else if (diagnostic->isPraise)
                targetLabel = "Correlacion buena!";
            else
                targetLabel = "Target: 0.85";

            setTargetCorrelation(0.85f, targetLabel);
        }

        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawPhaseOverlay — Overlay de advertencia de fase en el vectorscope
    //  Dibuja un borde pulsante (warning glow) + texto de advertencia
    //  cuando hay problemas de correlación detectados por el coach.
    // ═══════════════════════════════════════════════════════════════════════════
    void VectorscopeComponent::drawPhaseOverlay(juce::Graphics& g, juce::Rectangle<float> circleArea)
    {
        const auto colour    = phaseDiagnostic_.getDisplayColour();
        const float severity = phaseDiagnostic_.severity;

        // ─── Warning glow exterior (pulsante según severidad) ──────────────
        {
            float glowAlpha = 0.08f + severity * 0.20f;
            float glowWidth = 4.0f + severity * 6.0f;

            // Glow exterior difuso
            g.setColour(colour.withAlpha(glowAlpha * 0.5f));
            g.drawEllipse(circleArea.reduced(-glowWidth), glowWidth * 2.0f);

            // Anillo de advertencia más brillante
            g.setColour(colour.withAlpha(glowAlpha));
            g.drawEllipse(circleArea.reduced(-glowWidth * 0.3f), glowWidth * 0.8f);

            // Borde interior del círculo coloreado según severidad
            g.setColour(colour.withAlpha(glowAlpha * 1.5f));
            g.drawEllipse(circleArea.reduced(2.0f), 2.0f + severity * 2.0f);
        }

        // ─── Warning text overlay en la parte superior del circle ──────────
        if (phaseDiagnostic_.isWarning || phaseDiagnostic_.description.isNotEmpty()) {
            float textY = circleArea.getY() + 8.0f;
            auto textArea =
                juce::Rectangle<float>(circleArea.getX() + 12.0f, textY, circleArea.getWidth() - 24.0f, 18.0f);

            // Badge background
            g.setColour(juce::Colour(0xBB000000));
            g.fillRoundedRectangle(textArea.expanded(4.0f, 2.0f), MixCoachTheme::cornerRadius_medium);

            // Texto
            g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
            g.setColour(colour);

            juce::String displayText = phaseDiagnostic_.description;
            if (displayText.isEmpty()) {
                if (phaseDiagnostic_.isWarning) displayText = "[WARN] Phase Warning";
                else if (phaseDiagnostic_.isPraise)
                    displayText = "[OK] Good Phase";
            }

            if (displayText.isNotEmpty()) g.drawText(displayText, textArea, juce::Justification::centredTop);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setTargetCorrelation — Activa el overlay de círculo ideal
    // ═══════════════════════════════════════════════════════════════════════════
    void VectorscopeComponent::setTargetCorrelation(float targetCorrelation,
                                                     const juce::String& label)
    {
        float newVal = juce::jlimit(0.0f, 1.0f, targetCorrelation);
        targetCorrelationLabel_ = label;
        if (!hasTargetCorrelation_) {
            targetCorrelation_ = newVal;
            hasTargetCorrelation_ = true;
            repaint();
        } else if (std::abs(newVal - targetCorrelation_) > 0.01f
                   || targetCorrelationLabel_ != label) {
            targetCorrelation_ = newVal;
            repaint();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  clearTargetCorrelation — Desactiva el overlay
    // ═══════════════════════════════════════════════════════════════════════════
    void VectorscopeComponent::clearTargetCorrelation()
    {
        if (hasTargetCorrelation_) {
            hasTargetCorrelation_ = false;
            repaint();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawTargetOverlay — Dibuja un círculo/corona ideal en el vectorscope
    //
    //  Muestra un círculo punteado que representa la correlación objetivo:
    //    - Círculo compacto para high correlation (> 0.8)
    //    - Elipse más ancha para correlaciones medias (0.5-0.8)
    //    - Forma más difusa para baja correlación
    //  El overlay late (pulso) suavemente cuando la correlación actual difiere
    //  del target, para ayudar al usuario a ver visualmente qué ajustar.
    // ═══════════════════════════════════════════════════════════════════════════
    void VectorscopeComponent::drawTargetOverlay(juce::Graphics& g,
                                                  juce::Rectangle<float> circleArea)
    {
        auto cx = circleArea.getCentreX();
        auto cy = circleArea.getCentreY();
        float maxR = juce::jmin(circleArea.getWidth(), circleArea.getHeight()) * 0.5f - 3.0f;

        if (maxR < 10.0f) return;

        // ─── Calcular el radio del círculo ideal basado en la correlación target ──
        // Alta correlación (>0.8) = círculo pequeño y centrado (Mono-dominante)
        // Media correlación (0.5-0.8) = círculo mediano (estéreo balanceado)
        // Baja correlación (<0.5) = círculo más grande (estéreo amplio)
        float idealRadius = maxR * (1.0f - targetCorrelation_ * 0.55f);
        idealRadius = juce::jlimit(maxR * 0.20f, maxR * 0.75f, idealRadius);

        // Calcular pulso: late si la correlación actual difiere del target
        float pulseAlpha = 1.0f;
        float corrDiff = std::abs(correlation_ - targetCorrelation_);
        if (corrDiff > 0.10f) {
            // Breathing pulse: oscila entre 0.5 y 1.0
            pulseAlpha = 0.5f + 0.5f * std::sin(targetPulsePhase_);
        }

        juce::Colour targetColour;
        if (targetCorrelation_ >= 0.8f)
            targetColour = juce::Colour(0xFF44CC66); // Verde saludable
        else if (targetCorrelation_ >= 0.5f)
            targetColour = juce::Colour(0xFF44BBFF); // Azul - aceptable
        else
            targetColour = juce::Colour(0xFFFFCC44); // Amarillo - bajo

        // ─── Círculo punteado (corona ideal) ─────────────────────────────────
        constexpr int kNumDots = 32;
        constexpr float kDashAngle = juce::MathConstants<float>::twoPi / kNumDots;

        juce::Path idealGlowPath;
        juce::Path idealDashPath;
        bool glowFirst = true;
        bool dashFirst = true;

        for (int d = 0; d < kNumDots; ++d) {
            float angle = d * kDashAngle;
            float dotX = cx + idealRadius * std::cos(angle);
            float dotY = cy + idealRadius * std::sin(angle);

            float normAngle = angle / juce::MathConstants<float>::twoPi;
            float fractional = normAngle - std::floor(normAngle);

            if (fractional < 0.75f) {
                if (glowFirst) {
                    idealGlowPath.startNewSubPath(dotX, dotY);
                    glowFirst = false;
                } else {
                    idealGlowPath.lineTo(dotX, dotY);
                }

                if (dashFirst) {
                    idealDashPath.startNewSubPath(dotX, dotY);
                    dashFirst = false;
                } else {
                    idealDashPath.lineTo(dotX, dotY);
                }
            } else {
                glowFirst = true;
                dashFirst = true;
            }
        }

        // Glow exterior pulsante
        g.setColour(targetColour.withAlpha(0.08f * pulseAlpha));
        g.strokePath(idealGlowPath, juce::PathStrokeType(4.5f));

        // Línea punteada principal (late suavemente si diff > 0.10)
        float dashAlpha = corrDiff > 0.10f
            ? juce::jlimit(0.35f, 0.75f, 0.55f * pulseAlpha)
            : 0.55f;
        g.setColour(targetColour.withAlpha(dashAlpha));
        g.strokePath(idealDashPath, juce::PathStrokeType(1.8f));

        // ─── Label del target ────────────────────────────────────────────────
        juce::String label;
        if (targetCorrelationLabel_.isNotEmpty())
            label = targetCorrelationLabel_;
        else
            label = "Target corr: " + juce::String(targetCorrelation_, 2);

        auto labelBounds = juce::Rectangle<float>(
            circleArea.getX() + 4.0f,
            circleArea.getBottom() - 20.0f,
            160.0f, 16.0f);

        // Badge background
        g.setColour(juce::Colour(0xBB000000));
        g.fillRoundedRectangle(labelBounds.expanded(2.0f, 1.0f), MixCoachTheme::cornerRadius_small);
        g.setColour(targetColour.withAlpha(0.12f * pulseAlpha));
        g.fillRoundedRectangle(labelBounds.expanded(2.0f, 1.0f), MixCoachTheme::cornerRadius_small);
        g.setColour(targetColour.withAlpha(0.30f * pulseAlpha));
        g.drawRoundedRectangle(labelBounds.expanded(2.0f, 1.0f), MixCoachTheme::cornerRadius_small, 0.5f);

        g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
        g.setColour(targetColour);
        g.drawText(label, labelBounds, juce::Justification::centredLeft);

        // ─── Flecha de "dirección" pulsante: indicador visual de cómo el
        //     usuario puede mover la correlación hacia el target ────────────
        float currentRadius = maxR * (1.0f - correlation_ * 0.55f);
        currentRadius = juce::jlimit(maxR * 0.20f, maxR * 0.75f, currentRadius);

        float radiusDiff = idealRadius - currentRadius;
        if (std::abs(radiusDiff) > maxR * 0.04f) {
            float arrowY = cy - maxR * 0.55f;
            float startX, endX;
            juce::Colour arrowColour;

            float arrowAlpha = 0.40f * pulseAlpha;

            if (radiusDiff > 0) {
                startX = cx + currentRadius + 6.0f;
                endX = cx + idealRadius - 2.0f;
                arrowColour = juce::Colour(0xFFFFCC44); // Amarillo — expandir
            } else {
                startX = cx + idealRadius + 2.0f;
                endX = cx + currentRadius - 6.0f;
                arrowColour = juce::Colour(0xFF44CC66); // Verde — contraer
            }

            if (endX > startX + 4.0f) {
                g.setColour(arrowColour.withAlpha(arrowAlpha));
                g.drawArrow(juce::Line<float>(startX, arrowY, endX, arrowY),
                            2.0f, 4.0f, 4.0f);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  advanceFrame — Decae el phosphor trail y avanza la fase de pulso
    // ═══════════════════════════════════════════════════════════════════════════
    bool VectorscopeComponent::advanceFrame(double /*sampleRateHz*/, bool allowRepaint)
    {
        bool hasTrace = false;
        for (auto& p : trace_) {
            if (p.alpha > 0.01f) hasTrace = true;
            p.alpha *= 0.970f;
        }

        // ─── Avanzar fase de pulso del target overlay ────────────────────────
        // Late a ~0.66 Hz (2π / ~9.5 frames a 60fps ≈ 0.66 Hz)
        targetPulsePhase_ += 0.045f;
        if (targetPulsePhase_ > juce::MathConstants<float>::twoPi * 10.0f)
            targetPulsePhase_ -= juce::MathConstants<float>::twoPi * 10.0f;

        if (hasTrace && allowRepaint) repaint();

        return hasTrace;
    }

} // namespace mixcoach
