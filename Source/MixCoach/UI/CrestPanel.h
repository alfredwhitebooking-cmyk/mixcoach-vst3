#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "MixCoachTheme.h"
#include "SmoothValue.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  CrestPanel — Crest Factor analog gauge + metrics table
    //
    //  Diseño inspirado en TT Dynamic Range Meter + Insight:
    //  - Gauge semicircular 180° con 4 zonas de color (verde→rojo)
    //  - Aguja metálica gris cepillado con efecto 3D
    //  - Lectura digital prominente debajo del medidor
    //  - Tabla de 3 filas: PEAK | RMS | CREST
    // ═══════════════════════════════════════════════════════════════════════════
    class CrestPanel : public juce::Component
    {
    public:
        CrestPanel();
        ~CrestPanel() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;

        void setValues(float peak, float rms);
        bool advanceVisuals(double sampleRateHz = 60.0, bool allowRepaint = true);

        /** Establece un target de crest para mostrar como marca en el gauge.
            Dibuja un arco coloreado + tick en la posición del target.
            @param targetDb  Valor de crest objetivo (dB)
            @param label     Etiqueta opcional (ej: "Target: 10dB") */
        void setTargetCrest(float targetDb, const juce::String& label = {});

        /** Limpia la marca de target del gauge. */
        void clearTargetCrest();

        [[nodiscard]] bool hasTargetCrest() const noexcept { return hasTargetCrest_; }

    private:
        SmoothValue peak_{-80.0f, 1.0f, 100.0f};
        SmoothValue rms_{-80.0f, 5.0f, 250.0f};
        SmoothValue crest_{0.0f, 3.0f, 150.0f};

        float rawPeak_ = -80.0f;
        float rawRms_  = -80.0f;

        // Target crest overlay state
        bool hasTargetCrest_ = false;
        float targetCrestDb_ = 8.0f;
        juce::String targetCrestLabel_;

        static constexpr float kMaxCrest   = 30.0f;
        static constexpr float kGaugeStart = 3.14159f; // 180° (left)
        static constexpr float kGaugeEnd   = 6.28319f; // 360° (right) = 180° arc
        static constexpr float kGaugeRange = kGaugeEnd - kGaugeStart;

        void drawGauge(juce::Graphics& g, juce::Rectangle<float> bounds);
        void drawScale(juce::Graphics& g, float cx, float cy, float radius);
        void drawTargetMarker(juce::Graphics& g, float cx, float cy, float radius);
        void drawNeedle(juce::Graphics& g, float cx, float cy, float radius, float crestNorm);
        void drawDigitalReading(juce::Graphics& g, juce::Rectangle<float> bounds, float crest);
        void drawMetricsTable(juce::Graphics& g, juce::Rectangle<float> bounds);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CrestPanel)
    };

} // namespace mixcoach
