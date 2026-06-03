#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "StereoMeter.h"
#include "WaveformView.h"
#include "CircularGauge.h"
#include "ColourSwatch.h"
#include "ColourPresetStrip.h"
#include "../../Common/types/Types.h"

namespace mixcoach {

class MessengerAudioProcessor;

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
//  3-column NIVELES layout: INPUT (L+R) | GR (circular) | OUTPUT (L+R)
//  VU meters con escala exacta +6..-60 dB, peak triangle ◀, gradiente
// ═══════════════════════════════════════════════════════════════════════════
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

    // ─── Componentes UI ─────────────────────────────────────────────────

    // ===== INFO PANEL (form-style rows) =====

    // Row 1: NOMBRE
    juce::TextEditor nameEditor_;
    std::unique_ptr<ColourSwatch> colourSwatch_;
    std::unique_ptr<juce::Component> colourPresetStrip_;
    ColourPresetStrip* colourPresetStripRaw_{nullptr};

    // Row 2: GRUPO
    juce::Label      busValueLabel_;   // Texto violeta del bus (Drums Bus)
    juce::ComboBox   busComboBox_;

    // Row 3: COLOR (coloured dot + preset strip)
    juce::Label      statusLabel_;

    // Row 4: TIPO (derivado del bus)
    juce::Label      tipoValueLabel_;

    // Row 5: PRIORIDAD
    juce::Label      prioridadValueLabel_;

    // Row 6: NOTAS
    juce::TextEditor notasEditor_;

    // Row 7: MUTE button
    juce::TextButton muteButton_;

    // ─── 3-Column NIVELES: INPUT | REDUCCIÓN DE GANANCIA | OUTPUT ──────

    // Column headers
    juce::Label      inputLabel_;
    juce::Label      grLabel_;
    juce::Label      outputLabel_;

    // Column value labels (grandes, blancos)
    juce::Label      inputValueLabel_;
    juce::Label      grValueLabel_;
    juce::Label      outputValueLabel_;

    // 2 StereoMeters: Input (L+R), Output (L+R)
    std::unique_ptr<StereoMeter> stereoInput_;
    std::unique_ptr<StereoMeter> stereoOutput_;
    
    // Una etiqueta L/R debajo de cada estéreo
    juce::Label      stereoInputLabel_;
    juce::Label      stereoOutputLabel_;

    // Circular GR Gauge
    std::unique_ptr<CircularGauge> grGauge_;

    // Bottom info row (RMS, φ correlation)
    juce::Label      rmsLabel_;
    juce::Label      correlationLabel_;

    // Bounds para el fondo del panel NIVELES (con esquinas redondeadas)
    juce::Rectangle<int> nivelesPanelBounds_;

    // Datos de telemetria para el timer
    float lastPeakLeft_{-100.0f};
    float lastPeakRight_{-100.0f};
    float lastRMS_{-100.0f};
    float lastCorrelation_{1.0f};
    float lastGR_{0.0f};  // Gain reduction simulada (dB)

    juce::Colour currentColour_{0xFF808080};

    // Listener de cambio de color
    ColourSelectorListener colourListener_;

    void applyPresetColour(juce::Colour colour);
    void updateMuteDisplay();

    // ─── Helpers inline (evitan problemas de encoding con MSVC) ──────
    static juce::String getTipoForBus(BusType bus) {
        switch (bus) {
            case BusType::Drums:   return "Bateria / Transiente";
            case BusType::Bass:    return "Bajo / Fundamental";
            case BusType::Guitars: return "Guitarra / Armonica";
            case BusType::Keys:    return "Teclado / Armonico";
            case BusType::Vocals:  return "Voz / Melodica";
            case BusType::FX:      return "FX / Textura";
            default:               return "—";
        }
    }
    static juce::String getPrioridadForBus(BusType bus) {
        switch (bus) {
            case BusType::Drums:
            case BusType::Vocals:  return "Alta";
            case BusType::Bass:
            case BusType::Keys:    return "Media";
            case BusType::Guitars:
            case BusType::FX:      return "Baja";
            default:               return "—";
        }
    }
    static void drawRowIcon(juce::Graphics& g, int rowIndex, juce::Rectangle<float> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MessengerAudioProcessorEditor)
};

} // namespace mixcoach
