#include "GainStagingPanel.h"
#include <cmath>

namespace mixcoach {

    GainStagingPanel::GainStagingPanel()
    {
        setOpaque(false);
        setVisible(false);
    }

    void GainStagingPanel::setTrackData(const std::vector<TrackGainRow>& rows)
    {
        rows_ = rows;
        applyBtnBounds_.resize(rows.size());
        resized();
        repaint();
    }

    void GainStagingPanel::clear()
    {
        rows_.clear();
        applyBtnBounds_.clear();
        repaint();
    }

    void GainStagingPanel::resized()
    {
        auto bounds = getLocalBounds().reduced(kPadding, kPadding);
        int y = kHeaderHeight;

        // Apply All button at the bottom
        int totalContentH = kHeaderHeight + static_cast<int>(rows_.size()) * (kRowHeight + kGap);
        int applyAllY = juce::jmax(bounds.getY() + totalContentH + 8,
                                   bounds.getBottom() - 30);
        applyAllBounds_ = juce::Rectangle<int>(bounds.getCentreX() - 60, applyAllY, 120, 24);

        // Calculate per-row apply button bounds
        for (int i = 0; i < (int)rows_.size(); ++i) {
            int rowY = bounds.getY() + kHeaderHeight + i * (kRowHeight + kGap);
            int btnX = bounds.getRight() - kApplyWidth - 4;
            int btnY = rowY + (kRowHeight - 20) / 2;
            applyBtnBounds_[i] = juce::Rectangle<int>(btnX, btnY, kApplyWidth, 20);
        }
    }

    void GainStagingPanel::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().reduced(kPadding, kPadding);
        if (bounds.isEmpty()) return;

        // ─── Panel background ─────────────────────────────────────────────
        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        // ─── Header ───────────────────────────────────────────────────────
        {
            auto header = bounds.removeFromTop(kHeaderHeight);
            g.setColour(MixCoachTheme::accentGlow());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSectionHeader)).boldened());
            g.drawText("GAIN STAGING — Niveles por pista", header.reduced(4, 0),
                       juce::Justification::centredLeft);

            // Sub-header: labels
            auto labelArea = header.withTop(header.getBottom() - 14).withHeight(14);
            int x = labelArea.getX() + 4;
            g.setColour(MixCoachTheme::textDim());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
            g.drawText("Pista",   juce::Rectangle<int>(x, labelArea.getY(), kNameWidth, 14), juce::Justification::centredLeft); x += kNameWidth;
            g.drawText("Rol",    juce::Rectangle<int>(x, labelArea.getY(), kRoleWidth, 14), juce::Justification::centredLeft); x += kRoleWidth;
            g.drawText("Peak",   juce::Rectangle<int>(x, labelArea.getY(), kPeakWidth, 14), juce::Justification::centredLeft); x += kPeakWidth;
            g.drawText("Target", juce::Rectangle<int>(x, labelArea.getY(), kTargetWidth, 14), juce::Justification::centredLeft); x += kTargetWidth;
            g.drawText("Nivel",  juce::Rectangle<int>(x, labelArea.getY(), kBarWidth, 14), juce::Justification::centredLeft);
        }

        if (rows_.empty()) {
            g.setColour(MixCoachTheme::textMuted());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)));
            g.drawText("Esperando datos de pistas... Dale Play a tu sesi\\xC3\\xB3n.",
                       bounds.reduced(8), juce::Justification::centred);
            return;
        }

        // ─── Row rendering ────────────────────────────────────────────────
        int y = bounds.getY() + kHeaderHeight;
        for (int i = 0; i < (int)rows_.size(); ++i) {
            const auto& row = rows_[i];
            auto rowBounds = juce::Rectangle<int>(bounds.getX(), y, bounds.getWidth(), kRowHeight);

            // Row background (alternating + hover)
            if (i == hoveredRow_) {
                g.setColour(MixCoachTheme::accentBg().withAlpha(0.08f));
                g.fillRoundedRectangle(rowBounds.toFloat(), 2.0f);
            } else if (i % 2 == 0) {
                g.setColour(juce::Colours::white.withAlpha(0.02f));
                g.fillRoundedRectangle(rowBounds.toFloat(), 2.0f);
            }

            int x = rowBounds.getX() + 4;

            // Track name
            g.setColour(row.clipping ? MixCoachTheme::error() : MixCoachTheme::textBright());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
            juce::String displayName = row.trackName.substring(0, 12);
            g.drawText(displayName, juce::Rectangle<int>(x, rowBounds.getY(), kNameWidth, kRowHeight),
                       juce::Justification::centredLeft); x += kNameWidth;

            // Role
            g.setColour(MixCoachTheme::textDim());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
            g.drawText(row.roleName, juce::Rectangle<int>(x, rowBounds.getY(), kRoleWidth, kRowHeight),
                       juce::Justification::centredLeft); x += kRoleWidth;

            // Current peak
            juce::Colour peakCol = MixCoachTheme::textSecondary();
            if (row.clipping) peakCol = MixCoachTheme::error();
            else if (row.currentPeakDb > -6.0f) peakCol = MixCoachTheme::warning();
            else if (row.currentPeakDb > -18.0f) peakCol = MixCoachTheme::success();
            g.setColour(peakCol);
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
            g.drawText(juce::String(row.currentPeakDb, 1) + " dB",
                       juce::Rectangle<int>(x, rowBounds.getY(), kPeakWidth, kRowHeight),
                       juce::Justification::centredLeft); x += kPeakWidth;

            // Target
            g.setColour(MixCoachTheme::textMuted());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
            g.drawText(juce::String(row.targetPeakDb, 1) + " dB",
                       juce::Rectangle<int>(x, rowBounds.getY(), kTargetWidth, kRowHeight),
                       juce::Justification::centredLeft); x += kTargetWidth;

            // Level bar
            auto barBounds = juce::Rectangle<int>(x, rowBounds.getY() + 8, kBarWidth, kRowHeight - 16);
            float level = juce::jmap(juce::jlimit(-40.0f, 0.0f, row.currentPeakDb),
                                      -40.0f, 0.0f, 0.0f, 1.0f);
            MixCoachTheme::drawGradientMeter(g, barBounds.toFloat(), level, false, 2.0f);

            // Target marker on bar
            float targetNorm = juce::jmap(juce::jlimit(-40.0f, 0.0f, row.targetPeakDb),
                                           -40.0f, 0.0f, 0.0f, 1.0f);
            int markerX = barBounds.getX() + (int)(barBounds.getWidth() * targetNorm);
            g.setColour(MixCoachTheme::accentGlow().withAlpha(0.6f));
            g.drawVerticalLine(markerX, barBounds.getY() - 2, barBounds.getBottom() + 2);
            x += kBarWidth;

            // Apply button
            if (i < (int)applyBtnBounds_.size() && !row.isApplied) {
                auto btn = applyBtnBounds_[i];
                bool hovered = (hoveredRow_ == i && btn.contains(getMouseXYRelative()));
                g.setColour(hovered ? MixCoachTheme::accent() : MixCoachTheme::accentDim());
                g.fillRoundedRectangle(btn.toFloat(), 3.0f);
                g.setColour(MixCoachTheme::textBright());
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
                g.drawText("Aplicar", btn, juce::Justification::centred);
            } else if (row.isApplied) {
                auto btn = applyBtnBounds_[i];
                g.setColour(MixCoachTheme::success().withAlpha(0.2f));
                g.fillRoundedRectangle(btn.toFloat(), 3.0f);
                g.setColour(MixCoachTheme::success());
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
                g.drawText("\\xE2\\x9C\\x93", btn, juce::Justification::centred);
            }

            y += kRowHeight + kGap;
        }

        // ─── Apply All button ──────────────────────────────────────────────
        if (rows_.size() > 1) {
            bool allApplied = true;
            for (const auto& r : rows_) if (!r.isApplied) { allApplied = false; break; }

            if (!allApplied) {
                bool hovered = hoveredApplyAll_ >= 0;
                g.setColour(hovered ? MixCoachTheme::accent() : MixCoachTheme::accentDim());
                g.fillRoundedRectangle(applyAllBounds_.toFloat(), 4.0f);
                g.setColour(MixCoachTheme::textBright());
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
                g.drawText("Aplicar todas las sugerencias", applyAllBounds_, juce::Justification::centred);
            }
        }
    }

    void GainStagingPanel::mouseMove(const juce::MouseEvent& e)
    {
        int prevHover = hoveredRow_;
        int prevApplyAll = hoveredApplyAll_;
        hoveredRow_ = -1;
        hoveredApplyAll_ = -1;

        if (applyAllBounds_.contains(e.getPosition())) {
            hoveredApplyAll_ = 0;
        } else {
            for (int i = 0; i < (int)rows_.size(); ++i) {
                int y = kHeaderHeight + kPadding + i * (kRowHeight + kGap);
                auto rowBounds = juce::Rectangle<int>(kPadding, y, getWidth() - kPadding * 2, kRowHeight);
                if (rowBounds.contains(e.getPosition())) {
                    hoveredRow_ = i;
                    break;
                }
            }
        }

        if (prevHover != hoveredRow_ || prevApplyAll != hoveredApplyAll_)
            repaint();
    }

    void GainStagingPanel::mouseDown(const juce::MouseEvent& e)
    {
        // Check "Apply All" button
        if (applyAllBounds_.contains(e.getPosition())) {
            if (onApplyAll) onApplyAll();
            return;
        }

        // Check individual apply buttons
        for (int i = 0; i < (int)rows_.size() && i < (int)applyBtnBounds_.size(); ++i) {
            if (applyBtnBounds_[i].contains(e.getPosition()) && !rows_[i].isApplied) {
                if (onApplyGain) onApplyGain(rows_[i].slotIndex, rows_[i].suggestedDeltaDb);
                // Mark as applied
                rows_[i].isApplied = true;
                repaint();
                return;
            }
        }
    }

} // namespace mixcoach
