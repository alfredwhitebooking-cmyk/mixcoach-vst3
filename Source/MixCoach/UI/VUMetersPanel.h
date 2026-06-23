#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include "SmoothValue.h"
#include "VerticalGradientMeter.h"
#include "MixCoachTheme.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  VUMetersPanel — Contenedor con header "VU Meters"
    //  Muestra 4 barras verticales con gradiente (L, R, M, S) en grid 2×2
    //  Estilo limpio y moderno: barras con gradiente verde→amarillo→rojo,
    //  etiquetas claras y triángulos de peak hold.
    // ═══════════════════════════════════════════════════════════════════════════
    class VUMetersPanel : public juce::Component
    {
    public:
        VUMetersPanel();
        ~VUMetersPanel() override = default;

        void resized() override;
        void paint(juce::Graphics& g) override;

        void setLevel(int idx, float db);
        bool advanceMeters(double sampleRateHz = 60.0, bool allowRepaint = true);

    private:
        struct ChanMeter
        {
            SmoothValue level{-60.0f, 3.0f, 40.0f};
            float peakHold{-60.0f};
            int holdTimer{0};
        };

        juce::Label headerLabel_;
        std::array<ChanMeter, 4> meters_;
        juce::String labels_[4] = {"L", "R", "M", "S"};

        void drawMeter(juce::Graphics& g, juce::Rectangle<int> bounds, int idx, float levelDb, float peakDb);
    };

} // namespace mixcoach
