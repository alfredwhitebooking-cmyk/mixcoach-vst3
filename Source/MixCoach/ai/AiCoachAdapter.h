#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

#include "../../Common/memory/SharedData.h"
#include "../../Common/memory/SlotRegistry.h"
#include "../../Common/types/Constants.h"
#include "../audio/AudioAnalyzer.h"
#include "../engine/CoachEngine.h"
#include "../engine/PhaseManager.h"
#include "../engine/WorkflowDetector.h"
#include "../UI/MessengerListComponent.h"
#include "LlmClient.h"
#include "LlmChatSession.h"
#include "../engine/LlmCommandInterpreter.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  AiCoachAdapter — Puente entre los 2 oídos y un LLM
    //
    //  Toma los datos de:
    //    • Oído 1 (Messenger): SharedData + SlotRegistry → telemetría por pista
    //    • Oído 2 (Master):    AudioAnalyzer → FFT, LUFS, correlación, RMS/Peak
    //    • Referencia:         ReferenceFingerprint → comparación mix vs target
    //    • Contexto:           PhaseManager + session memory
    //
    //  Y produce un prompt estructurado listo para enviar a:
    //    • Ollama (Qwen 2.5 7B local)
    //    • OpenAI API
    //    • Claude API
    //
    //  Uso típico:
    //    1. setGenre("Reggaeton")
    //    2. setTrackIntent(slot, { "Kick", "Punchy" })
    //    3. buildFullContext() — cada ~8s o cuando el usuario pregunta
    //    4. buildUserQueryPrompt("Como va la mezcla?")
    //
    //  La integración HTTP con el LLM se hace desde otro módulo.
    // ═══════════════════════════════════════════════════════════════════════════
    class AiCoachAdapter
    {
    public:
        // ─── Track Intent — qué es y qué se espera de cada pista ──────────────
        struct TrackIntent
        {
            juce::String role;  // "Kick", "Snare", "808", "Voz Principal", "Pad", etc.
            juce::String style; // "Punchy", "Warm", "Aggressive", "Clean", etc.

            bool valid() const { return role.isNotEmpty(); }
        };

        AiCoachAdapter(SharedData& sharedData,
                       AudioAnalyzer& audioAnalyzer,
                       CoachEngine& coachEngine,
                       PhaseManager& phaseManager,
                       LlmClient* llmClient = nullptr);

        // ─── Contexto de mezcla ───────────────────────────────────────────────
        void setGenre(const juce::String& genre);

        [[nodiscard]] const juce::String& getGenre() const noexcept { return genre_; }

        void setTrackIntent(int slotIndex, const TrackIntent& intent);
        [[nodiscard]] const TrackIntent& getTrackIntent(int slotIndex) const;

        // ─── Generación de prompts ───────────────────────────────────────────
        /** Prompt del sistema: personalidad del ingeniero. */
        [[nodiscard]] juce::String buildSystemPrompt() const;

        /** Contexto completo de la sesión actual: tracks + master + ref + bus + fase. */
        [[nodiscard]] juce::String buildFullContext() const;

        /** Contexto completo + mensaje del usuario. */
        [[nodiscard]] juce::String buildUserQueryPrompt(const juce::String& userMessage) const;

        /** Resumen compacto (para enviar periódicamente sin que el usuario pregunte). */
        [[nodiscard]] juce::String buildCompactSummary() const;
        /** Contexto LIGERO (~3KB) para conversacion natural con el LLM.
            A diferencia de buildFullContext(), esto envia solo lo esencial:
            tracks activos, master (peak, LUFS, correlacion), fase, progreso,
            issues criticos y eventos recientes. */
        [[nodiscard]] juce::String buildChatContext() const;

        // ─── Memoria de sesión ───────────────────────────────────────────────
        struct SessionChange
        {
            int64_t timestampUs;
            int slotIndex;
            juce::String trackName;
            juce::String description; // "Gain -2dB", "EQ +3dB @ 3kHz", etc.
            float beforeValue;
            float afterValue;
        };

        void recordChange(int slotIndex,
                          const juce::String& trackName,
                          const juce::String& description,
                          float beforeValue,
                          float afterValue);

        [[nodiscard]] const std::vector<SessionChange>& getSessionHistory() const noexcept { return sessionHistory_; }

        void resetSession();

        // ═══ Persistencia de sesión (save/load a JSON) ═══════════════════
        /** Guarda el estado completo de la sesión a un archivo JSON. */
        void saveSessionMemory(const juce::File& file) const;
        /** Carga el estado de la sesión desde un archivo JSON. */
        void loadSessionMemory(const juce::File& file);
        /** Guarda automáticamente al archivo por defecto (Documents/MixCoach_Logs/session_memory.json). */
        void autoSave();
        /** Carga desde el archivo por defecto. */
        void autoLoad();
        /** Genera un string del historial de sesión para mostrar en el chat. */
        [[nodiscard]] juce::String buildSessionHistoryString() const;

        // ═══ User Profile — Patrones del ingeniero entre sesiones ═══════
        // ═══ V2: Expanded with tone preference, favorite plugins, behavior profile ═══
        struct UserProfile
        {
            // ─── Identidad ────────────────────────────────────────────────
            juce::String engineerName;   // Nombre del ingeniero (artístico o real)
            juce::String preferredTone;  // "motivador", "técnico", "directo", "paciente", etc.

            // ─── Estadísticas de sesión ───────────────────────────────────
            int sessionCount = 0;
            juce::String lastSessionDate;  // "2026-07-22"
            juce::String mostUsedGenre;
            std::map<juce::String, int> genreFrequency;

            // ─── Preferencias ─────────────────────────────────────────────
            juce::StringArray favoriteGenres;  // ["reggaeton", "trap"]
            juce::StringArray favoritePlugins; // ["FabFilter Pro-Q 3", "ValhallaRoom"]
            int experienceLevel = 2;            // 1=Novice, 2=Intermediate, 3=Advanced, 4=Expert

            // ─── Perfil de comportamiento ─────────────────────────────────
            struct BehaviorProfile
            {
                bool tendsToOverApply = false;     // Tiende a aplicar demasiado gain/corte
                bool tendsToIgnore = false;         // Tiende a ignorar recomendaciones
                float averageCorrectionTime = 0.0f; // Tiempo promedio entre recomendación y verify (segundos)
                int totalSessions = 0;
            };
            BehaviorProfile behavior;

            // ═══ V1 legacy fields (spectral tendency, habits) ════════════
            // Spectral tendency per band (Sub→Air): positive = consistently boosts, negative = cuts
            float tendencyPerBand[6] = {};
            int tendencyCounts[6]    = {}; // How many data points per band

            float avgCrestTargetPerBand[6] = {};
            float avgLufsTarget            = -14.0f;
            float avgCrestFactor           = 10.0f;

            // Common corrections detected as habits across sessions
            struct Habit
            {
                juce::String description;
                juce::String regionLabel; // "Sub", "Bass", "Low-Mid", etc.
                float frequencyHz = 0.0f;
                float magnitudeDb = 0.0f;
                bool isBoost      = false;
                int count         = 0;
            };

            std::vector<Habit> habits;

            // ═══ Session History — datos de la última sesión ═══════════════════
            /** Duración de la última sesión en segundos. */
            int lastSessionDurationS = 0;
            /** Problemas detectados en la última sesión. */
            int lastSessionProblemsDetected = 0;
            /** Problemas resueltos (verificados) en la última sesión. */
            int lastSessionProblemsResolved = 0;
            /** % de match vs referencia al final de la última sesión (0-100). */
            int lastSessionReferenceMatchPct = 0;
            /** Mix Score global al final de la última sesión (0-100). */
            int lastSessionMixScore = 0;
            /** Género trabajado en la última sesión. */
            juce::String lastSessionGenre;
            /** Brief text about what happened in last session */
            juce::String lastSessionSummary;
            /** Acumulado: total de problemas resueltos a lo largo de todas las sesiones. */
            int totalProblemsResolved = 0;
            /** Acumulado: mejor match % alcanzado en cualquier sesión. */
            int bestReferenceMatchPct = 0;

            bool walkthroughCompleted = false; // Tutorial 4.3: true si ya vieron el walkthrough
            bool valid = false;

            /** Builds a formatted string for injection into the LLM system prompt. */
            [[nodiscard]] juce::String toProfileContext() const;
        };

        /** Actualiza el perfil del usuario con datos de la sesión actual.
            Se llama automáticamente en autoSave(). */
        void updateUserProfile();

        /** Guarda el perfil del usuario a un archivo JSON separado. */
        void saveUserProfile(const juce::File& file) const;

        /** Carga el perfil del usuario desde un archivo JSON. */
        void loadUserProfile(const juce::File& file);

        /** Retorna el perfil del usuario (para depuración). */
        [[nodiscard]] const UserProfile& getUserProfile() const noexcept { return userProfile_; }

        /** Formatea el perfil del usuario para inyectar en buildSystemPrompt(). */
        [[nodiscard]] juce::String buildUserProfileContext() const;

        /** Setea el nombre del ingeniero (artístico o real). */
        void setEngineerName(const juce::String& name) noexcept { userProfile_.engineerName = name; }

        /** Marca el walkthrough como completado (Tutorial 4.3). */
        void setWalkthroughCompleted(bool completed = true) noexcept { userProfile_.walkthroughCompleted = completed; }

        /** Retorna true si el walkthrough ya fue completado. */
        [[nodiscard]] bool isWalkthroughCompleted() const noexcept { return userProfile_.walkthroughCompleted; }

        /** Retorna el nombre del ingeniero. */
        [[nodiscard]] const juce::String& getEngineerName() const noexcept { return userProfile_.engineerName; }

        // ═══ Session snapshots — historial multi-sesión ═══════════════
        /** Snapshot de una sesión anterior para el historial de progreso. */
        struct SessionSnapshotEntry
        {
            int64_t timestampUs = 0;
            int sessionNumber = 0;
            int mixScoreOverall = 0;
            int domainGain = 0;
            int domainTonal = 0;
            int domainDynamics = 0;
            int domainSpatial = 0;
            int domainReference = 0;
        };

        /** Carga el historial de sesiones desde el archivo persistente.
            Retorna vector vacío si no hay historial o hay error de lectura. */
        static std::vector<SessionSnapshotEntry> loadSessionHistory();

        /** Ruta por defecto: Documents/MixCoach_Logs/session_memory.json */
        static juce::File getDefaultSessionFile();

        /** Ruta por defecto: Documents/MixCoach_Logs/user_profile.json */
        static juce::File getDefaultProfileFile();

        // ═══ Integración con LLM ═══════════════════════════════════════
        /** Configura el cliente LLM. El adapter usará el LLM para generar respuestas. */
        void setLlmClient(LlmClient* client) noexcept { llmClient_ = client; }

        /** Envía un mensaje del usuario al LLM y entrega la respuesta completa vía callback. */
        void askLlm(const juce::String& userMessage,
                    std::function<void(bool success, const juce::String& response)> callback);

        /** Envía un mensaje del usuario al LLM con respuesta en streaming.
            @param userMessage  Mensaje del usuario
            @param onToken      Se llama por cada token (message thread)
            @param onComplete   Se llama al finalizar: (success, fullResponse)
            El historial de conversación se actualiza automáticamente al completar. */
        void askLlmStream(const juce::String& userMessage,
                          std::function<void(const juce::String& token)> onToken,
                          std::function<void(bool success, const juce::String& response)> onComplete);

        // ═══ Perfil de usuario — experiencia y tono ═════════════════════
        enum class ExperienceLevel : uint8_t
        {
            Novice,       // Principiante: explicaciones detalladas, tono amable
            Intermediate, // Intermedio: equilibrio entre detalle y concisión
            Advanced,     // Avanzado: jerga técnica, menos explicaciones
            Expert        // Experto: solo números, sin rodeos
        };

        void setExperienceLevel(ExperienceLevel level) noexcept { experienceLevel_ = level; }

        [[nodiscard]] ExperienceLevel getExperienceLevel() const noexcept { return experienceLevel_; }

        [[nodiscard]] static const char* experienceLevelName(ExperienceLevel level) noexcept;

        // ═══ Plugin suggestions — DAW detection + plugin catalog ═══════════
        /** Configura el nombre del DAW anfitrión (FL Studio, Ableton Live, etc.).
            El coach inyecta esto en el contexto para sugerir plugins nativos
            cuando da consejos de EQ, compresión, reverb, etc. */
        void setHostName(const juce::String& name) noexcept { dawName_ = name; }

        [[nodiscard]] const juce::String& getHostName() const noexcept { return dawName_; }

        /** Verifica si el LLM está disponible para responder. */
        [[nodiscard]] bool isLlmAvailable() const noexcept
        {
            return llmClient_ != nullptr && llmClient_->isAvailable();
        }

        /** Configura el intérprete de comandos UI del LLM.
            NavigationShell lo cablea para ejecutar comandos JSON (switch_tab, highlight_track, etc.). */
        void setCommandInterpreter(LlmCommandInterpreter* interp) noexcept { commandInterpreter_ = interp; }

        /** Activa/desactiva el modo LLM híbrido. */
        void setLlmEnabled(bool enabled) noexcept { llmEnabled_ = enabled; }

        [[nodiscard]] bool isLlmEnabled() const noexcept { return llmEnabled_; }

        // ═══ Priority Adherence Stats — tracking de consistencia del LLM ═══
        struct PriorityAdherenceStats
        {
            int totalChecks        = 0;
            int adherentResponses  = 0;
            int retriesTriggered   = 0;

            [[nodiscard]] float adherenceRate() const noexcept
            {
                return totalChecks > 0 ? static_cast<float>(adherentResponses) / static_cast<float>(totalChecks) : 1.0f;
            }

            void reset() noexcept
            {
                totalChecks = 0;
                adherentResponses = 0;
                retriesTriggered = 0;
            }
        };

        /** Retorna las estadísticas de adherencia a prioridad. */
        [[nodiscard]] const PriorityAdherenceStats& getPriorityAdherenceStats() const noexcept
        {
            return priorityAdherenceStats_;
        }

    private:
        /** Valida que la respuesta del LLM mencione el issue #1 de [PRIORITY ISSUES].
            @param response     Respuesta del LLM a validar
            @param topIssues    Lista de issues priorizados (de CoachEngine::getTopPriorityIssues)
            @return true si la respuesta menciona el issue #1 o no hay issues activos */
        [[nodiscard]] static bool validatePriorityAdherence(const juce::String& response,
                                                             const std::vector<PriorityScore>& topIssues) noexcept;
        // ─── Bloques del prompt ───────────────────────────────────────────────
        [[nodiscard]] juce::String buildMasterSummary() const;
        [[nodiscard]] juce::String buildTrackSummaries() const;
        [[nodiscard]] juce::String buildBusSummaries() const;
        [[nodiscard]] juce::String buildReferenceComparison() const;
        [[nodiscard]] juce::String buildPhaseSummary() const;
        [[nodiscard]] juce::String buildRecommendations() const;
        [[nodiscard]] juce::String buildSessionMemory() const;
        [[nodiscard]] juce::String buildTrackIntents() const;
        /** Análisis semántico: compara cada pista contra su perfil esperado y produce diffs. */
        [[nodiscard]] juce::String buildSemanticAnalysis() const;

        /** Interpretaciones de analizadores (Nivel 4): correlation, crest, LUFS, centroid → consecuencias + acciones. */
        [[nodiscard]] juce::String buildAnalyzerInterpretations() const;

        /** Interpretaciones por pista: gain, stereo width y mid/side para cada pista activa.
            Usa AnalyzerInterpreter::interpretGain(), interpretStereoWidth(), interpretMidSide()
            para transformar datos crudos en interpretaciones semánticas. */
        [[nodiscard]] juce::String buildPerTrackInterpretations() const;

        /** Eventos detectados por WorkflowDetector: qué hizo el usuario recientemente. */
        [[nodiscard]] juce::String buildWorkflowEvents() const;

        /** Historial de correcciones (CorrectionHistoryEntry → texto para el LLM).
            Incluye las últimas 20 correcciones con su estado final (Applied/OverApplied/etc.).
            Útil para que el LLM vea el historial de recomendaciones aplicadas. */
        [[nodiscard]] juce::String buildCorrectionHistory() const;

        /** Historial unificado de la mezcla (MixHistory circular buffer → texto para el LLM).
            Agrupa cambios por pista+dominio con delta neto, mostrando la evolución
            de cada elemento. Incluye eventos de usuario, correcciones y workflow. */
        [[nodiscard]] juce::String buildMixHistory() const;

        /** Plan de acción contra referencia: gaps priorizados por dominio. */
        [[nodiscard]] juce::String buildReferenceDrivenPlan() const;

        /** Progreso del plan: pasos completados vs pendientes contra la referencia. */
        [[nodiscard]] juce::String buildProgressPlan() const;

        /** Mix Score: puntaje global de salud de la mezcla 0-100. */
        [[nodiscard]] juce::String buildMixScore() const;

        // ═══════════════════════════════════════════════════════════════════════════
        //  Knowledge Base — RAG: carga archivos markdown de conocimiento de mezcla
        //  Los archivos están en el directorio knowledge/ relativo al plugin.
        //  Se inyectan en el contexto del LLM para darle conocimiento experto.
        // ═══════════════════════════════════════════════════════════════════════════
        /** Carga un archivo de conocimiento como string.
            @param relativePath Ruta relativa dentro de knowledge/, ej: "fundamentals.md", "genres/pop.md"
            @returns Contenido del archivo, o string vacío si no se encuentra.
            Busca en: 1) knowledge/ relativo al CWD, 2) knowledge/ relativo al ejecutable. */
        [[nodiscard]] static juce::String loadKnowledgeFile(const juce::String& relativePath);

        /** Carga un archivo de conocimiento con un header markdown para contexto. */
        [[nodiscard]] static juce::String loadKnowledgeSection(const juce::String& relativePath,
                                                               const juce::String& sectionLabel);

        // ─── Helpers de formateo ─────────────────────────────────────────────
        [[nodiscard]] static juce::String formatDb(float value) noexcept;
        [[nodiscard]] static juce::String formatLUFS(float value) noexcept;
        [[nodiscard]] static juce::String bandLabel(int bandIndex) noexcept;
        [[nodiscard]] static juce::String busLabel(BusType bus) noexcept;
        [[nodiscard]] static juce::String phaseLabel(MentorPhase phase) noexcept;
        [[nodiscard]] static juce::String suggestionStatusEmoji(SuggestionStatus status) noexcept;

        // ─── ConversationTurn — alias al tipo definido en LlmChatSession ─────
        using ConversationTurn = LlmChatSession::ConversationTurn;

        void addConversationTurn(ConversationTurn::Role role, const juce::String& message);
        [[nodiscard]] juce::String buildConversationHistory() const;

        // ─── Dependencias ────────────────────────────────────────────────────
        SharedData& sharedData_;
        AudioAnalyzer& audioAnalyzer_;
        CoachEngine& coachEngine_;
        PhaseManager& phaseManager_;

        // ─── Estado ──────────────────────────────────────────────────────────
        juce::String genre_ = "Unknown";
        std::array<TrackIntent, SlotRegistry::kMaxSlots> trackIntents_;
        std::vector<SessionChange> sessionHistory_;
        std::vector<ConversationTurn> conversationHistory_; // Historial de la charla con el LLM

        LlmClient* llmClient_{nullptr};
        bool llmEnabled_{false};
        ExperienceLevel experienceLevel_{ExperienceLevel::Intermediate};

        // ═══ Plugin catalog — carga y cache ════════════════════════════════
        /** Carga los archivos JSON de knowledge/plugins/ y los formatea
            para inyectar en buildSystemPrompt(). El resultado se cachea
            para evitar I/O de disco en cada generación de prompt. */
        [[nodiscard]] juce::String buildPluginCatalog() const;
        mutable bool pluginCatalogLoaded_ = false;
        mutable juce::String pluginCatalog_; // Cache: catálogo formateado

        // ═══ Cache de per-track interpretations ═══════════════════════════
        // Evita recalcular en cada buildFullContext() si los datos no han cambiado.
        mutable bool perTrackInterpCacheValid_ = false;
        mutable juce::String perTrackInterpCache_;
        mutable int64_t perTrackInterpCacheMaxTsUs_ = 0; // Máximo timestamp de todas las pistas al cachear
        mutable int perTrackInterpCacheActiveCount_ = 0; // Conteo de pistas activas al cachear

        static constexpr int kMaxConversationTurns = 50; // Máximo de turnos de conversación guardados
        static constexpr int kMaxSessionChanges    = 50;
        static constexpr float kEnergyThreshold    = -70.0f;
        static constexpr int kMaxPriorityRetries   = 1;    // Máximo re-envíos por mensaje si el LLM ignora issue #1

        // ═══ Priority Adherence tracking ═════════════════════════════════
        PriorityAdherenceStats priorityAdherenceStats_; // dB — umbral para considerar banda activa

        // ═══ User Profile storage ═══════════════════════════════════════
        UserProfile userProfile_;
        int initialProfileSessionCount_ = -1; // -1 = unset, snapshot al cargar para incrementar solo 1x por sesion;

        // ═══ LlmCommandInterpreter — procesa comandos JSON del LLM (switch_tab, highlight_track, etc.) ═══
        LlmCommandInterpreter* commandInterpreter_{nullptr};

        juce::String dawName_ = "FL Studio"; // DAW por defecto (el usuario usa FL Studio). Se puede cambiar via
                                             // setHostName() o cuando getHostProperties() esté disponible.
    };

} // namespace mixcoach
