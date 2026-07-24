#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  SpectralBand — Regiones espectrales de 6 bandas
    // ═══════════════════════════════════════════════════════════════════════════
    enum class SpectralBand : uint8_t
    {
        Sub       = 0, // 0-86 Hz
        Bass      = 1, // 86-301 Hz
        LowMid    = 2, // 301-1076 Hz
        HiMid     = 3, // 1076-3532 Hz
        Presence  = 4, // 3532-8355 Hz
        Air       = 5  // 8355-16458 Hz
    };

    // NOTA: kNumSpectralBands = 30 está definido en Constants.h para las 30 bandas FFT.
    // Aquí usamos 6 regiones espectrales agregadas.
    inline constexpr int kNumDensityBands = 6;

    /// Retorna nombre legible de la banda espectral.
    inline const char* spectralBandName(SpectralBand b) noexcept
    {
        switch (b) {
            case SpectralBand::Sub:       return "Sub (0-86Hz)";
            case SpectralBand::Bass:      return "Bass (86-301Hz)";
            case SpectralBand::LowMid:    return "Low-Mid (301-1076Hz)";
            case SpectralBand::HiMid:     return "Hi-Mid (1076-3532Hz)";
            case SpectralBand::Presence:  return "Presence (3532-8355Hz)";
            case SpectralBand::Air:       return "Air (8355-16458Hz)";
            default:                      return "Unknown";
        }
    }

    /// Frecuencia central aproximada de la banda para mensajes.
    inline const char* spectralBandCenter(SpectralBand b) noexcept
    {
        switch (b) {
            case SpectralBand::Sub:       return "60Hz";
            case SpectralBand::Bass:      return "200Hz";
            case SpectralBand::LowMid:    return "800Hz";
            case SpectralBand::HiMid:     return "2kHz";
            case SpectralBand::Presence:  return "5kHz";
            case SpectralBand::Air:       return "12kHz";
            default:                      return "";
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  DensityResult — Resultado del análisis de densidad espectral
    // ═══════════════════════════════════════════════════════════════════════════
    struct DensityResult
    {
        // ─── Conteo de tracks activos por banda ─────────────────────────────
        std::array<int, kNumDensityBands> trackCountPerBand{};

        // ─── Nombres de tracks por banda (para mensajes) ────────────────────
        std::array<std::vector<juce::String>, kNumDensityBands> trackNamesPerBand;

        // ─── Bandas con congestión (>3 tracks) ──────────────────────────────
        std::vector<SpectralBand> congestedBands;

        // ─── Bandas congestionadas críticamente (>5 tracks) ─────────────────
        std::vector<SpectralBand> criticalBands;

        // ─── Bandas vacías (<1 track con señal significativa) ───────────────
        std::vector<SpectralBand> emptyBands;

        // ─── Timestamp del análisis ─────────────────────────────────────────
        int64_t timestampUs = 0;

        bool valid() const noexcept
        {
            return !congestedBands.empty() || !emptyBands.empty();
        }

        /// Genera un mensaje de advertencia de congestión.
        [[nodiscard]] juce::String buildCongestionMessage() const
        {
            juce::String msg;

            // Congestión crítica (>5 tracks)
            for (auto band : criticalBands) {
                int idx   = static_cast<int>(band);
                int count = trackCountPerBand[idx];
                msg += "\\xF0\\x9F\\x94\\xB4 **Congesti\\xC3\\xB3n cr\\xC3\\xADtica en "
                       + juce::String(spectralBandName(band)) + "**:\\n";
                msg += "  Tienes " + juce::String(count)
                       + " elementos compitiendo por el mismo espacio. Esto causa fatiga auditiva.\\n";
                msg += "  Sugerencias:\\n";

                const auto& names = trackNamesPerBand[idx];
                if (names.size() >= 2) {
                    msg += "  - Mueve \\\"" + names[0] + "\\\" una octava arriba o abajo para liberar espacio\\n";
                    msg += "  - Aplica un cut de 2-3dB en "
                           + juce::String(spectralBandCenter(band)) + " en \"" + names[1]
                           + "\" para dar espacio a los demas\n";
                    if (names.size() >= 3)
                        msg += "  - Baja " + names[2] + " 2-3dB en esta secci\\xC3\\xB3n\\n";
                }
                msg += "\\n";
            }

            // Congestión warning (>3 tracks)
            for (auto band : congestedBands) {
                if (std::find(criticalBands.begin(), criticalBands.end(), band) != criticalBands.end())
                    continue; // Ya reportado como crítico
                int idx   = static_cast<int>(band);
                int count = trackCountPerBand[idx];
                msg += "\\xF0\\x9F\\x9F\\xA1 **Atenci\\xC3\\xB3n en "                           + juce::String(spectralBandName(band)) + "**:\n";
                msg += "  " + juce::String(count) + " elementos en la misma region. "
                       + "Considera hacer espacio con EQ o cambios de arreglo.\n\n";
            }

            // Regiones vacías
            for (auto band : emptyBands) {
                int idx = static_cast<int>(band);
                msg += "\\xF0\\x9F\\xAA\\A6 **Regi\\xC3\\xB3n vac\\xC3\\xADa en "
                       + juce::String(spectralBandName(band)) + "**:\\n";
                if (band == SpectralBand::Sub || band == SpectralBand::Bass)
                    msg += "  Los graves estan casi vacios. "
                           "?Falta un bajo o un 808?\n";
                else if (band == SpectralBand::Presence || band == SpectralBand::Air)
                    msg += "  Los agudos estan muy vacios. "
                           "La mezcla puede sonar apagada. Considera anadir brillo.\n";
                else
                    msg += "  Hay muy poca actividad en esta region. "
                           "Podria beneficiarse de mas contenido.\n";
                msg += "\\n";
            }

            return msg;
        }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  DensityAnalyzer — Analiza congestión espectral del arreglo
    //
    //  Cuenta tracks activos por banda de frecuencia (6 regiones) y detecta:
    //    • Congestión (>3 tracks en misma banda → warning, >5 → crítico)
    //    • Regiones vacías (<1 track con señal significativa)
    //  Usa las bandEnergies[30] de cada track para determinar qué bandas ocupa.
    // ═══════════════════════════════════════════════════════════════════════════
    class DensityAnalyzer
    {
    public:
        DensityAnalyzer();

        /// Analiza la densidad espectral de todas las pistas activas.
        /// @param trackCount     Número de pistas activas
        /// @param getTrackNames  Función que retorna el nombre de una pista dado su slotIndex
        /// @param getBandEnergy  Función que retorna bandEnergies[30] de una pista dado su slotIndex
        /// @param timestampUs    Timestamp actual en microsegundos
        /// @return Resultado del análisis (vacío si no hay suficientes datos)
        DensityResult analyze(int trackCount,
                              std::function<juce::String(int)> getTrackName,
                              std::function<const float*(int)> getBandEnergies,
                              int64_t timestampUs);

        /// Retorna el último resultado del análisis.
        [[nodiscard]] const DensityResult& getLastResult() const noexcept { return lastResult_; }

        /// Resetea el analizador.
        void reset();

    private:
        // ─── Umbrales de congestión ─────────────────────────────────────────
        static constexpr int kWarningThreshold  = 3; // >3 tracks = warning
        static constexpr int kCriticalThreshold = 5; // >5 tracks = critical
        static constexpr float kEnergyThreshold = -50.0f; // dBFS mínimo para considerar activa

        // ─── Mapeo de 30 bandas → 6 regiones (cada 5 bandas = 1 región) ────
        static SpectralBand bandIndexToRegion(int bandIdx) noexcept
        {
            if (bandIdx < 0) return SpectralBand::Sub;
            if (bandIdx < 5)  return SpectralBand::Sub;
            if (bandIdx < 10) return SpectralBand::Bass;
            if (bandIdx < 15) return SpectralBand::LowMid;
            if (bandIdx < 20) return SpectralBand::HiMid;
            if (bandIdx < 25) return SpectralBand::Presence;
            return SpectralBand::Air;
        }

        DensityResult lastResult_;
        int64_t lastAnalysisUs_{0};
        static constexpr int64_t kAnalysisIntervalUs = 10 * 1000 * 1000; // 10s entre análisis

        // ─── Deduplicación: fingerprint de la última congestión reportada ───
        juce::String lastCongestionFingerprint_;

        /// Genera un fingerprint único del conjunto de bandas congestionadas.
        static juce::String buildFingerprint(const DensityResult& result) noexcept
        {
            juce::String fp;
            for (auto b : result.criticalBands)  fp += "C" + juce::String(static_cast<int>(b));
            for (auto b : result.congestedBands) fp += "W" + juce::String(static_cast<int>(b));
            for (auto b : result.emptyBands)     fp += "E" + juce::String(static_cast<int>(b));
            return fp;
        }

        /// Computa la energía promedio de un track en una región espectral.
        static float computeRegionEnergy(const float* bandEnergies, SpectralBand region) noexcept;
    };

} // namespace mixcoach
