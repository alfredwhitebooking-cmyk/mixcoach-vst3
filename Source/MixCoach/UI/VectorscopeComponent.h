#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <cmath>
#include <array>
#include "MixCoachTheme.h"
#include "../../Common/audio/DiagnosticBridge.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  VectorscopeComponent — Vectorscope circular con traza dinámica
//  Sin correlation meter interno — el panel padre (PhaseScopePanel)
//  maneja el header, correlation meter y métricas.
//  Este componente SOLO dibuja el círculo con grid + trace,
//  más overlay de diagnóstico de fase si hay advertencias activas.
// ═══════════════════════════════════════════════════════════════════════════
class VectorscopeComponent : public juce::Component {
public:
    VectorscopeComponent();
    ~VectorscopeComponent() override = default;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void pushSample(float left, float right);
    void setDisplayCorrelation(float correlation);

    /** Setea el ancho estéreo promedio para overlay numérico. */
    void setStereoWidth(float width) noexcept { stereoWidth_ = juce::jlimit(0.0f, 1.0f, width); }

    /** Decaimiento phosphor + repaint si hay traza visible. */
    bool advanceFrame(double sampleRateHz = 60.0, bool allowRepaint = true);

    // ═══ Phase Diagnostic Overlay ══════════════════════════════════════
    /** Establece el diagnóstico de fase activo (o nullptr para limpiar).
        El componente dibujará un glow de advertencia y texto informativo. */
    void setPhaseDiagnostic(const PhaseDiagnostic* diagnostic);

private:
    static constexpr int kTraceLen = 1024;

    struct Point {
        float x = 0.0f, y = 0.0f;
        float alpha = 0.0f;
    };

    std::array<Point, kTraceLen> trace_{};
    int writePos_ = 0;

    // ─── Stereo width overlay ──────────────────────────────────────────
    float stereoWidth_ = 0.0f;

    // ─── Correlation meter state ───────────────────────────────────────
    float correlation_ = 1.0f;

    // ─── Phase diagnostic overlay state ────────────────────────────────
    PhaseDiagnostic phaseDiagnostic_;
    bool hasPhaseDiagnostic_ = false;

    // ─── Drawing methods ───────────────────────────────────────────────
    void drawGrid(juce::Graphics& g, juce::Rectangle<float> circleArea);
    void drawTrace(juce::Graphics& g, juce::Rectangle<float> circleArea);
    void drawPhaseOverlay(juce::Graphics& g, juce::Rectangle<float> circleArea);
    void rebuildGridCache();

    [[nodiscard]] juce::Rectangle<float> plotCircleArea() const;

    // ─── Cached backgrounds ────────────────────────────────────────────
    juce::Image gridCache_;
    bool gridCacheValid_ = false;

    // ─── Reusable paths ────────────────────────────────────────────────
    juce::Path oldTracePath_;
    juce::Path newTracePath_;
};

} // namespace mixcoach
