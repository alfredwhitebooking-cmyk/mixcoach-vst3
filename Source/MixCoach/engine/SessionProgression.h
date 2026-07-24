#pragma once
#include <juce_core/juce_core.h>
#include <functional>

namespace mixcoach {

    // Forward declarations
    class CoachEngine;
    class AudioAnalyzer;

    // ═══════════════════════════════════════════════════════════════════════════
    //  SessionProgression — Máquina de estados de sesión de alto nivel
    //
    //  Guía al usuario a través del viaje completo de MixCoach:
    //
    //    Setup → LoadReference → DeepAnalysis → GuidedCoaching → Refinement
    //    → Report → Memory
    //
    //  A diferencia de MentorPhase (que detalla pasos técnicos de mezcla),
    //  SessionProgression trackea el ciclo de vida completo de la sesión,
    //  desde que el usuario abre el plugin hasta que guarda el reporte final.
    //
    //  Cada fase tiene:
    //    • Un nombre e ícono para la UI
    //    • Una descripción de qué ocurre en esta fase
    //    • Un tip de "cómo avanzar" para guiar al usuario
    //    • Lógica de detección automática desde el estado del motor
    //    • Callback de transición para actualizar la UI
    // ═══════════════════════════════════════════════════════════════════════════
    struct SessionProgression
    {
        // ═══════════════════════════════════════════════════════════════════════
        //  Fases del ciclo de vida de sesión
        // ═══════════════════════════════════════════════════════════════════════
        enum class Phase : uint8_t
        {
            Setup,           // 0: Configuración inicial (género, modo, nombre)
            LoadReference,   // 1: Referencia cargada
            DeepAnalysis,    // 2: Análisis profundo completado (ReferenceSummary)
            GuidedCoaching,  // 3: Coaching activo con recomendaciones y correcciones
            Refinement,      // 4: Refinamiento artístico activo (MixScore >= 70)
            Report,          // 5: Reporte de sesión generado
            Memory,          // 6: Sesión guardada al historial

            COUNT            // Total de fases (7)
        };

        // ─── Fase actual ─────────────────────────────────────────────────────
        Phase currentPhase = Phase::Setup;

        // ─── Flags de completitud por fase ───────────────────────────────────
        bool setupCompleted         = false; // Setup terminado (SetupStep::Complete)
        bool referenceLoaded        = false; // Referencia cargada
        bool deepAnalysisCompleted  = false; // ReferenceSummary disponible
        bool coachingActive         = false; // Coaching activo (recomendaciones fluyendo)
        bool refinementAchieved     = false; // Refinement alcanzado
        bool reportGenerated        = false; // Reporte generado / visto
        bool memorySaved            = false; // Sesión guardada al historial

        // ─── Timestamps de transiciones (μs) ─────────────────────────────────
        int64_t phaseStartedAtUs[static_cast<int>(Phase::COUNT)] = {};

        // ─── Callback: se dispara cuando cambia la fase activa ───────────────
        std::function<void(Phase oldPhase, Phase newPhase)> onPhaseChanged;

        // ═══════════════════════════════════════════════════════════════════════
        //  API pública
        // ═══════════════════════════════════════════════════════════════════════

        /** Avanza manualmente a una fase específica.
            Retorna true si la transición fue válida.
            La transición solo es válida si newPhase > currentPhase (no retroceder)
            o si se fuerza con force=true. */
        bool advanceTo(Phase newPhase, bool force = false);

        /** Resetea toda la progresión a Setup. */
        void reset();

        /** Retorna true si la fase está completa. */
        [[nodiscard]] bool isPhaseComplete(Phase phase) const noexcept;

        /** Retorna el progreso global 0.0-1.0 (basado en fase actual). */
        [[nodiscard]] float getOverallProgress() const noexcept;

        /** Retorna el tiempo transcurrido desde que se inició la fase actual. */
        [[nodiscard]] int64_t getPhaseDurationUs() const noexcept;

        /** Detección automática desde el estado del motor.
            Examina CoachEngine y AudioAnalyzer para inferir la fase correcta.
            Se llama desde periodicAnalysis() (~8s).
            @return La fase detectada, sin cambiar el estado interno. */
        [[nodiscard]] static Phase detectPhase(const CoachEngine& engine,
                                                 const AudioAnalyzer& analyzer) noexcept;

        /** Actualiza la progresión desde un estado externo.
            Detecta la fase actual y avanza automáticamente si es necesario.
            Retorna true si la fase cambió. */
        bool updateFromEngine(const CoachEngine& engine, const AudioAnalyzer& analyzer);

        // ═══════════════════════════════════════════════════════════════════════
        //  Metadatos de fase
        // ═══════════════════════════════════════════════════════════════════════

        [[nodiscard]] static const char* phaseName(Phase phase) noexcept;
        [[nodiscard]] static const char* phaseShortName(Phase phase) noexcept;
        [[nodiscard]] static const char* phaseIcon(Phase phase) noexcept;
        [[nodiscard]] static const char* phaseDescription(Phase phase) noexcept;
        [[nodiscard]] static const char* phaseTip(Phase phase) noexcept;
        [[nodiscard]] static const char* phaseEmoji(Phase phase) noexcept;

        // ═══════════════════════════════════════════════════════════════════════
        //  Serialización JSON
        // ═══════════════════════════════════════════════════════════════════════

        void toJson(juce::DynamicObject& obj) const;
        static SessionProgression fromJson(const juce::DynamicObject& obj);

    private:
        // ─── Marca una fase como completada y actualiza flags ────────────────
        void markPhaseCompleted(Phase phase) noexcept;
    };

} // namespace mixcoach
