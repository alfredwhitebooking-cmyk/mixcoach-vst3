#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <cmath>
#include <array>
#include <vector>
#include <deque>
#include <algorithm>
#include "MixCoachTheme.h"
#include "../../Common/Types.h"
#include "../../Common/SlotRegistry.h"

namespace mixcoach {

class SharedData; // forward declaration (evita #include en header)

// ═══════════════════════════════════════════════════════════════════════════
//  Forward declarations
// ═══════════════════════════════════════════════════════════════════════════
class VectorscopeComponent;
class PhaseCorrelationMeter;
class CrestHistogramComponent;

// ═══════════════════════════════════════════════════════════════════════════
//  SmoothValue — Suavizado exponencial para animación de medidores
// ═══════════════════════════════════════════════════════════════════════════
class SmoothValue {
public:
    SmoothValue(float initial = -80.0f, float attackMs = 20.0f, float releaseMs = 200.0f);
    void setTarget(float newTarget, double sampleRate = 30.0);
    [[nodiscard]] float getCurrent() const noexcept { return current_; }
    [[nodiscard]] float getTarget() const noexcept { return target_; }
    void setBallistics(float attackMs, float releaseMs);
    void reset(float value = -80.0f);
    operator float() const { return current_; }

private:
    float current_ = -80.0f;
    float target_  = -80.0f;
    float attackCoeff_  = 0.8f;
    float releaseCoeff_ = 0.15f;
};

// ═══════════════════════════════════════════════════════════════════════════
//  LUFSMeter — EBU R128 / ITU BS.1770 Integrated, Short-term, Momentary,
//  True Peak y Loudness Range, con target markers profesionales
// ═══════════════════════════════════════════════════════════════════════════
class LUFSMeter : public juce::Component {
public:
    LUFSMeter();
    ~LUFSMeter() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setIntegrated(float value)  { integrated_.setTarget(value);  repaint(); }
    void setShortTerm(float value)   { shortTerm_.setTarget(value);   repaint(); }
    void setMomentary(float value)   { momentary_.setTarget(value);   repaint(); }
    void setTruePeak(float value)    { truePeak_.setTarget(value);    repaint(); }
    void setRange(float value)       { range_.setTarget(value);       repaint(); }

private:
    SmoothValue integrated_{ -30.0f, 20.0f, 300.0f };
    SmoothValue shortTerm_{  -30.0f, 10.0f, 200.0f };
    SmoothValue momentary_{  -30.0f, 5.0f,  150.0f };
    SmoothValue truePeak_{   -30.0f, 1.0f,  100.0f };
    SmoothValue range_{       0.0f,  50.0f, 400.0f };

    juce::Label titleLabel_;

    // Target markers positions (normalized 0-1 within the scale)
    static constexpr float kTargetIntegrated = 23.0f; // dB scale from bottom
    static constexpr float kTargetStreaming  = 14.0f;
    static constexpr float kTargetBroadcast  = 16.0f;

    void drawBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                 float value, const juce::String& label, const juce::String& unit,
                 juce::Colour colour, float targetLine = -1.0f);
};

// ═══════════════════════════════════════════════════════════════════════════
//  StereoVUMeter — Profesional RMS + Peak VU meter estilo IK Multimedia
//  Dos canales (L/R) con barra de RMS + línea de peak hold
// ═══════════════════════════════════════════════════════════════════════════
class StereoVUMeter : public juce::Component {
public:
    StereoVUMeter();
    ~StereoVUMeter() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setLevels(float leftRMS, float rightRMS, float leftPeak, float rightPeak);

    // Colores personalizados por canal
    void setLeftColour(juce::Colour c)  { leftColour_ = c;  repaint(); }
    void setRightColour(juce::Colour c) { rightColour_ = c; repaint(); }

private:
    SmoothValue leftRMS_{   -80.0f, 5.0f, 300.0f };
    SmoothValue rightRMS_{  -80.0f, 5.0f, 300.0f };
    SmoothValue leftPeak_{  -80.0f, 1.0f, 100.0f };
    SmoothValue rightPeak_{ -80.0f, 1.0f, 100.0f };

    float leftPeakHold_    = -80.0f;
    float rightPeakHold_   = -80.0f;
    int   leftHoldTimer_   = 0;
    int   rightHoldTimer_  = 0;

    juce::Colour leftColour_{ 0xFF3498DB };
    juce::Colour rightColour_{ 0xFF2ECC71 };
    juce::Label titleLabel_;
    juce::Label leftLabel_;
    juce::Label rightLabel_;

    void drawChannelMeter(juce::Graphics& g, juce::Rectangle<float> bounds,
                          float rms, float peak, float peakHold,
                          const juce::String& channelLabel, juce::Colour colour);
};

// ═══════════════════════════════════════════════════════════════════════════
//  SpectrographComponent — Espectrograma en tiempo real con colores
//  dinámicos azul → cyan → verde → amarillo → rojo
// ═══════════════════════════════════════════════════════════════════════════
class SpectrographComponent : public juce::Component {
public:
    SpectrographComponent();
    ~SpectrographComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateSpectrum(const float* data, int numBins);

private:
    std::vector<float> bins_;
    std::vector<float> smoothBins_;

    static constexpr int kNumBins = 256;

    juce::Colour getBinColour(float magnitude) const;
};

// ═══════════════════════════════════════════════════════════════════════════
//  VectorscopeComponent — Vectorscopio circular para correlación estéreo
//  Muestra L en X, R en Y con persistencia tipo fósforo
// ═══════════════════════════════════════════════════════════════════════════
class VectorscopeComponent : public juce::Component {
public:
    VectorscopeComponent();
    ~VectorscopeComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Actualizar con muestra estéreo (puede llamarse desde timer)
    void pushSample(float left, float right);

private:
    static constexpr int kTraceLen = 256;
    static constexpr int kPhosphorDecay = 8;

    struct Point {
        float x = 0.0f, y = 0.0f;
        float alpha = 0.0f;
    };

    std::array<Point, kTraceLen> trace_{};
    int writePos_ = 0;
    juce::Label titleLabel_;
    juce::Label corrLabel_;

    void drawGrid(juce::Graphics& g, juce::Rectangle<float> area);
};

// ═══════════════════════════════════════════════════════════════════════════
//  PhaseCorrelationMeter — Medidor de correlación de fase horizontal
//  -1 (fuera de fase) → 0 (mono) → +1 (en fase)
// ═══════════════════════════════════════════════════════════════════════════
class PhaseCorrelationMeter : public juce::Component {
public:
    PhaseCorrelationMeter();
    ~PhaseCorrelationMeter() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void setCorrelation(float value);

private:
    SmoothValue correlation_{ 1.0f, 5.0f, 100.0f };
    juce::Label titleLabel_;
    juce::Label valueLabel_;
};

// ═══════════════════════════════════════════════════════════════════════════
//  CrestHistogram — Histograma dinámico de Crest Factor (peak/RMS ratio)
//  Muestra la distribución del crest factor en el tiempo
// ═══════════════════════════════════════════════════════════════════════════
class CrestHistogram : public juce::Component {
public:
    CrestHistogram();
    ~CrestHistogram() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void pushCrest(float peakDb, float rmsDb);

private:
    static constexpr int kNumBins = 20;
    static constexpr int kMaxSamples = 500;

    std::array<int, kNumBins> histogram_{};
    int totalSamples_ = 0;
    std::deque<float> recentCrest_;

    juce::Label titleLabel_;
    juce::Label avgLabel_;
    juce::Label maxLabel_;
};

// ═══════════════════════════════════════════════════════════════════════════
//  TrackMiniStrip — Mini barra de nivel para resumen de pista en selector
// ═══════════════════════════════════════════════════════════════════════════
class TrackMiniStrip : public juce::Component {
public:
    TrackMiniStrip();
    ~TrackMiniStrip() override = default;

    void paint(juce::Graphics& g) override;
    void setTrackData(const SlotInfo& info, float peakLeft, float peakRight, bool hasSignal);
    void setSelected(bool selected);
    [[nodiscard]] bool isSelected() const noexcept { return selected_; }
    [[nodiscard]] int getSlotIndex() const noexcept { return slotIndex_; }

    std::function<void(int slotIndex)> onClick;

private:
    SlotInfo info_;
    float peakLeft_  = -80.0f;
    float peakRight_ = -80.0f;
    bool  hasSignal_ = false;
    bool  selected_  = false;
    int   slotIndex_ = -1;

    void mouseDown(const juce::MouseEvent& e) override;
};

// ═══════════════════════════════════════════════════════════════════════════
//  TrackSelectorStrip — Barra horizontal con mini strips de todas las pistas
//  activas, clickeable para seleccionar qué pista analizar en detalle
// ═══════════════════════════════════════════════════════════════════════════
class TrackSelectorStrip : public juce::Component {
public:
    TrackSelectorStrip();
    ~TrackSelectorStrip() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void updateTracks(SlotRegistry& registry);
    void setSelectedTrack(int slotIndex);
    [[nodiscard]] int getSelectedTrack() const noexcept { return selectedSlot_; }

    std::function<void(int slotIndex)> onTrackSelected;

private:
    static constexpr int kMaxVisible = 12;
    std::array<std::unique_ptr<TrackMiniStrip>, kMaxVisible> strips_{};
    int activeTrackCount_ = 0;
    int selectedSlot_ = -1;
    int scrollOffset_ = 0;

    juce::Label placeholderLabel_;
};

// ═══════════════════════════════════════════════════════════════════════════
//  AnalyzersPanelComponent — Panel principal de metering profesional
//  Organización en rack vertical con secciones bien delimitadas
// ═══════════════════════════════════════════════════════════════════════════
class AnalyzersPanelComponent : public juce::Component {
public:
    explicit AnalyzersPanelComponent(SharedData& sharedData);
    ~AnalyzersPanelComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    // Actualizar analizadores con datos de un slot específico
    void updateAnalyzers(SlotRegistry& registry);

    // Obtener selector de pistas
    TrackSelectorStrip& getTrackSelector() noexcept { return trackSelector_; }

private:
    // ─── Componentes ─────────────────────────────────────────────────────
    TrackSelectorStrip       trackSelector_;
    SpectrographComponent    spectrograph_;
    LUFSMeter                lufsMeter_;
    StereoVUMeter            stereoVUMeter_;
    VectorscopeComponent     vectorscope_;
    PhaseCorrelationMeter    phaseMeter_;
    CrestHistogram           crestHistogram_;

    // ─── Layout helpers ───────────────────────────────────────────────────
    juce::Label  headerLabel_;
    juce::Label  selectedTrackLabel_;

    // ─── Estado ───────────────────────────────────────────────────────────
    int selectedSlot_ = -1;
    juce::Colour selectedColour_{ 0xFF3498DB };
    juce::String selectedTrackName_;

    // Referencia a SharedData (evita llamar getInstance() desde lambdas)
    SharedData& sharedData_;

    // Smoothing para crest factor tracking
};

} // namespace mixcoach
