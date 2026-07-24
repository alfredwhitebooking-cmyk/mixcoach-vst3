#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "../../Common/memory/SlotRegistry.h"
#include "../../Common/types/Types.h"

namespace mixcoach {

    class CoachEngine;

    // ═══════════════════════════════════════════════════════════════════════════
    //  SessionPrepChecklist — Checklist premium de "Preparar sesión" (STATE 4.5)
    //
    //  Ritual pedagógico (visión fundacional: "el orden garantiza
    //  mejores mezclas"). El coach NO construye el MixMap hasta que la sesión
    //  está organizada, porque sin orden no puede ser preciso recomendando.
    //
    //  5 checks, todos visibles simultáneamente:
    //    ☐ Insertar Messenger en cada pista — al menos 1 Messenger activo
    //    ☐ Nombres   — sin tracks llamados "Pista NN" ni vacíos
    //    ☐ Colores   — sin tracks con color default (grey)
    //    ☐ Buses     — sin tracks con BusType::None
    //    ☐ Grupos    — ≥2 buses distintos con pistas (sesión agrupada)
    //
    //  Cuando los 5 están ✅, el botón "Verificar" se enciende y dispara
    //  onContinue → NavigationShell avanza a MixMapStage.
    //
    //  Datos: registra CoachEngine* + SlotRegistry* vía setRefs(). refresh() se
    //  invoca desde un Timer interno a 30Hz para reflejar los cambios del usuario
    //  en el DAW en vivo (renombrar/colorear/rutear) (Message Thread, seguro).
    // ═══════════════════════════════════════════════════════════════════════════
    class SessionPrepChecklist : public juce::Component,
                                private juce::Timer
    {
    public:
        SessionPrepChecklist();
        ~SessionPrepChecklist() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;

        void mouseDown(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;

        /** Inyecta las fuentes de datos. SlotRegistry debe vivir más que este
            componente (es singleton del processor). Llamar antes de mostrar. */
        void setRefs(CoachEngine* coach, SlotRegistry* registry) noexcept
        {
            coach_ = coach;
            registry_ = registry;
            refresh();
        }

        /** Recalcula los 5 checks iterando el registry. Idempotente, seguro
            en Message Thread (solo lee). */
        void refresh();

        /** Recalcula y repainta. */
        void refreshAndRepaint() { refresh(); repaint(); }

        /** True cuando los 4 checks de organización están listos (Tiempo 1). */
        bool isOrganized() const noexcept { return organized_; }

        /** True cuando los 5 checks están listos → botón Construir MixMap activo. */
        bool allReady() const noexcept { return tracksReady_ && organized_ && messengersActive_ && messengersRoutedAndColored_; }

        // ─── Acceso a datos para depuración / externos ──────────────────────
        int activeTrackCount() const noexcept { return activeTrackCount_; }
        int unamedCount()    const noexcept { return unamed_; }
        int unoColorCount()  const noexcept { return uncolored_; }
        int unoBusCount()    const noexcept { return unrouted_; }
        int distinctBuses()  const noexcept { return distinctBuses_; }

        /** Disparado al clic en "Construir MixMap" (solo si allReady()). */
        std::function<void()> onContinue;

        /** Disparado cuando un ítem del checklist pasa de incompleto a completado.
            @param itemIndex  0-4 (0=Messenger, 1=Nombres, 2=Colores, 3=Buses, 4=Grupos) */
        std::function<void(int itemIndex)> onItemChecked;

        void setVisible(bool show) override;

    private:
        void timerCallback() override;
    /** Pausa/restaura el timer segun la visibilidad del componente. */
    void visibilityChanged() override;

        // ─── Fuentes de datos ───────────────────────────────────────────────
        CoachEngine*   coach_    = nullptr;
        SlotRegistry*  registry_ = nullptr;

        // ─── 4 checks ────────────────────────────────────────────────────────
        bool tracksReady_              = false;  // Check 1: tracks insertados en DAW
        bool organized_                = false;  // Check 2: nombres+colores+buses+grupos
        bool messengersActive_         = false;  // Check 3: Messenger en cada track
        bool messengersRoutedAndColored_ = false; // Check 4: Messenger ruteados/coloreados

        // ─── Conteos en vivo (para labels "3 / 12 sin nombre") ──────────────
        int activeTrackCount_ = 0;
        int unamed_           = 0;  // tracks sin nombre (vacío o "Pista NN")
        int uncolored_        = 0;  // tracks con grey/default
        int unrouted_         = 0;  // tracks con BusType::None
        int distinctBuses_    = 0;  // buses distintos con pistas

        // ─── Hover/animación ─────────────────────────────────────────────────
        bool continueHovered_ = false;
        bool fadeIn_          = false;
        float fadeAlpha_      = 0.0f;
        juce::uint32 animStartMs_ = 0;

        // ─── Layout (caché de bounds para hit testing) ───────────────────────
        juce::Rectangle<float> continueBounds_;

        void drawCheckRow(juce::Graphics& g, juce::Rectangle<float> area,
                          bool done, const juce::String& label,
                          const juce::String& count) const;
        void drawContinueButton(juce::Graphics& g, juce::Rectangle<float> area);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SessionPrepChecklist)
    };

} // namespace mixcoach
