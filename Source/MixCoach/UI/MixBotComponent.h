#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "RobotAvatarComponent.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  MixBotState — 5 estados del robot sprite-based
//  Cada estado corresponde a un archivo PNG: robot_idle, robot_thinking,
//  robot_talking, robot_listening, robot_celebrating.
// ═══════════════════════════════════════════════════════════════════════════
enum class MixBotState : uint8_t
{
    Idle,        // robot_idle.png — parpadeo suave, respiración
    Thinking,    // robot_thinking.png — ojos entrecerrados, cabeza ladeada
    Talking,     // robot_talking.png — boca animada (movimiento)
    Listening,   // robot_listening.png — orejas atentas, mirada al usuario
    Celebrating  // robot_celebrating.png — brazos arriba, celebración
};

// ═══════════════════════════════════════════════════════════════════════════
//  MixBotComponent — Robot avatar con soporte dual:
//    1. Sprite-based: carga PNGs desde disco (Documents/MixCoach/Assets/)
//    2. Procedural fallback: delega a RobotAvatarComponent::drawAvatarStatic
//
//  Timer a ~60fps (~16ms) para animaciones suaves de sprite: parpadeo, inclinación de cabeza,
//  movimiento de boca. Es drop-in replacement de RobotAvatarComponent.
// ═══════════════════════════════════════════════════════════════════════════
class MixBotComponent : public juce::Component,
                            private juce::Timer
{
public:
    MixBotComponent();
    ~MixBotComponent() override = default;

    void paint(juce::Graphics& g) override;
    void timerCallback() override;
    /** Pausa/restaura el timer segun la visibilidad del componente. */
    void visibilityChanged() override;

    // ═══ RobotAvatarComponent API (drop-in replacement) ═══════════════════
    void setExpression(AvatarExpression newExpression);
    AvatarExpression getCurrentExpression() const noexcept { return currentExpression_; }
    float getExpressionIntensity() const noexcept { return expressionIntensity_; }
    void setExpressionWithDecay(AvatarExpression exp, int64_t decayMs = 3000);
    void setGaze(float gx, float gy) noexcept
    {
        gazeX_ = gx;
        gazeY_ = gy;
        repaint();
    }
    juce::Image renderToImage(int width, int height);

    // ═══ MixBot-specific API ═════════════════════════════════════════════
    void setMixBotState(MixBotState newState);
    MixBotState getMixBotState() const noexcept { return currentState_; }

    /** Retorna true si se cargaron sprites PNG exitosamente. */
    bool hasSprites() const noexcept { return hasSprites_; }

    // ═══ Wave (hand wave) animation ═════════════════════════════════════
    void setWaveActive(bool active, int64_t durationMs = 3000);
    bool isWaveActive() const noexcept { return waveActive_; }

    // ═══ Nod (head nod) animation for genre confirmation ═══════════════
    /** Dispara un cabeceo rápido (asentimiento) de ~500ms.
        En sprite mode: oscila headTiltAngle_ rápidamente.
        En procedural mode: se apoya en el cambio de expresión. */
    void triggerNod(int64_t durationMs = 500);
    bool isNodding() const noexcept { return nodActive_; }

    // ═══ Audio wave animation ════════════════════════════════════════════
    /** Activa ondas de audio animadas emanando de los audífonos.
        @param active  true para mostrar ondas, false para ocultarlas.
        @param level   0.0..1.0 intensidad de la onda. */
    void setAudioActive(bool active, float level = 0.5f) noexcept
    {
        audioActive_ = active;
        if (active) audioLevel_ = level;
        repaint();
    }
    bool isAudioActive() const noexcept { return audioActive_; }
    float getAudioLevel() const noexcept { return audioLevel_; }

private:
    // ═══ Sprite loading ═══════════════════════════════════════════════════
    bool loadSprites();
    juce::Image loadSprite(const juce::String& filename);

    // ═══ Sprite state ═════════════════════════════════════════════════════
    MixBotState currentState_{MixBotState::Idle};
    bool hasSprites_{false};

    // Imágenes de sprite por estado
    juce::Image idleSprite_;
    juce::Image thinkingSprite_;
    juce::Image talkingSprite_;
    juce::Image listeningSprite_;
    juce::Image celebratingSprite_;

    // ═══ Animation state (sprite mode — ~60fps) ════════════════════════
    int64_t lastAnimUpdate_{0};
    int64_t blinkTimer_{0};           // Cuándo parpadear (siguiente) o inicio del parpadeo actual
    float headTiltAngle_{0.0f};      // Inclinación de cabeza (-8..+8 grados)
    int blinkPhase_{0};              // 0=abierto, 1..4=cerrando, 5..8=abriendo
    bool isBlinking_{false};
    float mouthOpenAmount_{0.0f};     // 0.0..1.0 para Talking
    float floatOffset_{0.0f};         // Flotación suave Y
    float breathScale_{1.0f};         // Respiración 1.0..1.02

    // ═══ Wave animation state ═══════════════════════════════════════════
    bool waveActive_{false};
    int64_t waveStartMs_{0};
    int64_t waveDurationMs_{3000};
    float wavePhase_{0.0f};  // 0..1

    // ═══ Nod animation state ════════════════════════════════════════════
    bool nodActive_{false};
    int64_t nodStartMs_{0};
    int64_t nodDurationMs_{500};
    float nodPhase_{0.0f};   // 0..1 a lo largo del nod

    // ═══ Audio wave animation ════════════════════════════════════════════
    bool audioActive_{false};
    float audioLevel_{0.5f};

    // ═══ State transition (crossfade entre sprites) ═════════════════════
    juce::Image prevSprite_;
    int64_t stateTransitionStartMs_{0};
    float stateTransitionProgress_{1.0f};   // 0→1 durante transición

    // ═══ Procedural fallback state ═══════════════════════════════════════
    AvatarExpression currentExpression_{AvatarExpression::Neutral};
    int64_t expressionChangeMs_{0};
    int64_t expressionDecayMs_{0};
    float expressionIntensity_{1.0f};
    float gazeX_{0.0f};
    float gazeY_{0.0f};

    // ═══ Internal helpers ═════════════════════════════════════════════════
    void expressionTimerCallback();
    juce::Image getCurrentSprite() const;
    void drawSprite(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawProcedural(juce::Graphics& g, juce::Rectangle<float> bounds);
    void updateSpriteAnimations(int64_t now);
    void triggerBlink();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixBotComponent)
};

} // namespace mixcoach
