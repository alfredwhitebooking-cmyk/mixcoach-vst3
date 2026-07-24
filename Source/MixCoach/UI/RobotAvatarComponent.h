#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  AvatarExpression — Estado emocional del robot avatar
// ═══════════════════════════════════════════════════════════════════════════
enum class AvatarExpression : uint8_t
{
    Neutral,      // Expresión por defecto — mirada calmada
    Happy,        // Sonrisa cálida — feedback positivo del coach
    Surprised,    // Sorpresa — descubrimiento o cambio inesperado
    Thinking,     // Pensando — coach analizando o procesando
    Encouraging,  // Animando — motivación para el usuario
    Serious,      // Serio — issues críticos o advertencias
    Celebrating   // Celebración — logro completado, brazos arriba
};

inline const char* avatarExpressionLabel(AvatarExpression exp) noexcept
{
    switch (exp) {
        case AvatarExpression::Neutral:     return "NEUTRAL";
        case AvatarExpression::Happy:       return "HAPPY";
        case AvatarExpression::Surprised:   return "SURPRISED";
        case AvatarExpression::Thinking:    return "THINKING";
        case AvatarExpression::Encouraging: return "ENCOURAGING";
        case AvatarExpression::Serious:     return "SERIOUS";
        case AvatarExpression::Celebrating: return "CELEBRATING";
        default:                            return "UNKNOWN";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  RobotAvatarComponent — Avatar robótico procedural con expresiones
// ═══════════════════════════════════════════════════════════════════════════
class RobotAvatarComponent : public juce::Component,
                                  private juce::Timer
{
public:
    RobotAvatarComponent();
    ~RobotAvatarComponent() override = default;

    void paint(juce::Graphics& g) override;
    void timerCallback() override;
    /** Pausa/restaura el timer segun la visibilidad del componente. */
    void visibilityChanged() override;

    /** Cambia la expresión del avatar y force-repaint. */
    void setExpression(AvatarExpression newExpression);        /** Retorna la expresión actual. */
        AvatarExpression getCurrentExpression() const noexcept { return currentExpression_; }

        /** Retorna la intensidad actual de la expresión (0→1, para transiciones suaves). */
        float getExpressionIntensity() const noexcept { return expressionIntensity_; }

    /** Anima la expresión de vuelta a Neutral tras un tiempo. */
    void setExpressionWithDecay(AvatarExpression exp, int64_t decayMs = 3000);

    /** Activa la animación de saludo con la mano.
        El brazo se dibuja y anima automáticamente durante waveDurationMs.
        @param active  true para iniciar el wave, false para cancelarlo.
        @param durationMs  Duración del wave en ms (default 3000). */
    void setWaveActive(bool active, int64_t durationMs = 3000);

    /** Retorna true si la animación de wave está activa. */
    bool isWaveActive() const noexcept { return waveActive_; }

    /** Activa una animación de asentimiento (cabeceo) por durationMs.
        El avatar inclina la cabeza hacia abajo y arriba una vez. */
    void triggerNod(int64_t durationMs = 400);

    /** Cambia la dirección de la mirada: (-1,-1)=arriba-izq, (1,1)=abajo-der, (0,0)=centro. */
    void setGaze(float gx, float gy) noexcept
    {
        gazeX_ = gx;
        gazeY_ = gy;
        repaint();
    }

    /** Renderiza el avatar en un Image ARGB para usar donde se necesita
        una imagen estática (ej: ImageComponent). */
    juce::Image renderToImage(int width, int height);

    // ═══ LED animation states ═════════════════════════════════════════
    /** Activa/desactiva el modo "pensando" con LEDs parpadeantes. */
    void setThinkActive(bool active) noexcept { thinkActive_ = active; repaint(); }
    bool isThinkActive() const noexcept { return thinkActive_; }

    /** Activa/desactiva el modo "escuchando" con LEDs encendidos fijos. */
    void setListenActive(bool active) noexcept { listenActive_ = active; repaint(); }
    bool isListenActive() const noexcept { return listenActive_; }

    /** Activa la animación de boca (LED grid) para simular que el robot habla.
        talkPhase se actualiza automáticamente en el timer. */
    void setTalkActive(bool active) noexcept { talkActive_ = active; if (!active) repaint(); }
    bool isTalkActive() const noexcept { return talkActive_; }

    // ═══ Audio wave animation ════════════════════════════════════════════
    /** Activa ondas de audio animadas emanando de los audífonos.
        @param active  true para mostrar ondas, false para ocultarlas.
        @param level   0.0..1.0 intensidad de la onda (simula volumen). */
    void setAudioActive(bool active, float level = 0.5f) noexcept
    {
        audioActive_ = active;
        if (active) audioLevel_ = level;
        repaint();
    }
    bool isAudioActive() const noexcept { return audioActive_; }
    float getAudioLevel() const noexcept { return audioLevel_; }

    // ═══ Drawing methods (public para reutilización en ChatMessagesComponent) ═══
    static void drawAvatarStatic(juce::Graphics& g,
                                 juce::Rectangle<float> bounds,
                                 AvatarExpression expression,
                                 float scale = 1.0f,
                                 float glowIntensity = 1.0f,
                                 float gazeX = 0.0f,
                                 float gazeY = 0.0f,
                                 float wavePhase = 0.0f,
                                 float nodPhase = 0.0f,
                                 bool thinkActive = false,
                                 bool listenActive = false,
                                 bool audioActive = false,
                                 float audioLevel = 0.5f,
                                 float talkPhase = 0.0f);

private:
    void drawBackground(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawHeadphones(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawNeckAndBody(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawHead(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawVisor(juce::Graphics& g, juce::Rectangle<float> bounds);        static void drawEyes(juce::Graphics& g, juce::Rectangle<float> bounds, AvatarExpression expression, float gazeX = 0.0f, float gazeY = 0.0f);
    static void drawMouth(juce::Graphics& g, juce::Rectangle<float> bounds, AvatarExpression expression, float talkPhase = 0.0f);
    void drawChestLogo(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawShoulders(juce::Graphics& g, juce::Rectangle<float> bounds);
    static void drawWavingArm(juce::Graphics& g,
                              juce::Rectangle<float> bounds,
                              float s,
                              float cx,
                              float cy,
                              float wavePhase);

    // ═══ Expression state ═══════════════════════════════════════════════════
    AvatarExpression currentExpression_{AvatarExpression::Neutral};
    int64_t expressionChangeMs_{0};       // Cuando cambió la expresión
    int64_t expressionDecayMs_{0};        // Ms hasta volver a Neutral (0 = no decay)
    float expressionIntensity_{1.0f};     // Interpolación entre expresiones (0→1)

    // ═══ Wave animation state ════════════════════════════════════════════
    bool waveActive_{false};
    int64_t waveStartMs_{0};
    int64_t waveDurationMs_{3000};
    float wavePhase_{0.0f};  // 0..1 a lo largo del ciclo de wave

    // ═══ Nod animation state ═════════════════════════════════════════════
    bool nodActive_{false};
    int64_t nodStartMs_{0};
    int64_t nodDurationMs_{400};
    float nodPhase_{0.0f};   // 0..1 a lo largo del ciclo de nod

    // ═══ LED animation state ════════════════════════════════════════════
    bool thinkActive_{false};
    bool listenActive_{false};

    // ═══ Audio wave animation state ═════════════════════════════════════
    bool audioActive_{false};
    float audioLevel_{0.5f};

    // ═══ Blink animation ═══════════════════════════════════════════════
    int blinkCounter_{0};          // Frames desde último parpadeo
    bool blinkActive_{false};      // True durante los frames de parpadeo
    int blinkHoldFrames_{0};       // Cuántos frames lleva el parpadeo activo
    static constexpr int kBlinkInterval = 120; // ~4s at 30Hz entre parpadeos
    static constexpr int kBlinkDuration = 4;   // ~130ms duración del parpadeo

    // ═══ Talk animation ════════════════════════════════════════════════
    bool talkActive_{false};       // Activa animación de boca (LED grid parpadea)
    float talkPhase_{0.0f};        // 0..2π ciclo de habla

    // ═══ Gaze direction (eye movement) ════════════════════════════════════
    // Normalizado: (-1,-1)=arriba-izq, (0,0)=centro, (1,1)=abajo-der
    float gazeX_ = 0.0f;
    float gazeY_ = 0.0f;

    // ═══ Timer para decay de expresión ═════════════════════════════════════
    void expressionTimerCallback();

    // ═══ Colores del tema ══════════════════════════════════════════════════
    static juce::Colour purpleDark()    { return juce::Colour(0xFF2D1B4E); }
    static juce::Colour purpleMid()     { return juce::Colour(0xFF6B3DFF); }
    static juce::Colour purpleLight()   { return juce::Colour(0xFF9D6FFF); }
    static juce::Colour purpleGlow()    { return juce::Colour(0xFFC8A8FF); }
    static juce::Colour silverMetallic(){ return juce::Colour(0xFFE8ECF0); }
    static juce::Colour silverLight()   { return juce::Colour(0xFFF5F7FA); }
    static juce::Colour silverShadow()  { return juce::Colour(0xFFB0B8C4); }
    static juce::Colour visorBlack()    { return juce::Colour(0xFF050505); }
    static juce::Colour purpleEye()     { return juce::Colour(0xFFA855F7); }
    static juce::Colour purpleEyeGlow() { return juce::Colour(0xFF7C3AED); }
    static juce::Colour headphoneBlack(){ return juce::Colour(0xFF1A1A1A); }
    static juce::Colour headphoneMatte(){ return juce::Colour(0xFF2A2A2A); }
    static juce::Colour bodyWhite()     { return juce::Colour(0xFFF0F2F5); }
    static juce::Colour bodyShadow()    { return juce::Colour(0xFFD0D4DC); }
    static juce::Colour jointBlack()    { return juce::Colour(0xFF222222); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RobotAvatarComponent)
};

} // namespace mixcoach
