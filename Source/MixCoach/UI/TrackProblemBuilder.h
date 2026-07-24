#pragma once
#include <vector>
#include <juce_core/juce_core.h>
#include "TrackProblemCard.h"
#include "../engine/TrackRole.h"

namespace mixcoach {

    class CoachEngine;
    class PluginSuggestionsProvider;

    // ═══════════════════════════════════════════════════════════════════════════
    //  CategoryGroupInfo — Metadatos visuales para una categoría de grupo
    // ═══════════════════════════════════════════════════════════════════════════
    struct CategoryGroupInfo
    {
        const char* name;   // "Batería"
        const char* icon;   // "🥁"
        juce::Colour colour;
    };

    /** Retorna la info visual para una RoleCategory. */
    CategoryGroupInfo getCategoryGroupInfo(RoleCategory category) noexcept;

    // ═══════════════════════════════════════════════════════════════════════════
    //  buildTrackProblemGroups — Construye grupos de problemas desde el motor
    //
    //  Llama a CoachEngine::collectAllIssues(), agrupa por categoría de rol,
    //  y para cada track invoca populatePluginSuggestions() para obtener
    //  sugerencias de plugins contextuales.
    //
    //  @param coach  Referencia al CoachEngine (provee issues, roles, plugins)
    //  @return Vector de TrackProblemGroup listos para showTrackProblemCard()
    //          o addTrackGroupCard()
    // ═══════════════════════════════════════════════════════════════════════════
    std::vector<TrackProblemGroup> buildTrackProblemGroups(CoachEngine& coach);

} // namespace mixcoach
