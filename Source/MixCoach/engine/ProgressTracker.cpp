#include "ProgressTracker.h"
#include "DifferenceProfile.h"
#include "../../Common/types/LogHelper.h"
#include <cmath>
#include <algorithm>

namespace mixcoach {

    // Static constexpr array needs a definition outside the class
    constexpr float ProgressTracker::kMilestoneThresholds[];

    // ═══════════════════════════════════════════════════════════════════════════
    //  computeMatchScore — Convierte deltaScore (0-1) a porcentaje 0-100
    // ═══════════════════════════════════════════════════════════════════════════

    float ProgressTracker::computeMatchScore(const DifferenceProfile& profile) noexcept
    {
        if (!profile.valid) return 0.0f;

        // Usar deltaScore si está disponible, combinar con critical/warning gaps penalty
        float score = profile.deltaScore * 100.0f;

        // Penalizar por gaps críticos
        if (profile.criticalGaps > 0)
            score -= 8.0f * profile.criticalGaps;
        if (profile.warningGaps > 0)
            score -= 3.0f * profile.warningGaps;

        return juce::jlimit(0.0f, 100.0f, score);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  takeSnapshot — Toma un snapshot si ha pasado suficiente tiempo
    // ═══════════════════════════════════════════════════════════════════════════

    bool ProgressTracker::takeSnapshot(const DifferenceProfile& profile, int64_t nowUs)
    {
        if (!profile.valid) return false;

        // Cooldown: no tomar snapshots muy seguido
        if (nowUs - lastSnapshotUs_ < kSnapshotIntervalUs && !snapshots_.empty())
            return false;

        // Límite del buffer circular
        if ((int)snapshots_.size() >= kMaxSnapshots)
            snapshots_.erase(snapshots_.begin()); // FIFO: descartar la más vieja

        ProgressSnapshot snap;
        snap.timestampUs  = nowUs;
        snap.matchScore   = computeMatchScore(profile);
        snap.lufsGap      = std::abs(profile.deltaLUFS);
        snap.crestGap     = std::abs(profile.deltaCrestFactor);
        snap.criticalGaps = profile.criticalGaps;
        snap.warningGaps  = profile.warningGaps;
        snap.lufsMix      = profile.mixIntegratedLUFS;
        snap.lufsRef      = profile.refIntegratedLUFS;

        float oldScore = snapshots_.empty() ? 0.0f : snapshots_.back().matchScore;

        snapshots_.push_back(snap);
        lastSnapshotUs_ = nowUs;

        LogHelper::writeToLog("[ProgressTracker] Snapshot " + juce::String((int)snapshots_.size())
                              + ": match=" + juce::String(snap.matchScore, 1) + "%"
                              + " | LUFS gap=" + juce::String(snap.lufsGap, 1)
                              + " | Crest gap=" + juce::String(snap.crestGap, 1)
                              + " | Gaps: " + juce::String(snap.criticalGaps) + "c/"
                              + juce::String(snap.warningGaps) + "w");

        // Verificar milestones
        checkMilestones(oldScore, snap.matchScore, nowUs);

        return true;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getFirstSnapshot — Retorna el primer snapshot (o inválido)
    // ═══════════════════════════════════════════════════════════════════════════

    ProgressSnapshot ProgressTracker::getFirstSnapshot() const noexcept
    {
        if (snapshots_.empty()) return ProgressSnapshot{};
        return snapshots_.front();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getTimelinePoints — Extrae hasta maxPoints puntos espaciados
    // ═══════════════════════════════════════════════════════════════════════════

    std::vector<ProgressSnapshot> ProgressTracker::getTimelinePoints(int maxPoints) const noexcept
    {
        if (snapshots_.empty()) return {};

        int count = (int)snapshots_.size();
        if (count <= maxPoints)
            return snapshots_; // Devuelve todos si hay pocos

        std::vector<ProgressSnapshot> result;

        // Siempre incluir el primero y el último
        result.push_back(snapshots_.front());

        // Puntos intermedios espaciados uniformemente
        int step     = count / (maxPoints - 1);
        int remaining = maxPoints - 2; // Ya tenemos primero y último
        for (int i = 0; i < remaining; ++i) {
            int idx = step * (i + 1);
            idx     = juce::jlimit(1, count - 2, idx);
            result.push_back(snapshots_[idx]);
        }

        result.push_back(snapshots_.back());
        return result;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getTrackedDurationSeconds — Duración entre primer y último snapshot
    // ═══════════════════════════════════════════════════════════════════════════

    double ProgressTracker::getTrackedDurationSeconds() const noexcept
    {
        if (snapshots_.size() < 2) return 0.0;
        return (double)(snapshots_.back().timestampUs - snapshots_.front().timestampUs) / 1'000'000.0;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getTotalProgressDelta — Mejora total desde el inicio
    // ═══════════════════════════════════════════════════════════════════════════

    float ProgressTracker::getTotalProgressDelta() const noexcept
    {
        if (snapshots_.size() < 2) return 0.0f;
        return snapshots_.back().matchScore - snapshots_.front().matchScore;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  generateSessionSummary — Resumen de progreso para el reporte final
    // ═══════════════════════════════════════════════════════════════════════════

    juce::String ProgressTracker::generateSessionSummary() const
    {
        if (snapshots_.size() < 2) return {};

        float firstScore = snapshots_.front().matchScore;
        float lastScore  = snapshots_.back().matchScore;
        float delta      = lastScore - firstScore;
        double duration  = getTrackedDurationSeconds();
        int minutes      = (int)(duration / 60.0);

        juce::String summary;

        // Línea principal: evolución del match
        summary += "Tu mezcla paso de **" + juce::String((int)firstScore)
                   + "%** a **" + juce::String((int)lastScore) + "%** de match con la referencia";

        // Si hay suficiente duración, mencionar tiempo
        if (minutes >= 2)
            summary += " en " + juce::String(minutes) + " minutos";

        summary += ".";

        // Dirección del progreso
        if (delta > 15.0f)
            summary += " Excelente progreso!";
        else if (delta > 5.0f)
            summary += " Buen avance.";
        else if (delta > 0.0f)
            summary += " Ligera mejora.";
        else if (delta > -5.0f)
            summary += " Se mantuvo estable.";
        else
            summary += " Necesita mas trabajo.";

        // Datos adicionales
        if (snapshots_.size() >= 2) {
            summary += "\n\nDetalles:\n";
            summary += "  LUFS gap inicial: " + juce::String(snapshots_.front().lufsGap, 1) + " LUFS"
                       + " | final: " + juce::String(snapshots_.back().lufsGap, 1) + " LUFS\n";
            summary += "  Crest gap inicial: " + juce::String(snapshots_.front().crestGap, 1) + " dB"
                       + " | final: " + juce::String(snapshots_.back().crestGap, 1) + " dB\n";

            if (snapshots_.front().criticalGaps > 0 || snapshots_.back().criticalGaps > 0) {
                summary += "  Gaps criticos: " + juce::String(snapshots_.front().criticalGaps)
                           + " → " + juce::String(snapshots_.back().criticalGaps) + "\n";
            }
        }

        return summary;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  checkMilestones — Detecta cruce de umbral y dispara callback
    // ═══════════════════════════════════════════════════════════════════════════

    void ProgressTracker::checkMilestones(float oldScore, float newScore, int64_t nowUs)
    {
        // Cooldown entre celebraciones
        if (nowUs - lastMilestoneUs_ < kMilestoneCooldownUs) return;

        // Solo celebrar cuando el score SUBE
        if (newScore <= oldScore) return;

        for (int i = 0; i < kNumMilestones; ++i) {
            if (milestonesReached_[i]) continue;

            // Se cruza el umbral cuando oldScore < threshold <= newScore
            if (oldScore < kMilestoneThresholds[i] && newScore >= kMilestoneThresholds[i]) {
                milestonesReached_[i] = true;
                lastMilestoneUs_      = nowUs;

                LogHelper::writeToLog("[ProgressTracker] Milestone alcanzado: "
                                      + juce::String((int)kMilestoneThresholds[i]) + "% ("
                                      + juce::String((int)oldScore) + "% → " + juce::String((int)newScore) + "%)");

                if (milestoneCallback_)
                    milestoneCallback_(oldScore, newScore, i);

                // Solo celebrar el milestone más alto alcanzado esta vez
                break;
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  reset — Limpia todos los snapshots y milestones
    // ═══════════════════════════════════════════════════════════════════════════

    void ProgressTracker::reset() noexcept
    {
        snapshots_.clear();
        lastSnapshotUs_      = 0;
        lastMilestoneUs_     = 0;
        for (auto& m : milestonesReached_) m = false;
    }

} // namespace mixcoach
