#include "PluginSuggestionsProvider.h"
#include "../../Common/types/LogHelper.h"

namespace mixcoach {

    PluginSuggestionsProvider::PluginSuggestionsProvider()
    {
        // La inicialización perezosa se hace en initialize()
    }

    void PluginSuggestionsProvider::initialize()
    {
        auto& db = PluginDatabase::getInstance();
        if (!db.isLoaded()) {
            if (dbPath_.isNotEmpty())
                db.load(dbPath_);
            else
                db.load(); // Auto-detect path
        }
        ready_ = PluginDatabase::getInstance().isLoaded();
        if (ready_) {
            LogHelper::writeToLog("[PluginSuggestionsProvider] Initialized with "
                                  + juce::String(PluginDatabase::getInstance().getAllPlugins().size())
                                  + " plugins");
        } else {
            LogHelper::writeToLog("[PluginSuggestionsProvider] WARNING: No plugins loaded");
        }
    }

    void PluginSuggestionsProvider::addKnownPlugin(const juce::String& pluginId)
    {
        for (const auto& id : knownPluginIds_)
            if (id == pluginId) return; // Ya existe
        knownPluginIds_.push_back(pluginId);
    }

    void PluginSuggestionsProvider::addKnownPlugins(const std::vector<juce::String>& pluginIds)
    {
        for (const auto& id : pluginIds)
            addKnownPlugin(id);
    }

    bool PluginSuggestionsProvider::hasPlugin(const juce::String& pluginId) const noexcept
    {
        for (const auto& id : knownPluginIds_)
            if (id == pluginId) return true;
        return false;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  buildMessageForProblem — Construye mensaje completo de chat
    //  Prioriza plugins que el usuario ya tiene (UserHas) sobre los genéricos.
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String PluginSuggestionsProvider::buildMessageForProblem(
        ProblemType problem,
        const juce::String& trackName,
        float delta,
        float frequencyHz) const
    {
        if (!ready_) return {};

        // 1. Obtener sugerencias priorizadas (UserHas primero, luego tieres)
        auto suggestions = getSuggestionsForProblem(problem, delta, frequencyHz);
        if (suggestions.empty()) return {};

        // 2. Construir mensaje
        return buildMessageFromSuggestions(suggestions, trackName);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  buildMessageFromSuggestions
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String PluginSuggestionsProvider::buildMessageFromSuggestions(
        const std::vector<PluginSuggestion>& suggestions,
        const juce::String& trackName) const
    {
        if (suggestions.empty() || !ready_) return {};

        auto& db = PluginDatabase::getInstance();
        return db.formatSuggestions(suggestions, trackName);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  interpolateAction — Delega a la implementación canónica en PluginDatabase
    //  para evitar duplicación de lógica y mantener formato consistente.
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String PluginSuggestionsProvider::interpolateAction(
        const juce::String& actionText,
        float delta,
        float frequencyHz)
    {
        return PluginDatabase::interpolateAction(actionText, delta, frequencyHz);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  domainToProblemType — Mapea domain + issueType a ProblemType
    // ═══════════════════════════════════════════════════════════════════════════
    ProblemType PluginSuggestionsProvider::domainToProblemType(
        const juce::String& domain,
        const juce::String& issueType)
    {
        auto d = domain.trim().toLowerCase();
        auto it = issueType.trim().toUpperCase();

        if (d == "gain" || d == "level") {
            if (it.contains("CLIP")) return ProblemType::Clipping;
            return ProblemType::Gain;
        }
        if (d == "tonal" || d == "spectral" || d == "eq") {
            if (it.contains("EXCESS")) return ProblemType::TonalExcess;
            if (it.contains("DEFICIT")) return ProblemType::TonalDeficit;
            return ProblemType::TonalExcess; // Default tonal
        }
        if (d == "dynamics" || d == "dynamic") {
            if (it.contains("OVERCOMPRESS") || it.contains("LOW_CREST"))
                return ProblemType::DynamicsOvercompressed;
            if (it.contains("TOO_DYNAMIC") || it.contains("HIGH_CREST"))
                return ProblemType::DynamicsTooDynamic;
            return ProblemType::DynamicsTooDynamic; // Default dynamics
        }
        if (d == "phase" || d == "spatial" || d == "stereo") {
            if (it.contains("PHASE") || it.contains("CORRELATION"))
                return ProblemType::Phase;
            if (it.contains("WIDTH") || it.contains("STEREO"))
                return ProblemType::Spatial;
            return ProblemType::Phase;
        }
        if (d == "space" || d == "reverb" || d == "ambience" || d == "depth") {
            if (it.contains("DRY") || it.contains("SECA") || it.contains("REVERB"))
                return ProblemType::Reverb;
            if (it.contains("WIDTH") || it.contains("STEREO") || it.contains("DEPTH"))
                return ProblemType::Spatial;
            return ProblemType::Reverb;
        }
        if (d == "masking")  return ProblemType::Masking;
        if (d == "limiting" || d == "master") return ProblemType::Limiting;

        return ProblemType::Unknown;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  gainAdviceToProblemType
    // ═══════════════════════════════════════════════════════════════════════════
    ProblemType PluginSuggestionsProvider::gainAdviceToProblemType(
        float peakDeviation,
        float peakDb)
    {
        if (peakDb > -0.5f) return ProblemType::Clipping;
        if (peakDeviation > 0.0f) return ProblemType::Gain; // Demasiado alto
        return ProblemType::Gain;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  dynamicsAdviceToProblemType
    // ═══════════════════════════════════════════════════════════════════════════
    ProblemType PluginSuggestionsProvider::dynamicsAdviceToProblemType(
        bool isOvercompressed,
        bool isTooDynamic)
    {
        if (isOvercompressed) return ProblemType::DynamicsOvercompressed;
        if (isTooDynamic)     return ProblemType::DynamicsTooDynamic;
        return ProblemType::Unknown;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getSuggestionsForProblem — Raw PluginSuggestion para UI bridge
    //  Prioriza plugins que el usuario ya tiene (UserHas) sobre los genéricos.
    //
    //  Orden: [UserHas plugins que el usuario tiene instalados]
    //         [Native → Free → Premium, saltando duplicados]
    // ═══════════════════════════════════════════════════════════════════════════
    std::vector<PluginSuggestion> PluginSuggestionsProvider::getSuggestionsForProblem(
        ProblemType problem,
        float delta,
        float frequencyHz) const
    {
        if (!ready_ || problem == ProblemType::Unknown) return {};

        auto& db = PluginDatabase::getInstance();
        std::vector<PluginSuggestion> results;

        // ─── Paso 1: Buscar plugins UserHas compatibles ────────────────────
        // Si el usuario tiene plugins conocidos que resuelven este problema,
        // aparecen PRIMERO en la lista con el tier "⚡ Ya tienes".
        std::vector<PluginSuggestion> userHasSuggestions;
        for (const auto& knownId : knownPluginIds_) {
            const auto* entry = db.getById(knownId);
            if (entry == nullptr) continue;
            if (!entry->isCompatible(problem)) continue;

            PluginSuggestion sug;
            sug.plugin         = entry;
            sug.problem        = problem;
            sug.config         = &entry->getConfig(problem);
            sug.effectiveTier  = PluginTier::UserHas;
            sug.delta          = delta;
            sug.frequencyHz    = frequencyHz;
            userHasSuggestions.push_back(sug);
        }

        // Insertar UserHas al inicio
        results.insert(results.end(), userHasSuggestions.begin(), userHasSuggestions.end());

        // ─── Paso 2: Llenar con sugerencias por tier (saltando duplicados) ─
        auto tiered = db.getSuggestionsByTier(problem);
        for (auto& sug : tiered) {
            // Saltar si este plugin ya está incluido como UserHas
            bool alreadyIncluded = false;
            for (const auto& existing : results) {
                if (existing.plugin != nullptr && existing.plugin->id == sug.plugin->id) {
                    alreadyIncluded = true;
                    break;
                }
            }
            if (alreadyIncluded) continue;

            sug.delta = delta;
            sug.frequencyHz = frequencyHz;
            results.push_back(sug);
        }

        return results;
    }

} // namespace mixcoach
