#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>
#include "../audio/AudioAnalyzer.h"
#include "MixCoachTheme.h"
#include "SmoothValue.h"
#include "SpectrographComponent.h"
#include "VectorscopeComponent.h"
#include "StereoWidthMeter.h"
#include "PhaseCorrelationMeter.h"
#include "../audio/ReferenceAnalyzer.h"
#include "../engine/CoachEngine.h"
#include "CrestPanel.h"
#include "AudioDNAComponent.h"
#include "../../Common/audio/DiagnosticBridge.h"

// Sub-componentes extraídos del refactor
#include "MeterPanel.h"
#include "MeterCard.h"
#include "VintageVUMeters.h"
#include "RefToggle.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  PhaseScopePanel — SESIÓN 5: Panel inferior izquierdo (~48% width)
//  Correlation meter (top) + Vectorscope (bottom-left) + Crest gauge (bottom-right)
//  + PEAK/RMS/CREST metrics table
// ═══════════════════════════════════════════════════════════════════════════
class PhaseScopePanel : public juce::Component,
                        public juce::ChangeListener {
public:
    PhaseScopePanel();
    ~PhaseScopePanel() override;
    void resized() override;
    void paint(juce::Graphics& g) override;

    // ═══ ChangeListener ══════════════════════════════════════════════
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    void setCorrelation(float v) { correlation_.setTargetValue(v); }
    VectorscopeComponent& getVectorscope() noexcept { return vectorscope_; }
    void pushCrest(float peak, float rms);
    bool advanceVisuals(double sr = 60.0, bool allowRepaint = true);

    // ═══ Phase Diagnostic Overlay ══════════════════════════════════════
    /** Recibe diagnósticos de fase desde el DiagnosticBridge. */
    void setPhaseDiagnostics(const std::vector<PhaseDiagnostic>& diagnostics);

    /** Setea el DiagnosticBridge para recibir PhaseDiagnostics via ChangeListener.
        Reemplaza cualquier conexión anterior. */
    void setDiagnosticBridge(DiagnosticBridge* bridge);

private:
    /** Puntero al bridge (no owned), usado por changeListenerCallback. */
    DiagnosticBridge* diagnosticBridge_ = nullptr;
    SmoothValue correlation_{ 1.0f, 5.0f, 100.0f };
    float currentPeak_ = -80.0f, currentRms_ = -80.0f;
    VectorscopeComponent vectorscope_;
};

// ═══════════════════════════════════════════════════════════════════════════
//  AnalyzersPanelComponent — Panel de metering profesional (TAB 2)
//
//  Layout (basado en referencia visual):
//    SESIÓN 2 (28% W, 50% H): MeterPanel - VU + cards + LUFS
//    SESIÓN 3 (72% W, 50% H): SpectrographComponent - FFT RTA
//    SESIÓN 5 (48% W, 42% H): PhaseScopePanel - correlation + vec + crest
//    SESIÓN 4 (52% W, 42% H): VintageVUMeters - 2x2 vintage analog
// ═══════════════════════════════════════════════════════════════════════════
class AnalyzersPanelComponent : public juce::Component {
public:
    explicit AnalyzersPanelComponent(AudioAnalyzer& audioAnalyzer);
    ~AnalyzersPanelComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void updateAnalyzers(double sampleRate = 48000.0);
    void fastUpdateMeters();
    void smoothVisuals(double sampleRateHz = 60.0);
    SpectrographComponent& getSpectrograph() noexcept { return spectrograph_; }
    void setDiagnosticBridge(DiagnosticBridge* bridge);
    void setCentroidInfo(const SpectrographComponent::CentroidInfo& info)
    {
        spectrograph_.setCentroidInfo(info);
    }
    StereoWidthMeter& getStereoWidthMeter() noexcept { return stereoWidthMeter_; }
    /** Setea el coach engine para actualizar la curva de referencia del spectrograph. */
    void setCoachEngine(CoachEngine* engine) noexcept { coachEngine_ = engine; }

    /** Actualiza la curva de referencia del spectrograph desde el ReferenceAnalyzer.
        Solo tiene efecto si la referencia está cargada y tiene datos espectrales. */
    void updateReferenceCurve(const ReferenceAnalyzer& refAnalyzer);

    void updateAudioDNA(SlotRegistry& registry, SharedData& sharedData);

    // ═══ Toggle extra panels ═══════════════════════════════════════════
    void setShowDNA(bool show) noexcept { showDna_ = show; resized(); repaint(); }
    void setShowWidth(bool show) noexcept { showWidth_ = show; resized(); repaint(); }
    void setShowCrest(bool show) noexcept { showCrest_ = show; resized(); repaint(); }
    void setShowAI(bool show) noexcept { showAi_ = show; resized(); repaint(); }
    [[nodiscard]] bool isShowingDNA() const noexcept { return showDna_; }
    [[nodiscard]] bool isShowingWidth() const noexcept { return showWidth_; }
    [[nodiscard]] bool isShowingCrest() const noexcept { return showCrest_; }
    [[nodiscard]] bool isShowingAI() const noexcept { return showAi_; }

private:
    void refreshSpectrograph();
    void feedFromAudioAnalyzer(bool includeSpectrograph = true);
    void rebuildBgCache();

    void mouseDown(const juce::MouseEvent& e) override;

    // ─── Sub-components ──────────────────────────────────────────────────
    MeterPanel            meterPanel_;
    SpectrographComponent spectrograph_;
    PhaseScopePanel       phaseScope_;
    VintageVUMeters       vuMeters_;
    CrestPanel            crestPanel_;
    StereoWidthMeter      stereoWidthMeter_;
    AudioDNAComponent     audioDNA_;
    std::unique_ptr<RefToggle> refToggle_;

    // ─── Toggle states for extra panels ─────────────────────────────────
    bool showDna_ = false;
    bool showWidth_ = false;
    bool showCrest_ = false;
    bool showAi_ = true;

    struct ToggleBtn {
        juce::Rectangle<int> bounds;
        juce::String label;
        bool* active;
    };
    std::vector<ToggleBtn> toggleBtns_;

    // ─── Background cache (dot grid + gradient, painted una vez) ────────
    juce::Image bgCache_;
    bool bgCacheValid_ = false;

    // ─── Throttle para refreshSpectrograph (máx ~25 FPS) ───────────────
    uint32_t lastSpectrumUpdateMs_ = 0;

    // ─── Data sources ──────────────────────────────────────────────────
    AudioAnalyzer& audioAnalyzer_;
    CoachEngine* coachEngine_ = nullptr;
};

} // namespace mixcoach
