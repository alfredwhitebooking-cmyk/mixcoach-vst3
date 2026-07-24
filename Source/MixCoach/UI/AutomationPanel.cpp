#include "AutomationPanel.h"
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor
    // ═══════════════════════════════════════════════════════════════════════════
    AutomationPanel::AutomationPanel()
    {
        setOpaque(false);
        setVisible(false);
        startTimerHz(30); // 30fps for smooth animations
    }


    // ═══════════════════════════════════════════════════════════════════════════
    //  visibilityChanged — Pausa timer cuando el plugin no es visible
    // ═══════════════════════════════════════════════════════════════════════════

    void AutomationPanel::visibilityChanged()
    {
        if (isShowing()) {
            startTimerHz(30);
        } else {
            stopTimer();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setTimelineData — Actualiza todos los datos y dispara animación
    // ═══════════════════════════════════════════════════════════════════════════
    void AutomationPanel::setTimelineData(const AutomationTimelineData& data)
    {
        data_ = data;
        animProgress_ = 0.0f;
        hoveredSection_ = -1;
        lastUpdateMs_ = juce::Time::getMillisecondCounter();
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateLiveLUFS — Actualiza valores en tiempo real (timer callback)
    // ═══════════════════════════════════════════════════════════════════════════
    void AutomationPanel::updateLiveLUFS(float momentaryLUFS, float shortTermLUFS, float integratedLUFS)
    {
        liveMomentaryLUFS_ = momentaryLUFS;
        liveShortTermLUFS_ = shortTermLUFS;
        liveIntegratedLUFS_ = integratedLUFS;
        lastUpdateMs_ = juce::Time::getMillisecondCounter();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  clear — Reset
    // ═══════════════════════════════════════════════════════════════════════════
    void AutomationPanel::clear()
    {
        data_ = {};
        animProgress_ = 1.0f;
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  timerCallback — Animación de entrada + actualización live LUFS
    // ═══════════════════════════════════════════════════════════════════════════
    void AutomationPanel::timerCallback()
    {
        bool needsRepaint = false;

        // ─── Animación de entrada ──────────────────────────────────────────
        if (animProgress_ < 1.0f) {
            animProgress_ = juce::jmin(1.0f, animProgress_ + (1.0f / (float)kAnimFrames));
            needsRepaint = true;
        }

        // ─── Pulse effect on live meters (every 30 frames ≈ 1s) ────────────
        if (needsRepaint && isVisible())
            repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  resized
    // ═══════════════════════════════════════════════════════════════════════════
    void AutomationPanel::resized()
    {
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Panel completo con timeline LUFS + section cards + stats
    // ═══════════════════════════════════════════════════════════════════════════
    void AutomationPanel::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        if (bounds.isEmpty()) return;

        float animAlpha = easeOutCubic(animProgress_);
        float animOffset = (1.0f - animAlpha) * 12.0f;

        // ─── Glass background ──────────────────────────────────────────────
        MixCoachTheme::fillGlassPanel(g, bounds, MixCoachTheme::cornerRadius_medium);

        g.saveState();
        if (animAlpha < 1.0f) {
            g.setOpacity(animAlpha);
            g.addTransform(juce::AffineTransform::translation(0.0f, animOffset));
        }

        // ═══════════════════════════════════════════════════════════════════
        //  1. HEADER — "AUTOMATION · LUFS Timeline" with accent line
        // ═══════════════════════════════════════════════════════════════════
        auto headerArea = bounds.removeFromTop(30.0f).reduced(kPadding, 0);
        g.setColour(MixCoachTheme::accentGlow());
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSectionHeader)).boldened());
        g.drawText(juce::CharPointer_UTF8("\\xE2\\x9A\\x99\\xEF\\xB8\\x8F AUTOMATION \\u00B7 LUFS Timeline"),
                   headerArea, juce::Justification::centredLeft);

        // Accent underline
        auto lineY = headerArea.getBottom() + 2.0f;
        g.setColour(MixCoachTheme::accent().withAlpha(0.15f));
        g.drawHorizontalLine((int)lineY, bounds.getX() + kPadding, bounds.getRight() - kPadding);

        bounds.removeFromTop(6.0f);

        if (!data_.hasData()) {
            // ─── Empty state ──────────────────────────────────────────────
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)));
            g.drawText("Esperando datos de automatizaci\\u00F3n...",
                       bounds.reduced(kPadding), juce::Justification::centred);
            g.restoreState();
            return;
        }

        // ═══════════════════════════════════════════════════════════════════
        //  2. LUFS TIMELINE — Horizontal bar with section segments
        // ═══════════════════════════════════════════════════════════════════
        auto timelineArea = bounds.removeFromTop(kTimelineHeight).reduced(kPadding, 0);
        drawTimeline(g, timelineArea);

        bounds.removeFromTop(8.0f);

        // ═══════════════════════════════════════════════════════════════════
        //  3. STATS ROW — Integrated LUFS, Dynamic Range, Crest, Target
        // ═══════════════════════════════════════════════════════════════════
        auto statsArea = bounds.removeFromTop(28.0f).reduced(kPadding, 0);
        drawStatsRow(g, statsArea);

        bounds.removeFromTop(6.0f);

        // ═══════════════════════════════════════════════════════════════════
        //  4. SECTION CARDS — One per section with detailed metrics
        // ═══════════════════════════════════════════════════════════════════
        int numSections = data_.getNumSections();
        float availableH = bounds.getHeight() - kPadding;
        int maxVisibleSections = juce::jmax(1, (int)(availableH / kSectionCardH));

        for (int i = 0; i < juce::jmin(numSections, maxVisibleSections); ++i) {
            auto cardArea = bounds.removeFromTop(kSectionCardH).reduced(kPadding, 1);
            drawSectionCard(g, cardArea, data_.sections[i], i);
            bounds.removeFromTop(2.0f);
        }

        g.restoreState();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawTimeline — Timeline horizontal con LUFS por sección
    // ═══════════════════════════════════════════════════════════════════════════
    void AutomationPanel::drawTimeline(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        if (data_.getNumSections() == 0) return;

        // ─── Background track ──────────────────────────────────────────────
        g.setColour(MixCoachTheme::bgDarker().withAlpha(0.5f));
        g.fillRoundedRectangle(bounds, 6.0f);

        // ─── Target LUFS line (dashed) ────────────────────────────────────
        float targetY = lufsToY(data_.targetIntegratedLUFS, bounds.getY(), bounds.getHeight());
        g.setColour(MixCoachTheme::success().withAlpha(0.3f));
        for (float x = bounds.getX(); x < bounds.getRight(); x += 8.0f) {
            g.fillRect(x, targetY, 4.0f, 1.0f);
        }
        // Target label
        g.setFont(juce::Font(juce::FontOptions(7.0f)));
        g.setColour(MixCoachTheme::success().withAlpha(0.5f));
        g.drawText(juce::String(data_.targetIntegratedLUFS, 0) + " LUFS",
                   juce::Rectangle<float>(bounds.getRight() - 42.0f, targetY - 8.0f, 40.0f, 8.0f),
                   juce::Justification::centredRight);

        // ─── Timeline grid lines (every 6 LUFS) ────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(6.5f)));
        for (float lufs = kMinLUFS; lufs <= kMaxLUFS; lufs += 6.0f) {
            float gy = lufsToY(lufs, bounds.getY(), bounds.getHeight());
            g.setColour(MixCoachTheme::border().withAlpha(0.08f));
            g.drawHorizontalLine((int)gy, bounds.getX(), bounds.getRight());
            if (lufs > kMinLUFS && lufs < kMaxLUFS) {
                g.setColour(MixCoachTheme::textMuted().withAlpha(0.2f));
                g.drawText(juce::String((int)lufs) + " LUFS",
                           juce::Rectangle<float>(bounds.getX() + 2.0f, gy - 5.0f, 30.0f, 8.0f),
                           juce::Justification::centredLeft);
            }
        }

        // ─── Calculate total beats for proportional widths ─────────────────
        float totalBeats = 0.0f;
        for (auto& s : data_.sections)
            totalBeats += s.durationBeats;
        if (totalBeats <= 0.0f) totalBeats = 1.0f;

        float sectionX = bounds.getX() + 4.0f;
        float sectionW = bounds.getWidth() - 8.0f;

        // ─── Draw each section as a colored segment ────────────────────────
        for (int i = 0; i < data_.getNumSections(); ++i) {
            auto& sec = data_.sections[i];
            float segW = (sec.durationBeats / totalBeats) * sectionW;
            auto segBounds = juce::Rectangle<float>(sectionX, bounds.getY() + 4.0f,
                                                     segW, bounds.getHeight() - 8.0f);

            // LUFS fill: from bottom up to the integrated LUFS level
            float lufsTop = lufsToY(sec.integratedLUFS, segBounds.getY(), segBounds.getHeight());
            auto fillRect = juce::Rectangle<float>(segBounds.getX(), lufsTop,
                                                    segBounds.getWidth(), segBounds.getBottom() - lufsTop);

            // Colour based on how close to target
            float diffToTarget = std::abs(sec.integratedLUFS - data_.targetIntegratedLUFS);
            juce::Colour sectionColour;
            if (diffToTarget < 1.0f)      sectionColour = MixCoachTheme::success();
            else if (diffToTarget < 3.0f) sectionColour = MixCoachTheme::warning();
            else                          sectionColour = MixCoachTheme::error();

            // Fill with gradient
            auto fillGrad = juce::ColourGradient(
                sectionColour.withAlpha(0.5f), fillRect.getX(), fillRect.getY(),
                sectionColour.withAlpha(0.15f), fillRect.getRight(), fillRect.getBottom(), false);
            g.setGradientFill(fillGrad);
            g.fillRoundedRectangle(fillRect, 2.0f);

            // Section border
            g.setColour(sectionColour.withAlpha(hoveredSection_ == i ? 0.6f : 0.25f));
            g.drawRoundedRectangle(segBounds, 2.0f, hoveredSection_ == i ? 1.2f : 0.5f);

            // Section label (rotated or abbreviated)
            g.setFont(juce::Font(juce::FontOptions(7.0f)).boldened());
            g.setColour(MixCoachTheme::textBright().withAlpha(0.7f));
            juce::String label = sec.name.substring(0, 4); // Abbreviate to 4 chars
            g.drawText(label, segBounds.reduced(1, 0), juce::Justification::centredBottom);

            // LUFS value
            g.setFont(juce::Font(juce::FontOptions(6.5f)));
            g.setColour(MixCoachTheme::textDim().withAlpha(0.5f));
            g.drawText(juce::String(sec.integratedLUFS, 1),
                       juce::Rectangle<float>(segBounds.getX(), lufsTop - 8.0f,
                                               segBounds.getWidth(), 8.0f),
                       juce::Justification::centred);

            sectionX += segW;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawStatsRow — Fila de estadísticas globales
    // ═══════════════════════════════════════════════════════════════════════════
    void AutomationPanel::drawStatsRow(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        struct StatItem {
            const char* label;
            float value;
            const char* unit;
            juce::Colour colour;
        };

        juce::Colour integratedCol = (std::abs(liveIntegratedLUFS_ - data_.targetIntegratedLUFS) < 1.0f)
            ? MixCoachTheme::success() : MixCoachTheme::warning();

        StatItem stats[] = {
            { "Integrated", data_.overallIntegratedLUFS, "LUFS", integratedCol },
            { "Range",      data_.dynamicRange,          "dB",   MixCoachTheme::accentCyan() },
            { "Crest",      data_.crestFactor,           "dB",   MixCoachTheme::accent() },
            { "Target",     data_.targetIntegratedLUFS,   "LUFS", MixCoachTheme::success() },
        };

        int numStats = sizeof(stats) / sizeof(stats[0]);
        float statW = bounds.getWidth() / (float)numStats;

        for (int i = 0; i < numStats; ++i) {
            auto area = juce::Rectangle<float>(bounds.getX() + (float)i * statW,
                                                bounds.getY(), statW - 4.0f, bounds.getHeight());

            // Background pill
            g.setColour(stats[i].colour.withAlpha(0.06f));
            g.fillRoundedRectangle(area, 4.0f);

            // Label
            g.setFont(juce::Font(juce::FontOptions(6.5f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.drawText(stats[i].label, area.removeFromTop(10.0f).reduced(2, 0),
                       juce::Justification::centred);

            // Value + unit
            g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
            g.setColour(stats[i].colour);
            juce::String valText = juce::String(stats[i].value, 1) + " " + juce::String(stats[i].unit);
            g.drawText(valText, area.reduced(2, 0), juce::Justification::centred);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawSectionCard — Tarjeta individual de sección con métricas
    // ═══════════════════════════════════════════════════════════════════════════
    void AutomationPanel::drawSectionCard(juce::Graphics& g, juce::Rectangle<float> bounds,
                                           const AutomationSection& section, int index)
    {
        float cr = 6.0f;

        // ─── Sombra ────────────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.12f));
        g.fillRoundedRectangle(bounds.expanded(0.5f, 1.0f), cr);

        // ─── Section colour accent ─────────────────────────────────────────
        float diffToTarget = std::abs(section.integratedLUFS - data_.targetIntegratedLUFS);
        juce::Colour accentCol;
        if (diffToTarget < 1.0f)      accentCol = MixCoachTheme::success();
        else if (diffToTarget < 3.0f) accentCol = MixCoachTheme::warning();
        else                          accentCol = MixCoachTheme::error();

        // ─── Fondo glass ───────────────────────────────────────────────────
        if (hoveredSection_ == index) {
            g.setColour(juce::Colour(0x221A0A2E));
            g.fillRoundedRectangle(bounds, cr);
        }
        g.setColour(MixCoachTheme::bgPanel().withAlpha(0.75f));
        g.fillRoundedRectangle(bounds, cr);

        // Accent bar left
        g.setColour(accentCol.withAlpha(0.4f));
        g.fillRoundedRectangle(
            juce::Rectangle<float>(bounds.getX(), bounds.getY() + 4.0f,
                                    3.0f, bounds.getHeight() - 8.0f), 1.5f);

        // ─── Border ────────────────────────────────────────────────────────
        g.setColour(accentCol.withAlpha(0.12f));
        g.drawRoundedRectangle(bounds, cr, 0.4f);

        auto area = bounds.reduced(10.0f, 4.0f);

        // ─── Section name (left) ───────────────────────────────────────────
        auto nameArea = area.removeFromLeft(70.0f);
        g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText(section.name, nameArea, juce::Justification::centredLeft);

        // ─── Mini LUFS bar ──────────────────────────────────────────────────
        auto barArea = area.removeFromLeft(juce::jmin(area.getWidth() * 0.35f, 60.0f)).reduced(0, 6);
        float barNorm = (section.integratedLUFS - kMinLUFS) / kLUFSRange;
        g.setColour(MixCoachTheme::bgDarker());
        g.fillRoundedRectangle(barArea, 2.0f);
        auto fillBar = barArea.withWidth(barArea.getWidth() * juce::jlimit(0.0f, 1.0f, barNorm));
        g.setColour(accentCol.withAlpha(0.5f));
        g.fillRoundedRectangle(fillBar, 2.0f);

        // LUFS value on bar
        g.setFont(juce::Font(juce::FontOptions(7.0f)).boldened());
        g.setColour(accentCol);
        g.drawText(juce::String(section.integratedLUFS, 1) + " LUFS",
                   barArea, juce::Justification::centredRight);

        // ─── Metrics row (crest, correlation, short-term) ──────────────────
        auto metricsArea = area.reduced(4, 0);
        struct Metric {
            const char* label;
            float value;
            const char* unit;
        };
        Metric metrics[] = {
            { "Crest", section.crestDb, "dB" },
            { "Corr",  section.correlation * 100.0f, "%" },
            { "ST",    section.shortTermLUFS, "LUFS" },
        };

        float metricW = metricsArea.getWidth() / 3.0f;
        for (int m = 0; m < 3; ++m) {
            auto mArea = juce::Rectangle<float>(metricsArea.getX() + (float)m * metricW,
                                                  metricsArea.getY(), metricW - 2.0f, metricsArea.getHeight());
            g.setFont(juce::Font(juce::FontOptions(6.5f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.drawText(metrics[m].label, mArea.removeFromTop(8.0f), juce::Justification::centred);

            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.setColour(MixCoachTheme::textDim());
            g.drawText(juce::String(metrics[m].value, 1) + " " + metrics[m].unit,
                       mArea, juce::Justification::centred);
        }
    }

} // namespace mixcoach
