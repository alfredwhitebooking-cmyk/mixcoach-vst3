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

class SharedData;

// ═══════════════════════════════════════════════════════════════════════════
//  AnalyzersPanelComponent — Panel de metering profesional
//  Layout 4 secciones en perfecta proporcion:
//
//  ┌──────────────┬───────────────────┬────────────────────────────────┐
//  │              │    METER          │                                │
//  │  PLAYLIST    │   (25% W)         │   SPECTROGRAPH (50% W)        │
//  │  (25% W)     │   (50% H top)     │   (50% H top)                  │
//  │  FULL HEIGHT ├───────────────────┤                                │
//  │  scroll      │   PHASE SCOPE     ├────────────────────────────────┤
//  │              │   (25% W)         │   VU METERS (50% W)            │
//  │              │   (50% H bottom)  │   (50% H bottom)               │
//  │              │   Vec+Phase+Crest │   L ██  R ██  M ░░  S ░░      │
//  └──────────────┴───────────────────┴────────────────────────────────┘
// ═══════════════════════════════════════════════════════════════════════════
class AnalyzersPanelComponent : public juce::Component {
public:
    explicit AnalyzersPanelComponent(SharedData& sharedData);
    ~AnalyzersPanelComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void updateAnalyzers(SlotRegistry& registry, double sampleRate = 48000.0);
    PlaylistComponent& getPlaylist() noexcept { return playlist_; }
    int getSelectedSlot() const noexcept { return selectedSlot_; }
    SpectrographComponent& getSpectrograph() noexcept { return spectrograph_; }

    std::function<void()> onRescanRequested;
    void fastUpdateMeters(SlotRegistry& registry);

    /** 60 Hz: suavizado visual sin leer IPC (solo interpolación). */
    void smoothVisuals(double sampleRateHz = 60.0);

    void setSelectedSlot(int slotIndex);
    std::function<void(int slotIndex)> onTrackSelected;

private:
    // ─── Footer labels ─────────────────────────────────────────────────
    juce::Label footerActiveLabel_;
    juce::Label footerStatusLabel_;
    int lastActiveCount_{0};

    // ─── Analizadores ──────────────────────────────────────────────────
    juce::Viewport          playlistViewport_;
    PlaylistComponent        playlist_;
    MeterComponent           meter_;
    SpectrographComponent    spectrograph_;
    PhaseScopePanel          phaseScope_;
    VUMetersPanel            vuMeters_;

    // ─── Estado ─────────────────────────────────────────────────────────
    int selectedSlot_ = -1;
    juce::Colour selectedColour_{ 0xFF3498DB };
    SharedData& sharedData_;
};

} // namespace mixcoach
