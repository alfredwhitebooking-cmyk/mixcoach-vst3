#include "MeterPanel.h"
#include "AnalyzersPanelDrawing.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace mixcoach {

    MeterPanel::MeterPanel() {}

    void MeterPanel::resized() {}

    bool MeterPanel::advanceVisuals(double sr)
    {
        bool dirty = false;
        dirty |= leftPeak_.advance(sr);
        dirty |= rightPeak_.advance(sr);
        dirty |= leftRms_.advance(sr);
        dirty |= rightRms_.advance(sr);
        dirty |= momentaryLUFS_.advance(sr);
        dirty |= shortTermLUFS_.advance(sr);
        dirty |= integratedLUFS_.advance(sr);

        const float dt        = 1.0f / (float)sr;
        const float holdTime  = 2.0f;
        const float decayRate = 30.0f;

        if (rawLeftPeak_ > leftPeakHold_) {
            leftPeakHold_      = rawLeftPeak_;
            leftPeakHoldTimer_ = 0.0f;
        }
        else {
            leftPeakHoldTimer_ += dt;
            if (leftPeakHoldTimer_ > holdTime) {
                float old = leftPeakHold_;
                leftPeakHold_ -= decayRate * dt;
                if (leftPeakHold_ < rawLeftPeak_) leftPeakHold_ = rawLeftPeak_;
                if (std::abs(leftPeakHold_ - old) > 0.01f) dirty = true;
            }
        }

        if (rawRightPeak_ > rightPeakHold_) {
            rightPeakHold_      = rawRightPeak_;
            rightPeakHoldTimer_ = 0.0f;
        }
        else {
            rightPeakHoldTimer_ += dt;
            if (rightPeakHoldTimer_ > holdTime) {
                float old = rightPeakHold_;
                rightPeakHold_ -= decayRate * dt;
                if (rightPeakHold_ < rawRightPeak_) rightPeakHold_ = rawRightPeak_;
                if (std::abs(rightPeakHold_ - old) > 0.01f) dirty = true;
            }
        }

        if (dirty) repaint();
        return dirty;
    }

    void MeterPanel::updateData(const AudioAnalyzer& analyzer)
    {
        const auto& master = analyzer.getMasterAnalysis();
        const auto& left   = analyzer.getLeftAnalysis();
        const auto& right  = analyzer.getRightAnalysis();

        rawPeak_ = master.getPeak();
        rawRms_  = master.getRMS();
        rawLufs_ = analyzer.getIntegratedLUFS();

        float crest = rawPeak_ - rawRms_;
        if (crest > 0.0f && crest < 40.0f) rawDr_ = crest;

        rawLeftPeak_  = left.getPeak();
        rawRightPeak_ = right.getPeak();

        momentaryLUFS_.setTargetValue(analyzer.getMomentaryLUFS());
        shortTermLUFS_.setTargetValue(analyzer.getShortTermLUFS());
        integratedLUFS_.setTargetValue(analyzer.getIntegratedLUFS());

        leftPeak_.setTargetValue(left.getPeak());
        rightPeak_.setTargetValue(right.getPeak());
        leftRms_.setTargetValue(left.getRMS());
        rightRms_.setTargetValue(right.getRMS());

        // No repaint() aquí: advanceVisuals() (60 Hz) repinta solo cuando un
        // valor suavizado cambia. Repintar aquí además duplicaba repaints/frame.
    }

    void MeterPanel::paint(juce::Graphics& g)
    {
        auto area = getLocalBounds();

        {
            MixCoachTheme::fillGlassPanel(g, area.toFloat(), 6.0f);
        }

        auto bounds = area.reduced(6, 5);

        auto headerArea = bounds.removeFromTop(16);
        drawAnalyzerHeader(g, headerArea, "METER");
        bounds.removeFromTop(3);

        const int totalW = bounds.getWidth();
        const int gap    = 5;

        int leftW   = totalW * 34 / 100;
        int centerW = totalW * 38 / 100;
        int rightW  = totalW - leftW - centerW - gap * 2;

        auto leftCol = bounds.removeFromLeft(leftW);
        bounds.removeFromLeft(gap);
        auto centerCol = bounds.removeFromLeft(centerW);
        bounds.removeFromLeft(gap);
        auto rightCol = bounds;

        drawPeakMeterStereo(g, leftCol);
        drawStatsPanel(g, centerCol);
        drawLufsMeterStereo(g, rightCol);
    }

    void MeterPanel::drawPeakMeterStereo(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        auto col           = bounds.toFloat();
        auto scaleArea     = col.removeFromLeft(28);
        auto barArea       = col.reduced(1, 0);
        const float barGap = 3.0f;
        const float barW   = (barArea.getWidth() - barGap) * 0.5f;

        {
            auto labelArea = barArea.removeFromTop(14);
            float halfW    = (labelArea.getWidth() - barGap) * 0.5f;
            auto lLabel    = labelArea.removeFromLeft(halfW);
            labelArea.removeFromLeft(barGap);
            auto rLabel = labelArea;
            g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
            g.setColour(MixCoachTheme::textSecondary());
            g.drawText("L", lLabel, juce::Justification::centred);
            g.drawText("R", rLabel, juce::Justification::centred);
        }

        auto digitalArea = barArea.removeFromBottom(20);
        float halfDigW   = (digitalArea.getWidth() - barGap) * 0.5f;
        auto lDigArea    = digitalArea.removeFromLeft(halfDigW);
        digitalArea.removeFromLeft(barGap);
        auto rDigArea = digitalArea;

        auto lBarBounds = barArea.removeFromLeft(barW);
        barArea.removeFromLeft(barGap);
        auto rBarBounds = barArea;

        auto adjustedScale = scaleArea.withY(lBarBounds.getY()).withHeight(lBarBounds.getHeight());
        drawDbScale(g, adjustedScale, -60.0f, 6.0f, 6.0f, 0.0f, false);

        drawPeakBar(g, lBarBounds, leftPeak_.getCurrent(), leftPeakHold_);
        drawPeakBar(g, rBarBounds, rightPeak_.getCurrent(), rightPeakHold_);

        g.setFont(juce::Font(juce::FontOptions(15.0f)).boldened());

        auto zoneColor = [](float val) -> juce::Colour {
            if (val >= 0.0f) return MixCoachTheme::error();
            if (val >= -6.0f) return MixCoachTheme::warning();
            return MixCoachTheme::meterGreen();
        };

        float lVal        = leftPeak_.getCurrent();
        float rVal        = rightPeak_.getCurrent();
        juce::String lStr = (lVal > -60.0f) ? juce::String(lVal, 2) : "--.-";
        juce::String rStr = (rVal > -60.0f) ? juce::String(rVal, 2) : "--.-";

        for (auto* dig : {&lDigArea, &rDigArea}) {
            auto pill = dig->toFloat();
            g.setColour(MixCoachTheme::bgDarker().withAlpha(0.75f));
            g.fillRoundedRectangle(pill, 3.0f);
            g.setColour(MixCoachTheme::bgSurface().withAlpha(0.25f));
            g.drawRoundedRectangle(pill, 3.0f, 0.5f);
        }

        g.setColour(juce::Colour(0xFF000000).withAlpha(0.40f));
        g.drawText(lStr, lDigArea.translated(1, 1), juce::Justification::centred);
        g.setColour(zoneColor(lVal));
        g.drawText(lStr, lDigArea, juce::Justification::centred);

        g.setColour(juce::Colour(0xFF000000).withAlpha(0.40f));
        g.drawText(rStr, rDigArea.translated(1, 1), juce::Justification::centred);
        g.setColour(zoneColor(rVal));
        g.drawText(rStr, rDigArea, juce::Justification::centred);
    }

    void MeterPanel::drawStatsPanel(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        auto col             = bounds;
        const int numModules = 4;
        const int moduleGap  = 3;
        const int moduleH    = (col.getHeight() - (numModules - 1) * moduleGap) / numModules;

        struct StatModule
        {
            const char* title;
            float value;
            const char* unit;
            juce::Colour accent;
            bool highlight;
        };

        float peakPrecise = juce::jmax(leftPeak_.getCurrent(), rightPeak_.getCurrent());
        float rawDr       = peakPrecise - rawRms_;
        if (rawDr < 0.0f || rawDr > 40.0f) rawDr = rawDr_;

        StatModule modules[] = {
            {"PEAK", peakPrecise, "dB", MixCoachTheme::meterYellow(), false},
            {"RMS", rawRms_, "dB", MixCoachTheme::accentCyan(), false},
            {"LUFS (I)", (float)integratedLUFS_.getCurrent(), "LUFS", MixCoachTheme::accent(), true},
            {"DR", rawDr, "dB", MixCoachTheme::success(), false},
        };

        for (int i = 0; i < numModules; ++i) {
            auto moduleArea = col.removeFromTop(moduleH);
            if (i < numModules - 1) col.removeFromTop(moduleGap);
            drawStatModule(g,
                           moduleArea,
                           modules[i].title,
                           modules[i].value,
                           modules[i].unit,
                           modules[i].accent,
                           modules[i].highlight);
        }
    }

    void MeterPanel::drawLufsMeterStereo(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        auto col        = bounds.toFloat();
        auto headerArea = col.removeFromTop(14);
        g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
        g.setColour(MixCoachTheme::textSecondary());
        g.drawText("LUFS S/M", headerArea, juce::Justification::centred);

        auto scaleArea     = col.removeFromLeft(26);
        auto barArea       = col.reduced(1, 0);
        const float barGap = 3.0f;
        const float barW   = (barArea.getWidth() - barGap) * 0.5f;

        {
            auto labelArea = barArea.removeFromTop(12);
            float halfW    = (labelArea.getWidth() - barGap) * 0.5f;
            auto lLabel    = labelArea.removeFromLeft(halfW);
            labelArea.removeFromLeft(barGap);
            auto rLabel = labelArea;
            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.setColour(MixCoachTheme::textDim());
            g.drawText("S", lLabel, juce::Justification::centred);
            g.drawText("M", rLabel, juce::Justification::centred);
        }

        const float lufsRange = 66.0f;
        float targetNorm      = juce::jlimit(0.0f, 1.0f, (lufsTarget_ + 60.0f) / lufsRange);
        auto markerArea       = barArea;

        auto digitalArea = barArea.removeFromBottom(20);
        float halfDigW   = (digitalArea.getWidth() - barGap) * 0.5f;
        auto lDigArea    = digitalArea.removeFromLeft(halfDigW);
        digitalArea.removeFromLeft(barGap);
        auto rDigArea = digitalArea;

        auto lBarBounds = barArea.removeFromLeft(barW);
        barArea.removeFromLeft(barGap);
        auto rBarBounds = barArea;

        auto adjustedScale = scaleArea.withY(lBarBounds.getY()).withHeight(lBarBounds.getHeight());
        drawDbScale(g, adjustedScale, -60.0f, 6.0f, 6.0f, lufsTarget_, true);

        drawLufsBar(g, lBarBounds, shortTermLUFS_.getCurrent(), lufsTarget_);
        drawLufsBar(g, rBarBounds, momentaryLUFS_.getCurrent(), lufsTarget_);

        {
            float targetY         = markerArea.getBottom() - markerArea.getHeight() * targetNorm;
            auto arrowBoundsL     = juce::Rectangle<float>(lBarBounds.getRight() - 6.0f, targetY - 2.0f, 8.0f, 4.0f);
            auto arrowBoundsR     = juce::Rectangle<float>(rBarBounds.getRight() - 6.0f, targetY - 2.0f, 8.0f, 4.0f);
            juce::Colour arrowCol = MixCoachTheme::accentCyan().withAlpha(0.60f);

            juce::Path arrowL;
            arrowL.addTriangle(arrowBoundsL.getTopLeft().x,
                               arrowBoundsL.getCentreY(),
                               arrowBoundsL.getTopRight().x,
                               arrowBoundsL.getY(),
                               arrowBoundsL.getTopRight().x,
                               arrowBoundsL.getBottom());
            g.setColour(arrowCol);
            g.fillPath(arrowL);
            g.drawHorizontalLine((int)targetY, arrowBoundsL.getX(), arrowBoundsL.getRight());

            juce::Path arrowR;
            arrowR.addTriangle(arrowBoundsR.getTopLeft().x,
                               arrowBoundsR.getCentreY(),
                               arrowBoundsR.getTopRight().x,
                               arrowBoundsR.getY(),
                               arrowBoundsR.getTopRight().x,
                               arrowBoundsR.getBottom());
            g.setColour(arrowCol);
            g.fillPath(arrowR);
            g.drawHorizontalLine((int)targetY, arrowBoundsR.getX(), arrowBoundsR.getRight());

            g.setFont(juce::Font(juce::FontOptions(5.5f)));
            g.setColour(arrowCol.withAlpha(0.5f));
            g.drawText(juce::String((int)lufsTarget_) + " LUFS",
                       juce::Rectangle<float>(arrowBoundsL.getRight() + 1, targetY - 4, 22, 8),
                       juce::Justification::centredLeft);
        }

        g.setFont(juce::Font(juce::FontOptions(15.0f)).boldened());
        juce::Colour digCol = MixCoachTheme::accentCyan();

        float lLufs = static_cast<float>(shortTermLUFS_.getCurrent());
        float rLufs = static_cast<float>(momentaryLUFS_.getCurrent());

        juce::String lLufsStr = (lLufs > -60.0f) ? juce::String(lLufs, 2) : "--.-";
        juce::String rLufsStr = (rLufs > -60.0f) ? juce::String(rLufs, 2) : "--.-";

        for (auto* dig : {&lDigArea, &rDigArea}) {
            auto pill = dig->toFloat();
            g.setColour(MixCoachTheme::bgDarker().withAlpha(0.75f));
            g.fillRoundedRectangle(pill, 3.0f);
            g.setColour(MixCoachTheme::bgSurface().withAlpha(0.25f));
            g.drawRoundedRectangle(pill, 3.0f, 0.5f);
        }

        g.setColour(juce::Colour(0xFF000000).withAlpha(0.40f));
        g.drawText(lLufsStr, lDigArea.translated(1, 1), juce::Justification::centred);
        g.drawText(rLufsStr, rDigArea.translated(1, 1), juce::Justification::centred);

        g.setColour(digCol);
        g.drawText(lLufsStr, lDigArea, juce::Justification::centred);
        g.drawText(rLufsStr, rDigArea, juce::Justification::centred);
    }

    void MeterPanel::drawPeakBar(juce::Graphics& g, juce::Rectangle<float> bounds, float level, float peakHold)
    {
        const auto colGreen  = MixCoachTheme::meterGreen();
        const auto colYellow = MixCoachTheme::warning();
        const auto colRed    = MixCoachTheme::error();

        g.setColour(MixCoachTheme::bgInput().withAlpha(0.9f));
        g.fillRoundedRectangle(bounds, 2.0f);

        float range = 66.0f;
        float norm  = juce::jlimit(0.0f, 1.0f, (level + 60.0f) / range);

        if (norm > 0.01f) {
            auto fillBounds = bounds.withTop(bounds.getBottom() - bounds.getHeight() * norm);

            constexpr float greenThresh = (-6.0f + 60.0f) / 66.0f;
            constexpr float redThresh   = (0.0f + 60.0f) / 66.0f;

            float greenY = bounds.getBottom() - bounds.getHeight() * greenThresh;
            float redY   = bounds.getBottom() - bounds.getHeight() * redThresh;

            float greenTop = juce::jmax(fillBounds.getY(), greenY);
            g.setColour(colGreen);
            g.fillRoundedRectangle(fillBounds.withTop(greenTop), 2.0f);

            if (level > -6.0f) {
                float yelTop = juce::jmax(fillBounds.getY(), redY);
                float yelBot = juce::jmin(fillBounds.getBottom(), greenY);
                if (yelTop < yelBot) {
                    g.setColour(colYellow);
                    g.fillRect(
                        juce::Rectangle<float>(fillBounds.getX(), yelTop, fillBounds.getWidth(), yelBot - yelTop));
                }
            }

            if (level > 0.0f) {
                float redTop = fillBounds.getY();
                float redBot = juce::jmin(fillBounds.getBottom(), redY);
                if (redTop < redBot) {
                    g.setColour(colRed);
                    g.fillRect(
                        juce::Rectangle<float>(fillBounds.getX(), redTop, fillBounds.getWidth(), redBot - redTop));
                }
            }

            auto capGlow = fillBounds.withHeight(juce::jmax(1.5f, fillBounds.getHeight() * 0.05f));
            juce::ColourGradient capGrad(juce::Colours::white.withAlpha(0.20f),
                                         juce::Point<float>(capGlow.getCentreX(), capGlow.getY()),
                                         juce::Colour(0x00000000),
                                         juce::Point<float>(capGlow.getCentreX(), capGlow.getBottom()),
                                         false);
            g.setGradientFill(capGrad);
            g.fillRect(capGlow);
        }

        if (peakHold > -60.0f) {
            float holdNorm = juce::jlimit(0.0f, 1.0f, (peakHold + 60.0f) / range);
            float holdY    = bounds.getBottom() - bounds.getHeight() * holdNorm;

            g.setColour(MixCoachTheme::meterOrange().brighter(0.1f).withAlpha(0.15f));
            g.fillRect(bounds.getX() + 1, holdY - 1.5f, bounds.getWidth() - 2, 3.0f);

            g.setColour(MixCoachTheme::meterOrange());
            g.fillRect(bounds.getX() + 1, holdY - 0.5f, bounds.getWidth() - 2, 1.5f);
        }

        g.setColour(MixCoachTheme::bgSurface().withAlpha(0.25f));
        g.drawRoundedRectangle(bounds, 2.0f, 0.5f);
    }

    void MeterPanel::drawLufsBar(juce::Graphics& g, juce::Rectangle<float> bounds, float level, float targetLUFS)
    {
        juce::ignoreUnused(targetLUFS);
        g.setColour(MixCoachTheme::bgInput().withAlpha(0.9f));
        g.fillRoundedRectangle(bounds, 2.0f);

        float range = 66.0f;
        float norm  = juce::jlimit(0.0f, 1.0f, (level + 60.0f) / range);

        if (norm > 0.01f) {
            auto fillBounds = bounds.withTop(bounds.getBottom() - bounds.getHeight() * norm);

            juce::ColourGradient grad(MixCoachTheme::accentCyanBright(),
                                      juce::Point<float>(fillBounds.getCentreX(), fillBounds.getY()),
                                      MixCoachTheme::info().darker(0.3f),
                                      juce::Point<float>(fillBounds.getCentreX(), fillBounds.getBottom()),
                                      false);
            grad.addColour(0.3f, MixCoachTheme::accentCyan());
            grad.addColour(0.6f, MixCoachTheme::accentCyanDim());
            g.setGradientFill(grad);
            g.fillRoundedRectangle(fillBounds, 2.0f);

            auto capGlow = fillBounds.withHeight(juce::jmax(1.5f, fillBounds.getHeight() * 0.05f));
            juce::ColourGradient capGrad(juce::Colours::white.withAlpha(0.18f),
                                         juce::Point<float>(capGlow.getCentreX(), capGlow.getY()),
                                         juce::Colour(0x00000000),
                                         juce::Point<float>(capGlow.getCentreX(), capGlow.getBottom()),
                                         false);
            g.setGradientFill(capGrad);
            g.fillRect(capGlow);
        }

        g.setColour(MixCoachTheme::bgSurface().withAlpha(0.25f));
        g.drawRoundedRectangle(bounds, 2.0f, 0.5f);
    }

    void MeterPanel::drawStatModule(juce::Graphics& g,
                                    juce::Rectangle<int> bounds,
                                    const juce::String& title,
                                    float value,
                                    const juce::String& unit,
                                    juce::Colour accent,
                                    bool highlight)
    {
        auto b = bounds.toFloat();

        g.setColour(MixCoachTheme::bgInput().withAlpha(0.88f));
        g.fillRoundedRectangle(b, 4.0f);

        juce::ColourGradient topGlow(juce::Colours::white.withAlpha(highlight ? 0.06f : 0.04f),
                                     juce::Point<float>(b.getX(), b.getY()),
                                     juce::Colour(0x00000000),
                                     juce::Point<float>(b.getX(), b.getCentreY()),
                                     false);
        g.setGradientFill(topGlow);
        g.fillRoundedRectangle(b, 4.0f);

        g.setColour(accent.withAlpha(highlight ? 0.25f : 0.12f));
        g.drawRoundedRectangle(b.reduced(0.5f), 4.0f, 0.7f);

        auto innerShadow = b.reduced(2.0f);
        juce::ColourGradient shadowGrad(juce::Colour(0xFF000000).withAlpha(0.0f),
                                        juce::Point<float>(innerShadow.getX(), innerShadow.getY()),
                                        juce::Colour(0xFF000000).withAlpha(0.10f),
                                        juce::Point<float>(innerShadow.getX(), innerShadow.getBottom()),
                                        false);
        g.setGradientFill(shadowGrad);
        g.fillRoundedRectangle(innerShadow, 3.0f);

        auto content = bounds.reduced(4, 2);

        auto titleArea = content.removeFromTop(14);
        g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
        g.setColour(accent.withAlpha(highlight ? 0.95f : 0.75f));
        g.drawText(title, titleArea, juce::Justification::centred);

        auto valueArea = content.removeFromTop(content.getHeight() * 0.65f);
        float fontSize = highlight ? 26.0f : 24.0f;
        g.setFont(juce::Font(juce::FontOptions(fontSize)).boldened());

        juce::String valStr;
        if (value > -60.0f) valStr = juce::String(value, 2);
        else
            valStr = "--.-";

        juce::Colour valCol = MixCoachTheme::textBright();
        if (title == "LUFS (I)") {
            float target = -14.0f;
            if (value > target + 1.0f) valCol = MixCoachTheme::error();
            else if (value > target - 2.0f)
                valCol = MixCoachTheme::warning();
            else if (value > target - 6.0f)
                valCol = MixCoachTheme::success();
        }
        else if (title == "PEAK") {
            if (value >= 0.0f) valCol = MixCoachTheme::error();
            else if (value >= -6.0f)
                valCol = MixCoachTheme::warning();
            else
                valCol = MixCoachTheme::success();
        }
        else if (title == "DR") {
            valCol = (value < 6.0f) ? MixCoachTheme::warning() : MixCoachTheme::success();
        }

        g.setColour(juce::Colour(0xFF000000).withAlpha(0.15f));
        g.drawText(valStr, valueArea.translated(1, 1), juce::Justification::centred);

        g.setColour(valCol);
        g.drawText(valStr, valueArea, juce::Justification::centred);

        if (highlight && value > -60.0f) {
            auto glowBounds = valueArea.toFloat().expanded(6.0f, 2.0f);
            g.setColour(accent.withAlpha(0.06f));
            g.fillRoundedRectangle(glowBounds, 3.0f);
        }

        auto unitArea = content;
        g.setFont(juce::Font(juce::FontOptions(8.0f)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.7f));
        g.drawText(unit, unitArea, juce::Justification::centred);
    }

    void MeterPanel::drawDbScale(juce::Graphics& g,
                                 juce::Rectangle<float> bounds,
                                 float minDb,
                                 float maxDb,
                                 float stepDb,
                                 float highlightVal,
                                 bool isLufsScale)
    {
        float range = maxDb - minDb;

        g.setColour(MixCoachTheme::bgInput().withAlpha(0.5f));
        g.fillRoundedRectangle(bounds, 2.0f);

        g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());

        for (float db = minDb; db <= maxDb + 0.5f; db += stepDb) {
            float norm = juce::jlimit(0.0f, 1.0f, (db - minDb) / range);
            float y    = bounds.getBottom() - bounds.getHeight() * norm;

            bool isZero = (std::abs(db) < 0.5f);
            bool isMid  = (std::abs(db) == 6.0f || std::abs(db) == 12.0f);
            float alpha = isZero ? 0.65f : (isMid ? 0.45f : 0.30f);
            float tickW = isZero ? 6.0f : (isMid ? 4.0f : 3.0f);

            if (isLufsScale && std::abs(db - 6.0f) < 0.5f) {
                alpha = 0.50f;
                tickW = 4.0f;
            }

            g.setColour(MixCoachTheme::textSecondary().withAlpha(isLufsScale ? alpha * 0.7f : alpha));
            g.drawHorizontalLine((int)y, bounds.getRight() - tickW, bounds.getRight());

            juce::String label = juce::String((int)db);
            g.setColour(MixCoachTheme::textDim().withAlpha(isLufsScale ? alpha * 0.8f : alpha));
            g.drawText(label,
                       juce::Rectangle<float>(bounds.getX() + 1, y - 4.0f, bounds.getWidth() - 4, 8.0f),
                       juce::Justification::centredRight);
        }

        if (highlightVal > minDb && highlightVal < maxDb) {
            float hNorm = juce::jlimit(0.0f, 1.0f, (highlightVal - minDb) / range);
            float hY    = bounds.getBottom() - bounds.getHeight() * hNorm;
            g.setColour(MixCoachTheme::accentCyan().withAlpha(0.35f));
            g.drawHorizontalLine((int)hY, bounds.getX(), bounds.getRight());
        }
    }

} // namespace mixcoach
