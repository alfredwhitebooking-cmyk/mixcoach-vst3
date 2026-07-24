#include "EndOfSessionComponent.h"
#include "../engine/CoachEngine.h"
#include "../engine/ConfidenceScore.h"
#include "../ai/AiCoachAdapter.h"
#include "../audio/AudioAnalyzer.h"
#include "../../Common/types/LogHelper.h"
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#endif

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Static helpers for correction status display
    // ═══════════════════════════════════════════════════════════════════════════
    static juce::Colour statusColour(TrackRecommendation::Status s) noexcept
    {
        switch (s) {
            case TrackRecommendation::Status::Applied:
                return MixCoachTheme::success();
            case TrackRecommendation::Status::UnderApplied:
                return MixCoachTheme::warning();
            case TrackRecommendation::Status::OverApplied:
                return MixCoachTheme::error();
            case TrackRecommendation::Status::Ignored:
                return MixCoachTheme::textMuted();
            case TrackRecommendation::Status::Superseded:
                return MixCoachTheme::info();
            default:
                return MixCoachTheme::textDim();
        }
    }

    static const char* statusIcon(TrackRecommendation::Status s) noexcept
    {
        switch (s) {
            case TrackRecommendation::Status::Applied:
                return "[OK]";  // ✓
            case TrackRecommendation::Status::UnderApplied:
                return "[WARN]";  // ⚠
            case TrackRecommendation::Status::OverApplied:
                return "\xE2\x9C\x97";  // ✗
            case TrackRecommendation::Status::Ignored:
                return "\xE2\x80\x94";  // —
            case TrackRecommendation::Status::Superseded:
                return "\xE2\x86\xB3";  // ↳
            default:
                return "[EMPTY]";  // ○
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor
    // ═══════════════════════════════════════════════════════════════════════════
    EndOfSessionComponent::EndOfSessionComponent()
    {
        setBufferedToImage(true); // Paint once, scroll efficient

        // ═══ Robot avatar verde celebratorio ════════════════════════════════
        addAndMakeVisible(reportAvatar_);
        reportAvatar_.setExpression(AvatarExpression::Celebrating);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setState
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::setState(State newState)
    {
        if (state_ == newState) return;
        state_ = newState;

        if (state_ == State::Loading)
            spinnerTimer_.startTimerHz(30);
        else
            spinnerTimer_.stopTimer();

        resized();
        repaint();
    }


    // ═══════════════════════════════════════════════════════════════════════════
    //  visibilityChanged — Pausa el spinner cuando el plugin no es visible
    // ═══════════════════════════════════════════════════════════════════════════

    void EndOfSessionComponent::visibilityChanged()
    {
        if (isShowing()) {
            spinnerTimer_.startTimerHz(30);
        } else {
            spinnerTimer_.stopTimer();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setErrorMessage
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::setErrorMessage(const juce::String& msg)
    {
        errorMessage_ = msg;
        setState(State::Error);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  refresh — Recolecta datos frescos desde el motor
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::refresh()
    {
        // ─── Set loading state ──────────────────────────────────────────────
        setState(State::Loading);

        if (coachEngine_ == nullptr || audioAnalyzer_ == nullptr) {
            setErrorMessage("Coach engine or audio analyzer not available.\nPlease ensure the session is active and try again.");
            return;
        }

        // ─── 1. Compute MixScore ──────────────────────────────────────────────
        juce::String genre = genre_.isNotEmpty() ? genre_ : coachEngine_->getSetupGenre();
        currentScore_      = MixScore::compute(*coachEngine_, *audioAnalyzer_, genre);

        // ─── 2. Gather correction history ─────────────────────────────────────
        correctionHistory_ = coachEngine_->getCorrectionHistory();
        totalCorrections_  = static_cast<int>(correctionHistory_.size());
        appliedCorrections_ = 0;
        for (const auto& entry : correctionHistory_) {
            if (entry.finalStatus == TrackRecommendation::Status::Applied)
                appliedCorrections_++;
        }

        // ─── 3. Gather mix history (last 30 entries) ──────────────────────────
        mixHistory_ = coachEngine_->getMixHistory(30);

        // ─── 4. Gather track statistics ───────────────────────────────────────
        activeTrackCount_    = currentScore_.activeTrackCount;
        clippingTrackCount_  = currentScore_.clippingTrackCount;
        lowSignalTrackCount_ = currentScore_.lowSignalTrackCount;
        masterPeakDb_        = currentScore_.masterPeakDb;
        masterIntegratedLUFS_ = currentScore_.masterIntegratedLUFS;
        masterCorrelation_   = currentScore_.masterCorrelation;

        // ─── 4a. Populate \"Has aprendido\" phases from domain scores ──────────
        {
            completedPhases_.clear();
            struct PhaseDef {
                const char* emoji;
                const char* name;
            };
            static const PhaseDef kPhases[7] = {
                {"\xF0\x9F\x8C\x9F", "Gain Staging"},
                {"\xE2\x9A\x96", "Balance"},
                {"EQ", "EQ"},
                {"Comp", "Compression"},
                {"Spc", "Space"},
                {"Auto", "Automation"},
                {"Mstr", "Master Check"}
            };
            int domainScores[7] = {
                currentScore_.gain,
                currentScore_.gain,            // Balance → gain (level-related)
                currentScore_.tonal,
                currentScore_.dynamics,
                currentScore_.spatial,
                0,                              // Automation: no dedicated score yet
                currentScore_.reference
            };

            for (int i = 0; i < 7; ++i) {
                PhaseTag tag;
                tag.emoji = juce::String(kPhases[i].emoji);
                tag.name  = juce::String(kPhases[i].name);
                tag.score = domainScores[i];
                tag.completed = (domainScores[i] > 0);
                completedPhases_.push_back(tag);
            }
        }

        // ─── 4b. Compute ConfidenceScore from engine + correction learner
        {
            auto& sharedData = coachEngine_->getSharedData();
            confidenceScore_ = ConfidenceScore::compute(*coachEngine_, sharedData, sessionDurationUs_);

            auto& correctionLearner = coachEngine_->getCorrectionLearner();
            verifyTotalAttempts_ = correctionLearner.getTotalVerifyAttempts();
            verifyInvalidated_   = correctionLearner.getVerifyInvalidatedCount();
            verifyConfidencePct_ = correctionLearner.getVerifyConfidencePercent();

            LogHelper::writeToLog("[EndOfSession] Confidence: " + juce::String(confidenceScore_.overall)
                                  + "/100 | Verify attempts=" + juce::String(verifyTotalAttempts_)
                                  + " invalidated=" + juce::String(verifyInvalidated_));
        }

        // ─── 4c. Gather session metadata from real engine state
        {
            auto ctx = coachEngine_->buildSessionContext();
            sessionDurationUs_ = ctx.sessionDurationUs;
            achievementCount_  = ctx.achievementCount;
            masterTruePeakDb_  = ctx.masterTruePeak;
            drumTracks_       = ctx.drumTracks;
            bassTracks_       = ctx.bassTracks;
            guitarTracks_     = ctx.guitarTracks;
            keysTracks_       = ctx.keysTracks;
            vocalTracks_      = ctx.vocalTracks;
            fxTracks_         = ctx.fxTracks;
            melodyTracks_     = ctx.melodyTracks;
            unknownTracks_    = ctx.unknownTracks;

            // Capture SessionProgression phase + progress
            auto& prog = coachEngine_->getSessionProgression();
            sessionProgressionPhase_    = prog.currentPhase;
            sessionProgressionProgress_ = prog.getOverallProgress();
        }

        // ─── 5. Gather per-track diagnosis        // ─── 5. Gather per-track diagnosis ────────────────────────────────────
        trackDiagnosis_.clear();
        {
            // Run consolidated per-track analysis
            coachEngine_->analyzeAllTracks();

            auto& shared = coachEngine_->getSharedData();
            auto& registry = shared.getSlotRegistry();
            registry.forEachActive([&](const SlotInfo& info) {
                int idx = info.slotIndex;
                if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;

                auto advice = coachEngine_->getTrackAdvice(idx);
                if (!advice.isActionable() && advice.status == CoachEngine::TrackAdvice::Status::OnTarget
                    && advice.gain.status == CoachEngine::TrackGainAdvice::Status::NoSignal
                    && advice.dynamics.status == CoachEngine::TrackDynamicsAdvice::Status::NoSignal)
                    return; // Skip tracks with no signal at all

                TrackDiagnosisEntry entry;
                entry.slotIndex  = idx;
                entry.trackName  = juce::String(info.trackName);
                entry.roleName   = juce::String(getRoleName(coachEngine_->getTrackRole(idx)));
                entry.gainStatus     = static_cast<int>(advice.gain.status);
                entry.dynamicsStatus = static_cast<int>(advice.dynamics.status);
                entry.tonalStatus    = static_cast<int>(advice.tonal.status);
                entry.phaseStatus    = static_cast<int>(advice.phase.status);
                entry.consolidatedSeverity = advice.consolidatedSeverity;

                if (advice.isActionable()) {
                    if (advice.worstDomain == CoachEngine::TrackAdvice::Domain::Gain)
                        entry.worstMessage = advice.gain.message;
                    else if (advice.worstDomain == CoachEngine::TrackAdvice::Domain::Dynamics)
                        entry.worstMessage = advice.dynamics.message;
                    else if (advice.worstDomain == CoachEngine::TrackAdvice::Domain::Tonal)
                        entry.worstMessage = advice.tonal.message;
                    else if (advice.worstDomain == CoachEngine::TrackAdvice::Domain::Phase)
                        entry.worstMessage = advice.phase.message;
                }

                trackDiagnosis_.push_back(entry);
            });

            // Sort by severity descending (worst first)
            std::sort(trackDiagnosis_.begin(), trackDiagnosis_.end(),
                      [](const TrackDiagnosisEntry& a, const TrackDiagnosisEntry& b) {
                          return a.consolidatedSeverity > b.consolidatedSeverity;
                      });
        }

        // ─── 5b. Collect plugin recommendations per track
        pluginRecs_.clear();
        if (coachEngine_ != nullptr) {
            auto& pluginProvider = coachEngine_->getPluginSuggestionsProvider();
            for (const auto& diag : trackDiagnosis_) {
                if (diag.consolidatedSeverity < 0.1f) continue;

                // Determine domain + issueType from the worst domain
                juce::String domain, issueType;
                float delta = 0.0f, freqHz = 0.0f;

                if (diag.gainStatus >= 1) {
                    domain = "gain";
                    issueType = (diag.gainStatus >= 2) ? "OFF_TARGET" : "NEAR_TARGET";
                } else if (diag.dynamicsStatus >= 1) {
                    domain = "dynamics";
                    issueType = (diag.dynamicsStatus >= 2) ? "HIGH_CREST" : "LOW_CREST";
                } else if (diag.tonalStatus >= 1) {
                    domain = "tonal";
                    issueType = (diag.tonalStatus >= 2) ? "EXCESS" : "NEAR_LIMIT";
                } else if (diag.phaseStatus >= 1) {
                    domain = "phase";
                    issueType = (diag.phaseStatus >= 2) ? "PHASE_ISSUE" : "NEAR_PHASE";
                } else {
                    continue; // No actionable domain
                }

                // Build TrackProblemData and populate suggestions
                TrackProblemData trackData;
                trackData.trackName = diag.trackName;
                trackData.roleName  = diag.roleName;
                trackData.slotIndex = diag.slotIndex;
                trackData.severity  = diag.consolidatedSeverity;
                trackData.domain    = domain;
                trackData.issueType = issueType;
                trackData.delta     = delta;
                trackData.frequencyHz = freqHz;

                populatePluginSuggestions(pluginProvider, trackData, domain, issueType, delta, freqHz);

                if (!trackData.pluginSuggestions.empty()) {
                    PluginRecEntry rec;
                    rec.trackName  = diag.trackName;
                    rec.roleName   = diag.roleName;
                    rec.slotIndex  = diag.slotIndex;
                    rec.severity   = diag.consolidatedSeverity;
                    rec.domain     = domain;
                    rec.issueType  = issueType;
                    rec.delta      = delta;
                    rec.frequencyHz = freqHz;
                    rec.suggestions = std::move(trackData.pluginSuggestions);
                    pluginRecs_.push_back(std::move(rec));
                }
            }
        }

        // ─── 6. Load multi-session history ─────────────────────────────────────
        
        // Collect unique plugin names from pluginRecs_ suggestions
        usedPlugins_.clear();
        for (const auto& rec : pluginRecs_) {
            for (const auto& sug : rec.suggestions) {
                if (sug.pluginName.isNotEmpty() && !usedPlugins_.contains(sug.pluginName))
                    usedPlugins_.add(sug.pluginName);
            }
        }

        // Merge plugins that the user actually applied during the session
        for (const auto& p : appliedPlugins_) {
            if (p.isNotEmpty() && !usedPlugins_.contains(p))
                usedPlugins_.add(p);
        }

        // Sort alphabetically
        usedPlugins_.sort(true);

sessionHistory_ = AiCoachAdapter::loadSessionHistory();

        // ─── 6. Fetch reference comparison data ──────────────────────────────
        {
            if (coachEngine_->hasReference()) {
                referenceProfile_ = coachEngine_->getCachedDifferenceProfile();
                if (!referenceProfile_.valid || !referenceProfile_.hasData()) {
                    // Build fresh if cache is stale
                    referenceProfile_ = coachEngine_->buildDifferenceProfile();
                }
            }
            else {
                referenceProfile_ = DifferenceProfile{};
            }
        }

        // ─── 6.5. Fetch refinement profile (artistic quality) ──────────────────
        {
            refinementProfile_ = coachEngine_->getCachedRefinementProfile();
        }

        // ─── 6.6. Fetch coach adaptation history from FeedbackCollector ──────────
        {
            auto& feedbackCollector = coachEngine_->getFeedbackCollector();
            coachAdaptationHistory_ = feedbackCollector.getAdjustmentHistory();
        }

        // ─── 7. Capture initial score (only once — at start of session) ──────────
        if (!initialScoreCaptured_) {
            initialScoreCaptured_ = true;

            if (!sessionHistory_.empty()) {
                // Use the most recent periodic snapshot as "where you started this session".
                // Skip the very last entry if it was saved within the last 30 seconds,
                // because it's likely the "end session" snapshot just saved by
                // handleEndSession() — using it would make initial == final.
                int useIdx = static_cast<int>(sessionHistory_.size()) - 1;
                if (sessionHistory_.size() >= 2) {
                    // timestampUs is in microseconds — compare in us
                    int64_t nowUs  = juce::Time::currentTimeMillis() * 1000;
                    int64_t lastUs = sessionHistory_[useIdx].timestampUs;
                    if (lastUs > 0 && nowUs - lastUs < 30 * 1000 * 1000) // < 30 seconds
                        --useIdx;                                          // use the one before it
                }
                const auto& snap = sessionHistory_[useIdx];
                initialScore_.overall     = snap.mixScoreOverall;
                initialScore_.gain        = snap.domainGain;
                initialScore_.tonal       = snap.domainTonal;
                initialScore_.dynamics    = snap.domainDynamics;
                initialScore_.spatial     = snap.domainSpatial;
                initialScore_.reference   = snap.domainReference;
            }
            else {
                // No history — use current as baseline (delta will be 0)
                initialScore_ = currentScore_;
            }

            LogHelper::writeToLog("[EndOfSession] Initial score captured: " + juce::String(initialScore_.overall));
        }

        // Recompute layout with diagnosis data (which was empty when resized() first ran)
        resized();

        LogHelper::writeToLog("[EndOfSession] refreshed: score=" + juce::String(currentScore_.overall)
                              + " tracks=" + juce::String(activeTrackCount_)
                              + " corrections=" + juce::String(totalCorrections_)
                              + " snapshots=" + juce::String((int)sessionHistory_.size()));

        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  resized — Computa bounds de todos los elementos
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::resized()
    {
        // ─── Non-Ready states: only show state overlay ──────────────────────
        if (state_ != State::Ready) {
            headerBounds_ = overallScoreBounds_ = trackSummaryBounds_ = {};
            for (auto& b : scoreCardsBounds_) b = {};
            diagnosisBounds_ = referenceBounds_ = refinementBounds_ = {};
            coachAdaptationBounds_ = {};
            progressHistoryHeaderBounds_ = progressHistoryBounds_ = {};
            changesHeaderBounds_ = changesListBounds_ = {};
            pluginRecsBounds_ = {};
            exportButtonBounds_ = toggleBounds_ = newSessionBounds_ = {};
            avatarBounds_ = {};
            reportAvatar_.setVisible(false);
            return;
        }

        auto area = getLocalBounds().reduced(MixCoachTheme::spacingXL, MixCoachTheme::spacingMD);

        // ─── Robot avatar celebratorio ──────────────────────────────────────
        auto avatarArea = area.removeFromTop(72).removeFromLeft(72);
        avatarBounds_ = avatarArea.withSizeKeepingCentre(56, 56);
        // Aplicar scrollOffset_ al avatar
        // El contenido del reporte usa g.setOrigin(0, -scrollOffset_) en paint(),
        // pero reportAvatar_ es un child component que JUCE renderiza por separado.
        // Si no sincronizamos su posición con scrollOffset_, el avatar se queda
        // fijo mientras el texto scrollea — parece "flotar" incorrectamente.
        reportAvatar_.setBounds(avatarBounds_.withY(avatarBounds_.getY() - scrollOffset_));
        reportAvatar_.setVisible(true);

        // ─── Toggle technical details button ─────────────────────────────────
        auto toggleRow = area.removeFromTop(24);
        toggleBounds_   = toggleRow.withSizeKeepingCentre(260, 20);
        area.removeFromTop(MixCoachTheme::spacingXS);

        // ─── Header: "SESSION REPORT" (shifted right to make room for avatar) ─
        headerBounds_ = area.removeFromTop(36);
        area.removeFromTop(MixCoachTheme::spacingLG);

        // ═══ USER SUMMARY VIEW (showTechnicalDetails_ == false) ════════════
        if (!showTechnicalDetails_) {
            scrollOffset_ = 0;
            overallScoreBounds_ = area.removeFromTop(140);
            area.removeFromTop(MixCoachTheme::spacingSM);
            checkmarksBounds_ = area.removeFromTop(190);
            area.removeFromTop(MixCoachTheme::spacingSM);
            improvementBounds_ = area.removeFromTop(32);
            area.removeFromTop(MixCoachTheme::spacingMD);

            // Clear all technical sections
            for (auto& b : scoreCardsBounds_) b = {};
            trackSummaryBounds_ = diagnosisBounds_ = referenceBounds_ = refinementBounds_ = {};
            coachAdaptationBounds_ = {};
            progressHistoryHeaderBounds_ = progressHistoryBounds_ = {};
            changesHeaderBounds_ = changesListBounds_ = {};

            auto btnRow = area.removeFromTop(48);
            exportButtonBounds_ = btnRow.withSizeKeepingCentre(220, 40);
            // GAP #3: Botón "Nueva sesión"
            auto nsRow = area.removeFromTop(48);
            newSessionBounds_ = nsRow.withSizeKeepingCentre(180, 36);
            contentHeight_ = getLocalBounds().getHeight();
            return;
        }

        // ═══ TECHNICAL DETAILS VIEW (showTechnicalDetails_ == true) ═══════
        // ─── Overall score card (tall) ────────────────────────
        overallScoreBounds_ = area.removeFromTop(120);
        area.removeFromTop(MixCoachTheme::spacingMD);

        // ─── 5 Score domain cards row ─────────────────────────────────────────
        auto cardsRow      = area.removeFromTop(110);
        int cardW          = (cardsRow.getWidth() - MixCoachTheme::spacingSM * 4) / 5;
        int cardGap        = MixCoachTheme::spacingSM;
        for (int i = 0; i < 5; ++i) {
            scoreCardsBounds_[i] = cardsRow.removeFromLeft(cardW);
            if (i < 4) cardsRow.removeFromLeft(cardGap);
        }
        area.removeFromTop(MixCoachTheme::spacingMD);

        // ─── Track summary card ──────────────────────────────────────────────
        trackSummaryBounds_ = area.removeFromTop(118);
        area.removeFromTop(MixCoachTheme::spacingMD);

        // ─── Per-track Diagnosis section (solo si showTechnicalDetails_) ──────
        {
            if (showTechnicalDetails_) {
                int nDiag = juce::jmin((int)trackDiagnosis_.size(), 8);
                int diagH = (nDiag > 0) ? 32 + nDiag * 22 : 0;
                diagnosisBounds_ = area.removeFromTop(diagH);
                if (diagH > 0) area.removeFromTop(MixCoachTheme::spacingMD);
            } else {
                diagnosisBounds_ = {};
            }
        }

        // ─── Reference Comparison section (solo si showTechnicalDetails_) ─────
        {
            if (showTechnicalDetails_) {
                bool hasRef = referenceProfile_.valid && referenceProfile_.hasData();
                referenceBounds_ = area.removeFromTop(hasRef ? 200 : 0);
                if (hasRef) area.removeFromTop(MixCoachTheme::spacingMD);
            } else {
                referenceBounds_ = {};
            }
        }

        // ─── Refinement Profile section (solo si showTechnicalDetails_) ────────
        {
            if (showTechnicalDetails_) {
                bool hasRefinement = refinementProfile_.valid && refinementProfile_.isRelevant;
                refinementBounds_ = area.removeFromTop(hasRefinement ? 160 : 0);
                if (hasRefinement) area.removeFromTop(MixCoachTheme::spacingMD);
            } else {
                refinementBounds_ = {};
            }
        }

        // ─── Coach Adaptation section (solo si showTechnicalDetails_) ───────────
        {
            if (showTechnicalDetails_) {
                int nAdj = (int)coachAdaptationHistory_.size();
                bool hasAdaptation = nAdj > 0;
                int adjH = hasAdaptation ? juce::jmin(50 + nAdj * 24, 200) : 0;
                coachAdaptationBounds_ = area.removeFromTop(adjH);
                if (hasAdaptation) area.removeFromTop(MixCoachTheme::spacingMD);
            } else {
                coachAdaptationBounds_ = {};
            }
        }

        // ─── Progress History section (multi-session) ────────────────────────
        progressHistoryHeaderBounds_ = area.removeFromTop(28);
        area.removeFromTop(MixCoachTheme::spacingSM);
        progressHistoryBounds_ = area.removeFromTop(120);
        area.removeFromTop(MixCoachTheme::spacingMD);

        // ─── Changes section ──────────────────────────────────────────────────
        changesHeaderBounds_ = area.removeFromTop(28);
        area.removeFromTop(MixCoachTheme::spacingSM);
        changesListBounds_ = area.removeFromTop(juce::jmin(200, juce::jmax(0, area.getHeight() - 60)));
        area.removeFromTop(MixCoachTheme::spacingMD);

        // ─── Plugin Recommendations section ─────────────────────────
        {
            if (showTechnicalDetails_ && !pluginRecs_.empty()) {
                int nRecs = juce::jmin((int)pluginRecs_.size(), 6);
                int recH = 30 + nRecs * 34;
                pluginRecsBounds_ = area.removeFromTop(recH);
                area.removeFromTop(MixCoachTheme::spacingMD);
            } else {
                pluginRecsBounds_ = {};
            }
        }

        // Plugins Used section
        {
            if (showTechnicalDetails_ && !usedPlugins_.isEmpty()) {
                int nPlugins = juce::jmin(usedPlugins_.size(), 10);
                int plugH = 30 + nPlugins * 20;
                usedPluginsBounds_ = area.removeFromTop(plugH);
                area.removeFromTop(MixCoachTheme::spacingMD);
            } else {
                usedPluginsBounds_ = {};
            }
        }

        // ─── Export button ────────────────────────────────────────────────────
        auto btnRow     = area.removeFromTop(48);
        exportButtonBounds_ = btnRow.withSizeKeepingCentre(220, 40);

        // GAP #3: Botón "Nueva sesión"
        area.removeFromTop(MixCoachTheme::spacingMD);
        auto nsRow = area.removeFromTop(48);
        newSessionBounds_ = nsRow.withSizeKeepingCentre(180, 36);

        // Total content height for scrolling
        contentHeight_ = getLocalBounds().getHeight();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Renderiza el reporte completo
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::paint(juce::Graphics& g)
    {
        // ─── Background ────────────────────────────────────────────────────────
        g.fillAll(MixCoachTheme::bgCanvas());

        // ─── Green glow behind celebratory robot avatar ─────────────────────
        if (avatarBounds_.getWidth() > 0 && state_ == State::Ready) {
            auto avatarCentre = avatarBounds_.toFloat().getCentre();
            float glowR = avatarBounds_.getWidth() * 0.9f;
            juce::ColourGradient greenGlow(
                MixCoachTheme::success().withAlpha(0.18f),
                avatarCentre,
                MixCoachTheme::success().withAlpha(0.0f),
                avatarCentre.translated(glowR * 1.2f, glowR * 1.2f),
                true);
            g.setGradientFill(greenGlow);
            g.fillEllipse(avatarCentre.x - glowR, avatarCentre.y - glowR,
                          glowR * 2.0f, glowR * 2.0f);

            // Second outer glow ring
            juce::ColourGradient outerGlow(
                MixCoachTheme::success().withAlpha(0.06f),
                avatarCentre,
                MixCoachTheme::success().withAlpha(0.0f),
                avatarCentre.translated(glowR * 2.0f, glowR * 2.0f),
                true);
            g.setGradientFill(outerGlow);
            g.fillEllipse(avatarCentre.x - glowR * 1.8f, avatarCentre.y - glowR * 1.8f,
                          glowR * 3.6f, glowR * 3.6f);
        }

        // ─── State dispatch ──────────────────────────────────────────────────
        switch (state_) {
            case State::Empty:
                drawEmptyState(g, getLocalBounds());
                return;
            case State::Loading:
                drawLoadingState(g, getLocalBounds());
                return;
            case State::Error:
                drawErrorState(g, getLocalBounds());
                return;
            case State::Ready:
                break;
        }

        // ─── Apply scroll offset ──────────────────────────────────────────────
        g.saveState();
        g.setOrigin(0, -scrollOffset_);

        // ─── Toggle technical details button ───────────────────────────────────
        {
            auto bounds = toggleBounds_;
            juce::Colour toggleCol = toggleHovered_
                ? MixCoachTheme::accent().brighter(0.3f)
                : MixCoachTheme::textMuted();

            g.setColour(MixCoachTheme::bgSurface().withAlpha(0.5f));
            g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
            g.setColour(toggleCol.withAlpha(0.3f));
            g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 0.5f);

            g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
            g.setColour(toggleCol);
            juce::String toggleLabel = showTechnicalDetails_
                ? juce::CharPointer_UTF8("[EXPAND]  Ocultar detalles t\xC3\xA9" "cnicos")
                : juce::CharPointer_UTF8("[COLLAPSE]  Mostrar detalles t\xC3\xA9" "cnicos");
            g.drawText(toggleLabel, bounds, juce::Justification::centred);
        }

        // ─── Header ───────────────────────────────────────────────────────────
        {
            auto bounds = headerBounds_;
            g.setFont(juce::Font(juce::FontOptions(22.0f)).boldened());
            g.setColour(MixCoachTheme::textBright());
            g.drawText("SESSION REPORT",
                       bounds, juce::Justification::centredLeft);

            // Subtitle right-aligned
            g.setFont(juce::Font(juce::FontOptions(11.0f)));
            g.setColour(MixCoachTheme::textMuted());
            juce::String subtitle;
            if (currentScore_.genre.isNotEmpty())
                subtitle = "Genre: " + currentScore_.genre + "  |  ";
            subtitle += juce::Time::getCurrentTime().toString(true, true);
            g.drawText(subtitle, bounds, juce::Justification::centredRight);

            // Divider line
            g.setColour(MixCoachTheme::divider().withAlpha(0.3f));
            g.fillRect(bounds.getX(), bounds.getBottom() - 1, bounds.getWidth(), 1);
        }

        // ═══ USER SUMMARY VIEW ═════════════════════════════════════════════
        if (!showTechnicalDetails_) {
            drawOverallScore(g, overallScoreBounds_);
            drawUserCheckmarks(g, checkmarksBounds_);
            drawUserImprovement(g, improvementBounds_);
            drawExportButton(g, exportButtonBounds_);
            drawNewSessionButton(g, newSessionBounds_);
            g.restoreState();
            return;
        }

        // ─── Overall Score ────────────────────────────────────────────────────
        drawOverallScore(g, overallScoreBounds_);

        // ─── Domain Score Cards ───────────────────────────────────────────────
        {
            static const char* labels[5] = {"GAIN", "TONAL", "DYNAMICS", "SPATIAL", "REFERENCE"};
            int scores[5] = {
                currentScore_.gain,
                currentScore_.tonal,
                currentScore_.dynamics,
                currentScore_.spatial,
                currentScore_.reference
            };
            int initScores[5] = {
                initialScore_.gain,
                initialScore_.tonal,
                initialScore_.dynamics,
                initialScore_.spatial,
                initialScore_.reference
            };
            for (int i = 0; i < 5; ++i) {
                drawScoreCard(g, scoreCardsBounds_[i],
                              juce::String(labels[i]),
                              scores[i],
                              initScores[i],
                              domainColour(i),
                              hoveredCard_ == i);
            }
        }

        // ─── Track Summary ────────────────────────────────────────────────────
        drawTrackSummary(g, trackSummaryBounds_);

        // ─── Per-Track Diagnosis ──────────────────────────────────────────────
        if (!trackDiagnosis_.empty())
            drawTrackDiagnosis(g, diagnosisBounds_);

        // ─── Reference Comparison ─────────────────────────────────────────────
        if (referenceProfile_.valid && referenceProfile_.hasData())
            drawReferenceComparison(g, referenceBounds_);

        // ─── Refinement Profile (artistic quality: Depth, Impact, Movement, Glue, Emotion) ──
        if (refinementProfile_.valid && refinementProfile_.isRelevant)
            drawRefinementSection(g, refinementBounds_);

        // ─── Coach Adaptation (FeedbackCollector threshold adjustments) ────────
        if (!coachAdaptationHistory_.empty())
            drawCoachAdaptation(g, coachAdaptationBounds_);

        // ─── Multi-Session Progress History ───────────────────────────────────
        drawMultiSessionChart(g, progressHistoryHeaderBounds_, progressHistoryBounds_);

        // ─── Changes Section ──────────────────────────────────────────────────
        drawChangesSection(g, changesHeaderBounds_, changesListBounds_);

        // ─── Plugin Recommendations ──────────────────────
        if (showTechnicalDetails_ && !pluginRecs_.empty() && pluginRecsBounds_.getHeight() > 0)
            drawPluginRecommendations(g, pluginRecsBounds_);

        // Plugins Used
        if (showTechnicalDetails_ && usedPluginsBounds_.getHeight() > 0)
            drawUsedPlugins(g, usedPluginsBounds_);

        // ─── Export Button ────────────────────────────────────────────────────
        drawExportButton(g, exportButtonBounds_);
        // GAP #3: Botón "Nueva sesión" en ambas vistas
        drawNewSessionButton(g, newSessionBounds_);

        g.restoreState();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawUserCheckmarks — 7 checkmarks de fases aprendidas (\"Has aprendido\")
    //  + Match score vs referencia como barra adicional.
    //  Muestra fila cualitativa con emoji + label por cada fase completada.
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::drawUserCheckmarks(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        // ─── Panel background ─────────────────────────────────────────────────
        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_large);

        // ─── Header: \"Has aprendido\" ────────────────────────────────────────
        auto headerLabel = bounds.removeFromTop(22).reduced(MixCoachTheme::spacingMD, 0);
        g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText(juce::CharPointer_UTF8("[LEARN]  HAS APRENDIDO"),
                   headerLabel, juce::Justification::centredLeft);

        auto inner = bounds.reduced(MixCoachTheme::spacingMD, MixCoachTheme::spacingXS);

        // ─── Match Score vs Referencia (barra destacada en la parte superior) ──
        if (currentScore_.hasReference && currentScore_.reference > 0) {
            auto matchRow = inner.removeFromTop(22);
            g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
            g.setColour(MixCoachTheme::accentCyan());
            g.drawText(juce::CharPointer_UTF8("[TARGET]  MATCH SCORE vs REFERENCIA"),
                       matchRow.removeFromLeft(190), juce::Justification::centredLeft);

            // Mini bar
            auto barArea = matchRow.reduced(0, 3);
            g.setColour(MixCoachTheme::bgDarker());
            g.fillRoundedRectangle(barArea.toFloat(), 3.0f);
            float fillPct = juce::jlimit(0.0f, 1.0f, currentScore_.reference / 100.0f);
            auto fillBounds = barArea.withWidth((int)(barArea.getWidth() * fillPct));
            if (fillBounds.getWidth() > 2) {
                g.setColour(juce::Colour(0xFFEC4899)); // Pink — reference colour
                g.fillRoundedRectangle(fillBounds.toFloat(), 3.0f);
            }

            // Qualitative label
            juce::String matchQual;
            if (currentScore_.reference >= 80) matchQual = "\xC2" "\xA1" "Excelente match!";
            else if (currentScore_.reference >= 60) matchQual = "Buena coincidencia";
            else if (currentScore_.reference >= 40) matchQual = "Cercana";
            else matchQual = "Lejana";
            g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
            g.setColour(juce::Colour(0xFFEC4899).brighter(0.3f));
            g.drawText(matchQual, barArea, juce::Justification::centredRight);

            inner.removeFromTop(MixCoachTheme::spacingXS);
        }

        // ─── 7 fases de aprendizaje (Has aprendido tags) ─────────────────────
        int rowH = juce::jmin(16, inner.getHeight() / 7);

        for (int i = 0; i < 7 && i < (int)completedPhases_.size(); ++i) {
            const auto& phase = completedPhases_[i];
            auto row = inner.removeFromTop(rowH);

            // Row divider (except last)
            if (i < 6) {
                g.setColour(MixCoachTheme::divider().withAlpha(0.06f));
                g.fillRect(inner.getX(), row.getBottom(), inner.getWidth(), 1);
            }

            // ─── Checkmark / Done indicator ─────────────────────────────────
            auto indArea = row.removeFromLeft(18);
            g.setFont(juce::Font(juce::FontOptions(12.0f)));
            if (phase.completed) {
                g.setColour(MixCoachTheme::success());
                g.drawText("[OK]", indArea, juce::Justification::centred);
            } else {
                g.setColour(MixCoachTheme::textMuted().withAlpha(0.2f));
                g.drawText("[EMPTY]", indArea, juce::Justification::centred);
            }

            // ─── Phase emoji ────────────────────────────────────────────────
            auto emojiArea = row.removeFromLeft(18);
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(phase.completed ? MixCoachTheme::textDim() : MixCoachTheme::textMuted().withAlpha(0.3f));
            g.drawText(phase.emoji, emojiArea, juce::Justification::centred);

            // ─── Phase name ─────────────────────────────────────────────────
            auto nameArea = row.removeFromLeft(80);
            g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
            g.setColour(phase.completed ? MixCoachTheme::textSecondary() : MixCoachTheme::textMuted().withAlpha(0.3f));
            g.drawText(phase.name, nameArea, juce::Justification::centredLeft);

            // ─── Qualitative score pill ──────────────────────────────────────
            auto qualArea = row;
            if (phase.completed) {
                juce::String qualText;
                juce::Colour qualColour;
                if (phase.score >= 80) {
                    qualText = "\xC2" "\xA1" "Excelente!";
                    qualColour = MixCoachTheme::success();
                } else if (phase.score >= 60) {
                    qualText = "Bien";
                    qualColour = MixCoachTheme::warning();
                } else if (phase.score >= 40) {
                    qualText = "Mejorable";
                    qualColour = MixCoachTheme::error().withAlpha(0.7f);
                } else {
                    qualText = "A trabajar";
                    qualColour = MixCoachTheme::error();
                }

                // Mini pill badge
                // Calcular ancho del texto + padding, limitar al ancho disponible
                const int textW = juce::GlyphArrangement::getStringWidthInt(
                    juce::Font(juce::FontOptions(7.5f)).boldened(), qualText) + 14;
                const int availW = qualArea.getWidth();
                const int pillW = (textW < availW) ? textW : availW;
                auto pill = qualArea.withWidth(pillW);
                g.setColour(qualColour.withAlpha(0.10f));
                g.fillRoundedRectangle(pill.toFloat(), 3.0f);
                g.setColour(qualColour);
                g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
                g.drawText(qualText, pill, juce::Justification::centred);
            } else {
                g.setFont(juce::Font(juce::FontOptions(7.5f)));
                g.setColour(MixCoachTheme::textMuted().withAlpha(0.2f));
                g.drawText("Pendiente", qualArea, juce::Justification::centredLeft);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawUserImprovement — Texto de mejora cualitativa para vista usuario
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::drawUserImprovement(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        if (!initialScoreCaptured_ || initialScore_.overall <= 0) {
            // No improvement data — just show a subtle tip
            MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_small);
            g.setFont(juce::Font(juce::FontOptions(9.0f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.drawText(juce::CharPointer_UTF8("\xF0\x9F\x92\xA1  Sigue mezclando para ver tu progreso"),
                       bounds.reduced(MixCoachTheme::spacingSM, 0), juce::Justification::centredLeft);
            return;
        }

        int delta = currentScore_.overall - initialScore_.overall;

        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_small);

        auto inner = bounds.reduced(MixCoachTheme::spacingSM, 0);

        // ─── Emoji + improvement text ────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());

        juce::String impText;
        juce::Colour impColour;
        if (delta >= 10) {
            impText = "\xF0\x9F\x8C\x9F  Gran mejora respecto a tu \xC3\xBAltima sesi\xC3\xB3n";
            impColour = MixCoachTheme::success();
        } else if (delta >= 3) {
            impText = "\xE2\x9C\xA8  Vas mejorando, sigue as\xC3\xAD";
            impColour = MixCoachTheme::warning();
        } else if (delta > -3) {
            impText = "[TREND]  Manteniendo el nivel";
            impColour = MixCoachTheme::accentCyan();
        } else if (delta > -10) {
            impText = "\xF0\x9F\x92\xAA  \xC3\x81nimo, la pr\xC3\xB3xima ser\xC3\xA1 mejor";
            impColour = MixCoachTheme::warning().darker(0.3f);
        } else {
            impText = "\xF0\x9F\x94\xBA  Sigue practicando, el progreso llegar\xC3\xA1";
            impColour = MixCoachTheme::error().withAlpha(0.7f);
        }

        g.setColour(impColour);
        g.drawText(impText, inner, juce::Justification::centredLeft);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawOverallScore — Tarjeta principal con score grande + barra visual
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::drawOverallScore(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        // ─── Panel background ─────────────────────────────────────────────────
        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_large);

        auto inner = bounds.reduced(MixCoachTheme::spacingLG, MixCoachTheme::spacingSM);

        // ─── Emoji + Qualitative score (NO numbers) ─────────────────────────
        auto leftArea = inner.removeFromLeft(100);

        {
            // Glow circle behind indicator
            float cx = (float)leftArea.getCentreX();
            float cy = (float)leftArea.getCentreY();
            float r  = 38.0f;
            juce::ColourGradient glowGrad(
                scoreToColour(currentScore_.overall).withAlpha(0.12f),
                cx, cy,
                scoreToColour(currentScore_.overall).withAlpha(0.0f),
                cx + r * 1.5f, cy + r * 1.5f, true);
            g.setGradientFill(glowGrad);
            g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

            // Emoji grande según rango de score
            juce::String emoji;
            if (currentScore_.overall >= 80)      emoji = "\xF0\x9F\x8C\x9F";  // 🌟 Excelente
            else if (currentScore_.overall >= 60) emoji = "\xF0\x9F\x91\x8D";  // 👍 Bueno
            else if (currentScore_.overall >= 40) emoji = "\xF0\x9F\x92\xAA";  // 💪 Mejorable
            else                                  emoji = "[TREND]";  // 📈 En progreso

            g.setFont(juce::Font(juce::FontOptions(42.0f)));
            g.setColour(scoreToColour(currentScore_.overall));
            g.drawText(emoji, leftArea, juce::Justification::centred);
        }

        // ─── Right side: qualitative label + progress bar ─────────────────
        auto rightArea = inner;

        // Qualitative label (NO numeric score)
        juce::String qualLabel;
        if (currentScore_.overall >= 80)      qualLabel = "\xC2""\xA1""Excelente mezcla!";
        else if (currentScore_.overall >= 60) qualLabel = "Buen trabajo, sigue as\xC3\xAD";
        else if (currentScore_.overall >= 40) qualLabel = "Vamos por buen camino";
        else                                  qualLabel = "Sigamos trabajando";

        g.setFont(juce::Font(juce::FontOptions(16.0f)).boldened());
        g.setColour(scoreToColour(currentScore_.overall));
        g.drawText(qualLabel, rightArea.removeFromTop(26),
                   juce::Justification::centredLeft);

        // Subtitle (cualitativo, sin scores numéricos)
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.setColour(MixCoachTheme::textDim());
        juce::String subtitle = "Tracks: " + juce::String(activeTrackCount_);
        if (currentScore_.hasReference) {
            juce::String refQual;
            if (currentScore_.reference >= 70)      refQual = "Buena";
            else if (currentScore_.reference >= 40) refQual = "Regular";
            else                                    refQual = "Lejana";
            subtitle += "  |  Reference match: " + refQual;
        // Session duration (from real engine state)
        if (sessionDurationUs_ > 0) {
            int64_t secs = sessionDurationUs_ / 1000000;
            juce::String durStr;
            if (secs >= 3600)
                durStr = juce::String((int)(secs / 3600)) + "h " + juce::String((int)((secs % 3600) / 60)) + "m";
            else if (secs >= 60)
                durStr = juce::String((int)(secs / 60)) + "m " + juce::String((int)(secs % 60)) + "s";
            else
                durStr = juce::String((int)secs) + "s";
            subtitle += "  |  Session: " + durStr;
        }
        }
        g.drawText(subtitle, rightArea.removeFromTop(20),
                   juce::Justification::centredLeft);

        // ═══ V4b: Confidence line — verify attempts + confidence percent ═══
        if (verifyTotalAttempts_ > 0 || confidenceScore_.overall > 0) {
            auto confArea = rightArea.removeFromTop(18);
            g.setFont(juce::Font(juce::FontOptions(9.5f)));
            g.setColour(MixCoachTheme::accentCyan().withAlpha(0.8f));

            juce::String confText;
            if (verifyTotalAttempts_ > 0) {
                // Show verify-based confidence: "8 verificaciones, 65% confianza"
                juce::String verifyLabel = juce::String(verifyTotalAttempts_)
                    + " verificacion" + (verifyTotalAttempts_ == 1 ? "" : "es");
                if (verifyInvalidated_ > 0) {
                    verifyLabel += " (" + juce::String(verifyInvalidated_)
                        + " invalidad" + (verifyInvalidated_ == 1 ? "a" : "as")
                        + " por cambio de secci\xC3\xB3n)";
                }
                confText = "\xF0\x9F\x94\x92 " + verifyLabel + " | Confianza: "
                    + juce::String(verifyConfidencePct_) + "%";
            } else {
                // Fall back to system confidence score
                confText = "\xF0\x9F\x94\x92 Confianza del sistema: "
                    + juce::String(confidenceScore_.overall) + "% — "
                    + confidenceScore_.statusLabel;
            }
            g.drawText(confText, confArea, juce::Justification::centredLeft);
        }

        // Progress bar background
        auto barArea = rightArea.removeFromTop(24);
        g.setColour(MixCoachTheme::bgDarker());
        g.fillRoundedRectangle(barArea.toFloat(), 4.0f);

        // Progress bar fill
        float fillPct = juce::jlimit(0.0f, 1.0f, currentScore_.overall / 100.0f);
        auto fillBounds = barArea.withWidth((int)(barArea.getWidth() * fillPct));
        if (fillBounds.getWidth() > 4) {
            juce::ColourGradient barGrad(scoreToColour(currentScore_.overall).brighter(0.2f),
                                         (float)barArea.getX(), (float)barArea.getY(),
                                         scoreToColour(currentScore_.overall).darker(0.2f),
                                         (float)barArea.getRight(), (float)barArea.getY(),
                                         false);
            g.setGradientFill(barGrad);
            g.fillRoundedRectangle(fillBounds.toFloat(), 4.0f);

            // Glow at fill end
            g.setColour(scoreToColour(currentScore_.overall).withAlpha(0.3f));
            g.fillRoundedRectangle(
                juce::Rectangle<float>((float)fillBounds.getRight() - 4, (float)barArea.getY(),
                                        8.0f, (float)barArea.getHeight()),
                4.0f);
        }

        // ─── Qualitative improvement indicator (NO numbers) ─────────────────
        if (initialScoreCaptured_ && initialScore_.overall > 0) {
            int delta = currentScore_.overall - initialScore_.overall;
            auto evoArea = rightArea.removeFromTop(18);

            juce::String improvementText;
            juce::Colour impColour;
            if (delta >= 10) {
                improvementText = "\xF0\x9F\x8C\x9F  Gran mejora respecto a tu \xC3\xBAltima sesi\xC3\xB3n";
                impColour = MixCoachTheme::success();
            } else if (delta >= 3) {
                improvementText = "\xE2\x9C\xA8  Vas mejorando, sigue as\xC3\xAD";
                impColour = MixCoachTheme::warning();
            } else if (delta > -3) {
                improvementText = "[TREND]  Manteniendo el nivel";
                impColour = MixCoachTheme::accentCyan();
            } else {
                improvementText = "\xF0\x9F\x92\xAA  \xC3\x81nimo, la pr\xC3\xB3xima ser\xC3\xA1 mejor";
                impColour = MixCoachTheme::error().withAlpha(0.7f);
            }

            g.setFont(juce::Font(juce::FontOptions(9.5f)));
            g.setColour(impColour);
            g.drawText(improvementText, evoArea, juce::Justification::centredLeft);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawScoreCard — Tarjeta individual de dominio
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::drawScoreCard(juce::Graphics& g, juce::Rectangle<int> bounds,
                                               const juce::String& label, int score, int initialScore,
                                               juce::Colour accentColour, bool isHovered)
    {
        bool isEmpty = (label == "REFERENCE" && score == 0);

        // ─── Card background ─────────────────────────────────────────────────
        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        // Hover glow
        if (isHovered) {
            g.setColour(accentColour.withAlpha(0.08f));
            g.fillRoundedRectangle(bounds.toFloat().reduced(1.0f), (float)MixCoachTheme::cornerRadius_medium - 1);
        }

        auto inner = bounds.reduced(MixCoachTheme::spacingSM, MixCoachTheme::spacingXS);

        // ─── Domain label ────────────────────────────────────────────────────
        auto labelArea = inner.removeFromTop(20);
        g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
        g.setColour(isEmpty ? MixCoachTheme::textMuted() : accentColour.brighter(0.3f));
        g.drawText(label, labelArea, juce::Justification::centred);

        // ─── Qualitative label (NO numeric score) ───────────────────────────
        auto scoreArea = inner.removeFromTop(24);
        if (isEmpty) {
            g.setFont(juce::Font(juce::FontOptions(11.0f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.4f));
            g.drawText("Sin datos", scoreArea, juce::Justification::centred);
        }
        else {
            juce::String qualText;
            juce::Colour textColour;
            if (score >= 80) {
                qualText = "\xC2""\xA1""Excelente!";
                textColour = MixCoachTheme::success();
            } else if (score >= 60) {
                qualText = "Bien";
                textColour = MixCoachTheme::warning();
            } else if (score >= 40) {
                qualText = "Mejorable";
                textColour = MixCoachTheme::error().withAlpha(0.8f);
            } else {
                qualText = "A trabajar";
                textColour = MixCoachTheme::error();
            }

            g.setFont(juce::Font(juce::FontOptions(13.0f)).boldened());
            g.setColour(textColour);
            g.drawText(qualText, scoreArea, juce::Justification::centred);
        }

        // ─── Mini bar ────────────────────────────────────────────────────────
        auto barArea = inner.removeFromTop(16).reduced(MixCoachTheme::spacingXS, 0);
        g.setColour(MixCoachTheme::bgDarker());
        g.fillRoundedRectangle(barArea.toFloat(), 2.0f);

        if (!isEmpty && score > 0) {
            float fillPct = juce::jlimit(0.0f, 1.0f, score / 100.0f);
            auto fillBounds = barArea.withWidth((int)(barArea.getWidth() * fillPct));
            if (fillBounds.getWidth() > 2) {
                g.setColour(accentColour);
                g.fillRoundedRectangle(fillBounds.toFloat(), 2.0f);
            }
        }

        // ─── Health dot (cualitativo, sin números) ────────────────────────────
        if (!isEmpty && initialScoreCaptured_ && initialScore > 0 && initialScore != score) {
            int delta = score - initialScore;
            juce::String dotEmoji;
            juce::Colour dotColour;
            if (delta >= 5) {
                dotEmoji = "[EXPAND]";  // ▲ mejoró
                dotColour = MixCoachTheme::success();
            } else if (delta > 0) {
                dotEmoji = "\xE2\x86\x97";  // ↗ ligeramente mejor
                dotColour = MixCoachTheme::warning();
            } else if (delta > -5) {
                dotEmoji = "\xE2\x86\x98";  // ↘ ligeramente peor
                dotColour = MixCoachTheme::warning().darker(0.3f);
            } else {
                dotEmoji = "[COLLAPSE]";  // ▼ empeoró
                dotColour = MixCoachTheme::error();
            }

            auto deltaArea = inner.removeFromTop(14);
            g.setFont(juce::Font(juce::FontOptions(12.0f)));
            g.setColour(dotColour);
            g.drawText(dotEmoji, deltaArea, juce::Justification::centred);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawTrackSummary — Resumen de pistas y métricas del master
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::drawTrackSummary(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        auto inner = bounds.reduced(MixCoachTheme::spacingMD, MixCoachTheme::spacingSM);

        // ─── Header ──────────────────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText("TRACK SUMMARY",
                   inner.removeFromTop(22), juce::Justification::centredLeft);

        inner.removeFromTop(MixCoachTheme::spacingXS);

        // ─── Two-column metrics grid ─────────────────────────────────────────
        auto col1 = inner.removeFromLeft(inner.getWidth() / 2);
        auto col2 = inner;

        auto drawMetric = [&](juce::Rectangle<int>& area, const juce::String& label,
                              const juce::String& value, juce::Colour valueColour) {
            auto row = area.removeFromTop(18);
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(MixCoachTheme::textDim());
            g.drawText(label, row.removeFromLeft(90), juce::Justification::centredLeft);
            g.setColour(valueColour);
            g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
            g.drawText(value, row, juce::Justification::centredLeft);
        };

        drawMetric(col1, "Active Tracks", juce::String(activeTrackCount_), MixCoachTheme::textBright());
        drawMetric(col1, "Clipping", juce::String(clippingTrackCount_),
                   clippingTrackCount_ > 0 ? MixCoachTheme::error() : MixCoachTheme::success());
        drawMetric(col1, "Low Signal", juce::String(lowSignalTrackCount_),
                   lowSignalTrackCount_ > 0 ? MixCoachTheme::warning() : MixCoachTheme::textBright());
        // ─── Track category breakdown (from real engine state)
        {
            int nCats = 0;
            juce::String cats;
            if (drumTracks_ > 0)   { if (nCats++ > 0) cats += " | "; cats += juce::String(drumTracks_) + " drums"; }
            if (bassTracks_ > 0)   { if (nCats++ > 0) cats += " | "; cats += juce::String(bassTracks_) + " bass"; }
            if (guitarTracks_ > 0) { if (nCats++ > 0) cats += " | "; cats += juce::String(guitarTracks_) + " gtr"; }
            if (keysTracks_ > 0)   { if (nCats++ > 0) cats += " | "; cats += juce::String(keysTracks_) + " keys"; }
            if (vocalTracks_ > 0)  { if (nCats++ > 0) cats += " | "; cats += juce::String(vocalTracks_) + " vox"; }
            if (fxTracks_ > 0)     { if (nCats++ > 0) cats += " | "; cats += juce::String(fxTracks_) + " fx"; }
            if (melodyTracks_ > 0) { if (nCats++ > 0) cats += " | "; cats += juce::String(melodyTracks_) + " mel"; }
            if (unknownTracks_ > 0) { if (nCats++ > 0) cats += " | "; cats += juce::String(unknownTracks_) + " ?"; }
            if (nCats == 0) cats = "—";
            drawMetric(col1, "Categories", cats, MixCoachTheme::accentCyan());
        }

        drawMetric(col2, "Master Peak", juce::String(masterPeakDb_, 1) + " dBFS",
                   masterPeakDb_ > -1.0f ? MixCoachTheme::error() : MixCoachTheme::textBright());
        drawMetric(col2, "True Peak", juce::String(masterTruePeakDb_, 1) + " dBTP",
                   masterTruePeakDb_ > -1.0f ? MixCoachTheme::error() : MixCoachTheme::textMuted());
        drawMetric(col2, "Integrated LUFS", juce::String(masterIntegratedLUFS_, 1) + " LUFS",
                   MixCoachTheme::accentCyan());
        // ─── Session metadata row: duration + achievements + corrections ───
        inner.removeFromTop(MixCoachTheme::spacingXS);
        {
            auto metaRow = inner.removeFromTop(18);

            // Session duration
            auto durArea = metaRow.removeFromLeft(140);
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(MixCoachTheme::textDim());
            g.drawText("Session", durArea.removeFromLeft(52), juce::Justification::centredLeft);
            {
                juce::String durStr;
                int64_t secs = sessionDurationUs_ / 1000000;
                if (secs >= 3600)
                    durStr = juce::String((int)(secs / 3600)) + "h " + juce::String((int)((secs % 3600) / 60)) + "m";
                else if (secs >= 60)
                    durStr = juce::String((int)(secs / 60)) + "m " + juce::String((int)(secs % 60)) + "s";
                else
                    durStr = juce::String((int)secs) + "s";
                g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
                g.setColour(MixCoachTheme::accentCyan());
                g.drawText(durStr, durArea, juce::Justification::centredLeft);
            }

            // Achievements + Phase progression
            auto achArea = metaRow.removeFromLeft(110);
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(MixCoachTheme::textDim());
            g.drawText("Logros", achArea.removeFromLeft(44), juce::Justification::centredLeft);
            {
                juce::String achStr = juce::String(achievementCount_) + " 🏆";
                g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
                g.setColour(achievementCount_ > 0 ? MixCoachTheme::success() : MixCoachTheme::textMuted());
                g.drawText(achStr, achArea, juce::Justification::centredLeft);
            }

            // Phase progression
            auto phaseArea = metaRow.removeFromLeft(110);
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(MixCoachTheme::textDim());
            g.drawText("Fase", phaseArea.removeFromLeft(32), juce::Justification::centredLeft);
            {
                juce::String phaseStr = juce::String(SessionProgression::phaseEmoji(sessionProgressionPhase_))
                                        + " " + juce::String(SessionProgression::phaseShortName(sessionProgressionPhase_));
                g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
                g.setColour(MixCoachTheme::accentCyan());
                g.drawText(phaseStr, phaseArea, juce::Justification::centredLeft);
            }

            // Corrections + phase progress
            auto corrArea = metaRow;
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(MixCoachTheme::textDim());
            g.drawText("Progreso", corrArea.removeFromLeft(56), juce::Justification::centredLeft);
            {
                // Mini progress bar
                auto barArea = corrArea.removeFromLeft(60).reduced(0, 5);
                g.setColour(MixCoachTheme::bgDarker());
                g.fillRoundedRectangle(barArea.toFloat(), 3.0f);
                if (sessionProgressionProgress_ > 0.0f) {
                    int fillW = (int)(barArea.getWidth() * juce::jlimit(0.0f, 1.0f, sessionProgressionProgress_));
                    if (fillW > 2) {
                        g.setColour(MixCoachTheme::accent());
                        g.fillRoundedRectangle(barArea.withWidth(fillW).toFloat(), 3.0f);
                    }
                }
                // Percentage text
                int pct = (int)(sessionProgressionProgress_ * 100.0f);
                juce::String pctStr = juce::String(pct) + "%";
                g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
                g.setColour(MixCoachTheme::textMuted());
                g.drawText(pctStr, corrArea, juce::Justification::centredLeft);
            }
        }

    
        drawMetric(col2, "Phase Correlation", juce::String(masterCorrelation_, 2),
                   masterCorrelation_ < 0.3f ? MixCoachTheme::warning() : MixCoachTheme::success());

        // ─── Confidence indicator (cualitativo, sin porcentaje) ─────────
        inner.removeFromTop(MixCoachTheme::spacingXS);
        {
            ConfidenceScore cs;
            if (coachEngine_ != nullptr) {
                auto& shared = coachEngine_->getSharedData();
                cs = ConfidenceScore::compute(*coachEngine_, shared, 0);
            }

            auto confRow = inner.removeFromTop(18);
            auto confLabel = confRow.removeFromLeft(90);
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(MixCoachTheme::textDim());
            g.drawText("Confianza", confLabel, juce::Justification::centredLeft);

            // Draw mini bar for confidence (solo barra, sin número)
            auto barArea = confRow.removeFromLeft(80).reduced(0, 4);
            g.setColour(MixCoachTheme::bgDarker());
            g.fillRoundedRectangle(barArea.toFloat(), 2.0f);
            if (cs.overall > 0) {
                float fillPct = juce::jlimit(0.0f, 1.0f, cs.overall / 100.0f);
                auto fillBounds = barArea.withWidth((int)(barArea.getWidth() * fillPct));
                if (fillBounds.getWidth() > 2) {
                    g.setColour(cs.overall >= 60 ? MixCoachTheme::success()
                                : cs.overall >= 40 ? MixCoachTheme::warning()
                                : MixCoachTheme::error());
                    g.fillRoundedRectangle(fillBounds.toFloat(), 2.0f);
                }
            }

            // Label cualitativo (sin número %)
            juce::String confLabel_qual;
            juce::Colour confColour;
            if (cs.overall >= 60) {
                confLabel_qual = "\xF0\x9F\x9F\xA2 Confiable";
                confColour = MixCoachTheme::success();
            } else if (cs.overall >= 40) {
                confLabel_qual = "\xF0\x9F\x9F\xA1 Parcial";
                confColour = MixCoachTheme::warning();
            } else {
                confLabel_qual = "\xF0\x9F\x94\xB4 Baja";
                confColour = MixCoachTheme::error();
            }

            g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
            g.setColour(confColour);
            g.drawText(confLabel_qual, confRow, juce::Justification::centredLeft);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawTrackDiagnosis — Diagnóstico por pista con health dots
    //  Muestra cada pista activa con su estado en 4 dominios (Gain, Dynamics,
    //  Tonal, Phase) usando dots de salud (🟢🟡🔴).
    //  Ordenado por severidad (peor primero).
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::drawTrackDiagnosis(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        // ─── Card background ─────────────────────────────────────────────────
        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        auto inner = bounds.reduced(MixCoachTheme::spacingMD, MixCoachTheme::spacingSM);

        // ─── Header ──────────────────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText(juce::CharPointer_UTF8("[SEARCH]  TRACK DIAGNOSIS"),
                   inner.removeFromTop(22), juce::Justification::centredLeft);

        // Subtitle: count of actionable tracks
        int actionableCount = 0;
        for (const auto& e : trackDiagnosis_)
            if (e.consolidatedSeverity > 0.1f) actionableCount++;
        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.setColour(MixCoachTheme::textMuted());
        juce::String diagSub = juce::String((int)trackDiagnosis_.size()) + " tracks";
        if (actionableCount > 0)
            diagSub += " — " + juce::String(actionableCount) + " need attention";
        g.drawText(diagSub, inner.removeFromTop(14).withLeft(inner.getX()),
                   juce::Justification::centredLeft);

        // ─── Column headers ──────────────────────────────────────────────────
        auto headerRow = inner.removeFromTop(16);
        g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
        g.setColour(MixCoachTheme::textDim());

        // Indent for track name
        headerRow.removeFromLeft(14);
        // Track name header (120px)
        g.drawText("Track", headerRow.removeFromLeft(120), juce::Justification::centredLeft);
        // Role name (80px)
        g.drawText("Role", headerRow.removeFromLeft(72), juce::Justification::centredLeft);
        // Domain headers — 4 domains at ~42px each
        int domainW = 44;
        static const char* domainHeaders[4] = {"Gain", "Dyn", "Tonal", "Phase"};
        for (int d = 0; d < 4; ++d) {
            g.drawText(juce::String(domainHeaders[d]),
                       headerRow.removeFromLeft(domainW), juce::Justification::centred);
        }

        // Divider
        g.setColour(MixCoachTheme::divider().withAlpha(0.20f));
        g.fillRect(bounds.getX() + MixCoachTheme::spacingMD,
                   inner.getY() - 1,
                   bounds.getWidth() - MixCoachTheme::spacingMD * 2, 1);

        // ─── Draw each track row ─────────────────────────────────────────────
        auto drawHealthDot = [&](juce::Rectangle<int>& row, int status) {
            auto dotArea = row.removeFromLeft(domainW);
            int cx = dotArea.getCentreX();
            int cy = dotArea.getCentreY();
            int r  = 4;

            juce::Colour dotColour;
            if (status == 0)      dotColour = MixCoachTheme::success(); // OnTarget 🟢
            else if (status == 1) dotColour = MixCoachTheme::warning(); // NearTarget 🟡
            else if (status == 2) dotColour = MixCoachTheme::error();   // OffTarget 🔴
            else                  dotColour = MixCoachTheme::textMuted().withAlpha(0.25f); // no data

            g.setColour(dotColour.withAlpha(0.85f));
            g.fillEllipse((float)(cx - r), (float)(cy - r), (float)(r * 2), (float)(r * 2));

            // Glow for OffTarget
            if (status == 2) {
                g.setColour(dotColour.withAlpha(0.20f));
                g.fillEllipse((float)(cx - r - 2), (float)(cy - r - 2), (float)((r + 2) * 2), (float)((r + 2) * 2));
            }
        };

        int maxRows = juce::jmin((int)trackDiagnosis_.size(), 8);
        for (int i = 0; i < maxRows; ++i) {
            const auto& entry = trackDiagnosis_[i];
            auto row = inner.removeFromTop(22);

            // Row divider (except last)
            if (i < maxRows - 1) {
                g.setColour(MixCoachTheme::divider().withAlpha(0.08f));
                g.fillRect(inner.getX(), row.getY() + 21, inner.getWidth(), 1);
            }

            // ─── Health dot on the left (consolidated severity) ───────────────
            auto severityArea = row.removeFromLeft(14);
            int sevCx = severityArea.getCentreX();
            int sevCy = severityArea.getCentreY();
            juce::Colour sevCol;
            if (entry.consolidatedSeverity < 0.1f)      sevCol = MixCoachTheme::success();
            else if (entry.consolidatedSeverity < 0.4f) sevCol = MixCoachTheme::warning();
            else                                        sevCol = MixCoachTheme::error();
            g.setColour(sevCol.withAlpha(0.70f));
            g.fillEllipse((float)(sevCx - 3), (float)(sevCy - 3), 6.0f, 6.0f);

            // ─── Track name ────────────────────────────────────────────────
            auto nameArea = row.removeFromLeft(120);
            g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
            g.setColour(MixCoachTheme::textSecondary());
            g.drawText(entry.trackName, nameArea, juce::Justification::centredLeft);

            // ─── Role name ─────────────────────────────────────────────────
            auto roleArea = row.removeFromLeft(72);
            g.setFont(juce::Font(juce::FontOptions(9.0f)));
            juce::Colour roleCol;
            if (entry.roleName.isNotEmpty() && entry.roleName != "Unknown")
                roleCol = MixCoachTheme::accentCyan().withAlpha(0.8f);
            else
                roleCol = MixCoachTheme::textMuted().withAlpha(0.4f);
            g.setColour(roleCol);
            g.drawText(entry.roleName.isNotEmpty() ? entry.roleName : "—",
                       roleArea, juce::Justification::centredLeft);

            // ─── 4 domain health dots ─────────────────────────────────────
            drawHealthDot(row, entry.gainStatus);
            drawHealthDot(row, entry.dynamicsStatus);
            drawHealthDot(row, entry.tonalStatus);
            drawHealthDot(row, entry.phaseStatus);

            // ─── Worst message (if actionable) ────────────────────────────
            if (entry.worstMessage.isNotEmpty() && row.getWidth() > 60) {
                g.setFont(juce::Font(juce::FontOptions(8.0f)));
                g.setColour(MixCoachTheme::textMuted().withAlpha(0.6f));
                juce::String shortMsg = entry.worstMessage.substring(0, juce::jmin(entry.worstMessage.length(), 28));
                if (entry.worstMessage.length() > 28) shortMsg += "...";
                g.drawText(shortMsg, row.reduced(2, 0), juce::Justification::centredLeft);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawReferenceComparison — Comparación mix vs referencia por región
    //  Muestra los deltas de 6 regiones espectrales (Sub, Bass, LoMid, HiMid,
    //  Pres, Air) junto con métricas globales (LUFS, Crest, Correlation).
    //  Datos desde DifferenceProfile.
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::drawReferenceComparison(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        const auto& dp = referenceProfile_;

        // ─── Panel background ─────────────────────────────────────────────────
        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        auto inner = bounds.reduced(MixCoachTheme::spacingMD, MixCoachTheme::spacingSM);

        // ─── Header ──────────────────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText(juce::CharPointer_UTF8("[TARGET]  REFERENCE COMPARISON"),
                   inner.removeFromTop(20), juce::Justification::centredLeft);

        // Subtitle: reference name + similarity
        {
            auto subRow = inner.removeFromTop(16);
            g.setFont(juce::Font(juce::FontOptions(9.0f)));
            g.setColour(MixCoachTheme::textDim());
            juce::String refInfo = "vs " + dp.referenceName;
            if (dp.activeSectionLabel.isNotEmpty() && dp.activeSectionLabel != "Full")
                refInfo += " [" + dp.activeSectionLabel + "]";
            g.drawText(refInfo, subRow.removeFromLeft(180), juce::Justification::centredLeft);

            // Spectral similarity badge
            int simPct = (int)(dp.spectralSimilarity * 100.0f);
            juce::Colour simCol = simPct >= 70 ? MixCoachTheme::success()
                                 : simPct >= 45 ? MixCoachTheme::warning()
                                 : MixCoachTheme::error();
            g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
            g.setColour(simCol);
            g.drawText(juce::String(simPct) + "% match", subRow,
                       juce::Justification::centredRight);
        }

        // Divider
        g.setColour(MixCoachTheme::divider().withAlpha(0.20f));
        g.fillRect(inner.getX(), inner.getY() - 1, inner.getWidth(), 1);

        // ─── Similarity bar ───────────────────────────────────────────────────
        {
            auto barRow = inner.removeFromTop(14);
            int simPct  = juce::jlimit(0, 100, (int)(dp.spectralSimilarity * 100.0f));

            g.setFont(juce::Font(juce::FontOptions(8.0f)));
            g.setColour(MixCoachTheme::textMuted());
            g.drawText("Spectral Match", barRow.removeFromLeft(80), juce::Justification::centredLeft);

            auto barArea = barRow.removeFromLeft(120).reduced(0, 3);
            g.setColour(MixCoachTheme::bgDarker());
            g.fillRoundedRectangle(barArea.toFloat(), 2.0f);

            if (simPct > 0) {
                auto fill = barArea.withWidth((int)(barArea.getWidth() * simPct / 100.0f));
                juce::Colour barCol = simPct >= 70 ? MixCoachTheme::success()
                                     : simPct >= 45 ? MixCoachTheme::warning()
                                     : MixCoachTheme::error();
                g.setColour(barCol);
                g.fillRoundedRectangle(fill.toFloat(), 2.0f);
            }
        }

        inner.removeFromTop(MixCoachTheme::spacingXS);

        // ─── Region comparison (2 columns of 3 regions each) ─────────────────
        {
            static const char* regionNames[6] = {
                "Sub", "Bass", "LoMid", "HiMid", "Pres", "Air"
            };
            static const char* regionFreqs[6] = {
                "20-86", "86-301", "301-1k", "1k-3.5k", "3.5k-8.3k", "8.3k-16k"
            };

            auto regionsRow = inner.removeFromTop(72);
            auto leftCol    = regionsRow.removeFromLeft(regionsRow.getWidth() / 2);
            auto rightCol   = regionsRow;

            auto drawRegion = [&](juce::Rectangle<int>& col, int idx) {
                auto row = col.removeFromTop(24);

                // Region label + frequency
                auto labelArea = row.removeFromLeft(44);
                g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
                g.setColour(MixCoachTheme::textDim());
                g.drawText(juce::String(regionNames[idx]), labelArea.removeFromLeft(26),
                           juce::Justification::centredLeft);
                g.setFont(juce::Font(juce::FontOptions(7.0f)));
                g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
                g.drawText(juce::String(regionFreqs[idx]) + " Hz", labelArea,
                           juce::Justification::centredLeft);

                // Delta bar: centered, 80px wide
                auto barArea = row.removeFromLeft(80).reduced(0, 5);
                float delta = dp.deltaRegionEnergy[idx];
                float mixE  = dp.mixRegionEnergy[idx];
                float refE  = dp.refRegionEnergy[idx];

                // Bar background
                g.setColour(MixCoachTheme::bgDarker());
                g.fillRoundedRectangle(barArea.toFloat(), 2.0f);

                // Bar fill: show how close mix is to ref
                // Center = 0 delta, left side = mix quieter, right = mix louder
                if (mixE > -90.0f && refE > -90.0f) {
                    int barW = barArea.getWidth();
                    int cx   = barArea.getCentreX();

                    // Clamp delta to ±12dB for display
                    float clampedDelta = juce::jlimit(-12.0f, 12.0f, delta);
                    int fillW = (int)(barW * std::abs(clampedDelta) / 12.0f);

                    juce::Colour deltaColour;
                    if (std::abs(delta) < 2.0f)      deltaColour = MixCoachTheme::success();
                    else if (std::abs(delta) < 5.0f) deltaColour = MixCoachTheme::warning();
                    else                             deltaColour = MixCoachTheme::error();

                    if (delta > 0) {
                        // Reference has more energy → fill to the RIGHT of center
                        auto fillR = juce::Rectangle<int>(cx, barArea.getY(), fillW, barArea.getHeight());
                        g.setColour(deltaColour.withAlpha(0.7f));
                        g.fillRoundedRectangle(fillR.toFloat(), 2.0f);
                    }
                    else {
                        // Mix has more energy → fill to the LEFT of center
                        auto fillL = juce::Rectangle<int>(cx - fillW, barArea.getY(), fillW, barArea.getHeight());
                        g.setColour(deltaColour.withAlpha(0.7f));
                        g.fillRoundedRectangle(fillL.toFloat(), 2.0f);
                    }

                    // Center line
                    g.setColour(MixCoachTheme::divider().withAlpha(0.30f));
                    g.fillRect(cx, barArea.getY(), 1, barArea.getHeight());
                }

                // Delta text
                auto deltaLabel = row.removeFromLeft(36);
                float dVal = dp.deltaRegionEnergy[idx];
                if (dVal > -90.0f && dVal < 90.0f) {
                    juce::Colour dCol = std::abs(dVal) < 2.0f ? MixCoachTheme::textMuted()
                                        : dVal > 0          ? MixCoachTheme::warning()
                                        : MixCoachTheme::accentCyan();
                    g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
                    g.setColour(dCol);
                    juce::String dStr = (dVal > 0 ? "+" : "") + juce::String(dVal, 1) + " dB";
                    g.drawText(dStr, deltaLabel, juce::Justification::centredLeft);
                }
            };

            // First 3 regions in left column, next 3 in right column
            drawRegion(leftCol, 0);  // Sub
            drawRegion(leftCol, 1);  // Bass
            drawRegion(leftCol, 2);  // LoMid
            drawRegion(rightCol, 3); // HiMid
            drawRegion(rightCol, 4); // Pres
            drawRegion(rightCol, 5); // Air
        }

        // ─── Summary metrics row ─────────────────────────────────────────────
        inner.removeFromTop(MixCoachTheme::spacingXS);
        {
            auto summaryRow = inner.removeFromTop(18);

            auto drawMetric = [&](juce::String label, juce::String value, juce::Colour colour) {
                auto item = summaryRow.removeFromLeft(110);
                g.setFont(juce::Font(juce::FontOptions(8.0f)));
                g.setColour(MixCoachTheme::textMuted());
                g.drawText(label, item.removeFromLeft(40), juce::Justification::centredLeft);
                g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
                g.setColour(colour);
                g.drawText(value, item, juce::Justification::centredLeft);
            };

            // LUFS delta
            juce::Colour lufsCol = std::abs(dp.deltaLUFS) < 2.0f ? MixCoachTheme::success()
                                    : MixCoachTheme::warning();
            juce::String lufsStr = "ref " + juce::String(dp.refIntegratedLUFS, 1)
                                   + " | mix " + juce::String(dp.mixIntegratedLUFS, 1)
                                   + " | " + (dp.deltaLUFS > 0 ? "+" : "") + juce::String(dp.deltaLUFS, 1);
            drawMetric("LUFS", lufsStr, lufsCol);

            // Crest delta
            juce::Colour crestCol = std::abs(dp.deltaCrestFactor) < 1.5f ? MixCoachTheme::success()
                                     : MixCoachTheme::warning();
            juce::String crestStr = (dp.deltaCrestFactor > 0 ? "+" : "")
                                    + juce::String(dp.deltaCrestFactor, 1) + " dB";
            drawMetric("Crest", crestStr, crestCol);

            // Gaps count
            juce::Colour gapCol = dp.criticalGaps > 0 ? MixCoachTheme::error()
                                   : dp.warningGaps > 0 ? MixCoachTheme::warning()
                                   : MixCoachTheme::success();
            juce::String gapStr = juce::String(dp.totalGaps) + " gaps";
            if (dp.criticalGaps > 0) gapStr += " (" + juce::String(dp.criticalGaps) + " crit)";
            drawMetric("Gaps", gapStr, gapCol);

            // Score delta
            juce::Colour scoreCol = dp.deltaScore > 0.7f ? MixCoachTheme::success()
                                     : dp.deltaScore > 0.4f ? MixCoachTheme::warning()
                                     : MixCoachTheme::error();
            juce::String scoreStr = juce::String((int)(dp.deltaScore * 100.0f)) + "%";
            drawMetric("Score", scoreStr, scoreCol);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawRefinementSection — Refinement Profile (artistic quality: Depth, Impact,
    //  Movement, Glue, Emotion). Solo se muestra cuando isRelevant == true,
    //  es decir, cuando la mezcla técnicamente está sólida (MixScore >= 70).
    //  Muestra 5 barras horizontales con score, label cualitativo, y vsReference.
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::drawRefinementSection(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        const auto& rp = refinementProfile_;

        // ─── Panel background ─────────────────────────────────────────────────
        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        auto inner = bounds.reduced(MixCoachTheme::spacingMD, MixCoachTheme::spacingSM);

        // ─── Header ──────────────────────────────────────────────────────────
        {
            auto headerRow = inner.removeFromTop(20);
            g.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
            g.setColour(MixCoachTheme::textBright());
            g.drawText(juce::CharPointer_UTF8("[ART]  REFINEMENT PROFILE"),
                       headerRow, juce::Justification::centredLeft);

            g.setFont(juce::Font(juce::FontOptions(8.0f)));
            g.setColour(MixCoachTheme::textMuted());
            juce::String refineInfo = "Calidad art\xC3\xADstica de tu mezcla";
            g.drawText(refineInfo, headerRow, juce::Justification::centredRight);

            // Divider
            g.setColour(MixCoachTheme::divider().withAlpha(0.20f));
            g.fillRect(inner.getX(), inner.getY() - 1, inner.getWidth(), 1);
        }

        // ─── Color palette for the 5 refinement domains ──────────────────────
        static const juce::Colour domainColours[5] = {
            juce::Colour(0xFF8B5CF6), // Purple  — Depth
            juce::Colour(0xFFF97316), // Orange  — Impact
            juce::Colour(0xFF10B981), // Emerald — Movement
            juce::Colour(0xFF3B82F6), // Blue    — Glue
            juce::Colour(0xFFEC4899)  // Pink    — Emotion
        };
        static const char* domainEmojis[5] = {
            "\xF0\x9F\x93\x8F",  // 📏 — Depth
            "\xF0\x9F\x92\xA5",  // 💥 — Impact
            "\xF0\x9F\x8C\x8A",  // 🌊 — Movement
            "\xF0\x9F\xA7\xBE",  // 🧾 — Glue
            "\xF0\x9F\x92\x9C"   // 💜 — Emotion
        };
        static const char* domainNames[5] = {
            "Depth", "Impact", "Movement", "Glue", "Emotion"
        };

        // ─── Gather the 5 scores in an array for iteration ───────────────────
        const RefinementScore scores[5] = {
            rp.depth, rp.impact, rp.movement, rp.glue, rp.emotion
        };

        // ─── Compact row per domain: ~24px each (5 rows = 120px) ────────────
        for (int i = 0; i < 5; ++i) {
            const auto& s = scores[i];
            auto row = inner.removeFromTop(22);

            // Divider between rows (except last)
            if (i < 4) {
                g.setColour(MixCoachTheme::divider().withAlpha(0.08f));
                g.fillRect(inner.getX(), row.getY() + 21, inner.getWidth(), 1);
            }

            // ─── Emoji (left) ─────────────────────────────────────────────
            auto emojiArea = row.removeFromLeft(18);
            g.setFont(juce::Font(juce::FontOptions(11.0f)));
            g.drawText(juce::String(domainEmojis[i]), emojiArea, juce::Justification::centred);

            // ─── Domain name ──────────────────────────────────────────────
            auto nameArea = row.removeFromLeft(52);
            g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
            g.setColour(domainColours[i]);
            g.drawText(juce::String(domainNames[i]), nameArea, juce::Justification::centredLeft);

            // ─── Qualitative label ────────────────────────────────────────
            auto labelArea = row.removeFromLeft(62);
            g.setFont(juce::Font(juce::FontOptions(8.0f)));
            g.setColour(MixCoachTheme::textDim());
            g.drawText(s.label.isNotEmpty() ? s.label : "—",
                       labelArea, juce::Justification::centredLeft);

            // ─── Score bar (fill width available) ─────────────────────────
            int barMaxW = juce::jmin(100, row.getWidth() - 10);
            auto barArea = row.removeFromLeft(barMaxW).reduced(0, 5);
            g.setColour(MixCoachTheme::bgDarker());
            g.fillRoundedRectangle(barArea.toFloat(), 2.0f);

            // Fill
            if (s.score > 0.0f) {
                float fillPct = juce::jlimit(0.0f, 1.0f, s.score);
                auto fill = barArea.withWidth((int)(barArea.getWidth() * fillPct));
                if (fill.getWidth() > 2) {
                    g.setColour(domainColours[i].withAlpha(0.75f));
                    g.fillRoundedRectangle(fill.toFloat(), 2.0f);
                }
            }

            // ─── Score indicator (cualitativo, sin número) ─────────────
            auto scoreArea = row.removeFromLeft(20);
            juce::String scoreEmoji;
            if (s.score >= 0.80f)      scoreEmoji = "\xF0\x9F\x9F\xA2";  // 🟢
            else if (s.score >= 0.60f) scoreEmoji = "\xF0\x9F\x9F\xA1";  // 🟡
            else if (s.score >= 0.40f) scoreEmoji = "\xF0\x9F\x94\xB4";  // 🔴
            else                       scoreEmoji = "\xE2\xAD\x90";      // ⭐

            g.setFont(juce::Font(juce::FontOptions(12.0f)));
            g.setColour(domainColours[i].brighter(0.3f));
            g.drawText(scoreEmoji, scoreArea, juce::Justification::centred);

            // ─── vsReference delta ───────────────────────────────────────
            if (s.hasReferenceData && std::abs(s.vsReference) > 0.05f) {
                auto vsArea = row.removeFromLeft(32);
                juce::String vsStr;
                if (s.vsReference > 0) vsStr = juce::String("+") + juce::String(s.vsReference, 2);
                else                   vsStr = juce::String(s.vsReference, 2);

                juce::Colour vsCol = std::abs(s.vsReference) < 0.25f
                    ? MixCoachTheme::textMuted()
                    : s.vsReference > 0.25f ? MixCoachTheme::accentCyan()
                    : MixCoachTheme::warning();

                g.setFont(juce::Font(juce::FontOptions(8.0f)));
                g.setColour(vsCol);
                g.drawText(vsStr, vsArea, juce::Justification::centredRight);
            }
        }

        // ─── Bottom: suggestion line (if any actionable domain has one) ──────
        auto remaining = inner;
        if (remaining.getHeight() >= 16) {
            juce::String globalSuggestion;
            for (int i = 0; i < 5; ++i) {
                if (scores[i].isActionable() && scores[i].suggestion.isNotEmpty()) {
                    globalSuggestion = scores[i].suggestion;
                    break;
                }
            }

            if (globalSuggestion.isNotEmpty()) {
                auto tipRow = remaining.removeFromTop(16);
                g.setFont(juce::Font(juce::FontOptions(8.0f)));
                g.setColour(MixCoachTheme::info().withAlpha(0.7f));
                g.drawText(juce::String("\xF0\x9F\x92\xA1 ") + globalSuggestion,
                           tipRow, juce::Justification::centredLeft);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawCoachAdaptation — Historial de ajustes del FeedbackCollector
    //  Muestra una línea temporal de cada vez que el FeedbackCollector ajustó
    //  los thresholds del CorrectionLearner según el comportamiento del usuario,
    //  incluyendo el trigger, valores antes/después y si fue reset a defaults.
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::drawCoachAdaptation(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        // ─── Panel background ─────────────────────────────────────────────────
        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        auto inner = bounds.reduced(MixCoachTheme::spacingMD, MixCoachTheme::spacingSM);

        // ─── Header row ──────────────────────────────────────────────────────
        {
            auto headerRow = inner.removeFromTop(20);
            g.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
            g.setColour(MixCoachTheme::textBright());
            g.drawText(juce::CharPointer_UTF8("[BRAIN]  COACH ADAPTATION"),
                       headerRow, juce::Justification::centredLeft);

            g.setFont(juce::Font(juce::FontOptions(9.0f)));
            g.setColour(MixCoachTheme::textMuted());
            g.drawText(juce::String((int)coachAdaptationHistory_.size()) + " adjustments",
                       headerRow, juce::Justification::centredRight);

            // Divider
            g.setColour(MixCoachTheme::divider().withAlpha(0.20f));
            g.fillRect(inner.getX(), inner.getY() - 1, inner.getWidth(), 1);
        }

        // ─── Column headers ───────────────────────────────────────────────────
        auto colHeaderRow = inner.removeFromTop(16);
        g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
        g.setColour(MixCoachTheme::textDim());

        // Timestamp column (70px)
        g.drawText("Time", colHeaderRow.removeFromLeft(56), juce::Justification::centredLeft);
        // Trigger column (fill remaining space — we'll allocate specific columns)
        int triggerW = juce::jmin(140, colHeaderRow.getWidth() - 8);
        g.drawText("Trigger", colHeaderRow.removeFromLeft(triggerW), juce::Justification::centredLeft);
        // Parameters: before → after (4 columns of ~38px each)
        int paramColW = 36;
        int remainingAfterParam = colHeaderRow.getWidth() - paramColW * 4;
        if (remainingAfterParam > 0)
            colHeaderRow.removeFromLeft(remainingAfterParam); // center the param group
        g.drawText("minBr", colHeaderRow.removeFromLeft(paramColW), juce::Justification::centred);
        g.drawText("boost", colHeaderRow.removeFromLeft(paramColW), juce::Justification::centred);
        g.drawText("maxBr", colHeaderRow.removeFromLeft(paramColW), juce::Justification::centred);
        g.drawText("spec",  colHeaderRow.removeFromLeft(paramColW), juce::Justification::centred);

        // ─── Draw each adjustment row ─────────────────────────────────────────
        int maxRows = juce::jmin((int)coachAdaptationHistory_.size(), 6);
        int startIdx = (int)coachAdaptationHistory_.size() - maxRows;

        for (int i = startIdx; i < (int)coachAdaptationHistory_.size(); ++i) {
            const auto& adj = coachAdaptationHistory_[i];
            auto row = inner.removeFromTop(24);

            // Row divider (except last)
            if (i < (int)coachAdaptationHistory_.size() - 1) {
                g.setColour(MixCoachTheme::divider().withAlpha(0.08f));
                g.fillRect(inner.getX(), row.getY() + 23, inner.getWidth(), 1);
            }

            // ─── Status icon (left) ──────────────────────────────────────────
            auto iconArea = row.removeFromLeft(16);
            if (adj.wasReset) {
                g.setFont(juce::Font(juce::FontOptions(11.0f)));
                g.setColour(MixCoachTheme::info().withAlpha(0.7f));
                g.drawText(juce::CharPointer_UTF8("[RESET]"), iconArea, juce::Justification::centred); // ⟺ reset
            }
            else if (adj.minCorrectionsAfter < adj.minCorrectionsBefore
                     || adj.boostAfter > adj.boostBefore) {
                g.setFont(juce::Font(juce::FontOptions(11.0f)));
                g.setColour(MixCoachTheme::success().withAlpha(0.7f));
                g.drawText(juce::CharPointer_UTF8("[EXPAND]"), iconArea, juce::Justification::centred); // ▲ more aggressive
            }
            else {
                g.setFont(juce::Font(juce::FontOptions(11.0f)));
                g.setColour(MixCoachTheme::warning().withAlpha(0.7f));
                g.drawText(juce::CharPointer_UTF8("[COLLAPSE]"), iconArea, juce::Justification::centred); // ▼ more conservative
            }

            // ─── Timestamp ──────────────────────────────────────────────────
            auto timeArea = row.removeFromLeft(56);
            {
                int64_t elapsedUs = juce::Time::getMillisecondCounter() * 1000 - adj.timestampUs;
                int64_t minutesAgo = elapsedUs / (60 * 1000 * 1000);
                juce::String timeStr;
                if (minutesAgo < 1)      timeStr = "just now";
                else if (minutesAgo < 60) timeStr = juce::String((int)minutesAgo) + "m ago";
                else                     timeStr = juce::String((int)(minutesAgo / 60)) + "h ago";

                g.setFont(juce::Font(juce::FontOptions(8.0f)));
                g.setColour(MixCoachTheme::textMuted());
                g.drawText(timeStr, timeArea, juce::Justification::centredLeft);
            }

            // ─── Trigger ───────────────────────────────────────────────────
            auto triggerArea = row.removeFromLeft(triggerW);
            g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
            g.setColour(adj.wasReset ? MixCoachTheme::info()
                        : MixCoachTheme::accentCyan().darker(0.1f));
            juce::String shortTrigger = adj.trigger.substring(0, juce::jmin(adj.trigger.length(), 28));
            if (adj.trigger.length() > 28) shortTrigger += "...";
            g.drawText(shortTrigger, triggerArea, juce::Justification::centredLeft);

            // ─── Spacer to center the parameter columns ─────────────────────
            int remaining = row.getWidth() - paramColW * 4;
            if (remaining > 0) row.removeFromLeft(remaining / 2);

            // ─── Parameters: before → after with arrow ──────────────────────
            auto drawParam = [&](juce::Rectangle<int>& area, float beforeVal, float afterVal, bool isInt) {
                juce::String beforeStr, afterStr;
                if (isInt) {
                    beforeStr = juce::String((int)beforeVal);
                    afterStr  = juce::String((int)afterVal);
                }
                else if (afterVal < 1.0f) {
                    beforeStr = juce::String(beforeVal, 2);
                    afterStr  = juce::String(afterVal, 2);
                }
                else {
                    beforeStr = juce::String(beforeVal, 1);
                    afterStr  = juce::String(afterVal, 1);
                }

                bool changed = std::abs(afterVal - beforeVal) > 0.001f;
                juce::Colour chCol = changed ? MixCoachTheme::accentCyan() : MixCoachTheme::textMuted().withAlpha(0.4f);

                auto cell = area.removeFromLeft(paramColW);
                int cellW = cell.getWidth();
                int thirdW = cellW / 3;

                // Before value (left)
                g.setFont(juce::Font(juce::FontOptions(8.0f)));
                g.setColour(changed ? MixCoachTheme::textMuted() : MixCoachTheme::textMuted().withAlpha(0.3f));
                g.drawText(beforeStr, cell.removeFromLeft(thirdW), juce::Justification::centred);

                // Arrow (center)
                if (changed) {
                    g.setFont(juce::Font(juce::FontOptions(7.0f)));
                    g.setColour(chCol);
                    g.drawText(juce::CharPointer_UTF8("[RIGHT]"), cell.removeFromLeft(thirdW), juce::Justification::centred);
                }
                else {
                    cell.removeFromLeft(thirdW);
                }

                // After value (right)
                g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
                g.setColour(changed ? chCol : MixCoachTheme::textMuted().withAlpha(0.3f));
                g.drawText(afterStr, cell, juce::Justification::centred);
            };

            drawParam(row, (float)adj.minCorrectionsBefore, (float)adj.minCorrectionsAfter, true);
            drawParam(row, adj.boostBefore, adj.boostAfter, false);
            drawParam(row, adj.maxBoostBefore, adj.maxBoostAfter, false);
            drawParam(row, adj.spectralThresholdBefore, adj.spectralThresholdAfter, false);
        }

        // ─── Footer note if rows exceed display ──────────────────────────────
        if ((int)coachAdaptationHistory_.size() > maxRows) {
            auto footerRow = inner.removeFromTop(16);
            g.setFont(juce::Font(juce::FontOptions(8.0f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.drawText("+ " + juce::String((int)coachAdaptationHistory_.size() - maxRows)
                       + " earlier adjustments", footerRow, juce::Justification::centred);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawMultiSessionChart — Gráfico de evolución multi-sesión
    //  Muestra la evolución del MixScore overall + 5 dominios (Gain, Tonal,
    //  Dynamics, Spatial, Reference) a través de sesiones.
    //  Los dominios se dibujan como líneas finas semi-transparentes;
    //  el overall es la línea principal gruesa.
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::drawMultiSessionChart(juce::Graphics& g,
                                                       juce::Rectangle<int> headerBounds,
                                                       juce::Rectangle<int> chartBounds)
    {
        // ─── Header ──────────────────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText(juce::CharPointer_UTF8("[TREND]  PROGRESS HISTORY"),
                   headerBounds, juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.setColour(MixCoachTheme::textMuted());
        juce::String histSub = juce::String((int)sessionHistory_.size()) + " sessions recorded";
        g.drawText(histSub, headerBounds, juce::Justification::centredRight);

        if (sessionHistory_.size() < 2) {
            // ─── Empty state ────────────────────────────────────────────────
            MixCoachTheme::fillGlassPanel(g, chartBounds.toFloat(), MixCoachTheme::cornerRadius_medium);
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(MixCoachTheme::textMuted());
            juce::String msg = sessionHistory_.empty()
                ? "Keep mixing! Progress data will appear after multiple sessions."
                : "One session recorded. Come back after another session to see your progress.";
            g.drawText(msg, chartBounds, juce::Justification::centred);
            return;
        }

        // ─── Chart panel ─────────────────────────────────────────────────────
        MixCoachTheme::fillGlassPanel(g, chartBounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        auto inner = chartBounds.reduced(MixCoachTheme::spacingMD, MixCoachTheme::spacingSM);
        int chartW  = inner.getWidth();
        int chartH  = inner.getHeight();

        if (chartW < 40 || chartH < 30) return;

        // ─── Data preparation ────────────────────────────────────────────────
        int n = (int)sessionHistory_.size();
        int maxScoreSeen = 0;
        for (const auto& snap : sessionHistory_) {
            if (snap.mixScoreOverall > maxScoreSeen) maxScoreSeen = snap.mixScoreOverall;
            if (snap.domainGain > maxScoreSeen) maxScoreSeen = snap.domainGain;
            if (snap.domainTonal > maxScoreSeen) maxScoreSeen = snap.domainTonal;
            if (snap.domainDynamics > maxScoreSeen) maxScoreSeen = snap.domainDynamics;
            if (snap.domainSpatial > maxScoreSeen) maxScoreSeen = snap.domainSpatial;
            if (snap.domainReference > maxScoreSeen) maxScoreSeen = snap.domainReference;
        }
        int scoreRange = juce::jmax(100, maxScoreSeen + 10);

        // ─── Grid lines ──────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::divider().withAlpha(0.15f));
        for (int pct = 25; pct <= 75; pct += 25) {
            int y = inner.getY() + (int)(chartH * (1.0f - pct / 100.0f));
            g.fillRect(inner.getX(), y, chartW, 1);
        }

        // ─── Y-axis qualitative zones (NO numbers) ──────────────────────
        g.setFont(juce::Font(juce::FontOptions(7.0f)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
        g.drawText("\xF0\x9F\x8C\x9F", inner.getX() - 2, inner.getY() - 6, 16, 8,
                   juce::Justification::right);
        g.drawText("\xF0\x9F\x91\x8D", inner.getX() - 2, inner.getY() + chartH / 4 - 4, 16, 8,
                   juce::Justification::right);
        g.drawText("\xF0\x9F\x92\xAA", inner.getX() - 2, inner.getY() + chartH * 2 / 4 - 4, 16, 8,
                   juce::Justification::right);
        g.drawText("[TREND]", inner.getX() - 2, inner.getBottom() - 6, 16, 8,
                   juce::Justification::right);

        // ─── Plot point helper ───────────────────────────────────────────────
        auto plotPoint = [&](int idx, int value) -> juce::Point<int> {
            float xFrac = (n > 1) ? (float)idx / (n - 1) : 0.5f;
            int x = inner.getX() + (int)(xFrac * chartW);
            int y = inner.getBottom() - (int)((float)value / scoreRange * chartH);
            y = juce::jlimit(inner.getY(), inner.getBottom(), y);
            return {x, y};
        };

        // ─── Helper: extraer score por dominio ────────────────────────────
        static auto domainScore = [](const AiCoachAdapter::SessionSnapshotEntry& snap, int d) -> int {
            switch (d) {
                case 0: return snap.domainGain;
                case 1: return snap.domainTonal;
                case 2: return snap.domainDynamics;
                case 3: return snap.domainSpatial;
                case 4: return snap.domainReference;
                default: return 0;
            }
        };

        // ─── Check if domain data exists (skip old snapshots that have all zeros) ─
        bool hasDomainData = false;
        {
            for (int i = 0; i < n; ++i) {
                const auto& snap = sessionHistory_[i];
                if (snap.domainGain > 0 || snap.domainTonal > 0 ||
                    snap.domainDynamics > 0 || snap.domainSpatial > 0 ||
                    snap.domainReference > 0) {
                    hasDomainData = true;
                    break;
                }
            }
        }

        // ─── Draw domain lines (thin, behind overall) ────────────────────────
        if (hasDomainData) {
            for (int d = 0; d < 5; ++d) {
                juce::Path path;
                bool pathStarted = false;
                for (int i = 0; i < n; ++i) {
                    int val = domainScore(sessionHistory_[i], d);
                    if (val == 0 && d == 4) continue; // Skip Reference if all zeros
                    auto pt = plotPoint(i, val);
                    if (!pathStarted) { path.startNewSubPath((float)pt.x, (float)pt.y); pathStarted = true; }
                    else              { path.lineTo((float)pt.x, (float)pt.y); }
                }
                if (path.getLength() > 0.0f) {
                    g.setColour(domainColour(d).withAlpha(0.45f));
                    g.strokePath(path, juce::PathStrokeType(1.0f));
                }
            }
        }

        // ─── MixScore overall line (primary, thick, on top) ──────────────────
        juce::Path scorePath;
        for (int i = 0; i < n; ++i) {
            auto pt = plotPoint(i, sessionHistory_[i].mixScoreOverall);
            if (i == 0) scorePath.startNewSubPath((float)pt.x, (float)pt.y);
            else        scorePath.lineTo((float)pt.x, (float)pt.y);
        }
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.strokePath(scorePath, juce::PathStrokeType(2.5f));

        // ─── Outer glow on overall line ──────────────────────────────────────
        g.setColour(juce::Colours::white.withAlpha(0.10f));
        g.strokePath(scorePath, juce::PathStrokeType(5.0f));

        // ─── Data dots (overall) ─────────────────────────────────────────────
        for (int i = 0; i < n; ++i) {
            auto pt = plotPoint(i, sessionHistory_[i].mixScoreOverall);
            // Glow
            g.setColour(juce::Colours::white.withAlpha(0.15f));
            g.fillEllipse((float)pt.x - 5.0f, (float)pt.y - 5.0f, 10.0f, 10.0f);
            // Dot
            g.setColour(juce::Colours::white);
            g.fillEllipse((float)pt.x - 2.5f, (float)pt.y - 2.5f, 5.0f, 5.0f);
        }

        // ─── Domain data dots ────────────────────────────────────────────────
        if (hasDomainData) {
            for (int d = 0; d < 5; ++d) {
                for (int i = 0; i < n; ++i) {
                    int val = domainScore(sessionHistory_[i], d);
                    if (val == 0 && d == 4) continue;
                    auto pt = plotPoint(i, val);
                    g.setColour(domainColour(d).withAlpha(0.30f));
                    g.fillEllipse((float)pt.x - 2.0f, (float)pt.y - 2.0f, 4.0f, 4.0f);
                }
            }
        }

        // ─── X-axis labels (session numbers) ─────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(8.0f)));
        g.setColour(MixCoachTheme::textMuted());
        if (n <= 8) {
            for (int i = 0; i < n; ++i) {
                auto pt = plotPoint(i, 0);
                juce::String sNum = "S" + juce::String(sessionHistory_[i].sessionNumber);
                g.drawText(sNum, pt.x - 10, pt.y + 2, 20, 10, juce::Justification::centred);
            }
        }
        else {
            for (int i = 0; i < n; ++i) {
                if (i == 0 || i == n - 1 || i % 3 == 0) {
                    auto pt = plotPoint(i, 0);
                    juce::String sNum = "S" + juce::String(sessionHistory_[i].sessionNumber);
                    g.drawText(sNum, pt.x - 10, pt.y + 2, 20, 10, juce::Justification::centred);
                }
            }
        }

        // ─── Legend ──────────────────────────────────────────────────────────
        auto legendBounds = chartBounds.withHeight(14).reduced(MixCoachTheme::spacingMD, 0);
        legendBounds = legendBounds.translated(0, chartBounds.getHeight() - 14);
        {
            // Overall legend
            auto legItem = legendBounds.removeFromLeft(80);
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.fillRect(legItem.removeFromLeft(12).reduced(0, 6));
            g.setFont(juce::Font(juce::FontOptions(8.0f)));
            g.setColour(MixCoachTheme::textDim());
            g.drawText("Overall", legItem, juce::Justification::centredLeft);

            // Domain legends (only if domain data exists)
            if (hasDomainData) {
                static const char* domainLabels[5] = {"Gain", "Tonal", "Dynamics", "Spatial", "Reference"};
                for (int d = 0; d < 5; ++d) {
                    auto legItem2 = legendBounds.removeFromLeft(72);
                    g.setColour(domainColour(d).withAlpha(0.45f));
                    g.fillRect(legItem2.removeFromLeft(10).reduced(0, 6));
                    g.setFont(juce::Font(juce::FontOptions(8.0f)));
                    g.setColour(MixCoachTheme::textDim());
                    g.drawText(domainLabels[d], legItem2, juce::Justification::centredLeft);
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawChangesSection — Historial de correcciones
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::drawChangesSection(juce::Graphics& g,
                                                    juce::Rectangle<int> headerBounds,
                                                    juce::Rectangle<int> listBounds)
    {
        // ─── Header ──────────────────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText(juce::CharPointer_UTF8("[CHANGE]  CHANGES MADE"),
                   headerBounds, juce::Justification::centredLeft);

        // Subtitle
        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.setColour(MixCoachTheme::textMuted());
        juce::String changesSub = juce::String(totalCorrections_) + " corrections, "
                                  + juce::String(appliedCorrections_) + " applied";
        g.drawText(changesSub, headerBounds, juce::Justification::centredRight);

        // ─── List panel ─────────────────────────────────────────────────────
        MixCoachTheme::fillGlassPanel(g, listBounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        auto listInner = listBounds.reduced(MixCoachTheme::spacingMD, MixCoachTheme::spacingSM);

        if (correctionHistory_.empty()) {
            g.setFont(juce::Font(juce::FontOptions(11.0f)));
            g.setColour(MixCoachTheme::textMuted());
            g.drawText("No corrections recorded yet. Start mixing and the coach will track changes.",
                       listInner, juce::Justification::centred);
            return;
        }

        // ─── Draw correction entries (max ~12 visible) ──────────────────────
        int maxVisible = juce::jmin((int)correctionHistory_.size(),
                                     juce::jmax(1, listInner.getHeight() / 22));
        int startIdx   = juce::jmax(0, (int)correctionHistory_.size() - maxVisible);

        for (int i = startIdx; i < (int)correctionHistory_.size(); ++i) {
            const auto& entry = correctionHistory_[i];
            auto row = listInner.removeFromTop(22);

            // ─── Status icon ────────────────────────────────────────────────
            juce::Colour sCol = statusColour(entry.finalStatus);
            g.setFont(juce::Font(juce::FontOptions(12.0f)));
            g.setColour(sCol);
            g.drawText(juce::String(statusIcon(entry.finalStatus)),
                       row.removeFromLeft(20), juce::Justification::centred);

            // ─── Track name + action ────────────────────────────────────────
            juce::String actionText = entry.trackName + ": " + entry.action;
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(MixCoachTheme::textSecondary());
            g.drawText(actionText, row.removeFromLeft(row.getWidth() - 60),
                       juce::Justification::centredLeft);

            // ─── Delta badge ────────────────────────────────────────────────
            if (entry.appliedRatio > 0.01f) {
                juce::String pctStr = juce::String((int)(entry.appliedRatio * 100.0f)) + "%";
                g.setColour(sCol.withAlpha(0.7f));
                g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
                g.drawText(pctStr, row, juce::Justification::centredRight);
            }

            // Row divider
            if (i < (int)correctionHistory_.size() - 1) {
                g.setColour(MixCoachTheme::divider().withAlpha(0.15f));
                g.fillRect(listInner.getX(), row.getBottom(), listInner.getWidth(), 1);
            }
        }

        // Scroll indicator if more entries exist
        if ((int)correctionHistory_.size() > maxVisible + startIdx) {
            auto indicatorBounds = listBounds.removeFromBottom(16).reduced(listBounds.getWidth() / 3, 0);
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.4f));
            g.setFont(juce::Font(juce::FontOptions(8.0f)));
            g.drawText("+" + juce::String((int)correctionHistory_.size() - maxVisible - startIdx)
                       + " more entries", indicatorBounds, juce::Justification::centred);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawExportButton — Botón de exportación premium
    // ═══════════════════════════════════════════════════════════════════════════
    
    // ───────────────────────────────────────────────────────
    //  drawPluginRecommendations — Muestra sugerencias de plugins por pista
    // ───────────────────────────────────────────────────────
    void EndOfSessionComponent::drawPluginRecommendations(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        // ─── Panel background ───────────────────────────────
        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        auto inner = bounds.reduced(MixCoachTheme::spacingMD, MixCoachTheme::spacingSM);

        // ─── Header ───────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText(juce::CharPointer_UTF8("[CHANGE]  PLUGIN RECOMMENDATIONS"),
                   inner.removeFromTop(20), juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.setColour(MixCoachTheme::textMuted());
        juce::String recSub = juce::String((int)pluginRecs_.size()) + " tracks with suggestions";
        g.drawText(recSub, inner.removeFromTop(14).withLeft(inner.getX()),
                   juce::Justification::centredRight);

        // Divider
        g.setColour(MixCoachTheme::divider().withAlpha(0.20f));
        g.fillRect(inner.getX(), inner.getY() - 1, inner.getWidth(), 1);

        // ─── Draw each track row with its plugin pills ───
        int maxRows = juce::jmin((int)pluginRecs_.size(), 6);
        for (int i = 0; i < maxRows; ++i) {
            const auto& rec = pluginRecs_[i];
            auto row = inner.removeFromTop(34);

            // Row divider (except last)
            if (i < maxRows - 1) {
                g.setColour(MixCoachTheme::divider().withAlpha(0.08f));
                g.fillRect(inner.getX(), row.getY() + 33, inner.getWidth(), 1);
            }

            // ─── Severity indicator dot ───
            auto sevArea = row.removeFromLeft(10);
            juce::Colour sevCol;
            if (rec.severity >= 0.7f)      sevCol = MixCoachTheme::error();
            else if (rec.severity >= 0.4f) sevCol = MixCoachTheme::warning();
            else                           sevCol = MixCoachTheme::success();
            g.setColour(sevCol.withAlpha(0.70f));
            g.fillEllipse((float)(sevArea.getCentreX() - 3), (float)(sevArea.getCentreY() - 3), 6.0f, 6.0f);

            // ─── Track name + role ───
            auto nameArea = row.removeFromLeft(100);
            g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
            g.setColour(MixCoachTheme::textSecondary());
            g.drawText(rec.trackName, nameArea.removeFromLeft(60), juce::Justification::centredLeft);
            g.setFont(juce::Font(juce::FontOptions(8.0f)));
            g.setColour(MixCoachTheme::textMuted());
            g.drawText(rec.roleName.isNotEmpty() ? rec.roleName : "—",
                       nameArea, juce::Justification::centredLeft);

            // ─── Domain badge ───
            auto badgeArea = row.removeFromLeft(44);
            juce::Colour badgeCol;
            if (rec.domain == "gain")     badgeCol = juce::Colour(0xFF8B5CF6);
            else if (rec.domain == "tonal") badgeCol = juce::Colour(0xFF3B82F6);
            else if (rec.domain == "dynamics") badgeCol = juce::Colour(0xFF10B981);
            else if (rec.domain == "phase") badgeCol = juce::Colour(0xFFF59E0B);
            else                          badgeCol = MixCoachTheme::textMuted();

            g.setColour(badgeCol.withAlpha(0.15f));
            g.fillRoundedRectangle(badgeArea.toFloat(), 4.0f);
            g.setColour(badgeCol.withAlpha(0.7f));
            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.drawText(rec.domain.toUpperCase(), badgeArea, juce::Justification::centred);

            // ─── Plugin pills (up to 3) ───
            int maxPills = juce::jmin((int)rec.suggestions.size(), 3);
            int pillStartX = row.getX();
            for (int p = 0; p < maxPills; ++p) {
                const auto& sug = rec.suggestions[p];
                juce::String pillText = juce::String(TrackPluginSuggestion::tierIcon(sug.tier))
                                        + " " + sug.pluginName;

                // Measure text width
                juce::Font pillFont(juce::FontOptions(9.0f));
                int textW = juce::GlyphArrangement::getStringWidthInt(pillFont, pillText) + 12; // padding

                // Clamp to remaining row width
                int maxW = row.getRight() - pillStartX;
                textW = juce::jmin(textW, maxW);
                if (textW < 20) break;

                auto pillArea = juce::Rectangle<int>(pillStartX, row.getY() + 2, textW, 16);

                // Pill background
                juce::Colour tierCol;
                switch (sug.tier) {
                    case TrackPluginSuggestion::Tier::Native:  tierCol = MixCoachTheme::bgSurface(); break;
                    case TrackPluginSuggestion::Tier::Free:    tierCol = juce::Colour(0xFF10B981).withAlpha(0.20f); break;
                    case TrackPluginSuggestion::Tier::Premium: tierCol = juce::Colour(0xFFF59E0B).withAlpha(0.20f); break;
                    case TrackPluginSuggestion::Tier::UserHas: tierCol = juce::Colour(0xFF8B5CF6).withAlpha(0.20f); break;
                    default: tierCol = MixCoachTheme::bgSurface(); break;
                }
                g.setColour(tierCol);
                g.fillRoundedRectangle(pillArea.toFloat(), 6.0f);
                g.setColour(MixCoachTheme::divider().withAlpha(0.25f));
                g.drawRoundedRectangle(pillArea.toFloat(), 6.0f, 0.5f);

                // Pill text
                g.setFont(pillFont);
                g.setColour(MixCoachTheme::textSecondary());
                g.drawText(pillText, pillArea, juce::Justification::centred);

                pillStartX = pillArea.getRight() + 4;
                if (pillStartX > row.getRight() - 10) break;
            }
        }

        // Footer note if more tracks exist
        if ((int)pluginRecs_.size() > maxRows) {
            auto footerRow = inner.removeFromTop(16);
            g.setFont(juce::Font(juce::FontOptions(8.0f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.drawText("+ " + juce::String((int)pluginRecs_.size() - maxRows)
                       + " more tracks with recommendations", footerRow, juce::Justification::centred);
        }
    }

    // ===
    //  drawUsedPlugins — Muestra los plugins usados/recomendados
    // ===
    void EndOfSessionComponent::drawUsedPlugins(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        auto inner = bounds.reduced(MixCoachTheme::spacingMD, MixCoachTheme::spacingSM);

        // Header
        {
            auto headerRow = inner.removeFromTop(20);
            g.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
            g.setColour(MixCoachTheme::textBright());
            g.drawText(juce::CharPointer_UTF8("[PLUGIN]  PLUGINS IN SESSION"),
                       headerRow, juce::Justification::centredLeft);

            g.setFont(juce::Font(juce::FontOptions(9.0f)));
            g.setColour(MixCoachTheme::textMuted());
            g.drawText(juce::String(usedPlugins_.size()) + " plugins used/recommended",
                       headerRow, juce::Justification::centredRight);

            g.setColour(MixCoachTheme::divider().withAlpha(0.20f));
            g.fillRect(inner.getX(), inner.getY() - 1, inner.getWidth(), 1);
        }

        // Draw plugin pills in flow layout
        auto area = inner;
        float x = (float)area.getX();
        float y = (float)area.getY();
        const float pillH = 18.0f;
        const float gap = 4.0f;
        const float maxW = (float)area.getWidth();

        for (int i = 0; i < juce::jmin(usedPlugins_.size(), 10); ++i) {
            auto pluginName = usedPlugins_[i];
            float textW = juce::GlyphArrangement::getStringWidthInt(
                juce::Font(juce::FontOptions(8.5f)).boldened(), pluginName);
            float pillW = textW + 14.0f;

            if (x + pillW > area.getX() + maxW) {
                x = (float)area.getX();
                y += pillH + gap;
            }

            if (y + pillH > (float)area.getBottom()) break;

            auto pillRect = juce::Rectangle<float>(x, y, pillW, pillH);

            g.setColour(MixCoachTheme::accent().withAlpha(0.12f));
            g.fillRoundedRectangle(pillRect, 4.0f);
            g.setColour(MixCoachTheme::accent().withAlpha(0.30f));
            g.drawRoundedRectangle(pillRect, 4.0f, 0.5f);

            g.setFont(juce::Font(juce::FontOptions(8.5f)).boldened());
            g.setColour(MixCoachTheme::accentGlow());
            g.drawText(pluginName, pillRect, juce::Justification::centred);

            x += pillW + gap;
        }
    }

    void EndOfSessionComponent::drawExportButton(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        auto btn = bounds.toFloat();

        // ─── Shadow ─────────────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.30f));
        g.fillRoundedRectangle(btn.translated(0.0f, 2.0f), MixCoachTheme::cornerRadius_medium);

        // ─── Button gradient ────────────────────────────────────────────────
        juce::ColourGradient btnGrad(
            exportButtonHovered_ ? MixCoachTheme::accentGlow() : MixCoachTheme::accent().brighter(0.05f),
            btn.getCentreX(), btn.getY(),
            exportButtonHovered_ ? MixCoachTheme::accent().brighter(0.2f) : MixCoachTheme::accentDim(),
            btn.getCentreX(), btn.getBottom(), false);
        g.setGradientFill(btnGrad);
        g.fillRoundedRectangle(btn, MixCoachTheme::cornerRadius_medium);

        // ─── Hover glow ────────────────────────────────────────────────────
        if (exportButtonHovered_) {
            g.setColour(MixCoachTheme::accentGlow().withAlpha(0.15f));
            g.fillRoundedRectangle(btn.expanded(4.0f, 2.0f), MixCoachTheme::cornerRadius_large);
        }

        // ─── Glass highlight ───────────────────────────────────────────────
        auto glassTop = btn.withHeight(btn.getHeight() * 0.45f);
        juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.10f),
                                       glassTop.getCentreX(), glassTop.getY(),
                                       juce::Colour(0x00000000),
                                       glassTop.getCentreX(), glassTop.getBottom(), false);
        g.setGradientFill(glassGrad);
        g.fillRoundedRectangle(glassTop, MixCoachTheme::cornerRadius_medium);

        // ─── Border ────────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::accentGlow().withAlpha(0.15f));
        g.drawRoundedRectangle(btn, MixCoachTheme::cornerRadius_medium, 0.5f);

        // ─── Icon + Text ───────────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
        g.setColour(juce::Colours::white);
        g.drawText(juce::CharPointer_UTF8("[EXPORT]  EXPORT REPORT"),
                   bounds, juce::Justification::centred);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawNewSessionButton — Botón para reiniciar flujo (GAP #3)
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::drawNewSessionButton(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        auto btn = bounds.toFloat();

        // Shadow
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.fillRoundedRectangle(btn.translated(0.0f, 2.0f), MixCoachTheme::cornerRadius_medium);

        // Button gradient (verde éxito)
        juce::ColourGradient btnGrad(
            newSessionHovered_ ? MixCoachTheme::success().brighter(0.3f) : MixCoachTheme::success().withAlpha(0.85f),
            btn.getCentreX(), btn.getY(),
            newSessionHovered_ ? MixCoachTheme::success().brighter(0.1f) : MixCoachTheme::success().darker(0.2f),
            btn.getCentreX(), btn.getBottom(), false);
        g.setGradientFill(btnGrad);
        g.fillRoundedRectangle(btn, MixCoachTheme::cornerRadius_medium);

        // Hover glow
        if (newSessionHovered_) {
            g.setColour(MixCoachTheme::success().withAlpha(0.15f));
            g.fillRoundedRectangle(btn.expanded(4.0f, 2.0f), MixCoachTheme::cornerRadius_large);
        }

        // Glass highlight
        auto glassTop = btn.withHeight(btn.getHeight() * 0.45f);
        juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.12f),
                                       glassTop.getCentreX(), glassTop.getY(),
                                       juce::Colour(0x00000000),
                                       glassTop.getCentreX(), glassTop.getBottom(), false);
        g.setGradientFill(glassGrad);
        g.fillRoundedRectangle(glassTop, MixCoachTheme::cornerRadius_medium);

        // Border
        g.setColour(MixCoachTheme::success().withAlpha(0.2f));
        g.drawRoundedRectangle(btn, MixCoachTheme::cornerRadius_medium, 0.5f);

        // Icon + Text
        g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
        g.setColour(juce::Colours::white);
        g.drawText("NUEVA SESION",
                   bounds, juce::Justification::centred);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  generateHTMLReport — Genera un reporte HTML completo con todos los datos
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String EndOfSessionComponent::generateHTMLReport() const
    {
        juce::String html;
        html += "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
        html += "<meta charset=\"UTF-8\">\n";
        html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
        html += "<title>MixCoach Session Report</title>\n";
        html += "<style>\n";
        html += "* { margin: 0; padding: 0; box-sizing: border-box; }\n";
        html += "body { font-family: -apple-system, 'Segoe UI', Roboto, sans-serif; background: #0A1018; color: #E2E8F0; padding: 40px; }\n";
        html += ".container { max-width: 800px; margin: 0 auto; }\n";
        html += "h1 { font-size: 28px; color: #F8FAFC; margin-bottom: 4px; }\n";
        html += "h1 small { font-size: 14px; color: #64748B; font-weight: normal; }\n";
        html += ".subtitle { color: #64748B; font-size: 13px; margin-bottom: 32px; }\n";
        html += ".score-hero { background: linear-gradient(135deg, #0E1420, #121E36); border: 1px solid #1E293B; border-radius: 12px; padding: 28px; margin-bottom: 24px; display: flex; align-items: center; gap: 24px; }\n";
        html += ".score-number { font-size: 64px; font-weight: 800; line-height: 1; }\n";
        html += ".score-details { flex: 1; }\n";
        html += ".score-label { font-size: 20px; font-weight: 600; color: #F8FAFC; margin-bottom: 4px; }\n";
        html += ".score-bar { height: 8px; background: #060A10; border-radius: 4px; overflow: hidden; margin: 8px 0; }\n";
        html += ".score-bar-fill { height: 100%; border-radius: 4px; transition: width 0.3s; }\n";
        html += ".score-meta { color: #94A3B8; font-size: 12px; }\n";
        html += ".cards { display: grid; grid-template-columns: repeat(5, 1fr); gap: 10px; margin-bottom: 24px; }\n";
        html += ".card { background: #0E1420; border: 1px solid #1E293B; border-radius: 8px; padding: 14px; text-align: center; }\n";
        html += ".card-label { font-size: 10px; font-weight: 700; text-transform: uppercase; letter-spacing: 1px; margin-bottom: 6px; }\n";
        html += ".card-score { font-size: 32px; font-weight: 700; line-height: 1.2; }\n";
        html += ".card-sub { font-size: 10px; color: #64748B; margin-top: 4px; }\n";
        html += ".section { background: #0E1420; border: 1px solid #1E293B; border-radius: 10px; padding: 18px; margin-bottom: 16px; }\n";
        html += ".section h2 { font-size: 14px; color: #F8FAFC; margin-bottom: 12px; border-bottom: 1px solid #1E293B; padding-bottom: 8px; }\n";
        html += ".metric-row { display: flex; justify-content: space-between; padding: 4px 0; font-size: 13px; }\n";
        html += ".metric-label { color: #94A3B8; }\n";
        html += ".metric-value { color: #E2E8F0; font-weight: 600; }\n";
        html += ".correction-row { display: flex; align-items: center; gap: 8px; padding: 6px 0; border-bottom: 1px solid #0A1018; font-size: 13px; }\n";
        html += ".correction-row:last-child { border-bottom: none; }\n";
        html += ".badge { display: inline-block; padding: 1px 8px; border-radius: 4px; font-size: 10px; font-weight: 600; }\n";
        html += ".footer { text-align: center; color: #64748B; font-size: 11px; margin-top: 32px; padding-top: 16px; border-top: 1px solid #1E293B; }\n";
        html += "</style>\n</head>\n<body>\n";
        html += "<div class=\"container\">\n";

        // ─── Header ────────────────────────────────────────────────────────
        html += "<h1>MixCoach Session Report <small>v1.0</small></h1>\n";
        html += "<div class=\"subtitle\">Generated: " + juce::Time::getCurrentTime().toString(true, true) + "&nbsp;&nbsp;|&nbsp;&nbsp;";
        if (currentScore_.genre.isNotEmpty())
            html += "Genre: " + currentScore_.genre + "&nbsp;&nbsp;|&nbsp;&nbsp;";
        html += "MixCoach AI</div>\n";

        // ─── Overall Score Hero ────────────────────────────────────────────
        juce::Colour scoreCol = scoreToColour(currentScore_.overall);
        juce::String scoreHex = scoreCol.toDisplayString(false);
        html += "<div class=\"score-hero\">\n";
        html += "  <div class=\"score-number\" style=\"color: " + scoreHex + "\">"
                + juce::String(currentScore_.overall) + "</div>\n";
        html += "  <div class=\"score-details\">\n";
        html += "    <div class=\"score-label\">" + currentScore_.statusLabel + "</div>\n";
        html += "    <div class=\"score-bar\"><div class=\"score-bar-fill\" style=\"width: "
                + juce::String(currentScore_.overall) + "%; background: " + scoreHex + "\"></div></div>\n";
        html += "    <div class=\"score-meta\">" + juce::String(activeTrackCount_)
                + " tracks &bull; " + juce::String(totalCorrections_)
                + " corrections &bull; " + juce::String(appliedCorrections_) + " applied</div>\n";
        html += "  </div>\n</div>\n";

        // ─── Domain Scores ─────────────────────────────────────────────────
        static const char* domainNames[5] = {"Gain", "Tonal", "Dynamics", "Spatial", "Reference"};
        int domainScores[5] = {currentScore_.gain, currentScore_.tonal,
                               currentScore_.dynamics, currentScore_.spatial, currentScore_.reference};
        juce::Colour domainCols[5] = {domainColour(0), domainColour(1), domainColour(2),
                                       domainColour(3), domainColour(4)};
        html += "<div class=\"cards\">\n";
        for (int i = 0; i < 5; ++i) {
            juce::String dHex = domainCols[i].toDisplayString(false);
            bool isEmpty = (i == 4 && domainScores[i] == 0);
            html += "  <div class=\"card\">\n";
            html += "    <div class=\"card-label\" style=\"color: " + dHex + "\">"
                    + juce::String(domainNames[i]) + "</div>\n";
            if (isEmpty) {
                html += "    <div class=\"card-score\" style=\"color: #475569;\">N/A</div>\n";
            }
            else {
                html += "    <div class=\"card-score\" style=\"color: " + scoreToColour(domainScores[i]).toDisplayString(false) + "\">"
                        + juce::String(domainScores[i]) + "</div>\n";
                html += "    <div class=\"card-sub\">/100</div>\n";
            }
            html += "  </div>\n";
        }
        html += "</div>\n";

        // ─── Track Details Section ─────────────────────────────────────────
        html += "<div class=\"section\">\n";
        html += "  <h2>Track Details</h2>\n";
        auto addMetric = [&](const juce::String& label, const juce::String& value) {
            html += "  <div class=\"metric-row\"><span class=\"metric-label\">" + label
                    + "</span><span class=\"metric-value\">" + value + "</span></div>\n";
        };
        addMetric("Active Tracks", juce::String(activeTrackCount_));
        addMetric("Tracks Clipping", juce::String(clippingTrackCount_));
        addMetric("Tracks with Low Signal", juce::String(lowSignalTrackCount_));
        addMetric("Master Peak", juce::String(masterPeakDb_, 1) + " dBFS");
        addMetric("Integrated LUFS", juce::String(masterIntegratedLUFS_, 1) + " LUFS");
        addMetric("Phase Correlation", juce::String(masterCorrelation_, 2));
        addMetric("Reference", currentScore_.hasReference ? "Loaded" : "Not loaded");
        if (currentScore_.hasReference && currentScore_.reference > 0)
            addMetric("Reference Score", juce::String(currentScore_.reference) + "/100");
        html += "</div>\n";

        // ─── Reference Comparison Section ─────────────────────────────────--
        if (referenceProfile_.valid && referenceProfile_.hasData()) {
            const auto& ref = referenceProfile_;
            html += "<div class=\"section\" style=\"border-color: #EC489940;\">\n";
            html += "  <h2>[TARGET] Reference Comparison";
            html += " <span style=\"font-size: 11px; color: #64748B; font-weight: normal;\">vs "
                    + ref.referenceName + "</span></h2>\n";

            // Spectral match bar
            int simPct = juce::jlimit(0, 100, (int)(ref.spectralSimilarity * 100.0f));
            juce::String simCol = simPct >= 70 ? "#10B981" : simPct >= 45 ? "#F59E0B" : "#EF4444";
            html += "  <div style=\"margin: 6px 0;\">\n";
            html += "    <div style=\"color: #94A3B8; font-size: 11px; margin-bottom: 2px;\">Spectral Match: <span style=\"color: " + simCol + "; font-weight: 600;\">" + juce::String(simPct) + "%</span></div>\n";
            html += "    <div style=\"height: 6px; background: #060A10; border-radius: 3px; overflow: hidden;\">\n";
            html += "      <div style=\"height: 100%; width: " + juce::String(simPct) + "%; background: " + simCol + "; border-radius: 3px;\"></div>\n";
            html += "    </div>\n";
            html += "  </div>\n";

            // Region deltas grid (6 regions in 2 rows)
            static const char* rgnNames[6] = {"Sub","Bass","LoMid","HiMid","Pres","Air"};
            static const char* rgnFreqs[6] = {"20-86","86-301","301-1k","1k-3.5k","3.5k-8.3k","8.3k-16k"};
            html += "  <div style=\"display: grid; grid-template-columns: repeat(3, 1fr); gap: 4px; margin: 8px 0;\">\n";
            for (int r = 0; r < 6; ++r) {
                float dVal = ref.deltaRegionEnergy[r];
                float mVal = ref.mixRegionEnergy[r];
                if (mVal < -90.0f) continue;
                juce::String dCol = std::abs(dVal) < 2.0f ? "#64748B" : dVal > 0 ? "#F59E0B" : "#38BDF8";
                juce::String dStr = (dVal > 0 ? "+" : "") + juce::String(dVal, 1) + " dB";
                html += "    <div style=\"background: #0A1018; border-radius: 4px; padding: 4px 6px;\">\n";
                html += "      <div style=\"font-size: 9px; font-weight: 700; color: #EC4899;\">" + juce::String(rgnNames[r]) + " <span style=\"color: #475569; font-weight: 400;\">" + juce::String(rgnFreqs[r]) + " Hz</span></div>\n";
                html += "      <div style=\"font-size: 11px; font-weight: 600; color: " + dCol + "\">" + dStr + "</div>\n";
                html += "      <div style=\"font-size: 9px; color: #64748B;\">ref " + juce::String(ref.refRegionEnergy[r], 1) + " | mix " + juce::String(mVal, 1) + "</div>\n";
                html += "    </div>\n";
            }
            html += "  </div>\n";

            // Summary metrics row
            auto addSummaryMetric = [&](const juce::String& label, const juce::String& value) {
                html += "  <div class=\"metric-row\"><span class=\"metric-label\">" + label
                        + "</span><span class=\"metric-value\">" + value + "</span></div>\n";
            };
            html += "  <div style=\"margin-top: 4px;\">\n";
            addSummaryMetric("LUFS Delta", (ref.deltaLUFS > 0 ? "+" : "") + juce::String(ref.deltaLUFS, 1) + " LU");
            addSummaryMetric("Crest Delta", (ref.deltaCrestFactor > 0 ? "+" : "") + juce::String(ref.deltaCrestFactor, 1) + " dB");
            addSummaryMetric("Gaps", juce::String(ref.totalGaps) + " (" + juce::String(ref.criticalGaps) + " crit, " + juce::String(ref.warningGaps) + " warn)");
            addSummaryMetric("Delta Score", juce::String((int)(ref.deltaScore * 100.0f)) + "%");
            html += "  </div>\n";
            html += "</div>\n";
        }

        // ─── Per-Track Diagnosis Section ─────────────────────────────────--
        if (!trackDiagnosis_.empty()) {
            html += "<div class=\"section\">\n";
            html += "  <h2>[SEARCH] Track Diagnosis</h2>\n";
            html += "  <table style=\"width:100%; border-collapse: collapse; font-size: 12px;\">\n";
            html += "    <tr style=\"color: #64748B; font-size: 10px;\">\n";
            html += "      <th style=\"text-align: left; padding: 2px 4px;\">Track</th>\n";
            html += "      <th style=\"text-align: left; padding: 2px 4px;\">Role</th>\n";
            html += "      <th style=\"text-align: center; padding: 2px 4px;\">Gain</th>\n";
            html += "      <th style=\"text-align: center; padding: 2px 4px;\">Dyn</th>\n";
            html += "      <th style=\"text-align: center; padding: 2px 4px;\">Tonal</th>\n";
            html += "      <th style=\"text-align: center; padding: 2px 4px;\">Phase</th>\n";
            html += "      <th style=\"text-align: left; padding: 2px 4px;\">Note</th>\n";
            html += "    </tr>\n";

            auto statusDot = [](int s) -> const char* {
                if (s == 0) return "\xF0\x9F\x9F\xA2";  // 🟢
                if (s == 1) return "\xF0\x9F\x9F\xA1";  // 🟡
                if (s == 2) return "\xF0\x9F\x94\xB4";  // 🔴
                return "\xE2\x9A\xAB";                  // ⚫
            };

            int maxRows = juce::jmin((int)trackDiagnosis_.size(), 20);
            for (int i = 0; i < maxRows; ++i) {
                const auto& entry = trackDiagnosis_[i];
                juce::String rowBg = (i % 2 == 0) ? "#0A1018" : "transparent";
                html += "    <tr style=\"background: " + rowBg + "\">\n";
                html += "      <td style=\"padding: 3px 4px; font-weight: 600; color: #E2E8F0;\">"
                        + entry.trackName + "</td>\n";
                html += "      <td style=\"padding: 3px 4px; color: #60A5FA;\">"
                        + (entry.roleName.isNotEmpty() && entry.roleName != "Unknown" ? entry.roleName : "—")
                        + "</td>\n";
                html += "      <td style=\"text-align: center; padding: 3px 4px;\">" + juce::String(statusDot(entry.gainStatus)) + "</td>\n";
                html += "      <td style=\"text-align: center; padding: 3px 4px;\">" + juce::String(statusDot(entry.dynamicsStatus)) + "</td>\n";
                html += "      <td style=\"text-align: center; padding: 3px 4px;\">" + juce::String(statusDot(entry.tonalStatus)) + "</td>\n";
                html += "      <td style=\"text-align: center; padding: 3px 4px;\">" + juce::String(statusDot(entry.phaseStatus)) + "</td>\n";
                html += "      <td style=\"padding: 3px 4px; color: #94A3B8; font-size: 11px;\">"
                        + (entry.worstMessage.isNotEmpty() ? entry.worstMessage : "") + "</td>\n";
                html += "    </tr>\n";
            }
            html += "  </table>\n";
            html += "</div>\n";
        }

        // ─── Evolution Section (Initial → Final) ──────────────────────────
        if (initialScoreCaptured_ && initialScore_.overall != currentScore_.overall) {
            int overallDelta = currentScore_.overall - initialScore_.overall;
            juce::String overallDeltaStr = (overallDelta >= 0)
                ? "[EXPAND] +" + juce::String(overallDelta) + " pts"
                : "[COLLAPSE] " + juce::String(-overallDelta) + " pts";

            html += "<div class=\"section\" style=\"border-color: #10B98140;\">\n";
            html += "  <h2>[TREND] Score Evolution</h2>\n";
            html += "  <div style=\"display: flex; align-items: center; gap: 12px; margin: 8px 0;\">\n";
            html += "    <span style=\"color: #64748B; font-size: 16px;\">" + juce::String(initialScore_.overall) + "</span>\n";
            html += "    <span style=\"color: #94A3B8; font-size: 18px;\">[RIGHT]</span>\n";
            html += "    <span style=\"font-size: 24px; font-weight: 700; color: " + scoreToColour(currentScore_.overall).toDisplayString(false) + "\">"
                    + juce::String(currentScore_.overall) + "</span>\n";
            html += "    <span style=\"font-size: 14px; font-weight: 600; color: " + juce::String(overallDelta >= 0 ? "#10B981" : "#EF4444") + "\">"
                    + overallDeltaStr + "</span>\n";
            html += "  </div>\n";

            // Domain deltas grid
            static const char* domLabels[5] = {"Gain", "Tonal", "Dynamics", "Spatial", "Reference"};
            int curDom[5] = {currentScore_.gain, currentScore_.tonal,
                             currentScore_.dynamics, currentScore_.spatial, currentScore_.reference};
            int iniDom[5] = {initialScore_.gain, initialScore_.tonal,
                             initialScore_.dynamics, initialScore_.spatial, initialScore_.reference};
            juce::Colour domCols[5] = {domainColour(0), domainColour(1), domainColour(2),
                                        domainColour(3), domainColour(4)};

            html += "  <div style=\"display: grid; grid-template-columns: repeat(5, 1fr); gap: 6px; margin-top: 4px;\">\n";
            for (int d = 0; d < 5; ++d) {
                int dDelta = curDom[d] - iniDom[d];
                bool hasData = (d != 4 || curDom[d] > 0);
                if (!hasData) continue;
                html += "    <div style=\"text-align: center; padding: 4px; border-radius: 6px; background: #0A1018;\">\n";
                html += "      <div style=\"font-size: 9px; font-weight: 700; color: " + domCols[d].toDisplayString(false) + "\">" + juce::String(domLabels[d]) + "</div>\n";
                html += "      <div style=\"font-size: 11px; color: #94A3B8;\">" + juce::String(iniDom[d]) + " [RIGHT] <span style=\"color: " + scoreToColour(curDom[d]).toDisplayString(false) + "; font-weight: 600;\">" + juce::String(curDom[d]) + "</span></div>\n";
                html += "      <div style=\"font-size: 10px; font-weight: 600; color: " + juce::String(dDelta >= 0 ? "#10B981" : "#EF4444") + "\">"
                        + (dDelta >= 0 ? "[EXPAND] +" : "[COLLAPSE] ") + juce::String(dDelta >= 0 ? dDelta : -dDelta) + "</div>\n";
                html += "    </div>\n";
            }
            html += "  </div>\n";
            html += "</div>\n";
        }

        // ─── Breakdown Section (detailed sub-scores) ───────────────────────
        html += "<div class=\"section\">\n";
        html += "  <h2>[GEAR] Breakdown Details</h2>\n";
        addMetric("Gain - Clipping", juce::String(currentScore_.gainClipping) + "/100");
        addMetric("Gain - Headroom", juce::String(currentScore_.gainHeadroom) + "/100");
        addMetric("Gain - L/R Balance", juce::String(currentScore_.gainLRBalance) + "/100");
        addMetric("Gain - Low Signal", juce::String(currentScore_.gainLowSignal) + "/100");
        addMetric("Tonal - Master Spectrum", juce::String(currentScore_.tonalMasterSpec) + "/100");
        addMetric("Tonal - Per Track", juce::String(currentScore_.tonalPerTrack) + "/100");
        addMetric("Tonal - Ref Alignment", juce::String(currentScore_.tonalRefAlignment) + "/100");
        addMetric("Dynamics - Loudness", juce::String(currentScore_.dynLoudness) + "/100");
        addMetric("Dynamics - Crest Factor", juce::String(currentScore_.dynCrest) + "/100");
        addMetric("Dynamics - Transients", juce::String(currentScore_.dynTransients) + "/100");
        addMetric("Dynamics - True Peak", juce::String(currentScore_.dynTruePeak) + "/100");
        addMetric("Spatial - Correlation", juce::String(currentScore_.spatCorrelation) + "/100");
        addMetric("Spatial - Stereo Width", juce::String(currentScore_.spatStereoWidth) + "/100");
        addMetric("Spatial - Mono Compat", juce::String(currentScore_.spatMonoCompat) + "/100");
        html += "</div>\n";

        // ─── Correction History ────────────────────────────────────────────
        html += "<div class=\"section\">\n";
        html += "  <h2>[CHANGE] Correction History (" + juce::String(totalCorrections_) + ")</h2>\n";
        if (correctionHistory_.empty()) {
            html += "  <div style=\"color: #64748B; font-size: 13px; text-align: center; padding: 20px;\">";
            html += "No corrections recorded yet.</div>\n";
        }
        else {
            int start = juce::jmax(0, (int)correctionHistory_.size() - 50);
            for (int i = start; i < (int)correctionHistory_.size(); ++i) {
                const auto& entry = correctionHistory_[i];
                const char* icon;
                juce::String sCol;
                switch (entry.finalStatus) {
                    case TrackRecommendation::Status::Applied:
                        icon = "OK"; sCol = "#10B981"; break;
                    case TrackRecommendation::Status::UnderApplied:
                        icon = "[WARN]"; sCol = "#F59E0B"; break;
                    case TrackRecommendation::Status::OverApplied:
                        icon = "\xE2\x9D\x8C"; sCol = "#EF4444"; break;
                    case TrackRecommendation::Status::Ignored:
                        icon = "\xE2\xAD\x90"; sCol = "#64748B"; break;
                    default:
                        icon = "\xE2\x9E\xA1"; sCol = "#3B82F6"; break;
                }
                html += "  <div class=\"correction-row\">\n";
                html += "    <span>" + juce::String(icon) + "</span>\n";
                html += "    <span style=\"flex:1;\"><b>" + entry.trackName + "</b>: " + entry.action + "</span>\n";
                html += "    <span class=\"badge\" style=\"background: " + sCol
                        + "20; color: " + sCol + "\">";
                switch (entry.finalStatus) {
                    case TrackRecommendation::Status::Applied:      html += "Applied"; break;
                    case TrackRecommendation::Status::UnderApplied: html += "Under"; break;
                    case TrackRecommendation::Status::OverApplied:  html += "Over"; break;
                    case TrackRecommendation::Status::Ignored:      html += "Ignored"; break;
                    default:                                        html += "Pending"; break;
                }
                if (entry.appliedRatio > 0.01f)
                    html += " " + juce::String((int)(entry.appliedRatio * 100.0f)) + "%";
                html += "</span>\n";
                html += "  </div>\n";
            }
        }
        html += "</div>\n";

        // ─── Footer ────────────────────────────────────────────────────────
        html += "<div class=\"footer\">\n";
        html += "Generated by MixCoach &mdash; AI-Powered Mixing Assistant<br>\n";
        html += "(C) 2026 MixCoach. All rights reserved.\n";
        html += "</div>\n";

        html += "</div>\n</body>\n</html>\n";
        return html;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  exportToPdfWithEdge — Convierte un archivo HTML a PDF usando Edge headless
    //  (WebView2 / Chromium engine). Retorna true si el PDF se generó correctamente.
    // ═══════════════════════════════════════════════════════════════════════════
    static bool exportToPdfWithEdge(const juce::String& htmlPath, const juce::String& pdfPath)
    {
#ifdef _WIN32
        // ─── Localizar msedge.exe ─────────────────────────────────────────────
        juce::String edgePaths[] = {
            "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe",
            "C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe",
        };
        juce::String edgeExe;
        for (const auto& p : edgePaths) {
            if (juce::File(p).existsAsFile()) {
                edgeExe = p;
                break;
            }
        }

        if (edgeExe.isEmpty()) {
            LogHelper::writeToLog("[EndOfSession] Edge no encontrado — no se puede generar PDF");
            return false;
        }

        juce::String args = "--headless --disable-gpu --print-to-pdf=\"" + pdfPath + "\" \"" + htmlPath + "\"";

        LogHelper::writeToLog("[EndOfSession] Generando PDF via Edge headless...");
        LogHelper::writeToLog("[EndOfSession]   Edge: " + edgeExe);
        LogHelper::writeToLog("[EndOfSession]   HTML: " + htmlPath);
        LogHelper::writeToLog("[EndOfSession]   PDF:  " + pdfPath);

        juce::String cmdLine = "\"" + edgeExe + "\" " + args;

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {};

        juce::String cmdCopy = cmdLine;
        char* mutableCmd = strdup(cmdCopy.toRawUTF8());

        BOOL success = CreateProcessA(
            nullptr, mutableCmd, nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

        free(mutableCmd);

        if (!success) {
            LogHelper::writeToLog("[EndOfSession] CreateProcess fallo: " + juce::String((int)GetLastError()));
            return false;
        }

        // ─── Esperar hasta 60 segundos ─────────────────────────────────────
        DWORD waitResult = WaitForSingleObject(pi.hProcess, 60000);

        if (waitResult == WAIT_TIMEOUT) {
            LogHelper::writeToLog("[EndOfSession] Edge headless TIMEOUT (60s) — terminating");
            TerminateProcess(pi.hProcess, 1);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return false;
        }

        // ─── Cerrar handles y verificar PDF ────────────────────────────────
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        if (!juce::File(pdfPath).existsAsFile()) {
            LogHelper::writeToLog("[EndOfSession] PDF no se genero (archivo no existe)");
            return false;
        }

        auto pdfSize = juce::File(pdfPath).getSize();
        if (pdfSize == 0) {
            LogHelper::writeToLog("[EndOfSession] PDF generado pero vacio");
            return false;
        }

        LogHelper::writeToLog("[EndOfSession] PDF generado OK: " + juce::String(pdfSize) + " bytes");
        return true;
#else
        juce::ignoreUnused(htmlPath, pdfPath);
        LogHelper::writeToLog("[EndOfSession] PDF solo disponible en Windows (requiere Edge/WebView2)");
        return false;
#endif
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  exportReport — Abre diálogo de guardado y genera PDF vía WebView2/Edge
    //  Fallback a HTML directo si Edge no está disponible.
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::exportReport()
    {
        auto now     = juce::Time::getCurrentTime();
        auto defaultName = "MixCoach_Report_" + now.formatted("%Y%m%d_%H%M%S");

        auto* chooser = new juce::FileChooser(
            "Save MixCoach Report",
            juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                .getChildFile(defaultName + ".pdf"),
            "*.pdf;*.html");

        chooser->launchAsync(juce::FileBrowserComponent::saveMode,
                             [this, chooser, defaultName](const juce::FileChooser&) {
                                 auto file = chooser->getResult();
                                 if (file == juce::File{}) {
                                     // BUG FIX: Liberar FileChooser si usuario cancela
                                     delete chooser;
                                     return;
                                 }

                                 juce::String html = generateHTMLReport();
                                 juce::String filePath = file.getFullPathName();

                                 // ─── Si .html, exportar directamente ────────────
                                 if (filePath.toLowerCase().endsWith(".html")) {
                                     juce::FileOutputStream stream(file);
                                     if (stream.openedOk()) {
                                         stream.writeText(html, false, false, "\n");
                                         stream.flush();
                                         LogHelper::writeToLog("[EndOfSession] HTML export: " + filePath);
                                         file.startAsProcess();
                                     }
                                     delete chooser;
                                     return;
                                 }

                                 // ─── HTML temporal → PDF via Edge headless ────
                                 auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
                                 auto tempHtml = tempDir.getChildFile(defaultName + ".html");
                                 {
                                     juce::FileOutputStream stream(tempHtml);
                                     if (!stream.openedOk()) {
                                         LogHelper::writeToLog("[EndOfSession] No se pudo crear temp");
                                         delete chooser;
                                         return;
                                     }
                                     stream.writeText(html, false, false, "\n");
                                     stream.flush();
                                 }

                                 bool pdfOk = exportToPdfWithEdge(tempHtml.getFullPathName(), filePath);
                                 tempHtml.deleteFile();

                                 if (pdfOk) {
                                     LogHelper::writeToLog("[EndOfSession] PDF exported: " + filePath);
                                     file.startAsProcess();
                                 }
                                 else {
                                     LogHelper::writeToLog("[EndOfSession] PDF fallo, exportando HTML");
                                     juce::FileOutputStream fallbackStream(
                                         file.getParentDirectory().getChildFile(defaultName + ".html"));
                                     if (fallbackStream.openedOk()) {
                                         fallbackStream.writeText(html, false, false, "\n");
                                         fallbackStream.flush();
                                     }
                                 }

                                 delete chooser;
                             });
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  mouseDown — Manejar clicks en botón de exportación
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::mouseDown(const juce::MouseEvent& e)
    {
        auto pos = e.getPosition();

        // Adjust for scroll
        pos.setY(pos.getY() + scrollOffset_);

        // Toggle technical details
        if (toggleBounds_.contains(pos)) {
            showTechnicalDetails_ = !showTechnicalDetails_;
            resized();
            repaint();
            return;
        }

        if (exportButtonBounds_.contains(pos)) {
            exportReport();
        }

        // GAP #3: Click en botón "Nueva sesión"
        if (newSessionBounds_.contains(pos) && onNewSession) {
            onNewSession();
        }

    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  mouseMove — Track hover state for cards and button
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::mouseMove(const juce::MouseEvent& e)
    {
        auto pos = e.getPosition();
        pos.setY(pos.getY() + scrollOffset_);

        // Toggle hover
        toggleHovered_ = toggleBounds_.contains(pos);

        int oldHovered = hoveredCard_;
        hoveredCard_ = -1;
        for (int i = 0; i < 5; ++i) {
            if (scoreCardsBounds_[i].contains(pos)) {
                hoveredCard_ = i;
                break;
            }
        }

        bool oldBtnHover = exportButtonHovered_;
        exportButtonHovered_ = exportButtonBounds_.contains(pos);

        bool oldNsHover = newSessionHovered_;
        newSessionHovered_ = newSessionBounds_.contains(pos);

        if (oldHovered != hoveredCard_ || oldBtnHover != exportButtonHovered_
            || oldNsHover != newSessionHovered_)
            repaint();

        setMouseCursor((exportButtonHovered_ || newSessionHovered_)
                        ? juce::MouseCursor::PointingHandCursor
                        : juce::MouseCursor::NormalCursor);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  mouseWheelMove — Scroll content
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::mouseWheelMove(const juce::MouseEvent& e,
                                                 const juce::MouseWheelDetails& wheel)
    {
        int maxScroll = juce::jmax(0, contentHeight_ - getHeight());
        scrollOffset_ = juce::jlimit(0, maxScroll,
                                      scrollOffset_ - (int)(wheel.deltaY * 40.0f));
        // Avatar child component must scroll with content
        reportAvatar_.setBounds(avatarBounds_.withY(avatarBounds_.getY() - scrollOffset_));
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawEmptyState — Sin datos de sesión
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::drawEmptyState(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        auto area = bounds.reduced(40, 0);

        // ─── Icono ───────────────────────────────────────────────────────────
        auto iconArea = area.removeFromTop(80).withSizeKeepingCentre(72, 72);
        g.setFont(juce::Font(juce::FontOptions(44.0f)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.22f));
        g.drawText("·", iconArea, juce::Justification::centred);

        area.removeFromTop(16);

        // ─── Título ─────────────────────────────────────────────────────────
        auto titleArea = area.removeFromTop(28);
        g.setFont(juce::Font(juce::FontOptions(18.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText("No Session Report Yet", titleArea, juce::Justification::centred);

        area.removeFromTop(8);

        // ─── Descripción ────────────────────────────────────────────────────
        auto descArea = area.removeFromTop(40);
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.setColour(MixCoachTheme::textMuted());
        g.drawText("Complete a mixing session and end it to see your report.\nScores, corrections, and progress history will appear here.",
                   descArea, juce::Justification::centred);

        area.removeFromTop(24);

        // ─── Hint ───────────────────────────────────────────────────────────
        auto hintArea = area.removeFromTop(32).withSizeKeepingCentre(220, 32);
        g.setColour(MixCoachTheme::accent().withAlpha(0.10f));
        g.fillRoundedRectangle(hintArea.toFloat(), MixCoachTheme::cornerRadius_pill);
        g.setColour(MixCoachTheme::accent().withAlpha(0.30f));
        g.drawRoundedRectangle(hintArea.toFloat(), MixCoachTheme::cornerRadius_pill, 0.5f);

        g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
        g.setColour(MixCoachTheme::accent());
        g.drawText("\xF0\x9F\x91\x89  End your current session to generate a report",
                   hintArea, juce::Justification::centred);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawLoadingState — Spinner animado + mensaje contextual
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::drawLoadingState(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        auto area = bounds.reduced(40, 0);

        // ─── Spinner ─────────────────────────────────────────────────────────
        auto spinnerArea = area.removeFromTop(72).withSizeKeepingCentre(56, 56);
        float cx = (float)spinnerArea.getCentreX();
        float cy = (float)spinnerArea.getCentreY();
        float r  = 22.0f;

        float angle = (juce::Time::getMillisecondCounter() % 2000) / 2000.0f * juce::MathConstants<float>::twoPi;
        int numDots = 8;

        for (int i = 0; i < numDots; ++i) {
            float dotAngle = angle + juce::MathConstants<float>::twoPi * i / numDots;
            float dx = cx + r * std::cos(dotAngle);
            float dy = cy + r * std::sin(dotAngle);
            float alpha = 0.2f + 0.8f * (1.0f - (float)i / numDots);
            g.setColour(MixCoachTheme::accentGlow().withAlpha(alpha));
            g.fillEllipse(dx - 3.0f, dy - 3.0f, 6.0f, 6.0f);
        }

        area.removeFromTop(20);

        // ─── Título ─────────────────────────────────────────────────────────
        auto titleArea = area.removeFromTop(24);
        g.setFont(juce::Font(juce::FontOptions(16.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText("Generating Report", titleArea, juce::Justification::centred);

        area.removeFromTop(8);

        // ─── Subtítulo con mensajes rotativos ─────────────────────────────────
        auto subArea = area.removeFromTop(32);
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.setColour(MixCoachTheme::textMuted());

        int epoch = (juce::Time::getMillisecondCounter() / 5000) % 4;
        const char* msgs[] = {
            "Computing MixScore...",
            "Gathering correction history...",
            "Building reference comparison...",
            "Preparing session report..."
        };
        g.drawText(juce::String(msgs[epoch]), subArea, juce::Justification::centred);

        // ─── Barra indeterminada ─────────────────────────────────────────────
        area.removeFromTop(12);
        auto barArea = area.removeFromTop(6).withSizeKeepingCentre(200, 6);
        g.setColour(MixCoachTheme::bgDarker());
        g.fillRoundedRectangle(barArea.toFloat(), 3.0f);

        float barPhase = (juce::Time::getMillisecondCounter() % 1500) / 1500.0f;
        int barW = juce::jmax(20, barArea.getWidth() / 3);
        int barX = barArea.getX() + (int)((barArea.getWidth() - barW) * barPhase);
        g.setColour(MixCoachTheme::accentGlow().withAlpha(0.70f));
        g.fillRoundedRectangle(juce::Rectangle<int>(barX, barArea.getY(), barW, barArea.getHeight()).toFloat(), 3.0f);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawErrorState — Error al cargar datos del reporte
    // ═══════════════════════════════════════════════════════════════════════════
    void EndOfSessionComponent::drawErrorState(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        auto area = bounds.reduced(40, 0);

        // ─── Icono ───────────────────────────────────────────────────────────
        auto iconArea = area.removeFromTop(60).withSizeKeepingCentre(56, 56);
        g.setFont(juce::Font(juce::FontOptions(34.0f)));
        g.setColour(MixCoachTheme::error().withAlpha(0.55f));
        g.drawText(juce::CharPointer_UTF8("[WARN]"), iconArea, juce::Justification::centred);

        area.removeFromTop(12);

        // ─── Título ─────────────────────────────────────────────────────────
        auto titleArea = area.removeFromTop(24);
        g.setFont(juce::Font(juce::FontOptions(16.0f)).boldened());
        g.setColour(MixCoachTheme::error());
        g.drawText("Report Generation Failed", titleArea, juce::Justification::centred);

        area.removeFromTop(8);

        // ─── Mensaje de error ───────────────────────────────────────────────
        auto msgArea = area.removeFromTop(36);
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.setColour(MixCoachTheme::textMuted());
        juce::String displayMsg = errorMessage_.isNotEmpty()
            ? errorMessage_
            : "Could not generate the session report.\nPlease check that the session is active and try again.";
        g.drawText(displayMsg, msgArea, juce::Justification::centred);

        area.removeFromTop(16);

        // ─── Botón retry ────────────────────────────────────────────────────
        auto retryArea = area.removeFromTop(32).withSizeKeepingCentre(160, 32);
        g.setColour(MixCoachTheme::error().withAlpha(0.08f));
        g.fillRoundedRectangle(retryArea.toFloat(), MixCoachTheme::cornerRadius_pill);
        g.setColour(MixCoachTheme::error().withAlpha(0.30f));
        g.drawRoundedRectangle(retryArea.toFloat(), MixCoachTheme::cornerRadius_pill, 0.5f);

        g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
        g.setColour(MixCoachTheme::error());
        g.drawText("\xE2\x9F\xB3  Retry", retryArea, juce::Justification::centred);
    }

} // namespace mixcoach
