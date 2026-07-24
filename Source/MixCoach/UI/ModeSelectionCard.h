#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  ModeSelectionCard — Tarjeta visual grande para elegir modo (Mix/Master).
    //
    //  Calca el patrón de GenreSelectionCard (slide-up + fade-in 200ms, hover con
    //  glow, ícono dibujado a mano en paint()). Dos instancias: Mix y Master.
    //
    //  El callback onCardSelected dispara el mismo flujo que los chips de
    //  sugerencia, pero con el texto "Mezclar" / "Masterizar" para que
    //  NavigationShell::onSuggestionClicked lo procese.
    //
    //  Visual spec:
    //    - Dark background #0D0B1A con gradiente sutil
    //    - Borde hover/selected con glow púrpura
    //    - Ícono grande custom: Mix = faders, Master = spectrogram
    //    - Título 36px accent, descripción 12px textDim
    //    - Flecha indicadora en hover/selected
    //    - Animación de entrada slide-up + fade-in 200ms
    // ═══════════════════════════════════════════════════════════════════════════
    class ModeSelectionCard : public juce::Component,
                              private juce::Timer
    {
    public:
        enum class ModeType
        {
            Mix = 0,
            Master
        };

        explicit ModeSelectionCard(ModeType type);
        ~ModeSelectionCard() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;

        void mouseEnter(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;
        void mouseDown(const juce::MouseEvent& e) override;

        void setSelected(bool selected) noexcept
        {
            selected_ = selected;
            repaint();
        }

        bool isSelected() const noexcept { return selected_; }

        /** Reinicia la animación de entrada slide-up + fade-in.
            Llamar cuando la tarjeta se muestra después de estar oculta. */
        void restartAnimation()
        {
            animStartMs_ = juce::Time::getMillisecondCounter();
            setAlpha(0.0f);
            setTransform(juce::AffineTransform::translation(0.0f, 30.0f));
            startTimerHz(60);
        }

        void visibilityChanged() override
        {
            if (!isVisible()) stopTimer();
        }

        /** Retorna el título legible del modo (UI). */
        static juce::String modeTitle(ModeType type) noexcept;

        /** Retorna una descripción corta del modo (UI). */
        static juce::String modeDescription(ModeType type) noexcept;

        /** Devuelve el ModeType de esta tarjeta. */
        ModeType getType() const noexcept { return type_; }

        std::function<void()> onCardSelected;

    private:
        void timerCallback() override;

        ModeType type_ = ModeType::Mix;
        bool hovered_ = false;
        bool selected_ = false;
        int64_t animStartMs_ = 0;
        static constexpr float kAnimDurationMs = 200.0f;

        void drawMixIcon(juce::Graphics& g, juce::Rectangle<float> area);
        void drawMasterIcon(juce::Graphics& g, juce::Rectangle<float> area);
    };

} // namespace mixcoach
