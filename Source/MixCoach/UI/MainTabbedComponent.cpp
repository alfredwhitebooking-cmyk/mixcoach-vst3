#include "MainTabbedComponent.h"

namespace mixcoach {

MainTabbedComponent::MainTabbedComponent(juce::AudioProcessor& processor,
                                         SharedData& sharedData)
    : juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop)
    , processorRef_(processor)
    , sharedData_(sharedData)
    , coachPanel_(nullptr)
    , analyzersPanel_(std::make_unique<AnalyzersPanelComponent>(sharedData))
{
    coachPanel_ = std::make_unique<MixCoachPanel>();

    // ─── Tab 1: Mix Coach ──────────────────────────────────────────────────
    // Chat general con IA + referencias (archivos/enlaces) + lista de Messengers
    addTab(juce::CharPointer_UTF8("\xF0\x9F\x8E\x9B Mix Coach"),
           MixCoachTheme::bgPanel(), coachPanel_.get(), false, 0);

    // ─── Tab 2: Professional Metering ──────────────────────────────────────
    // Analizadores profesionales estilo IK Multimedia
               addTab(juce::CharPointer_UTF8("\xF0\x9F\x93\x8A Metering"),
            MixCoachTheme::bgPanel(), analyzersPanel_.get(), false, 1);

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

void MainTabbedComponent::updateAllPanels(SlotRegistry& registry, double sampleRate)
{
    juce::ignoreUnused(sampleRate);
    try
    {
        // Update MixCoach panel messenger list
        coachPanel_->updateMessengers(registry);
        // Existing analyzers panel update
        analyzersPanel_->updateAnalyzers(registry);
    }
    catch (const std::exception& e)
    {
        juce::Logger::outputDebugString("[MainTabbedComponent::updateAllPanels] Exception: "
                                         + juce::String(e.what()));
    }
}

void MainTabbedComponent::fastUpdateSpectrograph(SlotRegistry& registry)
{
    try
    {
        // Leer slot seleccionado de AnalyzersPanel
        int selSlot = analyzersPanel_->getSelectedSlot();
        if (selSlot < 0)
            return;

        auto& telem = registry.getTelemetry(selSlot);
        auto latest = telem.latest();

        // Quick FFT update (no necesita bloqueo — solo lectura)
        if (latest.active) {
            bool hasFFT = false;
            for (int fi = 0; fi < 256 && !hasFFT; ++fi) {
                if (latest.spectrum[fi] > 0.01f) hasFFT = true;
            }
            if (hasFFT) {
                analyzersPanel_->getSpectrograph().updateSpectrum(latest.spectrum, 256);
            }
        }
    }
    catch (const std::exception& e)
    {
        juce::Logger::outputDebugString("[MainTabbedComponent::fastUpdateSpectrograph] Exception: "
                                         + juce::String(e.what()));
    }
}

} // namespace mixcoach
