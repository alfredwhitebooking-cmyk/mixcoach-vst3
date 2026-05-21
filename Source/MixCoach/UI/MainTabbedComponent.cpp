#include "MainTabbedComponent.h"

namespace mixcoach {

MainTabbedComponent::MainTabbedComponent(juce::AudioProcessor& processor,
                                         SharedData& sharedData)
    : juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop)
    , processorRef_(processor)
    , sharedData_(sharedData)
{
    coachPanel_ = std::make_unique<CoachChatComponent>();
    dashboard_  = std::make_unique<TrackDashboardComponent>();

    addTab("💬 Mentor", MixCoachTheme::bgPanel(), coachPanel_.get(), false, 0);
    addTab("📊 Dashboard", MixCoachTheme::bgPanel(), dashboard_.get(),   false, 1);
    addTab("📈 Analizadores", MixCoachTheme::bgPanel(), &analyzersPanel_, false, 2);
    addTab("🔌 Buses Virtuales", MixCoachTheme::bgPanel(), &virtualBuses_, false, 3);

    setTabBarDepth(32);
    getTabbedButtonBar().setColour(juce::TabbedButtonBar::tabTextColourId, MixCoachTheme::textPrimary());
    getTabbedButtonBar().setColour(juce::TabbedButtonBar::frontOutlineColourId, MixCoachTheme::accent());

    setCurrentTabIndex(0);
}

void MainTabbedComponent::resized()
{
    juce::TabbedComponent::resized();
}

} // namespace mixcoach
