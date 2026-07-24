#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  TrackGainRow — Datos de una fila en la tabla de Gain Staging
// ═══════════════════════════════════════════════════════════════════════════
struct TrackGainRow
{
    int slotIndex = -1;
    juce::String trackName;
    juce::String roleName;
    float currentPeakDb = -100.0f;
    float targetPeakDb = -18.0f;
    float suggestedDeltaDb = 0.0f;
    bool clipping = false;
    bool isApplied = false;
};

// ═══════════════════════════════════════════════════════════════════════════
//  GainStagingPanel — Tabla de niveles de pista con targets y botón Aplicar
//
//  Muestra en formato de tabla:
//    [Nombre]  [Rol]  [Peak actual]  [→ Target]  [Barra nivel]  [Aplicar]
//  Aparece automáticamente al entrar en CoachRoomState::GainStaging
//  y se refresca desde CoachEngine::analyzeAllTracksGain().
// ═══════════════════════════════════════════════════════════════════════════
class GainStagingPanel : public juce::Component
{
public:
    GainStagingPanel();
    ~GainStagingPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    /** Actualiza la lista de tracks desde datos del engine. */
    void setTrackData(const std::vector<TrackGainRow>& rows);
    void clear();

    /** Retorna true si hay datos mostrados. */
    bool hasData() const noexcept { return !rows_.empty(); }

    /** Callback cuando el usuario hace clic en "Aplicar" para una pista.
        @param slotIndex  Índice del slot
        @param deltaDb    Cantidad de dB a ajustar */
    std::function<void(int slotIndex, float deltaDb)> onApplyGain;

    /** Callback cuando el usuario hace clic en "Aplicar todas". */
    std::function<void()> onApplyAll;

private:
    std::vector<TrackGainRow> rows_;

    // Layout
    static constexpr int kHeaderHeight = 22;
    static constexpr int kRowHeight = 32;
    static constexpr int kGap = 2;
    static constexpr int kPadding = 8;
    static constexpr int kNameWidth = 100;
    static constexpr int kRoleWidth = 60;
    static constexpr int kPeakWidth = 50;
    static constexpr int kTargetWidth = 50;
    static constexpr int kBarWidth = 80;
    static constexpr int kApplyWidth = 50;
    static constexpr int kButtonMargin = 12;

    // Hit test bounds
    std::vector<juce::Rectangle<int>> applyBtnBounds_;
    juce::Rectangle<int> applyAllBounds_;
    int hoveredRow_ = -1;
    int hoveredApplyAll_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GainStagingPanel)
};

} // namespace mixcoach
