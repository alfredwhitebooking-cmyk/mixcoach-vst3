#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#include "../UI/CoachChatComponent.h"
#include "../engine/PanelRevealManager.h"
#include "ChatMessageSequencer.h"
#include "CoachingNarrativeTypes.h"
#include "../../Common/memory/SharedData.h"

namespace mixcoach {

    // Forward declarations
    class NavigationShell;
    class MixCoachPanel;
    class EvidencePanel;
    class PluginSuggestionsProvider;

    // ═══════════════════════════════════════════════════════════════════════════
    //  CoachingProblem — Un problema detectado con todos los datos necesarios
    //  para ejecutar el ciclo narrativo completo.
    //
    //  Ahora incluye beforeSnapshot para verificación real.
    // ═══════════════════════════════════════════════════════════════════════════
    struct CoachingProblem
    {
        TrackProblemGroup trackGroup;
        CoachRoomState phase = CoachRoomState::GainStaging;

        // Qué analyzer abrir (para ShowEvidence)
        juce::String evidenceAnalyzer; // "vu", "spectrum", "crest", "vectorscope", "lufs"
        float highlightFreq = 0.0f;
        juce::String highlightLabel;

        // Mensaje de explicación (para Explain step)
        juce::String explanationMessage;

        // Opciones 3 tiers
        PluginSuggestionGroup nativeOption;
        PluginSuggestionGroup freeOption;
        PluginSuggestionGroup premiumOption;

        CorrectionCardData correctionData;

        bool isValid() const noexcept { return !trackGroup.tracks.empty(); }

        // ─── Snapshot pre-corrección (para verify loop real) ──────────────
        int beforeSlotIndex = -1;          // Slot al que se tomó snapshot
        juce::String beforeTrackName;      // Nombre del track ("Kick", "Voz", etc.)
        float beforePeakDb = -100.0f;     // Peak antes del cambio
        float beforeRmsDb = -100.0f;      // RMS antes del cambio
        float beforeCrestDb = 0.0f;       // Crest antes del cambio
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  CoachingNarrativeDirector — Orquestador único del loop de coaching
    //
    //  Ejecuta la secuencia completa para CADA problema:
    //    1. DETECT      → (implícito: el motor detecta el problema)
    //    2. SHOW        → Abre el analyzer contextual + evidencia visual
    //    3. EXPLAIN     → Postea mensaje en el chat explicando el problema
    //    4. SHOW_OPTIONS→ Muestra 3 tiers de soluciones vía QuickReply
    //    5. WAITING     → Espera que el usuario elija una opción
    //    6. VERIFY      → Espera confirmación + verifica el cambio
    //    7. CELEBRATE   → XP burst + animación del avatar + mensaje
    //    8. NEXT        → Busca el siguiente problema
    //
    //  Cada transición tiene delays intencionales para dar ritmo narrativo.
    //  No necesita polling externo: usa juce::Timer interno a 60fps.
    // ═══════════════════════════════════════════════════════════════════════════
    class CoachingNarrativeDirector : private juce::Timer
    {
    public:
        // Step is now an alias for NarrativeStep from CoachingNarrativeTypes.h
        using Step = NarrativeStep;

        CoachingNarrativeDirector(NavigationShell& navShell, MixCoachPanel& coachPanel,
                                   SharedData& sharedData);
        ~CoachingNarrativeDirector() override = default;

        // ─── API pública ─────────────────────────────────────────────────

        /** Inicia el ciclo narrativo con un problema detectado.
            Comienza automáticamente con el paso ShowEvidence. */
        void startProblem(const CoachingProblem& problem);

        /** El usuario seleccionó una opción (plugin tier o texto).
            Transiciona a ConfirmApplied. */
        void onOptionSelected(const juce::String& option);

        /** El usuario confirmó que aplicó el cambio.
            Transiciona a CelebrateStep. */
        void onCorrectionConfirmed();

        /** Resetea el director a Idle. */
        void reset();

        /** Retorna el paso actual del ciclo. */
        Step getCurrentStep() const noexcept { return currentStep_; }

        /** Número máximo de reintentos para correcciones fallidas. */
        static constexpr int kMaxRetries = 3;

        /** Retorna true si hay un ciclo narrativo activo. */
        bool isActive() const noexcept
        {
            return currentStep_ != Step::Idle && currentStep_ != Step::Complete;
        }

        /** Construye un CoachingProblem desde un ProblemType detectado + sugerencias.
            Separa las sugerencias por tier (Native/Free/Premium) y mapea el
            ProblemType al CoachRoomState correcto + analyzer contextual.
            \xC3\x9Atil para la integraci\xC3\xB3n con el LLM (processCoachMessage). */
        [[nodiscard]] static CoachingProblem buildFromProblemType(
            ProblemType detectedProblem,
            const PluginSuggestionsProvider& provider);

        /** Procesa una respuesta completa del LLM y la integra en el ciclo narrativo.
            Si el Director est\xC3\xA1 en un paso de espera (WaitingForUser), encola la respuesta.
            Si est\xC3\xA1 Idle, postea el mensaje directamente como respuesta del coach.
            Cableado desde CoachEngine::setLlmResponseCompleteCallback() en PluginEditor. */
        void processCoachMessage(const juce::String& llmResponse);

        /** Retorna el problema actual. */
        const CoachingProblem& getCurrentProblem() const noexcept { return currentProblem_; }

        /** Callback cuando el ciclo completo termina (para NavigationShell). */
        std::function<void()> onCycleComplete;

        /** Callback cuando el usuario selecciona un tier de plugin (para NavigationShell). */
        std::function<void(int tierIndex, const PluginSuggestionGroup& option)> onTierSelected;

        /** Callback para iniciar verificación (para NavigationShell). */
        std::function<void(const CorrectionCardData& data)> onStartVerification;

        // ═══ Phase population helpers (extraídos de NavigationShell) ═══════
        /** Pobla el GainStagingPanel con datos reales del engine y cablea
            los botones onApplyGain / onApplyAll.
            Extraído de NavigationShell::setCoachRoomState() (~215 líneas).
            @param coach  Referencia al CoachEngine para obtener track roles + advices */
        void populateGainStaging(CoachEngine& coach);

        // ═══ GAP #4: UI phase helpers (extraídos de NavigationShell) ═══════
        /** Aplica la UI de fase de coaching: muestra/oculta paneles y configura
            analyzers según la fase activa (EQ→spectrum, Space→vectorscope, etc.).
            Extraído de NavigationShell::setCoachRoomState() (~200 líneas).
            @param newState  Fase a aplicar (GainStaging, EQ, Compression, etc.) */
        void applyPhaseUI(CoachRoomState newState);

        /** Alimenta el evidence panel con datos en tiempo real según la fase.
            Se llama desde NavigationShell::timerCallback() cada ~15 ticks.
            @param analyzer  AudioAnalyzer para obtener datos de metering
            @param coach     CoachEngine para obtener advices de fase */
        void feedEvidenceForPhase(class AudioAnalyzer& analyzer, CoachEngine& coach);

        /** Maneja las reacciones UI tras verificar una corrección.
            Extraído de NavigationShell::onCorrectionApplied (~50 líneas).
            @param data         CorrectionCardData con resultado de verificación
            @param correctionMode true si CorrectionLearner ya procesó el dato */
        void handleCorrectionResult(CorrectionCardData& data, bool correctionMode);

        // ═══ Fase 8: CoachingGuide integration ═══════════════════════════
        /** Actualiza el CoachingGuideWidget con la fase actual del motor.
            Mapea MentorPhase → CoachingStage y llama setStageDirectly().
            Extraído de NavigationShell (3 callbacks duplicados: onAdvanceStage,
            onRequestHelp, y setCoachRoomState). */
        void updateCoachingGuide();

    private:
        void timerCallback() override;

        /** Transiciona a un nuevo paso después de un delay en frames (~16.6ms c/u). */
        void transitionTo(Step step, int delayFrames = 0);

        /** Ejecuta inmediatamente el paso (se llama desde timerCallback al vencer el delay). */
        void executeStep(Step step);

        // ─── Handlers de cada paso ───────────────────────────────────────
        void doShowEvidence();
        void doExplain();
        void doShowOptions();
        void doConfirm();
        void doCelebrateStep();
        void doNextProblemStep();
        /** Entrega el mensaje de "siguiente problema" después de la respuesta LLM pendiente. */
        void deliverNextProblemStep();

        // ─── Helpers ─────────────────────────────────────────────────────
        juce::String phaseToAnalyzerId(CoachRoomState phase) const;
        juce::String buildExplainMessage() const;
        PluginSuggestionGroup pickOption(int index) const;

        // ─── Helpers de verificación real ──────────────────────────────
        /** Toma un snapshot de los niveles actuales del slot afectado.
            Se llama desde onOptionSelected() antes de que el usuario aplique el cambio.
            Almacena los valores en currentProblem_.beforePeakDb / beforeRmsDb. */
        void takeBeforeSnapshot();

        /** Lee los niveles actuales, calcula el delta real y actualiza
            currentProblem_.correctionData con el resultado.
            Se llama desde onCorrectionConfirmed(). */
        void verifyWithRealData();

        // ═══ EngineeringIntervention — Intervención estructurada en 5 pasos ═══
        /** Construye una intervención de ingeniería completa para el problema actual.
            Los 5 campos (observación, causa, acción, escucha, criterio) se
            generan según el dominio y la fase del problema actual.
            @return EngineeringIntervention con campos poblados según el contexto */
        [[nodiscard]] EngineeringIntervention buildEngineeringIntervention() const;

        /** Postea en el chat la intervención completa formateada en 5 pasos.
            Se llama desde doExplain() para reemplazar el mensaje genérico.
            @param intervention  Intervención a postear (de buildEngineeringIntervention()) */
        void postEngineeringMessage(const EngineeringIntervention& intervention);

        /** Postea SOLO la acción + escucha + criterio (para el paso ShowOptions).
            @param intervention  Intervención ya construida */
        void postActionMessage(const EngineeringIntervention& intervention);

        // ─── Referencias ─────────────────────────────────────────────────
        NavigationShell& navShell_;
        MixCoachPanel& coachPanel_;
        EvidencePanel& evidencePanel_;
        ChatMessageSequencer messageSequencer_;
        SharedData& sharedData_;

        // ─── Estado ──────────────────────────────────────────────────────
        Step currentStep_ = Step::Idle;
        CoachingProblem currentProblem_;
        int selectedTierIndex_ = -1;

        // ─── EngineeringIntervention cache ────────────────────────────
        mutable EngineeringIntervention cachedIntervention_;
        mutable bool interventionCacheValid_ = false;

        /** Invalida la cache cuando cambia currentProblem_. */
        void invalidateInterventionCache() const { interventionCacheValid_ = false; }

        // ─── Retry logic para verify fallido ───────────────────────────
        int retryCount_ = 0;

        // ─── Sistema de delays controlados ───────────────────────────────
        bool awaitingTransition_ = false;
        int remainingTicks_ = 0;
        Step pendingStep_ = Step::Idle;

        // ─── Almacena respuesta del LLM cuando el Director está ocupado ─────────
        juce::String pendingLlmResponse_;

        // Colores y emojis para los tiers
        static juce::Colour tierColour(int index);
        static const char* tierIcon(int index);
        static const char* tierLabel(int index);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CoachingNarrativeDirector)
    };

} // namespace mixcoach
