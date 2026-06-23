#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "../../Common/types/Types.h"
#include "../../Common/memory/SharedData.h"
#include "../../Common/memory/SlotRegistry.h"
#include "../../Common/audio/DiagnosticBridge.h"
#include "MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  AudioDNAData — Snapshot de descriptores de audio de alto nivel
//  Agregados en promedios por pista activa + valores por pista para top-N
// ═══════════════════════════════════════════════════════════════════════════
struct AudioDNAData {
    // ─── Promedios globales ─────────────────────────────────────────────
    float avgTransientRatio       = 0.0f;
    int   activeTrackCount        = 0;
    int   highTransientCount      = 0;   // transientRatio > 2.0f

    float avgCrestPerBand[6]      = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    int   crestBandCounts[6]      = { 0, 0, 0, 0, 0, 0 };

    float avgStereoWidthPerBand[6]= { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    int   stereoWidthCounts[6]    = { 0, 0, 0, 0, 0, 0 };

    int   wideSubCount   = 0;   // stereoWidth[0] > 0.5
    int   wideBassCount  = 0;   // stereoWidth[1] > 0.6
    int   widePresAirCount = 0; // stereoWidth[4] > 0.8 || [5] > 0.8

    // ─── Per-track: top N tracks for detailed display ───────────────────
    struct TrackDNA {
        juce::String name;
        float transientRatio       = 0.0f;
        float crestPerBand[6]      = { 0.0f };
        float stereoWidthPerBand[6]= { 0.0f };
    };
    static constexpr int kMaxDisplayTracks = 5;
    std::vector<TrackDNA> tracks;

    [[nodiscard]] bool hasData() const noexcept { return activeTrackCount > 0; }
};

// ═══════════════════════════════════════════════════════════════════════════
//  AudioDNAComponent — Panel compacto con 3 visualizaciones:
//    1. Transient Energy Gauge (horizontal, con marcadores de umbral)
//    2. Crest/Band — 6 barras verticales (Sub→Air)
//    3. Stereo Width/Band — 6 barras verticales con umbral de riesgo
// ═══════════════════════════════════════════════════════════════════════════
class AudioDNAComponent : public juce::Component {
public:
    AudioDNAComponent();
    ~AudioDNAComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    /** Escanea todos los slots activos y computa AudioDNAData. */
    void update(SlotRegistry& registry, SharedData& sharedData);

    /** Conecta el DiagnosticBridge para que el health grid genere marcadores en el spectrograph. */
    void setDiagnosticBridge(DiagnosticBridge* bridge) noexcept { diagnosticBridge_ = bridge; }

private:
    // ─── Secciones de dibujo ─────────────────────────────────────────────
    void drawHeader(juce::Graphics& g, juce::Rectangle<int> area);
    void drawTransientSection(juce::Graphics& g, juce::Rectangle<int> area);
    void drawCrestSection(juce::Graphics& g, juce::Rectangle<int> area);
    void drawStereoWidthSection(juce::Graphics& g, juce::Rectangle<int> area);

    // ─── Helpers de dibujo ───────────────────────────────────────────────
    void drawVerticalBarGroup(juce::Graphics& g, juce::Rectangle<int> area,
                              const float* values, int numBars,
                              const char* const* labels,
                              float maxVal, bool isStereo,
                              const float* thresholds = nullptr);

    void drawTrackList(juce::Graphics& g, juce::Rectangle<int> area);

    // ─── Colores por banda (igual que CrestPanel) ────────────────────────
    static juce::Colour bandColour(int bandIndex, float value, float maxVal);

    // ─── Datos ───────────────────────────────────────────────────────────
    AudioDNAData data_;

    // ─── Cache de fondo ──────────────────────────────────────────────────
    juce::Image bgCache_;
    bool bgCacheValid_ = false;
    void rebuildBgCache();

    // ─── Puente de diagnóstico para marcadores en el spectrograph ───────
    DiagnosticBridge* diagnosticBridge_ = nullptr;
    int64_t lastDiagnosticPushMs_ = 0;
    void pushHealthDiagnostics();

    // ─── Etiquetas de frecuencia ─────────────────────────────────────────
    static constexpr const char* kBandLabels[6] = {
        "Sub", "Bass", "LoMid", "HiMid", "Pres", "Air"
    };
    static constexpr float kCrestMax = 20.0f;   // dB
    static constexpr float kWidthMax = 1.0f;     // 0-1 scale
};

} // namespace mixcoach
