#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "SmoothValue.h"
#include "CrestPanel.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  TrackCompressionRow — Datos de una fila en la tabla de compresión
// ═══════════════════════════════════════════════════════════════════════════
struct TrackCompressionRow
{
    int slotIndex = -1;
    juce::String trackName;
    juce::String roleName;
    float currentCrestDb = 12.0f;
    float targetCrestDb = 8.0f;
    float suggestedRatio = 3.0f;
    float suggestedAttackMs = 20.0f;
    float suggestedReleaseMs = 100.0f;
    float suggestedThresholdDb = -18.0f;
    bool isApplied = false;
};

// ═══════════════════════════════════════════════════════════════════════════
//  CompressionPanel — Panel de compresión con crest meter animado + gauge
//
//  Aparece automáticamente al entrar en CoachRoomState::Compression.
//  Muestra crest actual vs target para cada pista problemática.
//  El crest meter se anima suavemente usando SmoothValue.
//  Al hacer hover sobre una fila, se muestra el CrestPanel (gauge semicircular
//  con aguja) con los valores de esa pista.
//  Ofrece sugerencias de ratio/attack/release/threshold.
// ═══════════════════════════════════════════════════════════════════════════
class CompressionPanel : public juce::Component
{
public:
    CompressionPanel();
    ~CompressionPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    /**
     * Actualiza los datos de las pistas (crest actual, target, sugerencias).
     * Los valores actuales se animan suavemente hacia los nuevos targets.
     */
    void setTrackData(const std::vector<TrackCompressionRow>& rows);

    /**
     * Avanza la animación del crest meter. Llamar desde el timer del sistema (~60 Hz).
     * Devuelve true si algún valor cambió (necesita repaint).
     */
    bool advanceVisuals(double sampleRateHz = 60.0);

    void clear();

    bool hasData() const noexcept { return !rows_.empty(); }

    /** Callback cuando el usuario aplica compresión a una pista. */
    std::function<void(int slotIndex, float ratio, float attackMs,
                       float releaseMs, float thresholdDb)> onApplyCompression;

    /** Callback para aplicar compresión a todas las pistas. */
    std::function<void()> onApplyAll;

private:
    std::vector<TrackCompressionRow> rows_;

    // SmoothValues animados por track (crest actual animado)
    std::vector<SmoothValue> crestAnims_;

    // CrestPanel embebido (gauge semicircular con aguja)
    CrestPanel crestGauge_;
    bool crestGaugeVisible_ = false;
    int crestGaugeRowIndex_ = -1; // Índice de la fila hovered que alimenta el gauge

    // Layout
    static constexpr int kHeaderHeight = 22;
    static constexpr int kRowHeight = 38;
    static constexpr int kGap = 2;
    static constexpr int kPadding = 8;
    static constexpr int kNameWidth = 90;
    static constexpr int kCrestBarW = 80;
    static constexpr int kTargetW = 30;
    static constexpr int kRatioW = 35;
    static constexpr int kAttackW = 35;
    static constexpr int kReleaseW = 35;
    static constexpr int kApplyW = 50;

    std::vector<juce::Rectangle<int>> applyBtnBounds_;
    juce::Rectangle<int> applyAllBounds_;
    int hoveredRow_ = -1;
    int hoveredApplyAll_ = -1;

    void drawAnimatedCrestBar(juce::Graphics& g, juce::Rectangle<int> barBounds,
                              const TrackCompressionRow& row, int animIndex);

    void feedGauge(int rowIndex);
    void hideGauge();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CompressionPanel)
};

} // namespace mixcoach
