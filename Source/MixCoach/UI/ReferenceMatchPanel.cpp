#include "ReferenceMatchPanel.h"
#include <cmath>

namespace mixcoach {

    ReferenceMatchPanel::ReferenceMatchPanel() :
        mixSmooth_{{SmoothValue(-60.0f, 50.0f, 300.0f),
                    SmoothValue(-60.0f, 50.0f, 300.0f),
                    SmoothValue(-60.0f, 50.0f, 300.0f),
                    SmoothValue(-60.0f, 50.0f, 300.0f),
                    SmoothValue(-60.0f, 50.0f, 300.0f),
                    SmoothValue(-60.0f, 50.0f, 300.0f)}},
        refSmooth_{{SmoothValue(-60.0f, 50.0f, 300.0f),
                    SmoothValue(-60.0f, 50.0f, 300.0f),
                    SmoothValue(-60.0f, 50.0f, 300.0f),
                    SmoothValue(-60.0f, 50.0f, 300.0f),
                    SmoothValue(-60.0f, 50.0f, 300.0f),
                    SmoothValue(-60.0f, 50.0f, 300.0f)}},
        lufsSmooth_(0.0f, 80.0f, 200.0f),
        scoreSmooth_(0.0f, 100.0f, 400.0f)
    {
        setOpaque(false);
        setMouseCursor(juce::MouseCursor::NormalCursor);
        startTimerHz(60);
    }

    ReferenceMatchPanel::~ReferenceMatchPanel()
    {
        stopTimer();
    }

    void ReferenceMatchPanel::visibilityChanged()
    {
        if (isShowing()) startTimerHz(60);
        else
            stopTimer();
    }

    void ReferenceMatchPanel::timerCallback()
    {
        bool needsRepaint = false;

        // Avanzar SmoothValues para las regiones
        for (int r = 0; r < kNumRegions; ++r) {
            if (mixSmooth_[r].advance()) needsRepaint = true;
            if (refSmooth_[r].advance()) needsRepaint = true;
        }
        if (lufsSmooth_.advance()) needsRepaint = true;
        if (scoreSmooth_.advance()) needsRepaint = true;

        if (needsRepaint) repaint();
    }

    void ReferenceMatchPanel::updateMatchData(const DifferenceProfile& data)
    {
        data_       = data;
        matchScore_ = computeMatchScore();

        // Actualizar targets de SmoothValue
        bool hasChanges = false;

        for (int r = 0; r < kNumRegions; ++r) {
            float mixTarget = juce::jlimit(-60.0f, 0.0f, data.mixRegionEnergy[r]);
            if (std::abs(mixTarget - lastMixEnergy_[r]) > 0.5f) {
                mixSmooth_[r].setTargetValue(mixTarget);
                lastMixEnergy_[r] = mixTarget;
                hasChanges        = true;
            }

            float refTarget = juce::jlimit(-60.0f, 0.0f, data.refRegionEnergy[r]);
            if (std::abs(refTarget - lastRefEnergy_[r]) > 0.5f) {
                refSmooth_[r].setTargetValue(refTarget);
                lastRefEnergy_[r] = refTarget;
                hasChanges        = true;
            }
        }

        if (std::abs(data.mixIntegratedLUFS - lastLufsMix_) > 0.3f
            || std::abs(data.refIntegratedLUFS - lastLufsRef_) > 0.3f) {
            lufsSmooth_.setTargetValue(data.mixIntegratedLUFS - data.refIntegratedLUFS);
            lastLufsMix_ = data.mixIntegratedLUFS;
            lastLufsRef_ = data.refIntegratedLUFS;
            hasChanges   = true;
        }

        scoreSmooth_.setTargetValue((float)matchScore_);

        if (hasChanges) repaint();
    }

    int ReferenceMatchPanel::computeMatchScore() const noexcept
    {
        if (!data_.valid) return 0;

        float spectralScore = 0.0f;
        for (int b = 0; b < kNumRegions; ++b) {
            float diff      = std::abs(data_.mixRegionEnergy[b] - data_.refRegionEnergy[b]);
            float bandScore = 1.0f - juce::jmin(diff / 12.0f, 1.0f);
            spectralScore += bandScore * 15.0f;
        }

        float lufsDiff  = std::abs(data_.mixIntegratedLUFS - data_.refIntegratedLUFS);
        float lufsScore = 10.0f * (1.0f - juce::jmin(lufsDiff / 6.0f, 1.0f));

        return juce::jmin(100, (int)(spectralScore + lufsScore + 0.5f));
    }

    void ReferenceMatchPanel::paint(juce::Graphics& g)
    {
        if (!data_.valid) {
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.35f));
            g.setFont(juce::Font(juce::FontOptions(9.0f)));
            g.drawText("Carga una referencia para ver el matching espectral",
                       getLocalBounds().reduced(4),
                       juce::Justification::centred);
            return;
        }

        auto area         = getLocalBounds().reduced(4, 2);
        const int headerH = 16;

        // ─── Header: nombre de referencia + match score ───────────────────
        {
            auto header = area.removeFromTop(headerH);
            g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
            g.setColour(MixCoachTheme::textPrimary());
            g.drawText(
                "Matching: " + data_.referenceName, header.withTrimmedRight(54), juce::Justification::centredLeft);

            drawMatchScore(g, header.removeFromRight(50));
        }

        area.removeFromTop(2);

        // ─── Split: region bars (left 58%) | LUFS + metrics (right 38%) │ delta row (4%) ────
        int barsW        = area.getWidth() * 58 / 100;
        auto barsArea    = area.removeFromLeft(barsW);
        auto metricsArea = area.reduced(4, 0);

        // Delta row at the bottom of bars area
        auto deltaArea = barsArea.removeFromBottom(14);
        barsArea.removeFromBottom(2);

        drawRegionBars(g, barsArea);
        drawDeltaRow(g, deltaArea);
        drawMetricsRow(g, metricsArea);
    }

    void ReferenceMatchPanel::drawRegionBars(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        static const char* kRegionLabels[]         = {"Sub", "Bass", "LoMid", "HiMid", "Pres", "Air"};
        static const juce::Colour kRegionColours[] = {
            MixCoachTheme::specSub(), // Sub - violeta
            MixCoachTheme::specAir(), // Bass - rojo
            MixCoachTheme::specPres(), // LoMid - naranja
            MixCoachTheme::specLoMid(), // HiMid - teal
            MixCoachTheme::specBass(), // Pres - azul
            MixCoachTheme::specSub()  // Air - lavanda
        };

        // Layout: 6 columns, each column has mix bar (left half) + ref bar (right half)
        // with a 1px gap between columns
        int colGap   = 2;
        int numBars  = kNumRegions;
        int totalGap = colGap * (numBars - 1);
        int colW     = (bounds.getWidth() - totalGap) / numBars;
        int barGap   = 2; // gap between mix and ref bars within a column
        int barW     = (colW - barGap) / 2;
        int labelH   = 12;
        int barAreaH = bounds.getHeight() - labelH;

        // ─── Background for bar area ────────────────────────────────────────
        g.setColour(MixCoachTheme::bgDarker().withAlpha(0.15f));
        g.fillRoundedRectangle(bounds.toFloat().withTrimmedBottom((float)labelH), 3.0f);

        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)));

        for (int b = 0; b < numBars; ++b) {
            int colX = bounds.getX() + b * (colW + colGap);

            // ─── Mix bar (left half of column) ───────────────────────────────
            float mixNorm = juce::jmap(mixSmooth_[b].getCurrent(), -60.0f, 0.0f, 0.0f, 1.0f);
            mixNorm       = juce::jlimit(0.0f, 1.0f, mixNorm);
            int mixH      = juce::jmax(1, (int)(mixNorm * barAreaH));
            int mixY      = bounds.getY() + barAreaH - mixH;

            // Gradient fill for mix bar (lighter top, solid bottom)
            auto mixRect = juce::Rectangle<float>((float)colX, (float)mixY, (float)barW, (float)mixH);
            juce::ColourGradient mixGrad(kRegionColours[b].brighter(0.3f).withAlpha(0.9f),
                                         mixRect.getCentreX(),
                                         mixRect.getY(),
                                         kRegionColours[b].withAlpha(0.75f),
                                         mixRect.getCentreX(),
                                         mixRect.getBottom(),
                                         false);
            g.setGradientFill(mixGrad);
            g.fillRoundedRectangle(mixRect, 2.0f);

            // Mix bar shine
            auto mixShine = mixRect.withHeight(juce::jmin(3.0f, mixRect.getHeight() * 0.2f));
            g.setColour(juce::Colours::white.withAlpha(0.15f));
            g.fillRoundedRectangle(mixShine, 1.5f);

            // ─── Ref bar (right half of column) ──────────────────────────────
            float refNorm = juce::jmap(refSmooth_[b].getCurrent(), -60.0f, 0.0f, 0.0f, 1.0f);
            refNorm       = juce::jlimit(0.0f, 1.0f, refNorm);
            int refH      = juce::jmax(1, (int)(refNorm * barAreaH));
            int refY      = bounds.getY() + barAreaH - refH;
            int refX      = colX + barW + barGap;

            // Outline style for ref bar
            auto refRect = juce::Rectangle<float>((float)refX, (float)refY, (float)barW, (float)refH);

            // Fill with lower alpha
            g.setColour(kRegionColours[b].withAlpha(0.30f));
            g.fillRoundedRectangle(refRect, 2.0f);

            // Outline
            g.setColour(kRegionColours[b].withAlpha(0.55f));
            g.drawRoundedRectangle(refRect, 2.0f, 1.0f);

            // Ref "R" label inside bar (top-right corner)
            if (refH > 14) {
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizePico)).boldened());
                g.setColour(kRegionColours[b].withAlpha(0.7f));
                g.drawText(
                    "R", refRect.reduced(1, 1).removeFromTop(10).removeFromRight(10), juce::Justification::topRight);
            }

            // Mix "M" label
            if (mixH > 14) {
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizePico)).boldened());
                g.setColour(juce::Colours::white.withAlpha(0.35f));
                g.drawText(
                    "M", mixRect.reduced(1, 1).removeFromTop(10).removeFromLeft(10), juce::Justification::topLeft);
            }

            // ─── Column separator line ──────────────────────────────────────
            if (b < numBars - 1) {
                int sepX = colX + colW + colGap / 2;
                g.setColour(MixCoachTheme::border().withAlpha(0.08f));
                g.drawVerticalLine(sepX, (float)(bounds.getY() + 2), (float)(bounds.getBottom() - labelH - 2));
            }

            // ─── Label below column ─────────────────────────────────────────
            auto labelBounds = juce::Rectangle<int>(colX, bounds.getBottom() - labelH, colW, labelH);
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
            g.setColour(MixCoachTheme::textMuted());
            g.drawText(kRegionLabels[b], labelBounds, juce::Justification::centred);

            // ─── Difference text (small) ────────────────────────────────────
            if (data_.valid) {
                float delta = data_.mixRegionEnergy[b] - data_.refRegionEnergy[b];
                juce::String deltaStr;
                if (delta > 0.5f) deltaStr = "+" + juce::String(delta, 1);
                else if (delta < -0.5f)
                    deltaStr = juce::String(delta, 1);
                else
                    deltaStr = "0";

                auto diffBounds = labelBounds.translated(0, -2).withHeight(8);
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizePico)));
                juce::Colour diffCol;
                if (std::abs(delta) < 1.0f) diffCol = MixCoachTheme::success().withAlpha(0.5f);
                else if (std::abs(delta) < 3.0f)
                    diffCol = MixCoachTheme::warning().withAlpha(0.6f);
                else
                    diffCol = MixCoachTheme::error().withAlpha(0.7f);
                g.setColour(diffCol);
                g.drawText(deltaStr + "dB", diffBounds, juce::Justification::centred);
            }
        }

        // ─── Labels for M and R at the very bottom ──────────────────────────
        {
            int labelY = bounds.getBottom() + 2;
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizePico)));
            g.setColour(MixCoachTheme::textDim().withAlpha(0.4f));
            g.drawText("M = Mix  |  R = Ref",
                       juce::Rectangle<int>(bounds.getX(), labelY, bounds.getWidth(), 8),
                       juce::Justification::centred);
        }
    }

    void ReferenceMatchPanel::drawDeltaRow(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        // ─── Delta row: muestra barras de diferencia (ref - mix) por región ──
        // Verde = close match, Amarillo = slight diff, Rojo = big diff
        int colGap   = 2;
        int totalGap = colGap * (kNumRegions - 1);
        int colW     = (bounds.getWidth() - totalGap) / kNumRegions;

        // Center line
        int midY = bounds.getCentreY();

        // Draw label
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizePico)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
        g.drawText("DELTA", bounds.removeFromLeft(28), juce::Justification::centredLeft);

        for (int r = 0; r < kNumRegions; ++r) {
            int x    = bounds.getX() + r * (colW + colGap);
            int barH = 4;
            int barY = midY - barH / 2;

            float delta    = data_.deltaRegionEnergy[r];
            float absDelta = std::abs(delta);
            float norm     = juce::jmin(absDelta / 6.0f, 1.0f);
            int fillW      = (int)(norm * colW * 0.45f);

            // Background track
            g.setColour(MixCoachTheme::bgDarker().withAlpha(0.2f));
            g.fillRoundedRectangle((float)x, (float)barY, (float)colW, (float)barH, 1.5f);

            if (fillW > 0) {
                juce::Colour deltaCol;
                if (absDelta < 1.5f) deltaCol = MixCoachTheme::success().withAlpha(0.6f);
                else if (absDelta < 3.0f)
                    deltaCol = MixCoachTheme::warning().withAlpha(0.7f);
                else
                    deltaCol = MixCoachTheme::error().withAlpha(0.8f);

                int fillX;
                if (delta > 0) fillX = x + colW / 2; // Ref louder → grow right
                else
                    fillX = x + colW / 2 - fillW; // Mix louder → grow left

                g.setColour(deltaCol);
                g.fillRoundedRectangle((float)fillX, (float)barY, (float)fillW, (float)barH, 1.5f);
            }

            // Center zero line
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.15f));
            g.drawVerticalLine(x + colW / 2, (float)barY, (float)(barY + barH));
        }
    }

    void ReferenceMatchPanel::drawLUFSMeter(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        float lufsDiff    = data_.mixIntegratedLUFS - data_.refIntegratedLUFS;
        float diffClamped = juce::jmap(lufsDiff, -12.0f, 12.0f, -1.0f, 1.0f);
        diffClamped       = juce::jlimit(-1.0f, 1.0f, diffClamped);

        int midX = bounds.getCentreX();
        int barW = bounds.getWidth() - 20;
        int barH = 8;
        int barY = bounds.getY() + 14;

        // ─── Background bar ───────────────────────────────────────────────
        g.setColour(MixCoachTheme::bgDarker().withAlpha(0.4f));
        g.fillRoundedRectangle((float)(midX - barW / 2), (float)barY, (float)barW, (float)barH, 3.0f);

        // ─── Filled portion (animated via SmoothValue) ─────────────────────
        float animDiff = juce::jmap(lufsSmooth_.getCurrent(), -12.0f, 12.0f, -1.0f, 1.0f);
        animDiff       = juce::jlimit(-1.0f, 1.0f, animDiff);
        int fillW      = (int)(std::abs(animDiff) * barW / 2);
        if (fillW > 0) {
            auto fillCol = (lufsDiff > 0.0f) ? MixCoachTheme::warning()     // mix louder → yellow
                                             : MixCoachTheme::accentCyan(); // mix quieter → cyan
            int fillX    = (lufsDiff > 0.0f) ? midX : midX - fillW;
            g.setColour(fillCol.withAlpha(0.6f));
            g.fillRoundedRectangle((float)fillX, (float)barY, (float)fillW, (float)barH, 3.0f);

            // Fill shine
            auto fillShine = juce::Rectangle<float>((float)fillX, (float)barY, (float)fillW, (float)(barH / 2));
            g.setColour(juce::Colours::white.withAlpha(0.10f));
            g.fillRoundedRectangle(fillShine, 2.0f);
        }

        // ─── Centre line ──────────────────────────────────────────────────
        g.setColour(MixCoachTheme::textPrimary().withAlpha(0.5f));
        g.drawVerticalLine(midX, (float)barY, (float)(barY + barH));

        // ─── Tick marks at -6, 0, +6 LU ───────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(5.5f)));
        g.setColour(MixCoachTheme::textDim().withAlpha(0.3f));
        auto drawTick = [&](float lu, const char* label) {
            float normPos = juce::jmap(lu, -12.0f, 12.0f, 0.0f, 1.0f);
            int tickX     = midX - barW / 2 + (int)(normPos * barW);
            g.setColour(MixCoachTheme::textDim().withAlpha(0.25f));
            g.drawVerticalLine(tickX, (float)(barY - 2), (float)(barY - 1));
            g.setColour(MixCoachTheme::textDim().withAlpha(0.2f));
            g.drawText(label, tickX - 8, barY + barH + 1, 16, 8, juce::Justification::centred);
        };
        drawTick(-6.0f, "-6");
        drawTick(0.0f, "0");
        drawTick(6.0f, "+6");
    }

    void ReferenceMatchPanel::drawCrestMeter(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        int barGap   = 3;
        int labelH   = 10;
        int totalGap = barGap + 4;
        int barW     = (bounds.getWidth() - totalGap) / 2;
        int barAreaH = bounds.getHeight() - labelH;

        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)));

        // ─── Map crest dB (0-24) to 0-1 ────────────────────────────────────
        auto normCrest = [](float val) -> float {
            return juce::jlimit(0.0f, 1.0f, juce::jmap(val, 0.0f, 24.0f, 0.0f, 1.0f));
        };

        float mixVal    = normCrest(data_.mixCrestFactor);
        float refVal    = normCrest(data_.refCrestFactor);
        float crestDiff = data_.mixCrestFactor - data_.refCrestFactor;
        bool isMatch    = std::abs(crestDiff) < 3.0f;

        // ─── Mix Crest bar (left) ──────────────────────────────────────────
        int x       = bounds.getX();
        auto mixCol = isMatch ? MixCoachTheme::success() : MixCoachTheme::accent();
        int mixH    = (int)(mixVal * barAreaH);
        int mixY    = bounds.getY() + barAreaH - mixH;

        auto mixRect = juce::Rectangle<float>((float)x, (float)mixY, (float)barW, (float)mixH);
        juce::ColourGradient mixGrad(mixCol.brighter(0.2f).withAlpha(0.85f),
                                     mixRect.getCentreX(),
                                     mixRect.getY(),
                                     mixCol.withAlpha(0.65f),
                                     mixRect.getCentreX(),
                                     mixRect.getBottom(),
                                     false);
        g.setGradientFill(mixGrad);
        g.fillRoundedRectangle(mixRect, 2.0f);

        auto mixShine = mixRect.withHeight(juce::jmin(3.0f, mixRect.getHeight() * 0.3f));
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.fillRoundedRectangle(mixShine, 1.5f);

        g.setColour(mixCol.withAlpha(0.25f));
        g.drawRoundedRectangle(mixRect, 2.0f, 0.5f);
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened());
        g.setColour(MixCoachTheme::textDim());
        g.drawText("M " + juce::String(data_.mixCrestFactor, 1),
                   juce::Rectangle<int>(x, bounds.getBottom() - labelH, barW, labelH),
                   juce::Justification::centred);

        // ─── Ref Crest bar (right) ─────────────────────────────────────────
        int x2      = x + barW + barGap;
        auto refCol = isMatch ? MixCoachTheme::success() : MixCoachTheme::accentCyan();
        int refH    = (int)(refVal * barAreaH);
        int refY    = bounds.getY() + barAreaH - refH;

        auto refRect = juce::Rectangle<float>((float)x2, (float)refY, (float)barW, (float)refH);
        g.setColour(refCol.withAlpha(0.35f));
        g.fillRoundedRectangle(refRect, 2.0f);
        g.setColour(refCol.withAlpha(0.55f));
        g.drawRoundedRectangle(refRect, 2.0f, 0.8f);

        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened());
        g.setColour(MixCoachTheme::textDim());
        g.drawText("R " + juce::String(data_.refCrestFactor, 1),
                   juce::Rectangle<int>(x2, bounds.getBottom() - labelH, barW, labelH),
                   juce::Justification::centred);
    }

    void ReferenceMatchPanel::drawCorrelationMeter(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        float mixCorr  = juce::jlimit(-1.0f, 1.0f, data_.mixCorrelation);
        float refCorr  = juce::jlimit(-1.0f, 1.0f, data_.refCorrelation);
        float corrDiff = mixCorr - refCorr;
        bool isMatch   = std::abs(corrDiff) < 0.15f;

        int midX  = bounds.getCentreX();
        int barW  = bounds.getWidth() - 16;
        int barH  = 8;
        int barY  = bounds.getY() + 12;
        int infoH = 10;

        // ─── Background bar ───────────────────────────────────────────────
        g.setColour(MixCoachTheme::bgDarker().withAlpha(0.4f));
        g.fillRoundedRectangle((float)(midX - barW / 2), (float)barY, (float)barW, (float)barH, 3.0f);

        // ─── Zone indicators: -0.3, 0, +0.3 ───────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(5.5f)));
        g.setColour(MixCoachTheme::textDim().withAlpha(0.2f));
        auto drawCorrTick = [&](float val, const char* label) {
            float norm = juce::jmap(val, -1.0f, 1.0f, 0.0f, 1.0f);
            int tx     = midX - barW / 2 + (int)(norm * barW);
            g.drawVerticalLine(tx, (float)(barY - 1), (float)(barY));
            g.drawText(label, tx - 6, barY + barH + 1, 12, 8, juce::Justification::centred);
        };
        drawCorrTick(-0.3f, "-0.3");
        drawCorrTick(0.0f, "0");
        drawCorrTick(0.3f, "+0.3");

        // ─── Ref correlation marker (vertical line) ────────────────────────
        float refNorm  = juce::jmap(refCorr, -1.0f, 1.0f, 0.0f, 1.0f);
        int refMarkerX = midX - barW / 2 + (int)(refNorm * barW);
        g.setColour(MixCoachTheme::accentCyan().withAlpha(0.6f));
        g.drawVerticalLine(refMarkerX, (float)(barY - 2), (float)(barY + barH + 2));

        // ─── Mix correlation filled bar (centered around ref marker) ──────
        if (!isMatch) {
            float diffNorm = juce::jmap(corrDiff, -0.5f, 0.5f, -1.0f, 1.0f);
            diffNorm       = juce::jlimit(-1.0f, 1.0f, diffNorm);
            int fillW      = (int)(std::abs(diffNorm) * barW / 4);
            if (fillW > 0) {
                auto fillCol = (corrDiff > 0.0f) ? MixCoachTheme::warning()     // mix mas mono → yellow
                                                 : MixCoachTheme::accentCyan(); // mix mas wide → cyan
                int fillX    = (corrDiff > 0.0f) ? refMarkerX : refMarkerX - fillW;
                fillX        = juce::jmax(midX - barW / 2, juce::jmin(fillX, midX + barW / 2 - fillW));
                g.setColour(fillCol.withAlpha(0.6f));
                g.fillRoundedRectangle((float)fillX, (float)barY, (float)fillW, (float)barH, 3.0f);
            }
        }

        // ─── Mix correlation dot marker ────────────────────────────────────
        float mixNorm2 = juce::jmap(mixCorr, -1.0f, 1.0f, 0.0f, 1.0f);
        int mixMarkerX = midX - barW / 2 + (int)(mixNorm2 * barW);
        mixMarkerX     = juce::jlimit(midX - barW / 2 + 3, midX + barW / 2 - 3, mixMarkerX);
        auto dotCol    = isMatch ? MixCoachTheme::success() : MixCoachTheme::accentGlow();
        g.setColour(dotCol);
        g.fillEllipse((float)(mixMarkerX - 3), (float)(barY - 1), 7.0f, 10.0f);

        // ─── Centre line (0 correlation) ───────────────────────────────────
        int zeroX = midX - barW / 2 + (int)(juce::jmap(0.0f, -1.0f, 1.0f, 0.0f, 1.0f) * barW);
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.15f));
        g.drawVerticalLine(zeroX, (float)barY, (float)(barY + barH));

        // ─── Values text ──────────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)));
        g.setColour(MixCoachTheme::textDim());
        juce::String corrText = juce::String(mixCorr, 2) + " vs " + juce::String(refCorr, 2);
        g.drawText(corrText, bounds.removeFromTop(infoH).reduced(2, 0), juce::Justification::centredRight);
    }

    void ReferenceMatchPanel::drawMetricsRow(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        // ─── LUFS Section ──────────────────────────────────────────────────
        {
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
            g.setColour(MixCoachTheme::textMuted());
            g.drawText("LUFS", bounds.removeFromTop(12).reduced(2, 0), juce::Justification::centredLeft);

            auto lufsArea = bounds.removeFromTop(30);
            drawLUFSMeter(g, lufsArea);

            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)));
            g.setColour(MixCoachTheme::textDim());

            float lufsMix   = data_.mixIntegratedLUFS;
            float lufsRef   = data_.refIntegratedLUFS;
            float lufsDelta = lufsMix - lufsRef;

            juce::Colour deltaCol;
            juce::String arrow;
            if (std::abs(lufsDelta) < 1.0f) {
                deltaCol = MixCoachTheme::success();
                arrow    = "≈";
            }
            else if (lufsDelta > 0) {
                deltaCol = MixCoachTheme::warning();
                arrow    = "▲";
            }
            else {
                deltaCol = MixCoachTheme::accentCyan();
                arrow    = "▼";
            }

            g.setColour(deltaCol);
            juce::String lufsText = juce::String(lufsMix, 1) + " vs " + juce::String(lufsRef, 1) + " LUFS  " + arrow
                                    + juce::String(std::abs(lufsDelta), 1);
            g.drawText(lufsText, lufsArea.removeFromTop(10).reduced(2, 0), juce::Justification::centredRight);
        }

        bounds.removeFromTop(3);

        // ─── Crest Section ─────────────────────────────────────────────────
        {
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
            g.setColour(MixCoachTheme::textMuted());
            g.drawText("Crest", bounds.removeFromTop(12).reduced(2, 0), juce::Justification::centredLeft);

            auto crestArea = bounds.removeFromTop(40);
            drawCrestMeter(g, crestArea);
        }

        bounds.removeFromTop(3);

        // ─── Correlation Section ───────────────────────────────────────────
        {
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
            g.setColour(MixCoachTheme::textMuted());
            g.drawText("Corr", bounds.removeFromTop(12).reduced(2, 0), juce::Justification::centredLeft);

            auto corrArea = bounds.removeFromTop(30);
            drawCorrelationMeter(g, corrArea);
        }
    }

    void ReferenceMatchPanel::drawMatchScore(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        float animScore = scoreSmooth_.getCurrent();
        auto score      = juce::jmin(100, juce::jmax(0, (int)(animScore + 0.5f)));
        auto col        = score >= 80   ? MixCoachTheme::success()
                          : score >= 50 ? MixCoachTheme::warning()
                                        : MixCoachTheme::error();

        // ─── Background pill ───────────────────────────────────────────────
        g.setColour(col.withAlpha(0.12f));
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

        // ─── Inner glow for high scores ────────────────────────────────────
        if (score >= 80) {
            g.setColour(col.withAlpha(0.06f));
            g.fillRoundedRectangle(bounds.toFloat().expanded(2.0f, 1.0f), 6.0f);
        }

        // ─── Score number ──────────────────────────────────────────────────
        g.setColour(col);
        g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
        g.drawText(juce::String(score), bounds, juce::Justification::centred);

        // ─── Small \"pts\" label ────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(5.5f)));
        g.setColour(col.withAlpha(0.6f));
        g.drawText("pts", bounds.removeFromBottom(6), juce::Justification::centred);
    }

} // namespace mixcoach
