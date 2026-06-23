#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <cmath>
#include <array>
#include <deque>
#include <algorithm>
#include "MixCoachTheme.h"
#include "../../Common/types/Types.h"
#include "../../Common/memory/SlotRegistry.h"
#include "../audio/AudioAnalyzer.h"

namespace mixcoach {

// Forward declarations
class VectorscopeSystem;
class PhaseCorrelationSystem;

// ═══════════════════════════════════════════════════════════════════════════
//  ProfessionalAnalyzersComponent — Tab "SYSTEM" en MixCoach Brain
//  Contiene Vectorscope + Phase Correlation con estilo profesional
// ═══════════════════════════════════════════════════════════════════════════
class ProfessionalAnalyzersComponent : public juce::Component {
public:
    ProfessionalAnalyzersComponent();
    ~ProfessionalAnalyzersComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    // Actualizar con datos del SlotRegistry (llamado desde timer del editor)
    void updateAnalyzers(SlotRegistry& registry);

    /** Alimentar datos de audio reales desde el AudioAnalyzer (samples + correlación). */
    void updateFromAnalyzer(const AudioAnalyzer& analyzer);

    VectorscopeSystem&       getVectorscope() noexcept { return *vectorscope_; }
    PhaseCorrelationSystem&  getPhaseMeter()   noexcept { return *phaseMeter_; }

private:
    std::unique_ptr<VectorscopeSystem>       vectorscope_;
    std::unique_ptr<PhaseCorrelationSystem>   phaseMeter_;

    juce::Label headerLabel_;
    juce::Label infoLabel_;

    int currentSlotIndex_ = -1;
    juce::Colour currentColour_{ 0xFF3498DB };
    juce::String currentTrackName_;
};

// ═══════════════════════════════════════════════════════════════════════════
//  VectorscopeSystem — Vectorscopio circular con persistencia fósforo
//  L en X, R en Y, colores dinámicos según correlación.
//  Timer interno a 30fps para interpolación visual fluida.
// ═══════════════════════════════════════════════════════════════════════════
class VectorscopeSystem : public juce::Component,
                          private juce::Timer {
public:
    VectorscopeSystem();
    ~VectorscopeSystem() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

    // Push nueva muestra estéreo (desde timer del editor ~2fps)
    void pushSample(float left, float right);

    // Set correlation for color indication
    void setCorrelation(float corr) noexcept { correlation_ = corr; }

    void setSlotInfo(int slotIndex, const juce::Colour& colour, const juce::String& trackName);

private:
    void timerCallback() override;
    void drawBackground(juce::Graphics& g, juce::Rectangle<float> area);
    void drawGrid(juce::Graphics& g, juce::Rectangle<float> area);
    void drawPhosphorPoints(juce::Graphics& g, juce::Rectangle<float> area);
    void drawCorrelationIndicator(juce::Graphics& g, juce::Rectangle<float> area);

    void rebuildGridCache();
    juce::Image gridCache_;
    bool gridCacheValid_ = false;

    static constexpr int kTraceLen = 512;

    struct TracePoint {
        float x = 0.0f;
        float y = 0.0f;
        int age = 0; // frames since pushed (0 = newest)
    };

    // Circular buffer
    std::array<TracePoint, kTraceLen> points_{};
    int writePos_ = 0;
    int pointCount_ = 0;

    float correlation_ = 1.0f;

    int slotIndex_ = -1;
    juce::Colour slotColour_{ 0xFF3498DB };
    juce::String trackName_;

    juce::Label titleLabel_;
    juce::Label corrValueLabel_;
    juce::Label trackLabel_;

    float smoothCorrelation_ = 1.0f;

    static constexpr int kPhosphorSteps = 120; // frames until a point fades out

    // Phosphor trail
    std::deque<TracePoint> phosphorTrail_;
    static constexpr int kMaxPhosphor = 1500;
};

// ═══════════════════════════════════════════════════════════════════════════
//  PhaseCorrelationSystem — Medidor de correlación de fase horizontal
//  -1 (fuera de fase) → 0 (mono) → +1 (en fase)
//  Barra con gradiente rojo→amarillo→verde, animación suave a 30fps
// ═══════════════════════════════════════════════════════════════════════════
class PhaseCorrelationSystem : public juce::Component,
                                private juce::Timer {
public:
    PhaseCorrelationSystem();
    ~PhaseCorrelationSystem() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

    void setCorrelation(float value);

private:
    void timerCallback() override;
    void drawCorrelationBar(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawValueIndicator(juce::Graphics& g, juce::Rectangle<float> bounds);

    float current_ = 1.0f;
    float target_  = 1.0f;

    static constexpr float kAttackCoeff  = 0.35f;
    static constexpr float kReleaseCoeff = 0.12f;

    juce::Colour getCorrelationColour(float corr) const noexcept;

    juce::Label titleLabel_;
    juce::Label valueLabel_;
    juce::Label lWarning_;
    juce::Label rWarning_;
};

} // namespace mixcoach
