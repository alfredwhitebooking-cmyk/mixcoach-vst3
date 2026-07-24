#include "DensityAnalyzer.h"

namespace mixcoach {

    DensityAnalyzer::DensityAnalyzer()
    {
        reset();
    }

    DensityResult DensityAnalyzer::analyze(int trackCount,
                                            std::function<juce::String(int)> getTrackName,
                                            std::function<const float*(int)> getBandEnergies,
                                            int64_t timestampUs)
    {
        DensityResult result;
        result.timestampUs = timestampUs;

        // ═══ Throttle: solo cada ~10s ══════════════════════════════════════════
        if (timestampUs - lastAnalysisUs_ < kAnalysisIntervalUs) {
            return lastResult_;
        }
        lastAnalysisUs_ = timestampUs;

        // ═══ Si no hay suficientes tracks, no analizar ════════════════════════
        if (trackCount < 2) {
            lastResult_ = result;
            return result;
        }

        // ═══ Contar tracks activos por banda ═══════════════════════════════════
        // Para cada track, verificar en qué bandas tiene energía significativa
        for (int i = 0; i < 128; ++i) { // SlotRegistry::kMaxSlots
            const float* energies = getBandEnergies(i);
            if (energies == nullptr) continue;

            juce::String trackName = getTrackName(i);
            if (trackName.isEmpty()) continue;

            // Verificar si el track tiene señal en alguna banda
            bool hasSignal = false;
            SpectralBand dominantBand = SpectralBand::Sub;
            float maxEnergy = -100.0f;

            for (int b = 0; b < kNumDensityBands; ++b) {
                SpectralBand band = static_cast<SpectralBand>(b);
                float regionEnergy = computeRegionEnergy(energies, band);

                if (regionEnergy > kEnergyThreshold) {
                    result.trackCountPerBand[b]++;
                    if (regionEnergy > maxEnergy) {
                        maxEnergy = regionEnergy;
                        dominantBand = band;
                    }
                    hasSignal = true;
                }
            }

            // Asignar track a su banda dominante (donde más energía tiene)
            if (hasSignal) {
                int domIdx = static_cast<int>(dominantBand);
                result.trackNamesPerBand[domIdx].push_back(trackName);
            }
        }

        // ═══ Detectar congestión y regiones vacías ═════════════════════════════
        for (int b = 0; b < kNumDensityBands; ++b) {
            int count = result.trackCountPerBand[b];

            if (count >= kCriticalThreshold) {
                result.criticalBands.push_back(static_cast<SpectralBand>(b));
            }
            else if (count >= kWarningThreshold) {
                result.congestedBands.push_back(static_cast<SpectralBand>(b));
            }

            if (count == 0) {
                result.emptyBands.push_back(static_cast<SpectralBand>(b));
            }
        }

        lastResult_ = result;

        // ─── Deduplicación: solo reportar si el fingerprint cambió ────────
        juce::String newFingerprint = buildFingerprint(result);
        if (newFingerprint == lastCongestionFingerprint_) {
            // Mismo estado — devolver resultado vacío para no spammear
            lastResult_.congestedBands.clear();
            lastResult_.criticalBands.clear();
            lastResult_.emptyBands.clear();
            return lastResult_;
        }
        lastCongestionFingerprint_ = newFingerprint;

        lastResult_ = result;
        return result;
    }

    void DensityAnalyzer::reset()
    {
        DensityResult empty;
        lastResult_ = empty;
        lastAnalysisUs_ = 0;
        lastCongestionFingerprint_.clear();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  PRIVATE
    // ═══════════════════════════════════════════════════════════════════════════

    float DensityAnalyzer::computeRegionEnergy(const float* bandEnergies, SpectralBand region) noexcept
    {
        if (bandEnergies == nullptr) return -100.0f;

        int startBand = static_cast<int>(region) * 5; // 5 bandas por región
        int endBand   = startBand + 5;

        float sum = 0.0f;
        int count = 0;

        for (int b = startBand; b < endBand && b < 30; ++b) {
            if (bandEnergies[b] > -90.0f) {
                sum += bandEnergies[b];
                count++;
            }
        }

        if (count == 0) return -100.0f;
        return sum / static_cast<float>(count);
    }

} // namespace mixcoach
