#include "CoachEngine.h"
#include "../../Common/types/LogHelper.h"
#include "../../Common/name/NameInferrer.h"
#include "../../Messenger/core/MessengerType.h"

namespace mixcoach {

    // Forward declarations for free functions defined below
    TrackRole getTrackRoleForTrackType(TrackType type) noexcept;
    TrackType getTrackTypeForRole(TrackRole role) noexcept;

    // ═══════════════════════════════════════════════════════════════════════════
    //  SILENCE + LOAD ORDER INFERENCE V13
    // ═══════════════════════════════════════════════════════════════════════════

    static constexpr TrackRole kSignalOrderRoles[] = {
        TrackRole::Kick,         // Position 0
        TrackRole::Snare,        // Position 1
        TrackRole::HiHat,        // Position 2
        TrackRole::VozPrincipal, // Position 3
        TrackRole::BassSub,      // Position 4
        TrackRole::Clap,         // Position 5
        TrackRole::SynthPad,     // Position 6
        TrackRole::Percussion,   // Position 7
        TrackRole::FxRiser,      // Position 8
        TrackRole::FxAmbience,   // Position 9
    };

    static constexpr float kSignalOrderConfidences[] = {
        0.60f,
        0.50f,
        0.45f,
        0.55f,
        0.40f,
        0.45f,
        0.35f,
        0.40f,
        0.35f,
        0.30f,
    };

    static constexpr int kSignalOrderRolesCount = sizeof(kSignalOrderRoles) / sizeof(kSignalOrderRoles[0]);

    // ═══════════════════════════════════════════════════════════════════════════
    //  detectFirstSignal
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::detectFirstSignal()
    {
        int64_t now    = juce::Time::getMillisecondCounter() * 1000;
        auto& registry = sharedData_.getSlotRegistry();

        registry.forEachActive([&](const SlotInfo& info) {
            if (info.muted || (soloActive_ && !info.soloed)) return;
            int idx = info.slotIndex;
            if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;
            if (firstSignalTimestampsUs_[idx] > 0) return;

            auto telem = getLatestTelemetry(idx);
            if (telem.timestamp == 0) return;

            float peakDb = juce::jmax(telem.peakLeft, telem.peakRight);
            if (peakDb > kSignalThresholdDb && signalOrderCount_ < kMaxSignalOrder) {
                firstSignalTimestampsUs_[idx]          = now;
                signalOrderIndices_[signalOrderCount_] = idx;
                signalOrderCount_++;
                LogHelper::writeToLog("[SignalOrder] Slot " + juce::String(idx) + " primera senial en posicion "
                                      + juce::String(signalOrderCount_ - 1) + " (peak=" + juce::String(peakDb, 1)
                                      + " dB)");
            }

            // Sprint 6A: Role-aware gain analysis
            auto gainAdvice = analyzeTrackGain(idx);
            if (gainAdvice.isActionable()) {
                TrackEvent ev;
                ev.trackId     = idx;
                ev.timestampUs = telem.timestamp;
                ev.value       = gainAdvice.currentPeak;
                ev.threshold   = gainAdvice.peakTarget;
                ev.deviation   = gainAdvice.peakDeviation;
                ev.context     = "gain";
                int64_t nowGc  = telem.timestamp > 0 ? telem.timestamp : juce::Time::getMillisecondCounter() * 1000;
                if (!trackFeedCore_->checkAndSetGainCooldown(idx, nowGc, kGainAdviceCooldownUs)) return;
                ev.type     = (gainAdvice.currentPeak > -0.5f)    ? TrackEventType::ClippingDetected
                              : (gainAdvice.currentPeak > -6.0f)  ? TrackEventType::LevelSpike
                              : (gainAdvice.currentPeak < -30.0f) ? TrackEventType::LowSignal
                                                                  : TrackEventType::Info;
                ev.severity = (gainAdvice.currentPeak > -0.5f)    ? 0.9f
                              : (gainAdvice.currentPeak > -6.0f)  ? 0.6f
                              : (gainAdvice.currentPeak < -30.0f) ? 0.4f
                                                                  : 0.3f;
                ev.message  = gainAdvice.message;
                trackFeedCore_->pushTrackEvent(idx, ev);
                if (gainAdvice.currentPeak > -0.5f) trackFeedCore_->updateTrackHealth(idx, TrackHealth::ClippingRisk);
                else if (gainAdvice.currentPeak < -30.0f)
                    trackFeedCore_->updateTrackHealth(idx, TrackHealth::LowSignal);
            }

            // Sprint 6C: Role-aware tonal analysis
            auto tonalAdvice = analyzeTrackTonal(idx);
            if (tonalAdvice.isActionable()) {
                TrackEvent ev;
                ev.trackId     = idx;
                ev.timestampUs = telem.timestamp;
                ev.value       = tonalAdvice.worstDeviation;
                ev.threshold   = TrackTonalAdvice::kToleranceDb;
                ev.deviation   = tonalAdvice.worstDeviation;
                ev.context     = "tonal";
                int64_t nowTc  = telem.timestamp > 0 ? telem.timestamp : juce::Time::getMillisecondCounter() * 1000;
                if (!trackFeedCore_->checkAndSetTonalCooldown(idx, nowTc, kTonalAdviceCooldownUs)) return;
                ev.type     = TrackEventType::SpectralImbalance;
                ev.severity = (tonalAdvice.status == TrackTonalAdvice::Status::OffTarget) ? 0.7f : 0.3f;
                ev.message  = tonalAdvice.message;
                trackFeedCore_->pushTrackEvent(idx, ev);
                if (tonalAdvice.status == TrackTonalAdvice::Status::OffTarget)
                    trackFeedCore_->updateTrackHealth(idx, TrackHealth::NeedsEQ);
            }

            // Sprint 8: Role-aware phase analysis
            auto phaseAdvice = analyzeTrackPhase(idx);
            if (phaseAdvice.isActionable()) {
                TrackEvent ev;
                ev.trackId     = idx;
                ev.timestampUs = telem.timestamp;
                ev.value       = phaseAdvice.currentCorrelation;
                ev.threshold   = 0.3f;
                ev.deviation   = phaseAdvice.correlationDeviation;
                ev.context     = "phase";
                int64_t nowPh  = telem.timestamp > 0 ? telem.timestamp : juce::Time::getMillisecondCounter() * 1000;
                if (!trackFeedCore_->checkAndSetPhaseCooldown(idx, nowPh, kPhaseAdviceCooldownUs)) return;
                ev.type     = TrackEventType::PhaseIssue;
                ev.severity = (phaseAdvice.status == TrackPhaseAdvice::Status::OffTarget) ? 0.8f : 0.4f;
                ev.message  = phaseAdvice.message;
                trackFeedCore_->pushTrackEvent(idx, ev);
                if (phaseAdvice.status == TrackPhaseAdvice::Status::OffTarget)
                    trackFeedCore_->updateTrackHealth(idx, TrackHealth::PhaseIssue);
            }
        });
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  inferBySilenceOrder
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::inferBySilenceOrder()
    {
        if (signalOrderCount_ == 0) return;
        for (int pos = 0; pos < signalOrderCount_ && pos < kSignalOrderRolesCount; ++pos) {
            int idx = signalOrderIndices_[pos];
            if (idx < 0 || idx >= SlotRegistry::kMaxSlots) continue;
            if (trackRoles_[idx] != TrackRole::Unknown) continue;

            auto info              = sharedData_.getSlotRegistry().getSlotInfo(idx);
            juce::String trackName = juce::String(info.trackName).trim();
            if (trackName.isNotEmpty() && !trackName.startsWithIgnoreCase("Pista")
                && !trackName.startsWithIgnoreCase("Track") && !trackName.startsWithIgnoreCase("Channel"))
                continue;

            TrackRole inferredRole     = kSignalOrderRoles[pos];
            trackRoles_[idx]           = inferredRole;
            trackRoleWasInferred_[idx] = true;
            signalOrderApplied_[idx]   = true;
            LogHelper::writeToLog("[SignalOrder] Slot " + juce::String(idx) + " inferido como "
                                  + juce::String(getRoleName(inferredRole)) + " (posicion " + juce::String(pos) + ")");

            TrackType mappedType = getTrackTypeForRole(inferredRole);
            sharedData_.getSlotRegistry().updateSlotTrackType(idx, (int)mappedType);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  TrackType ↔ TrackRole mapping
    // ═══════════════════════════════════════════════════════════════════════════

    TrackRole getTrackRoleForTrackType(TrackType type) noexcept
    {
        switch (type) {
            case TrackType::Kick:
                return TrackRole::Kick;
            case TrackType::ReggaetonKick:
                return TrackRole::ReggaetonKick;
            case TrackType::Snare:
                return TrackRole::Snare;
            case TrackType::HiHat:
                return TrackRole::HiHat;
            case TrackType::Tom:
                return TrackRole::Tom;
            case TrackType::Percussion:
                return TrackRole::Percussion;
            case TrackType::Overheads:
                return TrackRole::DrumBus;
            case TrackType::Room:
                return TrackRole::DrumRoom;
            case TrackType::BassDI:
                return TrackRole::BassFinger;
            case TrackType::BassMic:
                return TrackRole::BassPick;
            case TrackType::Bass808:
                return TrackRole::Bass808;
            case TrackType::Sub:
                return TrackRole::BassSub;
            case TrackType::Piano:
                return TrackRole::KeysPiano;
            case TrackType::Guitar:
                return TrackRole::GuitarElectric;
            case TrackType::SynthLead:
                return TrackRole::SynthLead;
            case TrackType::SynthPad:
                return TrackRole::SynthPad;
            case TrackType::Strings:
                return TrackRole::Strings;
            case TrackType::LeadVocal:
                return TrackRole::VozPrincipal;
            case TrackType::DoubleVocal:
                return TrackRole::VozFondo;
            case TrackType::Adlibs:
                return TrackRole::Adlibs;
            case TrackType::Chorus:
                return TrackRole::VozFondo;
            case TrackType::Risers:
                return TrackRole::FxRiser;
            case TrackType::Impacts:
                return TrackRole::FxImpact;
            case TrackType::Ambience:
                return TrackRole::FxAmbience;
            default:
                return TrackRole::Unknown;
        }
    }

    TrackType getTrackTypeForRole(TrackRole role) noexcept
    {
        switch (role) {
            case TrackRole::Kick:
                return TrackType::Kick;
            case TrackRole::ReggaetonKick:
                return TrackType::ReggaetonKick;
            case TrackRole::Snare:
                return TrackType::Snare;
            case TrackRole::HiHat:
                return TrackType::HiHat;
            case TrackRole::Tom:
                return TrackType::Tom;
            case TrackRole::Bass808:
                return TrackType::Bass808;
            case TrackRole::BassSub:
                return TrackType::Sub;
            case TrackRole::SynthLead:
                return TrackType::SynthLead;
            case TrackRole::SynthPad:
                return TrackType::SynthPad;
            case TrackRole::Strings:
                return TrackType::Strings;
            case TrackRole::VozPrincipal:
                return TrackType::LeadVocal;
            case TrackRole::Adlibs:
                return TrackType::Adlibs;
            case TrackRole::FxRiser:
                return TrackType::Risers;
            case TrackRole::FxImpact:
                return TrackType::Impacts;
            case TrackRole::FxAmbience:
                return TrackType::Ambience;
            case TrackRole::Kick808:
                return TrackType::Kick;
            case TrackRole::SnareTrap:
                return TrackType::Snare;
            case TrackRole::HiHatOpen:
                return TrackType::HiHat;
            case TrackRole::TomFloor:
                return TrackType::Tom;
            case TrackRole::Ride:
            case TrackRole::Crash:
            case TrackRole::Clap:
            case TrackRole::Percussion:
                return TrackType::Percussion;
            case TrackRole::DrumBus:
                return TrackType::Overheads;
            case TrackRole::DrumRoom:
                return TrackType::Room;
            case TrackRole::BassFinger:
                return TrackType::BassDI;
            case TrackRole::BassPick:
                return TrackType::BassMic;
            case TrackRole::BassSynth:
            case TrackRole::BassBus:
                return TrackType::BassDI;
            case TrackRole::GuitarAcoustic:
            case TrackRole::GuitarElectric:
            case TrackRole::GuitarRhythm:
            case TrackRole::GuitarLead:
            case TrackRole::GuitarBus:
                return TrackType::Guitar;
            case TrackRole::KeysPiano:
            case TrackRole::KeysElectric:
            case TrackRole::KeysOrgan:
            case TrackRole::KeysBus:
            case TrackRole::MelodyBus:
                return TrackType::Piano;
            case TrackRole::SynthPluck:
                return TrackType::SynthLead;
            case TrackRole::VozFondo:
            case TrackRole::VozDouble:
            case TrackRole::VozBus:
                return TrackType::DoubleVocal;
            case TrackRole::FxNoise:
                return TrackType::Ambience;
            case TrackRole::Master:
            case TrackRole::Winds:
            default:
                return TrackType::None;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  INFERENCIA DE ROL POR NOMBRE
    // ═══════════════════════════════════════════════════════════════════════════

    CoachEngine::NameInferenceResult CoachEngine::inferTrackRoleFromName(const juce::String& trackName) noexcept
    {
        NameInferenceResult result;
        auto kw = NameInferrer::detectKeywords(trackName);
        if (kw.isEmpty) return result;

        auto& k = kw;

        if (k.has808Kick) {
            result = {TrackRole::Kick808, 0.95f, true};
            return result;
        }
        if (k.hasKick) {
            result = {TrackRole::Kick, 0.90f, true};
            return result;
        }
        if (k.has808Bass || (k.has808 && k.hasSub)) {
            result = {TrackRole::Bass808, 0.90f, true};
            return result;
        }
        if (k.has808 && !k.hasDrum) {
            result = {TrackRole::Bass808, 0.75f, true};
            return result;
        }
        if (k.hasOpenHH) {
            result = {TrackRole::HiHatOpen, 0.90f, true};
            return result;
        }
        if (k.hasHiHat) {
            result = {TrackRole::HiHat, 0.90f, true};
            return result;
        }
        if (k.hasSnare && k.hasTrap) {
            result = {TrackRole::SnareTrap, 0.90f, true};
            return result;
        }
        if (k.hasSnare) {
            result = {TrackRole::Snare, 0.90f, true};
            return result;
        }
        if (k.hasRim) {
            result = {TrackRole::Snare, 0.80f, true};
            return result;
        }
        if (k.hasTom
            && (k.normalizedName.contains("floor") || k.normalizedName.contains("piso")
                || k.normalizedName.contains("suelo"))) {
            result = {TrackRole::TomFloor, 0.90f, true};
            return result;
        }
        if (k.hasTom) {
            result = {TrackRole::Tom, 0.85f, true};
            return result;
        }
        if (k.hasReggaeton && k.hasKick) {
            result = {TrackRole::ReggaetonKick, 0.85f, true};
            return result;
        }
        if (k.hasReggaeton && k.hasDrum) {
            result = {TrackRole::ReggaetonKick, 0.70f, true};
            return result;
        }
        if (k.hasClap) {
            result = {TrackRole::Clap, 0.90f, true};
            return result;
        }
        if (k.hasRide) {
            result = {TrackRole::Ride, 0.85f, true};
            return result;
        }
        if (k.hasCrash) {
            result = {TrackRole::Crash, 0.85f, true};
            return result;
        }
        if (k.hasSub && k.hasBass) {
            result = {TrackRole::BassSub, 0.85f, true};
            return result;
        }
        if (k.hasFinger && k.hasBass) {
            result = {TrackRole::BassFinger, 0.85f, true};
            return result;
        }
        if (k.hasPick && k.hasBass) {
            result = {TrackRole::BassPick, 0.85f, true};
            return result;
        }
        if (k.hasSynth && k.hasBass) {
            result = {TrackRole::BassSynth, 0.80f, true};
            return result;
        }
        if (k.hasAcoustic && k.hasGuitar) {
            result = {TrackRole::GuitarAcoustic, 0.90f, true};
            return result;
        }
        if (k.hasLead && k.hasGuitar) {
            result = {TrackRole::GuitarLead, 0.85f, true};
            return result;
        }
        if (k.hasRhythm && k.hasGuitar) {
            result = {TrackRole::GuitarRhythm, 0.85f, true};
            return result;
        }
        if (k.hasGuitar) {
            result = {TrackRole::GuitarElectric, 0.75f, true};
            return result;
        }
        if (k.hasVozPrincipal) {
            result = {TrackRole::VozPrincipal, 0.90f, true};
            return result;
        }
        if (k.hasVozFondo) {
            result = {TrackRole::VozFondo, 0.85f, true};
            return result;
        }
        if (k.hasAdlib) {
            result = {TrackRole::Adlibs, 0.80f, true};
            return result;
        }
        if (k.hasVocal) {
            result = {TrackRole::VozPrincipal, 0.70f, true};
            return result;
        }
        if (k.hasPad) {
            result = {TrackRole::SynthPad, 0.85f, true};
            return result;
        }
        if (k.hasPluck) {
            result = {TrackRole::SynthPluck, 0.85f, true};
            return result;
        }
        if (k.hasLead && k.hasSynth) {
            result = {TrackRole::SynthLead, 0.85f, true};
            return result;
        }
        if (k.hasOrgan) {
            result = {TrackRole::KeysOrgan, 0.85f, true};
            return result;
        }
        if (k.hasPiano) {
            result = {TrackRole::KeysPiano, 0.85f, true};
            return result;
        }
        if (k.hasSynth) {
            result = {TrackRole::SynthLead, 0.70f, true};
            return result;
        }
        if (k.hasStrings) {
            result = {TrackRole::Strings, 0.85f, true};
            return result;
        }
        if (k.hasBrass) {
            result = {TrackRole::Brass, 0.85f, true};
            return result;
        }
        if (k.hasPerc) {
            result = {TrackRole::Percussion, 0.75f, true};
            return result;
        }
        if (k.hasRiser) {
            result = {TrackRole::FxRiser, 0.85f, true};
            return result;
        }
        if (k.hasAmbient) {
            result = {TrackRole::FxAmbience, 0.80f, true};
            return result;
        }
        if (k.hasNoise) {
            result = {TrackRole::FxNoise, 0.80f, true};
            return result;
        }
        if (k.hasFx) {
            result = {TrackRole::FxAmbience, 0.65f, true};
            return result;
        }
        if (k.hasDrum && k.hasBus) {
            result = {TrackRole::DrumBus, 0.90f, true};
            return result;
        }
        if (k.hasBus && k.hasBass) {
            result = {TrackRole::BassBus, 0.80f, true};
            return result;
        }
        if (k.hasMaster) {
            result = {TrackRole::Master, 0.95f, true};
            return result;
        }
        if (k.hasWinds) {
            result = {TrackRole::Winds, 0.75f, true};
            return result;
        }
        if (k.hasViolin) {
            result = {TrackRole::Strings, 0.65f, true};
            return result;
        }
        if (k.hasSample) {
            result = {TrackRole::FxAmbience, 0.45f, true};
            return result;
        }
        if (k.hasIntro) {
            result = {TrackRole::FxRiser, 0.60f, true};
            return result;
        }
        if (k.hasFill) {
            result = {TrackRole::Percussion, 0.55f, true};
            return result;
        }
        if (k.hasMelody || k.hasArp) {
            result = {TrackRole::SynthLead, 0.50f, true};
            return result;
        }
        if (k.hasClick) {
            result = {TrackRole::Percussion, 0.50f, true};
            return result;
        }
        if (k.hasRoomMic) {
            result = {TrackRole::DrumRoom, 0.85f, true};
            return result;
        }
        if (k.hasRoom) {
            result = {TrackRole::FxAmbience, 0.60f, true};
            return result;
        }

        result.role       = TrackRole::Unknown;
        result.confidence = 0.0f;
        result.fromName   = false;
        return result;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  inferTrackRoleCombined
    // ═══════════════════════════════════════════════════════════════════════════

    CoachEngine::NameInferenceResult CoachEngine::inferTrackRoleCombined(const juce::String& trackName,
                                                                         const TrackSpectralProfile& spectral) noexcept
    {
        auto nameResult = inferTrackRoleFromName(trackName);
        if (nameResult.isValid() && nameResult.confidence >= 0.70f) return nameResult;

        if (spectral.hasData()) {
            TrackRole spectralRole = SpectralProfiler::inferTrackRole(spectral);
            if (spectralRole != TrackRole::Unknown) {
                if (nameResult.isValid() && nameResult.confidence >= 0.40f && nameResult.role == spectralRole)
                    return {spectralRole, 0.80f, nameResult.fromName};
                return {spectralRole, 0.65f, false};
            }
        }
        return nameResult;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  inferTrackRoles
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::inferTrackRoles()
    {
        auto& registry = sharedData_.getSlotRegistry();
        int rolesInf = 0, nameInf = 0, spectralInf = 0, explicitInf = 0;

        detectFirstSignal();
        inferBySilenceOrder();

        registry.forEachActive([&](const SlotInfo& info) {
            int idx = info.slotIndex;
            if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;
            if (trackRoles_[idx] != TrackRole::Unknown && trackRoles_[idx] != TrackRole::Master) return;

            juce::String trackName = juce::String(info.trackName).trim();

            if (info.trackType >= 0 && info.trackType != (int)TrackType::None) {
                TrackRole explicitRole = getTrackRoleForTrackType((TrackType)info.trackType);
                if (explicitRole != TrackRole::Unknown && explicitRole != TrackRole::Master) {
                    trackRoles_[idx]           = explicitRole;
                    trackRoleWasInferred_[idx] = true;
                    trackRoleConfirmed_[idx]   = false;
                    rolesInf++;
                    explicitInf++;
                    return;
                }
            }

            TrackSpectralProfile spectral;
            auto telem = getLatestTelemetry(idx);
            if (telem.timestamp > 0) spectral = SpectralProfiler::computeProfile(telem);

            auto combined = inferTrackRoleCombined(trackName, spectral);
            if (combined.isValid()) {
                trackRoles_[idx]           = combined.role;
                trackRoleWasInferred_[idx] = true;
                trackRoleConfirmed_[idx]   = false;
                rolesInf++;
                if (combined.fromName) nameInf++;
                else
                    spectralInf++;
                if (combined.fromName && combined.confidence >= 0.7f) {
                    TrackType mappedType = getTrackTypeForRole(combined.role);
                    if (mappedType != TrackType::None) registry.updateSlotTrackType(idx, (int)mappedType);
                }
            }
        });

        if (rolesInf > 0) {
            LogHelper::writeToLog("[IdentityLayer] Roles inferidos: " + juce::String(rolesInf));
            showIdentitySummary();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  showIdentitySummary
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::showIdentitySummary()
    {
        auto& registry = sharedData_.getSlotRegistry();
        int d = 0, b = 0, g = 0, k = 0, v = 0, fx = 0, m = 0, u = 0;
        int totalActive = 0, identified = 0, inferred = 0, unknown = 0;
        juce::String identifiedLines, unknownLines;

        registry.forEachActive([&](const SlotInfo& info) {
            if (info.muted || (soloActive_ && !info.soloed)) return;
            int idx = info.slotIndex;
            if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;
            auto role         = trackRoles_[idx];
            auto cat          = getRoleCategory(role);
            juce::String name = juce::String(info.trackName).trim();
            if (name.isEmpty()) name = "Pista " + juce::String(idx + 1);

            switch (cat) {
                case RoleCategory::Drums:
                    d++;
                    break;
                case RoleCategory::Bass:
                    b++;
                    break;
                case RoleCategory::Guitars:
                    g++;
                    break;
                case RoleCategory::Keys:
                    k++;
                    break;
                case RoleCategory::Vocals:
                    v++;
                    break;
                case RoleCategory::FX:
                    fx++;
                    break;
                case RoleCategory::Melody:
                    m++;
                    break;
                default:
                    u++;
                    break;
            }
            totalActive++;
            if (role != TrackRole::Unknown && role != TrackRole::Master) {
                identified++;
                if (trackRoleWasInferred_[idx] && !trackRoleConfirmed_[idx]) inferred++;
                juce::String statusEmoji = isRoleConfirmed(idx) ? "\xE2\x9C\x85" : "\xE2\x9A\xA1";
                juce::String busStr;
                if (info.bus >= BusType::Drums && info.bus <= BusType::Melody)
                    busStr = getBusIcon(info.bus) + " " + juce::String(busNames[static_cast<int>(info.bus)]);
                else
                    busStr = "Sin bus";
                identifiedLines += "  " + statusEmoji + " " + name + " \xE2\x86\x92 " + juce::String(getRoleName(role))
                                   + " (" + busStr + ")\n";
            }
            else if (role == TrackRole::Unknown) {
                unknown++;
                unknownLines += "  \xE2\x9A\xA0\xEF\xB8\x8F \xE2\x80\x9C" + name
                                + "\xE2\x80\x9D \xE2\x86\x92 \xC2\xBFQu\xC3\xA9 instrumento es?\n";
            }
        });

        juce::String msg;
        msg += "\xF0\x9F\x93\xA1 **MESSENGER ACTIVO \xE2\x80\x94 PISTAS IDENTIFICADAS**\n\n";
        msg += "\xF0\x9F\x8E\xA7 **Total: " + juce::String(totalActive) + " pistas detectadas**\n\n";
        if (identified > 0)
            msg += "\xE2\x9C\x85 **Identificadas (" + juce::String(identified) + ")**\n" + identifiedLines + "\n";
        if (unknown > 0)
            msg +=
                "\xE2\x9A\xA0\xEF\xB8\x8F **Sin identificar (" + juce::String(unknown) + ")**\n" + unknownLines + "\n";

        juce::String famLine;
        if (d > 0)
            famLine +=
                "\xF0\x9F\xA5\x81 Bater\xC3\xAD"
                "a: "
                + juce::String(d) + " | ";
        if (b > 0) famLine += "\xF0\x9F\x8E\xB8 Bajo: " + juce::String(b) + " | ";
        if (g > 0) famLine += "\xF0\x9F\x8E\xB8 Guitarras: " + juce::String(g) + " | ";
        if (k > 0) famLine += "\xF0\x9F\x8E\xB9 Teclados: " + juce::String(k) + " | ";
        if (v > 0) famLine += "\xF0\x9F\x8E\xA4 Voces: " + juce::String(v) + " | ";
        if (fx > 0) famLine += "\xF0\x9F\x9B\x9B FX: " + juce::String(fx) + " | ";
        if (m > 0)
            famLine +=
                "\xF0\x9F\x8E\xB5 Mel\xC3\xB3"
                "dicos: "
                + juce::String(m) + " | ";
        if (u > 0) famLine += "\xF0\x9F\x93\xA1 Otros: " + juce::String(u) + " | ";
        if (famLine.isNotEmpty())
            msg += "\xF0\x9F\x93\x8A **Resumen por familias:**\n  " + famLine.substring(0, famLine.length() - 3) + "\n";
        if (inferred > 0 && unknown == 0)
            msg += "\n\xE2\x9A\xA1 **" + juce::String(inferred) + " pista(s) pendiente(s) de confirmaci\xC3\xB3n.**\n   Usa el bot\xC3\xB3n **Confirmar todo** o haz clic en \xE2\x9A\xA1.";
        else if (unknown > 0)
            msg += "\n\xF0\x9F\x92\xA1 **Asigna un rol a cada pista sin identificar** desde el panel Messenger.";
        else if (identified > 0 && unknown == 0 && inferred == 0)
            msg += "\n\xE2\x9C\x85 **Todas las pistas identificadas y confirmadas.** \xC2\xA1Listo para avanzar!";
        respondWith(msg, MentorMessage::Type::Info);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  confirmAllInferredRoles
    // ═══════════════════════════════════════════════════════════════════════════

    int CoachEngine::confirmAllInferredRoles() noexcept
    {
        auto& registry     = sharedData_.getSlotRegistry();
        int confirmedCount = 0;
        registry.forEachActive([&](const SlotInfo& info) {
            int idx = info.slotIndex;
            if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;
            if (isInferredRole(idx)) {
                confirmRole(idx);
                confirmedCount++;
            }
        });

        if (confirmedCount > 0) {
            LogHelper::writeToLog("[Sprint1] Roles confirmados: " + juce::String(confirmedCount));
            juce::String detailLines;
            registry.forEachActive([&](const SlotInfo& info) {
                if (info.muted || (soloActive_ && !info.soloed)) return;
                int idx = info.slotIndex;
                if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;
                auto role = trackRoles_[idx];
                if (role == TrackRole::Unknown || role == TrackRole::Master) return;
                juce::String name = juce::String(info.trackName).trim();
                if (name.isEmpty()) name = "Pista " + juce::String(idx + 1);
                juce::String busStr;
                if (info.bus >= BusType::Drums && info.bus <= BusType::Melody)
                    busStr =
                        " (" + getBusIcon(info.bus) + " " + juce::String(busNames[static_cast<int>(info.bus)]) + ")";
                detailLines +=
                    "  \xE2\x9C\x85 " + name + " \xE2\x86\x92 " + juce::String(getRoleName(role)) + busStr + "\n";
            });

            auto currentPhase = phaseManager_.getCurrentPhase();
            if (currentPhase == MentorPhase::Organizacion) {
                phaseManager_.advanceToNextPhase();
                respondWith("\xE2\x9C\x85 **" + juce::String(confirmedCount) + " roles confirmados!**\n\n" + detailLines
                                + "\n\xF0\x9F\x9A\x00 Todos los roles confirmados! Avanzamos a **"
                                + juce::String(phaseNames[static_cast<int>(phaseManager_.getCurrentPhase())]) + "**.",
                            MentorMessage::Type::Info);
                sendPhaseGuidance(phaseManager_.getCurrentPhase());
            }
            else {
                respondWith("\xE2\x9C\x85 **" + juce::String(confirmedCount) + " roles confirmados**\n\n" + detailLines
                                + "\n\xE2\x9C\x85 Sesi\xC3\xB3n completamente identificada.",
                            MentorMessage::Type::Info);
            }
        }
        else {
            int unknownCount = 0;
            registry.forEachActive([&](const SlotInfo& info) {
                if (info.muted || (soloActive_ && !info.soloed)) return;
                int idx = info.slotIndex;
                if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;
                if (trackRoles_[idx] == TrackRole::Unknown) unknownCount++;
            });
            if (unknownCount > 0)
                respondWith("\xE2\x9A\xA0\xEF\xB8\x8F No hay roles pendientes, pero **" + juce::String(unknownCount)
                                + " pista(s)** a\xC3\xBAn sin rol.",
                            MentorMessage::Type::Info);
            else
                respondWith("\xE2\x9C\x85 Todos los roles ya est\xC3\xA1n confirmados.", MentorMessage::Type::Info);
        }
        return confirmedCount;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getIdentityProgress
    // ═══════════════════════════════════════════════════════════════════════════

    IdentityProgress CoachEngine::getIdentityProgress() const noexcept
    {
        IdentityProgress prog;
        const auto& registry = sharedData_.getSlotRegistry();
        registry.forEachActive([&](const SlotInfo& info) {
            int idx = info.slotIndex;
            if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;
            prog.totalActive++;
            auto role = trackRoles_[idx];
            if (role != TrackRole::Unknown && role != TrackRole::Master) {
                prog.identified++;
                if (isRoleConfirmed(idx)) prog.confirmed++;
                else
                    prog.pendingInferred++;
            }
        });
        return prog;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setTrackRoleWithLearning
    // ═══════════════════════════════════════════════════════════════════════════

    void CoachEngine::setTrackRoleWithLearning(int slotIndex, TrackRole role) noexcept
    {
        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) return;
        signalOrderConfirmed_[slotIndex] = true;
        TrackRole oldRole                = trackRoles_[slotIndex];
        if (oldRole == role) return;

        if (trackRoleWasInferred_[slotIndex]) {
            bool inferredFromName = false;
            auto telem            = getLatestTelemetry(slotIndex);
            TrackSpectralProfile spectral;
            if (telem.timestamp > 0) spectral = SpectralProfiler::computeProfile(telem);
            correctionLearner_.recordCorrection(
                oldRole, role, inferredFromName, lastInferenceKeywords_[slotIndex], spectral);
            LogHelper::writeToLog("[CoachEngine] Aprendiendo correccion: Slot " + juce::String(slotIndex) + " "
                                  + juce::String(getRoleName(oldRole)) + " -> " + juce::String(getRoleName(role)));
        }

        trackRoles_[slotIndex]           = role;
        trackRoleWasInferred_[slotIndex] = false;
        trackRoleConfirmed_[slotIndex]   = true;
        lastInferenceKeywords_[slotIndex].clear();

        TrackType mappedType = getTrackTypeForRole(role);
        if (mappedType != TrackType::None)
            sharedData_.getSlotRegistry().updateSlotTrackType(slotIndex, (int)mappedType);
    }

} // namespace mixcoach
