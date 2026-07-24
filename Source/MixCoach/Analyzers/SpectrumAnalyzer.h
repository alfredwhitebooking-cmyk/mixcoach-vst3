#pragma once
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../UI/MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  SpectrumAnalyzer — Visualizador FFT con overlay de referencia.
//
//  Componente independiente que el Coach abre para mostrar evidencia de
//  problemas de EQ o balance tonal. Muestra el espectro de frecuencias
//  con una curva superpuesta de la referencia para comparación visual.
//
//  Uso:
//    SpectrumAnalyzer spec;
//    spec.updateSpectrum(binData, numBins);
//    spec.setReferenceCurve(refData, numBins);
//    addAndMakeVisible(spec);
// ═══════════════════════════════════════════════════════════════════════════
class SpectrumAnalyzer : public juce::Component
{
public:
    SpectrumAnalyzer();
    ~SpectrumAnalyzer() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Actualiza los datos del espectro (valores normalizados 0..1). */
    void updateSpectrum(const float* data, int numBins);

    /** Establece la curva de referencia para overlay. */
    void setReferenceCurve(const float* data, int numBins);

    /** Limpia la curva de referencia. */
    void clearReferenceCurve();

    /** Resalta una frecuencia específica (para "mira aquí"). */
    void setHighlightFreq(float freqHz, float gainDb);

    /** Define el label que identifica este analyzer (ej: "Kick EQ Match"). */
    void setTitle(const juce::String& title);

    /** Número de bins del espectro. */
    static constexpr int kNumBins = 256;

private:
    std::array<float, kNumBins> bins_{};
    std::array<float, kNumBins> referenceBins_{};
    bool hasReference_ = false;
    float highlightFreq_ = 0.0f;
    float highlightDb_ = 0.0f;
    juce::String title_;
    juce::Label titleLabel_;

    void drawGrid(juce::Graphics& g, juce::Rectangle<float> area);
    void drawSpectrum(juce::Graphics& g, juce::Rectangle<float> area);
    void drawReference(juce::Graphics& g, juce::Rectangle<float> area);
    void drawHighlight(juce::Graphics& g, juce::Rectangle<float> area);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};

} // namespace mixcoach
