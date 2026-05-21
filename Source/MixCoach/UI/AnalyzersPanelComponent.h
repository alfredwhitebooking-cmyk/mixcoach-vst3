#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Common/Types.h"
#include "MixCoachTheme.h"
#include "../Analyzers/SpectrumAnalyzer.h"
#include "../Analyzers/PhaseMeter.h"
#include "../Analyzers/LevelMeter.h"
#include "../Analyzers/TargetMarkers.h"

namespace mixcoach {

// ─── Panel de Analizadores (Master) ─────────────────────────────────────────
class AnalyzersPanelComponent : public juce::Component
{
public:
    AnalyzersPanelComponent();
    ~AnalyzersPanelComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void updateSpectrum(const float* data, int numBins);
    void updatePhase(float correlation);
    void updateLevels(float left, float right);

    SpectrumAnalyzer  spectrumAnalyzer;
    PhaseMeter        phaseMeter;
    LevelMeter        masterLevelMeter;
    LevelMeter        leftLevelMeter;
    LevelMeter        rightLevelMeter;
    TargetMarkers     targetMarkers;

private:
    juce::Label titleLabel_;
    juce::Label spectrumLabel_;
    juce::Label phaseLabel_;
    juce::Label levelLabel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyzersPanelComponent)
};

} // namespace mixcoach
