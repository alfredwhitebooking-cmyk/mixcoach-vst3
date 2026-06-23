#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <cmath>
#include "SmoothValue.h"
#include "MixCoachTheme.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  AnalogVUMeter — VU Meter analógico vintage con aguja
    //
    //  Estilo: fondo madera/beige envejecido, marco oscuro tipo bisel, escala
    //  impresa -20..+3 VU con zona roja, aguja oscura, label "VU" central.
    //  Ideal para la Sesión 4 del panel Analyzers (grid 2×2: L, R, M, S).
    //
    //  Entrada: dBFS → convertido internamente a VU (0 VU ≈ -18 dBFS).
    //  SmoothValue con ballistics VU estándar (attack 300ms / release 300ms).
    // ═══════════════════════════════════════════════════════════════════════════
    class AnalogVUMeter : public juce::Component
    {
    public:
        AnalogVUMeter();
        ~AnalogVUMeter() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;

        // ─── Recibe nivel en dBFS, lo convierte a VU internamente ────────────
        void setLevel(float levelDbFs);
        bool advanceFrame(double sampleRateHz = 60.0, bool allowRepaint = true);

        void setLabel(const juce::String& label)
        {
            labelText_      = label;
            faceCacheValid_ = false;
            repaint();
        }

        void setMeterColour(juce::Colour c)
        {
            labelColourOverride_ = c;
            faceCacheValid_      = false;
            repaint();
        }

    private:
        // ─── Conversión dBFS → VU ─────────────────────────────────────
        // 0 VU ≈ -18 dBFS (estándar profesional)
        static constexpr float kDbFsRef = -18.0f; // dBFS que corresponde a 0 VU
        static constexpr float kVuMin   = -20.0f;
        static constexpr float kVuMax   = 3.0f;

        // ─── Geometría del arco (en JUCE coords: 0°=right, 90°=down) ──
        // El arco va desde ~215° (izquierda, -20 VU) a ~325° (derecha, +3 VU)
        // pasando por 270° (arriba, 0 VU) → ~110° total
        static constexpr float kArcStartAngle = juce::MathConstants<float>::pi * 1.194f; // ~215°
        static constexpr float kArcEndAngle   = juce::MathConstants<float>::pi * 1.806f; // ~325°
        static constexpr float kArcRange      = kArcEndAngle - kArcStartAngle;           // ~110°
        // Constantes de geometría compartidas entre draw*() y paintNeedle()
        static constexpr float kPivotOffsetY      = 10.0f; // pívote sube un poco del borde
        static constexpr float kScaleRadiusFactor = 0.82f; // fraccion de (h - kPivotOffsetY)

        // ─── Suavizado ────────────────────────────────────────────────
        SmoothValue smoothedVu_{kVuMin, 6.0f, 40.0f};
        SmoothValue peakHoldVu_{kVuMin, 1.0, 1.0}; // instant ballistics — decay handled manually
        float peakHoldLevel_{kVuMin};
        uint32_t peakHoldTimeMs_{0};

        // ─── Configuración ────────────────────────────────────────────
        juce::String labelText_;
        juce::Colour labelColourOverride_{0x00000000}; // transparent = default textDim

        // ─── Métodos de dibujo ────────────────────────────────────────
        [[nodiscard]] float dbFsToVu(float dbFs) const;
        [[nodiscard]] float valueToAngle(float vu) const;
        void drawBezel(juce::Graphics& g, juce::Rectangle<float> faceBounds) const;
        void drawWoodBackground(juce::Graphics& g, juce::Rectangle<float> faceBounds) const;
        void drawScale(juce::Graphics& g, juce::Rectangle<float> faceBounds) const;
        void drawNeedle(juce::Graphics& g, juce::Rectangle<float> faceBounds, float angle) const;
        void drawVuLabel(juce::Graphics& g, juce::Rectangle<float> faceBounds) const;
        void drawChannelLabel(juce::Graphics& g, juce::Rectangle<float> faceBounds) const;

        void rebuildFaceCache();
        void paintStaticFace(juce::Graphics& g, juce::Rectangle<float> bounds) const;

        juce::Image faceCache_;
        bool faceCacheValid_ = false;
    };

} // namespace mixcoach
