#pragma once
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "SmoothValue.h"
#include "../audio/AudioAnalyzer.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  MeterPanel — Mastering Metering Suite profesional
    //
    //  Layout de 3 columnas:
    //    LEFT:   Stereo Peak Meter — 2 barras verticales (L, R) con escala dB
    //    CENTER: 4 módulos estadísticos apilados (PEAK, RMS, LUFS(I), DR)
    //    RIGHT:  LUFS Meter Stereo — 2 barras verticales (S, M) con escala LUFS
    // ═══════════════════════════════════════════════════════════════════════════
    class MeterPanel : public juce::Component
    {
    public:
        MeterPanel();
        void resized() override;
        void paint(juce::Graphics& g) override;
        void updateData(const AudioAnalyzer& analyzer);

        /** Avanza SmoothValues (llamar a 60fps). */
        bool advanceVisuals(double sr = 60.0);

    private:
        // ─── 3 columnas ──────────────────────────────────────────────────────
        void drawPeakMeterStereo(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawStatsPanel(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawLufsMeterStereo(juce::Graphics& g, juce::Rectangle<int> bounds);

        // ─── Sub-elementos ───────────────────────────────────────────────────
        void drawPeakBar(juce::Graphics& g, juce::Rectangle<float> bounds, float level, float peakHold);
        void drawLufsBar(juce::Graphics& g, juce::Rectangle<float> bounds, float level, float targetLUFS);
        void drawStatModule(juce::Graphics& g,
                            juce::Rectangle<int> bounds,
                            const juce::String& title,
                            float value,
                            const juce::String& unit,
                            juce::Colour accent,
                            bool highlight = false);
        void drawDbScale(juce::Graphics& g,
                         juce::Rectangle<float> bounds,
                         float minDb,
                         float maxDb,
                         float stepDb,
                         float highlightVal = -999.0f,
                         bool isLufsScale   = false);

        // ─── Smoothed values ─────────────────────────────────────────────────
        SmoothValue leftPeak_{-80.0f, 1.0f, 80.0f};
        SmoothValue rightPeak_{-80.0f, 1.0f, 80.0f};
        SmoothValue leftRms_{-80.0f, 5.0f, 150.0f};
        SmoothValue rightRms_{-80.0f, 5.0f, 150.0f};
        SmoothValue momentaryLUFS_{-70.0f, 3.0f, 120.0f};
        SmoothValue shortTermLUFS_{-70.0f, 3.0f, 120.0f};
        SmoothValue integratedLUFS_{-70.0f, 5.0f, 150.0f};

        // ─── Raw values para display digital (instantáneos) ──────────────────
        float rawPeak_ = -80.0f, rawRms_ = -80.0f;
        float rawLufs_ = -80.0f, rawDr_ = 0.0f;
        float rawLeftPeak_ = -80.0f, rawRightPeak_ = -80.0f;

        // ─── Peak hold ──────────────────────────────────────────────────────
        float leftPeakHold_ = -80.0f, rightPeakHold_ = -80.0f;
        float leftPeakHoldTimer_ = 0.0f, rightPeakHoldTimer_ = 0.0f;

        // ─── Target LUFS ─────────────────────────────────────────────────────
        float lufsTarget_ = -14.0f;
    };

} // namespace mixcoach
