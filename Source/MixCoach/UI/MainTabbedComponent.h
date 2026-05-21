#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Common/SharedData.h"
#include "MixCoachTheme.h"
#include "../../Common/Types.h"
#include "CoachChatComponent.h"
#include "TrackDashboardComponent.h"
#include "AnalyzersPanelComponent.h"
#include "VirtualBusesComponent.h"

namespace mixcoach {

// ─── Tabbed component principal ─────────────────────────────────────────────
class MainTabbedComponent : public juce::TabbedComponent
{
public:
    MainTabbedComponent(juce::AudioProcessor& processor,
                        SharedData& sharedData);

    void resized() override;

    CoachChatComponent&       getCoachPanel()       { return *coachPanel_; }
    TrackDashboardComponent&  getDashboard()        { return *dashboard_; }
    AnalyzersPanelComponent&  getAnalyzersPanel()   { return analyzersPanel_; }
    VirtualBusesComponent&    getVirtualBuses()     { return virtualBuses_; }

private:
    juce::AudioProcessor&           processorRef_;
    SharedData&                     sharedData_;

    std::unique_ptr<CoachChatComponent>      coachPanel_;
    std::unique_ptr<TrackDashboardComponent> dashboard_;
    AnalyzersPanelComponent                  analyzersPanel_;
    VirtualBusesComponent                    virtualBuses_;
};

} // namespace mixcoach
