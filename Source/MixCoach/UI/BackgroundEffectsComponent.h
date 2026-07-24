#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <cstdlib>
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  BackgroundEffectsComponent — Partículas decorativas + orbes glow
    //
    //  Renderiza un fondo animado con:
    //    • 25 partículas flotantes (dots 2-4px) con colores púrpura/cian/verde
    //    • 3 orbes glow decorativos con movimiento Lissajous lento
    //
    //  Se coloca como el hijo MÁS INFERIOR de NavigationShell para que
    //  todo el contenido UI se renderice encima sin interferir.
    //
    //  Uso:
    //    addChildComponent(backgroundEffects_.get());  // primero, antes de todo
    //    backgroundEffects_.get()->setAlwaysOnTop(false);
    //    setInterceptsMouseClicks(false, false);
    //
    //  NOTA: No usa SmoothValue porque es puramente decorativo — la
    //        imprecisión del float sumado directamente es aceptable y
    //        más eficiente para 25+ elementos a 30fps.
    // ═══════════════════════════════════════════════════════════════════════════
    class BackgroundEffectsComponent : public juce::Component,
                                        private juce::Timer
    {
    public:
        BackgroundEffectsComponent();
        ~BackgroundEffectsComponent() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void timerCallback() override;
    /** Pausa/restaura el timer segun la visibilidad del componente. */
    void visibilityChanged() override;

        /** Dispara una explosión de confeti (celebración).
        Genera ~60 partículas coloridas que caen desde arriba con
        gravedad, deriva horizontal y rotación. Se desvanecen en ~3s.
        @param count  Número de partículas (default 60). */
        void triggerConfetti(int count = 60);

        /** Retorna true mientras haya confeti activo. */
        bool hasActiveConfetti() const noexcept
        {
            for (const auto& c : confetti_)
                if (c.alpha > 0.0f) return true;
            return false;
        }

    private:
        // ═══ Floating particle ═══════════════════════════════════════════════
        struct Particle
        {
            float x = 0.0f, y = 0.0f;
            float phaseX = 0.0f, phaseY = 0.0f;  // Sine wave phase offsets
            float speedX = 0.0f, speedY = 0.0f;   // Frequency multipliers
            float ampX = 0.0f, ampY = 0.0f;        // Amplitude of sine drift
            float radius = 2.0f;
            float alpha = 0.08f;
            juce::Colour colour;
        };

        // ═══ Glow orb decorativo ════════════════════════════════════════════
        struct GlowOrb
        {
            float x = 0.0f, y = 0.0f;
            float baseX = 0.0f, baseY = 0.0f;     // Center of orbit
            float radius = 100.0f;                  // Glow circle radius
            float orbitRadiusX = 0.0f, orbitRadiusY = 0.0f; // Orbit size
            float phaseX = 0.0f, phaseY = 0.0f;     // Phase offsets
            float speedX = 0.0f, speedY = 0.0f;     // Angular velocity
            float alpha = 0.06f;
            juce::Colour colour;
        };

        std::vector<Particle> particles_;
        std::vector<GlowOrb> orbs_;
        float elapsed_ = 0.0f;  // Global time in seconds

        /** Inicializa las 25 partículas con posiciones y colores aleatorios. */
        void initParticles();

        /** Inicializa los 3 orbes glow con posiciones y colores fijos.
            Cada orb tiene una órbita Lissajous diferente. */
        void initOrbs();

        // ═══ Noise texture overlay ═══════════════════════════════════════════
        juce::Image noiseImage_;
        int noiseFrameCounter_ = 0;
        static constexpr int kNoiseRegenInterval = 60; // regenerar cada 60 frames (~2s)

        /** Regenera la textura de ruido procedimental. */
        void regenerateNoise(int width, int height);

        // ═══ Confetti particle (celebración) ═══════════════════════════════
        struct ConfettiParticle
        {
            float x = 0.0f, y = 0.0f;        // Position (pixels)
            float vx = 0.0f, vy = 0.0f;        // Velocity (px/s)
            float rotation = 0.0f;              // Current rotation angle
            float rotSpeed = 0.0f;              // Rotation speed (rad/s)
            float w = 6.0f, h = 4.0f;           // Width & height (px)
            float alpha = 1.0f;                 // Current opacity (0-1)
            float lifetime = 0.0f;               // Seconds elapsed
            float maxLifetime = 3.0f;            // Total lifetime before fade
            juce::Colour colour;
        };
        std::vector<ConfettiParticle> confetti_;

        /** Actualiza posiciones y alpha del confeti (llamado desde timerCallback). */
        void updateConfetti(float dt);

        /** Renderiza el confeti activo (llamado desde paint). */
        void drawConfetti(juce::Graphics& g);

        /** Retorna un color de celebración. */
        static juce::Colour randomParticleColour()
        {
            static const juce::Colour palette[] = {
                juce::Colour(0xFFA855F7),  // púrpura
                juce::Colour(0xFF06B6D4),  // cian
                juce::Colour(0xFF22D3A7),  // verde
                juce::Colour(0xFF8B5CF6),  // violeta
                juce::Colour(0xFF2DD4BF),  // teal
            };
            return palette[std::rand() % 5];
        }

        /** Retorna un color de la paleta de celebración (más brillante). */
        static juce::Colour randomConfettiColour()
        {
            static const juce::Colour palette[] = {
                juce::Colour(0xFFFBBF24),  // dorado
                juce::Colour(0xFFA855F7),  // púrpura
                juce::Colour(0xFF06B6D4),  // cian
                juce::Colour(0xFFEC4899),  // rosa
                juce::Colour(0xFF22D3A7),  // verde
                juce::Colour(0xFF8B5CF6),  // violeta
                juce::Colour(0xFFF97316),  // naranja
            };
            return palette[std::rand() % 7];
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackgroundEffectsComponent)
    };

} // namespace mixcoach
