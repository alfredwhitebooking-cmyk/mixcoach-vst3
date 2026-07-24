#include "CoachChatComponent.h"
#include "TrackProblemCard.h"
#include "OptionCardComponent.h"
#include <cmath>
#include <algorithm>

namespace mixcoach {

    // ─── writeChatLog — Chat logging helper (defined here to avoid linking ChatMessagesComponent.cpp)
    //     Escribe directamente a MixCoach_Crash.log con timestamp.
    void writeChatLog(const juce::String& msg)
    {
        try {
            auto logFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                               .getChildFile("MixCoach_Logs")
                               .getChildFile("MixCoach_Crash.log");
            logFile.getParentDirectory().createDirectory();
            juce::FileOutputStream fos(logFile, true);
            if (fos.openedOk()) {
                fos << "[" << juce::Time::getCurrentTime().toString(true, true) << "] [CHAT] " << msg << "\n";
                fos.flush();
            }
        }
        catch (...) {
            LogHelper::writeToLog("[ChatMessagesComponent_InlineCards] Unknown error caught");
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawTrackGroupCard — Inline track problem card rendering
    //  Renders the TrackProblemCard data inside the chat bubble flow.
    //  Called from ChatMessagesComponent::paint() for isTrackGroupCard bubbles.
    //
    //  Layout:
    //    [Header with accent bar + group icon + "Grupo: Batería"]
    //    [Track rows with severity dot, name, problem badge]
    //    [Plugin suggestion pills (if available)]
    //    [Footer with tip + "Ir a Tools"]
    // ═══════════════════════════════════════════════════════════════════════════
    float drawTrackGroupCard(juce::Graphics& g,
                             juce::Rectangle<float> bounds,
                             const ChatBubble& msg,
                             bool isGrouped)
    {
        juce::ignoreUnused(isGrouped);

        const auto& group = msg.trackGroup;
        if (group.tracks.empty()) return bounds.getHeight();

        const float cr = 8.0f;

        // ─── Card container ─────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.15f));
        g.fillRoundedRectangle(bounds.expanded(1.0f, 2.0f), cr);
        g.setColour(MixCoachTheme::bgPanel().withAlpha(0.92f));
        g.fillRoundedRectangle(bounds, cr);
        g.setColour(MixCoachTheme::border().withAlpha(0.25f));
        g.drawRoundedRectangle(bounds, cr, 0.6f);

        auto area = bounds.reduced(6, 4);

        // ═══ HEADER ══════════════════════════════════════════════════════════
        auto headerArea = area.removeFromTop(20.0f);

        // Accent bar
        g.setColour(group.colour.withAlpha(0.5f));
        g.fillRoundedRectangle(
            juce::Rectangle<float>(area.getX(), headerArea.getY() + 2,
                                    3.0f, headerArea.getHeight() - 4), 1.5f);

        // Icon
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText(group.icon, headerArea.removeFromLeft(18),
                   juce::Justification::centredLeft);

        // Group name
        g.setFont(juce::Font(juce::FontOptions(9.5f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText("Grupo: " + group.groupName, headerArea.removeFromLeft(110),
                   juce::Justification::centredLeft);

        // Track count badge
        auto badgeArea = headerArea.removeFromLeft(56);
        g.setColour(group.colour.withAlpha(0.12f));
        g.fillRoundedRectangle(badgeArea.toFloat(), 3.0f);
        g.setColour(group.colour);
        g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
        g.drawText(juce::String((int)group.tracks.size()) + " pistas",
                   badgeArea, juce::Justification::centred);

        area.removeFromTop(2);

        // ═══ TRACK ROWS ══════════════════════════════════════════════════════
        const float trackRowH = 20.0f;
        const float rowGap = 1.0f;

        for (size_t i = 0; i < group.tracks.size(); ++i) {
            auto& track = group.tracks[i];
            auto row = area.removeFromTop(trackRowH);

            // Severity dot
            float dotR = 2.5f;
            float dotCx = row.getX() + 7.0f;
            float dotCy = row.getCentreY();
            juce::Colour severityColour;
            if (track.severity >= 0.8f) severityColour = MixCoachTheme::error();
            else if (track.severity >= 0.4f) severityColour = MixCoachTheme::warning();
            else severityColour = MixCoachTheme::success();
            g.setColour(severityColour);
            g.fillEllipse(dotCx - dotR, dotCy - dotR, dotR * 2.0f, dotR * 2.0f);

            // Role/Name
            auto textArea = row.removeFromLeft(
                juce::jmin(90.0f, row.getWidth() * 0.35f));
            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.setColour(MixCoachTheme::textPrimary());
            juce::String displayName = track.roleName.isNotEmpty()
                                           ? track.roleName
                                           : track.trackName;
            g.drawText(displayName, textArea.translated(10, 0),
                       juce::Justification::centredLeft);

            // Problem badge
            auto badgeR = row.removeFromLeft(
                juce::jmin(row.getWidth() * 0.45f, 90.0f));
            g.setColour(severityColour.withAlpha(0.10f));
            g.fillRoundedRectangle(badgeR, 3.0f);
            g.setColour(severityColour);
            g.setFont(juce::Font(juce::FontOptions(6.5f)).boldened());
            g.drawText(track.problemType, badgeR.reduced(2, 0),
                       juce::Justification::centred);

            // Plugin suggestion pills (si el track las tiene)
            if (!track.pluginSuggestions.empty()) {
                area.removeFromTop(1);
                auto sugArea = area.removeFromTop(16);
                int numSugs = juce::jmin(3, (int)track.pluginSuggestions.size());
                float pillW = (sugArea.getWidth() - (numSugs - 1) * 4) / numSugs;

                for (int s = 0; s < numSugs; ++s) {
                    auto& sug = track.pluginSuggestions[s];
                    auto pill = sugArea.withWidth(pillW).translated(
                        s * (pillW + 4), 0).reduced(1, 0);

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

                    g.setColour(pillColour.withAlpha(0.10f));
                    g.fillRoundedRectangle(pill, 3.0f);
                    g.setColour(pillColour.withAlpha(0.25f));
                    g.drawRoundedRectangle(pill, 3.0f, 0.3f);

                    juce::String pillText = juce::String(
                        TrackPluginSuggestion::tierIcon(sug.tier)) + " "
                        + sug.pluginName;
                    if (sug.actionText.isNotEmpty())
                        pillText += " (" + sug.actionText + ")";

                    g.setFont(juce::Font(juce::FontOptions(6.0f)).boldened());
                    g.setColour(pillColour);
                    g.drawText(pillText, pill.reduced(2, 0),
                               juce::Justification::centredLeft);
                }
            }

            area.removeFromTop(rowGap);
        }

        // ═══ FOOTER ══════════════════════════════════════════════════════════
        if (area.getHeight() >= 16) {
            auto footerArea = area.removeFromTop(16);
            g.setFont(juce::Font(juce::FontOptions(7.0f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.drawText("Revisa estas pistas para mejorar tu mezcla.",
                       footerArea.withWidth(footerArea.getWidth() - 70),
                       juce::Justification::centredLeft);
        }

        return bounds.getHeight();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawReverbCard — Inline reverb suggestion card
    //  Renders the DecayGraphComponent-style curve + reverb parameters
    //  inside a chat bubble. Shows the decay curve, pre-delay marker,
    //  and parameter badges for a visually rich reverb suggestion.
    //
    //  Layout:
    //    [Header: "🌊 Espacio - Añadir profundidad"]
    //    [Track list with names + roles]
    //    [Decay graph: pre-delay marker, exponential decay curve, grid]
    //    [Parameter badges: PreDelay, Decay, HighCut, Mix]
    //    [Algorithm badge]
    // ═══════════════════════════════════════════════════════════════════════════
    float drawReverbCard(juce::Graphics& g,
                         juce::Rectangle<float> bounds,
                         const ChatBubble& msg)
    {
        const auto& data = msg.reverbData;
        if (!data.isValid()) return bounds.getHeight();

        const float cr = 8.0f;
        juce::Colour accentColour = MixCoachTheme::accent().interpolatedWith(
            MixCoachTheme::accentCyan(), 0.4f);

        // ─── Card container ─────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.15f));
        g.fillRoundedRectangle(bounds.expanded(1.0f, 2.0f), cr);
        g.setColour(MixCoachTheme::bgPanel().withAlpha(0.92f));
        g.fillRoundedRectangle(bounds, cr);
        g.setColour(MixCoachTheme::border().withAlpha(0.25f));
        g.drawRoundedRectangle(bounds, cr, 0.6f);

        auto area = bounds.reduced(6, 4);

        // ═══ HEADER ══════════════════════════════════════════════════════════
        auto headerArea = area.removeFromTop(20.0f);

        // Accent bar
        g.setColour(accentColour.withAlpha(0.5f));
        g.fillRoundedRectangle(
            juce::Rectangle<float>(area.getX(), headerArea.getY() + 2,
                                    3.0f, headerArea.getHeight() - 4), 1.5f);

        // Icon + title
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText("\xF0\x9F\x8C\x8A", headerArea.removeFromLeft(18),
                   juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(9.5f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText("Espacio \xE2\x80\x94 Anadir profundidad",
                   headerArea.removeFromLeft(170), juce::Justification::centredLeft);

        // Track count badge
        auto badgeArea = headerArea.removeFromLeft(56);
        g.setColour(accentColour.withAlpha(0.12f));
        g.fillRoundedRectangle(badgeArea.toFloat(), 3.0f);
        g.setColour(accentColour);
        g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
        g.drawText(juce::String((int)data.trackNames.size()) + " pistas",
                   badgeArea, juce::Justification::centred);

        // ═══ TRACK LIST ═══════════════════════════════════════════════════════
        area.removeFromTop(2);
        auto trackListArea = area.removeFromTop(
            juce::jmin(18.0f * (float)data.trackNames.size(), 54.0f));

        for (size_t i = 0; i < data.trackNames.size() && i < 3; ++i) {
            auto row = trackListArea.removeFromTop(18.0f);

            // Dot
            float dotR = 2.5f;
            g.setColour(accentColour.withAlpha(0.60f));
            g.fillEllipse(row.getX() + 5 - dotR, row.getCentreY() - dotR,
                          dotR * 2.0f, dotR * 2.0f);

            // Track name
            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.setColour(MixCoachTheme::textPrimary());
            g.drawText(data.trackNames[i],
                       juce::Rectangle<float>(row.getX() + 12, row.getY(),
                                               100.0f, 18.0f),
                       juce::Justification::centredLeft);

            // Role label
            if (i < data.trackRoles.size() && data.trackRoles[i].isNotEmpty()) {
                auto roleR = juce::Rectangle<float>(row.getRight() - 70, row.getY() + 2,
                                                     68, 14);
                g.setColour(juce::Colour(0xFF666666).withAlpha(0.15f));
                g.fillRoundedRectangle(roleR, 3.0f);
                g.setFont(juce::Font(juce::FontOptions(6.5f)));
                g.setColour(MixCoachTheme::textMuted());
                g.drawText(data.trackRoles[i], roleR, juce::Justification::centred);
            }
        }

        // ═══ DECAY GRAPH ════════════════════════════════════════════════════
        area.removeFromTop(2);
        auto graphArea = area.removeFromTop(50.0f).reduced(0, 1);

        if (graphArea.getWidth() > 40 && graphArea.getHeight() > 15) {
            const float gx = graphArea.getX();
            const float gy = graphArea.getY();
            const float gw = graphArea.getWidth();
            const float gh = graphArea.getHeight();

            // ─── Graph background ──────────────────────────────────────────
            g.setColour(juce::Colours::black.withAlpha(0.08f));
            g.fillRoundedRectangle(graphArea, 3.0f);

            // ─── Grid lines ────────────────────────────────────────────────
            g.setColour(juce::Colours::white.withAlpha(0.03f));
            for (int i = 1; i <= 3; ++i) {
                float y = gy + gh * (1.0f - i / 4.0f);
                g.drawHorizontalLine((int)y, gx, gx + gw);
            }

            // ─── Time axis ─────────────────────────────────────────────────
            float totalTime = data.decaySec * 1.2f + data.preDelayMs / 1000.0f;
            if (totalTime < 0.1f) totalTime = 2.0f;
            float timeStep = 0.5f;
            int numSteps = (int)(totalTime / timeStep) + 1;
            g.setFont(juce::Font(juce::FontOptions(5.0f)));
            g.setColour(juce::Colours::white.withAlpha(0.15f));
            for (int s = 0; s < numSteps; ++s) {
                float t = s * timeStep;
                float x = gx + gw * (t / totalTime);
                if (x <= gx + gw) {
                    g.drawVerticalLine((int)x, gy, gy + gh);
                    if (s % 2 == 0) {
                        juce::String timeLabel = juce::String(t, 1) + "s";
                        g.drawText(timeLabel, x - 8, gy + gh + 1, 16, 6,
                                   juce::Justification::centred);
                    }
                }
            }

            // ─── Baseline ──────────────────────────────────────────────────
            float baseY = gy + gh - 1;
            g.setColour(juce::Colours::white.withAlpha(0.10f));
            g.drawHorizontalLine((int)baseY, gx, gx + gw);

            // ─── Pre-delay marker ──────────────────────────────────────────
            float preDelayTime = data.preDelayMs / 1000.0f;
            float preDelayX = gx + gw * (preDelayTime / totalTime);
            if (preDelayX > gx && preDelayX < gx + gw) {
                g.setColour(accentColour.withAlpha(0.25f));
                for (float dy = gy; dy < gy + gh - 2; dy += 4.0f) {
                    float dashEnd = juce::jmin(dy + 2.0f, gy + gh - 2);
                    g.drawLine(preDelayX, dy, preDelayX, dashEnd, 0.5f);
                }
                g.setFont(juce::Font(juce::FontOptions(5.0f)));
                g.setColour(accentColour.withAlpha(0.45f));
                g.drawText(juce::String((int)data.preDelayMs) + "ms",
                           preDelayX - 10, gy + 1, 20, 7,
                           juce::Justification::centred);
            }

            // ─── Decay curve ───────────────────────────────────────────────
            if (gw > 20 && gh > 10) {
                juce::Path decayPath;
                bool first = true;

                for (int px = 0; px <= (int)gw; px += 2) {
                    float t_sec = (px / gw) * totalTime;
                    float t_after_pre = t_sec - preDelayTime;

                    float amplitude;
                    if (t_after_pre < 0.0f) {
                        amplitude = 0.0f;
                    } else {
                        amplitude = (data.mixPct / 100.0f)
                                    * std::exp(-t_after_pre / (data.decaySec * 0.66f));
                    }

                    float y = gy + gh - amplitude * gh;
                    y = juce::jlimit(gy, gy + gh, y);
                    float x = gx + px;

                    if (first) {
                        decayPath.startNewSubPath(x, baseY);
                        decayPath.lineTo(x, baseY);
                        first = false;
                    }

                    if (t_after_pre >= 0.0f) {
                        if (px == 0) {
                            decayPath.startNewSubPath(x, y);
                        }
                        decayPath.lineTo(x, y);
                    }
                }

                if (!first) {
                    g.setColour(accentColour.withAlpha(0.55f));
                    g.strokePath(decayPath, juce::PathStrokeType(1.3f));

                    juce::Path fillPath = decayPath;
                    fillPath.lineTo(gx + gw, baseY);
                    fillPath.closeSubPath();
                    g.setColour(accentColour.withAlpha(0.06f));
                    g.fillPath(fillPath);
                }
            }

            // ─── High-cut annotation ───────────────────────────────────────
            if (data.highCutHz > 0.0f) {
                juce::String hcLabel;
                if (data.highCutHz >= 1000.0f)
                    hcLabel = "HC: " + juce::String(data.highCutHz / 1000.0f, 1) + "kHz";
                else
                    hcLabel = "HC: " + juce::String((int)data.highCutHz) + "Hz";

                g.setFont(juce::Font(juce::FontOptions(5.0f)));
                g.setColour(juce::Colours::white.withAlpha(0.25f));
                g.drawText(hcLabel,
                           juce::Rectangle<float>(gx, gy + gh * 0.3f, gw - 4, 7),
                           juce::Justification::topRight);
            }
        }

        // ═══ PARAMETER BADGES ═══════════════════════════════════════════════
        area.removeFromTop(2);
        auto paramArea = area.removeFromTop(18.0f);
        float badgeW = (paramArea.getWidth() - 12.0f) / 4.0f;

        // PreDelay badge
        {
            auto b = paramArea.removeFromLeft(badgeW).reduced(1, 0);
            g.setColour(accentColour.withAlpha(0.08f));
            g.fillRoundedRectangle(b, 3.0f);
            g.setFont(juce::Font(juce::FontOptions(6.0f)).boldened());
            g.setColour(accentColour.withAlpha(0.70f));
            g.drawText(juce::String((int)data.preDelayMs) + " ms", b,
                       juce::Justification::centred);
            g.setFont(juce::Font(juce::FontOptions(5.5f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.drawText("PreD", b.removeFromTop(8), juce::Justification::centred);
        }

        paramArea.removeFromLeft(4);

        // Decay badge
        {
            auto b = paramArea.removeFromLeft(badgeW).reduced(1, 0);
            g.setColour(accentColour.withAlpha(0.08f));
            g.fillRoundedRectangle(b, 3.0f);
            g.setFont(juce::Font(juce::FontOptions(6.0f)).boldened());
            g.setColour(accentColour.withAlpha(0.70f));
            g.drawText(juce::String(data.decaySec, 1) + " s", b,
                       juce::Justification::centred);
            g.setFont(juce::Font(juce::FontOptions(5.5f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.drawText("Decay", b.removeFromTop(8), juce::Justification::centred);
        }

        paramArea.removeFromLeft(4);

        // HighCut badge
        {
            auto b = paramArea.removeFromLeft(badgeW).reduced(1, 0);
            g.setColour(accentColour.withAlpha(0.08f));
            g.fillRoundedRectangle(b, 3.0f);
            g.setFont(juce::Font(juce::FontOptions(6.0f)).boldened());
            g.setColour(accentColour.withAlpha(0.70f));
            juce::String hcStr;
            if (data.highCutHz >= 1000.0f)
                hcStr = juce::String(data.highCutHz / 1000.0f, 1) + "k";
            else
                hcStr = juce::String((int)data.highCutHz);
            g.drawText(hcStr, b, juce::Justification::centred);
            g.setFont(juce::Font(juce::FontOptions(5.5f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.drawText("H Cut", b.removeFromTop(8), juce::Justification::centred);
        }

        paramArea.removeFromLeft(4);

        // Mix badge
        {
            auto b = paramArea.removeFromLeft(badgeW).reduced(1, 0);
            g.setColour(accentColour.withAlpha(0.08f));
            g.fillRoundedRectangle(b, 3.0f);
            g.setFont(juce::Font(juce::FontOptions(6.0f)).boldened());
            g.setColour(accentColour.withAlpha(0.70f));
            g.drawText(juce::String((int)data.mixPct) + "%", b,
                       juce::Justification::centred);
            g.setFont(juce::Font(juce::FontOptions(5.5f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.drawText("Mix", b.removeFromTop(8), juce::Justification::centred);
        }

        // ═══ FOOTER ══════════════════════════════════════════════════════════
        if (area.getHeight() >= 14) {
            auto footerArea = area.removeFromTop(14);
            g.setFont(juce::Font(juce::FontOptions(6.5f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.45f));

            juce::String footerText = "Sugerencia para " + data.genre;
            if (data.algorithm.isNotEmpty())
                footerText += " \xE2\x80\xA2 " + data.algorithm;
            g.drawText(footerText, footerArea, juce::Justification::centredLeft);
        }

        return bounds.getHeight();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawPluginSuggestionCard — Inline plugin suggestion card with 3 tiers
    //  Renders tiered plugin suggestions as a rich inline card similar to
    //  the track group card. Each tier (Native/Free/Premium) is shown with
    //  its icon and action text.
    //
    //  Layout:
    //    [Header: "🔧 EQ - Enmascaramiento"]
    //    [Track name badge]
    //    [Tier cards: 🎛 Nativo / 🟢 Gratis / 💎 Premium]
    //    [Footer: "Selecciona el plugin que prefieras..."]
    //
    //  Si hitCache no es nullptr, almacena los bounds de cada tarjeta
    //  para poder hacer hit-test en mouseDown (FASE 5: option cards clicables).
    // ═══════════════════════════════════════════════════════════════════════════
    float drawPluginSuggestionCard(juce::Graphics& g,
                                    juce::Rectangle<float> bounds,
                                    const ChatBubble& msg,
                                    std::vector<PluginCardHitArea>* hitCache)
    {
        const auto& data = msg.pluginSuggestionData;
        if (!data.isValid()) return bounds.getHeight();

        const float cr = 8.0f;

        // ─── Card container ─────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.15f));
        g.fillRoundedRectangle(bounds.expanded(1.0f, 2.0f), cr);
        g.setColour(MixCoachTheme::bgPanel().withAlpha(0.92f));
        g.fillRoundedRectangle(bounds, cr);
        g.setColour(MixCoachTheme::border().withAlpha(0.25f));
        g.drawRoundedRectangle(bounds, cr, 0.6f);

        auto area = bounds.reduced(6, 4);

        // ═══ HEADER ══════════════════════════════════════════════════════════
        auto headerArea = area.removeFromTop(22.0f);

        // Accent bar (color según severidad)
        juce::Colour accentColour;
        if (data.severity >= 0.7f) accentColour = MixCoachTheme::error();
        else if (data.severity >= 0.4f) accentColour = MixCoachTheme::warning();
        else accentColour = MixCoachTheme::success();

        g.setColour(accentColour.withAlpha(0.5f));
        g.fillRoundedRectangle(
            juce::Rectangle<float>(area.getX(), headerArea.getY() + 2,
                                    3.0f, headerArea.getHeight() - 4), 1.5f);

        // Title
        g.setFont(juce::Font(juce::FontOptions(9.5f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText("[CHANGE] " + data.problemTitle,
                   headerArea.removeFromLeft(area.getWidth() - 10),
                   juce::Justification::centredLeft);

        area.removeFromTop(2);

        // ═══ TRACK NAME BADGE ════════════════════════════════════════════════
        if (data.trackName.isNotEmpty()) {
            auto trackBadge = area.removeFromTop(16);
            g.setColour(accentColour.withAlpha(0.10f));
            g.fillRoundedRectangle(
                trackBadge.withWidth(
                    juce::jmin(100.0f, trackBadge.getWidth() * 0.5f)).reduced(1, 0),
                3.0f);
            g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
            g.setColour(accentColour);
            g.drawText("\xF0\x9F\x94\x8A Pista: " + data.trackName,
                       trackBadge.reduced(2, 0),
                       juce::Justification::centredLeft);
            area.removeFromTop(2);
        }

        // ═══ SUGGESTION CARDS BY TIER ════════════════════════════════════════
        // Render each suggestion as an OptionCard via the static draw helper
        {
            int numCards = juce::jmin(3, (int)data.suggestions.size());
            if (numCards == 0) { return bounds.getHeight(); }

            float cardW = juce::jmax(70.0f, (area.getWidth() - ((float)numCards - 1) * 4.0f) / (float)numCards);
            float cardH = OptionCardComponent::kCardHeight;

            PluginCardHitArea snap;
            snap.containerBounds = bounds;

            for (int ci = 0; ci < numCards; ++ci) {
                const auto& sug = data.suggestions[ci];

                PluginTier tier;
                if (sug.isUserHas)
                    tier = PluginTier::UserHas;
                else if (sug.tier == "Nativo")
                    tier = PluginTier::Native;
                else if (sug.tier == "Gratis")
                    tier = PluginTier::Free;
                else
                    tier = PluginTier::Premium;

                OptionCardData cardData;
                cardData.tier = tier;
                cardData.pluginName = sug.pluginName;
                cardData.actionText = sug.actionText;

                float x = area.getX() + (float)ci * (cardW + 4.0f);
                auto cardBounds = juce::Rectangle<float>(x, area.getY(), cardW, cardH);
                OptionCardComponent::drawOptionCard(g, cardBounds, cardData, false, 0.0f);

                // ═══ FASE 5: Almacenar bounds para hit-test ═══════════════════
                snap.cardBounds.push_back(cardBounds);
                snap.pluginNames.push_back(sug.pluginName);
            }

            // Si se pasó hitCache, guardar snapshot
            if (hitCache) {
                hitCache->push_back(std::move(snap));
            }

            area.removeFromTop(cardH + 4);
        }

        // ═══ FOOTER ══════════════════════════════════════════════════════════
        if (area.getHeight() >= 14) {
            auto footerArea = area.removeFromTop(14);
            g.setFont(juce::Font(juce::FontOptions(6.5f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.45f));
            g.drawText("Selecciona el plugin que prefieras y te guiar\xC3\xA9 en la configuraci\xC3\xB3n.",
                       footerArea, juce::Justification::centredLeft);
        }

        return bounds.getHeight();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawMasterCheckCard — Inline master check comparison card
    //  Shows match score with rating emoji, gap bands, and recommendations.
    //
    //  Layout:
    //    [Header: "🎯 Master Check - Comparación con referencia"]
    //    [Match Score: 88/100 with progress bar]
    //    [Gaps: band name + deviation bar + recommendation]
    //    [Overall recommendation]
    // ═══════════════════════════════════════════════════════════════════════════
    float drawMasterCheckCard(juce::Graphics& g,
                               juce::Rectangle<float> bounds,
                               const ChatBubble& msg)
    {
        const auto& data = msg.masterCheckData;
        if (!data.isValid()) return bounds.getHeight();

        const float cr = 8.0f;
        juce::Colour accentColour = MixCoachTheme::accent();

        // ─── Card container ─────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.15f));
        g.fillRoundedRectangle(bounds.expanded(1.0f, 2.0f), cr);
        g.setColour(MixCoachTheme::bgPanel().withAlpha(0.92f));
        g.fillRoundedRectangle(bounds, cr);
        g.setColour(MixCoachTheme::border().withAlpha(0.25f));
        g.drawRoundedRectangle(bounds, cr, 0.6f);

        auto area = bounds.reduced(6, 4);

        // ═══ HEADER ══════════════════════════════════════════════════════════
        auto headerArea = area.removeFromTop(20.0f);

        // Accent bar
        g.setColour(accentColour.withAlpha(0.5f));
        g.fillRoundedRectangle(
            juce::Rectangle<float>(area.getX(), headerArea.getY() + 2,
                                    3.0f, headerArea.getHeight() - 4), 1.5f);

        g.setFont(juce::Font(juce::FontOptions(9.5f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText("\xE2\x98\x85 Master Check \xE2\x80\x94 Comparaci\xC3\xB3n con referencia",  // ★
                   headerArea, juce::Justification::centredLeft);

        area.removeFromTop(3);

        // ═══ MATCH SCORE ═════════════════════════════════════════════════════
        auto scoreArea = area.removeFromTop(36.0f);

        // Rating emoji
        g.setFont(juce::Font(juce::FontOptions(16.0f)));
        g.drawText(data.ratingEmoji(),
                   scoreArea.removeFromLeft(28),
                   juce::Justification::centred);

        // Score number
        int scoreInt = juce::jlimit(0, 100, (int)(data.matchScore * 100.0f));
        juce::String scoreStr = juce::String(scoreInt) + "/100";
        g.setFont(juce::Font(juce::FontOptions(14.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText(scoreStr, scoreArea.removeFromLeft(48),
                   juce::Justification::centredLeft);

        // ═══ PROGRESS BAR ════════════════════════════════════════════════════
        scoreArea.removeFromLeft(4);
        auto barArea = scoreArea.reduced(0, 8);
        float barW = barArea.getWidth();
        float barH = 6.0f;
        float barY = barArea.getCentreY() - barH * 0.5f;
        float barX = barArea.getX();

        // Bar background
        g.setColour(juce::Colours::black.withAlpha(0.10f));
        g.fillRoundedRectangle(barX, barY, barW, barH, 3.0f);

        // Bar fill
        float fillW = barW * juce::jmin(1.0f, data.matchScore);
        juce::Colour barColour;
        if (data.matchScore >= 0.80f) barColour = MixCoachTheme::success();
        else if (data.matchScore >= 0.60f) barColour = MixCoachTheme::warning();
        else barColour = MixCoachTheme::error();
        g.setColour(barColour);
        g.fillRoundedRectangle(barX, barY, fillW, barH, 3.0f);

        // Bar glow
        if (fillW > 10) {
            g.setColour(barColour.withAlpha(0.15f));
            g.fillRoundedRectangle(barX, barY - 1.0f, fillW, barH + 2.0f, 3.0f);
        }

        // Session duration label
        if (data.sessionDurationMinutes > 0) {
            g.setFont(juce::Font(juce::FontOptions(6.5f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.drawText("Sesi\xC3\xB3n: " + juce::String(data.sessionDurationMinutes) + " min",
                       juce::Rectangle<float>(barX + barW - 70, barY - 12, 70, 10),
                       juce::Justification::bottomRight);
        }

        area.removeFromTop(4);

        // ═══ GAPS SECTION ════════════════════════════════════════════════════
        if (!data.gaps.empty()) {
            auto gapsLabel = area.removeFromTop(12);
            g.setFont(juce::Font(juce::FontOptions(7.0f)).boldened());
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.6f));
            g.drawText("BRECHAS DETECTADAS", gapsLabel,
                       juce::Justification::centredLeft);

            area.removeFromTop(1);

            for (const auto& gap : data.gaps) {
                auto gapRow = area.removeFromTop(20.0f);

                // Band name
                g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
                g.setColour(MixCoachTheme::textPrimary());
                g.drawText(gap.bandName, gapRow.removeFromLeft(50),
                           juce::Justification::centredLeft);

                // Deviation (colored)
                juce::Colour devColour = gap.deviationDb > 0
                    ? MixCoachTheme::warning()
                    : MixCoachTheme::info();
                g.setFont(juce::Font(juce::FontOptions(7.0f)).boldened());
                g.setColour(devColour);
                juce::String devStr = (gap.deviationDb > 0 ? "+" : "")
                                       + juce::String(gap.deviationDb, 1) + " dB";
                g.drawText(devStr, gapRow.removeFromLeft(40),
                           juce::Justification::centredLeft);

                // Recommendation
                g.setFont(juce::Font(juce::FontOptions(6.5f)));
                g.setColour(MixCoachTheme::textMuted().withAlpha(0.7f));
                g.drawText(gap.recommendation, gapRow.reduced(2, 0),
                           juce::Justification::centredLeft);
            }
        }

        // ═══ OVERALL RECOMMENDATION ══════════════════════════════════════════
        if (data.overallRecommendation.isNotEmpty() && area.getHeight() >= 20) {
            area.removeFromTop(2);
            auto recArea = area.removeFromTop(18.0f);

            // Thin top border
            g.setColour(MixCoachTheme::border().withAlpha(0.15f));
            g.drawHorizontalLine((int)recArea.getY(),
                                 recArea.getX(), recArea.getRight());

            recArea.removeFromTop(3);
            g.setFont(juce::Font(juce::FontOptions(7.0f)).boldened());
            g.setColour(MixCoachTheme::accentGlow());
            g.drawText("\xF0\x9F\x92\xA1 " + data.overallRecommendation,
                       recArea.reduced(2, 0),
                       juce::Justification::centredLeft);
        }

        return bounds.getHeight();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawCorrectionCard — Inline correction → verify cycle card
    //  Shows a coach recommendation with [✓ Aplicado] button and verification result.
    //
    //  Layout:
    //    [Header: "🔧 EQ - Enmascaramiento en Bass"]
    //    [Action: "Corta 3dB en 60Hz (Q=2)"]
    //    [Status: ✅/⚠️/❌ with feedback message (if resolved)]
    //    [Button: "✓ Aplicado" (if pending)]
    // ═══════════════════════════════════════════════════════════════════════════
    float drawCorrectionCard(juce::Graphics& g,
                              juce::Rectangle<float> bounds,
                              const ChatBubble& msg)
    {
        const auto& data = msg.correctionData;
        if (!data.isValid()) return bounds.getHeight();

        const float cr = 8.0f;

        // ─── Card container ─────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.15f));
        g.fillRoundedRectangle(bounds.expanded(1.0f, 2.0f), cr);

        juce::Colour cardBg;
        if (data.status == CorrectionCardData::Status::Verified)
            cardBg = MixCoachTheme::success().withAlpha(0.08f);
        else if (data.status == CorrectionCardData::Status::Failed)
            cardBg = MixCoachTheme::error().withAlpha(0.08f);
        else
            cardBg = MixCoachTheme::bgPanel().withAlpha(0.92f);
        g.setColour(cardBg);
        g.fillRoundedRectangle(bounds, cr);

        g.setColour(MixCoachTheme::border().withAlpha(0.25f));
        g.drawRoundedRectangle(bounds, cr, 0.6f);

        auto area = bounds.reduced(6, 4);

        // ═══ HEADER ══════════════════════════════════════════════════════════
        auto headerArea = area.removeFromTop(22.0f);

        // Accent bar
        juce::Colour accentColour;
        if (data.status == CorrectionCardData::Status::Verified)
            accentColour = MixCoachTheme::success();
        else if (data.status == CorrectionCardData::Status::Failed || data.status == CorrectionCardData::Status::OverApplied)
            accentColour = MixCoachTheme::error();
        else if (data.status == CorrectionCardData::Status::Partial)
            accentColour = MixCoachTheme::warning();
        else
            accentColour = MixCoachTheme::accent();

        g.setColour(accentColour.withAlpha(0.5f));
        g.fillRoundedRectangle(
            juce::Rectangle<float>(area.getX(), headerArea.getY() + 2,
                                    3.0f, headerArea.getHeight() - 4), 1.5f);

        // Status icon
        juce::String statusIcon = CorrectionCardData::statusIcon(data.status);
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText(statusIcon, headerArea.removeFromLeft(18),
                   juce::Justification::centredLeft);

        // Title
        g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText("[CHANGE] " + data.problemTitle,
                   headerArea, juce::Justification::centredLeft);

        area.removeFromTop(2);

        // ═══ TRACK + ACTION ══════════════════════════════════════════════════
        // Track name badge
        auto trackBadge = area.removeFromTop(16.0f);
        g.setColour(accentColour.withAlpha(0.10f));
        g.fillRoundedRectangle(
            trackBadge.withWidth(
                juce::jmin(120.0f, trackBadge.getWidth() * 0.4f)).reduced(1, 0),
            3.0f);
        g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
        g.setColour(accentColour);
        g.drawText("\xF0\x9F\x94\x8A Pista: " + data.trackName,
                   trackBadge.reduced(4, 0),
                   juce::Justification::centredLeft);

        area.removeFromTop(2);

        // Action text
        auto actionArea = area.removeFromTop(18.0f);
        g.setFont(juce::Font(juce::FontOptions(8.5f)).boldened());
        g.setColour(MixCoachTheme::textPrimary());
        g.drawText(data.action, actionArea.reduced(2, 0),
                   juce::Justification::centredLeft);

        // ═══ STATUS / FEEDBACK (if resolved) ══════════════════════════════════
        if (data.isResolved() && data.feedbackMessage.isNotEmpty()) {
            area.removeFromTop(2);
            auto feedbackArea = area.removeFromTop(20.0f);

            // Feedback badge
            g.setColour(accentColour.withAlpha(0.10f));
            g.fillRoundedRectangle(feedbackArea.reduced(2, 0), 4.0f);
            g.setFont(juce::Font(juce::FontOptions(7.5f)));
            g.setColour(accentColour);
            g.drawText(statusIcon + " " + data.feedbackMessage,
                       feedbackArea.reduced(6, 0),
                       juce::Justification::centredLeft);
        }

        // ═══ [✓ Aplicado] BUTTON (if pending) ════════════════════════════════
        if (data.status < CorrectionCardData::Status::Verified) {
            area.removeFromTop(2);
            auto btnArea = area.removeFromTop(26.0f).reduced(2, 0);

            // Button background
            g.setColour(MixCoachTheme::success().withAlpha(0.15f));
            g.fillRoundedRectangle(btnArea, 5.0f);
            g.setColour(MixCoachTheme::success().withAlpha(0.30f));
            g.drawRoundedRectangle(btnArea, 5.0f, 0.5f);

            // Button text
            g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
            g.setColour(MixCoachTheme::success());
            g.drawText("\xE2\x9C\x93 Aplicado \xE2\x80\x94 Ya hice el cambio",  // ✓
                       btnArea, juce::Justification::centred);
        }

        // ═══ FOOTER ══════════════════════════════════════════════════════════
        if (area.getHeight() >= 14) {
            auto footerArea = area.removeFromTop(14);
            g.setFont(juce::Font(juce::FontOptions(6.5f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.45f));

            juce::String footerText;
            if (data.domain == "tonal")
                footerText = "Usa un EQ para ajustar la frecuencia indicada.";
            else if (data.domain == "gain")
                footerText = "Ajusta el fader o gain del canal.";
            else if (data.domain == "dynamics")
                footerText = "Usa un compresor para controlar la din\xC3\xA1mica.";
            else if (data.domain == "spatial")
                footerText = "Ajusta el paneo o la correlaci\xC3\xB3n est\xC3\xA9reo.";
            else
                footerText = "Aplica el cambio y conf\xC3\xADrmalo cuando est\xC3\xA9 listo.";
            g.drawText(footerText, footerArea, juce::Justification::centredLeft);
        }

        return bounds.getHeight();
    }

} // namespace mixcoach
