#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  QuickReplyBar — Barra de botones de respuesta rápida debajo del último
    //  mensaje del coach. Similar a los suggestion chips pero siempre asociada
    //  al último mensaje del coach y se limpia al enviar un mensaje.
    //
    //  Layout: fila horizontal de botones pill, max 4, overflow scroll
    // ═══════════════════════════════════════════════════════════════════════════
    class QuickReplyBar : public juce::Component,
                               private juce::Timer
    {
    public:
        QuickReplyBar();

        void paint(juce::Graphics& g) override;
        void resized() override;
        void timerCallback() override;
    /** Pausa/restaura el timer segun la visibilidad del componente. */
    void visibilityChanged() override;

        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;
        void mouseDown(const juce::MouseEvent& e) override;

        /** Establece las respuestas rápidas a mostrar (máx 4).
            Inicia la animación de entrada (slide-up + fade-in). */
        void setReplies(const std::vector<juce::String>& replies);

        /** Limpia todas las respuestas y detiene la animación (instantáneo). */
        void clearReplies();

        /** Inicia animación de fade-out (150ms). Al completarse, llama a
            onFadeOutComplete para que el padre oculte y re-layoutée. */
        void startFadeOut();

        /** Retorna true si hay replies activos o si está en fade-out. */
        bool hasReplies() const noexcept { return !replies_.empty() || fadingOut_; }

        /** Retorna true si está en medio de un fade-out. */
        bool isFadingOut() const noexcept { return fadingOut_; }

        /** Callback cuando el usuario selecciona una respuesta. */
        std::function<void(const juce::String&)> onReplySelected;

        /** Callback cuando el fade-out se completa. El padre debe ocultar
            el componente y llamar resized(). */
        std::function<void()> onFadeOutComplete;

    public:
        static constexpr int kHeight = 28;

    private:
        std::vector<juce::String> replies_;
        std::vector<juce::Rectangle<float>> replyBounds_;
        int hoveredIndex_ = -1;

        static constexpr int kMaxReplies = 4;

        // ═══ Entrance animation (slide-up + fade-in) ═══════════════════════
        bool animating_ = false;
        float slideOffset_ = 20.0f;
        float currentAlpha_ = 0.0f;
        int64_t animStartMs_ = 0;

        static constexpr float kSlidePx = 20.0f;
        static constexpr int kAnimDurationMs = 200;

        void startEntranceAnimation();

        // ═══ Fade-out animation ═══════════════════════════════════════════
        bool fadingOut_ = false;
        int64_t fadeOutStartMs_ = 0;
        static constexpr int kFadeOutDurationMs = 150;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuickReplyBar)
    };

} // namespace mixcoach
