#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  SessionScanCard — Tarjeta animada de escaneo de sesión (Fase 2)
    //  Muestra el progreso de identificación de pistas y roles.
    //  Aparece inline en el chat durante la transición de Prep → MixMap.
    // ═══════════════════════════════════════════════════════════════════════════
    class SessionScanCard : public juce::Component,
                            private juce::Timer
    {
    public:
        SessionScanCard();
        ~SessionScanCard() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void timerCallback() override;
    /** Pausa/restaura el timer segun la visibilidad del componente. */
    void visibilityChanged() override;

        /** Inicia el proceso de escaneo visual.
            @param totalTracks  Número total de pistas detectadas en el DAW.
            @param trackNames   Nombres de las pistas para efectos visuales. */
        void startScan(int totalTracks, const std::vector<juce::String>& trackNames);

        /** Actualiza el progreso manualmente (0.0 a 1.0). */
        void setProgress(float progress);

        /** Setea el mensaje de estado actual. */
        void setStatusMessage(const juce::String& status);

        /** Reinicia el estado interno de la tarjeta. */
        void reset() {
            currentProgress_ = 0.0f;
            targetProgress_ = 0.0f;
            currentTrackIndex_ = 0;
            flickeringTrackName_ = {};
            startTimeMs_ = 0;
            stopTimer();
            repaint();
        }

        /** Callback cuando la animación llega al 100%. */
        std::function<void()> onScanComplete;

        /** Callback cuando la animación llega al 100% (alias para compatibilidad). */
        std::function<void()> onComplete;

        /** Retorna el número de pistas visibles durante la animación. */
        int getVisibleTrackCount() const noexcept { return (int)trackNames_.size(); }

        /** Retorna true mientras la animación de escaneo esté activa. */
        bool isAnimating() const noexcept { return isTimerRunning() && currentProgress_ < 1.0f; }

    private:
        int totalTracks_ = 0;
        std::vector<juce::String> trackNames_;
        juce::String currentStatus_ = "Detectando pistas...";
        
        float currentProgress_ = 0.0f;
        float targetProgress_  = 0.0f;
        
        // ─── Animación de nombres de pistas saltando ───────────────────────
        int currentTrackIndex_ = 0;
        juce::String flickeringTrackName_;
        
        // ─── Entrance animation ────────────────────────────────────────────
        float alpha_ = 0.0f;
        int64_t startTimeMs_ = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SessionScanCard)
    };

} // namespace mixcoach
