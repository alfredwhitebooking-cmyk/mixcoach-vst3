#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Common/types/Types.h"
#include "../../Common/types/Constants.h"
#include "../../MixCoach/UI/MixCoachTheme.h"
#include "../../MixCoach/UI/SmoothValue.h"
#include "../core/MessengerType.h"

namespace mixcoach {

class MessengerAudioProcessor;

// ═══════════════════════════════════════════════════════════════════════════
//  Messenger Audio Processor Editor
//  ═══ TRACK INSPECTOR — Diseño premium tipo vidrio oscuro  ═══════════════
//  Referencia visual: workspace_memory/referencias_visuales/Messenger.png
//
//  Layout:
//    ┌─────────────────────────────────────────┐
//    │  INFORMACION TRACK                 ●    │ ← Título púrpura + LED
//    ├─────────────────────────────────────────┤ ← Divisor
//    │  ✏️ NOMBRE     [________________]      │
//    ├─────────────────────────────────────────┤
//    │  🎨 COLOR      [● ▾______________]      │
//    ├─────────────────────────────────────────┤
//    │  📦 TIPO       [▾________________]      │
//    ├─────────────────────────────────────────┤
//    │  ➡️ RUTEO      [▾________________]      │
//    └─────────────────────────────────────────┘
// ═══════════════════════════════════════════════════════════════════════════
class MessengerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::TextEditor::Listener,
                                      private juce::Timer
{
public:
    explicit MessengerAudioProcessorEditor(MessengerAudioProcessor&);
    ~MessengerAudioProcessorEditor() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    // TextEditor::Listener
    void textEditorTextChanged(juce::TextEditor&) override;
    void timerCallback() override;

    MessengerAudioProcessor& processorRef_;

    // ─── Componentes UI ──────────────────────────────────────────────

    // Título: "INFORMACION TRACK"
    juce::Label titleLabel_;

    // Nombre de pista (TextEditor)
    juce::TextEditor nameEditor_;

    // Tipo de instrumento (Kick, Snare, Voz, etc.)
    juce::ComboBox typeComboBox_;

    // Ruteo (Bus)
    juce::ComboBox busComboBox_;

    // Color: dropdown con preview circle + PopupMenu
    static constexpr int kNumColours = 8;
    juce::Colour presetColours_[kNumColours];
    juce::Rectangle<float> colourDropdownBounds_;  // Hit area: circle + arrow
    juce::Rectangle<float> colourCircleBounds_;    // Color preview circle
    int selectedColourIndex_ = 0;

    // ─── Layout bounds (para paint) ──────────────────────────────────
    juce::Rectangle<int> titleDividerBounds_;
    juce::Rectangle<int> nameDividerBounds_;
    juce::Rectangle<int> colourDividerBounds_;
    juce::Rectangle<int> typeDividerBounds_;

    // ─── Icon helpers ────────────────────────────────────────────────
    void drawPencilIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour);
    void drawPaletteIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour);
    void drawBoxIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour);
    void drawArrowIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour);
    void drawIconCircle(juce::Graphics& g, juce::Rectangle<float> bounds);

    // ─── Color dropdown ──────────────────────────────────────────────
    void showColourPopup();

    // ─── LED state ───────────────────────────────────────────────────
    bool ledState_ = false;

    // ─── Animation state ────────────────────────────────────────────
    SmoothValue iconHoverAlpha_[4];   // 0=normal, 1=hovered (4 icon rows)
    SmoothValue colourHoverGlow_;     // 0=normal, 1=hovered (colour dropdown)
    SmoothValue typeHoverGlow_;       // 0=normal, 1=hovered (TIPO combo)
    SmoothValue busHoverGlow_;        // 0=normal, 1=hovered (RUTEO combo)
    SmoothValue nameFocusGlow_;       // 0=normal, 1=focused (name editor)
    SmoothValue ledGlow_;             // 0.0-1.0 smooth LED pulse
    int hoveredRow_ = -1;            // -1=none, 0-3=row index
    int hoveredCombo_ = -1;          // -1=none, 0=type, 1=bus
    int iconRowY_[4] = {};           // cached Y position of each row start
    uint32_t lastTimerMs_ = 0;       // for delta-time in 60fps timer
    uint32_t lastTrackTypeSyncMs_ = 0; // throttle for Feedback Loop V9: sync TrackType ~1s
    float ledPhase_ = 0.0f;          // phase accumulator for sine pulse

    // ─── Actions ─────────────────────────────────────────────────────
    void applyType(TrackType type);
    void applyColour(int colourIndex);
    void applyBus(BusType bus);

    // Mouse handling
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MessengerAudioProcessorEditor)
};

} // namespace mixcoach
