#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "RobotAvatarComponent.h"
#include "../engine/MixScore.h"
#include "../engine/ConfidenceScore.h"
#include "../engine/CoachEngine.h"
#include "../engine/DifferenceProfile.h"
#include "../engine/RefinementProfile.h"
#include "../engine/FeedbackCollector.h"
#include "../UI/TrackProblemCard.h"
#include "../ai/AiCoachAdapter.h"

namespace mixcoach {

    class AudioAnalyzer;

    // ═══════════════════════════════════════════════════════════════════════════
    //  EndOfSessionComponent — Pantalla final con breakdown de MixScore,
    //  cambios realizados, historial multi-sesión y botón Exportar Reporte.
    //
    //  Diseño premium: glass panels, score bars con gradiente, historial de
    //  correcciones coloreado por status, progreso multi-sesión, y exportación
    //  a HTML.
    //
    //  Uso:
    //    auto* report = new EndOfSessionComponent();
    //    report->setCoachEngine(coachEngine);
    //    report->setAudioAnalyzer(audioAnalyzer);
    //    report->refresh();
    //    addAndMakeVisible(report);
    // ═══════════════════════════════════════════════════════════════════════════
    class EndOfSessionComponent : public juce::Component
    {
    public:
        enum class State
        {
            Empty,    // Sin datos de sesión (primera visita)
            Loading,  // refresh() en progreso
            Ready,    // Datos disponibles
            Error     // Error al cargar motor/analizador
        };

        EndOfSessionComponent();
        ~EndOfSessionComponent() override = default;

        void setCoachEngine(CoachEngine* engine) noexcept { coachEngine_ = engine; }
        void setAudioAnalyzer(AudioAnalyzer* analyzer) noexcept { audioAnalyzer_ = analyzer; }

        void setGenre(const juce::String& genre) noexcept { genre_ = genre; }

        void resized() override;
        void paint(juce::Graphics& g) override;
        /** Pausa/restaura el spinner segun la visibilidad. */
        void visibilityChanged() override;

        /** Establece el estado actual. */
        void setState(State newState);
        State getState() const noexcept { return state_; }

        /** Establece un mensaje de error opcional. */
        void setErrorMessage(const juce::String& msg);

        /** Refresca los datos desde el motor y repinta. */
        void refresh();

        /** Callback para reintentar en caso de error. */
        std::function<void()> onRetry;

        /** Callback para iniciar una nueva sesión (GAP #3). */
        std::function<void()> onNewSession;

        /**
         * Fuerza un score inicial específico (override del capturado automáticamente).
         * Útil si el PluginEditor quiere señalizar exactamente cuándo empezó la sesión.
         */
        void setInitialScore(const MixScore& score) noexcept
        {
            initialScore_        = score;
            initialScoreCaptured_ = true;
        }

        /** Establece la lista de plugins que el usuario realmente aplicó durante la sesión. */
        void setAppliedPlugins(const juce::StringArray& plugins) noexcept
        {
            appliedPlugins_ = plugins;
        }

    private:
        // ═══ State ═══════════════════════════════════════════════════════════
        State state_ = State::Empty;
        juce::String errorMessage_;

        // ─── Timer para spinner de carga ────────────────────────────────────
        class EosTimer : public juce::Timer
        {
        public:
            EosTimer(EndOfSessionComponent& owner) : owner_(owner) {}
            void timerCallback() override { owner_.repaint(); }
        private:
            EndOfSessionComponent& owner_;
        };
        EosTimer spinnerTimer_{*this};
        // ═══ Engine references ═══════════════════════════════════════════════
        CoachEngine* coachEngine_     = nullptr;
        AudioAnalyzer* audioAnalyzer_ = nullptr;
        juce::String genre_;

        // ═══ Cached data ═════════════════════════════════════════════════════
        MixScore currentScore_;
        MixScore initialScore_;
        bool initialScoreCaptured_ = false;
        std::vector<CoachEngine::CorrectionHistoryEntry> correctionHistory_;
        std::vector<CoachEngine::MixHistoryEntry> mixHistory_;
        int activeTrackCount_      = 0;
        int clippingTrackCount_    = 0;
        int lowSignalTrackCount_   = 0;
        // ═══ Completed coaching phases (\"Has aprendido\" tags) ═══════════
        struct PhaseTag {
            juce::String emoji;
            juce::String name;
            int score;
            bool completed;
        };
        std::vector<PhaseTag> completedPhases_;

        // ═══ Confidence score (V4b) ════════════════════════════════════════
        ConfidenceScore confidenceScore_;
        int verifyTotalAttempts_ = 0;       // Total verify attempts en sesión
        int verifyInvalidated_   = 0;       // Invólidados por cambio de sección
        int verifyConfidencePct_ = 100;      // 0-100

        int totalCorrections_      = 0;
        int appliedCorrections_    = 0;
        float masterPeakDb_         = -100.0f;
        float masterIntegratedLUFS_  = -100.0f;
        float masterTruePeakDb_      = -100.0f;
        float masterCorrelation_    = 0.0f;

        // ═══ Session metadata (from buildSessionContext) ════════════════════
        SessionProgression::Phase sessionProgressionPhase_ = SessionProgression::Phase::Setup;
        float sessionProgressionProgress_ = 0.0f;
        int64_t sessionDurationUs_ = 0;
        int achievementCount_     = 0;
        int drumTracks_          = 0;
        int bassTracks_          = 0;
        int guitarTracks_        = 0;
        int keysTracks_          = 0;
        int vocalTracks_         = 0;
        int fxTracks_            = 0;
        int melodyTracks_        = 0;
        int unknownTracks_       = 0;

        // ═══ Coach adaptation history (from FeedbackCollector) ══════════════
        std::vector<FeedbackCollector::ThresholdAdjustment> coachAdaptationHistory_;

        // ═══ Multi-session history ═════════════════════════════════════════
        std::vector<AiCoachAdapter::SessionSnapshotEntry> sessionHistory_;

        // ═══ Reference comparison cache ═══════════════════════════════════
        DifferenceProfile referenceProfile_;

        // ═══ Refinement profile cache (artistic quality: Depth, Impact, Movement, Glue, Emotion) ═══
        RefinementProfile refinementProfile_;

        // ═══ Per-track diagnosis ═══════════════════════════════════════════
        struct TrackDiagnosisEntry
        {
            int slotIndex = -1;
            juce::String trackName;
            juce::String roleName;

            // Status per domain (0 = OnTarget, 1 = NearTarget, 2 = OffTarget, -1 = no data)
            int gainStatus     = -1;
            int dynamicsStatus = -1;
            int tonalStatus    = -1;
            int phaseStatus    = -1;

            float consolidatedSeverity = 0.0f;
            juce::String worstMessage;
        };
        std::vector<TrackDiagnosisEntry> trackDiagnosis_;

        // ═══ Plugins used during session (from pluginRecs_ suggestions + appliedPlugins_)
        juce::StringArray usedPlugins_;

        // ═══ Plugins que el usuario realmente aplicó (trackeado desde onPluginClicked)
        juce::StringArray appliedPlugins_;

        // ═══ Plugin recommendations (generated from track diagnosis)
        struct PluginRecEntry
        {
            juce::String trackName;
            juce::String roleName;
            int slotIndex = -1;
            float severity = 0.0f;
            juce::String domain;
            juce::String issueType;
            float delta = 0.0f;
            float frequencyHz = 0.0f;
            std::vector<TrackPluginSuggestion> suggestions;
        };
        std::vector<PluginRecEntry> pluginRecs_;

        // ═══ Robot avatar verde celebratorio ════════════════════════════════
        RobotAvatarComponent reportAvatar_;

        // ═══ Scroll state ═══════════════════════════════════════════════════
        int scrollOffset_ = 0;
        int contentHeight_ = 0;

        // ═══ Technical details toggle (default: false = user summary view) ═══
        bool showTechnicalDetails_ = false;
        juce::Rectangle<int> toggleBounds_;
        bool toggleHovered_ = false;

        // ═══ Layout bounds (cacheados en resized) ═══════════════════════════
        juce::Rectangle<int> avatarBounds_;
        juce::Rectangle<int> headerBounds_;
        juce::Rectangle<int> checkmarksBounds_;    // 6 checkmarks (user view)
        juce::Rectangle<int> improvementBounds_;   // Mejora textual (user view)
        juce::Rectangle<int> overallScoreBounds_;
        juce::Rectangle<int> scoreCardsBounds_[5];
        juce::Rectangle<int> trackSummaryBounds_;
        juce::Rectangle<int> diagnosisBounds_;
        juce::Rectangle<int> referenceBounds_;
        juce::Rectangle<int> refinementBounds_;
        juce::Rectangle<int> coachAdaptationBounds_;
        juce::Rectangle<int> progressHistoryHeaderBounds_;
        juce::Rectangle<int> progressHistoryBounds_;
        juce::Rectangle<int> changesHeaderBounds_;
        juce::Rectangle<int> changesListBounds_;
        juce::Rectangle<int> pluginRecsBounds_;
        juce::Rectangle<int> usedPluginsBounds_;
        juce::Rectangle<int> exportButtonBounds_;
        juce::Rectangle<int> newSessionBounds_;

        // ─── Hover state ───────────────────────────────────────────────────
        int hoveredCard_        = -1;
        bool exportButtonHovered_ = false;
        bool newSessionHovered_ = false;

        // ─── Drawing helpers ───────────────────────────────────────────────
        void drawEmptyState(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawLoadingState(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawErrorState(juce::Graphics& g, juce::Rectangle<int> bounds);

        void drawOverallScore(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawUserCheckmarks(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawUserImprovement(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawScoreCard(juce::Graphics& g, juce::Rectangle<int> bounds,
                           const juce::String& label, int score, int initialScore,
                           juce::Colour accentColour, bool isHovered);
        void drawTrackSummary(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawTrackDiagnosis(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawReferenceComparison(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawRefinementSection(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawCoachAdaptation(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawMultiSessionChart(juce::Graphics& g, juce::Rectangle<int> headerBounds,
                                   juce::Rectangle<int> chartBounds);
        void drawChangesSection(juce::Graphics& g, juce::Rectangle<int> headerBounds,
                                juce::Rectangle<int> listBounds);
        void drawPluginRecommendations(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawUsedPlugins(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawExportButton(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawNewSessionButton(juce::Graphics& g, juce::Rectangle<int> bounds);

        // ─── Score colour helpers ──────────────────────────────────────────
        static juce::Colour scoreToColour(int score) noexcept
        {
            if (score >= 80) return MixCoachTheme::success();
            if (score >= 60) return MixCoachTheme::warning();
            return MixCoachTheme::error();
        }

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

        // ─── Report export ─────────────────────────────────────────────────
        [[nodiscard]] juce::String generateHTMLReport() const;
        void exportReport();

        void mouseDown(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EndOfSessionComponent)
    };

} // namespace mixcoach
