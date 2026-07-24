#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../UI/MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  TargetMarkers — Marcadores visuales de targets de referencia.
//
//  Componente independiente que el Coach abre para mostrar cómo se compara
//  la mezcla actual contra la referencia en múltiples dimensiones
//  (LUFS, crest, balance espectral, etc.). Cada target es una barra
//  horizontal con label.
//
//  Uso:
//    TargetMarkers targets;
//    targets.setTarget("LUFS", -14.0f, -12.5f);  // label, target, actual
//    targets.setTarget("Crest", 14.0f, 8.2f);
//    addAndMakeVisible(targets);
// ═══════════════════════════════════════════════════════════════════════════
class TargetMarkers : public juce::Component
{
public:
    TargetMarkers();
    ~TargetMarkers() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Añade un target: label descriptivo, valor objetivo, valor actual. */
    void setTarget(const juce::String& label, float targetValue, float actualValue);

    /** Limpia todos los targets. */
    void clearTargets();

    /** Define un mensaje de resumen debajo de los targets. */
    void setSummary(const juce::String& summary);

private:
    struct TargetItem {
        juce::String label;
        float targetValue = 0.0f;
        float actualValue = 0.0f;
        float normalised = 0.0f; // normalizado respecto al target
    };

    std::vector<TargetItem> targets_;
    juce::String summary_;
    juce::Label titleLabel_;
    juce::Label summaryLabel_;

    juce::Colour getDeviationColour(float actual, float target) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TargetMarkers)
};

} // namespace mixcoach
