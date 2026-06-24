#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include "TrackRole.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  MixPriorityEngine — Score multidimensional para issues de mezcla
    //
    //  Fórmula: finalScore = rawSeverity × (roleWeight / 10) × domainWeight × genreModifier
    //
    //  Factores:
    //    • rawSeverity: 0.0-1.0, qué tan grave es el issue (del detector)
    //    • roleWeight:  1-10, qué tan importante es el rol en la mezcla
    //    • domainWeight: 0.8-1.2, qué tan crítico es el dominio (gain > dynamics > tonal)
    //    • genreModifier: 0.8-1.2, ajuste por género (ej: bass en Reggaeton pesa más)
    //
    //  Sin IA — reglas C++, 0 tokens, instantáneo.
    // ═══════════════════════════════════════════════════════════════════════════

    // ═══ RolePriority — Peso de importancia por rol (1-10) ═══════════════════
    //  10 = crítico (Vocal Lead), 1 = irrelevante (Unknown)
    struct RolePriority
    {
        TrackRole role = TrackRole::Unknown;
        uint8_t weight = 1;  // 1-10

        static constexpr uint8_t kMinWeight = 1;
        static constexpr uint8_t kMaxWeight = 10;
    };

    // ═══ DomainWeight — Factor de criticidad por dominio ═════════════════════
    //  1.2 = gain issues son los más críticos (pueden dañar monitores/audición)
    //  1.0 = dynamics
    //  0.9 = tonal (menos urgente, más subjetivo)
    //  0.8 = spatial (fase/estereo, importante pero no urgente)
    struct DomainWeight
    {
        static constexpr float kGain     = 1.20f;
        static constexpr float kDynamics = 1.00f;
        static constexpr float kTonal    = 0.90f;
        static constexpr float kSpatial  = 0.80f;
        static constexpr float kMasking  = 0.85f;
        static constexpr float kDefault  = 1.00f;

        /** Retorna el factor de peso para un string de dominio. */
        [[nodiscard]] static float forDomain(const juce::String& domain) noexcept
        {
            if (domain == "gain")     return kGain;
            if (domain == "dynamics") return kDynamics;
            if (domain == "tonal")    return kTonal;
            if (domain == "spatial")  return kSpatial;
            if (domain == "masking")  return kMasking;
            return kDefault;
        }
    };

    // ═══ PriorityScore — Resultado del scoring multidimensional ══════════════
    struct PriorityScore
    {
        int slotIndex    = -1;
        float rawSeverity   = 0.0f; // Severidad original del detector
        uint8_t roleWeight  = 1;    // Peso del rol (1-10)
        float domainWeight  = 1.0f; // Peso del dominio
        float genreModifier = 1.0f; // Modificador por género
        float finalScore    = 0.0f; // rawSeverity × (roleWeight/10) × domainWeight × genreModifier

        // Desglose para debugging
        juce::String domain;
        juce::String issueType;
        juce::String trackName;
        juce::String roleName;

        [[nodiscard]] bool isValid() const noexcept { return slotIndex >= 0 && finalScore > 0.0f; }

        /** Retorna un resumen textual para debugging o LLM context. */
        [[nodiscard]] juce::String toSummary() const
        {
            return trackName + " [" + roleName + "] " + domain + "/" + issueType
                   + " score=" + juce::String(finalScore, 3)
                   + " (sev=" + juce::String(rawSeverity, 2)
                   + " × role=" + juce::String(roleWeight)
                   + " × domain=" + juce::String(domainWeight, 2)
                   + " × genre=" + juce::String(genreModifier, 2) + ")";
        }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  MixPriorityEngine — Clase principal de priorización
    // ═══════════════════════════════════════════════════════════════════════════
    class MixPriorityEngine
    {
    public:
        MixPriorityEngine() = default;
        ~MixPriorityEngine() = default;

        // ═══ NO copia ═══════════════════════════════════════════════════════════
        MixPriorityEngine(const MixPriorityEngine&)            = delete;
        MixPriorityEngine& operator=(const MixPriorityEngine&) = delete;

        // ═══ Role Priority Table ═══════════════════════════════════════════════

        /** Retorna el peso de importancia para un TrackRole (1-10). */
        [[nodiscard]] static uint8_t getRoleWeight(TrackRole role) noexcept;

        /** Retorna el nombre del peso para display (ej: "CRITICAL", "HIGH", "MEDIUM"). */
        [[nodiscard]] static const char* getRoleWeightLabel(uint8_t weight) noexcept;

        // ═══ Genre Modifiers ═══════════════════════════════════════════════════

        /** Retorna el modificador de género para un dominio específico.
            Ej: en Reggaeton, bass issues tienen modifier > 1.0 (más importantes).
            En Rock, guitar issues tienen modifier > 1.0.
            En Classical, tonal issues tienen modifier > 1.0. */
        [[nodiscard]] static float getGenreModifier(const juce::String& genre,
                                                       const juce::String& domain) noexcept;

        // ═══ Scoring ═══════════════════════════════════════════════════════════

        /** Computa el PriorityScore completo para un issue.
            @param rawSeverity  Severidad original (0.0-1.0)
            @param role         TrackRole de la pista
            @param domain       String de dominio ("gain", "dynamics", "tonal", "spatial")
            @param issueType    String de tipo de issue ("CLIPPING", "SOBRECOMPRIMIDO", etc.)
            @param trackName    Nombre de la pista (para debugging/display)
            @param genre        Género musical (para modifier, vacío = 1.0)
            @return PriorityScore completo */
        [[nodiscard]] static PriorityScore computeScore(float rawSeverity,
                                                         TrackRole role,
                                                         const juce::String& domain,
                                                         const juce::String& issueType,
                                                         const juce::String& trackName,
                                                         const juce::String& genre = {}) noexcept;

        /** Sortea un vector de PriorityScore por finalScore descendente.
            Los scores más altos (más urgentes) quedan primero. */
        static void sortByPriority(std::vector<PriorityScore>& scores) noexcept;

        /** Filtra y retorna los top N scores por finalScore.
            @param scores  Vector de scores (se modifica in-place)
            @param maxCount  Máximo número de resultados
            @return Vector con los top N (orden descendente) */
        [[nodiscard]] static std::vector<PriorityScore> getTopPriority(
            const std::vector<PriorityScore>& scores,
            int maxCount = 5) noexcept;

        /** Convierte un vector de PriorityScore a texto formateado para LLM context.
            Incluye score, rol, dominio y tipo de issue. */
        [[nodiscard]] static juce::String scoresToLLMContext(
            const std::vector<PriorityScore>& scores) noexcept;

    private:
        // ═══ Static role weight table ══════════════════════════════════════════
        static constexpr std::pair<TrackRole, uint8_t> kRoleWeights[] = {
            // 10 — CRITICAL: elementos que definen la canción
            { TrackRole::VozPrincipal, 10 },

            // 9 — FOUNDATION: el groove y el pulso
            { TrackRole::Kick,          9 },
            { TrackRole::Kick808,       9 },
            { TrackRole::Snare,         9 },
            { TrackRole::ReggaetonKick, 9 },
            { TrackRole::BassSub,       9 },
            { TrackRole::Bass808,       9 },
            { TrackRole::BassFinger,    9 },
            { TrackRole::BassPick,      9 },
            { TrackRole::BassSynth,     9 },
            { TrackRole::BassBus,       9 },

            // 8 — LEAD: instrumentos/roles que llevan la melodía
            { TrackRole::SnareTrap,     8 },
            { TrackRole::GuitarLead,    8 },
            { TrackRole::SynthLead,     8 },
            { TrackRole::VozFondo,      8 },
            { TrackRole::VozDouble,     8 },

            // 7 — RHYTHM / HARMONY: el cuerpo de la mezcla
            { TrackRole::Tom,           7 },
            { TrackRole::TomFloor,      7 },
            { TrackRole::GuitarRhythm,  7 },
            { TrackRole::GuitarElectric,7 },
            { TrackRole::KeysPiano,     7 },
            { TrackRole::KeysElectric,  7 },
            { TrackRole::KeysOrgan,     7 },
            { TrackRole::VozBus,        7 },
            { TrackRole::GuitarBus,     7 },

            // 6 — TEXTURE: capas de relleno
            { TrackRole::HiHat,         6 },
            { TrackRole::HiHatOpen,     6 },
            { TrackRole::SynthPad,      6 },
            { TrackRole::SynthPluck,    6 },
            { TrackRole::Strings,       6 },
            { TrackRole::Brass,         6 },
            { TrackRole::Winds,         6 },
            { TrackRole::KeysBus,       6 },
            { TrackRole::MelodyBus,     6 },
            { TrackRole::DrumBus,       6 },

            // 5 — PERCUSSION / FX: adornos
            { TrackRole::Percussion,    5 },
            { TrackRole::Clap,          5 },
            { TrackRole::Crash,         5 },
            { TrackRole::Ride,          5 },
            { TrackRole::Adlibs,        5 },
            { TrackRole::DrumRoom,      5 },
            { TrackRole::GuitarAcoustic,5 },

            // 3 — ATMOSPHERE: detalles y ambiente
            { TrackRole::FxRiser,       3 },
            { TrackRole::FxImpact,      3 },
            { TrackRole::FxAmbience,    3 },
            { TrackRole::FxNoise,       3 },

            // 1 — DEFAULT: sin rol o master
            { TrackRole::Unknown,       1 },
            { TrackRole::Master,        1 },
        };

        static constexpr int kNumRoleWeights = sizeof(kRoleWeights) / sizeof(kRoleWeights[0]);
    };

} // namespace mixcoach
