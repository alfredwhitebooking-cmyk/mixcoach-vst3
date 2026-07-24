#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "SmoothValue.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  ScoreRingComponent — Anillo de progreso animado estilo SVG
    //
    //  Muestra un score 0-100% como un arco circular con:
    //    • Fondo: arco gris oscuro completo (270°)
    //    • Relleno: arco degradado púrpura → verde que se llena animadamente
    //    • Centro: número grande del score + etiqueta "/100"
    //    • Animación: SmoothValue desde 0 hasta el score target
    //    • Brillo pulsante al completar la animación (score ≥ 85)
    //
    //  Uso:
    //    scoreRing.setScore(92.0f);
    //    scoreRing.startAnimation(); // anima 0 → 92 en ~600ms
    // ═══════════════════════════════════════════════════════════════════════════
    class ScoreRingComponent : public juce::Component,
                                private juce::Timer
    {
    public:
        ScoreRingComponent();
        ~ScoreRingComponent() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void timerCallback() override;
    /** Pausa/restaura el timer segun la visibilidad del componente. */
    void visibilityChanged() override;

        /** Fija el score target (0-100) y comienza la animación. */
        void setScore(float score);

        /** Comienza la animación desde 0 hasta el score target. */
        void startAnimation();

        /** Fija el score instantáneamente (sin animación). */
        void setScoreImmediate(float score);

        /** Retorna el score animado actual. */
        float getAnimatedScore() const noexcept { return animatedScore_.getCurrent(); }

        /** Retorna el score target. */
        float getTargetScore() const noexcept { return targetScore_; }

        /** Retorna true si la animación está en progreso. */
        bool isAnimating() const noexcept { return animating_; }

        /** Callback cuando la animación de entrada se completa. */
        std::function<void()> onAnimationComplete;

        /** Tamaño recomendado del componente. */
        static constexpr int kDefaultSize = 140;

    private:
        float targetScore_ = 0.0f;
        SmoothValue animatedScore_{0.0f, 30.0f, 80.0f}; // Attack rápido, release suave
        bool animating_ = false;
        int64_t animStartMs_ = 0;

        // ═══ Constantes de geometría del arco ═══════════════════════════════
        // El arco va de 135° a 405° (225° → 675° en el sistema de JUCE)
        // = 270° de apertura (3/4 de círculo)
        static constexpr float kStartDeg = 135.0f;
        static constexpr float kEndDeg = 405.0f;
        static constexpr float kArcRangeDeg = 270.0f;
        static constexpr float kStrokeWidth = 10.0f;

        /** Convierte grados a radianes en el sistema de JUCE (0=right, CCW). */
        static float degToRadians(float deg) noexcept
        {
            return juce::degreesToRadians(deg - 90.0f);
        }

        /** Interpola color entre púrpura (0%) y verde (100%). */
        static juce::Colour scoreGradient(float pct) noexcept
        {
            // 0% → púrpura (#A855F7), 50% → cian (#06B6D4), 100% → verde (#22D3A7)
            if (pct < 0.5f) {
                float t = pct / 0.5f; // 0→1
                return MixCoachTheme::accent().interpolatedWith(
                    juce::Colour(0xFF06B6D4), t);
            } else {
                float t = (pct - 0.5f) / 0.5f; // 0→1
                return juce::Colour(0xFF06B6D4).interpolatedWith(
                    juce::Colour(0xFF22D3A7), t);
            }
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScoreRingComponent)
    };

} // namespace mixcoach
