#include "ExperienceManager.h"
#include "../UI/NavigationShell.h"
#include "../UI/MixBotComponent.h"
#include "../../Common/types/LogHelper.h"

namespace mixcoach {

    // =======================================================================
    //  Constructor
    // =======================================================================
    ExperienceManager::ExperienceManager(NavigationShell& navShell, PanelRevealManager& revealManager)
        : navShell_(navShell)
        , revealManager_(revealManager)
    {
    }

    // =======================================================================
    //  wireToEngine — Conecta todos los callbacks al CoachEngine
    // =======================================================================
    void ExperienceManager::wireToEngine(CoachEngine& engine)
    {
        // Si ya estaba conectado al mismo engine, no hacer nada
        if (isWired_ && wiredEngine_ == &engine) return;

        // Desconectar engine anterior si existe
        if (isWired_ && wiredEngine_ != nullptr) {
            unwireFromEngine(*wiredEngine_);
        }

        wiredEngine_ = &engine;
        isWired_ = true;

        // ─── SetupStep callback ──────────────────────────────────────────
        setupStepCb_ = [this](CoachEngine::SetupStep oldStep, CoachEngine::SetupStep newStep) {
            onSetupStepChanged(oldStep, newStep);
        };
        engine.setSetupStepChangedCallback(setupStepCb_);

        // ─── CoachingStage callback ──────────────────────────────────────
        stageChangedCb_ = [this](CoachingStage oldStage, CoachingStage newStage) {
            onCoachingStageChanged(oldStage, newStage);
        };
        engine.setStageChangedCallback(stageChangedCb_);

        // ─── SessionProgression callback ─────────────────────────────────
        sessionProgCb_ = [this](SessionProgression::Phase oldPhase, SessionProgression::Phase newPhase) {
            onSessionProgressionChanged(oldPhase, newPhase);
        };
        engine.getSessionProgression().onPhaseChanged = sessionProgCb_;

        // ─── CriticalIssue callback ────────────────────────────────────
        criticalIssueCb_ = [this](bool hasClipping, int clippingCount) {
            onCriticalIssueDetected(hasClipping, clippingCount);
        };
        engine.setCriticalIssueCallback(criticalIssueCb_);

        // ─── WorkflowEvent callback (plugin changes) ─────────────────────
        workflowEventCb_ = [this](const WorkflowEvent& event) {
            onWorkflowEvent(event);
        };
        engine.setWorkflowEventCallback(workflowEventCb_);

        // ═══ Paso 3.2: DirectorEvent callback — SceneManager transiciones ═══
        directorEventCb_ = [this](const DirectorEvent& event) {
            navShell_.processDirectorEvent(event);
        };
        engine.setDirectorEventCallback(directorEventCb_);

        LogHelper::writeToLog("[ExperienceManager] Wired to CoachEngine");
    }

    // =======================================================================
    //  unwireFromEngine — Desconecta todos los callbacks
    // =======================================================================
    void ExperienceManager::unwireFromEngine(CoachEngine& engine)
    {
        engine.setSetupStepChangedCallback(nullptr);
        engine.setStageChangedCallback(nullptr);
        engine.getSessionProgression().onPhaseChanged = nullptr;
        engine.setCriticalIssueCallback(nullptr);
        engine.setWorkflowEventCallback(nullptr);
        engine.setDirectorEventCallback(nullptr);

        setupStepCb_ = nullptr;
        stageChangedCb_ = nullptr;
        sessionProgCb_ = nullptr;
        criticalIssueCb_ = nullptr;
        workflowEventCb_ = nullptr;
        directorEventCb_ = nullptr;

        wiredEngine_ = nullptr;
        isWired_ = false;

        LogHelper::writeToLog("[ExperienceManager] Unwired from CoachEngine");
    }

    // =======================================================================
    //  onSetupStepChanged — Mapea SetupStep a acciones UI
    // =======================================================================
    void ExperienceManager::onSetupStepChanged(CoachEngine::SetupStep oldStep, CoachEngine::SetupStep newStep)
    {
        juce::ignoreUnused(oldStep);

        LogHelper::writeToLog("[ExperienceManager] SetupStep: "
                              + juce::String(static_cast<int>(oldStep))
                              + " [RIGHT] " + juce::String(static_cast<int>(newStep)));

        switch (newStep) {
            case CoachEngine::SetupStep::NotStarted:
                navShell_.setAvatarExpression(AvatarExpression::Neutral);
                break;

            case CoachEngine::SetupStep::WaitingForName:
                // Preguntando nombre → Intention — expresión pensativa
                navShell_.setAvatarExpression(AvatarExpression::Thinking);
                if (navShell_.getCoachRoomState() == CoachRoomState::Welcome) {
                    navShell_.setCoachRoomState(CoachRoomState::Intention);
                }
                break;

            case CoachEngine::SetupStep::WaitingForMode:
                // P2: Saludo con la mano al entrar a selección de modo
                navShell_.setAvatarWave(true);
                if (navShell_.getCoachRoomState() == CoachRoomState::Welcome) {
                    navShell_.setCoachRoomState(CoachRoomState::Intention);
                }
                break;

            case CoachEngine::SetupStep::WaitingForReferenceFirst:
                // ═══ V6: Referencia ANTES que género → ReferenceStage (NO Genre)
                navShell_.setAvatarExpression(AvatarExpression::Encouraging);
                if (navShell_.getCoachRoomState() == CoachRoomState::Intention
                    || navShell_.getCoachRoomState() == CoachRoomState::Welcome) {
                    navShell_.setCoachRoomState(CoachRoomState::ReferenceStage);
                }
                // Revelar panel de referencia
                if (!revealManager_.isPanelRevealed(PanelId::Reference)) {
                    navShell_.revealPanel(PanelId::Reference);
                }
                break;

            case CoachEngine::SetupStep::WaitingForGenre:
                // Preguntando género → Genre — expresión pensativa
                navShell_.setAvatarExpression(AvatarExpression::Thinking);
                if (navShell_.getCoachRoomState() == CoachRoomState::Intention
                    || navShell_.getCoachRoomState() == CoachRoomState::Welcome) {
                    navShell_.setCoachRoomState(CoachRoomState::Genre);
                }
                break;

            case CoachEngine::SetupStep::WaitingForConfirm:
                // Confirmando setup → expresión de ánimo
                navShell_.setAvatarExpression(AvatarExpression::Encouraging);
                if (!revealManager_.isPanelRevealed(PanelId::Reference)) {
                    navShell_.revealPanel(PanelId::Reference);
                }
                break;

            case CoachEngine::SetupStep::WaitingForReference:
                // Preguntando por referencia → ReferenceStage — expresión de ánimo
                navShell_.setAvatarExpression(AvatarExpression::Encouraging);
                if (!revealManager_.isPanelRevealed(PanelId::Reference)) {
                    navShell_.revealPanel(PanelId::Reference);
                }
                break;

            case CoachEngine::SetupStep::WaitingForSetupInstructions:
                // Pide insertar Messengers → MessengerStage — expresión de ánimo
                navShell_.setAvatarExpression(AvatarExpression::Encouraging);
                if (!revealManager_.isPanelRevealed(PanelId::Messengers)) {
                    navShell_.revealPanel(PanelId::Messengers);
                }
                break;

            case CoachEngine::SetupStep::WaitingForDestination:
                // Master Mode — expresión pensativa
                navShell_.setAvatarExpression(AvatarExpression::Thinking);
                break;

            case CoachEngine::SetupStep::Complete:
                // Setup completo → Surprised ("\xBFYa terminamos el setup?")
                navShell_.setAvatarExpression(AvatarExpression::Surprised);
                if (navShell_.getCoachRoomState() == CoachRoomState::MessengerStage
                    || navShell_.getCoachRoomState() == CoachRoomState::MixMapStage) {
                    if (!revealManager_.isPanelRevealed(PanelId::MixMap)) {
                        navShell_.revealPanel(PanelId::MixMap);
                    }
                } else if (navShell_.getCoachRoomState() == CoachRoomState::Welcome
                    || navShell_.getCoachRoomState() == CoachRoomState::Intention
                    || navShell_.getCoachRoomState() == CoachRoomState::Genre) {
                    navShell_.setCoachRoomState(CoachRoomState::GainStaging);
                }
                break;
        }
    }

    // =======================================================================
    //  onCoachingStageChanged — Mapea cambios de etapa de coaching a UI
    //  auto-open del analyzer relevante seg\xFAr la etapa
    // =======================================================================
    void ExperienceManager::onCoachingStageChanged(CoachingStage oldStage, CoachingStage newStage)
    {
        juce::ignoreUnused(oldStage);

        LogHelper::writeToLog("[ExperienceManager] CoachingStage: "
                              + juce::String(static_cast<int>(oldStage))
                              + " [RIGHT] " + juce::String(static_cast<int>(newStage)));

        // ═══ Resetear toggles de analyzers para la nueva etapa ═══════════
        auto& analyzers = navShell_.getAnalyzersPanel();
        analyzers.setShowCrest(false);
        analyzers.setShowWidth(false);
        analyzers.setShowDNA(false);

        // ═══ Asegurar que Tools tab est\xE9 desbloqueado ══════════════════
        if (!revealManager_.isPanelRevealed(PanelId::Tools)) {
            navShell_.revealPanel(PanelId::Tools);
        }

        // ═══ Mapear CoachingStage → CoachRoomState + auto-open analyzer ═══
        switch (newStage) {
            case CoachingStage::GainStaging:
                navShell_.setCoachRoomState(CoachRoomState::GainStaging);
                navShell_.setAvatarExpression(AvatarExpression::Encouraging);
                // ▶ CREST: mostrar dyn\xE1mica (peak vs RMS) para gain staging
                analyzers.setShowCrest(true);
                navShell_.setActiveTab(TabBarComponent::Tools);
                navShell_.startAutoReturn();
                break;

            case CoachingStage::Balance:
                navShell_.setCoachRoomState(CoachRoomState::Balance);
                navShell_.setAvatarExpression(AvatarExpression::Thinking);
                // ▶ WIDTH: ancho est\xE9reo para balance de paneo
                analyzers.setShowWidth(true);
                navShell_.setActiveTab(TabBarComponent::Tools);
                navShell_.startAutoReturn();
                break;

            case CoachingStage::EQ:
                navShell_.setCoachRoomState(CoachRoomState::EQ);
                navShell_.setAvatarExpression(AvatarExpression::Thinking);
                // ▶ Spectrograph: el analyzer principal ya se muestra en Tools
                // No necesita toggle extra porque la spectrograph siempre est\xE1 visible
                navShell_.setActiveTab(TabBarComponent::Tools);
                navShell_.startAutoReturn();
                break;

            case CoachingStage::Compression:
                navShell_.setCoachRoomState(CoachRoomState::Compression);
                navShell_.setAvatarExpression(AvatarExpression::Thinking);
                // ▶ CREST: crest factor para ver compresi\xF3n
                analyzers.setShowCrest(true);
                navShell_.setActiveTab(TabBarComponent::Tools);
                navShell_.startAutoReturn();
                break;

            case CoachingStage::Spatial:
                navShell_.setCoachRoomState(CoachRoomState::Space);
                navShell_.setAvatarExpression(AvatarExpression::Encouraging);
                // ▶ WIDTH + PhaseScope: ancho est\xE9reo y correlaci\xF3n de fase
                analyzers.setShowWidth(true);
                navShell_.setActiveTab(TabBarComponent::Tools);
                navShell_.startAutoReturn();
                break;

            case CoachingStage::Refinement:
                navShell_.setCoachRoomState(CoachRoomState::Refinement);
                navShell_.setAvatarExpression(AvatarExpression::Surprised);
                // ▶ DNA: perfil espectral general para visi\xF3n completa
                analyzers.setShowDNA(true);
                navShell_.setActiveTab(TabBarComponent::Tools);
                navShell_.startAutoReturn();
                break;
        }
    }

    // =======================================================================
    //  onSessionProgressionChanged — Mapea cambios de progresión a UI
    // =======================================================================
    void ExperienceManager::onSessionProgressionChanged(SessionProgression::Phase oldPhase,
                                                         SessionProgression::Phase newPhase)
    {
        juce::ignoreUnused(oldPhase);

        LogHelper::writeToLog("[ExperienceManager] SessionProgression: "
                              + juce::String(static_cast<int>(oldPhase))
                              + " [RIGHT] " + juce::String(static_cast<int>(newPhase)));

        switch (newPhase) {
            case SessionProgression::Phase::Setup:
                break; // No action

            case SessionProgression::Phase::LoadReference:
                // Referencia cargada → revelar si no está
                if (!revealManager_.isPanelRevealed(PanelId::Reference)) {
                    navShell_.revealPanel(PanelId::Reference);
                }
                break;

            case SessionProgression::Phase::DeepAnalysis:
                // ▶ Session tab unlock + sorpresa al completar análisis
                navShell_.setAvatarExpression(AvatarExpression::Surprised);
                if (!revealManager_.isPanelRevealed(PanelId::Session)) {
                    navShell_.revealPanel(PanelId::Session);
                }
                break;

            case SessionProgression::Phase::GuidedCoaching:
                // Coaching activo — sorpresa al empezar la mentoría
                navShell_.setAvatarExpression(AvatarExpression::Surprised);
                if (navShell_.getCoachRoomState() == CoachRoomState::MixMapStage) {
                    navShell_.setCoachRoomState(CoachRoomState::GainStaging);
                }
                break;

            case SessionProgression::Phase::Refinement:
                // Refinamiento → Automation
                navShell_.setAvatarExpression(AvatarExpression::Happy);
                if (navShell_.getCoachRoomState() < CoachRoomState::Refinement) {
                    navShell_.setCoachRoomState(CoachRoomState::Refinement);
                }
                celebrate("Refinamiento alcanzado");
                break;

            case SessionProgression::Phase::Report:
                // ▶ Overlay reporte
                navShell_.setAvatarExpression(AvatarExpression::Happy);
                navShell_.setCoachRoomState(CoachRoomState::Report);
                break;

            case SessionProgression::Phase::Memory:
                navShell_.refreshReport();
                break;
        }
    }

    // =======================================================================
    //  onCriticalIssueDetected — Issues críticos/clipping → expresión Serious
    // =======================================================================
    void ExperienceManager::onCriticalIssueDetected(bool hasClipping, int clippingCount)
    {
        juce::ignoreUnused(clippingCount);

        if (hasClipping) {
            navShell_.setAvatarExpression(AvatarExpression::Serious);
            LogHelper::writeToLog("[ExperienceManager] Clipping detected — Serious expression");
        }
    }

    // =======================================================================
    //  onWorkflowEvent — Cambio en plugins → expresión del avatar
    // =======================================================================
    void ExperienceManager::onWorkflowEvent(const WorkflowEvent& event)
    {
        // Mapa de acciones a expresiones:
        //   Fader/Compresion → Encouraging (el usuario está trabajando activamente)
        //   EQ/Pan/Mute → Thinking (el usuario está explorando)
        switch (event.action) {
            case WorkflowAction::FaderUp:
            case WorkflowAction::FaderDown:
            case WorkflowAction::CompressionOn:
            case WorkflowAction::CompressionOff:
                navShell_.setAvatarExpression(AvatarExpression::Encouraging);
                break;

            case WorkflowAction::EQBoost:
            case WorkflowAction::EQCut:
            case WorkflowAction::PanLeft:
            case WorkflowAction::PanRight:
            case WorkflowAction::StereoWiden:
            case WorkflowAction::Muted:
            case WorkflowAction::Unmuted:
                navShell_.setAvatarExpression(AvatarExpression::Thinking);
                break;

            default:
                break;
        }
    }

    // =======================================================================
    //  advanceToState — Avanza al estado UI con orquestación adicional
    // =======================================================================
    void ExperienceManager::advanceToState(CoachRoomState state)
    {
        auto oldState = navShell_.getCoachRoomState();
        navShell_.setCoachRoomState(state);

        if (oldState == CoachRoomState::MixMapStage && state == CoachRoomState::GainStaging) {
            navShell_.setAvatarExpression(AvatarExpression::Happy);
            celebrate("\xC2\xA1Tu sesi\xC3\xB3n est\xC3\xA1 lista!");
        }
        else if (state == CoachRoomState::Report) {
            navShell_.setAvatarExpression(AvatarExpression::Happy);
            celebrate("\xC2\xA1Sesi\xC3\xB3n completada!");
        }
        else if (state == CoachRoomState::Welcome) {
            navShell_.setAvatarExpression(AvatarExpression::Neutral);
        }
    }

    // =======================================================================
    //  revealAndAnimate — Revela panel + animación + unlock de tabs
    //
    //  NOTA: Celebraciones para reveals clave (MixMap, Tools, Session) se
    //  manejan en NavigationShell::onRevealPanel, que se dispara desde
    //  revealPanel() sin importar el origen. Esto evita duplicación y asegura
    //  que ningún camino se salte la celebración.
    // =======================================================================
    void ExperienceManager::revealAndAnimate(PanelId panel)
    {
        navShell_.revealPanel(panel);

        // Acciones adicionales específicas de ExperienceManager
        switch (panel) {
            case PanelId::Report:
                navShell_.setCoachRoomState(CoachRoomState::Report);
                break;
            default:
                break;
        }
    }

    // =======================================================================
    //  celebrate — Dispara animación de celebración + postea en el chat
    // =======================================================================
    void ExperienceManager::celebrate(const juce::String& achievement)
    {
        // ═══ Fase 2: Postear celebración en el chat ═══════════════════════
        // Cada celebración se muestra como un evento UI en el chat.
        // No saturar: solo postear si no hay una celebración activa.
        if (celebrationActive_) return;

        celebrationActive_ = true;
        celebrationProgress_ = 0.0f;
        celebrationMessage_ = achievement;

        LogHelper::writeToLog("[ExperienceManager] Celebration: " + achievement);

        // ═══ Coach Emotions: Happy en celebraciones ═══════════════════════
        navShell_.setAvatarExpression(AvatarExpression::Happy);

        // Publicar en el chat vía NavigationShell
        navShell_.postUIEvent("\xF0\x9F\x8E\x89", achievement);
    }

    // =======================================================================
    //  returnToCoach — Vuelve al Coach desde Tools o Session
    // =======================================================================
    void ExperienceManager::returnToCoach()
    {
        if (navShell_.getActiveTab() != TabBarComponent::Coach) {
            navShell_.setActiveTab(TabBarComponent::Coach);
        }
    }

    // =======================================================================
    //  updateAnimations — Avanza animaciones de orquestación
    // =======================================================================
    void ExperienceManager::updateAnimations()
    {
        if (celebrationActive_) {
            celebrationProgress_ += kCelebrationStep;
            if (celebrationProgress_ >= 1.0f) {
                celebrationProgress_ = 1.0f;
                celebrationActive_ = false;
            }
            // V2: usar eased para overlay visual de celebración
            // float eased = 1.0f - (1.0f - celebrationProgress_) * (1.0f - celebrationProgress_);
        }
    }

} // namespace mixcoach
