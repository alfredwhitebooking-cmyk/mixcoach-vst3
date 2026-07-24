#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <unordered_map>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  ProblemType — Tipos de problemas que el Coach puede detectar
    //  Mapea 1:1 con los problem_types definidos en plugin_db.json
    // ═══════════════════════════════════════════════════════════════════════════
    enum class ProblemType : uint8_t
    {
        Gain,                // Track level too high/low
        Clipping,            // Digital clipping
        Masking,             // Spectral masking between tracks
        TonalExcess,         // Too much energy in a spectral region
        TonalDeficit,        // Not enough energy in a spectral region
        DynamicsOvercompressed, // Crest factor too low
        DynamicsTooDynamic,  // Crest factor too high
        Phase,               // Correlation issues
        Spatial,             // Stereo width issues
        Reverb,              // Need for ambience/depth
        Saturation,          // Need for harmonic distortion
        Limiting,            // Need for brickwall limiting
        Unknown
    };

    /** Convierte string a ProblemType (case-insensitive). */
    inline ProblemType problemTypeFromString(const juce::String& str) noexcept
    {
        auto lower = str.trim().toLowerCase();
        if (lower == "gain")                  return ProblemType::Gain;
        if (lower == "clipping")              return ProblemType::Clipping;
        if (lower == "masking")               return ProblemType::Masking;
        if (lower == "tonal_excess")          return ProblemType::TonalExcess;
        if (lower == "tonal_deficit")         return ProblemType::TonalDeficit;
        if (lower == "dynamics_overcompressed") return ProblemType::DynamicsOvercompressed;
        if (lower == "dynamics_toodynamic")   return ProblemType::DynamicsTooDynamic;
        if (lower == "phase")                 return ProblemType::Phase;
        if (lower == "spatial")               return ProblemType::Spatial;
        if (lower == "reverb")                return ProblemType::Reverb;
        if (lower == "saturation")            return ProblemType::Saturation;
        if (lower == "limiting")              return ProblemType::Limiting;
        return ProblemType::Unknown;
    }

    /** Convierte ProblemType a string legible para la UI. */
    inline const char* problemTypeToString(ProblemType type) noexcept
    {
        switch (type) {
            case ProblemType::Gain:                  return "gain";
            case ProblemType::Clipping:              return "clipping";
            case ProblemType::Masking:               return "masking";
            case ProblemType::TonalExcess:           return "tonal_excess";
            case ProblemType::TonalDeficit:          return "tonal_deficit";
            case ProblemType::DynamicsOvercompressed: return "dynamics_overcompressed";
            case ProblemType::DynamicsTooDynamic:    return "dynamics_toodynamic";
            case ProblemType::Phase:                 return "phase";
            case ProblemType::Spatial:               return "spatial";
            case ProblemType::Reverb:                return "reverb";
            case ProblemType::Saturation:            return "saturation";
            case ProblemType::Limiting:              return "limiting";
            default:                                 return "unknown";
        }
    }

    /** Retorna un emoji representativo para el tipo de problema. */
    inline const char* problemTypeEmoji(ProblemType type) noexcept
    {
        switch (type) {
            case ProblemType::Gain:                  return "[COACH]"; // 🎛
            case ProblemType::Clipping:              return "\xF0\x9F\x94\xB4"; // 🔴
            case ProblemType::Masking:               return "\xF0\x9F\x94\x8A"; // 🔊
            case ProblemType::TonalExcess:           return "\xE2\xAC\x86";    // ⬆
            case ProblemType::TonalDeficit:          return "\xE2\xAC\x87";    // ⬇
            case ProblemType::DynamicsOvercompressed: return "[TREND]"; // 📈
            case ProblemType::DynamicsTooDynamic:    return "[TREND]"; // 📉
            case ProblemType::Phase:                 return "\xF0\x9F\x94\xAE"; // 🔮
            case ProblemType::Spatial:               return "\xF0\x9F\x8C\x8A"; // 🌊
            case ProblemType::Reverb:                return "\xF0\x9F\x8C\x8A"; // 🌊
            default:                                 return "[QUESTION]";    // ❓
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  PluginTier — Categoría del plugin (Nativo/Gratis/Profesional)
    // ═══════════════════════════════════════════════════════════════════════════
    enum class PluginTier : uint8_t
    {
        Native,   // DAW nativo (Fruity Balance, etc.)
        Free,     // Gratuito (TDR Nova, Valhalla Supermassive, etc.)
        Premium,  // Profesional de pago (FabFilter, Valhalla, etc.)
        UserHas,  // El usuario ya tiene este plugin (detectado en inventory)
        Unknown
    };

    /** Convierte string a PluginTier. */
    inline PluginTier pluginTierFromString(const juce::String& str) noexcept
    {
        auto lower = str.trim().toLowerCase();
        if (lower == "native")  return PluginTier::Native;
        if (lower == "free")    return PluginTier::Free;
        if (lower == "premium") return PluginTier::Premium;
        if (lower == "user_has" || lower == "userhas") return PluginTier::UserHas;
        return PluginTier::Unknown;
    }

    /** Convierte PluginTier a string. */
    inline const char* pluginTierToString(PluginTier tier) noexcept
    {
        switch (tier) {
            case PluginTier::Native:  return "native";
            case PluginTier::Free:    return "free";
            case PluginTier::Premium: return "premium";
            case PluginTier::UserHas: return "user_has";
            default:                  return "unknown";
        }
    }

    /** Retorna el label en español para la UI. */
    inline const char* pluginTierLabel(PluginTier tier) noexcept
    {
        switch (tier) {
            case PluginTier::Native:  return "[COACH] Nativo";
            case PluginTier::Free:    return "\xF0\x9F\x9F\xA2 Gratis";
            case PluginTier::Premium: return "\xE2\xAD\x90 Profesional";
            case PluginTier::UserHas: return "[BOLT] Ya tienes";
            default:                  return "[QUESTION] Desconocido";
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  PluginConfig — Configuración recomendada para un plugin y problema
    // ═══════════════════════════════════════════════════════════════════════════
    struct PluginConfig
    {
        juce::String actionText;  // Texto legible de la acción (ej: "Band 2: 58 Hz, Q=1.6, -3 dB")
        std::unordered_map<juce::String, float> params;  // Parámetros numéricos
        juce::StringArray paramLabels;                   // Labels para params (ordenado)

        [[nodiscard]] bool isValid() const noexcept { return actionText.isNotEmpty(); }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  PluginEntry — Un plugin registrado en la base de datos
    // ═══════════════════════════════════════════════════════════════════════════
    struct PluginEntry
    {
        juce::String id;              // ID único (ej: "fruity_balance")
        juce::String name;            // Nombre mostrable (ej: "Fruity Balance")
        juce::String developer;       // Desarrollador
        PluginTier tier;              // Categoría
        juce::String daw;             // DAW de origen (para nativos)
        float rating;                 // Rating 1-5
        juce::String icon;            // Emoji representativo
        juce::String description;     // Breve descripción
        std::vector<ProblemType> compatibility; // Problemas que resuelve

        // Config por problema
        std::unordered_map<ProblemType, PluginConfig> configs;
        PluginConfig defaultConfig;

        [[nodiscard]] bool isCompatible(ProblemType problem) const noexcept
        {
            for (auto& p : compatibility)
                if (p == problem) return true;
            return false;
        }

        /** Retorna la config para un problem type, o defaultConfig si no hay específica. */
        [[nodiscard]] const PluginConfig& getConfig(ProblemType problem) const noexcept
        {
            auto it = configs.find(problem);
            if (it != configs.end()) return it->second;
            return defaultConfig;
        }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  PluginSuggestion — Recomendación completa de un plugin para un problema
    // ═══════════════════════════════════════════════════════════════════════════
    struct PluginSuggestion
    {
        const PluginEntry* plugin = nullptr;
        ProblemType problem = ProblemType::Unknown;
        const PluginConfig* config = nullptr;

        // Metadatos para el prompt del LLM
        juce::String trackName;       // Pista afectada
        float detectionValue = 0.0f;  // Valor detectado (peak, crest, etc.)
        float targetValue = 0.0f;     // Valor objetivo
        float delta = 0.0f;           // Cambio sugerido
        float frequencyHz = 0.0f;     // Frecuencia (para EQ)

        /** Override de tier para display. Cuando no es Unknown,
            se usa en vez de plugin->tier para el formateo en UI.
            Útil para promocionar plugins que el usuario ya tiene (UserHas)
            sin modificar el PluginEntry original. */
        PluginTier effectiveTier = PluginTier::Unknown;

        [[nodiscard]] bool isValid() const noexcept
        {
            return plugin != nullptr && config != nullptr;
        }

        /** Retorna el tier a mostrar (override si está seteado, original si no). */
        [[nodiscard]] PluginTier displayTier() const noexcept
        {
            return (effectiveTier != PluginTier::Unknown) ? effectiveTier : plugin->tier;
        }
    };

} // namespace mixcoach
