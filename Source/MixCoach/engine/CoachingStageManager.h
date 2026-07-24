#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#include "../../Common/types/Types.h"

namespace mixcoach {

    // Forward declarations
    class CoachEngine;
    class AudioAnalyzer;
    class PhaseManager;

    // ═══════════════════════════════════════════════════════════════════════════
    //  CoachingStage — Etapas secuenciales del flujo de mezcla
    //
    //  Estas 7 etapas cubren el journey completo del usuario DESPUÉS del setup:
    //
    //    GainStaging → Balance → EQ → Compression → Spatial → Automation → Refinement
    //
    //  Automation ahora es una fase independiente (antes estaba anidada bajo Spatial).
    //  Se enfoca en LUFS, loudness y dinámica de secciones (verso/coro).
    //
    //  A diferencia de MentorPhase (que auto-avanza por métricas), CoachingStage
    //  requiere APROBACIÓN del usuario antes de avanzar. El coach:
    //    1. Guía al usuario con instrucciones detalladas al entrar a una etapa
    //    2. Monitorea el progreso y envía tips contextuales
    //    3. Cuando la etapa está completa, PREGUNTA: "¿Listo para avanzar?"
    //    4. Solo avanza cuando el usuario responde afirmativamente
    // ═══════════════════════════════════════════════════════════════════════════
    enum class CoachingStage : uint8_t
    {
        GainStaging = 0, // Ajustar niveles: gain staging, clipping, headroom
        Balance     = 1, // Balance de mezcla: faders, paneo, niveles relativos
        EQ          = 2, // Balance tonal: EQ, filtros, carving espectral
        Compression = 3, // Dinámica: compresión, saturación, crest factor
        Spatial     = 4, // Espacio: reverb, delay, ancho estéreo
        Automation  = 5, // Automatización: LUFS, loudness, dinámica de secciones
        Refinement  = 6, // Refinamiento artístico: profundidad, impacto, emoción

        COUNT = 7
    };

    // ═══ Mapping CoachingStage ↔ MentorPhase ═════════════════════════════════
    inline MentorPhase coachingStageToMentorPhase(CoachingStage stage) noexcept
    {
        switch (stage) {
            case CoachingStage::GainStaging: return MentorPhase::GainStaging;
            case CoachingStage::Balance:     return MentorPhase::Balance;
            case CoachingStage::EQ:          return MentorPhase::EQ;
            case CoachingStage::Compression: return MentorPhase::Compresion;
            case CoachingStage::Spatial:     return MentorPhase::Espacio;
            case CoachingStage::Automation:  return MentorPhase::Espacio; // Automation mapea a Espacio para métricas de PhaseManager
            case CoachingStage::Refinement:  return MentorPhase::MasterCheck; // maps to last phase for check
            default:                         return MentorPhase::GainStaging;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  StageInfo — Metadatos de cada etapa (estáticos)
    // ═══════════════════════════════════════════════════════════════════════════
    struct StageInfo
    {
        [[nodiscard]] static const char* name(CoachingStage stage) noexcept;
        [[nodiscard]] static const char* icon(CoachingStage stage) noexcept;
        /** Descripción larga: qué hacer en esta etapa */
        [[nodiscard]] static const char* description(CoachingStage stage) noexcept;
        /** Checklist de criterios para considerar la etapa completa */
        [[nodiscard]] static const char* checklist(CoachingStage stage) noexcept;
        /** Mensaje de guía detallada al ENTRAR a la etapa */
        [[nodiscard]] static const char* guidanceMessage(CoachingStage stage) noexcept;
        /** Mensaje de aprobación: "¿Listo para avanzar?" */
        [[nodiscard]] static const char* approvalMessage(CoachingStage stage) noexcept;
        /** Mensaje de bienvenida al avanzar a la SIGUIENTE etapa */
        [[nodiscard]] static const char* welcomeMessage(CoachingStage stage) noexcept;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  CoachingStageManager — Controlador de flujo secuencial con aprobación
    // ═══════════════════════════════════════════════════════════════════════════
    class CoachingStageManager
    {
    public:
        explicit CoachingStageManager(PhaseManager& phaseManager);

        // ─── Inicialización ────────────────────────────────────────────────
        /** Inicia el stage manager después del setup.
            @param startStage  Etapa inicial (default = GainStaging)
            @param autoRequestApproval  Si true, pide aprobación automática al completar cada etapa */
        void initialize(CoachingStage startStage = CoachingStage::GainStaging,
                        bool autoRequestApproval = true);

        /** Resetea el manager a su estado inicial. */
        void reset();

        // ─── Estado ─────────────────────────────────────────────────────────
        [[nodiscard]] CoachingStage getCurrentStage() const noexcept { return currentStage_; }
        [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }
        [[nodiscard]] bool isAwaitingApproval() const noexcept { return awaitingApproval_; }
        [[nodiscard]] bool isStageCompleted(CoachingStage stage) const noexcept;
        [[nodiscard]] bool isComplete() const noexcept;

        // ─── Progreso ──────────────────────────────────────────────────────
        /** Progreso de la etapa actual (0.0-1.0).
            Delega a PhaseManager para las primeras 5 etapas, usa RefinementProfile para Refinement. */
        [[nodiscard]] float getStageProgress() const noexcept;

        /** Progreso global (0.0-1.0) a través de todas las 6 etapas. */
        [[nodiscard]] float getOverallProgress() const noexcept;

        // ─── Actualización periódica ────────────────────────────────────────
        /** Se llama desde periodicAnalysis(). Evalúa si la etapa actual está
            completa y, si lo está, pide aprobación automáticamente. */
        void update(const CoachEngine& engine, const AudioAnalyzer& analyzer);

        // ─── Acciones del usuario ──────────────────────────────────────────
        /** El usuario aprueba avanzar a la siguiente etapa.
            @return true si avanzó exitosamente */
        bool approveAdvance();

        /** El usuario rechaza/no está listo. El coach enviará un tip de qué falta.
            Internamente activa un cooldown de 120s para evitar que la misma
            pregunta de aprobación se repita en cada ciclo de periodicAnalysis(). */
        void rejectAdvance();

        /** Avance manual forzado (comando /next).
            @return true si avanzó exitosamente */
        bool forceAdvance();

        // ─── Envío de mensajes ─────────────────────────────────────────────
        /** Envía la guía detallada de la etapa actual. */
        void sendStageGuidance();

        /** Envía el mensaje de aprobación ("¿Listo para avanzar?"). */
        void sendApprovalRequest();

        /** Envía el mensaje de bienvenida a la nueva etapa. */
        void sendStageWelcome();

        // ─── Callbacks ─────────────────────────────────────────────────────
        using StageChangedCallback = std::function<void(CoachingStage oldStage, CoachingStage newStage)>;
        void setStageChangedCallback(StageChangedCallback callback) { stageChangedCb_ = callback; }

        using SendMessageCallback = std::function<void(const juce::String& text, MentorMessage::Type type)>;
        void setSendMessageCallback(SendMessageCallback callback) { sendMessageCb_ = callback; }

    private:
        PhaseManager& phaseManager_;

        CoachingStage currentStage_{CoachingStage::GainStaging};
        bool initialized_{false};
        bool awaitingApproval_{false};
        bool autoRequestApproval_{true};

        /** Flags de completitud por etapa (se setean externamente por llamadas a markStageComplete). */
        bool stageCompletedFlags_[static_cast<int>(CoachingStage::COUNT)]{};

        /** Cooldown después de rejectAdvance() — evita bucle infinito de aprobación.
            Se setea al rechazar y se revisa en evaluateStageCompletion(). */
        int64_t approvalRejectedAtUs_{0};
        static constexpr int64_t kApprovalRejectedCooldownUs = 120 * 1000 * 1000; // 120s

        StageChangedCallback stageChangedCb_;
        SendMessageCallback sendMessageCb_;

        /** Envía un mensaje al chat (wrapper sobre sendMessageCb_). */
        void sendMessage(const juce::String& text, MentorMessage::Type type);

        /** Evalúa si la etapa actual está completa según métricas de PhaseManager. */
        [[nodiscard]] bool evaluateStageCompletion(const CoachEngine& engine) const noexcept;
    };

} // namespace mixcoach
