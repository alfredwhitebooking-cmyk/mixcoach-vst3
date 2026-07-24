# P9: Integrate plugin suggestions into EndOfSessionComponent
# Applies changes to both .h and .cpp files

import sys
sys.stdout.reconfigure(encoding='utf-8')

changes = 0

# ═══════════════════════════════════════════════════════════════════════════
#  PART 1: EndOfSessionComponent.h — Add PluginRecEntry + members
# ═══════════════════════════════════════════════════════════════════════════

with open('Source/MixCoach/UI/EndOfSessionComponent.h', 'r', encoding='utf-8') as f:
    h_data = f.read()

# 1a. Add TrackProblemCard.h include
old_include = '#include "../engine/FeedbackCollector.h"\n#include "../ai/AiCoachAdapter.h"'
new_include = '#include "../engine/FeedbackCollector.h"\n#include "../UI/TrackProblemCard.h"\n#include "../ai/AiCoachAdapter.h"'

if old_include in h_data:
    h_data = h_data.replace(old_include, new_include, 1)
    print('H1: TrackProblemCard.h include added')
    changes += 1
else:
    print('H1: FAILED - include not found')

# 1b. Add PluginRecEntry struct + pluginRecs_ member after trackDiagnosis_
old_diag = '''        std::vector<TrackDiagnosisEntry> trackDiagnosis_;

        // \u2500\u2500\u2500 Scroll state \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
        int scrollOffset_ = 0;'''

new_diag = '''        std::vector<TrackDiagnosisEntry> trackDiagnosis_;

        // \u2500\u2500\u2500 Plugin recommendations (generated from track diagnosis) \u2500\u2500\u2500
        struct PluginRecEntry
        {
            juce::String trackName;
            juce::String roleName;
            int slotIndex = -1;
            float severity = 0.0f;
            juce::String domain;       // "gain", "tonal", "dynamics", "phase"
            juce::String issueType;
            float delta = 0.0f;
            float frequencyHz = 0.0f;

            std::vector<TrackPluginSuggestion> suggestions;
        };
        std::vector<PluginRecEntry> pluginRecs_;

        // \u2500\u2500\u2500 Scroll state \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
        int scrollOffset_ = 0;'''

if old_diag in h_data:
    h_data = h_data.replace(old_diag, new_diag, 1)
    print('H2: PluginRecEntry + pluginRecs_ added')
    changes += 1
else:
    print('H2: FAILED - trackDiagnosis_ not found')

# 1c. Add pluginRecsBounds_ + drawPluginRecommendations
old_bounds = '''        juce::Rectangle<int> changesHeaderBounds_;
        juce::Rectangle<int> changesListBounds_;
        juce::Rectangle<int> exportButtonBounds_;'''

new_bounds = '''        juce::Rectangle<int> changesHeaderBounds_;
        juce::Rectangle<int> changesListBounds_;
        juce::Rectangle<int> pluginRecsBounds_;
        juce::Rectangle<int> exportButtonBounds_;'''

if old_bounds in h_data:
    h_data = h_data.replace(old_bounds, new_bounds, 1)
    print('H3: pluginRecsBounds_ added')
    changes += 1
else:
    print('H3: FAILED - bounds not found')

# 1d. Add drawPluginRecommendations declaration
old_draw = '''        void drawChangesSection(juce::Graphics& g, juce::Rectangle<int> headerBounds,
                                juce::Rectangle<int> listBounds);
        void drawExportButton(juce::Graphics& g, juce::Rectangle<int> bounds);'''

new_draw = '''        void drawChangesSection(juce::Graphics& g, juce::Rectangle<int> headerBounds,
                                juce::Rectangle<int> listBounds);
        void drawPluginRecommendations(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawExportButton(juce::Graphics& g, juce::Rectangle<int> bounds);'''

if old_draw in h_data:
    h_data = h_data.replace(old_draw, new_draw, 1)
    print('H4: drawPluginRecommendations declaration added')
    changes += 1
else:
    print('H4: FAILED - drawChangesSection not found')

with open('Source/MixCoach/UI/EndOfSessionComponent.h', 'w', encoding='utf-8') as f:
    f.write(h_data)

# ═══════════════════════════════════════════════════════════════════════════
#  PART 2: EndOfSessionComponent.cpp — Implementation
# ═══════════════════════════════════════════════════════════════════════════

with open('Source/MixCoach/UI/EndOfSessionComponent.cpp', 'r', encoding='utf-8') as f:
    cpp_data = f.read()

# 2a. Add include for PluginSuggestionsProvider (needed for direct access)
# TrackProblemCard.h is already included from the header, so no need

# 2b. In refresh(): add plugin suggestions collection after track diagnosis
# Find the section after track diagnosis sorting and before "6. Load multi-session history"
old_after_diag = '''            // Sort by severity descending (worst first)
            std::sort(trackDiagnosis_.begin(), trackDiagnosis_.end(),
                      [](const TrackDiagnosisEntry& a, const TrackDiagnosisEntry& b) {
                          return a.consolidatedSeverity > b.consolidatedSeverity;
                      });
        }

        // \u2500\u2500\u2500 6. Load multi-session history'''

new_after_diag = '''            // Sort by severity descending (worst first)
            std::sort(trackDiagnosis_.begin(), trackDiagnosis_.end(),
                      [](const TrackDiagnosisEntry& a, const TrackDiagnosisEntry& b) {
                          return a.consolidatedSeverity > b.consolidatedSeverity;
                      });
        }

        // \u2500\u2500\u2500 5b. Collect plugin recommendations per track
        pluginRecs_.clear();
        if (coachEngine_ != nullptr) {
            auto& pluginProvider = coachEngine_->getPluginSuggestionsProvider();
            for (const auto& diag : trackDiagnosis_) {
                if (diag.consolidatedSeverity < 0.1f) continue;

                // Determine domain + issueType from the worst domain
                juce::String domain, issueType;
                float delta = 0.0f, freqHz = 0.0f;

                if (diag.gainStatus >= 1) {
                    domain = "gain";
                    issueType = (diag.gainStatus >= 2) ? "OFF_TARGET" : "NEAR_TARGET";
                } else if (diag.dynamicsStatus >= 1) {
                    domain = "dynamics";
                    issueType = (diag.dynamicsStatus >= 2) ? "HIGH_CREST" : "LOW_CREST";
                } else if (diag.tonalStatus >= 1) {
                    domain = "tonal";
                    issueType = (diag.tonalStatus >= 2) ? "EXCESS" : "NEAR_LIMIT";
                } else if (diag.phaseStatus >= 1) {
                    domain = "phase";
                    issueType = (diag.phaseStatus >= 2) ? "PHASE_ISSUE" : "NEAR_PHASE";
                } else {
                    continue; // No actionable domain
                }

                // Build TrackProblemData and populate suggestions
                TrackProblemData trackData;
                trackData.trackName = diag.trackName;
                trackData.roleName  = diag.roleName;
                trackData.slotIndex = diag.slotIndex;
                trackData.severity  = diag.consolidatedSeverity;
                trackData.domain    = domain;
                trackData.issueType = issueType;
                trackData.delta     = delta;
                trackData.frequencyHz = freqHz;

                populatePluginSuggestions(pluginProvider, trackData, domain, issueType, delta, freqHz);

                if (!trackData.pluginSuggestions.empty()) {
                    PluginRecEntry rec;
                    rec.trackName  = diag.trackName;
                    rec.roleName   = diag.roleName;
                    rec.slotIndex  = diag.slotIndex;
                    rec.severity   = diag.consolidatedSeverity;
                    rec.domain     = domain;
                    rec.issueType  = issueType;
                    rec.delta      = delta;
                    rec.frequencyHz = freqHz;
                    rec.suggestions = std::move(trackData.pluginSuggestions);
                    pluginRecs_.push_back(std::move(rec));
                }
            }
        }

        // \u2500\u2500\u2500 6. Load multi-session history'''

if old_after_diag in cpp_data:
    cpp_data = cpp_data.replace(old_after_diag, new_after_diag, 1)
    print('CPP1: Plugin recommendations collection added in refresh()')
    changes += 1
else:
    print('CPP1: FAILED - diagnosis sorting section not found')

# 2c. In resized(): add pluginRecsBounds_ after changesListBounds_
old_resized = '''        // \u2500\u2500\u2500 Changes section \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
        changesHeaderBounds_ = area.removeFromTop(28);
        area.removeFromTop(MixCoachTheme::spacingSM);
        changesListBounds_ = area.removeFromTop(juce::jmin(200, juce::jmax(0, area.getHeight() - 60)));
        area.removeFromTop(MixCoachTheme::spacingMD);

        // \u2500\u2500\u2500 Export button \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
        auto btnRow     = area.removeFromTop(48);
        exportButtonBounds_ = btnRow.withSizeKeepingCentre(220, 40);'''

new_resized = '''        // \u2500\u2500\u2500 Changes section \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
        changesHeaderBounds_ = area.removeFromTop(28);
        area.removeFromTop(MixCoachTheme::spacingSM);
        changesListBounds_ = area.removeFromTop(juce::jmin(200, juce::jmax(0, area.getHeight() - 60)));
        area.removeFromTop(MixCoachTheme::spacingMD);

        // \u2500\u2500\u2500 Plugin Recommendations section (show if showTechnicalDetails_)\u2500\u2500\u2500
        {
            if (showTechnicalDetails_ && !pluginRecs_.empty()) {
                int nRecs = juce::jmin((int)pluginRecs_.size(), 6);
                int recH = 30 + nRecs * 34; // header + each track with suggestions
                pluginRecsBounds_ = area.removeFromTop(recH);
                area.removeFromTop(MixCoachTheme::spacingMD);
            } else {
                pluginRecsBounds_ = {};
            }
        }

        // \u2500\u2500\u2500 Export button \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
        auto btnRow     = area.removeFromTop(48);
        exportButtonBounds_ = btnRow.withSizeKeepingCentre(220, 40);'''

if old_resized in cpp_data:
    cpp_data = cpp_data.replace(old_resized, new_resized, 1)
    print('CPP2: pluginRecsBounds_ layout added in resized()')
    changes += 1
else:
    print('CPP2: FAILED - resized changes section not found')

# 2d. In paint(): add drawPluginRecommendations call after drawChangesSection
old_paint = '''        // \u2500\u2500\u2500 Changes Section \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
        drawChangesSection(g, changesHeaderBounds_, changesListBounds_);

        // \u2500\u2500\u2500 Export Button \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
        drawExportButton(g, exportButtonBounds_);'''

new_paint = '''        // \u2500\u2500\u2500 Changes Section \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
        drawChangesSection(g, changesHeaderBounds_, changesListBounds_);

        // \u2500\u2500\u2500 Plugin Recommendations \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
        if (showTechnicalDetails_ && !pluginRecs_.empty() && pluginRecsBounds_.getHeight() > 0)
            drawPluginRecommendations(g, pluginRecsBounds_);

        // \u2500\u2500\u2500 Export Button \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
        drawExportButton(g, exportButtonBounds_);'''

if old_paint in cpp_data:
    cpp_data = cpp_data.replace(old_paint, new_paint, 1)
    print('CPP3: drawPluginRecommendations call added in paint()')
    changes += 1
else:
    print('CPP3: FAILED - paint changes section not found')

# 2e. Also clear pluginRecsBounds_ in non-technical view and non-ready states
# Find the non-technical clear section
old_clear = '''            // Clear all technical sections
            for (auto& b : scoreCardsBounds_) b = {};
            trackSummaryBounds_ = diagnosisBounds_ = referenceBounds_ = refinementBounds_ = {};
            coachAdaptationBounds_ = {};
            progressHistoryHeaderBounds_ = progressHistoryBounds_ = {};
            changesHeaderBounds_ = changesListBounds_ = {};
            exportButtonBounds_ = toggleBounds_ = {};'''

new_clear = '''            // Clear all technical sections
            for (auto& b : scoreCardsBounds_) b = {};
            trackSummaryBounds_ = diagnosisBounds_ = referenceBounds_ = refinementBounds_ = {};
            coachAdaptationBounds_ = {};
            progressHistoryHeaderBounds_ = progressHistoryBounds_ = {};
            changesHeaderBounds_ = changesListBounds_ = {};
            pluginRecsBounds_ = {};
            exportButtonBounds_ = toggleBounds_ = {};'''

if old_clear in cpp_data:
    cpp_data = cpp_data.replace(old_clear, new_clear, 1)
    print('CPP4: pluginRecsBounds_ cleared in non-technical view')
    changes += 1
else:
    print('CPP4: FAILED - clear section not found')

# Also add to the non-Ready state clear section
old_clear2 = '''            diagnosisBounds_ = referenceBounds_ = refinementBounds_ = {};
            coachAdaptationBounds_ = {};
            progressHistoryHeaderBounds_ = progressHistoryBounds_ = {};
            changesHeaderBounds_ = changesListBounds_ = {};
            exportButtonBounds_ = toggleBounds_ = {};'''

new_clear2 = '''            diagnosisBounds_ = referenceBounds_ = refinementBounds_ = {};
            coachAdaptationBounds_ = {};
            progressHistoryHeaderBounds_ = progressHistoryBounds_ = {};
            changesHeaderBounds_ = changesListBounds_ = {};
            pluginRecsBounds_ = {};
            exportButtonBounds_ = toggleBounds_ = {};'''

if old_clear2 in cpp_data:
    cpp_data = cpp_data.replace(old_clear2, new_clear2, 1)
    print('CPP5: pluginRecsBounds_ cleared in non-Ready state')
    changes += 1
else:
    print('CPP5: FAILED - non-Ready clear section not found')

# 2f. Add drawPluginRecommendations implementation
# Find the end of drawChangesSection and insert before drawExportButton
# Look for the last closing brace of drawChangesSection followed by drawExportButton
draw_changes_end = 'void EndOfSessionComponent::drawExportButton(juce::Graphics& g, juce::Rect'
if draw_changes_end in cpp_data:
    # Insert drawPluginRecommendations before drawExportButton
    insert_pos = cpp_data.find(draw_changes_end)
    
    plugin_recs_code = '''
    // \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
    //  drawPluginRecommendations — Muestra sugerencias de plugins por pista
    // \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
    void EndOfSessionComponent::drawPluginRecommendations(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        // \u2500\u2500\u2500 Panel background \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        auto inner = bounds.reduced(MixCoachTheme::spacingMD, MixCoachTheme::spacingSM);

        // \u2500\u2500\u2500 Header \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
        g.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText(juce::CharPointer_UTF8("\\xF0\\x9F\\x94\\xA7  PLUGIN RECOMMENDATIONS"),
                   inner.removeFromTop(20), juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.setColour(MixCoachTheme::textMuted());
        juce::String recSub = juce::String((int)pluginRecs_.size()) + " tracks with suggestions";
        g.drawText(recSub, inner.removeFromTop(14).withLeft(inner.getX()),
                   juce::Justification::centredRight);

        // Divider
        g.setColour(MixCoachTheme::divider().withAlpha(0.20f));
        g.fillRect(inner.getX(), inner.getY() - 1, inner.getWidth(), 1);

        // \u2500\u2500\u2500 Draw each track row with its plugin pills \u2500\u2500\u2500
        int maxRows = juce::jmin((int)pluginRecs_.size(), 6);
        for (int i = 0; i < maxRows; ++i) {
            const auto& rec = pluginRecs_[i];
            auto row = inner.removeFromTop(34);

            // Row divider (except last)
            if (i < maxRows - 1) {
                g.setColour(MixCoachTheme::divider().withAlpha(0.08f));
                g.fillRect(inner.getX(), row.getY() + 33, inner.getWidth(), 1);
            }

            // \u2500\u2500\u2500 Severity indicator dot \u2500\u2500\u2500
            auto sevArea = row.removeFromLeft(10);
            juce::Colour sevCol;
            if (rec.severity >= 0.7f)      sevCol = MixCoachTheme::error();
            else if (rec.severity >= 0.4f) sevCol = MixCoachTheme::warning();
            else                           sevCol = MixCoachTheme::success();
            g.setColour(sevCol.withAlpha(0.70f));
            g.fillEllipse((float)(sevArea.getCentreX() - 3), (float)(sevArea.getCentreY() - 3), 6.0f, 6.0f);

            // \u2500\u2500\u2500 Track name + role \u2500\u2500\u2500
            auto nameArea = row.removeFromLeft(100);
            g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
            g.setColour(MixCoachTheme::textSecondary());
            g.drawText(rec.trackName, nameArea.removeFromLeft(60), juce::Justification::centredLeft);
            g.setFont(juce::Font(juce::FontOptions(8.0f)));
            g.setColour(MixCoachTheme::textMuted());
            g.drawText(rec.roleName.isNotEmpty() ? rec.roleName : "\u2014",
                       nameArea, juce::Justification::centredLeft);

            // \u2500\u2500\u2500 Domain badge \u2500\u2500\u2500
            auto badgeArea = row.removeFromLeft(44);
            juce::Colour badgeCol;
            if (rec.domain == "gain")     badgeCol = juce::Colour(0xFF8B5CF6);
            else if (rec.domain == "tonal") badgeCol = juce::Colour(0xFF3B82F6);
            else if (rec.domain == "dynamics") badgeCol = juce::Colour(0xFF10B981);
            else if (rec.domain == "phase") badgeCol = juce::Colour(0xFFF59E0B);
            else                          badgeCol = MixCoachTheme::textMuted();

            g.setColour(badgeCol.withAlpha(0.15f));
            g.fillRoundedRectangle(badgeArea.toFloat(), 4.0f);
            g.setColour(badgeCol.withAlpha(0.7f));
            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.drawText(rec.domain.toUpperCase(), badgeArea, juce::Justification::centred);

            // \u2500\u2500\u2500 Plugin pills (up to 3) \u2500\u2500\u2500
            int maxPills = juce::jmin((int)rec.suggestions.size(), 3);
            int pillStartX = row.getX();
            for (int p = 0; p < maxPills; ++p) {
                const auto& sug = rec.suggestions[p];
                juce::String pillText = juce::String(TrackPluginSuggestion::tierIcon(sug.tier))
                                        + " " + sug.pluginName;

                // Measure text width
                juce::Font pillFont(juce::FontOptions(9.0f));
                int textW = pillFont.getStringWidth(pillText) + 12; // padding

                // Clamp to remaining row width
                int maxW = row.getRight() - pillStartX;
                textW = juce::jmin(textW, maxW);
                if (textW < 20) break;

                auto pillArea = juce::Rectangle<int>(pillStartX, row.getY() + 2, textW, 16);

                // Pill background
                juce::Colour tierCol;
                switch (sug.tier) {
                    case TrackPluginSuggestion::Tier::Native:  tierCol = MixCoachTheme::bgSurface(); break;
                    case TrackPluginSuggestion::Tier::Free:    tierCol = juce::Colour(0xFF10B981).withAlpha(0.20f); break;
                    case TrackPluginSuggestion::Tier::Premium: tierCol = juce::Colour(0xFFF59E0B).withAlpha(0.20f); break;
                    case TrackPluginSuggestion::Tier::UserHas: tierCol = juce::Colour(0xFF8B5CF6).withAlpha(0.20f); break;
                    default: tierCol = MixCoachTheme::bgSurface(); break;
                }
                g.setColour(tierCol);
                g.fillRoundedRectangle(pillArea.toFloat(), 6.0f);
                g.setColour(MixCoachTheme::divider().withAlpha(0.25f));
                g.drawRoundedRectangle(pillArea.toFloat(), 6.0f, 0.5f);

                // Pill text
                g.setFont(pillFont);
                g.setColour(MixCoachTheme::textSecondary());
                g.drawText(pillText, pillArea, juce::Justification::centred);

                pillStartX = pillArea.getRight() + 4;
                if (pillStartX > row.getRight() - 10) break;
            }
        }

        // Footer note if more tracks exist
        if ((int)pluginRecs_.size() > maxRows) {
            auto footerRow = inner.removeFromTop(16);
            g.setFont(juce::Font(juce::FontOptions(8.0f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.drawText("+ " + juce::String((int)pluginRecs_.size() - maxRows)
                       + " more tracks with recommendations", footerRow, juce::Justification::centred);
        }
    }

    '''
    
    cpp_data = cpp_data[:insert_pos] + plugin_recs_code + cpp_data[insert_pos:]
    print('CPP6: drawPluginRecommendations implementation added')
    changes += 1
else:
    print('CPP6: FAILED - drawExportButton declaration not found')

# Write cpp
with open('Source/MixCoach/UI/EndOfSessionComponent.cpp', 'w', encoding='utf-8') as f:
    f.write(cpp_data)

print(f'\nTotal changes applied: {changes}')
