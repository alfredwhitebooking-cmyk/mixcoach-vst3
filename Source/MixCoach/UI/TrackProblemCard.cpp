#include "TrackProblemCard.h"
#include "../engine/PluginSuggestionsProvider.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  populatePluginSuggestions — Bridge: PluginSuggestionsProvider → TrackPluginSuggestion
    // ═══════════════════════════════════════════════════════════════════════════

    /** Convierte PluginTier (engine) a TrackPluginSuggestion::Tier (UI). */
    static TrackPluginSuggestion::Tier pluginTierToTrackTier(PluginTier tier) noexcept
    {
        switch (tier) {
            case PluginTier::Native:  return TrackPluginSuggestion::Tier::Native;
            case PluginTier::Free:    return TrackPluginSuggestion::Tier::Free;
            case PluginTier::Premium: return TrackPluginSuggestion::Tier::Premium;
            case PluginTier::UserHas: return TrackPluginSuggestion::Tier::UserHas;
            default:                  return TrackPluginSuggestion::Tier::Free;
        }
    }

    void populatePluginSuggestions(
        const PluginSuggestionsProvider& provider,
        TrackProblemData& track,
        const juce::String& domain,
        const juce::String& issueType,
        float delta,
        float frequencyHz)
    {
        // 1. Guardar domain/issueType/delta/freq para reconstruir en clicks
        track.domain = domain;
        track.issueType = issueType;
        track.delta = delta;
        track.frequencyHz = frequencyHz;

        // 2. Domain string → ProblemType
        auto problemType = PluginSuggestionsProvider::domainToProblemType(domain, issueType);
        if (problemType == ProblemType::Unknown) return;

        // 3. Obtener sugerencias del motor
        auto engineSugs = provider.getSuggestionsForProblem(problemType, delta, frequencyHz);
        if (engineSugs.empty()) return;

        // 4. Convertir engine → UI structs
        track.pluginSuggestions.clear();
        track.pluginSuggestions.reserve(engineSugs.size());

        for (const auto& es : engineSugs) {
            if (es.plugin == nullptr) continue;

            TrackPluginSuggestion ts;
            ts.tier = pluginTierToTrackTier(es.plugin->tier);
            ts.pluginName = es.plugin->name;

            // Interpolar actionText con valores reales
            if (es.config != nullptr) {
                ts.actionText = PluginSuggestionsProvider::interpolateAction(
                    es.config->actionText,
                    (delta != 0.0f) ? delta : es.delta,
                    (frequencyHz > 0.0f) ? frequencyHz : es.frequencyHz);
            }

            // ExtraInfo: developer name como ayuda contextual
            if (es.plugin->developer.isNotEmpty() && es.plugin->tier != PluginTier::Native)
                ts.extraInfo = es.plugin->developer;

            track.pluginSuggestions.push_back(std::move(ts));
        }
    }

    TrackProblemCard::TrackProblemCard()
    {
        setSize(300, 100);
    }

    void TrackProblemCard::setGroup(const TrackProblemGroup& group)
    {
        group_ = group;
        expandedTrack_ = -1;
        hoveringSuggestion_ = false;
        int h = getPreferredHeight();
        setSize(getWidth(), h);
        // resized() llama a updateLayout()
        resized();
        repaint();
    }

    void TrackProblemCard::clear()
    {
        group_ = {};
        trackBounds_.clear();
        expandBounds_.clear();
        pluginSuggestionBounds_.clear();
        expandedTrack_ = -1;
        hoveringSuggestion_ = false;
        repaint();
    }

    int TrackProblemCard::getPreferredHeight() const
    {
        if (group_.tracks.empty()) return 0;

        int h = kHeaderHeight + (int)group_.tracks.size() * kTrackRowHeight
                + ((int)group_.tracks.size() - 1) * kCardGap + kFooterHeight
                + kActionFooterHeight;  // ═══ Acciones Directas: 3 botones abajo

        // Add space for plugin suggestions on the expanded track
        if (expandedTrack_ >= 0 && expandedTrack_ < (int)group_.tracks.size()) {
            auto& track = group_.tracks[expandedTrack_];
            if (!track.pluginSuggestions.empty())
                h += kSuggestionHeight + kCardGap;
        }

        return h;
    }

    void TrackProblemCard::resized()
    {
        // Todo el layout está en updateLayout()
        updateLayout();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateLayout — Calcula TODOS los bounds en un solo lugar
    //  Llamado desde resized() y setGroup(). Paint() solo LEE estos bounds.
    // ═══════════════════════════════════════════════════════════════════════════
    void TrackProblemCard::updateLayout()
    {
        // Limpiar bounds cacheados
        headerIconBounds_ = {};
        headerNameBounds_ = {};
        headerBadgeBounds_ = {};
        roleIconBounds_.clear();
        trackNameBounds_.clear();
        badgeAreaBounds_.clear();
        dotsAreaBounds_.clear();
        trackBounds_.clear();
        expandBounds_.clear();
        pluginSuggestionBounds_.clear();
        goToToolsBounds_ = {};
        footerTextBounds_ = {};

        if (group_.tracks.empty()) return;

        auto area = getLocalBounds().reduced(4, 2);
        trackBounds_.reserve(group_.tracks.size());
        expandBounds_.reserve(group_.tracks.size());
        roleIconBounds_.reserve(group_.tracks.size());
        trackNameBounds_.reserve(group_.tracks.size());
        badgeAreaBounds_.reserve(group_.tracks.size());
        dotsAreaBounds_.reserve(group_.tracks.size());

        // ═══ HEADER ═══════════════════════════════════════════════════════════
        auto headerArea = area.removeFromTop(kHeaderHeight);
        // Accent bar at left
        headerIconBounds_ = headerArea.removeFromLeft(20).toFloat();
        headerNameBounds_ = headerArea.removeFromLeft(120).toFloat();
        headerBadgeBounds_ = headerArea.removeFromLeft(60).toFloat();
        area.removeFromTop(kCardGap);

        // ═══ TRACK ROWS + PLUGIN SUGGESTIONS ═════════════════════════════════
        for (size_t i = 0; i < group_.tracks.size(); ++i) {
            auto row = area.removeFromTop(kTrackRowHeight);
            auto rowF = row.toFloat();
            trackBounds_.push_back(rowF);

            // Severity dot area: left 16px
            auto remainingRow = rowF;
            remainingRow.removeFromLeft(16.0f);

            // Role icon area: 22px
            roleIconBounds_.push_back(remainingRow.removeFromLeft(22.0f).translated(4, 0));

            // Track name area: 30% of remaining width, min 80px
            float nameW = juce::jmin(80.0f, remainingRow.getWidth() * 0.3f);
            trackNameBounds_.push_back(remainingRow.removeFromLeft(nameW));

            // Problem type badge area: 40% of remaining
            float badgeW = juce::jmin(remainingRow.getWidth() * 0.4f, 100.0f);
            badgeAreaBounds_.push_back(remainingRow.removeFromLeft(badgeW));

            // Impact dots area: right 60px
            dotsAreaBounds_.push_back(remainingRow.removeFromRight(60.0f));

            area.removeFromTop(kCardGap);

            // Expand button area (right side of the row)
            expandBounds_.push_back(
                juce::Rectangle<float>(rowF.getRight() - 50.0f, rowF.getY(),
                                       50.0f, (float)kTrackRowHeight));

            // Plugin suggestion pills (if expanded)
            if ((int)i == expandedTrack_ && !group_.tracks[i].pluginSuggestions.empty()) {
                auto sugArea = area.removeFromTop(kSuggestionHeight);
                for (size_t j = 0; j < group_.tracks[i].pluginSuggestions.size(); ++j) {
                    float pw = (sugArea.getWidth() - 8) / 3.0f;
                    pluginSuggestionBounds_.push_back(
                        juce::Rectangle<float>((float)sugArea.getX() + j * (pw + 4),
                                                (float)sugArea.getY() + 2,
                                                pw, (float)kSuggestionHeight - 4));
                }
                area.removeFromTop(kCardGap);
            }
        }

        // ═══ FOOTER (tip del coach) ════════════════════════════════════════
        if ((int)area.getHeight() >= kFooterHeight) {
            auto footerArea = area.removeFromTop((float)kFooterHeight);
            footerTextBounds_ = footerArea.withWidth(footerArea.getWidth() - 90.0f).toFloat();
            goToToolsBounds_ = juce::Rectangle<float>(
                (float)footerArea.getX(), (float)footerArea.getY() + 2.0f,
                80.0f, (float)kFooterHeight - 4.0f);
        }

        // ═══ ACCIONES DIRECTAS — 3 botones: Aplicado, Omitir, Explícame ═══
        if ((int)area.getHeight() >= kActionFooterHeight) {
            auto actionArea = area.removeFromTop((float)kActionFooterHeight);
            float btnW = (actionArea.getWidth() - 12.0f) / 3.0f;
            float btnH = (float)kActionFooterHeight - 4.0f;
            float btnY = (float)actionArea.getY() + 2.0f;
            actionAppliedBounds_ = juce::Rectangle<float>((float)actionArea.getX(), btnY, btnW, btnH);
            actionSkippedBounds_ = juce::Rectangle<float>((float)actionArea.getX() + btnW + 6.0f, btnY, btnW, btnH);
            actionExplainBounds_ = juce::Rectangle<float>((float)actionArea.getX() + (btnW + 6.0f) * 2.0f, btnY, btnW, btnH);
        }
    }

    void TrackProblemCard::paint(juce::Graphics& g)
    {
        if (group_.tracks.empty()) return;

        auto bounds = getLocalBounds().toFloat();
        const float cr = 6.0f;

        // ─── Shadow ──────────────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.12f));
        g.fillRoundedRectangle(bounds.expanded(1, 2), cr);

        // ─── Background ──────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::bgPanel().withAlpha(0.92f));
        g.fillRoundedRectangle(bounds, cr);

        // ─── Border ──────────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::border().withAlpha(0.3f));
        g.drawRoundedRectangle(bounds, cr, 0.8f);

        // ═══ Paint SOLO lee de cached bounds — sin removeFromLeft/removeFromTop ═══

        // ═══ HEADER ═══════════════════════════════════════════════════════════
        {
            // Accent bar (left edge)
            g.setColour(group_.colour.withAlpha(0.5f));
            g.fillRoundedRectangle(
                juce::Rectangle<float>(bounds.getX() + 4, headerIconBounds_.getY() + 2,
                                       3.0f, headerIconBounds_.getHeight() - 4), 1.5f);

            // Group icon
            g.setFont(juce::Font(juce::FontOptions(12.0f)));
            g.drawText(group_.icon, headerIconBounds_.toNearestInt(),
                       juce::Justification::centredLeft);

            // Group name
            g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
            g.setColour(MixCoachTheme::textBright());
            g.drawText("Grupo: " + group_.groupName, headerNameBounds_.toNearestInt(),
                       juce::Justification::centredLeft);

            // Track count badge
            g.setColour(group_.colour.withAlpha(0.15f));
            g.fillRoundedRectangle(headerBadgeBounds_, 3.0f);
            g.setColour(group_.colour);
            g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
            g.drawText(juce::String((int)group_.tracks.size()) + " pistas",
                       headerBadgeBounds_.toNearestInt(), juce::Justification::centred);
        }

        // ═══ TRACK ROWS + PLUGIN SUGGESTIONS ════════════════════════════════════
        size_t sugIdx = 0;
        for (size_t i = 0; i < group_.tracks.size(); ++i) {
            auto& track = group_.tracks[i];
            bool hovered = ((int)i == hoveredTrack_);
            bool expanded = ((int)i == expandedTrack_);

            if (i >= trackBounds_.size() || i >= roleIconBounds_.size()
                || i >= trackNameBounds_.size() || i >= badgeAreaBounds_.size()
                || i >= dotsAreaBounds_.size()) break;

            auto& row = trackBounds_[i];

            if (hovered) {
                g.setColour(MixCoachTheme::accent().withAlpha(0.05f));
                g.fillRoundedRectangle(row, 3.0f);
            }

            // Severity indicator (left dot)
            {
                float dotR = 3.0f;
                float dotCx = row.getX() + 8.0f;
                float dotCy = row.getCentreY();
                juce::Colour severityColour;
                if (track.severity >= 0.8f) severityColour = MixCoachTheme::error();
                else if (track.severity >= 0.4f) severityColour = MixCoachTheme::warning();
                else severityColour = MixCoachTheme::success();
                g.setColour(severityColour);
                g.fillEllipse(dotCx - dotR, dotCy - dotR, dotR * 2.0f, dotR * 2.0f);
            }

            // Role icon
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(MixCoachTheme::textDim());
            g.drawText(track.roleName.substring(0, 2),
                       roleIconBounds_[i].toNearestInt(),
                       juce::Justification::centred);

            // Track name
            g.setFont(juce::Font(juce::FontOptions(8.5f)).boldened());
            g.setColour(MixCoachTheme::textPrimary());
            g.drawText(track.trackName, trackNameBounds_[i].toNearestInt(),
                       juce::Justification::centredLeft);

            // Problem type badge
            {
                auto& badgeArea = badgeAreaBounds_[i];
                juce::Colour severityColour;
                if (track.severity >= 0.8f) severityColour = MixCoachTheme::error();
                else if (track.severity >= 0.4f) severityColour = MixCoachTheme::warning();
                else severityColour = MixCoachTheme::success();
                g.setColour(severityColour.withAlpha(0.12f));
                g.fillRoundedRectangle(badgeArea, 3.0f);
                g.setColour(severityColour);
                g.setFont(juce::Font(juce::FontOptions(7.0f)).boldened());
                g.drawText(track.problemType, badgeArea.toNearestInt().reduced(2, 0),
                           juce::Justification::centred);
            }

            // Impact dots
            {
                auto& dotsArea = dotsAreaBounds_[i];
                int numDots = juce::jmin(5, (int)(track.priorityScore * 5.0f + 0.5f));
                float dotSpacing = 9.0f;
                float dotsStartX = dotsArea.getCentreX()
                                   - ((float)numDots * dotSpacing) / 2.0f;
                for (int d = 0; d < 5; ++d) {
                    float dx = dotsStartX + (float)d * dotSpacing;
                    float filled = d < numDots ? 1.0f : 0.2f;
                    g.setColour(MixCoachTheme::accentGlow().withAlpha(filled));
                    g.fillEllipse(dx - 2.5f, dotsArea.getCentreY() - 2.5f,
                                  5.0f, 5.0f);
                }
            }

            // Expand button
            if (hovered && i < expandBounds_.size()) {
                g.setFont(juce::Font(juce::FontOptions(7.0f)).boldened());
                g.setColour(MixCoachTheme::textMuted());
                g.drawText(expanded ? "Ocultar" : "Ver m\u00E1s",
                           expandBounds_[i].toNearestInt(), juce::Justification::centred);
            }

            // ═══ PLUGIN SUGGESTIONS (expanded track) ═══════════════════════
            if (expanded && !track.pluginSuggestions.empty()) {
                for (size_t s = 0; s < track.pluginSuggestions.size()
                     && sugIdx < pluginSuggestionBounds_.size(); ++s) {
                    auto& sug = track.pluginSuggestions[s];
                    auto& pill = pluginSuggestionBounds_[sugIdx];

                    juce::Colour pillColour;
                    switch (sug.tier) {
                        case TrackPluginSuggestion::Tier::Native:
                            pillColour = MixCoachTheme::accent();
                            break;
                        case TrackPluginSuggestion::Tier::Free:
                            pillColour = MixCoachTheme::success();
                            break;
                        case TrackPluginSuggestion::Tier::Premium:
                            pillColour = MixCoachTheme::warning();
                            break;
                        case TrackPluginSuggestion::Tier::UserHas:
                            pillColour = MixCoachTheme::accentCyan();
                            break;
                    }

                    g.setColour(pillColour.withAlpha(0.12f));
                    g.fillRoundedRectangle(pill, 4.0f);
                    g.setColour(pillColour.withAlpha(0.30f));
                    g.drawRoundedRectangle(pill, 4.0f, 0.5f);

                    juce::String pillText = juce::String(
                        TrackPluginSuggestion::tierIcon(sug.tier)) + " "
                        + sug.pluginName;
                    if (sug.actionText.isNotEmpty())
                        pillText += " (" + sug.actionText + ")";

                    g.setFont(juce::Font(juce::FontOptions(6.5f)).boldened());
                    g.setColour(pillColour);
                    g.drawText(pillText, pill.toNearestInt().reduced(3, 0),
                               juce::Justification::centredLeft);

                    sugIdx++;
                }
            }
        }

        // ═══ FOOTER (tip del coach) ═══════════════════════════════════════
        if (footerTextBounds_.getWidth() > 0) {
            g.setFont(juce::Font(juce::FontOptions(7.5f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.6f));
            g.drawText("Te recomiendo trabajar primero en estas pistas.",
                       footerTextBounds_.toNearestInt(), juce::Justification::centredLeft);

            g.setColour(MixCoachTheme::accent().withAlpha(0.15f));
            g.fillRoundedRectangle(goToToolsBounds_, 3.0f);
            g.setColour(MixCoachTheme::accentGlow());
            g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
            g.drawText("Ir a Tools", goToToolsBounds_.toNearestInt().reduced(2, 0),
                       juce::Justification::centred);
        }

        // ═══ ACCIONES DIRECTAS — 3 botones ═════════════════════════════════
        {
            auto drawActionBtn = [&](const juce::Rectangle<float>& btnBounds,
                                     const char* icon,
                                     const char* label,
                                     juce::Colour colour) {
                if (btnBounds.getWidth() <= 0) return;
                // Fondo del botón
                g.setColour(colour.withAlpha(0.12f));
                g.fillRoundedRectangle(btnBounds, 4.0f);
                g.setColour(colour.withAlpha(0.25f));
                g.drawRoundedRectangle(btnBounds, 4.0f, 0.5f);
                // Texto del botón
                g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
                g.setColour(colour);
                g.drawText(juce::String(icon) + " " + juce::String(label),
                           btnBounds.toNearestInt().reduced(2, 0),
                           juce::Justification::centred);
            };

            drawActionBtn(actionAppliedBounds_, "\xE2\x9C\x85", "Aplicado", MixCoachTheme::success());
            drawActionBtn(actionSkippedBounds_, "\xE2\x8F\xAD\xEF\xB8\x8F", "Omitir", MixCoachTheme::textMuted());
            drawActionBtn(actionExplainBounds_, "\xF0\x9F\x94\x8D", "Explicate", MixCoachTheme::accentCyan());
        }
    }

    void TrackProblemCard::mouseMove(const juce::MouseEvent& e)
    {
        auto pos = e.getPosition().toFloat();
        int oldHover = hoveredTrack_;
        bool oldSugHover = hoveringSuggestion_;

        hoveringSuggestion_ = false;
        hoveredTrack_ = -1;

        // Check plugin suggestion pills first
        for (size_t i = 0; i < pluginSuggestionBounds_.size(); ++i) {
            if (pluginSuggestionBounds_[i].contains(pos)) {
                hoveringSuggestion_ = true;
                setMouseCursor(juce::MouseCursor::PointingHandCursor);
                if (oldHover != -1 || oldSugHover != true) repaint();
                return;
            }
        }

        // Check track rows
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        for (size_t i = 0; i < trackBounds_.size(); ++i) {
            if (trackBounds_[i].contains(pos)) {
                hoveredTrack_ = (int)i;
                break;
            }
        }

        if (hoveredTrack_ != oldHover || hoveringSuggestion_ != oldSugHover)
            repaint();
    }

    void TrackProblemCard::mouseExit(const juce::MouseEvent&)
    {
        if (hoveredTrack_ >= 0 || hoveringSuggestion_) {
            hoveredTrack_ = -1;
            hoveringSuggestion_ = false;
            repaint();
        }
    }

    void TrackProblemCard::mouseDown(const juce::MouseEvent& e)
    {
        auto pos = e.getPosition().toFloat();

        // ═══ Acciones Directas — 3 botones ═════════════════════════════
        if (actionAppliedBounds_.contains(pos) && onActionApplied) {
            // Encontrar el primer track del grupo para slotIndex + domain
            int slotIdx = -1;
            juce::String domain;
            if (!group_.tracks.empty()) {
                slotIdx = group_.tracks[0].slotIndex;
                domain = group_.tracks[0].domain;
            }
            if (slotIdx >= 0) onActionApplied(slotIdx, domain);
            return;
        }
        if (actionSkippedBounds_.contains(pos) && onActionSkipped) {
            int slotIdx = -1;
            if (!group_.tracks.empty()) slotIdx = group_.tracks[0].slotIndex;
            if (slotIdx >= 0) onActionSkipped(slotIdx);
            return;
        }
        if (actionExplainBounds_.contains(pos) && onActionExplain) {
            int slotIdx = -1;
            juce::String problemType;
            if (!group_.tracks.empty()) {
                slotIdx = group_.tracks[0].slotIndex;
                problemType = group_.tracks[0].problemType;
            }
            if (slotIdx >= 0) onActionExplain(slotIdx, problemType);
            return;
        }

        // Check "Go to Tools" button
        if (goToToolsBounds_.contains(pos)) {
            if (onGoToTools) onGoToTools();
            return;
        }

        // Check plugin suggestion pills
        for (size_t i = 0; i < pluginSuggestionBounds_.size(); ++i) {
            if (pluginSuggestionBounds_[i].contains(pos)) {
                if (onPluginClicked) {
                    int trackIdx = expandedTrack_;
                    int sugIdx = (int)i;
                    const TrackProblemData* trackPtr = nullptr;
                    if (trackIdx >= 0 && trackIdx < (int)group_.tracks.size())
                        trackPtr = &group_.tracks[trackIdx];
                    if (trackPtr) {
                        onPluginClicked(trackIdx, sugIdx, *trackPtr);
                    } else {
                        onPluginClicked(trackIdx, sugIdx, TrackProblemData{});
                    }
                }
                return;
            }
        }

        // Check track rows — expand/collapse
        for (size_t i = 0; i < trackBounds_.size() && i < group_.tracks.size(); ++i) {
            if (trackBounds_[i].contains(pos)) {
                if (expandBounds_.size() > i && expandBounds_[i].contains(pos)) {
                    // Toggle expansion
                    expandedTrack_ = (expandedTrack_ == (int)i) ? -1 : (int)i;
                    hoveringSuggestion_ = false;
                    int h = getPreferredHeight();
                    setSize(getWidth(), h);
                    // setSize() already triggers resized()
                    repaint();
                    return;
                }
                // Click on track body
                if (onRequestHelp) onRequestHelp(group_.tracks[i].slotIndex);
                return;
            }
        }
    }

} // namespace mixcoach
