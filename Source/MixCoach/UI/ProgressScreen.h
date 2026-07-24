#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "MixCoachTheme.h"
#include "../engine/CoachEngine.h"
#include "../ai/AiCoachAdapter.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  ProgressScreen — Timeline gamificado de sesión estilo Duolingo
    //
    //  Muestra:
    //    • Header con fase actual y progreso global
    //    • Rachas (current streak, best streak)
    //    • Timeline de hitos de sesión con iconos de completitud
    //    • Gráfico histórico de scores multi-sesión
    //    • Estadísticas de sesión (correcciones, tracks, score)
    //    • Botón "View Report" que dispara onViewReport callback
    //
    //  Uso:
    //    auto* progress = new ProgressScreen();
    //    progress->updateFromEngine(coachEngine);
    //    addAndMakeVisible(progress);
    //
    //  Data sources:
    //    • AiCoachAdapter::loadSessionHistory() — historial multi-sesión
    //    • SessionProgression — fase actual y progreso
    //    • MixScore — score actual
    //    • CoachEngine — correcciones, identity progress
    // ═══════════════════════════════════════════════════════════════════════════
    class ProgressScreen : public juce::Component
    {
    public:
        enum class State { Empty, Ready };

        ProgressScreen();
        ~ProgressScreen() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseDown(const juce::MouseEvent& e) override;

        /** Actualiza todos los datos desde el engine.
            @param engine Referencia al CoachEngine con datos vivos
            @return true si el estado cambió */
        bool updateFromEngine(CoachEngine& engine);

        /** Fuerza un estado específico. */
        void setState(State newState);

        [[nodiscard]] State getState() const noexcept { return state_; }

        // ─── Callbacks ─────────────────────────────────────────────────────
        std::function<void()> onViewReport;

    private:
        State state_ = State::Empty;

        // ═══ Milestone del timeline (derivado de SessionProgression phases) ══
        struct Milestone
        {
            juce::String phaseName; // "Setup", "Reference", etc.
            juce::String icon;      // Emoji for the milestone
            bool completed = false;
            bool isCurrent = false;
            juce::String description; // "Configured genre, mode, name"
        };
        std::vector<Milestone> milestones_;

        // ═══ Datos cacheados ════════════════════════════════════════════════
        std::vector<AiCoachAdapter::SessionSnapshotEntry> sessionHistory_;
        int mixScore_            = 0;
        int correctionCount_     = 0;
        int appliedCorrections_  = 0;
        int sessionNumber_       = 0;
        int activeTracks_        = 0;
        juce::String phaseName_;
        juce::String phaseEmoji_;
        float phaseProgress_     = 0.0f;
        juce::String genre_;
        juce::String engineerName_;

        // ═══ Streak tracking ════════════════════════════════════════════════
        int currentStreak_       = 0;
        int bestStreak_          = 0;

        // ═══ Hover state ═══════════════════════════════════════════════════
        bool viewReportHovered_ = false;

        // ═══ Animación de entrada ══════════════════════════════════════════
        float fadeIn_ = 0.0f;
        juce::uint32 startTimeMs_ = 0;

        // ═══ Layout bounds cacheados ═══════════════════════════════════════
        juce::Rectangle<int> headerBounds_;
        juce::Rectangle<int> streakBounds_;
        juce::Rectangle<int> milestonesBarBounds_;
        juce::Rectangle<int> timelineBounds_;
        juce::Rectangle<int> chartBounds_;
        juce::Rectangle<int> chartHeaderBounds_;
        juce::Rectangle<int> statsBounds_;
        juce::Rectangle<int> buttonBounds_;

        // ═══ Drawing helpers ═══════════════════════════════════════════════
        void drawHeader(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawStreaks(juce::Graphics& g, juce::Rectangle<int> bounds);
        /** Dibuja una barra horizontal compacta con nodos de fase (checkpoints). */
        void drawMilestonesBar(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawTimeline(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawScoreChart(juce::Graphics& g, juce::Rectangle<int> headerBounds,
                            juce::Rectangle<int> chartBounds);
        void drawSessionStats(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawViewReportButton(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawEmptyState(juce::Graphics& g, juce::Rectangle<int> bounds);

        void recalcLayout();

        // ═══ Timestamp helpers ═════════════════════════════════════════════
        static juce::String formatTimestamp(int64_t timestampUs);

        /** Compute streaks from session history.
            A streak = consecutive sessions within ~36 hours of each other. */
        void computeStreaks();

        /** Build milestones from SessionProgression phases. */
        void buildMilestones(const SessionProgression& progression);

        static juce::Colour domainColour(int idx) noexcept
        {
            static const juce::Colour cols[5] = {
                juce::Colour(0xFF8B5CF6), // Purple  — Gain
                juce::Colour(0xFF3B82F6), // Blue    — Tonal
                juce::Colour(0xFF10B981), // Emerald — Dynamics
                juce::Colour(0xFFF59E0B), // Amber   — Spatial
                juce::Colour(0xFFEC4899)  // Pink    — Reference
            };
            return cols[juce::jlimit(0, 4, idx)];
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgressScreen)
    };

} // namespace mixcoach
