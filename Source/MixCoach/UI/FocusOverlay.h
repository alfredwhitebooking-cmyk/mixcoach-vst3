#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "../../Common/types/Constants.h"
#include "../../Common/types/Types.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  FocusOverlay — Overlay full-screen que oscurece todo excepto un área
//  de enfoque (grupo de pistas o pista individual).
//
//  SCENE 9: "Everything becomes dark. Only DRUMS glows."
//
//  Dos modos:
//    • GroupFocus: cutout rectangular alrededor de un bus header
//    • TrackFocus: cutout alrededor de una fila de pista
//
//  Animación: fade-in 200ms (ease-out quad), fade-out 150ms (ease-in quad)
//  Borde del cutout: glow sutil del color del bus/pista
//  Label: texto descriptivo centrado debajo del cutout
//  Dismiss: clic fuera del cutout → clearFocus()
// ═══════════════════════════════════════════════════════════════════════════
class FocusOverlay : public juce::Component,
                     private juce::Timer
{
public:
    FocusOverlay();
    ~FocusOverlay() override;

    // ─── Tipos de enfoque ────────────────────────────────────────────────
    enum class FocusMode : uint8_t
    {
        None,        // Sin enfoque
        GroupFocus,  // Grupo completo (bus)
        TrackFocus   // Pista individual
    };

    // ─── API pública ─────────────────────────────────────────────────────
    /** Enfoca un grupo de pistas (bus). Oscurece todo excepto el área dada.
        @param busType     Tipo de bus (Drums, Bass, etc.)
        @param busName     Nombre legible del bus
        @param busColour   Color del bus para el glow del cutout
        @param focusBounds Área en coordenadas de pantalla que debe quedar visible */
    void focusGroup(BusType busType,
                    const juce::String& busName,
                    juce::Colour busColour,
                    juce::Rectangle<int> focusBounds);

    /** Enfoca una pista individual. Oscurece todo excepto el área dada.
        @param slotIndex   Índice del slot
        @param trackName   Nombre de la pista
        @param trackColour Color de la pista para el glow del cutout
        @param focusBounds Área en coordenadas de pantalla que debe quedar visible */
    void focusTrack(int slotIndex,
                    const juce::String& trackName,
                    juce::Colour trackColour,
                    juce::Rectangle<int> focusBounds);

    /** Limpia el enfoque con fade-out.
        @param fadeMs Duración del fade-out en ms (default 150) */
    void clearFocus(float fadeMs = 150.0f);

    /** Retorna true si el overlay está activo (animando o visible). */
    [[nodiscard]] bool isFocusActive() const noexcept { return focusMode_ != FocusMode::None; }

    /** Retorna el modo de enfoque actual. */
    [[nodiscard]] FocusMode getFocusMode() const noexcept { return focusMode_; }

    /** Retorna el slotIndex enfocado (-1 si no es TrackFocus). */
    [[nodiscard]] int getFocusedSlot() const noexcept { return focusedSlot_; }

    /** Retorna el BusType enfocado (None si no es GroupFocus). */
    [[nodiscard]] BusType getFocusedBus() const noexcept { return focusedBus_; }

    /** Callback cuando el usuario hace clic fuera del cutout para quitar el focus. */
    std::function<void()> onDismiss;

    // ─── Component overrides ─────────────────────────────────────────────
    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

private:
    void mouseDown(const juce::MouseEvent& e) override;
    void timerCallback() override;

    // ─── Estado ──────────────────────────────────────────────────────────
    FocusMode focusMode_{FocusMode::None};
    FocusMode targetFocusMode_{FocusMode::None};  // Modo destino (para animación)

    BusType focusedBus_{BusType::None};
    int focusedSlot_{-1};
    juce::String focusLabel_;
    juce::Colour focusColour_{juce::Colours::white};
    juce::Rectangle<int> cutoutBounds_;  // Área que NO se oscurece
    juce::Rectangle<int> targetCutoutBounds_; // Cutout destino (para animación)

    // ─── Animación ───────────────────────────────────────────────────────
    enum class AnimState : uint8_t
    {
        Idle,        // Sin animación
        FadingIn,    // Apareciendo (alpha 0→1, cutout escala 0.95→1)
        FadingOut   // Desapareciendo (alpha 1→0)
    };

    AnimState animState_{AnimState::Idle};
    float animProgress_{0.0f};      // 0.0 → 1.0

    static constexpr float kFadeInFrames  = 12.0f;  // 200ms a 60fps
    static constexpr float kFadeOutFrames = 9.0f;   // 150ms a 60fps

    // ─── Drawing helpers ─────────────────────────────────────────────────
    void drawCutoutPath(juce::Graphics& g);
    void drawGlowBorder(juce::Graphics& g);
    void drawFocusLabel(juce::Graphics& g);
    void drawHintText(juce::Graphics& g);

    // ─── Easing ──────────────────────────────────────────────────────────
    static float easeOutQuad(float t) noexcept { return t * (2.0f - t); }
    static float easeInQuad(float t) noexcept { return t * t; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FocusOverlay)
};

} // namespace mixcoach
