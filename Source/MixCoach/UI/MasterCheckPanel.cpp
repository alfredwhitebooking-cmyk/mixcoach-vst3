#include "MasterCheckPanel.h"
#include <cmath>

namespace mixcoach {

    MasterCheckPanel::MasterCheckPanel()
    {
        setOpaque(false);
        setVisible(false);
        addAndMakeVisible(scoreRing_);

        // ═══ Incremento 3c: Timer para animación del glow A/B ════════════
        // 15 Hz es suficiente para animación suave sin CPU innecesario.
        // El timer se inicia/apaga automáticamente via visibilityChanged().
    }

    void MasterCheckPanel::setMatchData(float matchScore, float lufsDiff, float spectralDiff,
                                         float dynamicsDiff, float spatialDiff,
                                         const juce::String& recommendation)
    {
        matchScore_ = matchScore;
        lufsDiff_ = lufsDiff;
        spectralDiff_ = spectralDiff;
        dynamicsDiff_ = dynamicsDiff;
        spatialDiff_ = spatialDiff;
        recommendation_ = recommendation;
        hasData_ = true;

        // Animar el score ring desde 0 hasta el match score
        scoreRing_.setScore(matchScore_);

        resized();
        repaint();
    }

    void MasterCheckPanel::visibilityChanged()
    {
        if (isVisible() && !isTimerRunning())
            startTimerHz(15);
        // Nunca detener el timer — el glow A/B de referencia debe seguir
        // animando aunque el panel no sea visible momentáneamente.
    }

    void MasterCheckPanel::clear()
    {
        hasData_ = false;
        matchScore_ = 0.0f;
        scoreRing_.setScoreImmediate(0.0f);
        glowAlpha_ = 0.0f;
        referenceWasActive_ = false;
        repaint();
    }

    void MasterCheckPanel::resized()
    {
        auto bounds = getLocalBounds().reduced(kPadding);
        if (bounds.isEmpty()) return;

        // Score ring: left side, fixed size
        auto gaugeArea = bounds.removeFromLeft(kGaugeSize + 16);
        scoreRing_.setBounds(gaugeArea.withSizeKeepingCentre(kGaugeSize, kGaugeSize));

    }

    // ═══ Incremento 3c: Timer para animación del glow A/B ═════════════
    void MasterCheckPanel::timerCallback()
    {
        bool isActive = false;
        if (onIsReferenceActive)
            isActive = onIsReferenceActive();

        const float targetAlpha = isActive ? 0.55f : 0.0f;
        const float speed = 0.12f; // Suavizado: ~8 frames para alcanzar target (0.53s a 15Hz)

        glowAlpha_ += (targetAlpha - glowAlpha_) * speed;

        // Clamp a la precisión de visualización para evitar tiny fades infinitos
        if (std::abs(glowAlpha_ - targetAlpha) < 0.005f)
            glowAlpha_ = targetAlpha;

        // Detectar transición activo→inactivo
        if (referenceWasActive_ != isActive) {
            referenceWasActive_ = isActive;
            repaint();
        }
        else if (std::abs(glowAlpha_ - targetAlpha) > 0.01f) {
            // Solo repaint si el alpha está cambiando (animación activa)
            repaint();
        }
    }

    void MasterCheckPanel::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().reduced(kPadding, kPadding);
        if (bounds.isEmpty()) return;

        // ═══ Incremento 3c: Glow border cuando la referencia está activa ═══
        if (glowAlpha_ > 0.01f) {
            drawReferenceActiveIndicator(g, bounds);
        }

        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        // Header
        auto header = bounds.removeFromTop(22);
        g.setColour(MixCoachTheme::accentGlow());
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSectionHeader)).boldened());
        g.drawText("MASTER CHECK — Comparaci\u00F3n vs Referencia", header.reduced(4, 0),
                   juce::Justification::centredLeft);

        // ═══ Incremento 3c: Label "🔊 REFERENCIA activa" en la cabecera ═══
        if (glowAlpha_ > 0.05f) {
            auto refLabelArea = header.removeFromRight(200).reduced(4, 1);
            g.setColour(MixCoachTheme::warning().withAlpha(glowAlpha_));
            g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
            // Mostrar el % de mezcla configurado (hardware gain desde ReferenceAudioPlayer)
            g.drawText("\xF0\x9F\x94\x8A REFERENCIA activa",
                       refLabelArea, juce::Justification::centredRight);
        }

        if (!hasData_) {
            g.setColour(MixCoachTheme::textMuted());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)));
            g.drawText("Carga una referencia para comparar tu mezcla final.",
                       bounds.reduced(8), juce::Justification::centred);
            return;
        }

        bounds.removeFromTop(4);

        // Score ring is positioned in resized() as a child component.
        // Remove its area from bounds so gap bars don't overlap.
        bounds.removeFromLeft(kGaugeSize + 16);

        // Gap bars (right side of score ring)
        auto gapsArea = bounds.reduced(4, 0);

        // LUFS diff
        drawGapBar(g, gapsArea.removeFromTop(kRowH), "LUFS", lufsDiff_, 6.0f,
                   MixCoachTheme::accent(), "dB");
        gapsArea.removeFromTop(4);

        // Spectral diff
        drawGapBar(g, gapsArea.removeFromTop(kRowH), "Espectro", spectralDiff_, 12.0f,
                   MixCoachTheme::specColour(2), "dB");
        gapsArea.removeFromTop(4);

        // Dynamics diff
        drawGapBar(g, gapsArea.removeFromTop(kRowH), "Din\u00E1mica", dynamicsDiff_, 8.0f,
                   MixCoachTheme::warning(), "dB");
        gapsArea.removeFromTop(4);

        // Spatial diff
        drawGapBar(g, gapsArea.removeFromTop(kRowH), "Espacial", spatialDiff_, 0.5f,
                   MixCoachTheme::info(), "");

        // Recommendation at bottom
        if (recommendation_.isNotEmpty()) {
            auto recArea = getLocalBounds().reduced(kPadding, kPadding)
                                .removeFromBottom(40).reduced(4, 0);
            g.setColour(juce::Colour(0xFF1A1A2E).withAlpha(0.6f));
            g.fillRoundedRectangle(recArea.toFloat(), 4.0f);
            g.setColour(MixCoachTheme::accentGlow());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
            juce::String recText = "\xF0\x9F\x92\xA1 " + recommendation_;
            g.drawText(recText, recArea.reduced(6, 0), juce::Justification::centredLeft);
        }
    }

    // ═══ Incremento 3c: Dibuja el glow border pulzante alrededor del panel ═══
    void MasterCheckPanel::drawReferenceActiveIndicator(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        auto b = bounds.toFloat().expanded(1.0f);
        const float radius = MixCoachTheme::cornerRadius_medium + 1.0f;

        // Primera capa: glow exterior (más tenue, más expandido)
        g.setColour(MixCoachTheme::warning().withAlpha(glowAlpha_ * 0.15f));
        g.drawRoundedRectangle(b.expanded(3.0f, 3.0f), radius + 2.0f, 3.0f);

        // Segunda capa: glow medio
        g.setColour(MixCoachTheme::warning().withAlpha(glowAlpha_ * 0.30f));
        g.drawRoundedRectangle(b.expanded(1.5f, 1.5f), radius + 1.0f, 2.0f);

        // Tercera capa: borde principal (más brillante)
        g.setColour(MixCoachTheme::warning().withAlpha(glowAlpha_ * 0.55f));
        g.drawRoundedRectangle(b, radius, 1.5f);
    }

    void MasterCheckPanel::drawGapBar(juce::Graphics& g, juce::Rectangle<int> area,
                                       const char* label, float value, float maxVal,
                                       juce::Colour colour, const juce::String& unit)
    {
        // Label
        g.setColour(MixCoachTheme::textDim());
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
        g.drawText(juce::String(label), area.removeFromLeft(70), juce::Justification::centredLeft);

        // Bar
        auto barArea = area.reduced(0, 4);
        float norm = juce::jlimit(0.0f, 1.0f, maxVal > 0.0f ? value / maxVal : 0.0f);
        g.setColour(MixCoachTheme::bgDarker());
        g.fillRoundedRectangle(barArea.toFloat(), 3.0f);
        g.setColour(colour.withAlpha(0.6f));
        auto fill = barArea.withWidth((int)(barArea.getWidth() * norm));
        if (fill.getWidth() > 2) g.fillRoundedRectangle(fill.toFloat(), 3.0f);

        // Value
        g.setColour(colour);
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
        juce::String valText = juce::String(value, 1);
        if (unit.isNotEmpty()) valText += " " + unit;
        g.drawText(valText, barArea.removeFromRight(50), juce::Justification::centredRight);
    }

} // namespace mixcoach
