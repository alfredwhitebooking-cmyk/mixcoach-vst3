#include "MainTabbedComponent.h"

namespace mixcoach {

MainTabbedComponent::MainTabbedComponent(juce::AudioProcessor& processor,
                                         SharedData& sharedData)
    : juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop)
    , processorRef_(processor)
    , sharedData_(sharedData)
    , analyzersPanel_(sharedData)
{
    coachPanel_ = std::make_unique<MixCoachPanel>();

    // ─── Tab 1: Mix Coach ──────────────────────────────────────────────────
    // Chat general con IA + referencias (archivos/enlaces) + lista de Messengers
    addTab(juce::CharPointer_UTF8("\xF0\x9F\x8E\x9B Mix Coach"),
           MixCoachTheme::bgPanel(), coachPanel_.get(), false, 0);

    // ─── Tab 2: Professional Metering ──────────────────────────────────────
    // Analizadores profesionales estilo IK Multimedia
    addTab(juce::CharPointer_UTF8("\xF0\x9F\x93\x8A Metering"),
           MixCoachTheme::bgPanel(), &analyzersPanel_, false, 1);

    setTabBarDepth(34);
    getTabbedButtonBar().setColour(juce::TabbedButtonBar::tabTextColourId, MixCoachTheme::textPrimary());
    getTabbedButtonBar().setColour(juce::TabbedButtonBar::frontOutlineColourId, MixCoachTheme::accent());
    getTabbedButtonBar().setColour(juce::TabbedButtonBar::tabOutlineColourId, MixCoachTheme::border());

    setCurrentTabIndex(0);
}

void MainTabbedComponent::resized()
{
    juce::TabbedComponent::resized();
}

} // namespace mixcoach
