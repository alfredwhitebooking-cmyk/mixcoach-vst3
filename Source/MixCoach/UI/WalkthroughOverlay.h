#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  WalkthroughOverlay — Tutorial interactivo de 5 pasos para nuevos usuarios
//
//  SCENE: Primera vez que abre MixCoach → overlay guiado que explica
//  los elementos clave del plugin: chat, mapa, recomendaciones,
//  referencia y comandos rápidos.
//
//  • 5 pasos con tooltips posicionados + flecha direccional
//  • Semi-transparente: oscurece todo excepto el área explicada
//  • Clic en cualquier lado → avanza al siguiente paso
//  • Clic en "Saltar tutorial" → cierra permanentemente
//  • Se muestra solo la primera vez (firstSession == true)
// ═══════════════════════════════════════════════════════════════════════════
class WalkthroughOverlay : public juce::Component,
                          private juce::Timer
{
public:
    WalkthroughOverlay();
    ~WalkthroughOverlay() override;

    // ─── Paso individual del walkthrough ─────────────────────────────────
    struct WalkthroughStep {
        juce::String title;              // Título corto (ej: "El Coach")
        juce::String description;        // Texto explicativo (2-3 líneas)
        juce::Rectangle<int> highlightArea; // Área a iluminar (coords del parent)
        juce::Rectangle<int> tooltipArea;   // Área donde va el tooltip
        float tooltipAnchorX{0.5f};      // 0=left, 0.5=center, 1=right
        float tooltipAnchorY{0.0f};      // 0=top, 0.5=center, 1=bottom
        juce::String icon;               // Emoji opcional
    };

    // ─── API pública ─────────────────────────────────────────────────────

    /** Inicia el walkthrough desde el paso 0. */
    void startWalkthrough();

    /** Avanza al siguiente paso. Si ya está en el último, oculta el overlay. */
    void advanceStep();

    /** Retrocede al paso anterior. */
    void previousStep();

    /** Salta el tutorial completo y lo marca como completado. */
    void skipWalkthrough();

    /** Reinicia el walkthrough desde el paso 0 (para settings/testing). */
    void restartWalkthrough();

    /** Retorna true si el walkthrough está activo (visible o animando). */
    [[nodiscard]] bool isWalkthroughActive() const noexcept { return isVisible() && currentStep_ >= 0; }

    /** Retorna el paso actual (-1 si inactivo). */
    [[nodiscard]] int getCurrentStep() const noexcept { return currentStep_; }

    /** Retorna el total de pasos. */
    [[nodiscard]] int getTotalSteps() const noexcept { return kNumSteps; }

    /** Configura las áreas de highlight para cada paso según el layout actual.
        Se llama desde NavigationShell::resized() para que las posiciones
        se adapten al tamaño real de los componentes. */
    void updateStepPositions(juce::Rectangle<int> parentBounds);

    /** Callback: se dispara cuando el walkthrough se completa. */
    std::function<void()> onComplete;

    /** Callback: se dispara cuando el usuario salta el tutorial. */
    std::function<void()> onSkipped;

    // ─── Component overrides ─────────────────────────────────────────────
    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

private:
    void mouseDown(const juce::MouseEvent& e) override;
    void timerCallback() override;

    // ─── Pasos definidos ─────────────────────────────────────────────────
    static constexpr int kNumSteps = 5;

    WalkthroughStep steps_[kNumSteps];
    int currentStep_{-1};  // -1 = inactivo

    // ─── Animación ───────────────────────────────────────────────────────
    float animProgress_{0.0f};  // 0.0 → 1.0 para fade-in de cada paso
    bool animating_{false};

    static constexpr float kFadeFrames = 10.0f; // ~167ms a 60fps
    static constexpr float kFadeStep = 1.0f / kFadeFrames;

    // ─── Drawing helpers ─────────────────────────────────────────────────
    void drawDimOverlay(juce::Graphics& g);
    void drawHighlightCutout(juce::Graphics& g);
    void drawStepTooltip(juce::Graphics& g);
    void drawStepDots(juce::Graphics& g);
    void drawSkipButton(juce::Graphics& g);

    // ─── Easing ──────────────────────────────────────────────────────────
    static float easeOutQuad(float t) noexcept { return t * (2.0f - t); }
    static float easeInQuad(float t) noexcept { return t * t; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WalkthroughOverlay)
};

} // namespace mixcoach
