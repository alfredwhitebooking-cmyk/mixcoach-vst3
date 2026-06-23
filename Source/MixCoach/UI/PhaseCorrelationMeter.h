#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <cmath>
#include "SmoothValue.h"
#include "MixCoachTheme.h"
#include "../../Common/audio/DiagnosticBridge.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  PhaseCorrelationMeter — Barra horizontal de correlación de fase
//  Con overlay visual para diagnósticos de fase del coach.
// ═══════════════════════════════════════════════════════════════════════════
class PhaseCorrelationMeter : public juce::Component {
public:
    PhaseCorrelationMeter();
    ~PhaseCorrelationMeter() override = default;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setCorrelation(float value);
    bool advanceFrame(double sampleRateHz = 60.0, bool allowRepaint = true);

    // ═══ Phase Diagnostic Overlay ══════════════════════════════════════
    /** Establece el diagnóstico de fase activo (o nullptr para limpiar).
        El componente dibujará una zona resaltada + texto de advertencia. */
    void setPhaseDiagnostic(const PhaseDiagnostic* diagnostic);

private:
    SmoothValue correlation_{ 1.0f, 5.0f, 100.0f };
    float correlationTarget_ = 1.0f;
    juce::Label titleLabel_;
    juce::Label valueLabel_;

    // ═══ Phase diagnostic overlay state ════════════════════════════════
    PhaseDiagnostic phaseDiagnostic_;
    bool hasPhaseDiagnostic_ = false;
};

} // namespace mixcoach
