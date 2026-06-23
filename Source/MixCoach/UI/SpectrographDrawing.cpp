#include "SpectrographComponent.h"
#include <cmath>

namespace mixcoach {

    namespace {

        constexpr float kLabelFreqsHz[] = {20.0f,
                                           30.0f,
                                           50.0f,
                                           70.0f,
                                           100.0f,
                                           200.0f,
                                           300.0f,
                                           500.0f,
                                           700.0f,
                                           1000.0f,
                                           2000.0f,
                                           3000.0f,
                                           5000.0f,
                                           7000.0f,
                                           10000.0f,
                                           20000.0f};

        juce::String formatFreqLabel(float hz)
        {
            if (hz >= 1000.0f) return juce::String(hz / 1000.0f, hz >= 10000.0f ? 0 : 1) + "k";
            return juce::String((int)hz);
        }

    } // namespace

    // ═══════════════════════════════════════════════════════════════════════════
    //  Plot Background — deep navy with subtle glass edge
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::drawPlotBackground(juce::Graphics& g, juce::Rectangle<float> plot) const
    {
        g.setColour(MixCoachTheme::bgCanvas());
        g.fillRoundedRectangle(plot, 4.0f);

        g.setColour(MixCoachTheme::bgSurface().withAlpha(0.5f));
        g.drawRoundedRectangle(plot, 4.0f, 1.0f);

        auto topShadow = plot.withHeight(plot.getHeight() * 0.12f);
        juce::ColourGradient shadowGrad(juce::Colours::black.withAlpha(0.25f),
                                        juce::Point<float>(plot.getX(), plot.getY()),
                                        juce::Colour(0x00000000),
                                        juce::Point<float>(plot.getX(), topShadow.getBottom()),
                                        false);
        g.setGradientFill(shadowGrad);
        g.fillRoundedRectangle(topShadow, 4.0f);

        auto glassEdge = plot.withHeight(1.5f);
        juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.04f),
                                       juce::Point<float>(plot.getX(), glassEdge.getY()),
                                       juce::Colour(0x00000000),
                                       juce::Point<float>(plot.getX(), glassEdge.getBottom()),
                                       false);
        g.setGradientFill(glassGrad);
        g.fillRect(glassEdge);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Grid — IK-style horizontal (amplitude) + vertical (frequency)
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::drawGrid(juce::Graphics& g, juce::Rectangle<float> plot) const
    {
        const float norm0 = juce::jmap(0.0f, kDisplayBottomDb, kDisplayTopDb, 0.0f, 1.0f);
        const float y0    = plot.getBottom() - norm0 * plot.getHeight();
        {
            auto glowZone = juce::Rectangle<float>(plot.getX(), y0 - 1.5f, plot.getWidth(), 3.0f);
            juce::ColourGradient zeroGlow(juce::Colour(0xFFFFFFFF).withAlpha(0.08f),
                                          juce::Point<float>(plot.getCentreX(), glowZone.getY()),
                                          juce::Colour(0xFFFFFFFF).withAlpha(0.0f),
                                          juce::Point<float>(plot.getCentreX(), glowZone.getBottom()),
                                          false);
            zeroGlow.addColour(0.5f, juce::Colour(0xFFFFFFFF).withAlpha(0.04f));
            g.setGradientFill(zeroGlow);
            g.fillRect(glowZone);
        }

        for (int db = 0; db >= (int)kDisplayBottomDb; db -= 5) {
            const float norm = juce::jmap((float)db, kDisplayBottomDb, kDisplayTopDb, 0.0f, 1.0f);
            const float y    = plot.getBottom() - norm * plot.getHeight();

            if (db == 0) {
                g.setColour(MixCoachTheme::textBright().withAlpha(0.35f));
                g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
                g.setColour(MixCoachTheme::textBright().withAlpha(0.15f));
                g.drawHorizontalLine(juce::roundToInt(y) - 1, plot.getX(), plot.getRight());
            }
            else if (db == -5) {
                g.setColour(MixCoachTheme::textMuted().withAlpha(0.18f));
                g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
                g.setColour(MixCoachTheme::textMuted().withAlpha(0.12f));
                g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getX() + 4.0f);
            }
            else if (db % 10 == 0) {
                g.setColour(MixCoachTheme::textMuted().withAlpha(0.12f));
                g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
            }
            else {
                g.setColour(MixCoachTheme::textMuted().withAlpha(0.04f));
                g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
            }
        }

        // ─── Vertical lines at key frequencies ───────────────────────────────
        for (int fi = 0; fi < (int)(sizeof(kLabelFreqsHz) / sizeof(kLabelFreqsHz[0])); ++fi) {
            const float freq = kLabelFreqsHz[fi];
            const float x    = freqToX(freq, plot);

            bool isMajor = (freq == 100.0f || freq == 1000.0f || freq == 10000.0f);
            g.setColour(MixCoachTheme::textMuted().withAlpha(isMajor ? 0.10f : 0.05f));
            g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
        }

        // ─── Extra sub-100Hz grid density ────────────────────────────────────
        constexpr float subFreqs[] = {
            25.0f, 30.0f, 35.0f, 40.0f, 45.0f, 50.0f, 55.0f, 60.0f, 65.0f, 70.0f, 75.0f, 80.0f, 85.0f, 90.0f, 95.0f};
        for (float freq : subFreqs) {
            const float x = freqToX(freq, plot);
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.025f));
            g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  RTA Bars — discrete stepped bars with frequency-dependent colors
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::drawRtaBars(juce::Graphics& g, juce::Rectangle<float> plot) const
    {
        if (bands_.empty()) return;

        const auto colLow      = MixCoachTheme::success();
        const auto colMid      = MixCoachTheme::warning();
        const auto colHigh     = MixCoachTheme::error();
        const auto colPeakHold = MixCoachTheme::vuFace();
        juce::ignoreUnused(colPeakHold);

        struct BarDim
        {
            float x, w, h, topY;
            float level;
            juce::Colour col;
        };

        std::vector<BarDim> bars(static_cast<size_t>(kNumRtaBands));

        for (int i = 0; i < kNumRtaBands; ++i) {
            const float peak = bandLevels_[(size_t)i].getCurrent();
            const float rms  = bandRmsLevels_[(size_t)i].getCurrent();

            float blendLevel;
            if (displayMode_ == kPeak) blendLevel = peak;
            else if (displayMode_ == kRms)
                blendLevel = rms;
            else
                blendLevel = peak * kBlendPeakRatio + rms * kBlendRmsRatio;

            const auto& band  = bands_[(size_t)i];
            const float x0    = freqToX(band.lowHz, plot);
            const float x1    = freqToX(band.highHz, plot);
            const float bandW = x1 - x0;

            const float freqLogT = std::log2(band.centerHz / 20.0f) / std::log2(20000.0f / 20.0f);
            const float barRatio = 0.94f - 0.04f * freqLogT;
            const float barW     = bandW * barRatio;
            const float barX     = (x0 + x1) * 0.5f - barW * 0.5f;
            const float barH     = blendLevel * plot.getHeight();
            const float barY     = plot.getBottom() - barH;

            juce::Colour barCol;
            if (freqLogT < 0.5f) {
                float t       = freqLogT * 2.0f;
                float smoothT = 0.5f - 0.5f * std::cos(t * juce::MathConstants<float>::pi);
                barCol        = colLow.interpolatedWith(colMid, smoothT);
            }
            else {
                float t       = (freqLogT - 0.5f) * 2.0f;
                float smoothT = 0.5f - 0.5f * std::cos(t * juce::MathConstants<float>::pi);
                barCol        = colMid.interpolatedWith(colHigh, smoothT);
            }

            bars[static_cast<size_t>(i)] = {barX, barW, barH, barY, blendLevel, barCol};
        }

        // PASO 1: Spectral Fill — masa continua debajo de las barras
        {
            juce::Path fillPath;
            fillPath.startNewSubPath(bars[0].x, plot.getBottom());

            for (int i = 0; i < kNumRtaBands; ++i) {
                const float cx = bars[i].x + bars[i].w * 0.5f;
                const float y  = bars[i].topY;
                fillPath.lineTo(cx, y);
            }

            const auto& last = bars[(size_t)kNumRtaBands - 1];
            fillPath.lineTo(last.x + last.w, plot.getBottom());
            fillPath.closeSubPath();

            juce::Colour fillCol =
                bars[(size_t)(kNumRtaBands / 4)].col.interpolatedWith(bars[(size_t)(kNumRtaBands * 3 / 4)].col, 0.5f);

            juce::ColourGradient fillGrad(fillCol.withAlpha(0.12f),
                                          juce::Point<float>(plot.getCentreX(), bars[0].topY),
                                          fillCol.withAlpha(0.0f),
                                          juce::Point<float>(plot.getCentreX(), plot.getBottom()),
                                          false);
            fillGrad.addColour(0.3f, fillCol.withAlpha(0.06f));
            g.setGradientFill(fillGrad);
            g.fillPath(fillPath);
        }

        // PASO 2: Barras individuales con gradient vertical
        for (int i = 0; i < kNumRtaBands; ++i) {
            const auto& bar = bars[static_cast<size_t>(i)];
            if (bar.h < 0.5f) continue;

            auto barRect = juce::Rectangle<float>(bar.x, bar.topY, bar.w, bar.h);

            juce::ColourGradient barGrad(bar.col.withAlpha(0.75f),
                                         juce::Point<float>(barRect.getCentreX(), bar.topY),
                                         bar.col.withAlpha(0.15f),
                                         juce::Point<float>(barRect.getCentreX(), plot.getBottom()),
                                         false);
            g.setGradientFill(barGrad);
            g.fillRect(barRect);

            g.setColour(bar.col.withAlpha(0.55f));
            g.fillRect(bar.x, bar.topY, bar.w, 1.5f);

            auto glowRect = juce::Rectangle<float>(bar.x, bar.topY - 0.5f, bar.w, 3.0f);
            juce::ColourGradient capGlow(bar.col.withAlpha(0.20f),
                                         juce::Point<float>(glowRect.getCentreX(), glowRect.getY()),
                                         bar.col.withAlpha(0.0f),
                                         juce::Point<float>(glowRect.getCentreX(), glowRect.getBottom()),
                                         false);
            g.setGradientFill(capGlow);
            g.fillRect(glowRect);
        }

        // PASO 3: Peak hold
        if (peakHoldEnabled_) {
            for (int i = 0; i < kNumRtaBands; ++i) {
                const float level = bandLevels_[(size_t)i].getCurrent();
                const float peak  = bandPeaks_[(size_t)i];

                if (peak > level && peak > 0.02f) {
                    const auto& bar   = bars[static_cast<size_t>(i)];
                    const float peakY = plot.getBottom() - peak * plot.getHeight();

                    g.setColour(MixCoachTheme::vuFace().withAlpha(0.20f));
                    g.fillRect(bar.x, peakY - 1.0f, bar.w, 3.0f);

                    g.setColour(MixCoachTheme::vuFace().withAlpha(0.92f));
                    g.fillRect(bar.x + 1.0f, peakY - 0.5f, bar.w - 2.0f, 1.5f);
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  dB Axis Labels
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::drawDbAxis(juce::Graphics& g, juce::Rectangle<float> labelCol) const
    {
        for (int db = 0; db >= (int)kDisplayBottomDb; db -= 5) {
            const float norm = juce::jmap((float)db, kDisplayBottomDb, kDisplayTopDb, 0.0f, 1.0f);
            const float y    = labelCol.getBottom() - norm * labelCol.getHeight();

            if (db == 0) {
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
                g.setColour(MixCoachTheme::textBright().withAlpha(0.7f).withAlpha(0.95f));
                g.drawText("0",
                           juce::Rectangle<float>(labelCol.getX(), y - 5.0f, labelCol.getWidth(), 10.0f),
                           juce::Justification::centredRight);
            }
            else {
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)));
                g.setColour(MixCoachTheme::textDim().withAlpha(0.75f));
                g.drawText(juce::String(db),
                           juce::Rectangle<float>(labelCol.getX(), y - 5.0f, labelCol.getWidth(), 10.0f),
                           juce::Justification::centredRight);

                g.setColour(MixCoachTheme::textDim().withAlpha(0.20f));
                g.drawHorizontalLine(juce::roundToInt(y), labelCol.getRight() - 0.5f, labelCol.getRight() + 3.0f);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Frequency Axis Labels
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::drawFreqAxis(juce::Graphics& g, juce::Rectangle<float> plot) const
    {
        juce::ignoreUnused(plot);
        static constexpr float kLabelFreqsHz[] = {20.0f,
                                                  30.0f,
                                                  50.0f,
                                                  70.0f,
                                                  100.0f,
                                                  200.0f,
                                                  300.0f,
                                                  500.0f,
                                                  700.0f,
                                                  1000.0f,
                                                  2000.0f,
                                                  3000.0f,
                                                  5000.0f,
                                                  7000.0f,
                                                  10000.0f,
                                                  20000.0f};

        const float labelH = 18.0f;
        const float labelY = (float)getHeight() - labelH - 2.0f;
        auto labelRow      = juce::Rectangle<float>(0.0f, labelY, (float)getWidth(), labelH);

        g.setColour(MixCoachTheme::bgCanvas().withAlpha(0.95f));
        g.fillRect(labelRow);

        auto edgeGlow = labelRow.withHeight(1.0f);
        juce::ColourGradient edgeGrad(MixCoachTheme::textMuted().withAlpha(0.20f),
                                      edgeGlow.getX(),
                                      edgeGlow.getY(),
                                      MixCoachTheme::textMuted().withAlpha(0.0f),
                                      edgeGlow.getRight(),
                                      edgeGlow.getY(),
                                      false);
        edgeGrad.addColour(0.5f, MixCoachTheme::textMuted().withAlpha(0.10f));
        g.setGradientFill(edgeGrad);
        g.fillRect(edgeGlow);

        for (float freq : kLabelFreqsHz) {
            const float x = freqToX(freq, plot);
            bool isMajor  = (freq == 100.0f || freq == 1000.0f || freq == 10000.0f);
            g.setColour(MixCoachTheme::textMuted().withAlpha(isMajor ? 0.25f : 0.12f));
            g.drawVerticalLine(juce::roundToInt(x), labelRow.getY(), labelRow.getBottom());
        }

        auto formatFreqLabel = [](float hz) -> juce::String {
            if (hz >= 1000.0f) return juce::String(hz / 1000.0f, hz >= 10000.0f ? 0 : 1) + "k";
            return juce::String((int)hz);
        };

        for (float freq : kLabelFreqsHz) {
            const float x = freqToX(freq, plot);
            const float w = (freq >= 1000.0f) ? 30.0f : 22.0f;

            bool isMajor = (freq == 100.0f || freq == 1000.0f || freq == 10000.0f);
            if (isMajor) {
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall - 1.5f)).boldened());
                g.setColour(juce::Colours::white.withAlpha(0.92f));
            }
            else {
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)));
                g.setColour(MixCoachTheme::textDim().withAlpha(0.80f));
            }
            g.drawText(formatFreqLabel(freq),
                       juce::Rectangle<float>(x - w * 0.5f, labelRow.getY(), w, labelRow.getHeight()),
                       juce::Justification::centred);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Settings Chrome — gear icon + status indicators + REF toggle button
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::drawSettingsChrome(juce::Graphics& g) const
    {
        if (settingsButton_.isEmpty()) return;

        auto btn = settingsButton_.toFloat().reduced(3.0f);
        auto c   = btn.getCentre();

        const float iconAlpha = (waterfallEnabled_ || referenceEnabled_) ? 0.40f : 0.20f;
        g.setColour(MixCoachTheme::textSecondary().withAlpha(iconAlpha));
        g.drawEllipse(btn, 0.8f);

        float outerR = 5.0f;
        float innerR = 2.5f;
        for (int i = 0; i < 6; ++i) {
            const float a = (float)i * juce::MathConstants<float>::twoPi / 6.0f - juce::MathConstants<float>::pi / 6.0f;
            float tipX    = c.x + outerR * std::cos(a);
            float tipY    = c.y + outerR * std::sin(a);
            float baseX   = c.x + innerR * std::cos(a);
            float baseY   = c.y + innerR * std::sin(a);
            g.drawLine(c.x, c.y, tipX, tipY, 1.0f);
            g.drawLine(baseX, baseY, tipX, tipY, 0.6f);
        }

        if (waterfallEnabled_) {
            g.setColour(MixCoachTheme::accentCyanBright().withAlpha(0.70f));
            g.fillEllipse(c.x - 1.5f, c.y - 1.5f, 3.0f, 3.0f);
        }
        else {
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.25f));
            g.drawEllipse(c.x - 1.5f, c.y - 1.5f, 3.0f, 3.0f, 0.8f);
        }

        // Slope indicator
        auto slopeInd         = btn.translated(0, 28).withHeight(8);
        juce::String slopeStr = "S" + juce::String(kSlopePresets[slopePresetIndex_], 1);
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizePico)).boldened());
        g.setColour(MixCoachTheme::accentCyanBright().withAlpha(0.50f));
        g.drawText(slopeStr, slopeInd, juce::Justification::centred);
        g.setColour(MixCoachTheme::accentCyanBright().withAlpha(0.08f));
        g.fillRoundedRectangle(slopeInd.expanded(2.0f, 1.0f).withHeight(8), 2.0f);

        // Tilt indicator
        auto tiltInd = btn.translated(0, 46).withHeight(7);
        if (pinkNoiseEnabled_) {
            g.setColour(MixCoachTheme::meterYellow().withAlpha(0.55f));
            g.setFont(juce::Font(juce::FontOptions(5.5f)).boldened());
            g.drawText("T", tiltInd, juce::Justification::centred);
            auto tiltPill = tiltInd.expanded(3.0f, 1.0f).withHeight(7);
            g.setColour(MixCoachTheme::meterYellow().withAlpha(0.08f));
            g.fillRoundedRectangle(tiltPill, 2.0f);
        }
        else {
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.15f));
            g.setFont(juce::Font(juce::FontOptions(5.0f)));
            g.drawText("T", tiltInd, juce::Justification::centred);
        }

        // Peak hold indicator "H"
        auto holdInd = btn.translated(0, 55).withHeight(7);
        if (peakHoldEnabled_) {
            g.setColour(MixCoachTheme::meterYellow().withAlpha(0.55f));
            g.setFont(juce::Font(juce::FontOptions(5.5f)).boldened());
            g.drawText("H", holdInd, juce::Justification::centred);
            auto holdPill = holdInd.expanded(3.0f, 1.0f).withHeight(7);
            g.setColour(MixCoachTheme::meterYellow().withAlpha(0.08f));
            g.fillRoundedRectangle(holdPill, 2.0f);
        }
        else {
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.15f));
            g.setFont(juce::Font(juce::FontOptions(5.0f)));
            g.drawText("H", holdInd, juce::Justification::centred);
        }

        // Display mode indicator (P/R/H)
        auto modeInd                               = btn.translated(0, 37).withHeight(7);
        static constexpr const char* kModeLabels[] = {"P", "R", "H"};
        static const juce::Colour kModeColours[]   = {
            MixCoachTheme::meterYellow(), MixCoachTheme::accentCyanBright(), MixCoachTheme::accentCyanBright().withAlpha(0.7f)};
        int modeIdx          = (int)displayMode_;
        juce::Colour modeCol = kModeColours[modeIdx].withAlpha(0.55f);
        g.setFont(juce::Font(juce::FontOptions(5.5f)).boldened());
        g.setColour(modeCol);
        g.drawText(kModeLabels[modeIdx], modeInd, juce::Justification::centred);
        auto modePill = modeInd.expanded(3.0f, 1.0f).withHeight(7);
        g.setColour(modeCol.withAlpha(0.08f));
        g.fillRoundedRectangle(modePill, 2.0f);

        // Reference Toggle Button
        if (referenceButton_.isEmpty()) return;

        auto refBtn = referenceButton_.toFloat();
        bool on     = referenceEnabled_;
        bool hover  = mouseOverRefButton_;

        auto bgCol  = on ? juce::Colour(0x44FFD700) : juce::Colour(0x1A888888);
        auto fgCol  = on ? juce::Colour(0xCCFFD700) : juce::Colour(0x55999999);
        auto dotCol = on ? MixCoachTheme::meterYellow() : juce::Colour(0x66999999);

        if (hover) {
            bgCol  = bgCol.brighter(0.6f);
            fgCol  = fgCol.brighter(0.4f);
            dotCol = dotCol.brighter(0.3f);
        }

        if (hover) {
            g.setColour(on ? juce::Colour(0x22FFD700) : juce::Colour(0x11888888));
            g.fillRoundedRectangle(refBtn.expanded(2.0f, 2.0f), 4.0f);
        }

        g.setColour(bgCol);
        g.fillRoundedRectangle(refBtn, 3.0f);
        g.setColour(fgCol);
        g.drawRoundedRectangle(refBtn, 3.0f, hover ? 1.2f : 0.7f);

        float dotR = hover ? 3.5f : 3.0f;
        float dotX = refBtn.getX() + dotR + 2.0f;
        float dotY = refBtn.getCentreY();
        g.setColour(dotCol);
        g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);

        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)).boldened());
        g.setColour(fgCol);
        g.drawText("REF", refBtn, juce::Justification::centred);

        if (hover && !getLocalBounds().isEmpty()) {
            juce::String tipText = on ? juce::CharPointer_UTF8("Hide reference curve  ")
                                      : juce::CharPointer_UTF8("Show reference curve  ");

            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
            float tipW = (float)tipText.length() * 5.0f + 8.0f;
            float tipH = 14.0f;
            float tipX = refBtn.getX() - tipW - 2.0f;
            float tipY = refBtn.getCentreY() - tipH * 0.5f;

            if (tipX < 2.0f) tipX = refBtn.getRight() + 2.0f;

            auto tipRect = juce::Rectangle<float>(tipX, tipY, tipW, tipH);

            g.setColour(juce::Colour(0xEE0A0E1A));
            g.fillRoundedRectangle(tipRect, 3.0f);
            g.setColour(juce::Colour(0x55FFD700));
            g.drawRoundedRectangle(tipRect, 3.0f, 0.6f);

            g.setColour(juce::Colour(0xEEFFD700));
            g.drawText(tipText, tipRect, juce::Justification::centred);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Waterfall 3D Spectrogram
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::drawWaterfall(juce::Graphics& g, juce::Rectangle<float> plot) const
    {
        if (!waterfallEnabled_ || waterfallCount_ == 0) return;

        const float sliceH = plot.getHeight() / (float)kWaterfallRows;
        if (sliceH < 0.5f) return;

        const auto cyanTop = MixCoachTheme::accentCyanBright();

        struct BandRect
        {
            float x;
            float w;
        };

        BandRect bandRects[kNumRtaBands];
        for (int b = 0; b < kNumRtaBands; ++b) {
            const auto& band = bands_[(size_t)b];
            const float x0   = freqToX(band.lowHz, plot);
            const float x1   = freqToX(band.highHz, plot);
            bandRects[b].w   = juce::jmax(1.0f, (x1 - x0) * 0.92f);
            bandRects[b].x   = (x0 + x1) * 0.5f - bandRects[b].w * 0.5f;
        }

        int startIdx;
        if (waterfallCount_ < kWaterfallRows) startIdx = 0;
        else
            startIdx = waterfallWritePos_;

        for (int row = 0; row < waterfallCount_; ++row) {
            int idx           = (startIdx + row) % kWaterfallRows;
            const auto& slice = waterfall_[idx];
            if (!slice.valid) continue;

            float age        = 1.0f - (float)row / (float)waterfallCount_;
            float sliceAlpha = 0.55f * (1.0f - age * age) + 0.02f;
            if (sliceAlpha < 0.01f) continue;

            float sliceY = plot.getY() + row * sliceH;
            if (sliceY + sliceH > plot.getBottom()) continue;

            for (int b = 0; b < kNumRtaBands; ++b) {
                float level    = slice.levels[(size_t)b];
                float barAlpha = sliceAlpha * (0.25f + 0.75f * level);
                if (barAlpha < 0.005f) continue;

                g.setColour(cyanTop.withAlpha(barAlpha));
                g.fillRect(bandRects[b].x, sliceY, bandRects[b].w, juce::jmax(1.0f, sliceH));
            }
        }

        auto fadeZone = plot.withHeight(plot.getHeight() * 0.25f);
        juce::ColourGradient fadeGrad(MixCoachTheme::bgPanel().withAlpha(0.65f),
                                      juce::Point<float>(plot.getX(), fadeZone.getY()),
                                      juce::Colour(0x00000000),
                                      juce::Point<float>(plot.getX(), fadeZone.getBottom()),
                                      false);
        g.setGradientFill(fadeGrad);
        g.fillRect(fadeZone);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Reference Overlay
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::drawReferenceOverlay(juce::Graphics& g, juce::Rectangle<float> plot) const
    {
        if (!referenceEnabled_ || referenceCurve_.empty() || bands_.empty()) return;

        juce::Path refPath;
        bool first = true;

        for (int i = 0; i < kNumRtaBands; ++i) {
            const float x    = freqToX(bands_[(size_t)i].centerHz, plot);
            const float norm = referenceCurve_[(size_t)i];
            const float y    = plot.getBottom() - norm * plot.getHeight();

            if (first) {
                refPath.startNewSubPath(x, y);
                first = false;
            }
            else {
                refPath.lineTo(x, y);
            }
        }

        {
            juce::Path filledPath(refPath);
            filledPath.lineTo(freqToX(kMaxFreq, plot), plot.getBottom());
            filledPath.lineTo(freqToX(0.0f, plot), plot.getBottom());
            filledPath.closeSubPath();

            g.setColour(juce::Colour(0x22FFD700));
            g.fillPath(filledPath);
        }

        g.setColour(juce::Colour(0xBBFFD700));
        g.strokePath(refPath, juce::PathStrokeType(1.5f));
        g.setColour(juce::Colour(0x22FFD700));
        g.strokePath(refPath, juce::PathStrokeType(4.0f));

        // -14 LUFS target line
        {
            float targetNorm = dbToDisplayNorm(-14.0f);
            float targetY    = plot.getBottom() - targetNorm * plot.getHeight();
            g.setColour(juce::Colour(0x44FFD700));
            float x       = plot.getX();
            float dashLen = 4.0f;
            float gapLen  = 3.0f;
            float xEnd    = plot.getRight();
            while (x < xEnd) {
                float endX = juce::jmin(x + dashLen, xEnd);
                g.drawHorizontalLine(juce::roundToInt(targetY), x, endX);
                x = endX + gapLen;
            }

            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizePico)));
            g.setColour(juce::Colour(0x88FFD700));
            g.drawText("-14 LUFS",
                       juce::Rectangle<float>(plot.getX() + 4, targetY - 8, 38, 8),
                       juce::Justification::centredLeft);
        }

        // REF badge
        {
            auto badge = juce::Rectangle<float>(plot.getRight() - 36, plot.getY() + 2, 32, 12);
            g.setColour(juce::Colour(0x44FFD700));
            g.fillRoundedRectangle(badge, 2.0f);
            g.setColour(juce::Colour(0xCCFFD700));
            g.drawRoundedRectangle(badge, 2.0f, 0.8f);
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened());
            g.setColour(juce::Colour(0xCCFFD700));
            g.drawText("REF", badge, juce::Justification::centred);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Diagnostic Overlay — Destaca bandas de frecuencia con problemas
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::drawDiagnosticOverlay(juce::Graphics& g, juce::Rectangle<float> plot) const
    {
        if (activeDiagnostics_.empty()) return;

        for (const auto& diag : activeDiagnostics_) {
            float x0 = freqToX(diag.lowFreqHz, plot);
            float x1 = freqToX(diag.highFreqHz, plot);
            if (x1 <= x0) x1 = x0 + 2.0f;

            auto bandRect = juce::Rectangle<float>(x0, plot.getY(), x1 - x0, plot.getHeight());

            juce::Colour diagCol = diag.getDisplayColour();
            float alpha          = diag.isPraise ? 0.15f : (diag.isCritical ? 0.25f : 0.12f + 0.10f * diag.severity);

            juce::ColourGradient fillGrad(diagCol.withAlpha(alpha),
                                          juce::Point<float>(bandRect.getCentreX(), bandRect.getY()),
                                          diagCol.withAlpha(0.0f),
                                          juce::Point<float>(bandRect.getCentreX(), bandRect.getBottom()),
                                          false);
            g.setGradientFill(fillGrad);
            g.fillRect(bandRect);

            if (diag.isCritical) {
                g.setColour(diagCol.withAlpha(0.60f));
                g.fillRect(bandRect.getX(), bandRect.getY(), bandRect.getWidth(), 2.0f);

                auto glowRect = bandRect.withHeight(8.0f);
                juce::ColourGradient glowGrad(diagCol.withAlpha(0.20f),
                                              juce::Point<float>(glowRect.getCentreX(), glowRect.getY()),
                                              diagCol.withAlpha(0.0f),
                                              juce::Point<float>(glowRect.getCentreX(), glowRect.getBottom()),
                                              false);
                g.setGradientFill(glowGrad);
                g.fillRect(glowRect);
            }

            g.setColour(diagCol.withAlpha(diag.isCritical ? 0.50f : 0.25f));
            g.drawVerticalLine((int)x0, plot.getY(), plot.getBottom());
            g.drawVerticalLine((int)x1, plot.getY(), plot.getBottom());

            if (diag.trackName.isNotEmpty()) {
                juce::String label = diag.trackName;
                if (diag.trackRole.isNotEmpty()) label += " (" + diag.trackRole + ")";

                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened());
                g.setColour(diagCol.withAlpha(0.85f));

                float labelW   = juce::jmin((float)label.length() * 5.0f + 6.0f, bandRect.getWidth());
                auto labelRect = juce::Rectangle<float>(x0 + 2.0f, plot.getY() + 2.0f, labelW, 12.0f);

                g.setColour(MixCoachTheme::bgCanvas().withAlpha(0.75f));
                g.fillRoundedRectangle(labelRect.expanded(2.0f, 1.0f), 2.0f);
                g.setColour(diagCol.withAlpha(0.30f));
                g.drawRoundedRectangle(labelRect.expanded(2.0f, 1.0f), 2.0f, 0.5f);

                g.setColour(diagCol.withAlpha(0.90f));
                g.drawText(label, labelRect, juce::Justification::centredLeft);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Frequency Markers — Puntos glow donde el coach menciona una frecuencia
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::drawFrequencyMarkers(juce::Graphics& g, juce::Rectangle<float> plot) const
    {
        if (frequencyMarkers_.empty()) return;

        int64_t nowMs = static_cast<int64_t>(juce::Time::getMillisecondCounter());

        for (const auto& marker : frequencyMarkers_) {
            if (marker.isExpired(nowMs)) continue;

            float alpha = marker.getAlpha(nowMs);
            if (alpha < 0.01f) continue;

            float x = freqToX(marker.frequencyHz, plot);
            if (x < plot.getX() || x > plot.getRight()) continue;

            juce::Colour markerCol;
            if (marker.isPraise) markerCol = MixCoachTheme::success();
            else if (marker.isCritical)
                markerCol = MixCoachTheme::error();
            else if (marker.severity > 0.6f)
                markerCol = MixCoachTheme::meterOrange();
            else
                markerCol = MixCoachTheme::accentCyanBright();

            float glowRadius = marker.isCritical ? 20.0f : 14.0f;
            g.setColour(markerCol.withAlpha(0.08f * alpha));
            g.fillEllipse(x - glowRadius, plot.getY() - glowRadius * 0.3f, glowRadius * 2.0f, glowRadius * 2.0f * 0.6f);

            float midRadius = marker.isCritical ? 12.0f : 9.0f;
            g.setColour(markerCol.withAlpha(0.20f * alpha));
            g.fillEllipse(x - midRadius, plot.getY() - midRadius * 0.3f, midRadius * 2.0f, midRadius * 2.0f * 0.6f);

            {
                constexpr float kDashLen = 4.0f;
                constexpr float kGapLen  = 3.0f;
                float y                  = plot.getY() + 8.0f;
                while (y < plot.getBottom()) {
                    float endY = juce::jmin(y + kDashLen, plot.getBottom());
                    g.setColour(markerCol.withAlpha(0.15f * alpha));
                    g.drawVerticalLine(juce::roundToInt(x), y, endY);
                    y = endY + kGapLen;
                }
            }

            float dotR = marker.isCritical ? 5.0f : 4.0f;
            float dotY = plot.getY() + 6.0f;
            g.setColour(markerCol.withAlpha(0.90f * alpha));
            g.fillEllipse(x - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);

            float innerR = dotR * 0.45f;
            g.setColour(juce::Colours::white.withAlpha(0.60f * alpha));
            g.fillEllipse(x - innerR, dotY - innerR, innerR * 2.0f, innerR * 2.0f);

            {
                juce::String displayText = marker.label;
                if (marker.description.isNotEmpty()) displayText += " " + marker.description;

                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
                float textW  = juce::jmin((float)displayText.length() * 5.5f + 8.0f, 180.0f);
                float textH  = 14.0f;
                float labelX = x - textW * 0.5f;
                float labelY = dotY + dotR + 3.0f;

                if (labelX < plot.getX()) labelX = plot.getX();
                if (labelX + textW > plot.getRight()) labelX = plot.getRight() - textW;

                auto labelRect = juce::Rectangle<float>(labelX, labelY, textW, textH);

                g.setColour(juce::Colour(0xDD0A0E1A));
                g.fillRoundedRectangle(labelRect, 3.0f);
                g.setColour(markerCol.withAlpha(0.25f * alpha));
                g.drawRoundedRectangle(labelRect, 3.0f, 0.6f);

                g.setColour(markerCol.withAlpha(0.90f * alpha));
                g.drawText(displayText, labelRect, juce::Justification::centred);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Hover Cursor — muestra frecuencia y dB exacto al pasar el mouse
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::drawHoverCursor(juce::Graphics& g, juce::Rectangle<float> plot) const
    {
        if (!mouseOverPlot_) return;

        const float x = mousePos_.x;
        const float y = mousePos_.y;
        juce::ignoreUnused(y);

        g.setColour(MixCoachTheme::accentCyanBright().withAlpha(0.08f));
        g.fillRect(x - 2.0f, plot.getY(), 5.0f, plot.getHeight());

        g.setColour(MixCoachTheme::accentCyanBright().withAlpha(0.40f));
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());

        float bandY = plot.getBottom() - hoverBandLevel_ * plot.getHeight();
        if (bandY >= plot.getY() && bandY <= plot.getBottom()) {
            g.setColour(MixCoachTheme::accentCyanBright().withAlpha(0.80f));
            g.fillEllipse(x - 2.5f, bandY - 2.5f, 5.0f, 5.0f);

            g.setColour(MixCoachTheme::accentCyanBright().withAlpha(0.15f));
            g.fillEllipse(x - 5.0f, bandY - 5.0f, 10.0f, 10.0f);
        }

        juce::String freqStr;
        if (hoverFreqHz_ >= 10000.0f) freqStr = juce::String(hoverFreqHz_ / 1000.0f, 1) + " kHz";
        else if (hoverFreqHz_ >= 1000.0f)
            freqStr = juce::String(hoverFreqHz_ / 1000.0f, 2) + " kHz";
        else
            freqStr = juce::String((int)hoverFreqHz_) + " Hz";

        juce::String dbStr = juce::String(hoverDbLevel_, 1) + " dB";

        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall - 1.0f)).boldened());
        float textW1 = (float)freqStr.length() * 6.0f;
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
        float textW2   = (float)dbStr.length() * 5.5f;
        float maxTextW = juce::jmax(textW1, textW2);

        float tooltipW = maxTextW + 14.0f;
        float tooltipH = 32.0f;

        float tooltipX = x + 12.0f;
        float tooltipY = mousePos_.y - 16.0f;
        if (tooltipX + tooltipW > plot.getRight()) tooltipX = x - tooltipW - 10.0f;
        if (tooltipY + tooltipH > plot.getBottom()) tooltipY = plot.getBottom() - tooltipH - 2.0f;
        if (tooltipY < plot.getY() + 2.0f) tooltipY = plot.getY() + 2.0f;

        auto tooltipRect = juce::Rectangle<float>(tooltipX, tooltipY, tooltipW, tooltipH);

        g.setColour(juce::Colour(0xDD0A0E1A));
        g.fillRoundedRectangle(tooltipRect, 3.0f);
        g.setColour(MixCoachTheme::accentCyanBright().withAlpha(0.25f));
        g.drawRoundedRectangle(tooltipRect, 3.0f, 0.8f);

        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall - 1.0f)).boldened());
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.drawText(freqStr, tooltipRect.reduced(5, 0).withHeight(16.0f), juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
        juce::Colour dbCol = (hoverDbLevel_ > -6.0f) ? MixCoachTheme::meterYellow() : MixCoachTheme::textMuted();
        g.setColour(dbCol.withAlpha(0.90f));
        g.drawText(dbStr, tooltipRect.reduced(5, 0).withTrimmedTop(16.0f), juce::Justification::centredLeft);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Centroid Marker — Muestra el centroide espectral actual vs esperado
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::drawCentroidMarker(juce::Graphics& g, juce::Rectangle<float> plot) const
    {
        if (!centroidInfo_.valid() || !layout_.valid) return;

        const float actualX   = freqToX(centroidInfo_.actualHz, plot);
        const float expectedX = freqToX(centroidInfo_.expectedHz, plot);

        const auto cyan = MixCoachTheme::accentCyanBright();
        const auto gold = MixCoachTheme::meterYellow();

        {
            constexpr float kDashLen = 6.0f;
            constexpr float kGapLen  = 4.0f;
            float y                  = plot.getY();
            while (y < plot.getBottom()) {
                float endY = juce::jmin(y + kDashLen, plot.getBottom());
                g.setColour(cyan.withAlpha(0.50f));
                g.drawVerticalLine(juce::roundToInt(actualX), y, endY);
                y = endY + kGapLen;
            }
        }

        g.setColour(cyan.withAlpha(0.06f));
        g.fillRect(actualX - 3.0f, plot.getY(), 7.0f, plot.getHeight());

        {
            constexpr float kDotLen = 3.0f;
            constexpr float kGapLen = 5.0f;
            float y                 = plot.getY();
            while (y < plot.getBottom()) {
                float endY = juce::jmin(y + kDotLen, plot.getBottom());
                g.setColour(gold.withAlpha(0.35f));
                g.drawVerticalLine(juce::roundToInt(expectedX), y, endY);
                y = endY + kGapLen;
            }
        }

        {
            const float topY = plot.getY() + 4.0f;
            g.setColour(cyan.withAlpha(0.85f));
            juce::Path diamond;
            diamond.addTriangle(actualX, topY - 5.0f, actualX - 5.0f, topY + 2.0f, actualX + 5.0f, topY + 2.0f);
            g.fillPath(diamond);
            g.setColour(cyan.withAlpha(0.15f));
            g.fillEllipse(actualX - 7.0f, topY - 8.0f, 14.0f, 14.0f);
        }

        {
            const float topY = plot.getY() + 4.0f;
            g.setColour(gold.withAlpha(0.65f));
            juce::Path diamond;
            diamond.addTriangle(expectedX, topY - 5.0f, expectedX - 5.0f, topY + 2.0f, expectedX + 5.0f, topY + 2.0f);
            g.fillPath(diamond);
            g.setColour(gold.withAlpha(0.10f));
            g.fillEllipse(expectedX - 6.0f, topY - 7.0f, 12.0f, 12.0f);
        }

        {
            auto formatFreqVal = [](float hz) -> juce::String {
                if (hz >= 1000.0f) return juce::String(hz / 1000.0f, 1) + "k";
                return juce::String((int)hz);
            };

            juce::String badgeText =
                "C: " + formatFreqVal(centroidInfo_.actualHz) + " | E: " + formatFreqVal(centroidInfo_.expectedHz);
            if (centroidInfo_.genre.isNotEmpty()) badgeText += " [" + centroidInfo_.genre + "]";

            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
            float textW  = (float)badgeText.length() * 5.5f + 10.0f;
            float textH  = 14.0f;
            float badgeX = plot.getX() + 4.0f;
            float badgeY = plot.getY() + plot.getHeight() - textH - 4.0f;

            auto badgeRect = juce::Rectangle<float>(badgeX, badgeY, textW, textH);

            g.setColour(juce::Colour(0xCC0A0E1A));
            g.fillRoundedRectangle(badgeRect, 3.0f);
            g.setColour(cyan.withAlpha(0.20f));
            g.drawRoundedRectangle(badgeRect, 3.0f, 0.6f);

            g.setColour(cyan.withAlpha(0.90f));
            g.drawText(badgeText, badgeRect, juce::Justification::centred);
        }
    }

} // namespace mixcoach
