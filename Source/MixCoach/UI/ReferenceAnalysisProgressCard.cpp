#include "ReferenceAnalysisProgressCard.h"
#include <cmath>

namespace mixcoach {

    ReferenceAnalysisProgressCard::ReferenceAnalysisProgressCard()
    {
        setSize(360, 60);
    }

    void ReferenceAnalysisProgressCard::startAnimation()
    {
        animating_ = true;
        complete_  = false;
        showSummary_ = false;
        progressSmooth_.reset(0.0f);
        targetProgress_ = 0.0f;
        displayedProgress_ = 0.0f;
        stageIndex_ = 0;
        visibleChecklistItems_ = 0;
        usingExternalProgress_ = false;
        hasBandData_ = false;
        summaryHasUsableSignal_ = true;
        for (auto& e : summaryBandEnergies_) e = -100.0f;
        setSize(getWidth(), 85);  // Taller to fit checklist
        startTimerHz(60);
        repaint();
    }

    void ReferenceAnalysisProgressCard::setExternalProgress(float progress)
    {
        usingExternalProgress_ = true;
        targetProgress_ = juce::jlimit(0.0f, 1.0f, progress);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  showSummary — Expanded with full metrics panel
    // ═══════════════════════════════════════════════════════════════════════════
    void ReferenceAnalysisProgressCard::showSummary(float integratedLUFS,
                                                     float loudnessRange,
                                                     float truePeak,
                                                     float crestFactor,
                                                     float correlation,
                                                     float spectralCentroidHz,
                                                     const float* bandEnergies)
    {
        showSummary_ = true;
        animating_   = false;
        complete_    = true;

        summaryLUFS_  = integratedLUFS;
        summaryRange_ = loudnessRange;
        summaryPeak_  = truePeak;
        summaryCrest_ = crestFactor;
        summaryCorrelation_ = correlation;
        summaryCentroidHz_  = spectralCentroidHz;

        // Un valor de silencio o una medición incompleta no debe aparecer como
        // diagnóstico. La tarjeta pasará a orientar al usuario hacia la acción
        // necesaria para obtener una lectura real.
        summaryHasUsableSignal_ = std::isfinite(integratedLUFS)
                               && std::isfinite(loudnessRange)
                               && std::isfinite(truePeak)
                               && std::isfinite(crestFactor)
                               && std::isfinite(correlation)
                               && std::isfinite(spectralCentroidHz)
                               && integratedLUFS > -90.0f
                               && truePeak > -100.0f;

        if (bandEnergies != nullptr) {
            hasBandData_ = true;
            for (int i = 0; i < 30; ++i)
                summaryBandEnergies_[i] = bandEnergies[i];
        } else {
            hasBandData_ = false;
            for (auto& e : summaryBandEnergies_) e = -100.0f;
        }

        stopTimer();
        setSize(getWidth(), 130);  // BUG #14: altura directa, no via resized()
        repaint();
        // BUG #19: nullear callback antes de disparar para evitar double-fire
        if (onComplete) { auto cb = onComplete; onComplete = nullptr; cb(); }
    }

    void ReferenceAnalysisProgressCard::reset()
    {
        animating_ = false;
        complete_  = false;
        showSummary_ = false;
        progressSmooth_.reset(0.0f);
        stageIndex_ = 0;
        hasBandData_ = false;
        summaryHasUsableSignal_ = true;
        for (auto& e : summaryBandEnergies_) e = -100.0f;
        setSize(getWidth(), 60);
        stopTimer();
        repaint();
    }

    void ReferenceAnalysisProgressCard::timerCallback()
    {
        if (!animating_) return;

        if (!usingExternalProgress_) {
            targetProgress_ += 1.0f / (kAnimDurationSecs * 60.0f);
            if (targetProgress_ >= 1.0f)
                targetProgress_ = 1.0f;
        }

        progressSmooth_.setTargetValue(targetProgress_);
        progressSmooth_.advance(60.0);
        displayedProgress_ = progressSmooth_.getCurrent();

        int prevStage = stageIndex_;
        for (int i = 0; i < kNumStages; ++i) {
            if (displayedProgress_ >= kStageThresholds[i])
                stageIndex_ = i;
        }
        // Fire onStageChanged when stage advances
        if (stageIndex_ > prevStage && onStageChanged)
            onStageChanged(stageIndex_);

        // Actualizar checklist visible
        for (int i = 0; i < kNumChecklistItems; ++i) {
            if (displayedProgress_ >= kChecklistThresholds[i])
                visibleChecklistItems_ = i + 1;
        }

        repaint();

        if (targetProgress_ >= 1.0f && displayedProgress_ >= 0.99f) {
            animating_ = false;
            complete_  = true;
            stopTimer();
            if (!showSummary_) {
                // BUG #19: nullear callback antes de disparar para evitar double-fire
                if (onComplete) { auto cb = onComplete; onComplete = nullptr; cb(); }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Qualitative label helpers
    // ═══════════════════════════════════════════════════════════════════════════

    float ReferenceAnalysisProgressCard::subEnergyScore(const float be[30]) noexcept
    {
        // Sub region = bands 0-4 (~20-86 Hz)
        float sum = 0.0f;
        int n = 0;
        for (int i = 0; i < 5 && i < 30; ++i) {
            if (be[i] > -80.0f) { sum += (be[i] + 80.0f); ++n; }
        }
        if (n == 0) return 0.0f;
        float avg = sum / (float)n;
        // Map -80..-20 → 0.0..1.0
        return juce::jlimit(0.0f, 1.0f, (avg - 0.0f) / 60.0f);
    }

    juce::String ReferenceAnalysisProgressCard::subLabel(const float be[30]) noexcept
    {
        float score = subEnergyScore(be);
        if (score < 0.15f) return "Poco presente";
        if (score < 0.35f) return "Moderado";
        if (score < 0.55f) return "Presente";
        if (score < 0.75f) return "Muy presente";
        return "Dominante";
    }

    juce::String ReferenceAnalysisProgressCard::airLabel(float centroidHz) noexcept
    {
        // Centroide espectral: < 500Hz = oscuro, > 3000Hz = muy brillante
        if (centroidHz <= 500.0f)  return "Oscuro";
        if (centroidHz <= 1000.0f) return "Cálido";
        if (centroidHz <= 1800.0f) return "Balanceado";
        if (centroidHz <= 2800.0f) return "Brillante";
        return "Muy brillante";
    }

    juce::String ReferenceAnalysisProgressCard::transientLabel(float crestDb) noexcept
    {
        // Crest factor: < 4dB = muy comprimido, > 16dB = muy dinámico
        if (crestDb <= 3.0f)  return "Muy controlados";
        if (crestDb <= 6.0f)  return "Controlados";
        if (crestDb <= 9.0f)  return "Moderados";
        if (crestDb <= 12.0f) return "Dinámicos";
        if (crestDb <= 15.0f) return "Muy dinámicos";
        return "Extremos";
    }

    juce::String ReferenceAnalysisProgressCard::widthLabel(float correlation) noexcept
    {
        float absC = std::abs(correlation);
        if (absC >= 0.95f) return "Mono";
        if (absC >= 0.80f) return "Estrecho";
        if (absC >= 0.55f) return "Moderado";
        if (absC >= 0.30f) return "Ancho";
        return "Muy ancho";
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Rich metrics display panel
    // ═══════════════════════════════════════════════════════════════════════════
    void ReferenceAnalysisProgressCard::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        const float cr = 8.0f;

        // ─── Shadow ──────────────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.15f));
        g.fillRoundedRectangle(bounds.expanded(1.0f, 2.0f), cr);

        // ─── Background ──────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::bgPanel().withAlpha(0.95f));
        g.fillRoundedRectangle(bounds, cr);

        // ─── Border ──────────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::border().withAlpha(0.35f));
        g.drawRoundedRectangle(bounds, cr, 0.8f);

        auto area = bounds.reduced(12, 10);

        if (showSummary_) {
            if (!summaryHasUsableSignal_) {
                auto headerArea = area.removeFromTop(20);
                g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
                g.setColour(MixCoachTheme::warning());
                g.drawText("Necesito más señal para analizar", headerArea,
                           juce::Justification::centredLeft);

                area.removeFromTop(8);
                auto guidanceArea = area.removeFromTop(36);
                g.setFont(juce::Font(juce::FontOptions(9.0f)));
                g.setColour(MixCoachTheme::textDim());
                g.drawFittedText("No obtuve una lectura útil de esta referencia. "
                                 "Reproduce un pasaje con nivel durante 10–15 segundos.",
                                 guidanceArea.toNearestInt(),
                                 juce::Justification::centredLeft, 2, 0.9f);

                area.removeFromTop(6);
                auto nextStepArea = area.removeFromTop(18);
                g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
                g.setColour(MixCoachTheme::accentCyan());
                g.drawText("Siguiente paso: vuelve a analizar la referencia.",
                           nextStepArea, juce::Justification::centredLeft);
                return;
            }

            // ═════════════════════════════════════════════════════════════════
            //  METRICS DISPLAY PANEL — Full reference analysis results
            // ═════════════════════════════════════════════════════════════════

            // ─── Header — "✅ Referencia analizada" ─────────────────────────
            auto headerArea = area.removeFromTop(20);
            g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
            g.setColour(MixCoachTheme::success());
            g.drawText("Referencia analizada", headerArea,
                       juce::Justification::centredLeft);

            area.removeFromTop(6);

            // ─── Grid: 3 columnas de métricas numéricas ─────────────────────
            float colW = (area.getWidth() - 16.0f) / 3.0f;
            float metricRowH = 32.0f;
            float metricGap = 2.0f;

            // Fila 1: LUFS | True Peak | Stereo Width
            auto row1 = area.removeFromTop(metricRowH);

            struct MetricCell {
                juce::String label;
                juce::String value;
                juce::Colour valueColour;
            };

            MetricCell row1Cells[3] = {
                {"LUFS",
                 juce::String(summaryLUFS_, 1),
                 (summaryLUFS_ > -8.0f ? MixCoachTheme::warning() : MixCoachTheme::accentGlow())},

                {"True Peak",
                 juce::String(summaryPeak_, 1) + " dBTP",
                 (summaryPeak_ > -0.5f ? MixCoachTheme::error() : MixCoachTheme::accentGlow())},

                {"Stereo",
                 juce::String(summaryCorrelation_, 2),
                 (summaryCorrelation_ < 0.0f ? MixCoachTheme::error() : MixCoachTheme::accentGlow())}
            };

            for (int i = 0; i < 3; ++i) {
                auto cell = row1.withTrimmedLeft(colW * (float)i)
                                 .withWidth(colW - metricGap);

                // Label
                g.setFont(juce::Font(juce::FontOptions(7.5f)));
                g.setColour(MixCoachTheme::textMuted());
                g.drawText(row1Cells[i].label, cell.removeFromTop(10),
                           juce::Justification::centredLeft);

                // Value
                g.setFont(juce::Font(juce::FontOptions(15.0f)).boldened());
                g.setColour(row1Cells[i].valueColour);
                g.drawText(row1Cells[i].value, cell,
                           juce::Justification::centredLeft);

                // Separador vertical (excepto último)
                if (i < 2) {
                    float sepX = row1.getX() + colW * (float)(i + 1) - 1.0f;
                    g.setColour(MixCoachTheme::border().withAlpha(0.2f));
                    g.drawVerticalLine((int)sepX, row1.getY() + 4, row1.getBottom() - 4);
                }
            }

            area.removeFromTop(4);

            // Fila 2: Crest | LRA | Width label
            auto row2 = area.removeFromTop(metricRowH);

            juce::String crestLabel = transientLabel(summaryCrest_);
            juce::String widthQlabel = widthLabel(summaryCorrelation_);

            MetricCell row2Cells[3] = {
                {"Crest",
                 juce::String(summaryCrest_, 1) + " dB  (" + crestLabel + ")",
                 (summaryCrest_ < 4.0f ? MixCoachTheme::warning() : MixCoachTheme::accentGlow())},

                {"Rango",
                 juce::String(summaryRange_, 1) + " LU",
                 (summaryRange_ > 15.0f ? MixCoachTheme::warning() : MixCoachTheme::accentGlow())},

                {"Estéreo",
                 widthQlabel,
                 (summaryCorrelation_ < 0.0f ? MixCoachTheme::error() : MixCoachTheme::accentCyan())}
            };

            for (int i = 0; i < 3; ++i) {
                auto cell = row2.withTrimmedLeft(colW * (float)i)
                                 .withWidth(colW - metricGap);

                // Label
                g.setFont(juce::Font(juce::FontOptions(7.5f)));
                g.setColour(MixCoachTheme::textMuted());
                g.drawText(row2Cells[i].label, cell.removeFromTop(10),
                           juce::Justification::centredLeft);

                // Value
                g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
                g.setColour(row2Cells[i].valueColour);
                g.drawText(row2Cells[i].value, cell,
                           juce::Justification::centredLeft);

                if (i < 2) {
                    float sepX = row2.getX() + colW * (float)(i + 1) - 1.0f;
                    g.setColour(MixCoachTheme::border().withAlpha(0.2f));
                    g.drawVerticalLine((int)sepX, row2.getY() + 4, row2.getBottom() - 4);
                }
            }

            // Separador horizontal entre numéricas y cualitativas
            area.removeFromTop(2);
            auto sepArea = area.removeFromTop(1);
            g.setColour(MixCoachTheme::border().withAlpha(0.15f));
            g.fillRect(sepArea);

            area.removeFromTop(4);

            // ─── Sección cualitativa: Sub · Aire · Transientes ─────────────
            float qualiRowH = 22.0f;
            auto qualiArea = area.removeFromTop(qualiRowH);

            juce::String subQ  = hasBandData_ ? subLabel(summaryBandEnergies_)
                                              : juce::String(summaryLUFS_ < -12.0f ? "Poco presente" : "Presente");
            juce::String airQ  = airLabel(summaryCentroidHz_);
            juce::String transQ = transientLabel(summaryCrest_);

            struct QualiCell {
                const char* label;
                juce::String value;
            };

            QualiCell qualiCells[3] = {
                {"Sub",   subQ},
                {"Aire",  airQ},
                {"Trans", transQ}
            };

            for (int i = 0; i < 3; ++i) {
                auto cell = qualiArea.withTrimmedLeft(colW * (float)i)
                                      .withWidth(colW - metricGap);

                g.setFont(juce::Font(juce::FontOptions(10.0f)));
                g.setColour(MixCoachTheme::textDim());

                juce::String qualiText = juce::String(qualiCells[i].label) + ": "
                                         + qualiCells[i].value;
                g.drawText(qualiText, cell, juce::Justification::centredLeft);
            }

            // ─── Footer — género inferido + hint de referencia ─────────────
            area.removeFromTop(2);
            auto footerArea = area.removeFromTop(16);
            g.setFont(juce::Font(juce::FontOptions(7.5f)));
            g.setColour(MixCoachTheme::textMuted());
            g.drawText("Usa estos valores como objetivo para tu mezcla",
                       footerArea, juce::Justification::centredLeft);

        } else {
            // ═══ BARRA DE PROGRESO ════════════════════════════════════════════

            // ─── Número porcentual (izquierda) ────────────────────────────────
            auto pctArea = area.removeFromLeft(40);
            g.setFont(juce::Font(juce::FontOptions(18.0f)).boldened());
            g.setColour(MixCoachTheme::accentGlow());
            g.drawText(juce::String(static_cast<int>(displayedProgress_ * 100.0f)) + "%",
                       pctArea, juce::Justification::centredLeft);

            // ─── Barra de progreso (centro) ──────────────────────────────────
            auto barArea = area.removeFromLeft(juce::jmin(area.getWidth() - 60.0f, 200.0f)).reduced(0, 4);
            const float barCr = 3.0f;

            g.setColour(MixCoachTheme::bgDarker());
            g.fillRoundedRectangle(barArea, barCr);

            if (displayedProgress_ > 0.0f) {
                auto fillArea = barArea.withWidth(barArea.getWidth() * displayedProgress_);
                juce::ColourGradient fillGrad(MixCoachTheme::accent().withAlpha(0.6f),
                                               fillArea.getX(), fillArea.getY(),
                                               MixCoachTheme::accentCyan().withAlpha(0.4f),
                                               fillArea.getRight(), fillArea.getY(),
                                               false);
                g.setGradientFill(fillGrad);
                g.fillRoundedRectangle(fillArea, barCr);

                auto glowArea = fillArea.withWidth(juce::jmin(4.0f, fillArea.getWidth()));
                g.setColour(juce::Colours::white.withAlpha(0.3f));
                g.fillRoundedRectangle(glowArea, barCr);
            }

            // ─── Texto de etapa (debajo de la barra) ─────────────────────────
            auto stageArea = bounds.reduced(10, 0).withTop(barArea.getBottom() + 4).withHeight(16);
            g.setFont(juce::Font(juce::FontOptions(8.0f)));
            g.setColour(complete_ ? MixCoachTheme::success() : MixCoachTheme::textMuted());
            g.drawText(juce::String(kStageText[stageIndex_]),
                       stageArea, juce::Justification::centredLeft);

            // ─── Checklist progresivo (debajo del texto de etapa) ──────────
            if (visibleChecklistItems_ > 0) {
                auto clArea = bounds.reduced(12, 0).withTop(stageArea.getBottom() + 2).withHeight(14 * kNumChecklistItems);
                g.setFont(juce::Font(juce::FontOptions(8.0f)));
                for (int i = 0; i < visibleChecklistItems_ && i < kNumChecklistItems; ++i) {
                    auto itemArea = clArea.withHeight(14).translated(0, (float)i * 14.0f);
                    g.setColour(MixCoachTheme::success().withAlpha(0.85f));
                    g.drawText(juce::String(kChecklistTexts[i]),
                               itemArea, juce::Justification::centredLeft);
                }
            }
        }
    }

    void ReferenceAnalysisProgressCard::resized()
    {
        // BUG #14: NO llamar a setSize() aquí — causa recursión con resized().
        // La altura la fija el método que cambia el estado (startAnimation, showSummary, reset).
        // El padre (CoachChatComponent) controla el layout general.
    }

} // namespace mixcoach
