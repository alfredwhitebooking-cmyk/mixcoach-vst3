#include "MixScore.h"
#include "CoachEngine.h"
#include "ReferenceDrivenEngine.h"
#include "SpectralProfiler.h"
#include "../audio/AudioAnalyzer.h"
#include "../../Common/memory/SlotRegistry.h"
#include "../../Common/memory/SharedData.h"
#include "../../Common/types/Constants.h"
#include "../../Common/types/LogHelper.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Helpers internos
    // ═══════════════════════════════════════════════════════════════════════════

    static int clampScore(int s) noexcept
    {
        return juce::jlimit(0, 100, s);
    }

    static float absF(float v) noexcept
    {
        return v < 0.0f ? -v : v;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  computeGainScore — Evalúa clipping, headroom, balance L/R, low signal
    //  AHORA usa datos per-track del bg worker (TrackAudioResult) + CoachEngine
    // ═══════════════════════════════════════════════════════════════════════════
    static int computeGainScore(const CoachEngine& engine,
                                const AudioAnalyzer& analyzer,
                                MixScore& score) // populate breakdown fields
    {
        int gainScore = 100;
        int clipScore = 100, headroomScore = 100, lrScore = 100, lowSigScore = 100;

        // ─── Master peak → headroom ────────────────────────────────────────
        const auto& master = analyzer.getMasterAnalysis();
        float masterPeak   = master.getPeak();
        score.masterPeakDb = masterPeak;

        if (masterPeak > -0.5f) {
            headroomScore = 10;
            gainScore -= 30;
        }
        else if (masterPeak > -3.0f) {
            headroomScore = 40;
            gainScore -= 20;
        }
        else if (masterPeak > -6.0f) {
            headroomScore = 70;
            gainScore -= 10;
        }
        else if (masterPeak > -10.0f) {
            headroomScore = 90;
            gainScore -= 3;
        }
        else {
            headroomScore = 100;
        }

        // ─── Clipping per-track ─────────────────────────────────────────────
        // Use CoachEngine's gain staging result + per-track audio cache
        int clipCount             = engine.lastGainStagingResult_.clippingCount;
        int lowSigCount           = engine.lastGainStagingResult_.lowSignalCount;
        score.clippingTrackCount  = clipCount;
        score.lowSignalTrackCount = lowSigCount;

        if (clipCount > 0) {
            clipScore = clampScore(100 - clipCount * 20);
            gainScore -= clipCount * 15;
            if (clipCount >= 3) gainScore -= 20; // Massive clipping penalty
        }

        if (lowSigCount > 0) {
            lowSigScore = clampScore(100 - lowSigCount * 15);
            gainScore -= lowSigCount * 5;
        }

        // ─── L/R balance ────────────────────────────────────────────────────
        float peakL = analyzer.getLeftAnalysis().getPeak();
        float peakR = analyzer.getRightAnalysis().getPeak();
        if (peakL > -50.0f && peakR > -50.0f) {
            float lrDiff = absF(peakL - peakR);
            if (lrDiff > 8.0f) {
                lrScore = 30;
                gainScore -= 15;
            }
            else if (lrDiff > 5.0f) {
                lrScore = 60;
                gainScore -= 8;
            }
            else if (lrDiff > 3.0f) {
                lrScore = 85;
                gainScore -= 3;
            }
        }

        // ─── Penalizar si no hay tracks activos ─────────────────────────────
        if (score.activeTrackCount == 0) {
            gainScore -= 30;
        }

        // ─── Breakdown ─────────────────────────────────────────────────────
        score.gainClipping  = clampScore(clipScore);
        score.gainHeadroom  = clampScore(headroomScore);
        score.gainLRBalance = clampScore(lrScore);
        score.gainLowSignal = clampScore(lowSigScore);

        return clampScore(gainScore);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  computeTonalScore — Evalúa balance espectral usando SpectralProfiler
    //  AHORA usa perfiles espectrales per-track + referencia + género
    // ═══════════════════════════════════════════════════════════════════════════
    static int computeTonalScore(const CoachEngine& engine,
                                 const AudioAnalyzer& analyzer,
                                 const juce::String& genre,
                                 MixScore& score)
    {
        int tonalScore      = 70; // Neutral by default
        int masterSpecScore = 80, perTrackScore = 80, refAlignScore = 100;

        // ─── Si hay referencia de audio, usar gaps del ReferenceDrivenEngine ─
        if (engine.hasReferenceAudio()) {
            auto gaps = engine.getReferenceGaps();
            if (!gaps.empty()) {
                int refPenalty = 0;
                int refPraise  = 0;
                for (const auto& g : gaps) {
                    if (g.domain == Domain::Tonal) {
                        switch (g.severity) {
                            case GapSeverity::Critical:
                                refPenalty += 20;
                                break;
                            case GapSeverity::Warning:
                                refPenalty += 10;
                                break;
                            case GapSeverity::Info:
                                refPenalty += 3;
                                break;
                            case GapSeverity::Praise:
                                refPraise += 5;
                                break;
                        }
                    }
                }
                refAlignScore = clampScore(100 - refPenalty + refPraise);
                tonalScore -= refPenalty;
                tonalScore += refPraise;
            }
            else {
                // Sin gaps = alineado con referencia
                refAlignScore = 95;
                tonalScore += 15;
            }
        }

        // ─── Master spectrum vs género ──────────────────────────────────────
        if (genre.isNotEmpty() && genre != "Unknown" && genre != "Desconocido") {
            auto& profile        = CoachEngine::getGenreProfile(genre);
            float integratedLUFS = analyzer.getIntegratedLUFS();
            if (integratedLUFS > -40.0f) {
                float lufsDiff = absF(integratedLUFS - profile.targetIntegratedLUFS);
                if (lufsDiff > profile.lufsTolerance * 2.0f) {
                    masterSpecScore = 40;
                    tonalScore -= 15;
                }
                else if (lufsDiff > profile.lufsTolerance) {
                    masterSpecScore = 70;
                    tonalScore -= 5;
                }
                else {
                    masterSpecScore = 100;
                    tonalScore += 5;
                }
            }
        }

        // ─── Per-track spectral balance (iterate active tracks) ────────────
        // Use SpectralProfiler to check if tracks have reasonable spectral distribution
        auto* sharedData = SharedData::safeGetInstance();
        int weakTracks   = 0;
        int totalChecked = 0;

        if (sharedData != nullptr) {
            auto& registry = sharedData->getSlotRegistry();
            registry.forEachActive([&](const SlotInfo& info) {
                int idx = info.slotIndex;
                if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;

                auto result = sharedData->getTrackAudioResult(idx);
                if (result.peakLeft > -60.0f || result.peakRight > -60.0f) {
                    totalChecked++;
                    auto profile = SpectralProfiler::computeProfile(result);

                    // Check if any band has extreme energy imbalance
                    // (an isolated band >15dB above neighbors → potential resonance/issue)
                    for (int b = 1; b < 5; ++b) {
                        if (profile.bandLevelDb[b] > -60.0f && profile.bandLevelDb[b - 1] > -60.0f
                            && profile.bandLevelDb[b + 1] > -60.0f) {
                            float aboveLeft  = profile.bandLevelDb[b] - profile.bandLevelDb[b - 1];
                            float aboveRight = profile.bandLevelDb[b] - profile.bandLevelDb[b + 1];
                            if (aboveLeft > 15.0f && aboveRight > 15.0f) {
                                weakTracks++;
                                break;
                            }
                        }
                    }
                }
            });
        }

        if (totalChecked > 0 && weakTracks > 0) {
            float ratio = (float)weakTracks / totalChecked;
            if (ratio > 0.3f) {
                perTrackScore = 30;
                tonalScore -= 20;
            }
            else if (ratio > 0.15f) {
                perTrackScore = 60;
                tonalScore -= 10;
            }
            else {
                perTrackScore = 85;
                tonalScore -= 3;
            }
        }

        score.tonalMasterSpec   = clampScore(masterSpecScore);
        score.tonalPerTrack     = clampScore(perTrackScore);
        score.tonalRefAlignment = clampScore(refAlignScore);

        return clampScore(tonalScore);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  computeDynamicsScore — Evalúa LUFS, crest, transients, true peak
    //  AHORA usa datos per-track de envelope (attackTime, releaseTime, sustain)
    // ═══════════════════════════════════════════════════════════════════════════
    static int computeDynamicsScore(const CoachEngine& engine,
                                    const AudioAnalyzer& analyzer,
                                    const juce::String& genre,
                                    MixScore& score)
    {
        int dynScore  = 70;
        int lufsScore = 80, crestScore = 80, transScore = 100, truePeakScore = 100;

        float integratedLUFS       = analyzer.getIntegratedLUFS();
        float loudnessRange        = analyzer.getLoudnessRange();
        float truePeakDBTP         = analyzer.getTruePeakDBTP();
        score.masterIntegratedLUFS = integratedLUFS;

        if (integratedLUFS > -40.0f) {
            // ─── Loudness range (LRA) ──────────────────────────────────────
            if (loudnessRange > 10.0f) {
                lufsScore = 30;
                dynScore -= 15;
            }
            else if (loudnessRange > 6.0f) {
                lufsScore = 60;
                dynScore -= 5;
            }
            else if (loudnessRange < 2.0f) {
                lufsScore = 50;
                dynScore -= 8; // Over-compressed
            }

            // ─── LUFS vs género ────────────────────────────────────────────
            if (genre.isNotEmpty() && genre != "Unknown" && genre != "Desconocido") {
                auto& profile  = CoachEngine::getGenreProfile(genre);
                float lufsDiff = absF(integratedLUFS - profile.targetIntegratedLUFS);

                if (lufsDiff > profile.lufsTolerance * 2.0f) {
                    lufsScore = juce::jmin(lufsScore, 40);
                    dynScore -= 15;
                }
                else if (lufsDiff > profile.lufsTolerance) {
                    lufsScore = juce::jmin(lufsScore, 65);
                    dynScore -= 5;
                }
                else {
                    lufsScore = juce::jmin(lufsScore, 95);
                    dynScore += 5;
                }
            }

            // ─── Crest factor aproximado del master ────────────────────────
            float peakL      = analyzer.getLeftAnalysis().getPeak();
            float peakR      = analyzer.getRightAnalysis().getPeak();
            float rmsL       = analyzer.getLeftAnalysis().getRMS();
            float rmsR       = analyzer.getRightAnalysis().getRMS();
            float masterPeak = juce::jmax(peakL, peakR);
            float masterRMS  = juce::jmax(rmsL, rmsR);

            if (masterRMS > -60.0f && masterPeak > -60.0f) {
                float crest = masterPeak - masterRMS;
                if (crest < 4.0f) {
                    crestScore = 20; // Over-compressed
                    dynScore -= 15;
                }
                else if (crest > 20.0f) {
                    crestScore = 50; // Very dynamic
                    dynScore -= 8;
                }
                else if (crest > 8.0f && crest < 16.0f) {
                    crestScore = 100; // Healthy range
                    dynScore += 5;
                }
                else {
                    crestScore = 75;
                }
            }
        }

        // ─── Per-track transient analysis ───────────────────────────────────
        // Use envelope data (attackTime, releaseTime, transientRatio)
        auto* sharedData      = SharedData::safeGetInstance();
        int extremeTransients = 0;
        int totalDynTracks    = 0;

        if (sharedData != nullptr) {
            auto& registry = sharedData->getSlotRegistry();
            registry.forEachActive([&](const SlotInfo& info) {
                int idx = info.slotIndex;
                if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;

                auto result = sharedData->getTrackAudioResult(idx);
                if (result.peakLeft > -60.0f || result.peakRight > -60.0f) {
                    totalDynTracks++;
                    // transientRatio > 3.0 → extreme transient (hihat, clap)
                    if (result.transientRatio > 3.0f) extremeTransients++;
                }
            });
        }

        if (totalDynTracks > 0) {
            float extRatio = (float)extremeTransients / totalDynTracks;
            if (extRatio > 0.3f) {
                transScore = 30;
                dynScore -= 15;
            }
            else if (extRatio > 0.15f) {
                transScore = 65;
                dynScore -= 5;
            }
        }

        // ─── True Peak ──────────────────────────────────────────────────────
        if (truePeakDBTP > -1.0f) {
            truePeakScore = 20;
            dynScore -= 15;
        }
        else if (truePeakDBTP > -3.0f) {
            truePeakScore = 60;
            dynScore -= 5;
        }

        score.dynLoudness   = clampScore(lufsScore);
        score.dynCrest      = clampScore(crestScore);
        score.dynTransients = clampScore(transScore);
        score.dynTruePeak   = clampScore(truePeakScore);

        return clampScore(dynScore);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  computeSpatialScore — Evalúa correlación, stereo width, mono compat
    //  AHORA usa datos per-track de stereoWidthPerBand + midSideRatio
    // ═══════════════════════════════════════════════════════════════════════════
    static int computeSpatialScore(const CoachEngine& /*engine*/, const AudioAnalyzer& analyzer, MixScore& score)
    {
        int spatialScore = 80;
        int corrScore = 80, widthScore = 90, monoScore = 100;

        const auto& master      = analyzer.getMasterAnalysis();
        float correlation       = master.getCorrelation();
        score.masterCorrelation = correlation;

        // ─── Master phase correlation ──────────────────────────────────────
        if (correlation < 0.0f) {
            corrScore = 10;
            spatialScore -= 35; // Out of phase — severe
        }
        else if (correlation < 0.3f) {
            corrScore = 30;
            spatialScore -= 20;
        }
        else if (correlation > 0.8f) {
            corrScore = 60;
            spatialScore -= 5; // Too mono
        }
        else if (correlation >= 0.4f && correlation <= 0.7f) {
            corrScore = 100;
            spatialScore += 5; // Ideal range
        }

        // ─── Per-track stereo width analysis ───────────────────────────────
        // Check if any track has extreme stereo width issues
        auto* sharedData = SharedData::safeGetInstance();
        int wideTracks = 0, narrowTracks = 0;
        int totalSpatialTracks = 0;

        if (sharedData != nullptr) {
            auto& registry = sharedData->getSlotRegistry();
            registry.forEachActive([&](const SlotInfo& info) {
                int idx = info.slotIndex;
                if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;

                auto result = sharedData->getTrackAudioResult(idx);
                if (result.peakLeft > -60.0f || result.peakRight > -60.0f) {
                    totalSpatialTracks++;
                    // Average stereo width across 6 regions
                    float avgWidth = 0.0f;
                    for (int b = 0; b < 6; ++b) avgWidth += result.stereoWidthPerBand[b];
                    avgWidth /= 6.0f;

                    if (avgWidth > 0.7f) wideTracks++; // Extremely wide — potential phase issues
                    else if (avgWidth < 0.05f && result.correlation > -0.3f)
                        narrowTracks++; // Near-mono track (intentional for bass/kick is OK)
                }
            });

            // Mono compatibility: if few tracks are very wide, we're mono-compatible
            if (totalSpatialTracks > 0) {
                float wideRatio = (float)wideTracks / totalSpatialTracks;
                if (wideRatio > 0.5f) {
                    widthScore = 30;
                    spatialScore -= 15;
                    monoScore = 40;
                }
                else if (wideRatio > 0.25f) {
                    widthScore = 65;
                    spatialScore -= 5;
                    monoScore = 75;
                }
            }
        }

        score.spatCorrelation = clampScore(corrScore);
        score.spatStereoWidth = clampScore(widthScore);
        score.spatMonoCompat  = clampScore(monoScore);

        return clampScore(spatialScore);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  computeReferenceScore — Evalúa qué tan alineada está la mezcla con la ref
    //  (sin cambios — ya usaba ReferenceDrivenEngine)
    // ═══════════════════════════════════════════════════════════════════════════
    static int computeReferenceScore(const CoachEngine& engine)
    {
        if (!engine.hasReference()) return 0; // Sin referencia → score 0 (no ponderado)

        if (engine.hasReferenceAudio()) {
            auto gaps = engine.getReferenceGaps();
            if (gaps.empty()) return 95;

            int score = 70;
            for (const auto& g : gaps) {
                switch (g.severity) {
                    case GapSeverity::Critical:
                        score -= 15;
                        break;
                    case GapSeverity::Warning:
                        score -= 8;
                        break;
                    case GapSeverity::Info:
                        score -= 3;
                        break;
                    case GapSeverity::Praise:
                        score += 3;
                        break;
                }
            }

            auto& plan = engine.getPlanManager();
            if (plan.hasPlan()) {
                int planPct = static_cast<int>(plan.getProgress() * 100.0f);
                score       = (score + planPct) / 2;
            }

            return clampScore(score);
        }

        return 50;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  MixScore::compute — Punto de entrada principal
    // ═══════════════════════════════════════════════════════════════════════════
    MixScore MixScore::compute(const CoachEngine& engine, const AudioAnalyzer& analyzer, const juce::String& genre)
    {
        MixScore s;
        s.genre = genre;

        // ─── Active track count ──────────────────────────────────────────────
        auto* sharedData = SharedData::safeGetInstance();
        if (sharedData != nullptr) s.activeTrackCount = sharedData->getSlotRegistry().activeCount();

        s.hasReference = engine.hasReference();

        // ═══ Compute domain scores (each populates its breakdown fields) ════
        s.gain      = computeGainScore(engine, analyzer, s);
        s.tonal     = computeTonalScore(engine, analyzer, genre, s);
        s.dynamics  = computeDynamicsScore(engine, analyzer, genre, s);
        s.spatial   = computeSpatialScore(engine, analyzer, s);
        s.reference = computeReferenceScore(engine);

        // ═══ Overall: weighted average ═══════════════════════════════════════
        bool hasRef = (s.reference > 0);

        if (hasRef) {
            s.overall = (s.gain * 25 + s.tonal * 25 + s.dynamics * 20 + s.spatial * 15 + s.reference * 15) / 100;
        }
        else {
            s.overall = (s.gain * 30 + s.tonal * 30 + s.dynamics * 20 + s.spatial * 20) / 100;
        }

        s.overall = clampScore(s.overall);

        // ═══ Status label ═══════════════════════════════════════════════════
        if (s.overall >= 90) s.statusLabel = "Excelente";
        else if (s.overall >= 75)
            s.statusLabel = "Buena";
        else if (s.overall >= 55)
            s.statusLabel = "Regular";
        else if (s.overall >= 35)
            s.statusLabel = "Necesita trabajo";
        else
            s.statusLabel = "Critica";

        LogHelper::writeToLog("[MixScore] Score: " + juce::String(s.overall) + " (G:" + juce::String(s.gain)
                              + " T:" + juce::String(s.tonal) + " D:" + juce::String(s.dynamics)
                              + " S:" + juce::String(s.spatial) + " R:" + juce::String(s.reference)
                              + ") tracks=" + juce::String(s.activeTrackCount)
                              + " clip=" + juce::String(s.clippingTrackCount) + " - " + s.statusLabel);

        return s;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  MixScore::toTextSummary — Para el contexto del LLM (corto)
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String MixScore::toTextSummary() const
    {
        juce::String s;

        s += "[MIX SCORE " + juce::String(overall) + "/100 - " + statusLabel + "]\n";

        // Barra visual
        int bars = overall / 10;
        s += "  [";
        for (int i = 0; i < 10; ++i) s += (i < bars) ? "#" : ".";
        s += "]\n\n";

        s += "  * Gain:      " + juce::String(gain) + "/100";
        if (clippingTrackCount > 0) s += " (clipping: " + juce::String(clippingTrackCount) + " tracks)";
        s += "\n";

        s += "  * Tonal:     " + juce::String(tonal) + "/100\n";
        s += "  * Dynamics:  " + juce::String(dynamics) + "/100\n";
        s += "  * Spatial:   " + juce::String(spatial) + "/100\n";

        if (reference > 0) s += "  * Reference: " + juce::String(reference) + "/100\n";
        else
            s += "  * Reference: N/A (load a reference track for alignment scoring)\n";

        s += "\n";
        return s;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  MixScore::toDetailedString — Breakdown completo para UI
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String MixScore::toDetailedString() const
    {
        juce::String s;
        s += "=== MIX SCORE: " + juce::String(overall) + "/100 - " + statusLabel + " ===\n\n";

        s += "Tracks: " + juce::String(activeTrackCount) + " | Clipping: " + juce::String(clippingTrackCount)
             + " | Low signal: " + juce::String(lowSignalTrackCount) + "\n\n";

        // ─── Gain ──────────────────────────────────────────────────────────
        s += "[GAIN " + juce::String(gain) + "/100]\n";
        s += "  * Clipping:     " + juce::String(gainClipping) + "/100";
        if (clippingTrackCount > 0) s += " (" + juce::String(clippingTrackCount) + " tracks clipping!)";
        s += "\n";
        s += "  * Headroom:     " + juce::String(gainHeadroom) + "/100";
        if (masterPeakDb > -50.0f) s += " (master peak: " + juce::String(masterPeakDb, 1) + " dBFS)";
        s += "\n";
        s += "  * L/R Balance:  " + juce::String(gainLRBalance) + "/100\n";
        s += "  * Low Signal:   " + juce::String(gainLowSignal) + "/100";
        if (lowSignalTrackCount > 0) s += " (" + juce::String(lowSignalTrackCount) + " tracks with low signal)";
        s += "\n\n";

        // ─── Tonal ─────────────────────────────────────────────────────────
        s += "[TONAL " + juce::String(tonal) + "/100]\n";
        s += "  * Master Spec:   " + juce::String(tonalMasterSpec) + "/100\n";
        s += "  * Per-track:     " + juce::String(tonalPerTrack) + "/100\n";
        if (reference > 0) s += "  * Ref Alignment: " + juce::String(tonalRefAlignment) + "/100\n";
        s += "\n";

        // ─── Dynamics ──────────────────────────────────────────────────────
        s += "[DYNAMICS " + juce::String(dynamics) + "/100]\n";
        s += "  * Loudness:    " + juce::String(dynLoudness) + "/100";
        if (masterIntegratedLUFS > -40.0f) s += " (LUFS: " + juce::String(masterIntegratedLUFS, 1) + ")";
        s += "\n";
        s += "  * Crest:       " + juce::String(dynCrest) + "/100\n";
        s += "  * Transients:  " + juce::String(dynTransients) + "/100\n";
        s += "  * True Peak:   " + juce::String(dynTruePeak) + "/100\n\n";

        // ─── Spatial ───────────────────────────────────────────────────────
        s += "[SPATIAL " + juce::String(spatial) + "/100]\n";
        s += "  * Correlation: " + juce::String(spatCorrelation) + "/100";
        if (masterCorrelation > -2.0f) s += " (phase: " + juce::String(masterCorrelation, 2) + ")";
        s += "\n";
        s += "  * Stereo Width: " + juce::String(spatStereoWidth) + "/100\n";
        s += "  * Mono Compat:  " + juce::String(spatMonoCompat) + "/100\n\n";

        // ─── Reference ─────────────────────────────────────────────────────
        if (reference > 0) s += "[REFERENCE " + juce::String(reference) + "/100] (loaded)\n";
        else
            s += "[REFERENCE N/A] (no reference loaded)\n";

        return s;
    }

} // namespace mixcoach
