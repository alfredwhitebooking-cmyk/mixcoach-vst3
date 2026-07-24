#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#include "../../Common/types/Types.h"
#include "../UI/CoachRoomState.h"
#include "CoachEngine.h"
#include "PanelRevealManager.h"
#include "CoachingStageManager.h"
#include "SessionProgression.h"
#include "WorkflowDetector.h"
#include "SceneManager.h"

namespace mixcoach {

    // Forward declarations
    class NavigationShell;

    // ═══════════════════════════════════════════════════════════════════════════
    //  ExperienceManager — El orquestador de la experiencia (UX 2.0 Fase 9)
    //
    //  Es el director de toda la experiencia. No analiza audio ni usa IA.
    //  Solo controla: qué aparece, qué desaparece, qué pestaña abrir,
    //  qué animación lanzar, qué fase mostrar, qué bloquear y qué desbloquear.
    //
    //  Arquitectura:
    //    CoachEngine ───────→ ExperienceManager ───────→ UI Components
    //      (eventos)              │
    //                              │
    //                         ┌────┴────┐
    //                         ▼         ▼
    //                   PanelReveal   NavigationShell
    //                   Manager       (sidebar + content)
    //
    //  Uso:
    //    1. NavigationShell crea el ExperienceManager
    //    2. Llama a wireToEngine() cuando CoachEngine está disponible
    //    3. ExperienceManager se conecta a los callbacks del engine
    //    4. Cuando el engine emite eventos, ExperienceManager orquesta la UI
    // ═══════════════════════════════════════════════════════════════════════════
    class ExperienceManager
    {
    public:
        /** Crea el orquestador.
            @param navShell       Referencia al NavigationShell para controlar UI
            @param revealManager  Referencia al PanelRevealManager para tracking de paneles */
        ExperienceManager(NavigationShell& navShell, PanelRevealManager& revealManager);

        // ─── Conexión al motor ─────────────────────────────────────────────
        /** Conecta todos los callbacks al CoachEngine.
            Se llama cuando el engine está disponible (lazy init).
            Desconecta callbacks anteriores si los hay. */
        void wireToEngine(CoachEngine& engine);

        /** Desconecta todos los callbacks del CoachEngine.
            Se llama al destruir o reiniciar. */
        void unwireFromEngine(CoachEngine& engine);

        // ─── Eventos del motor (llamados desde callbacks internos) ─────────
        /** Se dispara cuando cambiar el SetupStep del engine.
            Mapea cada paso a la acción UI correspondiente. */
        void onSetupStepChanged(CoachEngine::SetupStep oldStep, CoachEngine::SetupStep newStep);

        /** Se dispara cuando cambia el CoachingStage.
            Mapea cada etapa al estado UI correspondiente.
            EQ → desbloquea Tools tab. */
        void onCoachingStageChanged(CoachingStage oldStage, CoachingStage newStage);

        /** Se dispara cuando cambia la SessionProgression.
            DeepAnalysis → Session tab unlock.
            Report → overlay reporte. */
        void onSessionProgressionChanged(SessionProgression::Phase oldPhase, SessionProgression::Phase newPhase);

        /** Se dispara cuando el motor detecta issues críticos como clipping.
            Setea la expresión Serious en el avatar. */
        void onCriticalIssueDetected(bool hasClipping, int clippingCount);

        /** Se dispara cuando el WorkflowDetector detecta un cambio en plugins (fader, EQ, etc.).
            Setea la expresión Thinking/Encouraging según el tipo de cambio. */
        void onWorkflowEvent(const WorkflowEvent& event);

        // ─── Orquestación directa ──────────────────────────────────────────
        /** Avanza al estado UI con celebraciones en transiciones clave. */
        void advanceToState(CoachRoomState state);

        /** Revela un panel y maneja unlock de tabs secundarios. */
        void revealAndAnimate(PanelId panel);

        /** Dispara una animación de celebración.
            Se llama cuando el usuario completa una etapa significativa. */
        void celebrate(const juce::String& achievement);

        /** Retorna al Coach tab (desde Tools o Session). */
        void returnToCoach();

        // ─── Actualización de animaciones ──────────────────────────────────
        /** Avanza las animaciones de orquestación (celebraciones).
            Se llama desde NavigationShell::timerCallback() en cada frame. */
        void updateAnimations();

        // ─── Estado ────────────────────────────────────────────────────────
        [[nodiscard]] bool isAnimating() const noexcept { return celebrationActive_; }
        [[nodiscard]] bool isWired() const noexcept { return isWired_; }

    private:
        NavigationShell& navShell_;
        PanelRevealManager& revealManager_;

        // ─── Engine reference (para callbacks) ─────────────────────────────
        CoachEngine* wiredEngine_ = nullptr;
        bool isWired_ = false;

        // ─── Callback storage (para unwire) ────────────────────────────────
        std::function<void(CoachEngine::SetupStep, CoachEngine::SetupStep)> setupStepCb_;
        std::function<void(CoachingStage, CoachingStage)> stageChangedCb_;
        std::function<void(SessionProgression::Phase, SessionProgression::Phase)> sessionProgCb_;
        std::function<void(bool, int)> criticalIssueCb_;
        std::function<void(const WorkflowEvent&)> workflowEventCb_;
        std::function<void(const DirectorEvent&)> directorEventCb_;

        // ─── Animación de celebración ──────────────────────────────────────
        bool celebrationActive_ = false;
        float celebrationProgress_ = 0.0f;
        juce::String celebrationMessage_;
        static constexpr float kCelebrationDuration = 30.0f; // ~500ms at 60fps
        static constexpr float kCelebrationStep = 1.0f / kCelebrationDuration;
    };

} // namespace mixcoach
