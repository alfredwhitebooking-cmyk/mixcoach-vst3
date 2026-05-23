#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace mixcoach {

class MessengerAudioProcessor;
class VUMeter;
class WaveformView;
class ColourSwatch;
class ColourPresetStrip;

// ─── Change listener interno para el selector de color ─────────────────────
class ColourSelectorListener : public juce::ChangeListener
{
public:
    std::function<void(juce::Colour)> onColourChanged;

    void changeListenerCallback(juce::ChangeBroadcaster* source) override
    {
        if (auto* selector = dynamic_cast<juce::ColourSelector*>(source)) {
            if (onColourChanged)
                onColourChanged(selector->getCurrentColour());
        }
    }
};

// ─── Messenger Plugin Editor (Rediseñado) ─────────────────────────────────
class MessengerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     public juce::TextEditor::Listener,
                                     public juce::Timer
{
public:
    explicit MessengerAudioProcessorEditor(MessengerAudioProcessor&);
    ~MessengerAudioProcessorEditor() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    // TextEditor::Listener
    void textEditorTextChanged(juce::TextEditor&) override;

    // juce::Timer
    void timerCallback() override;

    void openColourSelector();

    MessengerAudioProcessor& processorRef_;

    // Componentes UI
    juce::Label      iconLabel_;
    juce::TextEditor nameEditor_;
    std::unique_ptr<ColourSwatch> colourSwatch_;
    std::unique_ptr<juce::Component> colourPresetStrip_;
    ColourPresetStrip* colourPresetStripRaw_{nullptr};

    juce::Label      statusLabel_;
    juce::Label      routingLabel_;
    juce::ComboBox   busComboBox_;

    juce::Label      levelLeftLabel_;
    juce::Label      levelRightLabel_;
    juce::Label      correlationLabel_;
    juce::Label      rmsLabel_;

    std::unique_ptr<VUMeter> vuMeterLeft_;
    std::unique_ptr<VUMeter> vuMeterRight_;
    std::unique_ptr<WaveformView> waveformView_;

    // Datos de telemetria para el timer
    float lastPeakLeft_{-100.0f};
    float lastPeakRight_{-100.0f};
    float lastRMS_{-100.0f};
    float lastCorrelation_{1.0f};

    juce::Colour currentColour_{0xFF808080};

    // Listener de cambio de color
    ColourSelectorListener colourListener_;

    void applyPresetColour(juce::Colour colour);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MessengerAudioProcessorEditor)
};

} // namespace mixcoach
