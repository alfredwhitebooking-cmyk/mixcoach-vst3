#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#include <vector>
#include "TrackRole.h"

namespace mixcoach {

    // Forward declarations
    class CorrectionLearner;

    // ═══════════════════════════════════════════════════════════════════════════
    //  FeedbackEntry — Registro granular de un evento de feedback
    //
    //  Se crea cada vez que una recomendación del coach se finaliza
    //  (Applied/OverApplied/UnderApplied/Ignored). Incluye metadatos
    //  para análisis posterior: dominio, slot, ratio de aplicación,
    //  tiempo de reacción del usuario, etc.
    // ═══════════════════════════════════════════════════════════════════════════
    struct FeedbackEntry
    {
        int64_t timestampUs          = 0;
        int slotIndex                = -1;
        juce::String trackName;
        juce::String domain;         // "gain", "tonal", "dynamics", "spatial"

        int domainInt                = 0; // TrackRecommendation::Domain as int
        int finalStatus              = 0; // TrackRecommendation::Status as int (Applied=1, OverApplied=2, ...)
        float appliedRatio           = 0.0f; // fraction of target achieved
        float beforeValue            = 0.0f;
        float afterValue             = 0.0f;
        float absDeltaDb             = 0.0f; // absolute change in dB

        // Metadata
        int64_t recommendationAgeUs  = 0; // how long the rec was active before outcome
        bool hadFollowUp             = false; // whether a follow-up was generated
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  FeedbackStats — Estadísticas computadas para un periodo o dominio
    // ═══════════════════════════════════════════════════════════════════════════
    struct FeedbackStats
    {
        // ─── Conteos absolutos ──────────────────────────────────────────────
        int total         = 0;
        int accepted      = 0; // Applied
        int overApplied   = 0; // OverApplied
        int underApplied  = 0; // UnderApplied
        int ignored       = 0; // Ignored / Superseded

        // ─── Ratios (0.0-1.0) ──────────────────────────────────────────────
        float acceptanceRate    = 0.0f; // accepted / total
        float overRate          = 0.0f;
        float underRate         = 0.0f;
        float ignoreRate        = 0.0f;

        // ─── Tendencia (últimos 10 vs previos 10) ──────────────────────────
        float recentAcceptance  = 0.0f; // acceptance rate in last 10 outcomes
        float previousAcceptance = 0.0f; // acceptance rate in 10 before that
        float trend             = 0.0f; // + = improving, - = declining

        // ─── Per-domain breakdown ──────────────────────────────────────────
        int perDomainTotal[4]     = {}; // gain, tonal, dynamics, spatial
        int perDomainAccepted[4]  = {};

        // ─── Tiempo de reacción promedio (μs) ──────────────────────────────
        int64_t avgReactionTimeUs = 0;

        [[nodiscard]] juce::String toShortReport() const;
        [[nodiscard]] bool hasData() const noexcept { return total > 0; }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  FeedbackCollector — Recolecta, analiza y ajusta basado en feedback
    //
    //  Propósito:
    //    1. Registrar cada resultado de recomendación con metadatos ricos
    //    2. Computar estadísticas de aceptación/rechazo por dominio y global
    //    3. Ajustar automáticamente los thresholds de CorrectionLearner
    //       según el comportamiento del usuario
    //
    //  Integración:
    //    • recordOutcome() se llama desde verifyTrackCorrections() cuando
    //      una recomendación se finaliza (Applied/OverApplied/UnderApplied/Ignored)
    //    • adjustLearnerThresholds() se llama periódicamente desde
    //      periodicAnalysis() para ajustar CorrectionLearner
    //    • getStats() se usa para reportes y logging
    // ═══════════════════════════════════════════════════════════════════════════
    class FeedbackCollector
    {
    public:
        FeedbackCollector() = default;

        // ═══ API principal ═══════════════════════════════════════════════════

        /** Registra el resultado de una recomendación finalizada.
            @param slotIndex     Slot de la pista
            @param trackName     Nombre de la pista
            @param domain        Dominio (0=Gain, 1=Tonal, 2=Dynamics, 3=Spatial)
            @param finalStatus   Estado final (0=Pending, 1=Applied, 2=OverApplied,
                                 3=UnderApplied, 4=Ignored, 5=Superseded)
            @param appliedRatio  Fracción del target alcanzado
            @param beforeValue   Valor inicial de la métrica
            @param afterValue    Valor después de la corrección
            @param hadFollowUp   Si se generó un follow-up
            @param ageUs         Tiempo que estuvo activa la recomendación */
        void recordOutcome(int slotIndex,
                           const juce::String& trackName,
                           int domain,
                           int finalStatus,
                           float appliedRatio,
                           float beforeValue,
                           float afterValue,
                           bool hadFollowUp,
                           int64_t ageUs);

        /** Ajusta los thresholds de CorrectionLearner según el feedback recolectado.
            Se llama periódicamente (cada ~10 nuevas entradas).
            @param learner  Referencia al CorrectionLearner a ajustar */
        void adjustLearnerThresholds(CorrectionLearner& learner) const;

        // ═══ Consultas ═══════════════════════════════════════════════════════

        /** Retorna estadísticas globales (todas las entradas). */
        [[nodiscard]] FeedbackStats getGlobalStats() const noexcept;

        /** Retorna estadísticas filtradas por dominio. */
        [[nodiscard]] FeedbackStats getDomainStats(int domainInt) const noexcept;

        /** Retorna estadísticas de las últimas N entradas. */
        [[nodiscard]] FeedbackStats getRecentStats(int count = 20) const noexcept;

        /** Retorna true si hay suficientes datos para ajustar thresholds. */
        [[nodiscard]] bool hasSignificantData() const noexcept;

        /** Genera un resumen del feedback del usuario para inyectar en el prompt del LLM.
            Incluye tasa de aceptación, tendencia, desglose por dominio y consejo
            sobre cómo ajustar el tono según el comportamiento del usuario. */
        [[nodiscard]] juce::String toLLMContext() const;

        // ═══ Historial de ajustes de thresholds ═══════════════════════════════

        /** Cada vez que adjustLearnerThresholds() modifica los parámetros del
            CorrectionLearner, se registra una entrada aquí con el trigger,
            los valores anteriores y los nuevos valores. */
        struct ThresholdAdjustment
        {
            int64_t timestampUs = 0;
            juce::String trigger;      // e.g. "High acceptance (75%)"
            int minCorrectionsBefore = 2;
            int minCorrectionsAfter  = 2;
            float boostBefore        = 0.10f;
            float boostAfter         = 0.10f;
            float maxBoostBefore     = 0.30f;
            float maxBoostAfter      = 0.30f;
            float spectralThresholdBefore = 0.75f;
            float spectralThresholdAfter  = 0.75f;
            bool wasReset            = false; // true si se reseteó a defaults
        };

        [[nodiscard]] const std::vector<ThresholdAdjustment>& getAdjustmentHistory() const noexcept
        {
            return adjustmentHistory_;
        }

        // ═══ Acceso a datos crudos ═══════════════════════════════════════════

        [[nodiscard]] const std::vector<FeedbackEntry>& getEntries() const noexcept { return entries_; }

        [[nodiscard]] int getTotalEntries() const noexcept { return (int)entries_.size(); }

        /** Limpia todas las entradas. */
        void reset() noexcept { entries_.clear(); }

        // ═══ Persistencia ════════════════════════════════════════════════════

        void toJson(juce::DynamicObject& obj) const;
        void fromJson(const juce::DynamicObject& obj);

        // ═══ Debug ═══════════════════════════════════════════════════════════

        [[nodiscard]] juce::String dumpReport() const;

    private:
        std::vector<FeedbackEntry> entries_;
        mutable std::vector<ThresholdAdjustment> adjustmentHistory_;

        static constexpr int kMinEntriesForAdjustment = 5;  // Min entries before adjusting
        static constexpr int kMaxEntries              = 200; // Max stored entries (FIFO)

        // ─── Compute stats from a range of entries ──────────────────────────
        [[nodiscard]] FeedbackStats computeStats(int startIdx, int count) const noexcept;
    };

} // namespace mixcoach
