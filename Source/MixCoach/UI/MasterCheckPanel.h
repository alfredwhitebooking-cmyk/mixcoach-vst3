#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "ScoreRingComponent.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  MasterCheckPanel — Panel de comparación final vs referencia
//
//  Aparece automáticamente al entrar en CoachRoomState::MasterCheck.
//  Muestra un score de match (0-100) animado con ScoreRingComponent,
//  gaps por banda espectral, y recomendaciones generales para la mezcla.
// ═══════════════════════════════════════════════════════════════════════════
class MasterCheckPanel : public juce::Component,
                        public juce::Timer
{
public:
    MasterCheckPanel();
    ~MasterCheckPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;
    void timerCallback() override;

    /** Actualiza los datos de comparación. */
    void setMatchData(float matchScore, float lufsDiff, float spectralDiff,
                      float dynamicsDiff, float spatialDiff,
                      const juce::String& recommendation);

    void clear();
    bool hasData() const noexcept { return hasData_; }

    ScoreRingComponent& getScoreRing() noexcept { return scoreRing_; }

    std::function<void()> onFinish;
    std::function<void()> onBackToFixes;

    // ═══ Incremento 3c: A/B Reference Visual Indicator ═════════════
    /** Callback que retorna true si la referencia de audio está sonando.
        Se cablea desde NavigationShell con processorRef_.getRefPlayer().isPlaying()
        para no acoplar MasterCheckPanel al PluginProcessor. */
    std::function<bool()> onIsReferenceActive;

private:
    void drawGapBar(juce::Graphics& g, juce::Rectangle<int> area,
                    const char* label, float value, float maxVal,
                    juce::Colour colour, const juce::String& unit);

    /** Dibuja el indicador A/B de referencia activa con glow animado. */
    void drawReferenceActiveIndicator(juce::Graphics& g, juce::Rectangle<int> bounds);

    bool hasData_ = false;
    float matchScore_ = 0.0f;
    float lufsDiff_ = 0.0f;
    float spectralDiff_ = 0.0f;
    float dynamicsDiff_ = 0.0f;
    float spatialDiff_ = 0.0f;
    juce::String recommendation_;

    // ═══ Score ring animado ═══════════════════════════════════════════
    ScoreRingComponent scoreRing_;

    // ═══ Incremento 3c: Glow animado para A/B reference indicator ═══
    float glowAlpha_ = 0.0f;        // 0.0 = inactivo, 0.6 = activo (animado)
    bool referenceWasActive_ = false; // Para detectar transiciones

    static constexpr int kPadding = 8;
    static constexpr int kGaugeSize = 100;
    static constexpr int kRowH = 24;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterCheckPanel)
};

} // namespace mixcoach
