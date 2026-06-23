#pragma once
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "SmoothValue.h"

namespace mixcoach {

    // ===========================================================================
    //  VintageVUMeters — 4 VU meters analógicos PREMIUM en grid 2×2
    //  Estilo: Hardware físico real, panel de metal cepillado, diales empotrados.
    // ===========================================================================
    class VintageVUMeters : public juce::Component
    {
    public:
        VintageVUMeters();
        void resized() override;
        void paint(juce::Graphics& g) override;
        void setLevels(float l, float r, float m, float s);
        bool advanceVisuals(double sr = 60.0, bool allowRepaint = true);

    private:
        struct VUPoint
        {
            float db;
            float pct;
        };

        static constexpr VUPoint vuScale_[11] = {{-20.0f, 0.00f},
                                                 {-10.0f, 0.25f},
                                                 {-7.0f, 0.35f},
                                                 {-5.0f, 0.45f},
                                                 {-3.0f, 0.58f},
                                                 {-2.0f, 0.65f},
                                                 {-1.0f, 0.72f},
                                                 {0.0f, 0.82f},
                                                 {1.0f, 0.88f},
                                                 {2.0f, 0.94f},
                                                 {3.0f, 1.00f}};

        static constexpr float kAngleLeft  = -2.35619f; // -135° (-0.75 * π)
        static constexpr float kAngleRight = -0.78540f; // -45° (-0.25 * π)
        static constexpr float kAngleRange = 1.57080f;  // 90° (0.5 * π)

        static constexpr const char* kLabels_[4] = {"L", "R", "M", "S"};

        static constexpr float kVuIntegrationMs = 300.0f;
        static constexpr float kVuRefDbfs       = -18.0f;
        static constexpr float kVuPeakLedVU     = 0.0f;

        struct VUChannel
        {
            SmoothValue level{-20.0f, kVuIntegrationMs, kVuIntegrationMs};
            float peakHold      = -20.0f;
            float peakHoldTimer = 0.0f;
        };

        VUChannel channels_[4];

        static constexpr float kPeakHoldSec    = 1.5f;
        static constexpr float kPeakDecay_dBps = 30.0f;

        juce::Image dialCache_;
        juce::Image panelCache_;
        bool cacheValid_ = false;
        bool panelValid_ = false;

        float scaleArcCx_ = 0.0f, scaleArcCy_ = 0.0f, scaleArcR_ = 0.0f;

        float vuDbToNorm(float db) const noexcept;

        struct FaceGeom
        {
            juce::Rectangle<float> face;
            juce::Rectangle<float> housing;
            float cx = 0.0f, cy = 0.0f, radius = 0.0f;
        };

        [[nodiscard]] FaceGeom computeGeom(juce::Rectangle<float> cell) const noexcept;
        void computeCells(juce::Rectangle<int> bounds, juce::Rectangle<float> (&cells)[4]) const noexcept;

        void rebuildPanelCache(juce::Rectangle<int> bounds);
        void drawBrushedPanel(juce::Graphics& g, juce::Rectangle<float> bounds);
        void drawBevelFrame(juce::Graphics& g, juce::Rectangle<float> cell);
        void drawHub(juce::Graphics& g, float cx, float cy, float faceH);
        void paintMeterStatic(juce::Graphics& g, juce::Rectangle<float> cell, int idx);
        void drawStaticDialFace(juce::Graphics& g, juce::Rectangle<float> bounds, float cx, float cy, float radius);
        void drawNeedle(juce::Graphics& g, float cx, float cy, float radius, float level, float peakHold);
    };

} // namespace mixcoach
