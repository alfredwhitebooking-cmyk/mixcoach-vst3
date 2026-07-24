#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#include <vector>
#include <set>

#include "PanelRevealManager.h"
#include "../UI/CoachRoomState.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  SceneId — Identifica cada escena de la sesión (obra de teatro en 3 actos)
    //
    //  Cada escena es un snapshot del estado visible en un momento dado.
    //  Define QUÉ paneles están vivos, QUÉ tab está activo, y QUÉ sigue.
    // ═══════════════════════════════════════════════════════════════════════════
    enum class SceneId : uint8_t {
        // ─── Acto I: Descubrimiento (Setup) ───
        Welcome,            // Solo chat + avatar. SetupManager espera.
        ModeSelection,      // Usuario ingresa nombre, selecciona Mix/Master
        GenreSelection,     // Usuario selecciona género musical
        ReferenceLoad,      // Reference panel aparece, usuario carga referencia
        ReferenceAnalysis,  // Coach analiza referencia, muestra resultados

        // ─── Acto II: Setup ───
        MixMapReview,       // MixMap aparece, usuario confirma routing
        SetupComplete,      // Coach resume setup, pregunta si empezar

        // ─── Acto III: Coaching Activo ───
        Coaching,           // Loop principal: Coach → usuario → Coach
        ToolInFocus,        // Un panel/tool está abierto (auto-return activo)
        GroupFocus,         // FocusOverlay activo enfocando un grupo/bus (SCENE 9)
        TrackFocus,         // FocusOverlay activo enfocando una pista individual (SCENE 9)

        // ─── Acto IV: Refinamiento ───
        Refinement,         // MixScore >= 70, entra modo refinamiento artístico
        RefinementTool,     // Tool de refinamiento abierta

        // ─── Acto V: Cierre ───
        SessionEnd,         // Coach cierra sesión, pregunta por reporte
        ReportView,         // Reporte visible, usuario exporta

        Count
    };

    /** Retorna un label textual para debug. */
    inline const char* sceneIdLabel(SceneId id) noexcept
    {
        switch (id) {
            case SceneId::Welcome:           return "Welcome";
            case SceneId::ModeSelection:     return "ModeSelection";
            case SceneId::GenreSelection:    return "GenreSelection";
            case SceneId::ReferenceLoad:     return "ReferenceLoad";
            case SceneId::ReferenceAnalysis: return "ReferenceAnalysis";
            case SceneId::MixMapReview:      return "MixMapReview";
            case SceneId::SetupComplete:     return "SetupComplete";
            case SceneId::Coaching:          return "Coaching";
            case SceneId::ToolInFocus:       return "ToolInFocus";
            case SceneId::GroupFocus:        return "GroupFocus";
            case SceneId::TrackFocus:        return "TrackFocus";
            case SceneId::Refinement:        return "Refinement";
            case SceneId::RefinementTool:    return "RefinementTool";
            case SceneId::SessionEnd:        return "SessionEnd";
            case SceneId::ReportView:        return "ReportView";
            default:                         return "Unknown";
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  PanelVisibility — Nivel de visibilidad de un panel en una escena
    // ═══════════════════════════════════════════════════════════════════════════
    enum class PanelVisibility : uint8_t {
        Hidden,      // Existe pero no visible (pool, reset state)
        Visible,     // Visible pero sin foco
        Focused      // Visible + foco + highlight central
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  PanelSceneInfo — Estado de un panel individual dentro de una escena
    // ═══════════════════════════════════════════════════════════════════════════
    struct PanelSceneInfo {
        PanelId        id{PanelId::Coach};
        PanelVisibility visibility{PanelVisibility::Hidden};
        float          cornerWeight{0.0f};    // 0.0 = center-dominant, 0.5 = sidebar, 1.0 = corner
        juce::String   context;               // Contexto textual (ej. "Kick drum", "Spectrum")
        bool           resetOnShow{true};     // Si debe resetear estado al mostrar
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  SceneDef — Definición completa de una escena (lo que se ve y qué pasa)
    // ═══════════════════════════════════════════════════════════════════════════
    struct SceneDef {
        SceneId id{SceneId::Welcome};
        juce::String name;                        // Nombre legible
        CoachRoomState coachRoomState{CoachRoomState::Count};  // Count = no cambiar, solo setup scenes setean explícitamente
        juce::String coachMessage;                // Qué dice el Coach al entrar

        std::vector<PanelSceneInfo> panels;        // Estado de cada panel
        bool sidebarEnabled{false};                // ¿Mostrar sidebar con tabs?
        float autoReturnTimeout{0.0f};             // 0 = no auto-return, >0 = timeout en segundos
        bool animate{true};                        // ¿Usar crossfade transition?
        float transitionMs{150.0f};                // Duración de transición

        // Comandos adicionales para la UI
        bool celebrate{false};                     // Disparar celebración
        juce::String achievement;                  // Nombre del logro si celebrate=true
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  DirectorEvent — Eventos que SceneManager entiende
    // ═══════════════════════════════════════════════════════════════════════════
    struct DirectorEvent {
        enum class Type : uint8_t {
            CoachMessage,        // Coach dijo algo (texto plano)
            CoachCommand,        // Coach emitió un comando estructurado (JSON)

            PhaseChanged,        // PhaseManager avanzó de fase
            MixScoreChanged,     // MixScore cambió significativamente
            ReferenceLoaded,     // Usuario cargó una referencia
            ReferenceAnalyzed,   // Análisis de referencia completado
            CorrectionApplied,   // Usuario aplicó una corrección
            MixMapConfirmed,     // Usuario confirmó el MixMap

            UserInteracted,      // Usuario interactuó con un panel
            UserIdle,            // Usuario sin actividad por N segundos
            UserAskedHelp,       // Usuario pidió ayuda explícitamente

            TimerTick,           // Tick de 1s para timing interno
            StreakMilestone,     // Racha alcanzada (3, 7, 14, 30 días)

            // Eventos de setup flow
            UserNameEntered,     // Usuario ingresó su nombre
            ModeSelected,        // Usuario seleccionó Mix/Master
            GenreSelected,       // Usuario seleccionó género
            SessionPrepped,      // Usuario completó checklist de sesión
            ReferenceSkipped,     // Usuario saltó la referencia
            Backward,            // Usuario quiere retroceder un paso en el onboarding

            // Eventos de FocusOverlay (SCENE 9)
            FocusGroupBus,       // Enfocar un grupo/bus completo (Drums, Bass, etc.)
            FocusTrack,          // Enfocar una pista individual
            FocusDismissed,      // Usuario hizo clic fuera del cutout → volver a Coaching

            // Eventos de engine (emitidos desde CoachEngine::periodicAnalysis)
            PriorityIssueDetected,   // Top priority issue cambió
            PhaseProgressUpdated,    // Progreso de fase cambió significativamente
            ReferenceProgressChanged, // Match % contra referencia cambió

            Count
        };

        Type type{Type::TimerTick};
        juce::String payload;       // Texto del mensaje o payload JSON
        float numericValue{0.0f};   // Para MixScoreChanged, TimerTick, etc.
        int64_t timestamp{0};       // Timestamp del evento
    };

    inline const char* directorEventTypeLabel(DirectorEvent::Type type) noexcept
    {
        switch (type) {
            case DirectorEvent::Type::CoachMessage:      return "CoachMessage";
            case DirectorEvent::Type::CoachCommand:      return "CoachCommand";
            case DirectorEvent::Type::PhaseChanged:      return "PhaseChanged";
            case DirectorEvent::Type::MixScoreChanged:   return "MixScoreChanged";
            case DirectorEvent::Type::ReferenceLoaded:   return "ReferenceLoaded";
            case DirectorEvent::Type::ReferenceAnalyzed: return "ReferenceAnalyzed";
            case DirectorEvent::Type::CorrectionApplied: return "CorrectionApplied";
            case DirectorEvent::Type::MixMapConfirmed:   return "MixMapConfirmed";
            case DirectorEvent::Type::UserInteracted:    return "UserInteracted";
            case DirectorEvent::Type::UserIdle:          return "UserIdle";
            case DirectorEvent::Type::UserAskedHelp:     return "UserAskedHelp";
            case DirectorEvent::Type::TimerTick:         return "TimerTick";
            case DirectorEvent::Type::StreakMilestone:          return "StreakMilestone";
            case DirectorEvent::Type::UserNameEntered:         return "UserNameEntered";
            case DirectorEvent::Type::ModeSelected:            return "ModeSelected";
            case DirectorEvent::Type::GenreSelected:           return "GenreSelected";
            case DirectorEvent::Type::SessionPrepped:          return "SessionPrepped";
            case DirectorEvent::Type::ReferenceSkipped:        return "ReferenceSkipped";
            case DirectorEvent::Type::PriorityIssueDetected:   return "PriorityIssueDetected";
            case DirectorEvent::Type::FocusGroupBus:          return "FocusGroupBus";
            case DirectorEvent::Type::FocusTrack:             return "FocusTrack";
            case DirectorEvent::Type::FocusDismissed:         return "FocusDismissed";
            case DirectorEvent::Type::PhaseProgressUpdated:    return "PhaseProgressUpdated";
            case DirectorEvent::Type::ReferenceProgressChanged: return "ReferenceProgressChanged";
            default:                                           return "Unknown";
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  SceneManager — El director de la experiencia
    //
    //  "No analiza audio. No hace IA. No mezcla. Solo dice ¿qué debe pasar ahora?"
    //
    //  SceneManager es la capa que decide en qué escena estamos y qué paneles
    //  deben estar visibles/ocultos. No habla con la UI directamente — produce
    //  SceneDef que NavigationShell consume.
    //
    //  Arquitectura:
    //    CoachEngine ───→ SceneManager ───→ NavigationShell (aplica SceneDef)
    //    PhaseManager ───→ SceneManager        │
    //    User Input  ───→ SceneManager         ▼
    //                                    Paneles visibles/ocultos
    // ═══════════════════════════════════════════════════════════════════════════
    class SceneManager {
    public:
        SceneManager();
        ~SceneManager() = default;

        // ═══ API Pública ═══════════════════════════════════════════════════

        /** Punto de entrada único: procesa un evento y devuelve la SceneDef resultante.
            El NavigationShell debe aplicar esta SceneDef para actualizar la UI. */
        SceneDef processEvent(const DirectorEvent& event);

        /** Consulta la escena actual. */
        SceneId getCurrentScene() const noexcept { return currentScene_; }

        /** Consulta la definición de la escena actual. */
        const SceneDef& getCurrentSceneDef() const noexcept { return currentSceneDef_; }

        /** Fuerza una transición (usado por tests y restauración). */
        SceneDef forceTransition(SceneId scene);

        /** Resetea a Welcome (nueva sesión). */
        SceneDef resetToWelcome();

        // ═══ Consultas ════════════════════════════════════════════════════

        /** Retorna true si la escena actual es pre-FullUI (solo chat). */
        bool isPreFullUI() const noexcept;

        /** Retorna true si la escena actual es de coaching activo. */
        bool isCoachingScene() const noexcept;

        /** Retorna true si la escena actual es de refinamiento. */
        bool isRefinementScene() const noexcept;

        /** Retorna el timeout de auto-return si aplica, o 0. */
        float getAutoReturnTimeout() const noexcept;

    private:
        // ═══ Transición de escenas ═══════════════════════════════════════

        struct TransitionRule {
            SceneId from;
            DirectorEvent::Type trigger;
            SceneId to;
        };

        static const std::vector<TransitionRule>& getTransitionTable();

        SceneId evaluateNextScene(const DirectorEvent& event) const;

        // ═══ Constructores de escenas ════════════════════════════════════

        /** Dispatch: construye la SceneDef para un SceneId dado. */
        SceneDef buildScene(SceneId id);

        SceneDef buildScene_Welcome();
        SceneDef buildScene_ModeSelection();
        SceneDef buildScene_GenreSelection();
        SceneDef buildScene_ReferenceLoad();
        SceneDef buildScene_ReferenceAnalysis();
        SceneDef buildScene_MixMapReview();
        SceneDef buildScene_SetupComplete();
        SceneDef buildScene_Coaching();
        SceneDef buildScene_ToolInFocus();
        SceneDef buildScene_GroupFocus();
        SceneDef buildScene_TrackFocus();
        SceneDef buildScene_Refinement();
        SceneDef buildScene_RefinementTool();
        SceneDef buildScene_SessionEnd();
        SceneDef buildScene_ReportView();

        // ═══ Estado ══════════════════════════════════════════════════════

        SceneId currentScene_{SceneId::Welcome};
        SceneDef currentSceneDef_;
        int64_t lastEventTimestamp_{0};
    };

} // namespace mixcoach
