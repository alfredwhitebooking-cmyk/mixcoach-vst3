#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include "MixCoachTheme.h"
#include "SmoothValue.h"
#include "../engine/CoachEngine.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  ReferenceMatchPanel — Panel visual de matching espectral + LUFS
//  Muestra 6 barras espectrales comparativas (mix vs ref) lado a lado
//  y métricas de loudness, crest, y correlación con indicadores de desviación.
//
//  V2: SmoothValue animation para transiciones suaves de barras.
//      Timer 60fps con pausa cuando no visible.
//      Layout mejorado con columna mix + columna ref separadas.
// ═══════════════════════════════════════════════════════════════════════════
class ReferenceMatchPanel : public juce::Component,
                            private juce::Timer
{
public:
    ReferenceMatchPanel();
    ~ReferenceMatchPanel() override;

    void paint(juce::Graphics& g) override;

    /** Actualiza los datos de matching y refresca la UI. */
    void updateMatchData(const DifferenceProfile& data);

    /** Retorna true si hay datos de matching válidos. */
    [[nodiscard]] bool hasMatchData() const noexcept { return data_.valid; }

    /** Retorna el puntaje de matching general (0-100). */
    [[nodiscard]] int getMatchScore() const noexcept { return matchScore_; }

private:
    static constexpr int kNumRegions = 6;

    // Datos actuales
    DifferenceProfile data_;
    int matchScore_ = 0;

    // ─── SmoothValues para animación suave de barras ────────────────────
    // attack rápido (50ms), release medio (300ms) para sensación responsiva
    std::array<SmoothValue, kNumRegions> mixSmooth_;
    std::array<SmoothValue, kNumRegions> refSmooth_;
    SmoothValue lufsSmooth_;
    SmoothValue scoreSmooth_;

    // Últimos valores target para detectar cambios
    float lastMixEnergy_[kNumRegions] = { -100.0f };
    float lastRefEnergy_[kNumRegions] = { -100.0f };
    float lastLufsMix_ = -100.0f;
    float lastLufsRef_ = -100.0f;

    // ─── Timer callback (60fps animation) ──────────────────────────────
    void timerCallback() override;
    void visibilityChanged() override;

    // Layout helpers
    void drawRegionBars(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawLUFSMeter(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawCrestMeter(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawCorrelationMeter(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawMetricsRow(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawMatchScore(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawDeltaRow(juce::Graphics& g, juce::Rectangle<int> bounds);

    /** Calcula el match score 0-100 desde los datos actuales. */
    int computeMatchScore() const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReferenceMatchPanel)
};

} // namespace mixcoach
