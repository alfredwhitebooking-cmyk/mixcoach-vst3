#include "TrackProblemBuilder.h"
#include "../engine/CoachEngine.h"
#include "../engine/PluginSuggestionsProvider.h"
#include "../../Common/memory/SlotRegistry.h"
#include "MixCoachTheme.h"
#include <map>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Category info
    // ═══════════════════════════════════════════════════════════════════════════
    CategoryGroupInfo getCategoryGroupInfo(RoleCategory category) noexcept
    {
        switch (category) {
            case RoleCategory::Drums:
                return {"Bateria",     "[DRUM]", MixCoachTheme::roleDrums()};
            case RoleCategory::Bass:
                return {"Bajos",       "[MUSIC]", MixCoachTheme::roleBass()};
            case RoleCategory::Guitars:
                return {"Guitarras",   "[MUSIC]", MixCoachTheme::roleGuitars()};
            case RoleCategory::Keys:
                return {"Teclados",    "[MUSIC]", MixCoachTheme::roleKeys()};
            case RoleCategory::Vocals:
                return {"Voces",       "[MIC]", MixCoachTheme::roleVocals()};
            case RoleCategory::FX:
                return {"FX",          "\xE2\x9C\xA8",     MixCoachTheme::roleFX()};
            case RoleCategory::Melody:
                return {"Melodicos",   "\xF0\x9F\x8E\xBB", MixCoachTheme::roleMelody()};
            default:
                return {"Otras pistas","\xF0\x9F\x93\x8D", MixCoachTheme::roleUnknown()};
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  issueToTrackProblemData — TrackIssue → TrackProblemData
    // ═══════════════════════════════════════════════════════════════════════════
    static TrackProblemData issueToTrackProblemData(
        const CoachEngine::TrackIssue& issue,
        TrackRole role,
        const PluginSuggestionsProvider& provider)
    {
        TrackProblemData data;
        data.slotIndex    = issue.slotIndex;
        data.trackName    = issue.trackName;
        data.roleName     = juce::String(getRoleName(role));
        data.severity     = issue.severity;
        data.priorityScore = issue.severity; // Severidad como prioridad base

        // problemType: usar description si está disponible, sino issueType
        data.problemType  = issue.description.isNotEmpty()
                                ? issue.description
                                : issue.issueType;

        // Poblar sugerencias de plugin contextuales
        populatePluginSuggestions(
            provider,
            data,
            issue.domain,
            issue.issueType,
            issue.suggestedDelta,
            issue.frequencyHz);

        return data;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  buildTrackProblemGroups — TrackIssue[] → TrackProblemGroup[]
    // ═══════════════════════════════════════════════════════════════════════════
    std::vector<TrackProblemGroup> buildTrackProblemGroups(CoachEngine& coach)
    {
        // 1. Obtener issues del motor
        auto issues = coach.collectAllIssues();
        if (issues.empty()) return {};

        auto& roles = coach.getTrackRoles();
        auto& provider = coach.getPluginSuggestionsProvider();

        // 2. Agrupar por RoleCategory usando un map ordenado
        std::map<RoleCategory, std::vector<CoachEngine::TrackIssue>> grouped;

        for (const auto& issue : issues) {
            // Saltar pistas óptimas (sin problemas)
            if (issue.isOptimal) continue;

            // Obtener rol desde el slotIndex
            TrackRole role = TrackRole::Unknown;
            if (issue.slotIndex >= 0 && issue.slotIndex < SlotRegistry::kMaxSlots)
                role = roles[issue.slotIndex];

            auto category = getRoleCategory(role);
            grouped[category].push_back(issue);
        }

        if (grouped.empty()) return {};

        // 3. Convertir a TrackProblemGroup
        std::vector<TrackProblemGroup> result;
        result.reserve(grouped.size());

        for (auto& [category, catIssues] : grouped) {
            TrackProblemGroup group;

            // Obtener metadatos visuales de la categoría
            auto info = getCategoryGroupInfo(category);
            group.groupName = info.name;
            group.icon      = info.icon;
            group.colour    = info.colour;

            // Convertir cada issue a TrackProblemData
            group.tracks.reserve(catIssues.size());
            for (const auto& issue : catIssues) {
                TrackRole role = TrackRole::Unknown;
                if (issue.slotIndex >= 0 && issue.slotIndex < SlotRegistry::kMaxSlots)
                    role = roles[issue.slotIndex];

                auto trackData = issueToTrackProblemData(issue, role, provider);
                group.tracks.push_back(std::move(trackData));
            }

            result.push_back(std::move(group));
        }

        return result;
    }

} // namespace mixcoach
