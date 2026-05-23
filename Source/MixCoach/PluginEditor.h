#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "UI/MainTabbedComponent.h"
#include "UI/MixCoachTheme.h"
#include "../Common/SharedData.h"

namespace mixcoach {

// ─── MixCoach Plugin Editor — Inicialización segura ─────────────────────────
// CRÍTICO: sharedData_ puede ser nullptr durante la creación del editor.
// No bloqueamos el message thread de FL Studio esperando SharedData.
// El timerCallback() llama processorRef_.ensureSharedData() gradualmente,
// y cuando sharedData_ esté disponible, se crea la UI completa.
class MixCoachAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    MixCoachAudioProcessorEditor(MixCoachAudioProcessor& processor, SharedData* sharedData);
    ~MixCoachAudioProcessorEditor() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;
    void initSharedData();
    void buildFullUI();
    void detectNewMessengers();

    MixCoachAudioProcessor& processorRef_;
    SharedData*             sharedData_;

    // UI completa (creada LAZY cuando sharedData esté disponible)
    std::unique_ptr<MainTabbedComponent> tabbedComponent_;

    // Placeholder mientras sharedData no está disponible
    juce::Label placeholderLabel_;
    bool fullUIBuilt_{false};
    uint32_t lastInitAttemptMs_{0}; // backoff para reintentos

    // Seguimiento de slots ya anunciados
    std::array<bool, SlotRegistry::kMaxSlots> announcedSlots_{};
    int lastActiveSlotCount_{0};

    // Layout
    juce::Label versionLabel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixCoachAudioProcessorEditor)
};

} // namespace mixcoach
