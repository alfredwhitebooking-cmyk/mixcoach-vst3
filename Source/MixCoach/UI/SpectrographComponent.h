#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>
#include "SmoothValue.h"
#include "MixCoachTheme.h"
#include "../../Common/audio/DiagnosticBridge.h"

namespace mixcoach {

    // SESIÓN 3 – RTA (barras log 20 Hz–20 kHz, estilo UI_REFERENCES Tab2)
    class SpectrographComponent : public juce::Component, private juce::ChangeListener
    {
    public:
        SpectrographComponent();
        ~SpectrographComponent() override;

        /** Configura el puente de diagnóstico para overlay visual.
            Se registra automáticamente como ChangeListener. */
        void setDiagnosticBridge(DiagnosticBridge* bridge);

        /** Retorna los diagnósticos activos actuales (para otros componentes). */
        [[nodiscard]] const std::vector<BandDiagnostic>& getActiveDiagnostics() const noexcept
        {
            return activeDiagnostics_;
        }

        [[nodiscard]] bool hasActiveDiagnostics() const noexcept { return !activeDiagnostics_.empty(); }

        void paint(juce::Graphics& g) override;
        void resized() override;

        void setSampleRate(double sampleRate);
        void updateSpectrum(const float* data, int numBins);
        void resetSpectrum();
        /** Avanza suavizado de barras RTA a sampleRateHz (p. ej. 60). */
        bool smoothSpectrum(double sampleRateHz = 60.0, bool allowRepaint = true);

        // ═══ Reference Overlay (commercial mix curve) ════════════════════════════
        /** Activa/desactiva el overlay de referencia. */
        void setReferenceEnabled(bool enabled)
        {
            referenceEnabled_ = enabled;
            repaint();
        }

        /** ¿El overlay de referencia está visible? */
        [[nodiscard]] bool isReferenceEnabled() const noexcept { return referenceEnabled_; }

        /** Computa la curva por defecto de mezcla comercial (pink noise + shaping). */
        void computeDefaultReferenceCurve();
        /** Carga una curva externa desde un ReferenceAnalyzer (96 bands). */
        void setReferenceCurve(const float* data, int numBands);

    private:
        // IK Multimedia Metering: 60 bands logarítmicas (IK usa 60 barritas) + 2048 high-res.
        static constexpr int kNumRtaBands = 60;
        // Envelope de alta resolución con 2048 puntos log-spaced (4x más que antes)
        // para una reconstrucción espectral más precisa y transiciones banda a banda
        // más suaves. Cada banda RTA agrega ~34 puntos high-res en promedio.
        static constexpr int kHighResPoints     = 2048;
        static constexpr int kMaxFFTBins        = 8192;
        static constexpr float kMinFreq         = 20.0f;
        static constexpr float kMaxFreq         = 20000.0f;
        static constexpr float kDisplayTopDb    = 0.0f;
        static constexpr float kDisplayBottomDb = -45.0f; // IK Multimedia: -45 a 0 dB

        // ═══ Peak/RMS blend ratio (estilo IK T-RackS: 35% peak + 65% RMS) ═══
        static constexpr float kBlendPeakRatio = 0.35f;
        static constexpr float kBlendRmsRatio  = 0.65f;

        struct RtaBand
        {
            float centerHz = 0.0f;
            float lowHz    = 0.0f;
            float highHz   = 0.0f;
        };

        /** Geometría del gráfico, en coordenadas locales de staticCache_ (origen plotArea_). */
        struct PlotLayout
        {
            juce::Rectangle<float> plot;
            juce::Rectangle<float> dbCol;
            bool valid = false;
        };

        double sampleRate_ = 48000.0;
        std::vector<RtaBand> bands_;
        std::vector<SmoothValue> bandLevels_;    // Peak levels (smoothed)
        std::vector<SmoothValue> bandRmsLevels_; // RMS levels (smoothed, for fill)
        std::vector<float> bandPeaks_;
        std::vector<float> peakDecayRates_; // Per-band τ (seconds): 0.35s@20Hz→0.15s@20kHz
        std::vector<float> peakHoldTimers_; // Per-band hold timer for dual-stage peak decay

        // ═══ High-resolution spectral envelope (IK-style) ═══════════════════════
        // 512 log-spaced internal points, interpolated from FFT bins after
        // fractional octave smoothing. Each visual band aggregates from these
        // high-res points for smooth, continuous spectrum reconstruction.
        std::vector<float> highResEnvelope_;

        // ═══ Display Mode (Peak / RMS / Hybrid) ═══════════════════════════════
        enum DisplayMode
        {
            kPeak,
            kRms,
            kHybrid
        };

        DisplayMode displayMode_ = kPeak; // IK Multimedia usa Peak mode (NO hybrid)

        // ═══ Slope Compensation configurable (0 / 1.5 / 3.0 / 4.5 dB/octave) ═══
        // Default 4.5 dB/oct = mastering standard (IK Multimedia Metering usa esta
        // pendiente para que pink noise se vea plano y el balance tonal sea neutral).
        static constexpr float kSlopePresets[4] = {0.0f, 1.5f, 3.0f, 4.5f};
        int slopePresetIndex_ =
            0; // Precision metering: flat dBFS response by default. (Ctrl+Click para cambiar: 0/1.5/3.0/4.5)

        juce::Rectangle<int> plotArea_;
        juce::Rectangle<int> settingsButton_;
        juce::Rectangle<int> referenceButton_;

        uint32_t lastSpectrumUpdateMs_ = 0;
        juce::Image staticCache_;
        bool staticCacheValid_ = false;
        PlotLayout layout_;

        void rebuildBands();
        void invalidateStaticCache();
        void rebuildStaticCache();

        float freqToX(float freqHz, juce::Rectangle<float> plot) const noexcept;
        float dbToDisplayNorm(float db) const noexcept;

        PlotLayout computePlotLayout() const;

        void drawPlotBackground(juce::Graphics& g, juce::Rectangle<float> plot) const;
        void drawGrid(juce::Graphics& g, juce::Rectangle<float> plot) const;
        void drawRtaBars(juce::Graphics& g, juce::Rectangle<float> plot) const;
        void drawDbAxis(juce::Graphics& g, juce::Rectangle<float> labelCol) const;
        void drawFreqAxis(juce::Graphics& g, juce::Rectangle<float> plot) const;
        void drawSettingsChrome(juce::Graphics& g) const;

        // ═══ Waterfall 3D Spectrogram ═══════════════════════════════════════════
        static constexpr int kWaterfallRows = 40; // Mitad de filas = mitad de rects, sin artefactos

        struct WaterfallSlice
        {
            std::array<float, kNumRtaBands> levels{}; // normalized 0..1
            bool valid = false;
        };

        std::array<WaterfallSlice, kWaterfallRows> waterfall_{};
        int waterfallWritePos_  = 0;
        int waterfallCount_     = 0;
        int waterfallFrameSkip_ = 0; // skip counter for ~15fps waterfall capture

        bool waterfallEnabled_ = false;
        bool referenceEnabled_ = false;
        bool pinkNoiseEnabled_ = false; // Pink noise tilt — OFF por defecto (Ctrl+Shift+Click para activar)
        bool peakHoldEnabled_  = true;  // Peak hold lines — ON por defecto (Alt+Click para toggle)

        // ─── Reference overlay curve (kNumRtaBands normalized 0..1 values) ──
        std::vector<float> referenceCurve_;

        void pushWaterfallSlice();
        void drawWaterfall(juce::Graphics& g, juce::Rectangle<float> plot) const;
        void drawReferenceOverlay(juce::Graphics& g, juce::Rectangle<float> plot) const;

        // ═══ Diagnostic Overlay — highlighting problem frequency bands ═══════
        void drawDiagnosticOverlay(juce::Graphics& g, juce::Rectangle<float> plot) const;

        void mouseDown(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;

        /** Inverse of freqToX — converts pixel X to frequency (Hz) using the same
            log mapping, so the hover cursor shows the exact frequency under the mouse. */
        float xToFreq(float x, juce::Rectangle<float> plot) const noexcept;

        /** Inverse of dbToDisplayNorm — converts normalized value back to dB. */
        float displayNormToDb(float norm) const noexcept;

        // ─── ChangeListener ───────────────────────────────────────────────────
        void changeListenerCallback(juce::ChangeBroadcaster* source) override;

        // ─── Hover Cursor State ───────────────────────────────────────────────
        void drawHoverCursor(juce::Graphics& g, juce::Rectangle<float> plot) const;

        bool mouseOverPlot_      = false;
        bool mouseOverRefButton_ = false;
        juce::Point<float> mousePos_;
        float hoverFreqHz_    = 0.0f;
        float hoverDbLevel_   = 0.0f;
        float hoverBandLevel_ = 0.0f; // normalized 0..1

        // ═══ Centroid info (spectral centroid vs genre expected) ═════════════

    public:
        struct CentroidInfo
        {
            float actualHz   = 0.0f;
            float expectedHz = 0.0f;
            juce::String genre;

            bool valid() const noexcept { return actualHz > 0.0f && expectedHz > 0.0f; }
        };

        /** Setea la información del centroide espectral para el overlay visual.
            Se llama desde el timer del editor para mostrar centroid actual vs esperado. */
        void setCentroidInfo(const CentroidInfo& info) noexcept
        {
            centroidInfo_ = info;
            repaint();
        }

    private:
        /** Dibuja el marcador de centroide: línea vertical + badge. */
        void drawCentroidMarker(juce::Graphics& g, juce::Rectangle<float> plot) const;

        CentroidInfo centroidInfo_;

        // ═══ Frequency Markers — glowing dots when coach mentions a specific frequency ═══

    public:
        struct FrequencyMarker
        {
            float frequencyHz = 0.0f; // Center frequency (Hz)
            float severity    = 0.5f; // 0.0-1.0 for intensity
            bool isCritical   = false;
            bool isPraise     = false;
            juce::String label;          // Short label (e.g., "250 Hz")
            juce::String description;    // Full description (e.g., "Exceso en guitarras")
            int64_t timestampMs = 0;     // When added (ms)
            float durationSecs  = 10.0f; // Auto-fade after this time

            [[nodiscard]] float getAgeMs(int64_t nowMs) const noexcept
            {
                return static_cast<float>(static_cast<int64_t>(nowMs - timestampMs));
            }

            [[nodiscard]] float getAlpha(int64_t nowMs) const noexcept
            {
                float age = getAgeMs(nowMs);
                if (age < durationSecs * 1000.0f * 0.85f) return 1.0f;
                float fadeStart = durationSecs * 1000.0f * 0.85f;
                float fadeDur   = durationSecs * 1000.0f * 0.15f;
                if (fadeDur <= 0.0f) return 0.0f;
                return juce::jlimit(0.0f, 1.0f, 1.0f - (age - fadeStart) / fadeDur);
            }

            [[nodiscard]] bool isExpired(int64_t nowMs) const noexcept
            {
                return getAgeMs(nowMs) > durationSecs * 1000.0f;
            }
        };

        /** Añade un marcador de frecuencia que aparece como punto glow en el espectro.
            @param freqHz      Frecuencia central en Hz
            @param label       Etiqueta corta (ej: "250 Hz")
            @param severity    Intensidad 0.0-1.0
            @param isCritical  Si es crítico (rojo intenso)
            @param isPraise    Si es un acierto (verde)
            @param durationSecs Tiempo visible antes de desvanecerse */
        void addFrequencyMarker(float freqHz,
                                const juce::String& label,
                                float severity     = 0.5f,
                                bool isCritical    = false,
                                bool isPraise      = false,
                                float durationSecs = 10.0f);

        /** Limpia todos los marcadores de frecuencia. */
        void clearFrequencyMarkers();

    private:
        void pruneFrequencyMarkers();
        void drawFrequencyMarkers(juce::Graphics& g, juce::Rectangle<float> plot) const;

        std::vector<FrequencyMarker> frequencyMarkers_;
        mutable int64_t lastMarkerPruneMs_        = 0;
        static constexpr int kMaxFrequencyMarkers = 20;

        // ═══ Diagnostic bridge (owned by PluginProcessor, injected) ═══════════
        DiagnosticBridge* diagnosticBridge_ = nullptr;
        std::vector<BandDiagnostic> activeDiagnostics_;
    };

} // namespace mixcoach
