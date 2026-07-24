#include "SpacePanel.h"
#include <cmath>

namespace mixcoach {

    SpacePanel::SpacePanel()
    {
        setOpaque(false);
        setVisible(false);
    }

    void SpacePanel::setTrackData(const std::vector<SpaceTrackRow>& rows)
    {
        rows_ = rows;
        applyBtnBounds_.resize(rows.size());
        resized();
        repaint();
    }

    void SpacePanel::clear()
    {
        rows_.clear();
        applyBtnBounds_.clear();
        repaint();
    }

    void SpacePanel::resized()
    {
        auto bounds = getLocalBounds().reduced(kPadding, kPadding);
        int totalContentH = kHeaderHeight + static_cast<int>(rows_.size()) * (kRowHeight + kGap);
        int applyAllY = juce::jmax(bounds.getY() + totalContentH + 8, bounds.getBottom() - 30);
        applyAllBounds_ = juce::Rectangle<int>(bounds.getCentreX() - 60, applyAllY, 120, 24);

        for (int i = 0; i < (int)rows_.size(); ++i) {
            int rowY = bounds.getY() + kHeaderHeight + i * (kRowHeight + kGap);
            int btnX = bounds.getRight() - kApplyW - 4;
            int btnY = rowY + (kRowHeight - 20) / 2;
            applyBtnBounds_[i] = juce::Rectangle<int>(btnX, btnY, kApplyW, 20);
        }
    }

    void SpacePanel::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().reduced(kPadding, kPadding);
        if (bounds.isEmpty()) return;

        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        // Header
        {
            auto header = bounds.removeFromTop(kHeaderHeight);
            g.setColour(MixCoachTheme::accentGlow());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSectionHeader)).boldened());
            g.drawText("ESPACIO — Reverb y profundidad", header.reduced(4, 0),
                       juce::Justification::centredLeft);

            auto labelArea = header.withTop(header.getBottom() - 14).withHeight(14);
            int x = labelArea.getX() + 4;
            g.setColour(MixCoachTheme::textDim());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
            g.drawText("Pista",   juce::Rectangle<int>(x, labelArea.getY(), kNameWidth, 14), juce::Justification::centredLeft); x += kNameWidth;
            g.drawText("Corr",    juce::Rectangle<int>(x, labelArea.getY(), kCorrBarW, 14), juce::Justification::centredLeft); x += kCorrBarW;
            g.drawText("Ancho",   juce::Rectangle<int>(x, labelArea.getY(), kWidthBarW, 14), juce::Justification::centredLeft); x += kWidthBarW;
            g.drawText("Pre",     juce::Rectangle<int>(x, labelArea.getY(), kPreDelayW, 14), juce::Justification::centredLeft); x += kPreDelayW;
            g.drawText("Decay",   juce::Rectangle<int>(x, labelArea.getY(), kDecayW, 14), juce::Justification::centredLeft); x += kDecayW;
            g.drawText("Mix",     juce::Rectangle<int>(x, labelArea.getY(), kMixW, 14), juce::Justification::centredLeft); x += kMixW;
            g.drawText("Tipo",    juce::Rectangle<int>(x, labelArea.getY(), kTypeW, 14), juce::Justification::centredLeft);
        }

        if (rows_.empty()) {
            g.setColour(MixCoachTheme::textMuted());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)));
            g.drawText("La mezcla est\\u00E1 seca. Sugerencias de espacio aparecer\\u00E1n aqu\\u00ED.",
                       bounds.reduced(8), juce::Justification::centred);
            return;
        }

        int y = bounds.getY() + kHeaderHeight;
        for (int i = 0; i < (int)rows_.size(); ++i) {
            const auto& row = rows_[i];
            auto rowBounds = juce::Rectangle<int>(bounds.getX(), y, bounds.getWidth(), kRowHeight);

            if (i == hoveredRow_) {
                g.setColour(MixCoachTheme::accentBg().withAlpha(0.08f));
                g.fillRoundedRectangle(rowBounds.toFloat(), 2.0f);
            } else if (i % 2 == 0) {
                g.setColour(juce::Colours::white.withAlpha(0.02f));
                g.fillRoundedRectangle(rowBounds.toFloat(), 2.0f);
            }

            int x = rowBounds.getX() + 4;

            // Track name
            g.setColour(MixCoachTheme::textBright());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
            g.drawText(row.trackName.substring(0, 9),
                       juce::Rectangle<int>(x, rowBounds.getY(), kNameWidth, kRowHeight),
                       juce::Justification::centredLeft); x += kNameWidth;

            // Correlation bar
            auto corrBar = juce::Rectangle<int>(x, rowBounds.getY() + 8, kCorrBarW, kRowHeight - 16);
            float corrNorm = juce::jmap(juce::jlimit(-1.0f, 1.0f, row.correlation), -1.0f, 1.0f, 0.0f, 1.0f);
            juce::Colour corrCol = (row.correlation < 0.3f) ? MixCoachTheme::error()
                                 : (row.correlation < 0.7f) ? MixCoachTheme::warning()
                                 : MixCoachTheme::success();
            g.setColour(MixCoachTheme::bgDarker());
            g.fillRoundedRectangle(corrBar.toFloat(), 2.0f);
            g.setColour(corrCol);
            auto fillBar = corrBar.withWidth((int)(corrBar.getWidth() * corrNorm));
            if (fillBar.getWidth() > 2) g.fillRoundedRectangle(fillBar.toFloat(), 2.0f);
            x += kCorrBarW;

            // Width indicator
            auto wBar = juce::Rectangle<int>(x, rowBounds.getY() + 8, kWidthBarW, kRowHeight - 16);
            float wNorm = juce::jlimit(0.0f, 1.0f, row.stereoWidth);
            juce::Colour wCol = (row.stereoWidth < 0.2f) ? MixCoachTheme::textMuted()
                              : (row.stereoWidth < 0.4f) ? MixCoachTheme::info()
                              : (row.stereoWidth < 0.7f) ? MixCoachTheme::success()
                              : MixCoachTheme::warning();
            g.setColour(MixCoachTheme::bgDarker());
            g.fillRoundedRectangle(wBar.toFloat(), 2.0f);
            g.setColour(wCol);
            auto wFill = wBar.withWidth((int)(wBar.getWidth() * wNorm));
            if (wFill.getWidth() > 2) g.fillRoundedRectangle(wFill.toFloat(), 2.0f);
            x += kWidthBarW;

            // Pre-delay
            g.setColour(MixCoachTheme::textSecondary());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
            g.drawText(juce::String((int)row.suggestedPreDelayMs) + "ms",
                       juce::Rectangle<int>(x, rowBounds.getY(), kPreDelayW, kRowHeight),
                       juce::Justification::centredLeft); x += kPreDelayW;

            // Decay
            g.drawText(juce::String(row.suggestedDecaySec, 1) + "s",
                       juce::Rectangle<int>(x, rowBounds.getY(), kDecayW, kRowHeight),
                       juce::Justification::centredLeft); x += kDecayW;

            // Mix %
            g.drawText(juce::String((int)row.suggestedMixPct) + "%",
                       juce::Rectangle<int>(x, rowBounds.getY(), kMixW, kRowHeight),
                       juce::Justification::centredLeft); x += kMixW;

            // Reverb type
            g.setColour(MixCoachTheme::accentGlow());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
            g.drawText(row.reverbType,
                       juce::Rectangle<int>(x, rowBounds.getY(), kTypeW, kRowHeight),
                       juce::Justification::centredLeft);

            // Apply button
            if (i < (int)applyBtnBounds_.size() && !row.isApplied) {
                auto btn = applyBtnBounds_[i];
                bool hovered = (hoveredRow_ == i && btn.contains(getMouseXYRelative()));
                g.setColour(hovered ? MixCoachTheme::accent() : MixCoachTheme::accentDim());
                g.fillRoundedRectangle(btn.toFloat(), 3.0f);
                g.setColour(MixCoachTheme::textBright());
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
                g.drawText("Aplicar", btn, juce::Justification::centred);
            }

            y += kRowHeight + kGap;
        }

        // Apply All button
        if (rows_.size() > 1) {
            bool allApplied = true;
            for (const auto& r : rows_) if (!r.isApplied) { allApplied = false; break; }
            if (!allApplied) {
                bool hovered = hoveredApplyAll_ >= 0;
                g.setColour(hovered ? MixCoachTheme::accent() : MixCoachTheme::accentDim());
                g.fillRoundedRectangle(applyAllBounds_.toFloat(), 4.0f);
                g.setColour(MixCoachTheme::textBright());
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
                g.drawText("Aplicar todas", applyAllBounds_, juce::Justification::centred);
            }
        }
    }

    void SpacePanel::mouseMove(const juce::MouseEvent& e)
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
                if (rowBounds.contains(e.getPosition())) { hoveredRow_ = i; break; }
            }
        }
        if (prevHover != hoveredRow_ || prevApplyAll != hoveredApplyAll_) repaint();
    }

    void SpacePanel::mouseDown(const juce::MouseEvent& e)
    {
        if (applyAllBounds_.contains(e.getPosition())) {
            if (onApplyAll) onApplyAll();
            return;
        }
        for (int i = 0; i < (int)rows_.size() && i < (int)applyBtnBounds_.size(); ++i) {
            if (applyBtnBounds_[i].contains(e.getPosition()) && !rows_[i].isApplied) {
                if (onApplyReverb)
                    onApplyReverb(rows_[i].slotIndex, rows_[i].suggestedPreDelayMs,
                                  rows_[i].suggestedDecaySec, rows_[i].suggestedMixPct,
                                  rows_[i].reverbType);
                rows_[i].isApplied = true;
                repaint();
                return;
            }
        }
    }

} // namespace mixcoach
