#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Common/memory/SharedData.h"
#include "../core/PluginProcessor.h"
#include "MixCoachTheme.h"
#include "../../Common/types/Types.h"
#include "CoachChatComponent.h"
#include "AnalyzersPanelComponent.h"


namespace mixcoach {

// ─── Tabbed component principal — 2 pestañas: AI Coach + Analyzers ───────────
class MainTabbedComponent : public juce::TabbedComponent
{
public:
    MainTabbedComponent(MixCoachAudioProcessor& processor,
                        SharedData& sharedData);

    void resized() override;

    MixCoachPanel&                 getCoachPanel()             { return *coachPanel_; }
    AnalyzersPanelComponent&       getAnalyzersPanel()         { return *analyzersPanel_; }

    // Actualización completa de todos los paneles
    void updateAllPanels(SlotRegistry& registry, SharedData& sharedData, double sampleRate);

    // Actualización de master meters (desde AudioAnalyzer)
    void updateMasterMeters(const AudioAnalyzer& analyzer)
    {
        coachPanel_->updateMasterMeters(analyzer);
    }

    // Smooth de meters y analyzers SIN lock (60fps, no necesita SlotRegistry)
    void smoothMeters() { coachPanel_->smoothMeters(); }
    void smoothAnalyzersPanel(double sampleRateHz = 60.0);

private:
    mixcoach::MixCoachAudioProcessor& processorRef_;

    std::unique_ptr<MixCoachPanel>                  coachPanel_;
    std::unique_ptr<AnalyzersPanelComponent>        analyzersPanel_;
};

} // namespace mixcoach
