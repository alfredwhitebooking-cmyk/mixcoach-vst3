#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../UI/MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  PhaseMeter — Medidor de correlación de fase horizontal.
//
//  Componente independiente que el Coach abre para mostrar evidencia de
//  problemas de fase. Muestra una barra horizontal con indicador que va
//  desde -1 (out of phase) hasta +1 (in phase), con zona roja central
//  cuando la correlación es baja.
//
//  Uso:
//    PhaseMeter meter;
//    meter.updateCorrelation(0.85f);
//    addAndMakeVisible(meter);
// ═══════════════════════════════════════════════════════════════════════════
class PhaseMeter : public juce::Component
{
public:
    PhaseMeter();
    ~PhaseMeter() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Actualiza el valor de correlación (-1.0 a +1.0). */
    void updateCorrelation(float correlation) noexcept;

    /** Resetea a 1.0 (fase perfecta). */
    void reset() noexcept;

    /** Devuelve el valor actual de correlación. */
    float getCorrelation() const noexcept { return correlation_; }

private:
    float correlation_ = 1.0f;
    juce::Label titleLabel_;
    juce::Label valueLabel_;

    juce::Colour getIndicatorColour(float corr) const noexcept;
    juce::String getPhaseLabel(float corr) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhaseMeter)
};

} // namespace mixcoach
