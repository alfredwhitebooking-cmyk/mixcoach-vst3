#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "VectorscopeComponent.h"
#include "PhaseCorrelationMeter.h"
#include "CrestHistogram.h"
#include "MixCoachTheme.h"
#include "../../Common/audio/DiagnosticBridge.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  PhaseScopePanel — Contenedor con header "Phase Scope"
    //  Agrupa Vectorscope + PhaseCorrelation + CrestHistogram
    //  Además recibe diagnósticos de fase desde el DiagnosticBridge
    //  para mostrar overlays visuales de advertencia.
    // ═══════════════════════════════════════════════════════════════════════════
    class PhaseScopePanel : public juce::Component
    {
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

        // ═══ Phase Diagnostic Overlay ══════════════════════════════════════
        /** Recibe diagnósticos de fase desde el DiagnosticBridge y los
            distribuye a VectorscopeComponent y PhaseCorrelationMeter. */
        void setPhaseDiagnostics(const std::vector<PhaseDiagnostic>& diagnostics);

    private:
        juce::Label headerLabel_;
        VectorscopeComponent vectorscope_;
        PhaseCorrelationMeter phaseMeter_;
        CrestHistogram crestHistogram_;
    };

} // namespace mixcoach
