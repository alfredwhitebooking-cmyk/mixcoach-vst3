#include "ProgressScreen.h"
#include "../engine/MixScore.h"
#include "../engine/SessionProgression.h"
#include "../ai/AiCoachAdapter.h"
#include "MixCoachTheme.h"
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor
    // ═══════════════════════════════════════════════════════════════════════════
    ProgressScreen::ProgressScreen()
    {
        startTimeMs_ = juce::Time::getMillisecondCounter();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setState
    // ═══════════════════════════════════════════════════════════════════════════
    void ProgressScreen::setState(State newState)
    {
        state_ = newState;
        recalcLayout();
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateFromEngine — Poblar datos desde el motor
    // ═══════════════════════════════════════════════════════════════════════════
    bool ProgressScreen::updateFromEngine(CoachEngine& engine)
    {
        // ─── Estado: Ready si hay datos mínimos ─────────────────────────────
        auto& progression = engine.getSessionProgression();
        bool hasGenre = engine.getSetupGenre().isNotEmpty();
        bool hasMessengers = (engine.getSharedData().getSlotRegistry().activeCount() > 0);
        bool hasSetup = (engine.hasReference() || hasGenre || hasMessengers);

        State newState = hasSetup ? State::Ready : State::Empty;
        bool changed = (state_ != newState);
        state_ = newState;

        // ─── SessionProgression data ───────────────────────────────────────
        phaseName_     = SessionProgression::phaseName(progression.currentPhase);
        phaseEmoji_    = SessionProgression::phaseEmoji(progression.currentPhase);
        phaseProgress_ = progression.getOverallProgress();

        // ─── Engine data ────────────────────────────────────────────────────
        genre_        = engine.getSetupGenre();
        activeTracks_ = engine.getSharedData().getSlotRegistry().activeCount();

        // ─── Correction history ─────────────────────────────────────────────
        {
            const auto& history = engine.getCorrectionHistory();
            correctionCount_ = (int)history.size();
            appliedCorrections_ = 0;
            for (const auto& entry : history) {
                if (entry.finalStatus == TrackRecommendation::Status::Applied
                    || entry.finalStatus == TrackRecommendation::Status::OverApplied
                    || entry.finalStatus == TrackRecommendation::Status::UnderApplied)
                    ++appliedCorrections_;
            }
        }

        // ─── MixScore ──────────────────────────────────────────────────────
        {
            auto& analyzer = engine.getAudioAnalyzer();
            MixScore score = MixScore::compute(engine, analyzer, genre_);
            mixScore_ = score.overall;
        }

        // ─── Multi-session history + streaks ───────────────────────────────
        sessionHistory_ = AiCoachAdapter::loadSessionHistory();
        sessionNumber_  = (int)sessionHistory_.size();
        computeStreaks();

        // ─── Build milestones from session progression ─────────────────────
        buildMilestones(progression);

        recalcLayout();
        repaint();

        return changed;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  computeStreaks — Calcula rachas desde el historial de sesiones
    // ═══════════════════════════════════════════════════════════════════════════
    void ProgressScreen::computeStreaks()
    {
        currentStreak_ = 0;
        bestStreak_    = 0;

        if (sessionHistory_.size() < 2) {
            currentStreak_ = (sessionHistory_.size() == 1) ? 1 : 0;
            bestStreak_    = currentStreak_;
            return;
        }

        // Find streak: consecutive sessions within ~36 hours
        int streak = 1;
        for (size_t i = sessionHistory_.size() - 1; i >= 1; --i) {
            const auto& curr = sessionHistory_[i];
            const auto& prev = sessionHistory_[i - 1];

            // TimestampUs in microseconds → 36 hours = 36 * 60 * 60 * 1,000,000
            int64_t diffUs = curr.timestampUs - prev.timestampUs;
            if (diffUs > 0 && diffUs < 36LL * 60 * 60 * 1000 * 1000) {
                streak++;
            } else {
                // Streak broken
                if (streak > bestStreak_)
                    bestStreak_ = streak;
                streak = 1;
            }
        }

        // Evaluate final streak
        currentStreak_ = streak;
        if (streak > bestStreak_)
            bestStreak_ = streak;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  buildMilestones — Construye hitos desde las fases de SessionProgression
    // ═══════════════════════════════════════════════════════════════════════════
    void ProgressScreen::buildMilestones(const SessionProgression& progression)
    {
        milestones_.clear();

        struct PhaseMilestone {
            SessionProgression::Phase phase;
            const char* name;
            const char* icon;
            const char* desc;
        };

        static const PhaseMilestone kPhaseMilestones[] = {
            { SessionProgression::Phase::Setup,         "Setup",           "[GEAR]", "Configuraci\xC3\xB3n inicial: g\xC3\xA9nero, modo, nombre" },
            { SessionProgression::Phase::LoadReference,  "Reference",        "[MUSIC]", "Carga de archivo o URL de referencia" },
            { SessionProgression::Phase::DeepAnalysis,   "Deep Analysis",    "[SEARCH]", "An\xC3\xA1lisis espectral y comparaci\xC3\xB3n" },
            { SessionProgression::Phase::GuidedCoaching, "Guided Coaching",  "\xF0\x9F\x92\xAC", "Recomendaciones activas del coach" },
            { SessionProgression::Phase::Refinement,     "Refinement",       "\xE2\x9C\xA8", "Refinamiento art\xC3\xADstico" },
            { SessionProgression::Phase::Report,         "Report",           "[EXPORT]", "Reporte de sesi\xC3\xB3n generado" },
            { SessionProgression::Phase::Memory,         "Saved",            "\xF0\x9F\x92\xBE", "Sesi\xC3\xB3n guardada al historial" },
        };

        int totalPhases = sizeof(kPhaseMilestones) / sizeof(kPhaseMilestones[0]);
        int currentIdx  = static_cast<int>(progression.currentPhase);

        for (int i = 0; i < totalPhases; ++i) {
            Milestone m;
            m.phaseName   = juce::String(kPhaseMilestones[i].name);
            m.icon        = juce::String(kPhaseMilestones[i].icon);
            m.description = juce::String(kPhaseMilestones[i].desc);
            m.completed   = progression.isPhaseComplete(kPhaseMilestones[i].phase);
            m.isCurrent   = (i == currentIdx);
            milestones_.push_back(std::move(m));
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  recalcLayout
    // ═══════════════════════════════════════════════════════════════════════════
    void ProgressScreen::recalcLayout()
    {
        auto area = getLocalBounds().reduced(24, 16);

        if (state_ != State::Ready) {
            headerBounds_ = streakBounds_ = milestonesBarBounds_ = timelineBounds_ = {};
            chartBounds_ = chartHeaderBounds_ = statsBounds_ = buttonBounds_ = {};
            return;
        }

        // Header: 56px
        headerBounds_ = area.removeFromTop(56);
        area.removeFromTop(8);

        // Streak card: ~80px
        streakBounds_ = area.removeFromTop(80);
        area.removeFromTop(8);

        // Milestones bar: horizontal compact bar ~56px
        milestonesBarBounds_ = area.removeFromTop(56);
        area.removeFromTop(8);

        // Timeline: flexible, ~32px per milestone (max 7 = 224px) + header
        int nMilestones = juce::jmin((int)milestones_.size(), 7);
        int timelineH = 24 + nMilestones * 28;
        timelineBounds_ = area.removeFromTop(timelineH);
        area.removeFromTop(8);

        // Score chart: ~140px (header 24px + chart 120px)
        chartHeaderBounds_ = area.removeFromTop(24);
        chartBounds_ = area.removeFromTop(120);
        area.removeFromTop(8);

        // Stats: ~60px
        statsBounds_ = area.removeFromTop(60);
        area.removeFromTop(8);

        // View Report button: ~42px
        buttonBounds_ = area.removeFromTop(42);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  resized
    // ═══════════════════════════════════════════════════════════════════════════
    void ProgressScreen::resized()
    {
        recalcLayout();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  mouseMove — Hover tracking para View Report button
    // ═══════════════════════════════════════════════════════════════════════════
    void ProgressScreen::mouseMove(const juce::MouseEvent& e)
    {
        bool newHover = buttonBounds_.contains(e.getPosition());
        if (newHover != viewReportHovered_) {
            viewReportHovered_ = newHover;
            repaint();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  mouseDown — Detecta click en View Report button
    // ═══════════════════════════════════════════════════════════════════════════
    void ProgressScreen::mouseDown(const juce::MouseEvent& e)
    {
        if (buttonBounds_.contains(e.getPosition()) && onViewReport) {
            onViewReport();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint
    // ═══════════════════════════════════════════════════════════════════════════
    void ProgressScreen::paint(juce::Graphics& g)
    {
        // Fade-in animation
        auto elapsed = juce::Time::getMillisecondCounter() - startTimeMs_;
        fadeIn_ = juce::jmin(1.0f, elapsed / 300.0f);

        if (state_ == State::Empty) {
            drawEmptyState(g, getLocalBounds());
        } else {
            drawHeader(g, headerBounds_);
            drawStreaks(g, streakBounds_);
            drawMilestonesBar(g, milestonesBarBounds_);
            drawTimeline(g, timelineBounds_);
            drawScoreChart(g, chartHeaderBounds_, chartBounds_);
            drawSessionStats(g, statsBounds_);
            drawViewReportButton(g, buttonBounds_);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawEmptyState
    // ═══════════════════════════════════════════════════════════════════════════
    void ProgressScreen::drawEmptyState(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        g.setColour(juce::Colours::white.withAlpha(0.04f * fadeIn_));
        g.drawRoundedRectangle(bounds.toFloat().reduced(40, 60), 16.0f, 1.0f);

        auto centre = bounds.getCentre();

        // Icon
        g.setFont(juce::Font(juce::FontOptions(42.0f)));
        g.setColour(MixCoachTheme::accent().withAlpha(0.25f * fadeIn_));
        g.drawText(juce::CharPointer_UTF8("[TREND]"),
                   juce::Rectangle<int>(centre.x - 36, centre.y - 70, 72, 54),
                   juce::Justification::centred);

        // Title
        g.setFont(juce::Font(juce::FontOptions(18.0f)).boldened());
        g.setColour(MixCoachTheme::textBright().withAlpha(fadeIn_));
        g.drawText("Your Progress Journey",
                   juce::Rectangle<int>(centre.x - 130, centre.y - 18, 260, 26),
                   juce::Justification::centred);

        // Subtitle
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.8f * fadeIn_));
        g.drawText("Start a session and begin mixing to track your progress",
                   juce::Rectangle<int>(centre.x - 190, centre.y + 14, 380, 18),
                   juce::Justification::centred);

        // Hint pill
        auto hintRect = juce::Rectangle<int>(centre.x - 80, centre.y + 48, 160, 28).toFloat();
        g.setColour(MixCoachTheme::accent().withAlpha(0.10f * fadeIn_));
        g.fillRoundedRectangle(hintRect, 14.0f);
        g.setColour(MixCoachTheme::accent().withAlpha(0.30f * fadeIn_));
        g.drawRoundedRectangle(hintRect, 14.0f, 0.5f);

        g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
        g.setColour(MixCoachTheme::accent().withAlpha(fadeIn_));
        g.drawText("\xF0\x9F\x93\x9D  No sessions yet",
                   hintRect.toNearestInt(), juce::Justification::centred);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawHeader — Fase actual + progress bar
    // ═══════════════════════════════════════════════════════════════════════════
    void ProgressScreen::drawHeader(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        if (bounds.isEmpty()) return;

        // ─── Panel background ─────────────────────────────────────────────
        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        auto inner = bounds.reduced(12, 8);

        // ─── Left: Emoji + "PROGRESS" + current phase ─────────────────────
        auto leftArea = inner.removeFromLeft(inner.getWidth() / 2);

        // Icon
        auto iconArea = leftArea.removeFromLeft(28);
        g.setFont(juce::Font(juce::FontOptions(18.0f)));
        g.setColour(MixCoachTheme::accentGlow());
        g.drawText(juce::CharPointer_UTF8("[TREND]"), iconArea, juce::Justification::centred);

        // "PROGRESS" title
        auto titleArea = leftArea.removeFromLeft(90);
        g.setFont(juce::Font(juce::FontOptions(14.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText("PROGRESS", titleArea, juce::Justification::centredLeft);

        // Phase badge pill
        auto phaseArea = leftArea.removeFromLeft(160);
        auto pillRect = phaseArea.toFloat().reduced(4, 6);
        g.setColour(MixCoachTheme::accent().withAlpha(0.12f));
        g.fillRoundedRectangle(pillRect, 6.0f);
        g.setColour(MixCoachTheme::accent().withAlpha(0.30f));
        g.drawRoundedRectangle(pillRect, 6.0f, 0.5f);

        g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
        g.setColour(MixCoachTheme::accent());
        g.drawText(phaseEmoji_ + " " + phaseName_, phaseArea, juce::Justification::centred);

        // ─── Right: Genre + Session number ────────────────────────────────
        auto rightArea = inner.removeFromRight(inner.getWidth());

        // Session number
        if (sessionNumber_ > 0) {
            auto sessArea = rightArea.removeFromRight(120);
            auto sessPill = sessArea.toFloat().reduced(4, 6);
            g.setColour(MixCoachTheme::info().withAlpha(0.10f));
            g.fillRoundedRectangle(sessPill, 6.0f);
            g.setColour(MixCoachTheme::info().withAlpha(0.30f));
            g.drawRoundedRectangle(sessPill, 6.0f, 0.5f);

            g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
            g.setColour(MixCoachTheme::info());
            g.drawText("Session #" + juce::String(sessionNumber_), sessArea, juce::Justification::centred);
        }

        // Genre tag
        if (genre_.isNotEmpty()) {
            auto genreArea = rightArea.removeFromRight(120);
            auto genrePill = genreArea.toFloat().reduced(4, 6);
            g.setColour(MixCoachTheme::success().withAlpha(0.10f));
            g.fillRoundedRectangle(genrePill, 6.0f);
            g.setColour(MixCoachTheme::success().withAlpha(0.30f));
            g.drawRoundedRectangle(genrePill, 6.0f, 0.5f);

            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.setColour(MixCoachTheme::success());
            g.drawText("[TARGET]" + genre_, genreArea, juce::Justification::centred);
        }

        // ─── Bottom: Progress bar ─────────────────────────────────────────
        auto progressBarArea = juce::Rectangle<int>(
            bounds.getX() + 12,
            bounds.getBottom() - 8,
            bounds.getWidth() - 24,
            4);

        g.setColour(juce::Colour(0x15FFFFFF));
        g.fillRoundedRectangle(progressBarArea.toFloat(), 2.0f);

        int fillW = (int)(progressBarArea.getWidth() * phaseProgress_);
        if (fillW > 4) {
            auto fillArea = progressBarArea.withWidth(fillW);
            juce::ColourGradient fillGrad(
                MixCoachTheme::accent().withAlpha(0.6f),
                (float)fillArea.getX(), 0.0f,
                MixCoachTheme::success().withAlpha(0.6f),
                (float)fillArea.getRight(), 0.0f,
                false);
            g.setGradientFill(fillGrad);
            g.fillRoundedRectangle(fillArea.toFloat(), 2.0f);
        }

        // Progress percentage
        g.setFont(juce::Font(juce::FontOptions(7.0f)).boldened());
        g.setColour(MixCoachTheme::textMuted());
        g.drawText(juce::String((int)(phaseProgress_ * 100.0f)) + "%",
                   progressBarArea.translated(progressBarArea.getWidth() + 4, -3),
                   juce::Justification::centredLeft);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawStreaks — Current streak + best streak (Duolingo-style)
    // ═══════════════════════════════════════════════════════════════════════════
    void ProgressScreen::drawStreaks(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        if (bounds.isEmpty()) return;

        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        auto inner = bounds.reduced(16, 12);

        // ─── Left: Current streak (big fire icon + number) ────────────────
        auto leftCol = inner.removeFromLeft(inner.getWidth() / 2);

        // Fire icon
        auto fireArea = leftCol.removeFromLeft(44);
        g.setFont(juce::Font(juce::FontOptions(28.0f)));
        if (currentStreak_ >= 3) {
            g.setColour(juce::Colour(0xFFFF6B35)); // Hot orange for active streaks
        } else if (currentStreak_ >= 1) {
            g.setColour(MixCoachTheme::warning());
        } else {
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.4f));
        }
        g.drawText(juce::CharPointer_UTF8("[FIRE]"), fireArea, juce::Justification::centred);

        // Streak number
        auto numArea = leftCol.removeFromLeft(36);
        g.setFont(juce::Font(juce::FontOptions(32.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText(juce::String(currentStreak_), numArea, juce::Justification::centred);

        // "day streak" label
        auto labelArea = leftCol;
        g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
        g.setColour(MixCoachTheme::textDim());
        juce::String streakLabel = (currentStreak_ == 1) ? "Session Streak" : "Sessions Streak";
        g.drawText(streakLabel, labelArea, juce::Justification::centredLeft);

        // ─── Right: Best streak ──────────────────────────────────────────
        auto rightCol = inner;

        // Trophy icon
        auto trophyArea = rightCol.removeFromLeft(36);
        g.setFont(juce::Font(juce::FontOptions(24.0f)));
        g.setColour(MixCoachTheme::warning().withAlpha(0.7f));
        g.drawText(juce::CharPointer_UTF8("[TROPHY]"), trophyArea, juce::Justification::centred);

        // Best streak number
        auto bestNumArea = rightCol.removeFromLeft(36);
        g.setFont(juce::Font(juce::FontOptions(28.0f)).boldened());
        g.setColour(MixCoachTheme::warning().brighter(0.3f));
        g.drawText(juce::String(bestStreak_), bestNumArea, juce::Justification::centred);

        // "best" label
        auto bestLabelArea = rightCol;
        g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
        g.setColour(MixCoachTheme::textDim());
        g.drawText("Best Streak", bestLabelArea, juce::Justification::centredLeft);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawMilestonesBar — Barra horizontal compacta con checkpoints por fase
    //  Muestra 7 nodos (⚙️→🎵→🔍→💬→✨→📄→💾) conectados por línea de progreso.
    //  - Completados: círculo verde con ✓
    //  - Actual: círculo pulsante (acento)
    //  - Futuros: círculo hueco gris
    // ═══════════════════════════════════════════════════════════════════════════
    void ProgressScreen::drawMilestonesBar(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        if (bounds.isEmpty() || milestones_.empty()) return;

        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        auto inner = bounds.reduced(12, 6);

        // ─── "PHASE CHECKPOINTS" label ────────────────────────────────────
        auto labelArea = inner.removeFromTop(14);
        g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
        g.setColour(MixCoachTheme::textMuted());
        g.drawText("\xE2\x96\xA0  PHASE CHECKPOINTS", labelArea, juce::Justification::centredLeft);

        // ─── Current phase label (right-aligned) ──────────────────────────
        if (!phaseName_.isEmpty()) {
            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.setColour(MixCoachTheme::accent().withAlpha(0.8f));
            juce::String currentLabel = "Current: " + phaseEmoji_ + " " + phaseName_;
            g.drawText(currentLabel, labelArea, juce::Justification::centredRight);
        }

        // ─── Bar area (nodes + connecting line) ───────────────────────────
        auto barArea = inner;
        int nMilestones = juce::jmin((int)milestones_.size(), 7);
        if (nMilestones < 2) return;

        // Calculate node positions
        int nodeY      = barArea.getCentreY();
        int nodeSpacing = barArea.getWidth() / (nMilestones - 1);
        int nodeR      = 8;  // Radius of each node
        int lineY      = nodeY;

        // Count completed milestones
        int completedCount = 0;
        for (const auto& m : milestones_)
            if (m.completed) ++completedCount;

        // ─── Draw connecting line ─────────────────────────────────────────
        int lineLeft  = barArea.getX() + nodeR;
        int lineRight = barArea.getX() + (nMilestones - 1) * nodeSpacing + nodeR;

        // Line background (dim)
        g.setColour(MixCoachTheme::divider().withAlpha(0.15f));
        g.fillRect(lineLeft, lineY - 1, lineRight - lineLeft, 2);

        // Line fill (completed portion)
        if (completedCount > 0 && completedCount < nMilestones) {
            int filledRight = lineLeft + (int)((float)(lineRight - lineLeft)
                                         * (float)completedCount / (float)(nMilestones - 1));
            auto fillRect = juce::Rectangle<int>(lineLeft, lineY - 1, filledRight - lineLeft, 2);
            juce::ColourGradient fillGrad(
                MixCoachTheme::accent().withAlpha(0.7f),
                (float)lineLeft, 0.0f,
                MixCoachTheme::success().withAlpha(0.7f),
                (float)filledRight, 0.0f,
                false);
            g.setGradientFill(fillGrad);
            g.fillRect(fillRect);
        } else if (completedCount >= nMilestones) {
            g.setColour(MixCoachTheme::success().withAlpha(0.7f));
            g.fillRect(lineLeft, lineY - 1, lineRight - lineLeft, 2);
        }

        // ─── Draw milestone icons above nodes ────────────────────────────
        int iconY = nodeY - nodeR - 16;
        for (int i = 0; i < nMilestones; ++i) {
            const auto& m = milestones_[i];
            int nodeX = barArea.getX() + i * nodeSpacing;

            juce::Colour iconColour;
            if (m.completed)           iconColour = MixCoachTheme::textBright();
            else if (m.isCurrent)      iconColour = MixCoachTheme::accentGlow();
            else                       iconColour = MixCoachTheme::textMuted().withAlpha(0.35f);

            g.setFont(juce::Font(juce::FontOptions(11.0f)));
            g.setColour(iconColour);
            g.drawText(m.icon,
                       juce::Rectangle<int>(nodeX - 10, iconY, 20, 14),
                       juce::Justification::centred);

            // Phase label below icon (shortened)
            g.setFont(juce::Font(juce::FontOptions(6.5f)));
            g.setColour(m.completed ? MixCoachTheme::textDim() :
                        m.isCurrent ? MixCoachTheme::accent().withAlpha(0.7f) :
                        MixCoachTheme::textMuted().withAlpha(0.3f));
            juce::String shortName = m.phaseName.substring(0, 8);
            g.drawText(shortName,
                       juce::Rectangle<int>(nodeX - 24, iconY + 14, 48, 10),
                       juce::Justification::centred);
        }

        // ─── Draw nodes ──────────────────────────────────────────────────
        int64_t now = juce::Time::getMillisecondCounter();

        for (int i = 0; i < nMilestones; ++i) {
            const auto& m = milestones_[i];
            int nodeX = barArea.getX() + i * nodeSpacing;

            if (m.completed) {
                // ✅ Completed: green filled circle with checkmark
                int r = nodeR;
                g.setColour(MixCoachTheme::success().withAlpha(0.85f));
                g.fillEllipse((float)(nodeX - r), (float)(nodeY - r),
                              (float)(r * 2), (float)(r * 2));
                // White border
                g.setColour(juce::Colours::white.withAlpha(0.30f));
                g.drawEllipse((float)(nodeX - r), (float)(nodeY - r),
                              (float)(r * 2), (float)(r * 2), 1.0f);
                // Checkmark
                g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
                g.setColour(juce::Colours::white);
                g.drawText("[OK]",
                           juce::Rectangle<int>(nodeX - r, nodeY - r, r * 2, r * 2),
                           juce::Justification::centred);
            }
            else if (m.isCurrent) {
                // ◎ Current: pulsing accent circle (slightly larger)
                int r = nodeR + 2;
                float pulse = 0.80f + 0.20f * std::sin(now * 0.005f + (float)i * 0.3f);
                // Outer glow ring
                g.setColour(MixCoachTheme::accent().withAlpha(0.15f * pulse));
                g.fillEllipse((float)(nodeX - r - 4), (float)(nodeY - r - 4),
                              (float)((r + 4) * 2), (float)((r + 4) * 2));
                // Mid glow
                g.setColour(MixCoachTheme::accent().withAlpha(0.25f * pulse));
                g.fillEllipse((float)(nodeX - r - 2), (float)(nodeY - r - 2),
                              (float)((r + 2) * 2), (float)((r + 2) * 2));
                // Filled circle
                g.setColour(MixCoachTheme::accent().withAlpha(pulse));
                g.fillEllipse((float)(nodeX - r), (float)(nodeY - r),
                              (float)(r * 2), (float)(r * 2));
                // White border
                g.setColour(juce::Colours::white.withAlpha(0.40f));
                g.drawEllipse((float)(nodeX - r), (float)(nodeY - r),
                              (float)(r * 2), (float)(r * 2), 1.5f);
            }
            else {
                // ○ Future: hollow gray circle
                int r = nodeR;
                g.setColour(MixCoachTheme::divider().withAlpha(0.25f));
                g.drawEllipse((float)(nodeX - r), (float)(nodeY - r),
                              (float)(r * 2), (float)(r * 2), 1.5f);
                // Light fill on hover
                g.setColour(MixCoachTheme::divider().withAlpha(0.05f));
                g.fillEllipse((float)(nodeX - r), (float)(nodeY - r),
                              (float)(r * 2), (float)(r * 2));
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawTimeline — Línea de tiempo vertical con hitos de sesión
    // ═══════════════════════════════════════════════════════════════════════════
    void ProgressScreen::drawTimeline(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        if (bounds.isEmpty() || milestones_.empty()) return;

        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        auto inner = bounds.reduced(16, 8);

        // ─── Timeline header ──────────────────────────────────────────────
        auto headerArea = inner.removeFromTop(20);
        g.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText("\xE2\x8F\xB0  SESSION TIMELINE", headerArea, juce::Justification::centredLeft);

        // Divider
        g.setColour(MixCoachTheme::divider().withAlpha(0.20f));
        g.fillRect(inner.getX(), inner.getY() - 1, inner.getWidth(), 1);

        // ─── Draw vertical line ──────────────────────────────────────────
        int lineX = inner.getX() + 18;
        int lineTop = inner.getY() + 4;
        int lineBottom = inner.getBottom() - 4;

        // Line background (dim)
        g.setColour(MixCoachTheme::divider().withAlpha(0.15f));
        g.fillRect(lineX - 1, lineTop, 2, lineBottom - lineTop);

        // Line fill (active portion)
        int completedCount = 0;
        for (const auto& m : milestones_)
            if (m.completed || m.isCurrent) ++completedCount;

        if (completedCount > 0 && !milestones_.empty()) {
            float fillRatio = (float)completedCount / (float)milestones_.size();
            int fillH = (int)((lineBottom - lineTop) * fillRatio);
            int fillTop = lineBottom - fillH;

            juce::ColourGradient lineGrad(
                MixCoachTheme::accent().withAlpha(0.6f),
                (float)lineX, (float)fillTop,
                MixCoachTheme::success().withAlpha(0.6f),
                (float)lineX, (float)lineBottom,
                false);
            g.setGradientFill(lineGrad);
            g.fillRect(lineX - 1, fillTop, 2, fillH);
        }

        // ─── Draw each milestone ─────────────────────────────────────────
        int rowH = 28;
        int maxRows = juce::jmin((int)milestones_.size(), inner.getHeight() / rowH);

        for (int i = 0; i < maxRows; ++i) {
            const auto& m = milestones_[i];
            auto rowBounds = inner.removeFromTop(rowH).reduced(4, 2);

            // ─── Node on timeline ────────────────────────────────────────
            int nodeCx = lineX;
            int nodeCy = rowBounds.getCentreY();
            int nodeR = 7;

            if (m.completed) {
                // Green checkmark circle
                g.setColour(MixCoachTheme::success().withAlpha(0.85f));
                g.fillEllipse((float)(nodeCx - nodeR), (float)(nodeCy - nodeR),
                              (float)(nodeR * 2), (float)(nodeR * 2));
                g.setFont(juce::Font(juce::FontOptions(10.0f)));
                g.setColour(juce::Colours::white);
                g.drawText("[OK]",
                           juce::Rectangle<int>(nodeCx - nodeR, nodeCy - nodeR, nodeR * 2, nodeR * 2),
                           juce::Justification::centred);
            } else if (m.isCurrent) {
                // Current phase: pulsing accent circle
                float pulse = 0.85f + 0.15f * std::sin(juce::Time::getMillisecondCounter() * 0.004f);
                g.setColour(MixCoachTheme::accent().withAlpha(pulse));
                g.fillEllipse((float)(nodeCx - nodeR), (float)(nodeCy - nodeR),
                              (float)(nodeR * 2), (float)(nodeR * 2));
                // Outer ring
                g.setColour(MixCoachTheme::accent().withAlpha(0.3f));
                g.drawEllipse((float)(nodeCx - nodeR - 2), (float)(nodeCy - nodeR - 2),
                              (float)((nodeR + 2) * 2), (float)((nodeR + 2) * 2), 1.5f);
            } else {
                // Future: empty circle
                g.setColour(MixCoachTheme::divider().withAlpha(0.25f));
                g.drawEllipse((float)(nodeCx - nodeR), (float)(nodeCy - nodeR),
                              (float)(nodeR * 2), (float)(nodeR * 2), 1.0f);
            }

            // ─── Milestone icon ─────────────────────────────────────────
            auto iconArea = rowBounds.removeFromLeft(22).translated(28, 0);
            g.setFont(juce::Font(juce::FontOptions(12.0f)));
            g.setColour(m.completed ? MixCoachTheme::textBright() :
                        m.isCurrent ? MixCoachTheme::accentGlow() :
                        MixCoachTheme::textMuted().withAlpha(0.4f));
            g.drawText(m.icon, iconArea, juce::Justification::centred);

            // ─── Milestone name ─────────────────────────────────────────
            auto nameArea = rowBounds.removeFromLeft(120);
            g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
            g.setColour(m.completed ? MixCoachTheme::textBright() :
                        m.isCurrent ? MixCoachTheme::accentGlow() :
                        MixCoachTheme::textMuted().withAlpha(0.4f));
            g.drawText(m.phaseName, nameArea, juce::Justification::centredLeft);

            // ─── Milestone description ──────────────────────────────────
            if (m.description.isNotEmpty()) {
                g.setFont(juce::Font(juce::FontOptions(8.0f)));
                g.setColour(m.completed ? MixCoachTheme::textDim() :
                            MixCoachTheme::textMuted().withAlpha(0.5f));
                juce::String desc = m.description;
                if (desc.length() > 40)
                    desc = desc.substring(0, 37) + "...";
                g.drawText(desc, rowBounds, juce::Justification::centredLeft);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawScoreChart — Gráfico de evolución multi-sesión
    // ═══════════════════════════════════════════════════════════════════════════
    void ProgressScreen::drawScoreChart(juce::Graphics& g,
                                          juce::Rectangle<int> headerBounds,
                                          juce::Rectangle<int> chartBounds)
    {
        if (headerBounds.isEmpty() || chartBounds.isEmpty()) return;

        // ─── Header ──────────────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText("[TREND]  SCORE HISTORY", headerBounds, juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.setColour(MixCoachTheme::textMuted());
        juce::String histSub = juce::String((int)sessionHistory_.size()) + " sessions recorded";
        g.drawText(histSub, headerBounds, juce::Justification::centredRight);

        if (sessionHistory_.size() < 2) {
            MixCoachTheme::fillGlassPanel(g, chartBounds.toFloat(), MixCoachTheme::cornerRadius_medium);
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(MixCoachTheme::textMuted());
            juce::String msg = sessionHistory_.empty()
                ? "Keep mixing! Score data will appear after multiple sessions."
                : "One session recorded. Come back after another session to see progress.";
            g.drawText(msg, chartBounds, juce::Justification::centred);
            return;
        }

        // ─── Chart panel ─────────────────────────────────────────────────
        MixCoachTheme::fillGlassPanel(g, chartBounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        auto inner = chartBounds.reduced(12, 8);
        int chartW = inner.getWidth();
        int chartH = inner.getHeight();

        if (chartW < 40 || chartH < 30) return;

        int n = (int)sessionHistory_.size();
        int maxScore = 0;
        for (const auto& snap : sessionHistory_)
            if (snap.mixScoreOverall > maxScore) maxScore = snap.mixScoreOverall;
        int scoreRange = juce::jmax(100, maxScore + 10);

        // ─── Grid lines ──────────────────────────────────────────────────
        g.setColour(MixCoachTheme::divider().withAlpha(0.15f));
        for (int pct = 25; pct <= 75; pct += 25) {
            int y = inner.getY() + (int)(chartH * (1.0f - pct / 100.0f));
            g.fillRect(inner.getX(), y, chartW, 1);
        }

        // ─── Y-axis qualitative zones ──────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(7.0f)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
        g.drawText("\xF0\x9F\x8C\x9F", inner.getX() - 2, inner.getY() - 6, 16, 8, juce::Justification::right);
        g.drawText("\xF0\x9F\x91\x8D", inner.getX() - 2, inner.getY() + chartH / 4 - 4, 16, 8, juce::Justification::right);
        g.drawText("\xF0\x9F\x92\xAA", inner.getX() - 2, inner.getY() + chartH * 2 / 4 - 4, 16, 8, juce::Justification::right);
        g.drawText("[TREND]", inner.getX() - 2, inner.getBottom() - 6, 16, 8, juce::Justification::right);

        // ─── Helper: plot point ──────────────────────────────────────────
        auto plotPoint = [&](int idx, int value) -> juce::Point<int> {
            float xFrac = (n > 1) ? (float)idx / (n - 1) : 0.5f;
            int x = inner.getX() + (int)(xFrac * chartW);
            int y = inner.getBottom() - (int)((float)value / scoreRange * chartH);
            y = juce::jlimit(inner.getY(), inner.getBottom(), y);
            return {x, y};
        };

        // ─── Helper: extraer score por dominio ──────────────────────────
        auto domainScore = [](const AiCoachAdapter::SessionSnapshotEntry& snap, int d) -> int {
            switch (d) {
                case 0: return snap.domainGain;
                case 1: return snap.domainTonal;
                case 2: return snap.domainDynamics;
                case 3: return snap.domainSpatial;
                case 4: return snap.domainReference;
                default: return 0;
            }
        };

        // ─── Check for domain data ────────────────────────────────────────
        bool hasDomainData = false;
        for (int i = 0; i < n; ++i) {
            const auto& snap = sessionHistory_[i];
            if (snap.domainGain > 0 || snap.domainTonal > 0 ||
                snap.domainDynamics > 0 || snap.domainSpatial > 0 ||
                snap.domainReference > 0) {
                hasDomainData = true;
                break;
            }
        }

        // ─── Draw domain lines (thin, behind overall) ────────────────────
        if (hasDomainData) {
            for (int d = 0; d < 5; ++d) {
                juce::Path path;
                bool pathStarted = false;
                for (int i = 0; i < n; ++i) {
                    int val = domainScore(sessionHistory_[i], d);
                    if (val == 0 && d == 4) continue;
                    auto pt = plotPoint(i, val);
                    if (!pathStarted) {
                        path.startNewSubPath((float)pt.x, (float)pt.y);
                        pathStarted = true;
                    } else {
                        path.lineTo((float)pt.x, (float)pt.y);
                    }
                }
                if (path.getLength() > 0.0f) {
                    g.setColour(domainColour(d).withAlpha(0.50f));
                    g.strokePath(path, juce::PathStrokeType(1.5f));
                }
            }
        }

        // ─── MixScore overall line ────────────────────────────────────────
        juce::Path scorePath;
        for (int i = 0; i < n; ++i) {
            auto pt = plotPoint(i, sessionHistory_[i].mixScoreOverall);
            if (i == 0) scorePath.startNewSubPath((float)pt.x, (float)pt.y);
            else scorePath.lineTo((float)pt.x, (float)pt.y);
        }
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.strokePath(scorePath, juce::PathStrokeType(2.5f));

        // Outer glow
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.strokePath(scorePath, juce::PathStrokeType(5.0f));

        // Data dots
        for (int i = 0; i < n; ++i) {
            auto pt = plotPoint(i, sessionHistory_[i].mixScoreOverall);
            g.setColour(juce::Colours::white.withAlpha(0.15f));
            g.fillEllipse((float)pt.x - 5.0f, (float)pt.y - 5.0f, 10.0f, 10.0f);
            g.setColour(juce::Colours::white);
            g.fillEllipse((float)pt.x - 2.5f, (float)pt.y - 2.5f, 5.0f, 5.0f);
        }

        // ─── X-axis labels ──────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(8.0f)));
        g.setColour(MixCoachTheme::textMuted());
        if (n <= 8) {
            for (int i = 0; i < n; ++i) {
                auto pt = plotPoint(i, 0);
                juce::String sNum = "S" + juce::String(sessionHistory_[i].sessionNumber);
                g.drawText(sNum, pt.x - 10, pt.y + 2, 20, 10, juce::Justification::centred);
            }
        } else {
            for (int i = 0; i < n; ++i) {
                if (i == 0 || i == n - 1 || i % 3 == 0) {
                    auto pt = plotPoint(i, 0);
                    juce::String sNum = "S" + juce::String(sessionHistory_[i].sessionNumber);
                    g.drawText(sNum, pt.x - 10, pt.y + 2, 20, 10, juce::Justification::centred);
                }
            }
        }

        // ─── Legend ──────────────────────────────────────────────────────
        auto legendBounds = chartBounds.withHeight(14).reduced(12, 0);
        legendBounds = legendBounds.translated(0, chartBounds.getHeight() - 14);
        {
            auto legItem = legendBounds.removeFromLeft(70);
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.fillRect(legItem.removeFromLeft(12).reduced(0, 6));
            g.setFont(juce::Font(juce::FontOptions(8.0f)));
            g.setColour(MixCoachTheme::textDim());
            g.drawText("Score", legItem, juce::Justification::centredLeft);

            if (hasDomainData) {
                static const char* domainLabels[5] = {"Gain", "Tonal", "Dyn", "Spatial", "Ref"};
                for (int d = 0; d < 5; ++d) {
                    auto legItem2 = legendBounds.removeFromLeft(60);
                    g.setColour(domainColour(d).withAlpha(0.50f));
                    g.fillRect(legItem2.removeFromLeft(10).reduced(0, 6));
                    g.setFont(juce::Font(juce::FontOptions(8.0f)));
                    g.setColour(MixCoachTheme::textDim());
                    g.drawText(domainLabels[d], legItem2, juce::Justification::centredLeft);
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawSessionStats — Resumen de estadísticas
    // ═══════════════════════════════════════════════════════════════════════════
    void ProgressScreen::drawSessionStats(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        if (bounds.isEmpty()) return;

        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        auto inner = bounds.reduced(16, 10);

        // ─── 3 stats columns ──────────────────────────────────────────────
        auto drawStat = [&](juce::Rectangle<int>& area,
                            const juce::String& label,
                            const juce::String& value,
                            juce::Colour valueColour) {
            auto col = area.removeFromLeft(area.getWidth() / 3);

            // Value
            g.setFont(juce::Font(juce::FontOptions(24.0f)).boldened());
            g.setColour(valueColour);
            g.drawText(value, col.removeFromTop(32), juce::Justification::centred);

            // Label
            g.setFont(juce::Font(juce::FontOptions(9.0f)));
            g.setColour(MixCoachTheme::textMuted());
            g.drawText(label, col, juce::Justification::centred);
        };

        drawStat(inner, "Sessions", juce::String(sessionNumber_),
                 MixCoachTheme::accentGlow());
        drawStat(inner, "Corrections", juce::String(correctionCount_),
                 MixCoachTheme::info());
        drawStat(inner, "Score", juce::String(mixScore_),
                 mixScore_ >= 70 ? MixCoachTheme::success() :
                 mixScore_ >= 40 ? MixCoachTheme::warning() :
                 MixCoachTheme::error());
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawViewReportButton — Botón premium "View Report"
    // ═══════════════════════════════════════════════════════════════════════════
    void ProgressScreen::drawViewReportButton(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        if (bounds.isEmpty()) return;

        auto btn = bounds.toFloat();

        // ─── Shadow ──────────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.30f));
        g.fillRoundedRectangle(btn.translated(0.0f, 2.0f), MixCoachTheme::cornerRadius_medium);

        // ─── Button gradient ─────────────────────────────────────────────
        juce::ColourGradient btnGrad(
            viewReportHovered_ ? MixCoachTheme::accentGlow() : MixCoachTheme::accent().brighter(0.05f),
            btn.getCentreX(), btn.getY(),
            viewReportHovered_ ? MixCoachTheme::accent().brighter(0.2f) : MixCoachTheme::accentDim(),
            btn.getCentreX(), btn.getBottom(), false);
        g.setGradientFill(btnGrad);
        g.fillRoundedRectangle(btn, MixCoachTheme::cornerRadius_medium);

        // ─── Hover glow ─────────────────────────────────────────────────
        if (viewReportHovered_) {
            g.setColour(MixCoachTheme::accentGlow().withAlpha(0.15f));
            g.fillRoundedRectangle(btn.expanded(4.0f, 2.0f), MixCoachTheme::cornerRadius_large);
        }

        // ─── Glass highlight ─────────────────────────────────────────────
        auto glassTop = btn.withHeight(btn.getHeight() * 0.45f);
        juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.10f),
                                       glassTop.getCentreX(), glassTop.getY(),
                                       juce::Colour(0x00000000),
                                       glassTop.getCentreX(), glassTop.getBottom(), false);
        g.setGradientFill(glassGrad);
        g.fillRoundedRectangle(glassTop, MixCoachTheme::cornerRadius_medium);

        // ─── Border ─────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::accentGlow().withAlpha(0.15f));
        g.drawRoundedRectangle(btn, MixCoachTheme::cornerRadius_medium, 0.5f);

        // ─── Text ───────────────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
        g.setColour(juce::Colours::white);
        g.drawText("[EXPORT]  VIEW REPORT", bounds, juce::Justification::centred);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  formatTimestamp
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String ProgressScreen::formatTimestamp(int64_t timestampUs)
    {
        if (timestampUs <= 0) return "";

        int64_t nowUs = juce::Time::currentTimeMillis() * 1000;
        int64_t elapsedUs = nowUs - timestampUs;
        int64_t minutesAgo = elapsedUs / (60 * 1000 * 1000);

        if (minutesAgo < 1)      return "just now";
        if (minutesAgo < 60)     return juce::String((int)minutesAgo) + "m ago";
        int64_t hoursAgo = minutesAgo / 60;
        if (hoursAgo < 24)       return juce::String((int)hoursAgo) + "h ago";
        int64_t daysAgo = hoursAgo / 24;
        if (daysAgo < 7)         return juce::String((int)daysAgo) + "d ago";
        return juce::String((int)(daysAgo / 7)) + "w ago";
    }

} // namespace mixcoach
