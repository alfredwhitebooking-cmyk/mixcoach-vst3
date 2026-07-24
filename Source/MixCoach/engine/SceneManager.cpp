#include "SceneManager.h"
#include "../../Common/types/LogHelper.h"

namespace mixcoach {

    // =======================================================================
    //  Constructor — Inicializa la escena Welcome
    // =======================================================================
    SceneManager::SceneManager()
    {
        currentSceneDef_ = buildScene(SceneId::Welcome);
    }

    // =======================================================================
    //  Transition Table — Define qué transiciones son válidas
    //
    //  Cada regla: desde qué escena, con qué disparador, a qué escena.
    //  Cualquier transición no listada es ignorada.
    // =======================================================================
    const std::vector<SceneManager::TransitionRule>& SceneManager::getTransitionTable()
    {
        static const std::vector<TransitionRule> kRules = {
            // ─── Acto I: Descubrimiento ─────────────────────────────────
            { SceneId::Welcome,        DirectorEvent::Type::UserNameEntered,   SceneId::ModeSelection },
            { SceneId::Welcome,        DirectorEvent::Type::ReferenceLoaded,   SceneId::ReferenceLoad },
            { SceneId::Welcome,        DirectorEvent::Type::UserIdle,          SceneId::ModeSelection },

            { SceneId::ModeSelection,  DirectorEvent::Type::ModeSelected,      SceneId::GenreSelection },
            { SceneId::ModeSelection,  DirectorEvent::Type::ReferenceLoaded,   SceneId::ReferenceLoad },
            { SceneId::ModeSelection,  DirectorEvent::Type::CoachMessage,      SceneId::ModeSelection }, // stay
            { SceneId::ModeSelection,  DirectorEvent::Type::UserAskedHelp,     SceneId::ModeSelection }, // stay, give more help
            { SceneId::ModeSelection,  DirectorEvent::Type::Backward,          SceneId::Welcome },     // ← atrás: volver a nombre

            { SceneId::GenreSelection, DirectorEvent::Type::GenreSelected,     SceneId::ReferenceLoad },
            { SceneId::GenreSelection, DirectorEvent::Type::ReferenceLoaded,   SceneId::ReferenceLoad },
            { SceneId::GenreSelection, DirectorEvent::Type::CoachMessage,      SceneId::GenreSelection }, // stay
            { SceneId::GenreSelection, DirectorEvent::Type::UserAskedHelp,     SceneId::GenreSelection }, // stay, give more help
            { SceneId::GenreSelection, DirectorEvent::Type::Backward,          SceneId::ModeSelection }, // ← atrás: volver a modo

            { SceneId::ReferenceLoad, DirectorEvent::Type::ReferenceLoaded,       SceneId::ReferenceAnalysis },
            { SceneId::ReferenceLoad, DirectorEvent::Type::ReferenceSkipped,      SceneId::SetupComplete },
            { SceneId::ReferenceLoad, DirectorEvent::Type::UserAskedHelp,         SceneId::ReferenceLoad }, // explain again
            { SceneId::ReferenceLoad, DirectorEvent::Type::Backward,               SceneId::GenreSelection }, // ← atrás: volver a género

            // ─── Acto II: Setup ─────────────────────────────────────────
            { SceneId::ReferenceAnalysis, DirectorEvent::Type::ReferenceAnalyzed, SceneId::SetupComplete },
            { SceneId::ReferenceAnalysis, DirectorEvent::Type::MixMapConfirmed,   SceneId::SetupComplete },
            { SceneId::ReferenceAnalysis, DirectorEvent::Type::Backward,          SceneId::ReferenceLoad }, // ← atrás: volver a carga de referencia

            { SceneId::SetupComplete,     DirectorEvent::Type::SessionPrepped,    SceneId::MixMapReview },
            { SceneId::SetupComplete,     DirectorEvent::Type::MixMapConfirmed,   SceneId::Coaching },
            { SceneId::SetupComplete,     DirectorEvent::Type::Backward,          SceneId::ReferenceAnalysis }, // ← atrás: volver al análisis

            { SceneId::MixMapReview,      DirectorEvent::Type::SessionPrepped,   SceneId::MixMapReview }, // stay
            { SceneId::MixMapReview,      DirectorEvent::Type::MixMapConfirmed,  SceneId::Coaching },
            { SceneId::MixMapReview,      DirectorEvent::Type::UserAskedHelp,    SceneId::MixMapReview },
            { SceneId::MixMapReview,      DirectorEvent::Type::Backward,          SceneId::SetupComplete }, // ← atrás: volver a setup

            // ─── Acto III: Coaching Activo ──────────────────────────────
            { SceneId::Coaching,     DirectorEvent::Type::PhaseChanged,      SceneId::Coaching },     // stay, new phase
            { SceneId::Coaching,     DirectorEvent::Type::CoachCommand,      SceneId::ToolInFocus },  // LLM switches to tools
            { SceneId::Coaching,     DirectorEvent::Type::MixScoreChanged,   SceneId::Refinement },   // score >= 70
            { SceneId::Coaching,     DirectorEvent::Type::CorrectionApplied, SceneId::Coaching },     // stay, continue
            { SceneId::Coaching,     DirectorEvent::Type::UserAskedHelp,     SceneId::Coaching },     // stay, give hint

            { SceneId::ToolInFocus,  DirectorEvent::Type::UserInteracted,   SceneId::ToolInFocus },  // restart timer
            { SceneId::ToolInFocus,  DirectorEvent::Type::TimerTick,         SceneId::Coaching },    // auto-return
            { SceneId::ToolInFocus,  DirectorEvent::Type::CoachCommand,      SceneId::Coaching },    // return_to_coach

            // --- GroupFocus / TrackFocus (SCENE 9) ---
            { SceneId::Coaching,     DirectorEvent::Type::FocusGroupBus,    SceneId::GroupFocus },  // Focus DRUMS/Bass/Vocals
            { SceneId::Coaching,     DirectorEvent::Type::FocusTrack,       SceneId::TrackFocus },  // Focus a specific track
            { SceneId::Coaching,     DirectorEvent::Type::FocusDismissed,   SceneId::Coaching },    // dismiss while in coaching (safety)

            { SceneId::GroupFocus,   DirectorEvent::Type::FocusTrack,       SceneId::TrackFocus },  // switch from group to track
            { SceneId::GroupFocus,   DirectorEvent::Type::FocusGroupBus,    SceneId::GroupFocus },  // stay, new group
            { SceneId::GroupFocus,   DirectorEvent::Type::FocusDismissed,   SceneId::Coaching },    // dismiss back to coaching
            { SceneId::GroupFocus,   DirectorEvent::Type::UserInteracted,   SceneId::GroupFocus },  // restart auto-return
            { SceneId::GroupFocus,   DirectorEvent::Type::TimerTick,         SceneId::Coaching },   // auto-return
            { SceneId::GroupFocus,   DirectorEvent::Type::CoachCommand,      SceneId::Coaching },   // return_to_coach

            { SceneId::TrackFocus,   DirectorEvent::Type::FocusGroupBus,    SceneId::GroupFocus },  // switch from track to group
            { SceneId::TrackFocus,   DirectorEvent::Type::FocusTrack,       SceneId::TrackFocus },  // stay, new track
            { SceneId::TrackFocus,   DirectorEvent::Type::FocusDismissed,   SceneId::Coaching },    // dismiss back to coaching
            { SceneId::TrackFocus,   DirectorEvent::Type::UserInteracted,   SceneId::TrackFocus },  // restart auto-return
            { SceneId::TrackFocus,   DirectorEvent::Type::TimerTick,         SceneId::Coaching },   // auto-return
            { SceneId::TrackFocus,   DirectorEvent::Type::CoachCommand,      SceneId::Coaching },   // return_to_coach

            // ─── Acto IV: Refinamiento ──────────────────────────────────
            { SceneId::Refinement,  DirectorEvent::Type::CoachCommand,      SceneId::RefinementTool },
            { SceneId::Refinement,  DirectorEvent::Type::PhaseChanged,      SceneId::SessionEnd },
            { SceneId::Refinement,  DirectorEvent::Type::CorrectionApplied, SceneId::Refinement },   // stay, continue
            { SceneId::Refinement,  DirectorEvent::Type::MixScoreChanged,   SceneId::Coaching },     // fell below 70

            { SceneId::RefinementTool, DirectorEvent::Type::TimerTick,       SceneId::Refinement },  // auto-return
            { SceneId::RefinementTool, DirectorEvent::Type::UserInteracted, SceneId::RefinementTool }, // restart timer

            // ─── Acto V: Cierre ─────────────────────────────────────────
            { SceneId::SessionEnd, DirectorEvent::Type::CoachMessage,       SceneId::ReportView },
            { SceneId::SessionEnd, DirectorEvent::Type::UserIdle,           SceneId::SessionEnd },  // stay, prompt for report

            { SceneId::ReportView, DirectorEvent::Type::CoachMessage,       SceneId::Welcome },     // new session
        };

        return kRules;
    }

    // =======================================================================
    //  Scene Builders — Cada escena define qué paneles están visibles
    // =======================================================================

    SceneDef SceneManager::buildScene_Welcome()
    {
        SceneDef def;
        def.id = SceneId::Welcome;
        def.name = "Welcome";
        def.coachRoomState = CoachRoomState::Welcome;
        def.coachMessage = "";  // WelcomeComponent maneja su propio mensaje
        def.sidebarEnabled = false;
        def.animate = false;

        // Solo chat visible
        def.panels = {
            { PanelId::Coach,     PanelVisibility::Focused, 0.0f },
            { PanelId::Reference, PanelVisibility::Hidden,  1.0f },
            { PanelId::Messengers,PanelVisibility::Hidden,  1.0f },
            { PanelId::MixMap,    PanelVisibility::Hidden,  1.0f },
            { PanelId::Tools,     PanelVisibility::Hidden,  1.0f },
            { PanelId::Session,   PanelVisibility::Hidden,  1.0f },
            { PanelId::Report,    PanelVisibility::Hidden,  1.0f },
        };
        return def;
    }

    SceneDef SceneManager::buildScene_ModeSelection()
    {
        SceneDef def;
        def.id = SceneId::ModeSelection;
        def.name = "Elegir Modo";
        def.coachRoomState = CoachRoomState::Intention;
        def.coachMessage = "[WAVE] " + juce::String(juce::CharPointer_UTF8("\xC2\xBF")) + juce::String(juce::CharPointer_UTF8("Qu\xC3\xA9")) + " vamos a hacer hoy?";
        def.sidebarEnabled = false;
        def.animate = true;

        def.panels = {
            { PanelId::Coach,     PanelVisibility::Focused, 0.0f },
            { PanelId::Reference, PanelVisibility::Hidden,  1.0f },
            { PanelId::Messengers,PanelVisibility::Hidden,  1.0f },
            { PanelId::MixMap,    PanelVisibility::Hidden,  1.0f },
            { PanelId::Tools,     PanelVisibility::Hidden,  1.0f },
            { PanelId::Session,   PanelVisibility::Hidden,  1.0f },
            { PanelId::Report,    PanelVisibility::Hidden,  1.0f },
        };
        return def;
    }

    SceneDef SceneManager::buildScene_GenreSelection()
    {
        SceneDef def;
        def.id = SceneId::GenreSelection;
        def.name = "Elegir G\u00e9nero";
        def.coachRoomState = CoachRoomState::Genre;
        def.coachMessage = "\xE2\x9C\xA8 " + juce::String(juce::CharPointer_UTF8("\xC2\xBF")) + "Y qu" + juce::String(juce::CharPointer_UTF8("\xC3\xA9")) + " g" + juce::String(juce::CharPointer_UTF8("\xC3\xA9")) + "nero vas a mezclar?";
        def.sidebarEnabled = false;
        def.animate = true;

        def.panels = {
            { PanelId::Coach,     PanelVisibility::Focused, 0.0f },
            { PanelId::Reference, PanelVisibility::Hidden,  1.0f },
            { PanelId::Messengers,PanelVisibility::Hidden,  1.0f },
            { PanelId::MixMap,    PanelVisibility::Hidden,  1.0f },
            { PanelId::Tools,     PanelVisibility::Hidden,  1.0f },
            { PanelId::Session,   PanelVisibility::Hidden,  1.0f },
            { PanelId::Report,    PanelVisibility::Hidden,  1.0f },
        };
        return def;
    }

    SceneDef SceneManager::buildScene_ReferenceLoad()
    {
        SceneDef def;
        def.id = SceneId::ReferenceLoad;
        def.name = "Cargar Referencia";
        def.coachRoomState = CoachRoomState::ReferenceStage;
        def.coachMessage = "";  // El mensaje se postea desde NavigationShell con auto-advance
        def.sidebarEnabled = false;
        def.animate = true;

        def.panels = {
            { PanelId::Coach,     PanelVisibility::Focused,   0.0f },
            { PanelId::Reference, PanelVisibility::Visible,   0.3f, "Drop zone", true },
            { PanelId::Messengers,PanelVisibility::Hidden,    1.0f },
            { PanelId::MixMap,    PanelVisibility::Hidden,    1.0f },
            { PanelId::Tools,     PanelVisibility::Hidden,    1.0f },
            { PanelId::Session,   PanelVisibility::Hidden,    1.0f },
            { PanelId::Report,    PanelVisibility::Hidden,    1.0f },
        };
        return def;
    }

    SceneDef SceneManager::buildScene_ReferenceAnalysis()
    {
        SceneDef def;
        def.id = SceneId::ReferenceAnalysis;
        def.name = "Analizando Referencia";
        def.coachRoomState = CoachRoomState::ReferenceStage;
        def.coachMessage = "He analizado tu referencia. No vamos a copiarla. "
                           "Vamos a entender por qué funciona.";
        def.sidebarEnabled = false;
        def.animate = true;

        def.panels = {
            { PanelId::Coach,     PanelVisibility::Focused,   0.0f, "Análisis completo" },
            { PanelId::Reference, PanelVisibility::Visible,   0.3f, "Resultados", true },
            { PanelId::Messengers,PanelVisibility::Hidden,    1.0f },
            { PanelId::MixMap,    PanelVisibility::Hidden,    1.0f },
            { PanelId::Tools,     PanelVisibility::Hidden,    1.0f },
            { PanelId::Session,   PanelVisibility::Hidden,    1.0f },
            { PanelId::Report,    PanelVisibility::Hidden,    1.0f },
        };
        return def;
    }

    SceneDef SceneManager::buildScene_MixMapReview()
    {
        SceneDef def;
        def.id = SceneId::MixMapReview;
        def.name = "Mapa de Mezcla";
        def.coachRoomState = CoachRoomState::MixMapStage;
        def.coachMessage = "Mapa de mezcla listo. He organizado tu sesión. "
                           "Revisa el mapa y confírmalo para empezar.";
        def.sidebarEnabled = false;
        def.animate = true;

        def.panels = {
            { PanelId::Coach,     PanelVisibility::Focused,   0.0f, "Confirma el mapa" },
            { PanelId::Reference, PanelVisibility::Visible,   0.3f, "Referencia" },
            { PanelId::Messengers,PanelVisibility::Visible,   0.3f, "Tracks detectados", true },
            { PanelId::MixMap,    PanelVisibility::Visible,   0.5f, "Routing", true },
            { PanelId::Tools,     PanelVisibility::Hidden,    1.0f },
            { PanelId::Session,   PanelVisibility::Hidden,    1.0f },
            { PanelId::Report,    PanelVisibility::Hidden,    1.0f },
        };
        return def;
    }

    SceneDef SceneManager::buildScene_SetupComplete()
    {
        SceneDef def;
        def.id = SceneId::SetupComplete;
        def.name = "Setup Completo";
        def.coachRoomState = CoachRoomState::SessionPrep;
        def.coachMessage = "";  // Cada callback postea su propio mensaje personalizado
        def.celebrate = true;
        def.achievement = "¡Setup completado!";
        def.sidebarEnabled = false;  // SessionPrep es pre-FullUI, sin tab bar
        def.animate = true;

        def.panels = {
            { PanelId::Coach,     PanelVisibility::Focused,   0.0f, "¡A mezclar!" },
            // El cierre del setup conserva una sola tarea visible: confirmar que
            // estamos listos. Los paneles técnicos se revelan al entrar al loop de
            // coaching, no mientras el usuario todavía completa la preparación.
            { PanelId::Reference, PanelVisibility::Hidden,    1.0f },
            { PanelId::Messengers,PanelVisibility::Hidden,    1.0f },
            { PanelId::MixMap,    PanelVisibility::Hidden,    1.0f },
            { PanelId::Tools,     PanelVisibility::Hidden,    1.0f },
            { PanelId::Session,   PanelVisibility::Hidden,    1.0f },
            { PanelId::Report,    PanelVisibility::Hidden,    1.0f },
        };
        return def;
    }

    SceneDef SceneManager::buildScene_Coaching()
    {
        SceneDef def;
        def.id = SceneId::Coaching;
        def.name = "Coaching";
        def.coachMessage = "Bien. Escuchemos y trabajemos paso a paso.";
        def.sidebarEnabled = true;
        def.animate = false;    // Sin transición visual para el loop principal

        def.panels = {
            { PanelId::Coach,     PanelVisibility::Focused,   0.0f, "Chat activo" },
            { PanelId::Reference, PanelVisibility::Visible,   0.2f, "Referencia" },
            { PanelId::Messengers,PanelVisibility::Visible,   0.2f, "Tracks" },
            { PanelId::MixMap,    PanelVisibility::Hidden,    0.5f },
            { PanelId::Tools,     PanelVisibility::Visible,   0.3f, "Analizadores" },
            { PanelId::Session,   PanelVisibility::Hidden,    1.0f },
            { PanelId::Report,    PanelVisibility::Hidden,    1.0f },
        };
        return def;
    }

    SceneDef SceneManager::buildScene_ToolInFocus()
    {
        SceneDef def;
        def.id = SceneId::ToolInFocus;
        def.name = "Herramienta Activa";
        def.coachMessage = "";
        def.sidebarEnabled = true;
        def.autoReturnTimeout = 5.0f;   // Auto-return en 5s
        def.animate = true;

        def.panels = {
            { PanelId::Coach,     PanelVisibility::Visible,   0.0f, "Chat — volveré en 5s" },
            { PanelId::Reference, PanelVisibility::Hidden,    0.3f },
            { PanelId::Messengers,PanelVisibility::Visible,   0.2f, "Tracks" },
            { PanelId::MixMap,    PanelVisibility::Hidden,    0.5f },
            { PanelId::Tools,     PanelVisibility::Focused,   0.5f, "🔍 Herramienta activa" },
            { PanelId::Session,   PanelVisibility::Hidden,    1.0f },
            { PanelId::Report,    PanelVisibility::Hidden,    1.0f },
        };
        return def;
    }

    SceneDef SceneManager::buildScene_Refinement()
    {
        SceneDef def;
        def.id = SceneId::Refinement;
        def.name = "Refinamiento";
        def.coachMessage = "Tu mezcla suena sólida. Hablemos de calidad artística. "
                           "Podemos trabajar en profundidad, impacto, movimiento...";
        def.celebrate = true;
        def.achievement = "🎨 ¡Has llegado a refinamiento artístico!";
        def.sidebarEnabled = true;
        def.animate = true;

        def.panels = {
            { PanelId::Coach,     PanelVisibility::Focused,   0.0f, "Refinamiento activo" },
            { PanelId::Reference, PanelVisibility::Visible,   0.2f, "Referencia" },
            { PanelId::Messengers,PanelVisibility::Visible,   0.2f, "Tracks" },
            { PanelId::MixMap,    PanelVisibility::Hidden,    0.5f },
            { PanelId::Tools,     PanelVisibility::Visible,   0.3f, "Analizadores" },
            { PanelId::Session,   PanelVisibility::Visible,   0.2f, "Progreso" },
            { PanelId::Report,    PanelVisibility::Hidden,    1.0f },
        };
        return def;
    }


    SceneDef SceneManager::buildScene_GroupFocus()
    {
        SceneDef def;
        def.id = SceneId::GroupFocus;
        def.name = "Enfoque: Grupo";
        def.coachMessage = "";
        def.sidebarEnabled = true;
        def.autoReturnTimeout = 8.0f;
        def.animate = true;

        def.panels = {
            { PanelId::Coach,     PanelVisibility::Visible,   0.0f, "Chat - enfoque activo" },
            { PanelId::Reference, PanelVisibility::Hidden,    0.3f },
            { PanelId::Messengers,PanelVisibility::Visible,   0.2f, "Tracks" },
            { PanelId::MixMap,    PanelVisibility::Focused,   0.5f, "GRUPO ENFOQUE" },
            { PanelId::Tools,     PanelVisibility::Hidden,    0.3f },
            { PanelId::Session,   PanelVisibility::Hidden,    0.2f },
            { PanelId::Report,    PanelVisibility::Hidden,    1.0f },
        };
        return def;
    }

    SceneDef SceneManager::buildScene_TrackFocus()
    {
        SceneDef def;
        def.id = SceneId::TrackFocus;
        def.name = "Enfoque: Pista";
        def.coachMessage = "";
        def.sidebarEnabled = true;
        def.autoReturnTimeout = 8.0f;
        def.animate = true;

        def.panels = {
            { PanelId::Coach,     PanelVisibility::Visible,   0.0f, "Chat - enfoque activo" },
            { PanelId::Reference, PanelVisibility::Hidden,    0.3f },
            { PanelId::Messengers,PanelVisibility::Visible,   0.2f, "Tracks" },
            { PanelId::MixMap,    PanelVisibility::Focused,   0.5f, "PISTA ENFOQUE" },
            { PanelId::Tools,     PanelVisibility::Hidden,    0.3f },
            { PanelId::Session,   PanelVisibility::Hidden,    0.2f },
            { PanelId::Report,    PanelVisibility::Hidden,    1.0f },
        };
        return def;
    }

    SceneDef SceneManager::buildScene_RefinementTool()
    {
        SceneDef def;
        def.id = SceneId::RefinementTool;
        def.name = "Herramienta de Refinamiento";
        def.coachMessage = "";
        def.sidebarEnabled = true;
        def.autoReturnTimeout = 5.0f;
        def.animate = true;

        def.panels = {
            { PanelId::Coach,     PanelVisibility::Visible,   0.0f, "Chat — volveré en 5s" },
            { PanelId::Reference, PanelVisibility::Visible,   0.2f, "Referencia" },
            { PanelId::Messengers,PanelVisibility::Visible,   0.2f, "Tracks" },
            { PanelId::MixMap,    PanelVisibility::Hidden,    0.5f },
            { PanelId::Tools,     PanelVisibility::Focused,   0.5f, "🔍 Herramienta activa" },
            { PanelId::Session,   PanelVisibility::Hidden,    1.0f },
            { PanelId::Report,    PanelVisibility::Hidden,    1.0f },
        };
        return def;
    }

    SceneDef SceneManager::buildScene_SessionEnd()
    {
        SceneDef def;
        def.id = SceneId::SessionEnd;
        def.name = "Fin de Sesión";
        def.coachMessage = "Hemos recorrido un gran camino juntos. "
                           "¿Quieres generar el reporte de tu sesión?";
        def.celebrate = true;
        def.achievement = "🏁 ¡Sesión completada!";
        def.sidebarEnabled = false;
        def.animate = true;

        def.panels = {
            { PanelId::Coach,     PanelVisibility::Focused,   0.0f, "Sesión completada" },
            { PanelId::Reference, PanelVisibility::Hidden,    0.3f },
            { PanelId::Messengers,PanelVisibility::Hidden,    0.2f },
            { PanelId::MixMap,    PanelVisibility::Hidden,    0.5f },
            { PanelId::Tools,     PanelVisibility::Hidden,    0.3f },
            { PanelId::Session,   PanelVisibility::Hidden,    0.2f },
            { PanelId::Report,    PanelVisibility::Visible,   0.3f, "Reporte disponible", true },
        };
        return def;
    }

    SceneDef SceneManager::buildScene_ReportView()
    {
        SceneDef def;
        def.id = SceneId::ReportView;
        def.name = "Reporte";
        def.coachMessage = "Aquí tienes el resumen de tu sesión. "
                           "Puedes exportarlo o empezar una nueva.";
        def.sidebarEnabled = false;
        def.animate = true;

        def.panels = {
            { PanelId::Coach,     PanelVisibility::Visible,   0.0f, "Chat" },
            { PanelId::Reference, PanelVisibility::Hidden,    0.3f },
            { PanelId::Messengers,PanelVisibility::Hidden,    0.2f },
            { PanelId::MixMap,    PanelVisibility::Hidden,    0.5f },
            { PanelId::Tools,     PanelVisibility::Hidden,    0.3f },
            { PanelId::Session,   PanelVisibility::Hidden,    0.2f },
            { PanelId::Report,    PanelVisibility::Focused,   0.5f, "📄 Reporte final" },
        };
        return def;
    }

    // =======================================================================
    //  SceneBuilder dispatch
    // =======================================================================
    SceneDef SceneManager::buildScene(SceneId id)
    {
        switch (id) {
            case SceneId::Welcome:           return buildScene_Welcome();
            case SceneId::ModeSelection:     return buildScene_ModeSelection();
            case SceneId::GenreSelection:    return buildScene_GenreSelection();
            case SceneId::ReferenceLoad:     return buildScene_ReferenceLoad();
            case SceneId::ReferenceAnalysis: return buildScene_ReferenceAnalysis();
            case SceneId::MixMapReview:      return buildScene_MixMapReview();
            case SceneId::SetupComplete:     return buildScene_SetupComplete();
            case SceneId::Coaching:          return buildScene_Coaching();
            case SceneId::ToolInFocus:       return buildScene_ToolInFocus();
            case SceneId::GroupFocus:        return buildScene_GroupFocus();
            case SceneId::TrackFocus:        return buildScene_TrackFocus();
            case SceneId::Refinement:        return buildScene_Refinement();
            case SceneId::RefinementTool:    return buildScene_RefinementTool();
            case SceneId::SessionEnd:        return buildScene_SessionEnd();
            case SceneId::ReportView:        return buildScene_ReportView();
            default:                         return buildScene_Welcome();
        }
    }

    // =======================================================================
    //  Evaluate next scene based on transition table
    // =======================================================================
    SceneId SceneManager::evaluateNextScene(const DirectorEvent& event) const
    {
        const auto& rules = getTransitionTable();

        for (const auto& rule : rules) {
            if (rule.from == currentScene_ && rule.trigger == event.type) {
                return rule.to;
            }
        }

        return currentScene_;   // No matching rule → stay
    }

    // =======================================================================
    //  processEvent — Main entry point
    // =======================================================================
    SceneDef SceneManager::processEvent(const DirectorEvent& event)
    {
        lastEventTimestamp_ = event.timestamp;

        SceneId nextScene = evaluateNextScene(event);

        if (nextScene == currentScene_) {
            // No change — return current scene def unchanged
            return currentSceneDef_;
        }

        // ─── Transition! ───────────────────────────────────────────────
        LogHelper::writeToLog("[SceneManager] "
                              + juce::String(sceneIdLabel(currentScene_))
                              + " → "
                              + juce::String(sceneIdLabel(nextScene))
                              + " (trigger: "
                              + juce::String(directorEventTypeLabel(event.type))
                              + ")");

        currentScene_ = nextScene;
        currentSceneDef_ = buildScene(nextScene);

        // Propagate context from event if applicable
        if (!event.payload.isEmpty()) {
            // For ToolInFocus, store what tool/panel is active
            if (currentScene_ == SceneId::ToolInFocus 
                || currentScene_ == SceneId::RefinementTool
                || currentScene_ == SceneId::GroupFocus
                || currentScene_ == SceneId::TrackFocus) {
                for (auto& panel : currentSceneDef_.panels) {
                    if (panel.visibility == PanelVisibility::Focused) {
                        panel.context = event.payload;
                    }
                }
            }
        }

        return currentSceneDef_;
    }

    // =======================================================================
    //  forceTransition — Forzar cambio de escena (tests / restore)
    // =======================================================================
    SceneDef SceneManager::forceTransition(SceneId scene)
    {
        if (scene == currentScene_) return currentSceneDef_;

        LogHelper::writeToLog("[SceneManager] FORCE transition: "
                              + juce::String(sceneIdLabel(currentScene_))
                              + " → "
                              + juce::String(sceneIdLabel(scene)));

        currentScene_ = scene;
        currentSceneDef_ = buildScene(scene);
        return currentSceneDef_;
    }

    // =======================================================================
    //  resetToWelcome
    // =======================================================================
    SceneDef SceneManager::resetToWelcome()
    {
        return forceTransition(SceneId::Welcome);
    }

    // =======================================================================
    //  Query helpers
    // =======================================================================

    bool SceneManager::isPreFullUI() const noexcept
    {
        return currentScene_ == SceneId::Welcome
            || currentScene_ == SceneId::ModeSelection
            || currentScene_ == SceneId::GenreSelection
            || currentScene_ == SceneId::ReferenceLoad
            || currentScene_ == SceneId::ReferenceAnalysis
            || currentScene_ == SceneId::MixMapReview
            || currentScene_ == SceneId::SetupComplete;
    }

    bool SceneManager::isCoachingScene() const noexcept
    {
        return currentScene_ == SceneId::Coaching
            || currentScene_ == SceneId::ToolInFocus
            || currentScene_ == SceneId::GroupFocus
            || currentScene_ == SceneId::TrackFocus;
    }

    bool SceneManager::isRefinementScene() const noexcept
    {
        return currentScene_ == SceneId::Refinement
            || currentScene_ == SceneId::RefinementTool;
    }

    float SceneManager::getAutoReturnTimeout() const noexcept
    {
        return currentSceneDef_.autoReturnTimeout;
    }

} // namespace mixcoach
