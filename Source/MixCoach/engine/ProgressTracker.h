#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <functional>

namespace mixcoach {

    // Forward declaration — DifferenceProfile se incluye en .cpp
    struct DifferenceProfile;

    // ═══════════════════════════════════════════════════════════════════════════
    //  ProgressSnapshot — Una fotografía del progreso en un instante
    // ═══════════════════════════════════════════════════════════════════════════
    struct ProgressSnapshot
    {
        int64_t timestampUs = 0;       // Cuando se tomó el snapshot
        float matchScore    = 0.0f;    // 0-100 (porcentaje de match)
        float lufsGap       = 0.0f;    // |mix - ref| LUFS
        float crestGap      = 0.0f;    // |mix - ref| crest dB
        int criticalGaps    = 0;       // Número de gaps críticos
        int warningGaps     = 0;       // Número de gaps warning
        float lufsMix       = -100.0f; // LUFS integrated de la mezcla
        float lufsRef       = -100.0f; // LUFS integrated de la referencia

        [[nodiscard]] bool isValid() const noexcept { return timestampUs > 0 && matchScore > 0.0f; }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  ProgressTracker — Toma snapshots periódicos, detecta milestones,
    //  genera resumen de sesión "antes vs después".
    // ═══════════════════════════════════════════════════════════════════════════
    class ProgressTracker
    {
    public:
        ProgressTracker() = default;

        // ═══ Configuración ═══════════════════════════════════════════════════
        static constexpr int kSnapshotIntervalUs = 120 * 1000 * 1000; // 120s entre snapshots
        static constexpr int kMaxSnapshots       = 50;                // ~100 minutos de sesión
        static constexpr int kMilestoneCooldownUs = 300 * 1000 * 1000; // 5 min entre celebraciones

        // ═══ Milestone thresholds ════════════════════════════════════════════
        // El tracker celebra cuando se CRUZA un umbral hacia arriba
        static constexpr float kMilestoneThresholds[] = {
            25.0f,  // Starting — 1/4
            45.0f,  // Progress — casi la mitad
            60.0f,  // Good — mayoría del camino
            75.0f,  // Advanced — 3/4
            90.0f   // Excellent — casi perfecto
        };

        static constexpr int kNumMilestones = sizeof(kMilestoneThresholds) / sizeof(kMilestoneThresholds[0]);

        // ═══ Callback para mensajes de celebración ═══════════════════════════
        using MilestoneCallback = std::function<void(float oldScore, float newScore, int milestoneIndex)>;
        void setMilestoneCallback(MilestoneCallback cb) { milestoneCallback_ = cb; }

        /** Toma un snapshot del DifferenceProfile actual.
            Solo toma snapshot si ha pasado suficiente tiempo desde el último.
            @param profile  El DifferenceProfile actual de la mezcla vs referencia
            @param nowUs    Timestamp actual en microsegundos
            @return         true si se tomó un snapshot nuevo */
        bool takeSnapshot(const DifferenceProfile& profile, int64_t nowUs);

        /** Retorna el snapshot más reciente (o inválido si no hay). */
        [[nodiscard]] const ProgressSnapshot& getLatestSnapshot() const noexcept { return snapshots_.back(); }

        /** Retorna el primer snapshot (inicio de sesión, o inválido si no hay). */
        [[nodiscard]] ProgressSnapshot getFirstSnapshot() const noexcept;

        /** Retorna todos los snapshots disponibles. */
        [[nodiscard]] const std::vector<ProgressSnapshot>& getSnapshots() const noexcept { return snapshots_; }

        /** Retorna los snapshots como un array circular para el timeline UI.
            Hasta 6 puntos igualmente espaciados en el tiempo. */
        [[nodiscard]] std::vector<ProgressSnapshot> getTimelinePoints(int maxPoints = 6) const noexcept;

        /** Retorna cuántos snapshots se han tomado. */
        [[nodiscard]] int getSnapshotCount() const noexcept { return (int)snapshots_.size(); }

        /** Retorna la duración total trackeada en segundos.
            Diferencia entre el primer y último snapshot. */
        [[nodiscard]] double getTrackedDurationSeconds() const noexcept;

        /** Retorna el progreso total: matchScore actual - matchScore inicial.
            Retorna 0 si no hay suficientes datos. */
        [[nodiscard]] float getTotalProgressDelta() const noexcept;

        /** Genera un resumen de la sesión para el reporte final.
            Ej: "Tu mezcla pasó de 38% a 72% de match en 25 minutos" */
        [[nodiscard]] juce::String generateSessionSummary() const;

        /** Resetea el tracker para una nueva sesión. */
        void reset() noexcept;

        /** Retorna el puntaje de match (0-100) para un DifferenceProfile.
            Usa deltaScore (0.0-1.0) y lo convierte a porcentaje. */
        static float computeMatchScore(const DifferenceProfile& profile) noexcept;

    private:
        std::vector<ProgressSnapshot> snapshots_;
        int64_t lastSnapshotUs_{0};
        int64_t lastMilestoneUs_{0};
        MilestoneCallback milestoneCallback_;

        // Milestones ya alcanzados (no volver a celebrar)
        bool milestonesReached_[kNumMilestones] = {false};

        /** Verifica si se ha cruzado un milestone y dispara el callback. */
        void checkMilestones(float oldScore, float newScore, int64_t nowUs);
    };

} // namespace mixcoach
