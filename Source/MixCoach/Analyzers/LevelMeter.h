#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../UI/MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  LevelMeter — Barra de nivel vertical con gradiente de color.
//
//  Componente independiente que el Coach abre para mostrar evidencia de
//  problemas de gain staging o balance. Recibe un nivel en dB y lo
//  renderiza como barra vertical con colores: azul (bajo) → verde (sano)
//  → amarillo (alto) → rojo (clipping).
//
//  Uso:
//    LevelMeter meter;
//    meter.updateLevel(-6.2f); // dB
//    addAndMakeVisible(meter);
// ═══════════════════════════════════════════════════════════════════════════
class LevelMeter : public juce::Component
{
public:
    LevelMeter();
    ~LevelMeter() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Actualiza el nivel mostrado. value en dB (ej: -6.2f). */
    void updateLevel(float levelDb) noexcept;

    /** Define el label que identifica este meter (ej: "Kick", "Master"). */
    void setTrackLabel(const juce::String& label);

    /** Resetea el nivel a -infinito. */
    void reset() noexcept;

private:
    float levelDb_ = -60.0f;
    juce::String trackLabel_;
    juce::Label valueLabel_;

    juce::Colour getBarColour(float db) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};

} // namespace mixcoach
