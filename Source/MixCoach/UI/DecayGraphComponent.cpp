#include "DecayGraphComponent.h"
#include <cmath>

namespace mixcoach {

    DecayGraphComponent::DecayGraphComponent()
    {
        setSize(kPreferredWidth, kPreferredHeight);
    }

    void DecayGraphComponent::setReverbParams(float preDelayMs, float decaySec,
                                              float highCutHz, float mixPct) noexcept
    {
        preDelayMs_ = preDelayMs;
        decaySec_ = decaySec;
        highCutHz_ = highCutHz;
        mixPct_ = mixPct;
        repaint();
    }

    void DecayGraphComponent::resized()
    {
        // No child components — todo se pinta directamente
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Dibuja el gráfico de decay de reverb
    //
    //  Layout:
    //    [Label "Decay"]
    //    [Graph area with grid + curve + annotations]
    //    [Mix badge]
    //
    //  El eje X representa el tiempo (0 → decaySec * 1.2)
    //  El eje Y representa la amplitud (0% → 100%, arriba)
    // ═══════════════════════════════════════════════════════════════════════════
    void DecayGraphComponent::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        const float cr = 4.0f;

        // ─── Fondo del panel ────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.10f));
        g.fillRoundedRectangle(bounds.reduced(0.5f, 0.5f), cr);
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawRoundedRectangle(bounds.reduced(0.5f, 0.5f), cr, 0.5f);

        auto area = bounds.reduced(6, 4);
        if (area.getWidth() < 40 || area.getHeight() < 20) return;

        // ─── Header: "Decay" label ──────────────────────────────────────────
        auto headerArea = area.removeFromTop(12);
        g.setFont(juce::Font(juce::FontOptions(7.0f)).boldened());
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.drawText("Decay", headerArea, juce::Justification::centredLeft);

        // ─── Graph area ─────────────────────────────────────────────────────
        auto graphArea = area.reduced(0, 2);
        if (graphArea.getHeight() < 10) return;

        const float gx = graphArea.getX();
        const float gy = graphArea.getY();
        const float gw = graphArea.getWidth();
        const float gh = graphArea.getHeight();

        // ─── Grid lines (horizontal) ────────────────────────────────────────
        g.setColour(juce::Colours::white.withAlpha(0.04f));
        for (int i = 1; i <= 3; ++i) {
            float y = gy + gh * (1.0f - i / 4.0f);
            g.drawHorizontalLine((int)y, gx, gx + gw);
        }

        // ─── Ejes de tiempo (marcas cada 0.5s) ──────────────────────────────
        float totalTime = decaySec_ * 1.2f + preDelayMs_ / 1000.0f;
        if (totalTime < 0.1f) totalTime = 2.0f;
        float timeStep = 0.5f;
        int numSteps = (int)(totalTime / timeStep) + 1;
        g.setFont(juce::Font(juce::FontOptions(5.5f)));
        g.setColour(juce::Colours::white.withAlpha(0.20f));
        for (int s = 0; s < numSteps; ++s) {
            float t = s * timeStep;
            float x = gx + gw * (t / totalTime);
            if (x <= gx + gw) {
                g.drawVerticalLine((int)x, gy, gy + gh);
                if (s % 2 == 0) {
                    juce::String timeLabel = juce::String(t, 1) + "s";
                    g.drawText(timeLabel, x - 8, gy + gh + 1, 16, 8,
                               juce::Justification::centred);
                }
            }
        }

        // ─── Línea base (dry signal at bottom) ──────────────────────────────
        float baseY = gy + gh - 1;
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.drawHorizontalLine((int)baseY, gx, gx + gw);

        // ─── Pre-delay marker (vertical dashed line) ─────────────────────────
        float preDelayTime = preDelayMs_ / 1000.0f;
        float preDelayX = gx + gw * (preDelayTime / totalTime);
        if (preDelayX > gx && preDelayX < gx + gw) {
            g.setColour(groupColour_.withAlpha(0.30f));
            // Dashed line
            for (float dy = gy; dy < gy + gh; dy += 4.0f) {
                float dashEnd = juce::jmin(dy + 2.0f, gy + gh);
                g.drawHorizontalLine((int)dy, preDelayX, preDelayX);
                dy += 2.0f;
            }
            // Label
            g.setFont(juce::Font(juce::FontOptions(5.5f)));
            g.setColour(groupColour_.withAlpha(0.50f));
            g.drawText(juce::String((int)preDelayMs_) + "ms",
                       preDelayX - 10, gy + 2, 20, 8,
                       juce::Justification::centred);
        }

        // ─── Curva de decay exponencial ─────────────────────────────────────
        // Usamos: amplitude(t) = mixPct * exp(-t / decaySec)
        // Donde t=0 es después del pre-delay
        if (gw > 20 && gh > 10) {
            juce::Path decayPath;
            bool first = true;

            for (int px = 0; px <= (int)gw; px += 2) {
                float t_sec = (px / gw) * totalTime;
                float t_after_pre = t_sec - preDelayTime;

                float amplitude;
                if (t_after_pre < 0.0f) {
                    amplitude = 0.0f; // Silencio durante pre-delay
                } else {
                    amplitude = (mixPct_ / 100.0f)
                                * std::exp(-t_after_pre / (decaySec_ * 0.66f));
                }

                float y = gy + gh - amplitude * gh;
                y = juce::jlimit(gy, gy + gh, y);
                float x = gx + px;

                if (first) {
                    decayPath.startNewSubPath(x, baseY);
                    decayPath.lineTo(x, baseY);
                    first = false;
                }

                if (t_after_pre >= 0.0f) {
                    if (px == 0) {
                        decayPath.startNewSubPath(x, y);
                    }
                    decayPath.lineTo(x, y);
                }
            }

            // Dibujar curva
            if (!first) {
                // Trazo suave
                g.setColour(groupColour_.withAlpha(0.60f));
                g.strokePath(decayPath, juce::PathStrokeType(1.5f));

                // Glow sutil debajo de la curva (área rellena)
                juce::Path fillPath = decayPath;
                fillPath.lineTo(gx + gw, baseY);
                fillPath.closeSubPath();
                g.setColour(groupColour_.withAlpha(0.08f));
                g.fillPath(fillPath);
            }
        }

        // ─── High-cut annotation ────────────────────────────────────────────
        if (highCutHz_ > 0.0f) {
            juce::String hcLabel;
            if (highCutHz_ >= 1000.0f)
                hcLabel = "HC: " + juce::String(highCutHz_ / 1000.0f, 1) + "kHz";
            else
                hcLabel = "HC: " + juce::String((int)highCutHz_) + "Hz";

            g.setFont(juce::Font(juce::FontOptions(5.5f)));
            g.setColour(juce::Colours::white.withAlpha(0.30f));
            g.drawText(hcLabel,
                       juce::Rectangle<float>(gx, gy + gh * 0.3f, gw, 8),
                       juce::Justification::topRight);
        }

        // ─── Mix badge (esquina inferior derecha) ───────────────────────────
        {
            juce::String mixLabel = juce::String((int)mixPct_) + "% mix";
            auto mixBounds = juce::Rectangle<float>(gx + gw - 36, gy + gh - 10, 34, 9);
            g.setColour(groupColour_.withAlpha(0.15f));
            g.fillRoundedRectangle(mixBounds, 2.0f);
            g.setColour(groupColour_.withAlpha(0.60f));
            g.setFont(juce::Font(juce::FontOptions(5.5f)).boldened());
            g.drawText(mixLabel, mixBounds, juce::Justification::centred);
        }
    }

} // namespace mixcoach
