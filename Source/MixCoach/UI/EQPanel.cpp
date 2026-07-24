#include "EQPanel.h"
#include <cmath>
#include <algorithm>

namespace mixcoach {

    EQPanel::EQPanel()
    {
        setOpaque(false);
        setSize(300, 200); // default size evita 0x0 si se agrega sin bounds
        setVisible(false);
    }

    void EQPanel::setTrackData(const EQTrackData& data)
    {
        data_ = data;
        hasData_ = true;
        hasSecondTrack_ = false;
        resized();
        repaint();
    }

    void EQPanel::setTrackPair(const EQTrackData& primary, const EQTrackData& secondary)
    {
        data_ = primary;
        secondTrackData_ = secondary;
        hasData_ = true;
        hasSecondTrack_ = true;
        resized();
        repaint();
    }

    void EQPanel::clear()
    {
        hasData_ = false;
        data_ = EQTrackData{};
        frequencyHz_ = 1000.0f;
        q_ = 1.0f;
        gainDb_ = 0.0f;
        repaint();
    }

    void EQPanel::resized()
    {
        auto area = getLocalBounds().reduced(kPadding, kPadding);
        int y = area.getY() + kHeaderH;

        // Spectral overlay
        y += kSpecH + 4;

        // Region labels below spectrum
        y += kRegionLabelH + 8;

        // Frequency slider
        freqSliderBounds_ = juce::Rectangle<int>(area.getX(), y, area.getWidth(), kSliderH);
        y += kSliderH + 4;

        // Q slider
        qSliderBounds_ = juce::Rectangle<int>(area.getX(), y, area.getWidth(), kSliderH);
        y += kSliderH + 4;

        // Gain slider
        gainSliderBounds_ = juce::Rectangle<int>(area.getX(), y, area.getWidth(), kSliderH);
        y += kSliderH + 8;

        // Apply button
        applyBtnBounds_ = juce::Rectangle<int>(area.getCentreX() - 50, y, 100, kApplyH);
    }

    void EQPanel::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().reduced(kPadding, kPadding);
        if (bounds.isEmpty()) return;

        // ─── Panel background ─────────────────────────────────────────────
        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        auto area = getLocalBounds().reduced(kPadding, kPadding);
        int y = area.getY();

        // ─── Header ───────────────────────────────────────────────────────
        {
            auto header = juce::Rectangle<int>(area.getX(), y, area.getWidth(), kHeaderH);
            g.setColour(MixCoachTheme::accentGlow());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSectionHeader)).boldened());
            juce::String headerText = "EQ — Ajuste espectral";
            if (hasData_ && data_.trackName.isNotEmpty())
                headerText += ": " + data_.trackName;
            g.drawText(headerText, header.reduced(4, 0), juce::Justification::centredLeft);
            y += kHeaderH;
        }

        if (!hasData_) {
            g.setColour(MixCoachTheme::textMuted());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)));
            g.drawText("Selecciona una pista o espera a que el Coach detecte un problema espectral.",
                       juce::Rectangle<int>(area.getX(), y + 40, area.getWidth(), 40),
                       juce::Justification::centred);
            return;
        }

        // ─── Spectral overlay ─────────────────────────────────────────────
        auto specArea = juce::Rectangle<int>(area.getX(), y, area.getWidth(), kSpecH);
        drawSpectralOverlay(g, specArea);
        y += kSpecH + 4;

        // ─── Region labels ────────────────────────────────────────────────
        drawRegionLabel(g, juce::Rectangle<int>(area.getX(), y, area.getWidth(), kRegionLabelH));
        y += kRegionLabelH + 8;

        // ─── Frequency slider ──────────────────────────────────────────────
        drawSlider(g, freqSliderBounds_, frequencyHz_, kFreqMin, kFreqMax, "Frecuencia", "Hz");
        y += kSliderH + 4;

        // ─── Q slider ──────────────────────────────────────────────────────
        drawSlider(g, qSliderBounds_, q_, kQMin, kQMax, "Q", "");
        y += kSliderH + 4;

        // ─── Gain slider ───────────────────────────────────────────────────
        drawSlider(g, gainSliderBounds_, gainDb_, kGainMin, kGainMax, "Ganancia", "dB");
        y += kSliderH + 8;

        // ─── Apply button ──────────────────────────────────────────────────
        auto btn = applyBtnBounds_;
        bool hovered = (hoverTarget_ == DragTarget::ApplyBtn);
        g.setColour(hovered ? MixCoachTheme::accent() : MixCoachTheme::accentDim());
        g.fillRoundedRectangle(btn.toFloat(), 4.0f);
        g.setColour(MixCoachTheme::textBright());
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
        g.drawText("Aplicar EQ", btn, juce::Justification::centred);
    }

    void EQPanel::drawSpectralOverlay(juce::Graphics& g, juce::Rectangle<int> area)
    {
        if (area.isEmpty()) return;

        int w = area.getWidth();
        int h = area.getHeight();
        int x0 = area.getX();
        int y0 = area.getY();

        // ─── Grid background ──────────────────────────────────────────────
        g.setColour(MixCoachTheme::bgDarker());
        g.fillRoundedRectangle(area.toFloat(), 4.0f);

        // Grid lines (vertical, 6 regions)
        float regionW = (float)w / 6.0f;
        g.setColour(juce::Colour(0xFF1E293B).withAlpha(0.3f));
        for (int r = 0; r <= 6; ++r) {
            float lx = x0 + r * regionW;
            g.drawVerticalLine((int)lx, (float)y0, (float)(y0 + h));
        }

        // Horizontal grid: -24, -12, 0 dBFS
        g.setColour(juce::Colour(0xFF1E293B).withAlpha(0.2f));
        int zeroY = y0 + h / 2;
        g.drawHorizontalLine(zeroY, (float)x0, (float)(x0 + w));
        g.setColour(juce::Colour(0xFF1E293B).withAlpha(0.1f));
        g.drawHorizontalLine(y0 + h / 4, (float)x0, (float)(x0 + w));
        g.drawHorizontalLine(y0 + h * 3 / 4, (float)x0, (float)(x0 + w));

        // Helper lambda to draw a spectral curve
        auto drawCurve = [&](const EQTrackData& trackData, juce::Colour colour, float thickness, bool filled)
        {
            juce::Path path;
            bool first = true;
            for (int r = 0; r < 6; ++r) {
                float val = juce::jlimit(-24.0f, 0.0f, trackData.currentEnergy[r]);
                float norm = juce::jmap(val, -24.0f, 0.0f, 0.0f, 1.0f);
                float px = x0 + (r + 0.5f) * regionW;
                float py = y0 + h - h * norm;
                if (first) { path.startNewSubPath(px, py); first = false; }
                else path.lineTo(px, py);
            }
            g.setColour(colour);
            g.strokePath(path, juce::PathStrokeType(thickness));

            // Region dots
            for (int r = 0; r < 6; ++r) {
                float val = juce::jlimit(-24.0f, 0.0f, trackData.currentEnergy[r]);
                float norm = juce::jmap(val, -24.0f, 0.0f, 0.0f, 1.0f);
                float px = x0 + (r + 0.5f) * regionW;
                float py = y0 + h - h * norm;
                g.fillEllipse(px - 2.5f, py - 2.5f, 5.0f, 5.0f);
            }

            // Fill under curve
            if (filled) {
                juce::Path fillPath(path);
                fillPath.lineTo(x0 + w, y0 + h);
                fillPath.lineTo(x0, y0 + h);
                fillPath.closeSubPath();
                g.setColour(colour.withAlpha(0.07f));
                g.fillPath(fillPath);
            }
        };

        // ─── Primary track curve (solid, main color) ──────────────────────
        juce::Colour mainCol = MixCoachTheme::accent();
        drawCurve(data_, mainCol, 2.0f, true);

        // ─── Second track curve (dashed/pink, for masking visual) ──────────
        if (hasSecondTrack_) {
            juce::Colour secondCol = juce::Colour(0xFFEC4899); // Pink-500
            // Draw dashed effect by drawing segments
            juce::Path secondPath;
            bool first = true;
            for (int r = 0; r < 6; ++r) {
                float val = juce::jlimit(-24.0f, 0.0f, secondTrackData_.currentEnergy[r]);
                float norm = juce::jmap(val, -24.0f, 0.0f, 0.0f, 1.0f);
                float px = x0 + (r + 0.5f) * regionW;
                float py = y0 + h - h * norm;
                if (first) { secondPath.startNewSubPath(px, py); first = false; }
                else secondPath.lineTo(px, py);
            }
            g.setColour(secondCol.withAlpha(0.6f));
            // Draw dashed: stroke with a dash pattern
            juce::PathStrokeType dashStroke(2.0f);
            const float dashLengths[] = {4.0f, 4.0f};
            dashStroke.createDashedStroke(secondPath, secondPath, dashLengths, 2);
            g.strokePath(secondPath, juce::PathStrokeType(2.0f));

            // Region dots (diamond shape for secondary)
            for (int r = 0; r < 6; ++r) {
                float val = juce::jlimit(-24.0f, 0.0f, secondTrackData_.currentEnergy[r]);
                float norm = juce::jmap(val, -24.0f, 0.0f, 0.0f, 1.0f);
                float px = x0 + (r + 0.5f) * regionW;
                float py = y0 + h - h * norm;
                g.setColour(secondCol);
                // Diamond shape
                juce::Path diamond;
                diamond.startNewSubPath(px, py - 3.0f);
                diamond.lineTo(px + 3.0f, py);
                diamond.lineTo(px, py + 3.0f);
                diamond.lineTo(px - 3.0f, py);
                diamond.closeSubPath();
                g.fillPath(diamond);
            }
        }

        // ─── Target curve (dashed line) ───────────────────────────────────
        if (data_.hasTarget) {
            juce::Path targetPath;
            bool first = true;
            for (int r = 0; r < 6; ++r) {
                float val = juce::jlimit(-24.0f, 0.0f, data_.targetEnergy[r]);
                float norm = juce::jmap(val, -24.0f, 0.0f, 0.0f, 1.0f);
                float px = x0 + (r + 0.5f) * regionW;
                float py = y0 + h - h * norm;
                if (first) { targetPath.startNewSubPath(px, py); first = false; }
                else targetPath.lineTo(px, py);
            }
            g.setColour(MixCoachTheme::accentNeon().withAlpha(0.5f));
            g.strokePath(targetPath, juce::PathStrokeType(1.5f));
            for (int r = 0; r < 6; ++r) {
                float val = juce::jlimit(-24.0f, 0.0f, data_.targetEnergy[r]);
                float norm = juce::jmap(val, -24.0f, 0.0f, 0.0f, 1.0f);
                float px = x0 + (r + 0.5f) * regionW;
                float py = y0 + h - h * norm;
                g.fillEllipse(px - 2.0f, py - 2.0f, 4.0f, 4.0f);
            }
        }

        // ─── Legend ───────────────────────────────────────────────────────
        int legendY = y0 + h - 14;
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
        int legendX = x0 + 4;

        // Primary track dot
        g.setColour(MixCoachTheme::accent());
        g.fillEllipse((float)legendX, (float)legendY, 8.0f, 8.0f);
        g.setColour(MixCoachTheme::textDim());
        juce::String primaryName = data_.trackName.isNotEmpty() ? data_.trackName : "Track 1";
        g.drawText(primaryName, legendX + 12, legendY, 80, 12, juce::Justification::centredLeft);
        legendX += 90;

        // Second track diamond
        if (hasSecondTrack_) {
            juce::Colour secondCol = juce::Colour(0xFFEC4899);
            g.setColour(secondCol);
            juce::Path diamond;
            diamond.startNewSubPath((float)(legendX + 4), (float)(legendY));
            diamond.lineTo((float)(legendX + 8), (float)(legendY + 4));
            diamond.lineTo((float)(legendX + 4), (float)(legendY + 8));
            diamond.lineTo((float)legendX, (float)(legendY + 4));
            diamond.closeSubPath();
            g.fillPath(diamond);
            g.setColour(MixCoachTheme::textDim());
            juce::String secName = secondTrackData_.trackName.isNotEmpty() ? secondTrackData_.trackName : "Track 2";
            g.drawText(secName, legendX + 12, legendY, 80, 12, juce::Justification::centredLeft);
            legendX += 90;
        }

        // Target dot
        if (data_.hasTarget) {
            g.setColour(MixCoachTheme::accentNeon());
            g.fillEllipse((float)legendX, (float)legendY, 8.0f, 8.0f);
            g.setColour(MixCoachTheme::textDim());
            g.drawText("Target", legendX + 12, legendY, 50, 12, juce::Justification::centredLeft);
        }

        // ─── Masking zone indicator (highlighted region where tracks overlap) ─
        if (hasSecondTrack_) {
            // Find the region with largest overlap (smallest energy gap between tracks)
            float minGap = 999.0f;
            int overlapRegion = -1;
            for (int r = 0; r < 6; ++r) {
                float gap = std::abs(data_.currentEnergy[r] - secondTrackData_.currentEnergy[r]);
                if (gap < minGap) {
                    minGap = gap;
                    overlapRegion = r;
                }
            }
            // If gap is small (< 6dB), highlight that region
            if (overlapRegion >= 0 && minGap < 6.0f) {
                float barX = x0 + overlapRegion * regionW;
                float barW = regionW;
                g.setColour(juce::Colour(0xFFF59E0B).withAlpha(0.12f)); // Amber glow
                g.fillRoundedRectangle(barX, (float)y0, barW, (float)h, 2.0f);
                // Label
                g.setColour(juce::Colour(0xFFF59E0B).withAlpha(0.7f));
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
                g.drawText("[WARN] Enmascaramiento",
                           juce::Rectangle<int>((int)barX, y0 + 4, (int)barW, 14),
                           juce::Justification::centred);
            }
        }

        // ─── Region colors (bar at bottom) ────────────────────────────────
        for (int r = 0; r < 6; ++r) {
            auto bar = juce::Rectangle<float>(x0 + r * regionW, y0 + h - 3, regionW, 3.0f);
            g.setColour(MixCoachTheme::specColour(r).withAlpha(0.5f));
            g.fillRect(bar);
        }
    }

    void EQPanel::drawRegionLabel(juce::Graphics& g, juce::Rectangle<int> area)
    {
        float regionW = (float)area.getWidth() / 6.0f;
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
        for (int r = 0; r < 6; ++r) {
            g.setColour(MixCoachTheme::specColour(r));
            auto labelArea = juce::Rectangle<int>(area.getX() + (int)(r * regionW), area.getY(),
                                                   (int)regionW, area.getHeight());
            g.drawText(kRegionNames[r], labelArea, juce::Justification::centred);
        }
    }

    void EQPanel::drawSlider(juce::Graphics& g, juce::Rectangle<int> bounds,
                              float value, float min, float max,
                              const char* label, const char* unit)
    {
        int x = bounds.getX();
        int y = bounds.getY() + 2;
        int w = bounds.getWidth();
        int h = bounds.getHeight() - 4;

        // Label
        g.setColour(MixCoachTheme::textDim());
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
        g.drawText(juce::String(label), juce::Rectangle<int>(x, y, kSliderLabelW, h),
                   juce::Justification::centredLeft);

        // Track background
        int trackX = x + kSliderLabelW;
        int trackW = w - kSliderLabelW - 60;
        auto trackArea = juce::Rectangle<int>(trackX, y + (h - kSliderTrackH) / 2, trackW, kSliderTrackH);
        g.setColour(MixCoachTheme::bgDarker());
        g.fillRoundedRectangle(trackArea.toFloat(), kSliderTrackH / 2.0f);

        // Fill
        float norm = juce::jmap(value, min, max, 0.0f, 1.0f);
        auto fillArea = trackArea.withWidth((int)(trackArea.getWidth() * norm));
        if (fillArea.getWidth() > 2) {
            g.setColour(MixCoachTheme::accent().withAlpha(0.6f));
            g.fillRoundedRectangle(fillArea.toFloat(), kSliderTrackH / 2.0f);
        }

        // Thumb
        int thumbX = trackArea.getX() + (int)(trackArea.getWidth() * norm);
        int thumbR = kSliderTrackH + 2;
        g.setColour(MixCoachTheme::accentGlow());
        g.fillEllipse((float)(thumbX - thumbR), (float)(trackArea.getCentreY() - thumbR),
                      (float)(thumbR * 2), (float)(thumbR * 2));

        // Value display
        juce::String valStr;
        juce::String unitStr(unit);
        if (unitStr == "Hz") {
            if (value >= 1000.0f)
                valStr = juce::String(value / 1000.0f, 1) + "k";
            else
                valStr = juce::String((int)value);
        } else {
            valStr = juce::String(value, 1);
        }
        if (unit[0] != '\\0') valStr += " " + juce::String(unit);
        g.setColour(MixCoachTheme::textBright());
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
        g.drawText(valStr, juce::Rectangle<int>(trackX + trackW + 4, y, 56, h),
                   juce::Justification::centredLeft);
    }

    EQPanel::DragTarget EQPanel::hitTestSlider(juce::Point<int> pos) const
    {
        if (!hasData_) return DragTarget::None;
        if (applyBtnBounds_.contains(pos)) return DragTarget::ApplyBtn;
        if (freqSliderBounds_.contains(pos)) return DragTarget::FreqSlider;
        if (qSliderBounds_.contains(pos)) return DragTarget::QSlider;
        if (gainSliderBounds_.contains(pos)) return DragTarget::GainSlider;
        return DragTarget::None;
    }

    void EQPanel::mouseMove(const juce::MouseEvent& e)
    {
        DragTarget newHover = hitTestSlider(e.getPosition());
        if (newHover != hoverTarget_) {
            hoverTarget_ = newHover;
            repaint();
        }
    }

    void EQPanel::mouseDown(const juce::MouseEvent& e)
    {
        dragTarget_ = hitTestSlider(e.getPosition());
        if (dragTarget_ == DragTarget::ApplyBtn) {
            if (onApplyEQ && hasData_) {
                onApplyEQ(data_.slotIndex, frequencyHz_, q_, gainDb_);
            }
            dragTarget_ = DragTarget::None;
            return;
        }

        if (dragTarget_ != DragTarget::None) {
            dragStartPos_ = e.getPosition();
            switch (dragTarget_) {
                case DragTarget::FreqSlider: dragStartValue_ = frequencyHz_; break;
                case DragTarget::QSlider:    dragStartValue_ = q_; break;
                case DragTarget::GainSlider: dragStartValue_ = gainDb_; break;
                default: break;
            }
        }
    }

    void EQPanel::mouseDrag(const juce::MouseEvent& e)
    {
        if (dragTarget_ == DragTarget::None) return;

        auto sliderBounds = [this]() -> juce::Rectangle<int> {
            switch (dragTarget_) {
                case DragTarget::FreqSlider: return freqSliderBounds_;
                case DragTarget::QSlider:    return qSliderBounds_;
                case DragTarget::GainSlider: return gainSliderBounds_;
                default: return {};
            }
        }();

        int trackX = sliderBounds.getX() + kSliderLabelW;
        int trackW = sliderBounds.getWidth() - kSliderLabelW - 60;
        if (trackW <= 0) return;

        float dx = (float)(e.getPosition().x - dragStartPos_.x);
        float normDelta = dx / (float)trackW;

        switch (dragTarget_) {
            case DragTarget::FreqSlider: {
                // Logarithmic mapping for frequency
                float logMin = std::log10(kFreqMin);
                float logMax = std::log10(kFreqMax);
                float logVal = std::log10(dragStartValue_) + normDelta * (logMax - logMin);
                frequencyHz_ = juce::jlimit(kFreqMin, kFreqMax, std::pow(10.0f, logVal));
                break;
            }
            case DragTarget::QSlider: {
                float range = kQMax - kQMin;
                q_ = juce::jlimit(kQMin, kQMax, dragStartValue_ + normDelta * range);
                break;
            }
            case DragTarget::GainSlider: {
                float range = kGainMax - kGainMin;
                gainDb_ = juce::jlimit(kGainMin, kGainMax, dragStartValue_ + normDelta * range);
                break;
            }
            default: break;
        }

        repaint();
    }

    void EQPanel::mouseUp(const juce::MouseEvent& e)
    {
        juce::ignoreUnused(e);
        dragTarget_ = DragTarget::None;
    }

} // namespace mixcoach
