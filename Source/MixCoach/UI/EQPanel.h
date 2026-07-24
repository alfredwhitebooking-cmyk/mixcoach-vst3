#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  EQTrackData — Datos espectrales de una pista para el EQPanel
// ═══════════════════════════════════════════════════════════════════════════
struct EQTrackData
{
    int slotIndex = -1;
    juce::String trackName;
    juce::String roleName;
    // 6 regiones espectrales (Sub, Bass, LoMid, HiMid, Presence, Air)
    float currentEnergy[6] = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};
    float targetEnergy[6] = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};
    bool hasTarget = false;
};

// ═══════════════════════════════════════════════════════════════════════════
//  EQPanel — Superposición espectral entre pista problemática y target
//
//  Layout:
//    [Header: Nombre pista + Rol]
//    [Curva espectral: 6 regiones, actual vs target]
//    [Slider: Frecuencia (Hz) — 20..20000]
//    [Slider: Q (0.1..10)]
//    [Slider: Ganancia (dB) — -12..+12]
//    [Botón: "Aplicar EQ"]
//
//  Aparece automáticamente al entrar en CoachRoomState::EQ
//  o cuando el Coach detecta enmascaramiento espectral.
// ═══════════════════════════════════════════════════════════════════════════
class EQPanel : public juce::Component
{
public:
    EQPanel();
    ~EQPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    /** Actualiza los datos espectrales de la pista actual. */
    void setTrackData(const EQTrackData& data);

    /** Actualiza datos de 2 pistas para superposición espectral (ej. Kick + Bass enmascarados).
        @param primary  Pista principal (curva sólida)
        @param secondary  Segunda pista (curva punteada, color diferente) */
    void setTrackPair(const EQTrackData& primary, const EQTrackData& secondary);

    void clear();

    bool hasData() const noexcept { return hasData_; }

    // ─── Slider values (read/write) ──────────────────────────────────────
    float getFrequency() const noexcept { return frequencyHz_; }
    void setFrequency(float hz) { frequencyHz_ = hz; repaint(); }

    float getQ() const noexcept { return q_; }
    void setQ(float q) { q_ = q; repaint(); }

    float getGainDb() const noexcept { return gainDb_; }
    void setGainDb(float db) { gainDb_ = db; repaint(); }

    /** Callback cuando el usuario hace clic en "Aplicar EQ". */
    std::function<void(int slotIndex, float frequencyHz, float q, float gainDb)> onApplyEQ;

private:
    // ─── Drawing helpers ───────────────────────────────────────────────
    void drawSpectralOverlay(juce::Graphics& g, juce::Rectangle<int> area);
    void drawSlider(juce::Graphics& g, juce::Rectangle<int> bounds,
                    float value, float min, float max, const char* label, const char* unit);
    void drawRegionLabel(juce::Graphics& g, juce::Rectangle<int> area);

    // ─── Hit testing ─────────────────────────────────────────────────
    enum class DragTarget { None, FreqSlider, QSlider, GainSlider, ApplyBtn };
    DragTarget hitTestSlider(juce::Point<int> pos) const;

    // ─── State ────────────────────────────────────────────────────────
    bool hasData_ = false;
    EQTrackData data_;
    EQTrackData secondTrackData_;
    bool hasSecondTrack_ = false;

    // Slider state
    float frequencyHz_ = 1000.0f;
    float q_ = 1.0f;
    float gainDb_ = 0.0f;

    // Drag state
    DragTarget dragTarget_{DragTarget::None};
    float dragStartValue_ = 0.0f;
    juce::Point<int> dragStartPos_;

    // Hover state
    DragTarget hoverTarget_{DragTarget::None};

    // ─── Layout constants ─────────────────────────────────────────────
    static constexpr int kPadding = 8;
    static constexpr int kHeaderH = 22;
    static constexpr int kSpecH = 130;
    static constexpr int kSliderH = 28;
    static constexpr int kSliderLabelW = 90;
    static constexpr int kSliderTrackH = 8;
    static constexpr int kApplyH = 24;
    static constexpr int kRegionLabelH = 14;

    // Slider regions (hit testing)
    juce::Rectangle<int> freqSliderBounds_;
    juce::Rectangle<int> qSliderBounds_;
    juce::Rectangle<int> gainSliderBounds_;
    juce::Rectangle<int> applyBtnBounds_;

    static constexpr float kFreqMin = 20.0f;
    static constexpr float kFreqMax = 20000.0f;
    static constexpr float kQMin = 0.1f;
    static constexpr float kQMax = 10.0f;
    static constexpr float kGainMin = -12.0f;
    static constexpr float kGainMax = 12.0f;

    // Spectral region labels
    static constexpr const char* kRegionNames[6] = {
        "Sub", "Bass", "LoMid", "HiMid", "Pres", "Air"
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQPanel)
};

} // namespace mixcoach
