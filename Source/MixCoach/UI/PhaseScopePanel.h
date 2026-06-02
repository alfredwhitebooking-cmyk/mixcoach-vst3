#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "VectorscopeComponent.h"
#include "PhaseCorrelationMeter.h"
#include "CrestHistogram.h"
#include "MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  PhaseScopePanel — Contenedor con header "Phase Scope"
//  Agrupa Vectorscope + PhaseCorrelation + CrestHistogram
// ═══════════════════════════════════════════════════════════════════════════
class PhaseScopePanel : public juce::Component {
public:
    PhaseScopePanel();
    ~PhaseScopePanel() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    VectorscopeComponent& getVectorscope() noexcept { return vectorscope_; }
    PhaseCorrelationMeter& getPhaseMeter() noexcept { return phaseMeter_; }
    CrestHistogram& getCrestHistogram() noexcept { return crestHistogram_; }

    void setCorrelation(float v) { phaseMeter_.setCorrelation(v); }
    void pushSample(float l, float r) { vectorscope_.pushSample(l, r); }
    void pushCrest(float p, float r) { crestHistogram_.pushCrest(p, r); }

    bool advanceVisuals(double sampleRateHz = 60.0, bool allowRepaint = true);

private:
    juce::Label headerLabel_;
    VectorscopeComponent     vectorscope_;
    PhaseCorrelationMeter    phaseMeter_;
    CrestHistogram           crestHistogram_;
};

} // namespace mixcoach
