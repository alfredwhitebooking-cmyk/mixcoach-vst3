#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Common/SharedData.h"
#include "MixCoachTheme.h"
#include "../../Common/Types.h"
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
    AnalyzersPanelComponent& getAnalyzersPanel()   { return analyzersPanel_; }

private:
    juce::AudioProcessor&           processorRef_;
    SharedData&                     sharedData_;

    std::unique_ptr<MixCoachPanel>           coachPanel_;
    AnalyzersPanelComponent                  analyzersPanel_;
};

} // namespace mixcoach
