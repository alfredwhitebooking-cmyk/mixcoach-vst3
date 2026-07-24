#include "PhaseMeter.h"

namespace mixcoach {

PhaseMeter::PhaseMeter()
{
    titleLabel_.setText("Fase", juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(9.0f)));
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel_);

    valueLabel_.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
    valueLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentCyan());
    valueLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(valueLabel_);
}

void PhaseMeter::updateCorrelation(float correlation) noexcept
{
    correlation_ = juce::jlimit(-1.0f, 1.0f, correlation);
    valueLabel_.setText(juce::String(correlation_, 2), juce::dontSendNotification);
    repaint();
}

void PhaseMeter::reset() noexcept
{
    correlation_ = 1.0f;
    valueLabel_.setText("1.00", juce::dontSendNotification);
    repaint();
}

juce::Colour PhaseMeter::getIndicatorColour(float corr) const noexcept
{
    if (std::abs(corr) < 0.2f) return MixCoachTheme::error();       // Rojo — casi mono
    if (corr < 0.0f)           return MixCoachTheme::warning();      // Naranja — fuera de fase
    if (corr < 0.5f)           return MixCoachTheme::warning();      // Amarillo — baja correlación
    return MixCoachTheme::success();                                  // Verde — buena fase
}

juce::String PhaseMeter::getPhaseLabel(float corr) const noexcept
{
    if (std::abs(corr) < 0.2f) return "WIDE";
    if (corr < 0.0f)           return "OUT";
    if (corr < 0.5f)           return "FAIR";
    if (corr < 0.8f)           return "GOOD";
    return "FULL";
}

void PhaseMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float cr = 4.0f;

    // ─── Fondo oscuro ──────────────────────────────────────────────────
    g.setColour(MixCoachTheme::bgPanel());
    g.fillRoundedRectangle(bounds, cr);

    // ─── Área del medidor ───────────────────────────────────────────────
    auto meterArea = bounds.reduced(6, 4);
    meterArea.removeFromTop(14.0f); // espacio para titleLabel_
    float meterH = 14.0f;
    auto barBounds = meterArea.withHeight(meterH);
    float cx = barBounds.getCentreX();

    // ─── Barra de fondo ─────────────────────────────────────────────────
    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(barBounds, meterH * 0.5f);

    // ─── Gradiente de la barra: rojo(izq) → verde(centro) → rojo(der) ──
    //    No es un gradiente lineal — usamos una técnica de centrado:
    //    - Centro = correlación 1.0 (verde)
    //    - Extremos = correlación -1.0 / 0 (rojo)
    {
        float halfW = barBounds.getWidth() * 0.5f;
        auto leftHalf = barBounds.withWidth(halfW);
        auto rightHalf = barBounds.withLeft(cx).withWidth(halfW);

        juce::ColourGradient leftGrad(MixCoachTheme::error().withAlpha(0.3f),
                                       leftHalf.getX(), 0,
                                       MixCoachTheme::success().withAlpha(0.3f),
                                       leftHalf.getRight(), 0, false);
        g.setGradientFill(leftGrad);
        g.fillRoundedRectangle(leftHalf, meterH * 0.5f);

        juce::ColourGradient rightGrad(MixCoachTheme::success().withAlpha(0.3f),
                                        rightHalf.getX(), 0,
                                        MixCoachTheme::error().withAlpha(0.3f),
                                        rightHalf.getRight(), 0, false);
        g.setGradientFill(rightGrad);
        g.fillRoundedRectangle(rightHalf, meterH * 0.5f);
    }

    // ─── Marcador de centro (0 = correlación) ───────────────────────────
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.drawVerticalLine((int)cx, barBounds.getY(), barBounds.getBottom());

    // ─── Indicador de correlación actual ─────────────────────────────────
    float normalized = juce::jmap(correlation_, -1.0f, 1.0f, 0.0f, 1.0f);
    float indicatorX = barBounds.getX() + normalized * barBounds.getWidth();
    float indicatorR = 4.0f;

    juce::Colour indicatorColour = getIndicatorColour(correlation_);
    g.setColour(indicatorColour.withAlpha(0.3f));
    g.fillEllipse(indicatorX - indicatorR * 2.0f,
                   barBounds.getCentreY() - indicatorR * 2.0f,
                   indicatorR * 4.0f, indicatorR * 4.0f);
    g.setColour(indicatorColour);
    g.fillEllipse(indicatorX - indicatorR,
                   barBounds.getCentreY() - indicatorR,
                   indicatorR * 2.0f, indicatorR * 2.0f);

    // ─── Labels de fase: OUT - MONO - FULL ──────────────────────────────
    float labelY = barBounds.getBottom() + 2.0f;
    g.setFont(juce::Font(juce::FontOptions(7.5f)));
    g.setColour(MixCoachTheme::textDim().withAlpha(0.6f));
    g.drawText("OUT", barBounds.getX(), labelY, 30.0f, 12.0f, juce::Justification::centredLeft);
    g.drawText("WIDE", cx - 20.0f, labelY, 40.0f, 12.0f, juce::Justification::centred);
    g.drawText("FULL", barBounds.getRight() - 30.0f, labelY, 30.0f, 12.0f, juce::Justification::centredRight);
}

void PhaseMeter::resized()
{
    auto bounds = getLocalBounds();
    titleLabel_.setBounds(bounds.removeFromTop(14).reduced(4, 0));
    valueLabel_.setBounds(bounds.removeFromBottom(18));
}

} // namespace mixcoach
