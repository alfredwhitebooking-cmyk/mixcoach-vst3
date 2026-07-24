#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "CoachRoomState.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  CoachingEvidenceHost — Contenedor unificado del panel derecho en coaching
    //
    //  Layout:
    //    ┌─────────────────────────────────────────────────────┐
    //    │ ◉ GAIN STAGING                                      │
    //    │ Ajusta los niveles de cada pista antes del balance  │
    //    ├──────────────────────┬──────────────────────────────┤
    //    │  PHASE PANEL (65%)   │  EVIDENCE PANEL (35%)        │
    //    │                      │                              │
    //    │  · GainStagingPanel  │  · VU / Spectrum             │
    //    │  · EQPanel           │  · Crest / Vectorscope       │
    //    │  · CompressionPanel  │  · Match Score               │
    //    │  · SpacePanel        │                              │
    //    │  · MasterCheckPanel  │                              │
    //    └──────────────────────┴──────────────────────────────┘
    //
    //  NOTA: Los dots de progreso están en PhaseProgressBar (top bar),
    //  NO duplicados aquí. El header muestra título + subtítulo de la fase activa.
    //
    //  Sustituye el if-else chain de 5 paneles + EvidencePanel
    //  en MixCoachPanel::resized() por un layout unificado.
    // ═══════════════════════════════════════════════════════════════════════════
    class CoachingEvidenceHost : public juce::Component
    {
    public:
        CoachingEvidenceHost();
        ~CoachingEvidenceHost() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;

        /** Registra los 7 paneles que este host gestionará.
            Los paneles NO son owned por este host — MixCoachPanel los posee. */
        void setPanels(juce::Component& gainStaging,
                       juce::Component& eq,
                       juce::Component& compression,
                       juce::Component& space,
                       juce::Component& automation,
                       juce::Component& masterCheck,
                       juce::Component& evidence);

        /** Activa la fase actual: muestra el panel correspondiente + título. */
        void setActivePhase(CoachRoomState phase);

        /** Retorna el panel de evidencia para data feeding. */
        juce::Component& getEvidencePanel() noexcept { return *evidencePanel_; }

        /** Retorna la fase activa actual. */
        CoachRoomState getActivePhase() const noexcept { return activePhase_; }

    private:
        // ─── Paneles gestionados (no owned) ───────────────────────────────
        juce::Component* gainPanel_      = nullptr;
        juce::Component* eqPanel_        = nullptr;
        juce::Component* compPanel_      = nullptr;
        juce::Component* spacePanel_     = nullptr;
        juce::Component* autoPanel_     = nullptr;
        juce::Component* masterPanel_    = nullptr;
        juce::Component* evidencePanel_  = nullptr;

        // ─── Estado ───────────────────────────────────────────────────────
        CoachRoomState activePhase_ = CoachRoomState::GainStaging;

        // ─── Layout helpers ───────────────────────────────────────────────
        static constexpr int kHeaderHeight = 44;
        static constexpr int kSplitRatio = 65; // 65% phase, 35% evidence

        /** Retorna el panel activo según la fase. */
        juce::Component* getActivePanel() const noexcept;

        /** Retorna el título de la fase actual. */
        juce::String getPhaseTitle() const;
        juce::String getPhaseSubtitle() const;

        // Los dots de progreso están en PhaseProgressBar (NavigationShell top bar),
        // no duplicados aquí.
    };

} // namespace mixcoach
