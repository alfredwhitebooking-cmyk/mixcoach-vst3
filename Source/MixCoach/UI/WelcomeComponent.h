#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "MixBotComponent.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  WelcomeComponent — Pantalla de bienvenida pixel-perfect
//
//  Diseño exacto según docs/UI_WELCOME_SCREEN_SPEC.md:
//    Fondo: Gradiente #06060B → #090812 → #130E21 + glow radial + noise 2%
//    Header: MIXCOACH 28px bold + v1.0.0 top-left
//    Avatar: Círculo 300x300 con RobotAvatarComponent + glow #8B5CF6
//    Título: "Bienvenido a MixCoach" 56px, "MixCoach" en #A855F7
//    Subtítulo: "Tu mentor de mezcla impulsado por IA." 28px, "mentor" púrpura
//    Label: "¿CÓMO TE LLAMAS?" 13px uppercase tracking 0.30em
//    Input: 740x74, fondo #1A1826, border 2px #7C3AED, radius 14px
//    Botón: COMENZAR 430x74, gradiente #A855F7→#7C3AED, glow + flecha →
//    Footer: ícono escudo + texto privacidad 16px
//    Animación: staggered 800ms, robot floating/breathing/halo
// ═══════════════════════════════════════════════════════════════════════════
class WelcomeComponent : public juce::Component,
                         private juce::Timer,
                         private juce::TextEditor::Listener
{
public:
    WelcomeComponent();
    ~WelcomeComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Inicia la animación de aparición escalonada. */
    void startWelcomeAnimation();

    /** Callback cuando el usuario completa el ingreso de nombre y presiona COMENZAR. */
    std::function<void(const juce::String& userName)> onStart;

private:
    // ═══ Staggered Animation Phases ═══════════════════════════════════════
    enum class AnimPhase : uint8_t {
        Inactive,       // Sin iniciar
        RobotReveal,    // 0-160ms
        TitleFadeIn,    // 160-260ms
        SubtitleFadeIn, // 260-360ms
        InputFadeIn,    // 360-460ms
        ButtonFadeIn,   // 460-560ms
        Complete        // Animación completa, solo robot animations activas
    };

    AnimPhase animPhase_{AnimPhase::Inactive};
    int64_t animStartMs_{0};

    // ═══ Alpha values for staggered elements ══════════════════════════════
    float robotAlpha_{0.0f};
    float robotScale_{0.95f};
    float titleAlpha_{0.0f};
    float subtitleAlpha_{0.0f};
    float inputAlpha_{0.0f};
    float buttonAlpha_{0.0f};

    // ═══ Robot continuous animations ══════════════════════════════════════
    float robotHaloAlpha_{0.0f};     // Halo pulsing alpha (0.0 to 0.55)
    int64_t robotAnimStartMs_{0};

    // ═══ Validation state ════════════════════════════════════════════════
    bool hasValidationError_{false};

    // ═══ UI Components ════════════════════════════════════════════════════
    MixBotComponent avatar_;
    juce::TextEditor nameEditor_;
    juce::TextButton startButton_;

    // ═══ Timer callback (animations) ══════════════════════════════════════
    void timerCallback() override;
    /** Pausa/restaura el timer segun la visibilidad del componente. */
    void visibilityChanged() override;

    // ═══ TextEditor::Listener ═════════════════════════════════════════════
    void textEditorReturnKeyPressed(juce::TextEditor&) override;
    void textEditorEscapeKeyPressed(juce::TextEditor&) override;
    void textEditorTextChanged(juce::TextEditor&) override;

    // ═══ Handlers ═════════════════════════════════════════════════════════
    void onStartClicked();
    void updateAnimations();

    // ═══ Layout helpers ══════════════════════════════════════════════════
    [[nodiscard]] float getLayoutScale() const noexcept;

    /** Retorna la Y del centro del contenido (robot center). */
    int getContentCenterY() const noexcept;

    /** Retorna la altura total del bloque de contenido estimado. */
    int getContentBlockHeight() const noexcept;

    // ═══ Drawing helpers ══════════════════════════════════════════════════
    void drawBackground(juce::Graphics& g);
    void drawRadialGlow(juce::Graphics& g);
    void drawAvatarArea(juce::Graphics& g, juce::Rectangle<int> avatarBounds);
    void drawTitle(juce::Graphics& g, int titleY);
    void drawSubtitle(juce::Graphics& g, int subtitleY);
    void drawInputBorder(juce::Graphics& g, juce::Rectangle<int> inputBounds);

    // ═══ Easing helpers ══════════════════════════════════════════════════
    static float easeOutCubic(float t);
    static float easeOutBack(float t);

    // ═══ Custom LookAndFeel for button ═══════════════════════════════════
    class StartButtonLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        void drawButtonBackground(juce::Graphics& g,
                                  juce::Button& button,
                                  const juce::Colour& backgroundColour,
                                  bool shouldDrawButtonAsHighlighted,
                                  bool shouldDrawButtonAsDown) override;
        void drawButtonText(juce::Graphics& g,
                            juce::TextButton& button,
                            bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown) override;
    };

    StartButtonLookAndFeel startButtonLnf_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WelcomeComponent)
};

} // namespace mixcoach
