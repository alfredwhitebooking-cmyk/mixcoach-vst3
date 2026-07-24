#include "MessengerListComponent.h"
#include "MessengerListRoles.h"
#include "MixCoachTheme.h"
#include "../engine/CoachEngine.h"
#include <cmath>

namespace mixcoach {

    // ─── Drawing constants ────────────────────────────────────────────────────
    static constexpr float kMeterMinDb = -60.0f;
    static constexpr float kMeterMaxDb = 0.0f;

    // Text colours now use textBright()/textDim()/textMuted() from MixCoachTheme.h

    // ─── Pulsing animation helper ─────────────────────────────────────────────
    // Returns a value that oscillates between 0.0 and 1.0 at the given frequency.
    // phaseOffset shifts the wave (0.0 = no shift, 1.0 = full cycle shift).
    // Uses continuous time (no modulo) so there is no discontinuity at boundaries.
    static float pulseAlpha(float speedHz, float phaseOffset) noexcept
    {
        // Continuous time in seconds — no modulo, sin wraps naturally forever
        float t = juce::Time::getMillisecondCounter() * 0.001f;
        return 0.5f
               + 0.5f
                     * std::sin(t * juce::MathConstants<float>::twoPi * speedHz
                                + phaseOffset * juce::MathConstants<float>::twoPi);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Renderiza lista de tracks con cabeceras de bus
    // ═══════════════════════════════════════════════════════════════════════════
    void MessengerListComponent::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds();
        g.fillAll(juce::Colours::transparentBlack);

        auto area = bounds.reduced(MixCoachTheme::spacingXXS, MixCoachTheme::spacingXS);

        if (activeMessengerCount_ == 0) {
            emptyLabel_.setVisible(true);
            return;
        }
        emptyLabel_.setVisible(false);

        // ═══ Health summary bar (dual: detailed TrackHealth + consolidated TrackAdvice Status) ═══
        {
            // Row 1: Detailed TrackHealth counts (legacy)
            int clean = 0, warn = 0, crit = 0, silent = 0, unknown = 0;
            // Row 2: Consolidated TrackAdvice::Status counts (Sprint 7)
            int consOnTarget = 0, consNearTarget = 0, consOffTarget = 0, consNoSignal = 0;

            for (int idx = 0; idx < SlotRegistry::kMaxSlots; ++idx) {
                if (!messengers_[idx].info.active) continue;

                // Detailed health from TrackFeedCore
                if (!messengers_[idx].hasSignal) {
                    silent++;
                } else {
                    switch (messengers_[idx].trackHealth) {
                        case TrackHealth::Clean:          clean++;   break;
                        case TrackHealth::NeedsEQ:
                        case TrackHealth::NeedsCompression:
                        case TrackHealth::MaskingIssue:
                        case TrackHealth::PhaseIssue:     warn++;   break;
                        case TrackHealth::Overcompressed:
                        case TrackHealth::ClippingRisk:
                        case TrackHealth::StereoCollapse: crit++;   break;
                        case TrackHealth::LowSignal:
                        case TrackHealth::Silent:         silent++; break;
                        default:                          unknown++; break;
                    }
                }

                // Consolidated health from TrackAdvice::Status (Sprint 7)
                switch (messengers_[idx].consolidatedHealth) {
                    case SuggestionStatus::Green:  consOnTarget++;   break;
                    case SuggestionStatus::Yellow: consNearTarget++; break;
                    case SuggestionStatus::Red:    consOffTarget++;  break;
                    case SuggestionStatus::White:  consNoSignal++;   break;
                    default: break;
                }
            }

            if (clean + warn + crit + silent > 0) {
                auto healthBar = area.removeFromTop(kTitleHeight).reduced(0, MixCoachTheme::spacingXXS / 2);
                g.setFont(interFont(9.0f).boldened());

                auto drawPill = [&](int count, juce::Colour colour, const char* label, int& xOff) {
                    if (count <= 0) return;
                    juce::String text = juce::String(count) + " " + label;
                    int w     = juce::GlyphArrangement::getStringWidthInt(interFont(9.0f).boldened(), text) + 14;
                    auto pill = juce::Rectangle<int>(xOff,
                                                     healthBar.getY() + MixCoachTheme::spacingSM / 2,
                                                     w,
                                                     healthBar.getHeight() - MixCoachTheme::spacingSM);
                    g.setColour(colour.withAlpha(0.10f));
                    g.fillRoundedRectangle(pill.toFloat(), 4.0f);
                    g.setColour(colour.withAlpha(0.85f));
                    g.drawFittedText(text, pill, juce::Justification::centred, 1);
                    xOff += w + MixCoachTheme::spacingXS;
                };

                int xOff = healthBar.getX();

                // Row 1: Detailed TrackHealth pills (legacy)
                drawPill(crit, MixCoachTheme::error(), "[EXCLAMATION]", xOff);
                drawPill(warn, MixCoachTheme::warning(), "[WARN]", xOff);
                drawPill(clean, MixCoachTheme::success(), "[OK]", xOff);
                drawPill(silent, textMuted(), "\xE2\x9C\xB0", xOff);

                // Separator before consolidated pills
                if (consOnTarget + consNearTarget + consOffTarget > 0) {
                    g.setColour(textMuted().withAlpha(0.25f));
                    int sepX = xOff;
                    auto sep = juce::Rectangle<int>(sepX, healthBar.getY() + 4, 8, healthBar.getHeight() - 8);
                    g.setFont(interFont(9.0f));
                    g.drawFittedText("|", sep, juce::Justification::centred, 1);
                    xOff += 10;

                    // Row 2: Consolidated TrackAdvice status pills
                    drawPill(consOffTarget,  MixCoachTheme::error(),   "[EMPTY]", xOff);
                    drawPill(consNearTarget, MixCoachTheme::warning(), "\xE2\x97\xAF", xOff);
                    drawPill(consOnTarget,   MixCoachTheme::success(), "[ACTIVE]", xOff);
                }

                // Sprint 1: Identity progress badge 🎯 X/Y (confirmed / identified)
                if (coachEngine_ != nullptr) {
                    auto prog = coachEngine_->getIdentityProgress();
                    if (prog.totalActive > 0 && prog.identified > 0) {
                        bool allConfirmed   = (prog.pendingInferred == 0);
                        auto idCol          = allConfirmed ? MixCoachTheme::success() : MixCoachTheme::accentCyan();
                        juce::String idText = "[TARGET] " // 🎯
                                              + juce::String(prog.confirmed) + "/" + juce::String(prog.identified);
                        int idW = juce::GlyphArrangement::getStringWidthInt(interFont(9.0f).boldened(), idText) + 14;
                        auto idPill = juce::Rectangle<int>(xOff, healthBar.getY() + 3, idW, healthBar.getHeight() - 6);
                        g.setColour(idCol.withAlpha(0.10f));
                        g.fillRoundedRectangle(idPill.toFloat(), 4.0f);
                        g.setColour(idCol.withAlpha(0.85f));
                        g.drawFittedText(idText, idPill, juce::Justification::centred, 1);
                    }
                }
            }
            area.removeFromTop(2);
        }

        // ═══ SPRINT 5: TrackFeed Banner (top events) ═══════════════════════════
        {
            auto bannerArea = area.removeFromTop(kTitleHeight).reduced(0, 1);

            // Colapso/expande button
            {
                int btnX           = bannerArea.getX();
                int btnY           = bannerArea.getY() + MixCoachTheme::spacingSM / 2;
                int btnSize        = bannerArea.getHeight() - MixCoachTheme::spacingSM;
                bannerCollapseBtn_ = {btnX, btnY, btnSize, btnSize};

                g.setColour(textDim().withAlpha(0.50f));
                g.setFont(interFont(9.0f));
                g.drawFittedText(bannerCollapsed_ ? "▶" : "▼", bannerCollapseBtn_, juce::Justification::centred, 1);
            }

            // TRACKFEED label
            {
                int labelX     = bannerCollapseBtn_.getRight() + MixCoachTheme::spacingXS;
                auto labelArea = juce::Rectangle<int>(labelX,
                                                      bannerArea.getY() + MixCoachTheme::spacingSM / 2,
                                                      70,
                                                      bannerArea.getHeight() - MixCoachTheme::spacingSM);
                g.setFont(interFont(9.0f).boldened());
                g.setColour(MixCoachTheme::accentCyan().withAlpha(0.85f));
                g.drawFittedText("TRACKFEED", labelArea, juce::Justification::centredLeft, 1);
            }

            bannerEventBounds_.clear();

            if (!bannerCollapsed_ && !topEvents_.empty()) {
                int eventX  = bannerCollapseBtn_.getRight() + 76;
                int maxW    = bannerArea.getRight() - eventX;
                int maxShow = 3;

                for (int ei = 0; ei < juce::jmin(maxShow, (int)topEvents_.size()); ++ei) {
                    const auto& ev = topEvents_[ei];
                    int dotSize    = 6;
                    int dotY       = bannerArea.getY() + (bannerArea.getHeight() - dotSize) / 2;

                    juce::Colour dotCol;
                    if (ev.severity >= 0.7f) dotCol = MixCoachTheme::error();
                    else if (ev.severity >= 0.4f)
                        dotCol = MixCoachTheme::warning();
                    else
                        dotCol = MixCoachTheme::success();

                    // Draw dot — highlight if this event is currently selected
                    float dotAlpha = (clickedEventIdx_ == ei) ? 1.0f : 0.80f;
                    g.setColour(dotCol.withAlpha(dotAlpha));
                    g.fillEllipse((float)eventX, (float)dotY, (float)dotSize, (float)dotSize);

                    // Selected glow ring
                    if (clickedEventIdx_ == ei) {
                        g.setColour(dotCol.withAlpha(0.35f));
                        g.drawEllipse(
                            (float)(eventX - 2), (float)(dotY - 2), (float)(dotSize + 4), (float)(dotSize + 4), 1.5f);
                    }

                    eventX += dotSize + 4;

                    // Event message (truncated)
                    juce::String evMsg = ev.message;
                    if (evMsg.length() > 28) evMsg = evMsg.substring(0, 26) + "…";

                    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)));
                    auto textCol = (clickedEventIdx_ == ei) ? textBright().withAlpha(1.0f)
                                                            : textDim().withAlpha(0.80f);
                    g.setColour(textCol);
                    int textW = juce::GlyphArrangement::getStringWidthInt(
                                    juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)), evMsg)
                                + 6;
                    if (eventX + textW > maxW) break;

                    auto textArea =
                        juce::Rectangle<int>(eventX, bannerArea.getY() + 3, textW, bannerArea.getHeight() - 6);

                    // Selected highlight bg
                    if (clickedEventIdx_ == ei) {
                        g.setColour(MixCoachTheme::tooltipBorder().withAlpha(0.50f));
                        g.fillRoundedRectangle(textArea.toFloat(), 3.0f);
                    }

                    g.drawFittedText(evMsg, textArea, juce::Justification::centredLeft, 1);

                    // Store hit bounds for this event (dot + text combined)
                    auto eventBounds = juce::Rectangle<int>(
                        eventX - (dotSize + 4), bannerArea.getY(), textW + dotSize + 4, bannerArea.getHeight());
                    bannerEventBounds_.push_back(eventBounds);

                    eventX += textW + 8;
                }

                // Event count indicator
                if ((int)topEvents_.size() > maxShow) {
                    int remaining       = (int)topEvents_.size() - maxShow;
                    juce::String remStr = "+" + juce::String(remaining);
                    g.setFont(interFont(7.5f).boldened());
                    g.setColour(MixCoachTheme::accentCyan().withAlpha(0.60f));
                    auto remArea = juce::Rectangle<int>(eventX, bannerArea.getY() + 3, 24, bannerArea.getHeight() - 6);
                    g.drawFittedText(remStr, remArea, juce::Justification::centredLeft, 1);
                }
            }
            else if (!bannerCollapsed_) {
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)));
                g.setColour(textMuted().withAlpha(0.60f));
                auto emptyArea = juce::Rectangle<int>(
                    bannerCollapseBtn_.getRight() + 76, bannerArea.getY() + 3, 200, bannerArea.getHeight() - 6);
                g.drawFittedText("No recent events", emptyArea, juce::Justification::centredLeft, 1);
            }
            else {
                // Collapsed: show count
                if (!topEvents_.empty()) {
                    int nEvents           = (int)topEvents_.size();
                    juce::String countStr = juce::String(nEvents) + " event" + (nEvents > 1 ? "s" : "");
                    g.setFont(interFont(8.0f).boldened());
                    g.setColour(textDim().withAlpha(0.50f));

                    int countX = bannerCollapseBtn_.getRight() + 76;
                    auto countArea =
                        juce::Rectangle<int>(countX, bannerArea.getY() + 3, 100, bannerArea.getHeight() - 6);
                    g.drawFittedText(countStr, countArea, juce::Justification::centredLeft, 1);
                }
            }
            area.removeFromTop(2);
        }

        for (int busIdx = 0; busIdx <= kNumBuses; ++busIdx) {
            auto& group = busGroups_[busIdx];
            if (group.count == 0) continue;

            if (area.getHeight() < kHeaderHeight + kCardHeight + MixCoachTheme::spacingSM / 2) break;
            drawBusHeader(g, area, busIdx, group.count);

            int rowsInGroup = juce::jmin(group.count, kMaxRowsPerBus);
            for (int r = 0; r < rowsInGroup; ++r) {
                if (area.getHeight() < kCardHeight + MixCoachTheme::spacingSM / 2) break;
                int slotIdx = group.slotIndices[r];
                // SPRINT 5: Health filter - skip filtered out tracks
                if (isFilteredOut(slotIdx)) continue;
                auto cardArea = area.removeFromTop(kCardHeight).reduced(0, 1);
                drawTrackCard(g, cardArea, messengers_[slotIdx], r);
            }
            area.removeFromTop(MixCoachTheme::spacingXS);
        }

        // ═══ SPRINT 5: Health dot tooltip ═══════════════════════════════
        if (tooltipSlot_ >= 0 && tooltipSlot_ < SlotRegistry::kMaxSlots) {
            auto& entry = messengers_[tooltipSlot_];
            if (entry.info.active && entry.hasSignal && coachEngine_ != nullptr) {
                auto& feed  = coachEngine_->getTrackFeedCore();
                auto events = feed.getRecentEvents(tooltipSlot_, 5);

                if (!events.empty()) {
                    int tooltipW  = 220;
                    int tooltipH  = 12 + (int)events.size() * 14 + 6;
                    auto mousePos = getMouseXYRelative();
                    int tipX      = juce::jmin(mousePos.x + 12, getWidth() - tooltipW - 8);
                    int tipY      = juce::jmax(mousePos.y - tooltipH - 8, 0);

                    auto tipBounds = juce::Rectangle<int>(tipX, tipY, tooltipW, tooltipH);

                    g.setColour(MixCoachTheme::tooltipBg());
                    g.fillRoundedRectangle(tipBounds.toFloat(), 6.0f);

                    g.setColour(MixCoachTheme::tooltipBorder());
                    g.drawRoundedRectangle(tipBounds.toFloat(), 6.0f, 1.0f);

                    int ty = tipY + 6;
                    for (const auto& ev : events) {
                        // Severity dot
                        juce::Colour dotCol;
                        if (ev.severity >= 0.7f) dotCol = MixCoachTheme::error();
                        else if (ev.severity >= 0.4f)
                            dotCol = MixCoachTheme::warning();
                        else
                            dotCol = MixCoachTheme::success();

                        int dotSize = 5;
                        g.setColour(dotCol.withAlpha(0.85f));
                        g.fillEllipse((float)(tipX + 6), (float)(ty + 4), (float)dotSize, (float)dotSize);

                        // Message
                        juce::String evMsg = ev.message;
                        if (evMsg.length() > 32) evMsg = evMsg.substring(0, 30) + "…";

                        g.setFont(juce::Font(juce::FontOptions(8.0f)));
                        g.setColour(textBright());
                        auto textArea = juce::Rectangle<int>(tipX + 14, ty, tooltipW - 20, 14);
                        g.drawFittedText(evMsg, textArea, juce::Justification::centredLeft, 1);

                        ty += 14;
                    }
                }
            }
        }
        // === SPRINT 7: Banner event detail popup ===
        if (clickedEventIdx_ >= 0 && clickedEventIdx_ < (int)topEvents_.size()) {
            const auto& ev = topEvents_[clickedEventIdx_];

            juce::String trackName;
            if (ev.trackId >= 0 && ev.trackId < SlotRegistry::kMaxSlots) {
                auto& entry = messengers_[ev.trackId];
                if (entry.info.active) {
                    trackName = juce::String(entry.info.trackName).trim();
                    if (trackName.isEmpty()) trackName = "Track " + juce::String(ev.trackId + 1);
                }
            }

            juce::String contextLabel;
            if (ev.context == "gain") contextLabel = "Gain Staging";
            else if (ev.context == "dynamics")
                contextLabel = "Dynamics";
            else if (ev.context == "tonal")
                contextLabel = "Tonal Balance";
            else
                contextLabel = "General";

            juce::String severityLabel;
            juce::Colour sevCol;
            if (ev.severity >= 0.7f) {
                severityLabel = "HIGH";
                sevCol        = MixCoachTheme::error();
            }
            else if (ev.severity >= 0.4f) {
                severityLabel = "MEDIUM";
                sevCol        = MixCoachTheme::warning();
            }
            else {
                severityLabel = "LOW";
                sevCol        = MixCoachTheme::success();
            }

            int popupW    = 280;
            int lineH     = 14;
            int headerH   = 20;
            int bodyLines = 4;
            if (trackName.isNotEmpty()) bodyLines++;
            if (std::abs(ev.deviation) > 0.001f || std::abs(ev.value) > 0.001f) bodyLines++;
            int popupH = headerH + 6 + bodyLines * lineH + 8;

            // Position popup near mouse click, clamped within viewport
            int popupX = juce::jmax(4, juce::jmin(clickedMousePos_.x - popupW / 2, getWidth() - popupW - 4));
            int popupY = clickedMousePos_.y + 16;
            if (popupY + popupH > getHeight() - 4) popupY = clickedMousePos_.y - popupH - 8;
            popupY = juce::jmax(4, popupY);

            auto popupBounds = juce::Rectangle<int>(popupX, popupY, popupW, popupH);
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            g.fillRoundedRectangle(popupBounds.expanded(2.0f).toFloat(), 8.0f);
            g.setColour(MixCoachTheme::tooltipBg().withAlpha(0.97f));
            g.fillRoundedRectangle(popupBounds.toFloat(), 6.0f);
            g.setColour(sevCol.withAlpha(0.50f));
            g.drawRoundedRectangle(popupBounds.toFloat(), MixCoachTheme::cornerRadius_medium, 1.2f);

            int py = popupY + 4;
            // Header: context label + severity badge
            {
                auto headerArea = juce::Rectangle<int>(popupX + 8, py, popupW - 16, headerH);
                g.setFont(interFont(9.0f).boldened());
                g.setColour(MixCoachTheme::accentCyan().withAlpha(0.90f));
                g.drawFittedText(contextLabel, headerArea, juce::Justification::centredLeft, 1);
                int badgeW = juce::GlyphArrangement::getStringWidthInt(interFont(7.5f).boldened(), severityLabel) + 10;
                auto badgeArea = juce::Rectangle<int>(popupX + popupW - 8 - badgeW, py + 2, badgeW, headerH - 4);
                g.setColour(sevCol.withAlpha(0.15f));
                g.fillRoundedRectangle(badgeArea.toFloat(), MixCoachTheme::cornerRadius_small);
                g.setColour(sevCol.withAlpha(0.85f));
                g.setFont(interFont(MixCoachTheme::fontSizeExtraTiny).boldened());
                g.drawFittedText(severityLabel, badgeArea, juce::Justification::centred, 1);
                py += headerH + 2;
            }
            // Divider
            {
                auto divArea = juce::Rectangle<int>(popupX + 8, py, popupW - 16, 1);
                g.setColour(MixCoachTheme::tooltipBorder().withAlpha(0.60f));
                g.fillRect(divArea.toFloat());
                py += 4;
            }
            // Track name
            if (trackName.isNotEmpty()) {
                g.setFont(interFont(11.0f).boldened());
                g.setColour(textBright());
                auto nameArea = juce::Rectangle<int>(popupX + 8, py, popupW - 16, lineH);
                g.drawFittedText(trackName, nameArea, juce::Justification::centredLeft, 1);
                py += lineH;
            }
            // Full message
            {
                g.setFont(interFont(8.5f));
                g.setColour(textBright());
                auto msgArea = juce::Rectangle<int>(popupX + 8, py, popupW - 16, lineH * 3);
                g.drawFittedText(ev.message, msgArea, juce::Justification::centredLeft, 3);
                py += lineH * 3;
            }
            // Metrics
            if (std::abs(ev.deviation) > 0.001f || std::abs(ev.value) > 0.001f) {
                juce::String metricsStr = "Value: " + juce::String(ev.value, 1);
                if (std::abs(ev.threshold) > 0.001f) metricsStr += " | Target: " + juce::String(ev.threshold, 1);
                if (std::abs(ev.deviation) > 0.001f)
                    metricsStr +=
                        " | " + juce::String(ev.deviation > 0 ? "+" : "") + juce::String(ev.deviation, 1) + " dB";
                g.setFont(interFont(8.0f));
                g.setColour(textDim());
                auto metricsArea = juce::Rectangle<int>(popupX + 8, py, popupW - 16, lineH);
                g.drawFittedText(metricsStr, metricsArea, juce::Justification::centredLeft, 1);
                py += lineH;
            }
            // Dismiss hint
            {
                g.setFont(interFont(7.0f));
                g.setColour(textMuted().withAlpha(0.60f));
                auto hintArea = juce::Rectangle<int>(popupX + 8, py, popupW - 16, lineH);
                g.drawFittedText("Click outside to dismiss", hintArea, juce::Justification::centredRight, 1);
            }
        }
    }

    // ════════════════════════════════════════════════════════════════════
    //  drawBusHeader
    // ═══════════════════════════════════════════════════════════════════════════
    void MessengerListComponent::drawBusHeader(juce::Graphics& g, juce::Rectangle<int>& bounds, int busIdx, int count)
    {
        auto headerArea = bounds.removeFromTop(kHeaderHeight).reduced(0, 1);

        juce::Colour busColour;
        juce::String busName;

        if (busIdx == kNumBuses) {
            busColour = MixCoachTheme::textMuted();
            busName   = "UNASSIGNED";
        }
        else {
            busColour = getBusColour(busIdx);
            busName   = juce::String(busNames[busIdx]).toUpperCase();
        }

        auto accentBar = headerArea.removeFromLeft(2);
        g.setColour(busColour.withAlpha(0.6f));
        g.fillRect(accentBar.reduced(0, 2).toFloat());
        headerArea.removeFromLeft(4);

        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
        g.setColour(busColour);
        auto nameArea = headerArea.removeFromLeft(80);
        g.drawText(busName, nameArea, juce::Justification::centredLeft);

        auto badgeArea = headerArea.removeFromLeft(26).reduced(0, MixCoachTheme::spacingSM / 2);
        g.setColour(busColour.withAlpha(0.12f));
        g.fillRoundedRectangle(badgeArea.toFloat(), 5.0f);
        g.setColour(busColour);
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
        g.drawText(juce::String(count), badgeArea, juce::Justification::centred);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawTrackCard
    // ═══════════════════════════════════════════════════════════════════════════
    void MessengerListComponent::drawTrackCard(juce::Graphics& g,
                                               juce::Rectangle<int> bounds,
                                               const MessengerEntry& entry,
                                               int index)
    {
        juce::ignoreUnused(index);

        float fade = entry.fadeAlpha;
        if (fade <= 0.001f) return;

        bool isSelected    = (entry.info.slotIndex == selectedSlot_);
        auto cardBounds    = bounds.toFloat();
        const float corner = (float)kCardCorner;

        float hoverA   = hoverGlow_.getCurrent();
        bool isHovered = (hoveredSlot_ == entry.info.slotIndex) && (hoverA > 0.01f);

        if (isHovered && !isSelected) {
            auto glowBounds = cardBounds.expanded(2.0f);
            g.setColour(MixCoachTheme::accent().withAlpha(0.06f * hoverA * fade));
            g.fillRoundedRectangle(glowBounds, corner + 2.0f);
            g.setColour(MixCoachTheme::accent().withAlpha(0.18f * hoverA * fade));
            g.drawRoundedRectangle(cardBounds, corner, 1.0f);
        }

        if (isSelected) {
            auto selGlowBounds = cardBounds.expanded(3.0f);
            g.setColour(MixCoachTheme::accent().withAlpha(0.08f * fade));
            g.fillRoundedRectangle(selGlowBounds, corner + 3.0f);
        }

        // ═══ Pending role outer glow (pulsating cyan halo) ═══════════════════
        // A subtle pulsating glow appears behind track cards where the role was
        // inferred (via signal-order, name, or spectral) but not yet confirmed.
        // Pulse at ~1.2 Hz with a slight phase offset from the border pulse.
        bool isPending = (entry.signalOrderPending || entry.roleInferredPending)
                         && entry.trackRole != TrackRole::Unknown;

        if (isPending) {
            float pg               = pulseAlpha(1.2f, 0.0f);
            float pulseAlphaVal    = 0.06f + 0.12f * pg;
            auto pendingGlowBounds = cardBounds.expanded(4.0f);
            g.setColour(MixCoachTheme::accentCyan().withAlpha(pulseAlphaVal * fade));
            g.fillRoundedRectangle(pendingGlowBounds, corner + 3.0f);

            // Second wider glow layer for depth
            auto pendingGlowOuter = cardBounds.expanded(6.0f);
            g.setColour(MixCoachTheme::accentCyan().withAlpha(pulseAlphaVal * 0.5f * fade));
            g.fillRoundedRectangle(pendingGlowOuter, corner + 4.0f);
        }

        auto shadowOuter = cardBounds.expanded(3.0f);
        g.setColour(juce::Colours::black.withAlpha(0.25f * fade));
        g.fillRoundedRectangle(shadowOuter, corner + 2.0f);

        auto shadowMid = cardBounds.expanded(1.5f);
        g.setColour(juce::Colours::black.withAlpha(0.15f * fade));
        g.fillRoundedRectangle(shadowMid, corner + 1.0f);

        g.setColour(MixCoachTheme::bgCanvas().withAlpha(fade));
        g.fillRoundedRectangle(cardBounds, corner);

        if (isSelected) {
            g.setColour(MixCoachTheme::accent().withAlpha(0.12f));
            g.fillRoundedRectangle(cardBounds, corner);
        }

        if (isSelected) {
            g.setColour(MixCoachTheme::accent().withAlpha(fade * 0.85f));
            g.drawRoundedRectangle(cardBounds, corner, 1.5f);
            g.setColour(MixCoachTheme::accent().withAlpha(fade * 0.12f));
            g.drawRoundedRectangle(cardBounds.reduced(1.0f), corner - 1.0f, 1.0f);
        }
        else if (isHovered) {
            g.setColour(MixCoachTheme::accent().withAlpha(fade * 0.25f));
            g.drawRoundedRectangle(cardBounds, corner, 1.0f);
        }
        else {
            g.setColour(MixCoachTheme::tooltipBorder().withAlpha(fade * 0.5f));
            g.drawRoundedRectangle(cardBounds, corner, 0.8f);
        }

        // ═══ Pending role pulsating border (over the normal border) ═════════
        // Draws a pulsing cyan outline on top of the existing border to draw
        // attention to tracks that need role confirmation.
        // Phase offset of 0.3 ensures border and outer glow don't pulse in sync.
        if (isPending) {
            float pb          = pulseAlpha(1.4f, 0.3f);
            float borderAlpha = 0.25f + 0.50f * pb;
            g.setColour(MixCoachTheme::accentCyan().withAlpha(borderAlpha * fade));
            g.drawRoundedRectangle(cardBounds, corner, 1.5f);

            // Inner secondary line for extra emphasis
            g.setColour(MixCoachTheme::accentCyan().withAlpha(borderAlpha * 0.3f * fade));
            g.drawRoundedRectangle(cardBounds.reduced(1.5f), corner - 1.0f, 0.8f);
        }

        auto glassBar = cardBounds.withHeight(cardBounds.getHeight() * 0.45f);
        juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(isSelected ? 0.09f : 0.06f * fade),
                                       juce::Point<float>(0.0f, glassBar.getY()),
                                       juce::Colour(0x00000000),
                                       juce::Point<float>(0.0f, glassBar.getBottom()),
                                       false);
        g.setGradientFill(glassGrad);
        g.fillRoundedRectangle(glassBar, corner);

        int h  = bounds.getHeight();
        int w  = bounds.getWidth();
        int x0 = bounds.getX();
        int y0 = bounds.getY();

        // ─── 1. Bus pill (right) ─────────────────────────────────────────────
        int routeX = x0 + w - 34;
        juce::Rectangle<int> routeArea(routeX, y0 + 4, 32, h - 8);

        if (entry.info.bus != BusType::None) {
            int busIdx  = static_cast<int>(entry.info.bus);
            auto busCol = getBusColour(busIdx);
            juce::String abbr;
            switch (entry.info.bus) {
                case BusType::Drums:
                    abbr = "DRM";
                    break;
                case BusType::Bass:
                    abbr = "BAS";
                    break;
                case BusType::Guitars:
                    abbr = "GTR";
                    break;
                case BusType::Keys:
                    abbr = "KEY";
                    break;
                case BusType::Vocals:
                    abbr = "VOX";
                    break;
                case BusType::FX:
                    abbr = "FX";
                    break;
                default:
                    abbr = juce::String(busNames[busIdx]).substring(0, 3).toUpperCase();
                    break;
            }
            g.setColour(busCol.withAlpha(0.12f * fade));
            g.fillRoundedRectangle(routeArea.toFloat(), 4.0f);
            g.setColour(busCol.withAlpha(fade * 0.85f));
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened());
            g.drawFittedText(abbr, routeArea, juce::Justification::centred, 1);
        }

        // ─── 2. Role pill with confidence indicator ───────────────────────────
        int roleX     = routeX - (entry.signalOrderPending ? 48 : 32); // leave room for badge
        int confW     = 10;                                            // width for confidence icon
        auto roleArea = juce::Rectangle<int>(roleX, y0 + h - 14, 32 + confW, 12);

        bool hasRole = (entry.trackRole != TrackRole::Unknown);
        if (hasRole) {
            auto cat               = getRoleCategory(entry.trackRole);
            auto roleCol           = roleColourForCategory(cat);
            juce::String roleLabel = juce::String(getRoleName(entry.trackRole)).substring(0, 4).toUpperCase();

            // If signal order pending, add blue tint to the role pill
            float alpha  = entry.signalOrderPending ? 0.15f : 0.08f;
            auto pillCol = entry.signalOrderPending ? MixCoachTheme::accentCyan() : roleCol;

            // Pill background (wider to fit confidence icon)
            g.setColour(pillCol.withAlpha(alpha * fade));
            g.fillRoundedRectangle(roleArea.toFloat(), 3.0f);

            // Role label
            auto labelArea = roleArea.removeFromLeft(32);
            g.setColour(roleCol.withAlpha(fade * 0.65f));
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizePico)).boldened());
            g.drawFittedText(roleLabel, labelArea, juce::Justification::centred, 1);

            // Confidence icon ✅/⚠️/❌ inside the pill
            juce::String confIcon;
            if (entry.roleConfidence >= 0.75f) confIcon = "[OK]"; // ✓
            else if (entry.roleConfidence >= 0.4f)
                confIcon = "[WARN]"; // ⚠
            else
                confIcon = "\xE2\x9D\x8C"; // ❌

            juce::Colour confCol = (entry.roleConfidence >= 0.75f)  ? MixCoachTheme::success()
                                   : (entry.roleConfidence >= 0.4f) ? MixCoachTheme::warning()
                                                                    : MixCoachTheme::error();
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizePico)).boldened());
            g.setColour(confCol.withAlpha(fade * 0.80f));
            g.drawFittedText(confIcon, roleArea, juce::Justification::centred, 1);
        }
        else {
            g.setColour(textMuted().withAlpha(0.08f * fade));
            g.fillRoundedRectangle(roleArea.toFloat(), 3.0f);
            g.setColour(textMuted().withAlpha(fade * 0.35f));
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizePico)).boldened());
            g.drawFittedText("SET", roleArea, juce::Justification::centred, 1);
            auto penArea = roleArea.removeFromRight(10).removeFromBottom(10);
            g.setColour(textMuted().withAlpha(fade * 0.25f));
            g.setFont(juce::Font(juce::FontOptions(5.5f)));
            g.drawFittedText("\xE2\x9C\x8F", penArea, juce::Justification::centred, 1);
        }

        // ─── 2b. Pending confirmation ⚡ badge (Sprint 1: generalized) ─────
        // Shows for ANY pending inferred role: signal-order, name, or spectral.
        // Click the badge in MessengerListComponent::mouseDown to confirm.
        // Pulsates to draw attention to unconfirmed roles.
        if (isPending) {
            // Position just right of the role pill (pill is now 42px wide: 32 label + 10 confidence)
            auto badgeArea = juce::Rectangle<int>(roleX + 43, y0 + h - 13, 14, 10);

            // Pulsating glow (slightly offset phase from border for visual variety)
            float bb        = pulseAlpha(1.6f, 0.6f);
            float pulseGlow = 0.10f + 0.20f * bb;
            float pulseText = 0.60f + 0.35f * bb;

            // Glow
            g.setColour(MixCoachTheme::accentCyan().withAlpha(pulseGlow * fade));
            g.fillRoundedRectangle(badgeArea.toFloat(), 3.0f);
            // Border
            g.setColour(MixCoachTheme::accentCyan().withAlpha(pulseText * fade * 0.8f));
            g.drawRoundedRectangle(badgeArea.toFloat(), 3.0f, 0.8f);
            // Lightning bolt text
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened());
            g.setColour(MixCoachTheme::accentCyan().withAlpha(pulseText * fade));
            g.drawFittedText("[BOLT]", badgeArea, juce::Justification::centred, 1);
        }

        // ─── 3. L/R peak values ─────────────────────────────────────────────
        int statsX = roleX - 52;
        auto lArea = juce::Rectangle<int>(statsX, y0 + 2, 62, h / 2 - 1);
        auto rArea = juce::Rectangle<int>(statsX, y0 + h / 2 + 1, 62, h / 2 - 3);

        float peakL = entry.peakLeft;
        float peakR = entry.peakRight;

        g.setColour(MixCoachTheme::channelLeft().withAlpha(fade * 0.9f));
        g.setFont(juce::Font(juce::FontOptions(8.5f)).boldened());
        juce::String lStr = peakL > kMeterMinDb + 1.0f ? "L " + juce::String(peakL, 1) + " dB" : "L --.- dB";
        g.drawFittedText(lStr, lArea, juce::Justification::centredRight, 1);

        g.setColour(MixCoachTheme::channelRight().withAlpha(fade * 0.9f));
        g.setFont(juce::Font(juce::FontOptions(8.5f)).boldened());
        juce::String rStr = peakR > kMeterMinDb + 1.0f ? "R " + juce::String(peakR, 1) + " dB" : "R --.- dB";
        g.drawFittedText(rStr, rArea, juce::Justification::centredRight, 1);

        // ─── 4. Mini-bars L/R ───────────────────────────────────────────────
        int meterX   = statsX - 12;
        float barW   = 5.0f;
        float gap    = 1.0f;
        float barTop = (float)(y0 + 8);
        float barBot = (float)(y0 + h - 8);
        float barH   = barBot - barTop;

        auto drawStereoBar = [&](float level, juce::Colour colour, float mx, float peakHoldLevel) {
            auto bgBounds = juce::Rectangle<float>(mx, barTop, barW, barH);
            g.setColour(MixCoachTheme::bgDarker().withAlpha(fade));
            g.fillRoundedRectangle(bgBounds, 2.0f);
            g.setColour(MixCoachTheme::bgDark().withAlpha(fade * 0.5f));
            g.drawRoundedRectangle(bgBounds, 2.0f, 0.5f);

            float norm = juce::jlimit(0.01f, 1.0f, (level - kMeterMinDb) / (kMeterMaxDb - kMeterMinDb));
            if (norm > 0.01f) {
                float fillH     = barH * norm;
                auto fillBounds = juce::Rectangle<float>(mx, barBot - fillH, barW, fillH);
                g.setColour(colour.withAlpha(0.85f));
                g.fillRoundedRectangle(fillBounds, 2.0f);
                if (fillH > 3.0f) {
                    auto shine = fillBounds.withHeight(juce::jmax(1.5f, fillH * 0.12f));
                    g.setColour(juce::Colours::white.withAlpha(0.18f * fade));
                    g.fillRoundedRectangle(shine, 2.0f);
                }
                if (fillH > 4.0f) {
                    auto glow = fillBounds.withHeight(juce::jmax(1.0f, fillH * 0.06f));
                    g.setColour(colour.withAlpha(0.12f));
                    g.fillRoundedRectangle(glow, 2.0f);
                }
            }
            if (peakHoldLevel > level + 0.5f && peakHoldLevel > kMeterMinDb + 1.0f) {
                float pkNorm = juce::jlimit(0.01f, 1.0f, (peakHoldLevel - kMeterMinDb) / (kMeterMaxDb - kMeterMinDb));
                float pkY    = barBot - barH * pkNorm;
                g.setColour(juce::Colours::white.withAlpha(0.08f * fade));
                g.fillEllipse(mx + barW * 0.5f - 3.0f, pkY - 3.0f, 6.0f, 6.0f);
                g.setColour(MixCoachTheme::warning().withAlpha(0.15f).withAlpha(0.85f * fade));
                g.fillEllipse(mx + barW * 0.5f - 1.5f, pkY - 1.5f, 3.0f, 3.0f);
            }
        };

        drawStereoBar(peakL, MixCoachTheme::channelLeft(), (float)meterX, entry.peakHold);
        drawStereoBar(peakR, MixCoachTheme::channelRight(), (float)meterX + barW + gap, entry.peakHold);

        // ─── 5. Color circle ─────────────────────────────────────────────────
        int circleSize = 10;
        int circleX    = x0 + 6;
        int circleY    = y0 + (h - circleSize) / 2;

        juce::Colour circleColour;
        if (entry.info.colour.getARGB() != 0xFF808080) circleColour = entry.info.colour;
        else if (entry.info.bus != BusType::None)
            circleColour = getBusColour(static_cast<int>(entry.info.bus));
        else
            circleColour = textMuted().withAlpha(0.3f);

        g.setColour(circleColour.withAlpha(0.15f * fade));
        g.fillEllipse((float)(circleX - 1), (float)(circleY - 1), (float)(circleSize + 2), (float)(circleSize + 2));
        g.setColour(circleColour.withAlpha(fade));
        g.fillEllipse((float)circleX, (float)circleY, (float)circleSize, (float)circleSize);
        g.setColour(juce::Colours::white.withAlpha(0.20f * fade));
        g.fillEllipse(
            (float)(circleX + 2), (float)(circleY + 1), (float)(circleSize * 0.3f), (float)(circleSize * 0.25f));

        // ─── Status strip (left edge) ────────────────────────────────────────
        {
            juce::Colour statusColour;
            if (entry.coachAdviceText.isNotEmpty()) {
                switch (entry.coachAdviceStatus) {
                    case SuggestionStatus::Red:
                        statusColour = MixCoachTheme::error();
                        break;
                    case SuggestionStatus::Yellow:
                        statusColour = MixCoachTheme::warning();
                        break;
                    case SuggestionStatus::Green:
                        statusColour = MixCoachTheme::success();
                        break;
                    case SuggestionStatus::White:
                        statusColour = textMuted();
                        break;
                    default:
                        statusColour = juce::Colours::transparentBlack;
                        break;
                }
                auto glowBounds = juce::Rectangle<float>((float)x0 - 1.0f, (float)y0 + 2.0f, 7.0f, (float)h - 4.0f);
                g.setColour(statusColour.withAlpha(0.10f * fade));
                g.fillRoundedRectangle(glowBounds, 3.0f);
                auto stripBounds = juce::Rectangle<float>((float)x0, (float)y0 + 3.0f, 4.0f, (float)h - 6.0f);
                g.setColour(statusColour.withAlpha(0.60f * fade));
                g.fillRoundedRectangle(stripBounds, 2.0f);
                auto centreLine = stripBounds.withWidth(2.0f).withLeft((float)x0 + 1.0f);
                g.setColour(statusColour.withAlpha(0.80f * fade));
                g.fillRoundedRectangle(centreLine, 1.5f);
            }
        }

        // ═══ Health dot (🟢🟡🔴⚪) — Sprint 7: consolidated from TrackAdvice::Status ═════
        int nameX = circleX + circleSize + 8; // se ajusta si hay health dot
        {
            juce::Colour healthColour;
            switch (entry.consolidatedHealth) {
                case SuggestionStatus::Green:
                    healthColour = MixCoachTheme::success();
                    break;
                case SuggestionStatus::Yellow:
                    healthColour = MixCoachTheme::warning();
                    break;
                case SuggestionStatus::Red:
                    healthColour = MixCoachTheme::error();
                    break;
                case SuggestionStatus::White:
                    healthColour = textMuted();
                    break;
                default:
                    healthColour = textMuted().withAlpha(0.2f);
                    break;
            }

            if (entry.hasSignal && entry.consolidatedHealth != SuggestionStatus::None) {
                int hDotSize = 8;
                int hDotX    = nameX;
                int hDotY    = y0 + (h - hDotSize) / 2;

                // Glow exterior
                g.setColour(healthColour.withAlpha(0.12f * fade));
                g.fillEllipse((float)(hDotX - 1), (float)(hDotY - 1), (float)(hDotSize + 2), (float)(hDotSize + 2));
                // Dot
                g.setColour(healthColour.withAlpha(0.80f * fade));
                g.fillEllipse((float)hDotX, (float)hDotY, (float)hDotSize, (float)hDotSize);
                // Highlight
                g.setColour(juce::Colours::white.withAlpha(0.18f * fade));
                g.fillEllipse(
                    (float)(hDotX + 2), (float)(hDotY + 1), (float)(hDotSize * 0.3f), (float)(hDotSize * 0.25f));

                nameX = hDotX + hDotSize + 5; // shift name right
            }
        }

        // ═══ Attention bar (thin vertical, right edge next to bus pill) ══════
        if (entry.hasSignal && entry.attentionScore > 0.01f) {
            int attBarX = routeX + 32 + 2; // right of bus pill
            int attBarY = y0 + 5;
            int attBarH = h - 10;
            int attBarW = 3;

            // Background
            g.setColour(MixCoachTheme::bgDarker().withAlpha(fade));
            g.fillRoundedRectangle((float)attBarX, (float)attBarY, (float)attBarW, (float)attBarH, 1.5f);

            // Fill
            float attNorm = juce::jlimit(0.01f, 1.0f, entry.attentionScore);
            float fillH   = (float)attBarH * attNorm;
            float fillY   = (float)(attBarY + attBarH) - fillH;

            juce::Colour attCol = (attNorm < 0.4f)   ? MixCoachTheme::success()
                                  : (attNorm < 0.7f) ? MixCoachTheme::warning()
                                                     : MixCoachTheme::error();
            g.setColour(attCol.withAlpha(0.65f * fade));
            g.fillRoundedRectangle((float)attBarX, fillY, (float)attBarW, fillH, 1.5f);
        }

        // ─── Track name ──────────────────────────────────────────────────────
        int nameRightX = juce::jmin(meterX - 4, routeX - 34 - 4); // avoid bus+role+att pills
        int nameW      = juce::jmax(40, nameRightX - nameX);
        auto nameArea  = juce::Rectangle<int>(nameX, y0 + 2, nameW, 16);
        g.setFont(interFont(13.0f));
        g.setColour(textBright().withAlpha(fade));
        auto name = juce::String(entry.info.trackName).trim();
        if (name.isEmpty()) name = "Pista " + juce::String(entry.info.slotIndex + 1);
        g.drawFittedText(name, nameArea, juce::Justification::centredLeft, 1);

        // ─── TrackFeed message ───────────────────────────────────────────────
        float peakCombined = juce::jmax(entry.peakLeft, entry.peakRight, kMeterMinDb);
        float rmsDb        = entry.rmsAvg;

        if (entry.coachAdviceText.isNotEmpty()) {
            juce::Colour trackFeedColour;
            switch (entry.coachAdviceStatus) {
                case SuggestionStatus::Red:
                    trackFeedColour = MixCoachTheme::error();
                    break;
                case SuggestionStatus::Yellow:
                    trackFeedColour = MixCoachTheme::warning();
                    break;
                case SuggestionStatus::Green:
                    trackFeedColour = MixCoachTheme::success();
                    break;
                case SuggestionStatus::White:
                    trackFeedColour = textMuted();
                    break;
                default:
                    trackFeedColour = textDim();
                    break;
            }
            auto feedArea =
                juce::Rectangle<int>(nameX, y0 + 17, nameW, 13); // h=13 + 1px overlap con name (igual que original)
            g.setFont(interFont(9.5f).boldened());
            g.setColour(trackFeedColour.withAlpha(fade * 0.95f));
            g.drawFittedText(entry.coachAdviceText, feedArea, juce::Justification::centredLeft, 1);
        }

        // ─── Analytics strip (PK, RMS, spectrum, CR) — pequeñito, para el cerebro ──
        {
            auto analArea = juce::Rectangle<int>(nameX, y0 + 30, nameW, 10);
            int aX        = analArea.getX();
            int aY        = analArea.getY();
            int aH        = analArea.getHeight();
            float aFade   = fade * 0.50f;

            // PK
            {
                juce::String val = peakCombined > kMeterMinDb + 1.0f ? juce::String(peakCombined, 1) + "dB" : "---";
                g.setFont(interFont(7.0f));
                g.setColour(textDim().withAlpha(aFade));
                g.drawFittedText("PK " + val, {aX, aY, 42, aH}, juce::Justification::centredLeft, 1);
                aX += 42;
            }

            // Separador
            g.setColour(textMuted().withAlpha(aFade * 0.3f));
            g.drawFittedText("|", {aX, aY, 6, aH}, juce::Justification::centredLeft, 1);
            aX += 7;

            // RMS
            {
                juce::String val = rmsDb > kMeterMinDb + 1.0f ? juce::String(rmsDb, 1) + "dB" : "---";
                g.setFont(interFont(7.0f));
                g.setColour(textDim().withAlpha(aFade));
                g.drawFittedText("RMS " + val, {aX, aY, 44, aH}, juce::Justification::centredLeft, 1);
                aX += 45;
            }

            // Separador
            g.setColour(textMuted().withAlpha(aFade * 0.3f));
            g.drawFittedText("|", {aX, aY, 6, aH}, juce::Justification::centredLeft, 1);
            aX += 7;

            // 6-band spectrum (mini barras verticales)
            if (entry.hasSignal) {
                float maxE = -100.0f;
                for (int r = 0; r < 6; ++r) maxE = juce::jmax(maxE, entry.bandLevelDb[r]);

                float bandH = (float)aH - 3.0f;
                float bandW = 3.0f;
                float gap   = 1.0f;

                for (int r = 0; r < 6; ++r) {
                    float e    = entry.bandLevelDb[r];
                    float frac = 0.05f;
                    if (maxE > -90.0f && e > -90.0f) frac = juce::jlimit(0.05f, 1.0f, (e + 60.0f) / 54.0f);

                    float bH  = bandH * frac;
                    float bX  = (float)(aX + 2);
                    float bY2 = (float)(aY + aH) - bH - 1.5f;

                    g.setColour(MixCoachTheme::specColour(r).withAlpha(aFade * 0.80f));
                    g.fillRect(bX, bY2, bandW, bH);
                    aX += (int)(bandW + gap);
                }
                aX += 4;
            }

            // Separador
            g.setColour(textMuted().withAlpha(aFade * 0.3f));
            g.drawFittedText("|", {aX, aY, 6, aH}, juce::Justification::centredLeft, 1);
            aX += 7;

            // CR value + mini bar
            {
                juce::Colour crestCol = textMuted().withAlpha(aFade);
                if (entry.hasSignal && entry.crestDb > 0.1f) {
                    if (entry.crestDb < 4.0f || entry.crestDb > 24.0f)
                        crestCol = MixCoachTheme::error().withAlpha(aFade * 0.85f);
                    else if (entry.crestDb < 6.0f || entry.crestDb > 18.0f)
                        crestCol = MixCoachTheme::warning().withAlpha(aFade * 0.85f);
                    else
                        crestCol = MixCoachTheme::success().withAlpha(aFade * 0.85f);
                }
                juce::String val = (entry.hasSignal && entry.crestDb > 0.1f) ? juce::String(entry.crestDb, 1) + "dB"
                                                                             : "---";
                g.setFont(interFont(7.0f).boldened());
                g.setColour(crestCol);
                g.drawFittedText("CR " + val, {aX, aY, 36, aH}, juce::Justification::centredLeft, 1);

                // CR mini-bar horizontal
                if (entry.hasSignal && entry.crestDb > 0.1f) {
                    float crNorm = juce::jlimit(0.05f, 1.0f, entry.crestDb / 24.0f);
                    float crBW   = 16.0f * crNorm;
                    float crBX   = (float)(aX + 36);
                    float crBY   = (float)(aY + aH - 3);
                    g.setColour(crestCol.withAlpha(aFade * 0.45f));
                    g.fillRect(crBX, crBY, crBW, 2.0f);
                }
            }
        }

        // ─── Stereo width indicator (bottom edge) ────────────────────────────
        {
            int rightEdge = routeX + 2;
            int swBarX    = rightEdge - 14;
            int swBarY    = y0 + h - 4;
            int swBarW    = 12;
            int swBarH    = 3;
            float swNorm  = juce::jlimit(0.0f, 1.0f, entry.avgStereoWidth);
            float swFill  = swNorm * (float)swBarW;

            // Background
            g.setColour(MixCoachTheme::bgDarker().withAlpha(fade));
            g.fillRect((float)swBarX, (float)swBarY, (float)swBarW, (float)swBarH);

            // Fill — color según zona
            juce::Colour swColour;
            if (entry.correlation < 0.0f) swColour = MixCoachTheme::error().withAlpha(fade * 0.70f);
            else if (swNorm < 0.2f)
                swColour = MixCoachTheme::textDim().withAlpha(fade * 0.50f);
            else if (swNorm < 0.4f)
                swColour = MixCoachTheme::accentCyan().withAlpha(fade * 0.65f);
            else
                swColour = MixCoachTheme::accent().withAlpha(fade * 0.70f);

            g.setColour(swColour);
            g.fillRect((float)swBarX, (float)swBarY, swFill, (float)swBarH);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getPreferredHeight
    // ═══════════════════════════════════════════════════════════════════════════
    int MessengerListComponent::getPreferredHeight() const
    {
        if (activeMessengerCount_ == 0) return getHeight();

        int totalHeight = 4;

        if (groupingMode_ == GroupingMode::Bus) {
            for (int busIdx = 0; busIdx <= kNumBuses; ++busIdx) {
                auto& group = busGroups_[busIdx];
                if (group.count == 0) continue;
                totalHeight += kHeaderHeight + 2;
                if (!collapsedGroups_[busIdx]) totalHeight += (kCardHeight + kCardGap) * group.count;
                totalHeight += 6;
            }
        }
        else {
            totalHeight += (kCardHeight + kCardGap) * activeMessengerCount_;
            totalHeight += 6;
        }
        return totalHeight + 8;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  hitTestSlot
    // ═══════════════════════════════════════════════════════════════════════════
    int MessengerListComponent::hitTestSlot(juce::Point<int> point) const
    {
        if (activeMessengerCount_ == 0) return -1;

        auto area = getLocalBounds().reduced(2, 4);
        int y     = area.getY();

        for (int busIdx = 0; busIdx <= kNumBuses; ++busIdx) {
            auto& group = busGroups_[busIdx];
            if (group.count == 0) continue;

            y += kHeaderHeight;

            if (!collapsedGroups_[busIdx]) {
                for (int r = 0; r < group.count; ++r) {
                    int slotIdx = group.slotIndices[r];
                    if (juce::Rectangle<int>(area.getX(), y, area.getWidth(), kCardHeight).contains(point))
                        return messengers_[slotIdx].info.slotIndex;
                    y += kCardHeight;
                }
            }
            y += 4;
        }
        return -1;
    }

} // namespace mixcoach
