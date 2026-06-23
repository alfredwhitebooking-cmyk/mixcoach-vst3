#include "PhaseCorrelationMeter.h"

namespace mixcoach {

PhaseCorrelationMeter::PhaseCorrelationMeter()
{
    titleLabel_.setText(juce::CharPointer_UTF8("\xCF\x86 Phase Correlation"),
                        juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(titleLabel_);

    valueLabel_.setText("+1.00", juce::dontSendNotification);
    valueLabel_.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
    valueLabel_.setJustificationType(juce::Justification::centred);
    valueLabel_.setColour(juce::Label::textColourId, MixCoachTheme::success());
    addAndMakeVisible(valueLabel_);
}

void PhaseCorrelationMeter::resized()
{
    auto area = getLocalBounds().reduced(2);
    titleLabel_.setBounds(area.removeFromTop(14));
    valueLabel_.setBounds(area.removeFromTop(16));
}

void PhaseCorrelationMeter::setCorrelation(float value)
{
    correlationTarget_ = juce::jlimit(-1.0f, 1.0f, value);
    correlation_.setTargetValue(correlationTarget_);
}

bool PhaseCorrelationMeter::advanceFrame(double sampleRateHz, bool allowRepaint)
{
    const bool dirty = correlation_.advance(sampleRateHz);
    if (! dirty)
        return false;

    const float corr = correlation_.getCurrent();

    juce::Colour col;
    if (std::abs(corr) < 0.3f)
        col = MixCoachTheme::error();
    else if (corr < 0.0f)
        col = MixCoachTheme::warning();
    else
        col = MixCoachTheme::success();

    valueLabel_.setColour(juce::Label::textColourId, col);
    valueLabel_.setText(juce::String(corr, 2), juce::dontSendNotification);

    if (allowRepaint)
        repaint();

    return true;
}

void PhaseCorrelationMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(30); // título + valor

    // ─── Barra de correlación ──────────────────────────────────────────
    auto barBounds = area.reduced(6, 2).toFloat();

    // Fondo oscuro de la barra
    g.setColour(MixCoachTheme::bgDarker().withAlpha(0.7f));
    g.fillRoundedRectangle(barBounds, 5.0f);

    // ─── Gradiente de fondo por zonas: Rojo | Amarillo | Verde ────────
    // Zona roja: -1.0 a -0.3 (out of phase)
    // Zona amarilla: -0.3 a +0.3 (cauteloso)
    // Zona verde: +0.3 a +1.0 (buena fase)
    float totalW = barBounds.getWidth();
    float leftX = barBounds.getX();

    // Rojo (-1.0 a -0.3): 0% a 35%
    auto redZone = barBounds.withWidth(totalW * 0.35f);
    juce::ColourGradient redGrad(
        MixCoachTheme::error().withAlpha(0.25f),
        juce::Point<float>(redZone.getX(), 0.0f),
        MixCoachTheme::error().withAlpha(0.05f),
        juce::Point<float>(redZone.getRight(), 0.0f),
        false);
    g.setGradientFill(redGrad);
    g.fillRoundedRectangle(redZone, 5.0f);

    // Amarillo (-0.3 a +0.3): 35% a 65%
    auto yellowZone = barBounds.withLeft(leftX + totalW * 0.35f)
                                .withWidth(totalW * 0.30f);
    juce::ColourGradient yellowGrad(
        MixCoachTheme::warning().withAlpha(0.20f),
        juce::Point<float>(yellowZone.getX(), 0.0f),
        MixCoachTheme::warning().withAlpha(0.05f),
        juce::Point<float>(yellowZone.getRight(), 0.0f),
        false);
    g.setGradientFill(yellowGrad);
    g.fillRoundedRectangle(yellowZone, 5.0f);

    // Verde (+0.3 a +1.0): 65% a 100%
    auto greenZone = barBounds.withLeft(leftX + totalW * 0.65f)
                              .withWidth(totalW * 0.35f);
    juce::ColourGradient greenGrad(
        MixCoachTheme::success().withAlpha(0.25f),
        juce::Point<float>(greenZone.getRight(), 0.0f),
        MixCoachTheme::success().withAlpha(0.05f),
        juce::Point<float>(greenZone.getX(), 0.0f),
        false);
    g.setGradientFill(greenGrad);
    g.fillRoundedRectangle(greenZone, 5.0f);

    // ─── Separadores de zonas ──────────────────────────────────────────
    g.setColour(MixCoachTheme::divider().withAlpha(0.15f));
    float sepX1 = leftX + totalW * 0.35f;
    float sepX2 = leftX + totalW * 0.65f;
    g.drawVerticalLine((int)sepX1, barBounds.getY() + 2, barBounds.getBottom() - 2);
    g.drawVerticalLine((int)sepX2, barBounds.getY() + 2, barBounds.getBottom() - 2);

    // ─── Center line (0) ───────────────────────────────────────────────
    float centerX = barBounds.getCentreX();
    g.setColour(MixCoachTheme::divider().withAlpha(0.3f));
    g.drawVerticalLine((int)centerX, barBounds.getY(), barBounds.getBottom());

    // ─── Scale labels ──────────────────────────────────────────────────
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)));
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.4f));

    auto labelY = barBounds.getBottom() + 2;
    juce::Rectangle<float> lblArea;
    
    lblArea = juce::Rectangle<float>(barBounds.getX() - 2, labelY, 16, 10);
    g.drawText("-1", lblArea, juce::Justification::centredLeft);

    lblArea = juce::Rectangle<float>(leftX + totalW * 0.35f - 8, labelY, 16, 10);
    g.drawText("-0.3", lblArea, juce::Justification::centred);

    lblArea = juce::Rectangle<float>(centerX - 10, labelY, 20, 10);
    g.drawText("0", lblArea, juce::Justification::centred);

    lblArea = juce::Rectangle<float>(leftX + totalW * 0.65f - 8, labelY, 16, 10);
    g.drawText("+0.3", lblArea, juce::Justification::centred);

    lblArea = juce::Rectangle<float>(barBounds.getRight() - 16, labelY, 16, 10);
    g.drawText("+1", lblArea, juce::Justification::centredRight);

    // ─── Marker (indicador de correlación actual) ──────────────────────
    float corr = juce::jlimit(-1.0f, 1.0f, correlation_.getCurrent());
    float norm = (corr + 1.0f) * 0.5f;
    float markerX = barBounds.getX() + norm * barBounds.getWidth();

    // Color del marker
    juce::Colour indicatorColour;
    if (std::abs(corr) < 0.3f)
        indicatorColour = MixCoachTheme::error();
    else if (corr < 0.0f)
        indicatorColour = MixCoachTheme::warning();
    else
        indicatorColour = MixCoachTheme::success();

    // Glow del marker
    juce::ColourGradient markerGlow(
        indicatorColour.withAlpha(0.3f),
        juce::Point<float>(markerX, barBounds.getCentreY()),
        indicatorColour.withAlpha(0.0f),
        juce::Point<float>(markerX + 18.0f, barBounds.getCentreY()),
        false);
    markerGlow.addColour(0.5, indicatorColour.withAlpha(0.15f));
    g.setGradientFill(markerGlow);
    g.fillEllipse(markerX - 8.0f, barBounds.getCentreY() - 5.0f, 16.0f, 10.0f);

    // Marker circle
    g.setColour(indicatorColour);
    g.fillEllipse(markerX - 4.5f, barBounds.getCentreY() - 4.5f, 9.0f, 9.0f);

    // Highlight del marker
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.fillEllipse(markerX - 2.0f, barBounds.getCentreY() - 4.0f, 4.0f, 4.0f);

    // ─── Borde de la barra ─────────────────────────────────────────────
    g.setColour(MixCoachTheme::border().withAlpha(0.3f));
    g.drawRoundedRectangle(barBounds, 5.0f, 1.0f);

    // ─── Warning si fuera de fase ──────────────────────────────────────
    if (corr < -0.3f) {
        g.setColour(MixCoachTheme::error().withAlpha(0.6f));
        g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
        g.drawText("\xE2\x9A\xA0 Out of phase!",
                   barBounds.withTop(barBounds.getBottom() - 18).toNearestInt(),
                   juce::Justification::centred);
    }

    // ═══ Phase diagnostic overlay (del coach) ══════════════════════════
    if (hasPhaseDiagnostic_ && phaseDiagnostic_.description.isNotEmpty())
    {
        const auto colour = phaseDiagnostic_.getDisplayColour();
        const float severity = phaseDiagnostic_.severity;
        
        // ─── Highlight zone en la barra según severidad ────────────────
        {
            // Si la correlación es negativa, resalta la zona roja izquierda
            // Si es positiva baja, resalta la zona amarilla central
            float highlightWidth = 0.15f + severity * 0.20f;
            float highlightPos;
            
            if (corr < 0.0f) {
                // Resaltar zona izquierda (roja)
                highlightPos = 0.0f;
            } else if (corr < 0.3f) {
                // Resaltar zona central (amarilla)
                highlightPos = 0.35f - highlightWidth * 0.5f;
            } else {
                // Resaltar zona derecha (verde) solo si es praise
                if (!phaseDiagnostic_.isPraise) {
                    highlightPos = 0.65f - highlightWidth * 0.5f;
                } else {
                    highlightPos = 0.80f;
                }
            }
            
            auto highlightRect = juce::Rectangle<float>(
                barBounds.getX() + highlightPos * totalW,
                barBounds.getY() - 2.0f,
                highlightWidth * totalW,
                barBounds.getHeight() + 4.0f
            );
            
            g.setColour(colour.withAlpha(0.12f + severity * 0.15f));
            g.fillRoundedRectangle(highlightRect, 6.0f);
            
            g.setColour(colour.withAlpha(0.25f + severity * 0.30f));
            g.drawRoundedRectangle(highlightRect, 6.0f, 1.5f + severity * 1.0f);
        }
        
        // ─── Badge de advertencia debajo de la barra ───────────────────
        {
            auto warnArea = juce::Rectangle<float>(
                barBounds.getX(),
                barBounds.getBottom() + 2.0f,
                barBounds.getWidth(),
                16.0f
            );
            
            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.setColour(colour.withAlpha(0.4f + severity * 0.4f));
            g.drawText(phaseDiagnostic_.description,
                       warnArea.toNearestInt(),
                       juce::Justification::centred);
        }
    }
}

} // namespace mixcoach
