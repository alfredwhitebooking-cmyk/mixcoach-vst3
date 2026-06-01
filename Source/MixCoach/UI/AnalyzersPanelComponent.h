#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <functional>
#include "MixCoachTheme.h"
#include "SmoothValue.h"
#include "LUFSMeter.h"
#include "StereoVUMeter.h"
#include "SpectrographComponent.h"
#include "VectorscopeComponent.h"
#include "PhaseCorrelationMeter.h"
#include "CrestHistogram.h"
#include "PlaylistComponent.h"
#include "MeterComponent.h"
#include "AnalogVUMeter.h"
#include "VUMetersPanel.h"
#include "PhaseScopePanel.h"

namespace mixcoach {

class SharedData; // forward declaration

// ═══════════════════════════════════════════════════════════════════════════
//  AnalyzersPanelComponent — Panel principal de metering
//  Layout 5 sesiones:
//    Playlist (25% L) | Meter (25% C-top) | Spectrum (50% R-top)
//                      | Phase Scope (C-bot) | VU Meters (50% R-bot)
// ═══════════════════════════════════════════════════════════════════════════
class AnalyzersPanelComponent : public juce::Component {
public:
    explicit AnalyzersPanelComponent(SharedData& sharedData);
    ~AnalyzersPanelComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    // Actualizar analizadores con datos de un slot específico
    void updateAnalyzers(SlotRegistry& registry);

    // Obtener playlist (para callbacks externos)
    PlaylistComponent& getPlaylist() noexcept { return playlist_; }

    // Getter para slot seleccionado
    int getSelectedSlot() const noexcept { return selectedSlot_; }

    // Getter para spectrograph (fast updates desde MainTabbedComponent)
    SpectrographComponent& getSpectrograph() noexcept { return spectrograph_; }

    // Callback: re-scan solicitado externamente
    std::function<void()> onRescanRequested;

    // Actualización rápida de meters (60fps ligero)
    void fastUpdateMeters(SlotRegistry& registry);

private:
    // ─── 5 Sesiones ──────────────────────────────────────────────────────
    PlaylistComponent        playlist_;
    MeterComponent           meter_;
    SpectrographComponent    spectrograph_;
    PhaseScopePanel          phaseScope_;
    VUMetersPanel            vuMeters_;

    // ─── Estado ───────────────────────────────────────────────────────────
    int selectedSlot_ = -1;
    juce::Colour selectedColour_{ 0xFF3498DB };
    juce::String selectedTrackName_;

    // Referencia a SharedData
    SharedData& sharedData_;
};

} // namespace mixcoach
