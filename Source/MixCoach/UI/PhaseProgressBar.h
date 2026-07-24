#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "MixCoachTheme.h"
#include "SmoothValue.h"
#include "../engine/PhaseManager.h"
#include "../../Common/types/Types.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  PhaseProgressBar — Barra de progreso horizontal con 7 dots de fase
    //  y contador XP animado.
    //
    //  Layout:
    //    [●]─[●]─[●]─[●]─[●]─[●]─[●]   XP: 450
    //
    //  Estados de cada dot:
    //    • No iniciado:  círculo gris hueco
    //    • Fase actual:  círculo pulsante (acento púrpura)
    //    • Completado:   círculo verde relleno con ✓
    //
    //  Animaciones vía SmoothValue para transiciones suaves entre estados.
    // ═══════════════════════════════════════════════════════════════════════════
    class PhaseProgressBar : public juce::Component,
                              private juce::Timer
    {
    public:
        PhaseProgressBar();
        ~PhaseProgressBar() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void timerCallback() override;
    /** Pausa/restaura el timer segun la visibilidad del componente. */
    void visibilityChanged() override;

        /** Actualiza el estado de los dots y XP desde el PhaseManager.
            Anima automáticamente las transiciones vía SmoothValue. */
        void updateFromPhaseManager(const PhaseManager& pm);

        /** Fuerza un XP específico. */
        void setXp(int xp) noexcept { targetXp_ = xp; }

        /** Dispara la animación XP burst manualmente (usado desde CoachingNarrativeDirector).
            @param xpAmount  Cantidad de XP a mostrar en el burst (default 45). */
        void triggerXpBurst(int xpAmount = 45);

        /** Retorna el XP actual (animado). */
        int getDisplayedXp() const noexcept { return (int)xpValue_.getCurrent(); }

        /** Dispara animación de burst en un dot específico (1.5x → checkmark → ripple).
            Llamado automáticamente cuando un dot pasa de incomplete a completed. */
        void triggerDotBurst(int dotIndex);

        /** Altura fija de la barra. */
        static constexpr int kHeight = 28;

        // ═══ Dot burst — animación 1.5× → checkmark → ripple al completar fase ═══
        // Cada dot tiene su propio burst que se dispara cuando pasa de incomplete a complete.
        struct DotBurst {
            bool active = false;
            int startTimeMs = 0;
            float scale = 1.0f;       // 1.0 → 1.5 → 1.0 durante la animación
            float glowRadius = 0.0f;   // 0 → 30px → 0 (anillo expansivo)
            float glowAlpha = 0.0f;    // 0 → 0.6 → 0
            static constexpr int kDurationMs = 900;  // 900ms total
        };

        // ═══ WavePulse — propagación desde el dot completado hacia los siguientes
        // Crea un ripple que viaja desde el dot origen expandiendo un anillo luminoso.
        struct WavePulse {
            bool active = false;
            int startTimeMs = 0;
            int originIndex = -1;
            float waveRadius = 0.0f;     // 0 → 1.0 (progreso normalizado de la onda)
            float waveAlpha = 0.0f;      // 0 → 0.5 → 0
            static constexpr int kDurationMs = 600;  // 600ms de propagación
        };

        // ═══ XP burst animation (celebración al completar fase) ════════════
        struct XpBurst {
            bool active = false;
            int startTimeMs = 0;
            int xpAmount = 0;
            static constexpr int kDurationMs = 1800;  // 1.8s total (más tiempo para animación bounce)
            static constexpr int kDelayMs = 300;      // 300ms de retardo antes de empezar
            // Permite que el dot burst termine primero antes del XP burst
        };
        XpBurst xpBurst_;

        /** Tiempo de gracia para evitar falsos positivos al cargar datos iniciales.
            Durante este periodo (ms desde construcción), no se disparan celebraciones.
            A diferencia de firstUpdateDone, esto permite celebraciones reales
            si el usuario ya estaba en una fase completada al reabrir el plugin. */
        static constexpr int64_t kGracePeriodMs = 2000;
        int64_t creationTimeMs_ = 0;

    private:
        // ─── Datos de cada fase ────────────────────────────────────────────
        struct PhaseDot {
            MentorPhase phase;
            const char* label;      // Etiqueta corta (ej: "GAIN")
            const char* icon;       // Emoji (ej: "🎚️")
            bool completed = false;
            bool isCurrent = false;
            SmoothValue progress;   // 0 = no iniciado, 0.5 = actual, 1.0 = completado
            float displayRadius;    // Radio actual (para animación pulse)
        };

        std::vector<PhaseDot> dots_;
        int targetXp_ = 0;
        SmoothValue xpValue_{0.0f, 30.0f, 100.0f}; // Attack rápido, release lento para XP

        // ═══ Burst + Wave state ═══════════════════════════════════════════
        std::vector<DotBurst> dotBursts_;       // Un burst por dot (mayoría inactivos)
        WavePulse wavePulse_;                    // Onda de propagación activa

        // ─── Layout cacheados ─────────────────────────────────────────────
        struct DotLayout {
            juce::Rectangle<float> bounds;
            juce::Point<float> lineTo; // Conexión al siguiente dot
        };
        std::vector<DotLayout> dotLayouts_;
        juce::Rectangle<int> xpLabelBounds_;

        // ─── Inicializa los 7 dots (una vez en constructor) ───────────────
        void initDots();

        // ═══ Helpers de dibujo ═════════════════════════════════════════════
        void drawDot(juce::Graphics& g, int index, const DotLayout& layout);
        void drawConnectingLine(juce::Graphics& g);
        void drawXpCounter(juce::Graphics& g, const juce::Rectangle<int>& bounds);
        void drawWavePropagation(juce::Graphics& g);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhaseProgressBar)
    };

} // namespace mixcoach
