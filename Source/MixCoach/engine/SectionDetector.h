#pragma once
#include <juce_core/juce_core.h>
#include <array>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  SectionType — Tipos de sección musical detectados
    // ═══════════════════════════════════════════════════════════════════════════
    enum class SectionType : uint8_t
    {
        Unknown = 0,
        Intro,   // Baja energía, pocos elementos
        Verse,   // Energía media, estructura normal
        Chorus,  // Alta energía, drop, climax
        Bridge,  // Transición, energía decreciente
        Outro    // Baja energía, fading out
    };

    /// Retorna nombre legible del tipo de sección.
    inline const char* sectionTypeName(SectionType t) noexcept
    {
        switch (t) {
            case SectionType::Intro:  return "Intro";
            case SectionType::Verse:  return "Verso";
            case SectionType::Chorus: return "Coro";
            case SectionType::Bridge: return "Puente";
            case SectionType::Outro:  return "Outro";
            default:                  return "Desconocido";
        }
    }

    /// Retorna emoji representativo del tipo de sección.
    inline const char* sectionTypeEmoji(SectionType t) noexcept
    {
        switch (t) {
            case SectionType::Intro:  return "\xF0\x9F\x8C\xA5"; // 🌥
            case SectionType::Verse:  return "\xF0\x9F\x93\x9D"; // 📝
            case SectionType::Chorus: return "\xF0\x9F\x94\xA5"; // 🔥
            case SectionType::Bridge: return "\xF0\x9F\x8C\x89"; // 🌉
            case SectionType::Outro:  return "\xF0\x9F\x8C\x92"; // 🌒
            default:                  return "\xE2\x9D\x93";     // ❓
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  SectionInfo — Información de una sección detectada
    // ═══════════════════════════════════════════════════════════════════════════
    struct SectionInfo
    {
        SectionType type        = SectionType::Unknown;
        float startTimeSec      = 0.0f; // Tiempo de inicio en segundos
        float avgEnergyDb       = -80.0f; // Energía RMS promedio
        float avgSpectralCentroid = 0.0f; // Centroide espectral promedio (Hz)
        float avgCorrelation    = 0.0f; // Correlación estéreo promedio
        int64_t firstSeenUs     = 0; // Timestamp en μs cuando se detectó

        bool valid() const noexcept { return type != SectionType::Unknown; }

        /// Retorna texto formateado para el LLM context.
        juce::String toContextString(float currentTimeSec) const
        {
            juce::String s;
            s += juce::String(sectionTypeEmoji(type)) + " " + juce::String(sectionTypeName(type));
            s += " (" + juce::String(currentTimeSec, 0) + "s";
            if (avgEnergyDb > -80.0f)
                s += ", " + juce::String(avgEnergyDb, 1) + " dB RMS";
            s += ")";
            return s;
        }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  SectionDetector — Detecta cambios de sección musical en vivo
    //
    //  Analiza la energía RMS del master cada ~2s y detecta transiciones
    //  cuando la energía cambia >20% entre ventanas consecutivas.
    //  Clasifica la sección según el perfil de energía y posición en la canción.
    // ═══════════════════════════════════════════════════════════════════════════
    class SectionDetector
    {
    public:
        SectionDetector();

        /// Analiza un nuevo frame de energía RMS y detecta si hubo transición.
        /// @param currentRmsDb    Energía RMS actual del master en dB
        /// @param currentLufs     LUFS short-term actual
        /// @param currentCorr     Correlación estéreo actual
        /// @param currentCentroid Centroide espectral actual (Hz), 0 si no disponible
        /// @param songTimeSec     Tiempo actual de la canción en segundos
        /// @param timestampUs     Timestamp en microsegundos
        /// @return true si se detectó una transición de sección
        bool analyzeFrame(float currentRmsDb,
                          float currentLufs,
                          float currentCorr,
                          float currentCentroid,
                          float songTimeSec,
                          int64_t timestampUs);

        /// Retorna la sección actual.
        [[nodiscard]] const SectionInfo& getCurrentSection() const noexcept { return currentSection_; }

        /// Retorna la sección anterior (útil para contexto de transición).
        [[nodiscard]] const SectionInfo& getPreviousSection() const noexcept { return previousSection_; }

        /// Retorna un texto formateado con la sección actual para el LLM context.
        [[nodiscard]] juce::String getSectionContext() const;

        /// Retorna true si se detectó una transición en el último frame.
        [[nodiscard]] bool hasRecentTransition() const noexcept { return hasRecentTransition_; }

        /// Resetea el detector (nueva canción/referencia).
        void reset();

    private:
        // ─── Historial de energía (ring buffer de 5 frames ~10s) ────────────
        static constexpr int kHistorySize = 5;
        std::array<float, kHistorySize> energyHistoryDb_{};
        int historyIndex_ = 0;
        int historyCount_ = 0;

        // ─── Estado de la sección ────────────────────────────────────────────
        SectionInfo currentSection_;
        SectionInfo previousSection_;
        bool hasRecentTransition_ = false;

        // ─── Cooldowns ───────────────────────────────────────────────────────
        int64_t lastAnalysisUs_{0};
        int64_t lastTransitionUs_{0};
        static constexpr int64_t kAnalysisIntervalUs  = 2 * 1000 * 1000; // 2s entre análisis
        static constexpr int64_t kTransitionCooldownUs = 8 * 1000 * 1000; // 8s entre transiciones
        static constexpr float kTransitionThreshold    = 0.20f;          // 20% cambio de energía

        // ─── Constantes de clasificación ─────────────────────────────────────
        static constexpr float kLowEnergyThreshold  = -25.0f; // dB RMS — por debajo = "baja energía"
        static constexpr float kHighEnergyThreshold = -14.0f; // dB RMS — por encima = "alta energía"

        /// Clasifica el tipo de sección basado en energía, tiempo de canción e historial.
        SectionType classifySection(float avgEnergyDb, float songTimeSec) const;

        /// Computa la energía RMS promedio del historial reciente.
        [[nodiscard]] float getAverageEnergy() const;

        /// Detecta si hubo un cambio significativo de energía.
        [[nodiscard]] bool detectTransition(float currentEnergy, float avgEnergy) const;
    };

} // namespace mixcoach
