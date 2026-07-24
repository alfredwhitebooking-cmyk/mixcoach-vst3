#include "ModeSelectionCard.h"
#include "../../Common/types/LogHelper.h"
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  ModeSelectionCard — Tarjeta visual grande para elegir modo Mix/Master
    // ═══════════════════════════════════════════════════════════════════════════

    ModeSelectionCard::ModeSelectionCard(ModeType type)
        : type_(type)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setSize(260, 320); // Default size: 260x320 antes de que Layout la posicione
        animStartMs_ = juce::Time::getMillisecondCounter();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  timerCallback — Animación de entrada slide-up + fade-in (200ms)
    // ═══════════════════════════════════════════════════════════════════════════
    void ModeSelectionCard::timerCallback()
    {
        int64_t elapsed = juce::Time::getMillisecondCounter() - animStartMs_;
        float t = juce::jmin(1.0f, (float)elapsed / kAnimDurationMs);

        if (t >= 1.0f) {
            setAlpha(1.0f);
            setTransform(juce::AffineTransform());
            stopTimer();
            return;
        }

        // Ease-out quad: t_eased = 1 - (1-t)^2
        float eased = 1.0f - (1.0f - t) * (1.0f - t);
        setAlpha(eased);

        // Slide-up: empieza 30px abajo, termina en posición normal
        float slideY = 30.0f * (1.0f - eased);
        setTransform(juce::AffineTransform::translation(0.0f, slideY));

        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Metadata estática
    // ═══════════════════════════════════════════════════════════════════════════

    juce::String ModeSelectionCard::modeTitle(ModeType type) noexcept
    {
        switch (type) {
            case ModeType::Mix:    return "Mezclar";
            case ModeType::Master: return "Masterizar";
            default:               return {};
        }
    }

    juce::String ModeSelectionCard::modeDescription(ModeType type) noexcept
    {
        switch (type) {
            case ModeType::Mix:
                return "Trabaja en el balance, claridad y espacio de cada elemento.";
            case ModeType::Master:
                return "Optimiza el sonido final para que suene fuerte y profesional.";
            default:
                return {};
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Dibuja la tarjeta completa con todos sus elementos visuales
    // ═══════════════════════════════════════════════════════════════════════════
    void ModeSelectionCard::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        const float cr = 12.0f;

        // ─── Sombra profunda ──────────────────────────────────────────────────
        float shadowAlpha = selected_ ? 0.35f : (hovered_ ? 0.28f : 0.20f);
        g.setColour(juce::Colours::black.withAlpha(shadowAlpha));
        g.fillRoundedRectangle(bounds.expanded(2.0f, 3.0f), cr + 2.0f);

        // ─── Glow hover (expansivo) ───────────────────────────────────────────
        if (hovered_ || selected_) {
            float glowAlpha = selected_ ? 0.12f : 0.06f;
            g.setColour(MixCoachTheme::accent().withAlpha(glowAlpha));
            g.fillRoundedRectangle(bounds.expanded(5.0f, 5.0f), cr + 4.0f);

            if (hovered_) {
                g.setColour(MixCoachTheme::accent().withAlpha(0.03f));
                g.fillRoundedRectangle(bounds.expanded(8.0f, 8.0f), cr + 6.0f);
            }
        }

        // ─── Fondo principal: card gradient con variables de tema ──────────────
            juce::ColourGradient bgGrad(MixCoachTheme::bgCard(),
                                         bounds.getX(), bounds.getY(),
                                         hovered_ ? MixCoachTheme::bgSurface() : MixCoachTheme::bgDarker(),
                                         bounds.getX(), bounds.getBottom(),
                                         false);
            if (hovered_ || selected_) {
                bgGrad.addColour(0.5f, MixCoachTheme::gradientMid());
            }
            g.setGradientFill(bgGrad);
            g.fillRoundedRectangle(bounds, cr);

        // ─── Glass highlight superior ─────────────────────────────────────────
        auto glassTop = bounds.withHeight(bounds.getHeight() * 0.25f);
        juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(hovered_ ? 0.06f : 0.04f),
                                       glassTop.getX(), glassTop.getY(),
                                       juce::Colour(0x00000000),
                                       glassTop.getX(), glassTop.getBottom(),
                                       false);
        g.setGradientFill(glassGrad);
        g.fillRoundedRectangle(glassTop, cr);

        // ─── Borde ────────────────────────────────────────────────────────────
        if (selected_) {
            g.setColour(MixCoachTheme::accent().withAlpha(0.60f));
            g.drawRoundedRectangle(bounds, cr, 1.2f);
            g.setColour(MixCoachTheme::accent().withAlpha(0.20f));
            g.drawRoundedRectangle(bounds.expanded(1.0f, 1.0f), cr + 0.5f, 0.5f);
        } else if (hovered_) {
            g.setColour(MixCoachTheme::accent().withAlpha(0.35f));
            g.drawRoundedRectangle(bounds, cr, 1.0f);
        } else {
            g.setColour(juce::Colours::white.withAlpha(0.08f));
            g.drawRoundedRectangle(bounds, cr, 0.8f);
        }

        auto area = bounds.reduced(20, 16);

        // ═══ SECCIÓN 1: Ícono grande custom (180px height) ═════════════════
        auto iconArea = area.removeFromTop(180);
        if (type_ == ModeType::Mix)
            drawMixIcon(g, iconArea);
        else
            drawMasterIcon(g, iconArea);

        area.removeFromTop(12);

        // ═══ SECCIÓN 2: Título (acentuado) ═════════════════════════════════
        auto titleArea = area.removeFromTop(36);
        g.setFont(juce::Font(juce::FontOptions(36.0f)).boldened());
        g.setColour(MixCoachTheme::accent());
        g.drawText(modeTitle(type_), titleArea, juce::Justification::centred);

        // ═══ SECCIÓN 3: Descripción (textDim) ═════════════════════════════
        area.removeFromTop(4);
        auto descArea = area.removeFromTop(40);
        g.setFont(juce::Font(juce::FontOptions(14.0f)));
        g.setColour(MixCoachTheme::textDim());
        g.drawText(modeDescription(type_), descArea, juce::Justification::centred);

        // ═══ SECCIÓN 4: Flecha indicadora (hover/selected) ════════════════
        if (hovered_ || selected_) {
            float arrowCx = bounds.getCentreX();
            float arrowBottom = bounds.getBottom() - 14.0f;
            float arrowSize = 12.0f;

            juce::Path arrowPath;
            arrowPath.startNewSubPath(arrowCx - arrowSize, arrowBottom - arrowSize);
            arrowPath.lineTo(arrowCx, arrowBottom);
            arrowPath.lineTo(arrowCx + arrowSize, arrowBottom - arrowSize);

            g.setColour(MixCoachTheme::accent().withAlpha(0.5f));
            g.strokePath(arrowPath, juce::PathStrokeType(2.0f));

            g.setColour(MixCoachTheme::accent().withAlpha(0.7f));
            g.fillEllipse(arrowCx - 2.5f, arrowBottom - 2.5f, 5.0f, 5.0f);
        }
    }

    void ModeSelectionCard::resized()
    {
        // No child components — todo se dibuja en paint()
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Mouse events — Hover + Click
    // ═══════════════════════════════════════════════════════════════════════════
    void ModeSelectionCard::mouseEnter(const juce::MouseEvent&)
    {
        hovered_ = true;
        repaint();
    }

    void ModeSelectionCard::mouseExit(const juce::MouseEvent&)
    {
        hovered_ = false;
        repaint();
    }

    void ModeSelectionCard::mouseDown(const juce::MouseEvent& e)
    {
        LogHelper::writeToLog("[DIAG] ModeSelectionCard::mouseDown - bounds="
                              + juce::String(getWidth()) + "x" + juce::String(getHeight())
                              + " visible=" + (isVisible() ? "Y" : "N"));
        if (onCardSelected) {
            LogHelper::writeToLog("[DIAG] ModeSelectionCard::mouseDown - onCardSelected is SET, calling it");
            onCardSelected();
        } else {
            LogHelper::writeToLog("[DIAG] ModeSelectionCard::mouseDown - onCardSelected is NULL");
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawMixIcon — 3 faders verticales con glow morado + waveform difuso
    // ═══════════════════════════════════════════════════════════════════════════
    void ModeSelectionCard::drawMixIcon(juce::Graphics& g, juce::Rectangle<float> area)
    {
        float cx = area.getCentreX();
        float cy = area.getCentreY();

        // ─── Waveform de fondo difuso ─────────────────────────────────────────
        float waveW = area.getWidth() * 0.75f;
        float waveX0 = cx - waveW * 0.5f;
        juce::Path bgWave;
        bgWave.startNewSubPath(waveX0, cy + 20.0f);
        for (int i = 0; i <= 30; ++i) {
            float t = (float)i / 30.0f;
            float x = waveX0 + waveW * t;
            float y = cy + 20.0f
                      + std::sin(t * juce::MathConstants<float>::twoPi * 2.5f) * 14.0f
                      + std::sin(t * juce::MathConstants<float>::twoPi * 5.0f) * 6.0f;
            bgWave.lineTo(x, y);
        }
        g.setColour(MixCoachTheme::accent().withAlpha(0.08f));
        g.strokePath(bgWave, juce::PathStrokeType(1.5f));

        // ─── 3 Faders ─────────────────────────────────────────────────────────
        struct FaderDef {
            float heightRatio; // 0.0 - 1.0 normalized
            float xOffset;     // relative to center
        };
        FaderDef faders[] = {
            { 0.60f, -40.0f },
            { 0.80f,   0.0f },
            { 0.45f,  40.0f }
        };

        float faderW = 6.0f;
        float faderMaxH = area.getHeight() * 0.60f;
        float faderBottom = cy + 24.0f;
        float trackW = 2.0f;

        for (auto& fd : faders) {
            float fx = cx + fd.xOffset;
            float faderH = faderMaxH * fd.heightRatio;
            float faderTop = faderBottom - faderH;

            // ─── Track (línea vertical base) ────────────────────────────────
            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.fillRect(fx - trackW * 0.5f, faderTop, trackW, faderH);

            // ─── Fader body (glow) ──────────────────────────────────────────
            juce::ColourGradient faderGrad(MixCoachTheme::accent().withAlpha(0.50f),
                                            fx, faderTop,
                                            MixCoachTheme::accent().withAlpha(0.15f),
                                            fx, faderBottom, false);
            g.setGradientFill(faderGrad);
            g.fillRect(fx - faderW * 0.5f, faderTop, faderW, faderH);

            // ─── Cap circular (thumb) ──────────────────────────────────────
            float thumbR = 5.0f;
            float thumbY = faderBottom - 4.0f;
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.fillEllipse(fx - thumbR, thumbY - thumbR, thumbR * 2.0f, thumbR * 2.0f);
            g.setColour(MixCoachTheme::accent().withAlpha(0.30f));
            g.drawEllipse(fx - thumbR, thumbY - thumbR, thumbR * 2.0f, thumbR * 2.0f, 0.8f);
        }

        // ─── Label sutil debajo de los faders ────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.setColour(MixCoachTheme::textDim().withAlpha(0.5f));
        g.drawText("GAIN  PAN  VOL", juce::Rectangle<float>(cx - 60.0f, faderBottom + 4.0f, 120.0f, 16.0f),
                   juce::Justification::centred);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawMasterIcon — Espectro de barras verticales + knob central
    // ═══════════════════════════════════════════════════════════════════════════
    void ModeSelectionCard::drawMasterIcon(juce::Graphics& g, juce::Rectangle<float> area)
    {
        float cx = area.getCentreX();
        float cy = area.getCentreY();

        // ─── Espectro de barras (7-9 barras estilo montaña) ──────────────────
        constexpr int kNumBars = 9;
        float barW = 8.0f;
        float totalW = kNumBars * barW + (kNumBars - 1) * 3.0f;
        float startX = cx - totalW * 0.5f;
        float barMaxH = area.getHeight() * 0.50f;
        float barBottom = cy + 2.0f;

        // Perfil de montaña: centro más alto, bordes más bajos
        float barHeights[kNumBars] = {
            0.30f, 0.45f, 0.65f, 0.85f, 1.00f, 0.85f, 0.65f, 0.45f, 0.30f
        };

        for (int i = 0; i < kNumBars; ++i) {
            float h = barMaxH * barHeights[i];
            float x = startX + i * (barW + 3.0f);
            float y = barBottom - h;

            // Gradiente azul-cyan para cada barra
            float t = (float)i / (float)(kNumBars - 1);
            juce::Colour barColour = MixCoachTheme::accentCyan().interpolatedWith(
                MixCoachTheme::accentDim(), t * 0.5f);

            // Glow detrás de la barra
            g.setColour(barColour.withAlpha(0.10f));
            g.fillRect(x - 2.0f, y - 2.0f, barW + 4.0f, h + 4.0f);

            // Barra principal con gradiente vertical
            juce::ColourGradient barGrad(barColour.withAlpha(0.70f),
                                          x, y,
                                          barColour.withAlpha(0.25f),
                                          x, barBottom, false);
            g.setGradientFill(barGrad);
            g.fillRoundedRectangle(x, y, barW, h, 2.0f);
        }

        // ─── Knob circular grande centrado abajo ──────────────────────────────
        float knobR = 22.0f;
        float knobY = cy + 30.0f;

        // Fondo del knob
        g.setColour(MixCoachTheme::bgPanel().withAlpha(0.8f));
        g.fillEllipse(cx - knobR, knobY - knobR, knobR * 2.0f, knobR * 2.0f);

        // Anillo exterior del knob
        juce::ColourGradient ringGrad(MixCoachTheme::accentCyan().withAlpha(0.5f),
                                       cx - knobR * 0.3f, knobY - knobR * 0.3f,
                                       MixCoachTheme::accentCyan().withAlpha(0.10f),
                                       cx + knobR, knobY + knobR, true);
        g.setGradientFill(ringGrad);
        g.drawEllipse(cx - knobR, knobY - knobR, knobR * 2.0f, knobR * 2.0f, 2.0f);

        // Marca del knob (indicador de posición, ~45°)
        float markAngle = juce::MathConstants<float>::pi * 0.25f; // 45°
        float markLen = knobR * 0.65f;
        float mx = cx + std::sin(markAngle) * markLen;
        float my = knobY - std::cos(markAngle) * markLen;
        g.setColour(MixCoachTheme::accentCyan().withAlpha(0.8f));
        g.drawLine(cx, knobY, mx, my, 2.5f);

        // Punto central
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.fillEllipse(cx - 3.0f, knobY - 3.0f, 6.0f, 6.0f);

        // Label sutil
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.setColour(MixCoachTheme::textDim().withAlpha(0.4f));
        g.drawText("LIMITER", juce::Rectangle<float>(cx - 40.0f, knobY + knobR + 2.0f, 80.0f, 14.0f),
                   juce::Justification::centred);
    }

} // namespace mixcoach
