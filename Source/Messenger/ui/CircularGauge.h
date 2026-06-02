#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../MixCoach/UI/SmoothValue.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  CircularGauge — Gauge semicircular profesional estilo referencia
//
//  Diseñado para mostrar REDUCCIÓN DE GANANCIA (GR) en el Messenger.
//  Arco semicircular de -90° a +90° con escala, aguja violeta luminosa
//  y efecto glow. Animación suave via SmoothValue + timer interno.
//
//  Escala: -12 | -6 | 0 | +6 | +12 dB
//  rango:  minDb_ (-12) a maxDb_ (+12)
// ═══════════════════════════════════════════════════════════════════════════
class CircularGauge : public juce::Component,
                      private juce::Timer
{
public:
    CircularGauge();
    ~CircularGauge() override;

    // ─── Configuración ──────────────────────────────────────────────────
    void setTitle(const juce::String& title);
    void setValue(float valueDb);
    void setRange(float minDb, float maxDb);
    void setLabelPrefix(const juce::String& prefix);

    // ─── Obtener valor actual (ya smoothed) ─────────────────────────────
    float getCurrentValue() const { return smoothValue_.getCurrent(); }

    // ─── Callback opcional para valor numérico externo ──────────────────
    std::function<void(float)> onValueChanged;

private:
    // juce::Timer
    void timerCallback() override;
    void visibilityChanged() override;

    // ─── Dibujado ───────────────────────────────────────────────────────
    void paint(juce::Graphics& g) override;
    void resized() override;

    void drawArc(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawScale(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawNeedle(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawCenterDot(juce::Graphics& g, juce::Rectangle<float> bounds);

    // ─── Conversión valor → ángulo ─────────────────────────────────────
    float valueToAngle(float valueDb) const;

    // ─── Componentes ────────────────────────────────────────────────────
    juce::Label titleLabel_;
    juce::Label valueLabel_;

    // ─── Datos ──────────────────────────────────────────────────────────
    SmoothValue smoothValue_;
    float targetValue_{0.0f};
    float minDb_{-12.0f};
    float maxDb_{12.0f};
    juce::String labelPrefix_;

    // ─── Constantes de dibujo ──────────────────────────────────────────
    static constexpr float kArcStartAngle  = juce::MathConstants<float>::pi * 0.75f;   // 135° (abajo izq)
    static constexpr float kArcEndAngle    = juce::MathConstants<float>::pi * 0.25f;    // 45° (abajo der)
    // Rango total del arco: 270° (de 135° a 45° pasando por 90°=top)
    static constexpr float kArcRange       = juce::MathConstants<float>::pi * 1.5f;     // 270°
    static constexpr float kAngleOffset    = juce::MathConstants<float>::pi;            // 180° offset

    // Colores
    static juce::Colour kArcBg()       { return juce::Colour(0xFF2A2B3E); }
    static juce::Colour kNeedleColour(){ return juce::Colour(0xFF8B5CF6); }  // violeta
    static juce::Colour kGlowColour()  { return juce::Colour(0xFFA78BFA); }
    static juce::Colour kTickColour()  { return juce::Colour(0xFF5C5F73); }
    static juce::Colour kTextDim()     { return juce::Colour(0xFF8B8FA3); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CircularGauge)
};

} // namespace mixcoach
