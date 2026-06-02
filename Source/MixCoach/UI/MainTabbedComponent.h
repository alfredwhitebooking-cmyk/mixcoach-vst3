#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Common/memory/SharedData.h"
#include "MixCoachTheme.h"
#include "../../Common/types/Types.h"
#include "CoachChatComponent.h"
#include "AnalyzersPanelComponent.h"

namespace mixcoach {

// ─── Tabbed component principal (2 tabs rediseñadas) ────────────────────────
class MainTabbedComponent : public juce::TabbedComponent
{
public:
    MainTabbedComponent(juce::AudioProcessor& processor,
                        SharedData& sharedData);

    void resized() override;

    MixCoachPanel&           getCoachPanel()       { return *coachPanel_; }
    AnalyzersPanelComponent& getAnalyzersPanel()   { return *analyzersPanel_; }

    // Actualización completa de todos los paneles
    void updateAllPanels(SlotRegistry& registry, double sampleRate);

    // Actualización rápida del spectrograph (60fps ligero)
    void fastUpdateSpectrograph(SlotRegistry& registry);

    // Smooth de meters y spectrograph SIN lock (60fps, no necesita SlotRegistry)
    void smoothMeters() { coachPanel_->smoothMeters(); }
    void smoothAnalyzersPanel(double sampleRateHz = 60.0);
    void smoothSpectrograph() { smoothAnalyzersPanel(60.0); }

private:
    juce::AudioProcessor&           processorRef_;
    SharedData&                     sharedData_;

    std::unique_ptr<MixCoachPanel>           coachPanel_;
    std::unique_ptr<AnalyzersPanelComponent> analyzersPanel_;
};

} // namespace mixcoach
