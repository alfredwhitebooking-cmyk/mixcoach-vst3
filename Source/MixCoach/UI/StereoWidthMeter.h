#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "SmoothValue.h"
#include "MixCoachTheme.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  StereoWidthMeter — Medidor de ancho estéreo claro e intuitivo
    //
    //  ¿Qué es el ancho estéreo? Es qué tan separado suena el canal izquierdo
    //  del derecho. 0% = mismo sonido en ambos (mono), 100% = completamente
    //  diferentes (ultra-wide).
    //
    //  Diseñado para que el usuario ENTIENDA de un vistazo:
    //    - Barra horizontal con 4 zonas de color + marker animado
    //    - Header con emoji + nombre + badge de porcentaje
    //    - Tip contextual que explica el valor en palabras simples
    //    - Per-band breakdown opcional (6 frecuencias)
    // ═══════════════════════════════════════════════════════════════════════════
    class StereoWidthMeter : public juce::Component
    {
    public:
        StereoWidthMeter();
        ~StereoWidthMeter() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;

        /** Setea el valor de ancho estéreo promedio (0.0 = mono, 1.0 = máximo ancho). */
        void setAvgWidth(float width);

        /** Setea los valores por banda (6 bandas). */
        void setPerBandWidth(const float* perBand);

        [[nodiscard]] float getAvgWidth() const noexcept { return widthSmooth_.getCurrent(); }

        bool advanceVisuals(double sr = 60.0, bool allowRepaint = true);

        // ─── 6 bandas de frecuencia ──────────────────────────────────────────
        static constexpr int kNumBands                      = 6;
        static constexpr const char* kBandLabels[kNumBands] = {"SUB", "BAJ", "MED", "AGD", "BRL", "AIR"};

    private:
        // ─── Zonas de ancho estéreo (de 0% a 100%) ───────────────────────────
        // Cada zona tiene: rango, nombre claro, color, y emoji representativo
        struct WidthZone
        {
            float minNorm;
            float maxNorm;
            const char* label;
            const char* icon; // Emoji que representa la zona
            const char* tip;  // Explicación en palabras simples
            juce::Colour colour;
        };

        static constexpr int kNumZones = 4;
        WidthZone zones_[kNumZones]    = {
            {0.00f,
             0.15f,
             "MUY JUNTO",
             "\xF0\x9F\x9F\xA5", // 🔴
             "Suena casi en mono: revisa fase y pans",
             juce::Colour(0xFF6B7280)}, // Gris
            {0.15f,
             0.40f,
             "NATURAL",
             "\xF0\x9F\x9F\xA2", // 🟢
             "Ancho equilibrado: suena limpio en mono y est\xC3\xA9reo",
             juce::Colour(0xFF22C55E)}, // Verde
            {0.40f,
             0.70f,
             "AMPLIO",
             "\xF0\x9F\x94\xB5", // 🔵
             "Sonido envolvente: ideal para pads, FX y atmosferas",
             juce::Colour(0xFF3B82F6)}, // Azul
            {0.70f,
             1.00f,
             "EXCESO",
             "\xF0\x9F\x9F\xA0", // 🟠
             "Demasiado ancho: riesgo de cancelaci\xC3\xB3n de fase en mono",
             juce::Colour(0xFFF97316)}, // Naranja
        };

        static constexpr float kWarningTooWide = 0.70f;
        static constexpr float kWarningNarrow  = 0.15f;

        // ─── Estado ─────────────────────────────────────────────────────────
        SmoothValue widthSmooth_{0.0f, 3.0f, 100.0f};
        float rawWidth_ = 0.0f;

        // Per-band data (opcional)
        float perBandValues_[kNumBands] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        bool hasPerBandData_            = false;

        // Layout se calcula dinámicamente en paint()

        // ─── Drawing helpers ─────────────────────────────────────────────────
        void drawHeader(juce::Graphics& g, juce::Rectangle<float> area);
        void drawBar(juce::Graphics& g, juce::Rectangle<float> area);
        void drawTip(juce::Graphics& g, juce::Rectangle<float> area);
        void drawPerBand(juce::Graphics& g, juce::Rectangle<float> area);

        [[nodiscard]] const WidthZone& getZone(float width) const noexcept;
        [[nodiscard]] const char* getContextTip(float width) const noexcept;
    };

} // namespace mixcoach
