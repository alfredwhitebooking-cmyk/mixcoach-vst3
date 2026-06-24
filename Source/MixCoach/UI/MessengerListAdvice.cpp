#include "MessengerListComponent.h"
#include "MessengerListAdvice.h"
#include "../engine/CoachEngine.h"
#include <cmath>

namespace mixcoach {

    // ─── Static helpers ═══════════════════════════════════════════════════════

    juce::String getBusEmoji(BusType bus) noexcept
    {
        switch (bus) {
            case BusType::Drums:
                return "\xF0\x9F\xA5\x81";
            case BusType::Bass:
                return "\xF0\x9F\x8E\xB8";
            case BusType::Guitars:
                return "\xF0\x9F\x8E\xB8";
            case BusType::Keys:
                return "\xF0\x9F\x8E\xB9";
            case BusType::Vocals:
                return "\xF0\x9F\x8E\xA4";
            case BusType::FX:
                return "\xF0\x9F\x8E\x9B";
            case BusType::Melody:
                return "\xF0\x9F\x8E\xB5";
            default:
                return "\xF0\x9F\x93\x8D";
        }
    }

    TrackSuggestion analyzeTrackSuggestion(float peakDb, float rmsDb, bool hasSignal, const SlotInfo& info)
    {
        TrackSuggestion result;
        juce::String busEmoji = (info.bus != BusType::None) ? getBusEmoji(info.bus) + " " : juce::String();

        if (!hasSignal || peakDb < -60.0f) {
            result.text = busEmoji + "\xE2\x9A\xAA Sin se" "\xC3\xB1" "al — " "\xC2\xBF" "el Messenger recibe audio?";
            result.status = SuggestionStatus::White;
            return result;
        }
        if (peakDb > -0.5f) {
            float reduceBy = peakDb + 6.0f;
            result.text    = busEmoji + "\xF0\x9F\x94\xB4 Recortando a " + juce::String(peakDb, 1) + "dB! Baja "
                             + juce::String(reduceBy, 1) + " dB YA";
            result.status  = SuggestionStatus::Red;
            return result;
        }
        if (peakDb > -3.0f) {
            float reduceBy = peakDb + 6.0f;
            result.text   = busEmoji + "\xF0\x9F\x9F\xA1 Pico alto " + juce::String(peakDb, 1) + "dB \xE2\x80\x94 baja "
                            + juce::String(reduceBy, 1) + " dB (target: -6dB)";
            result.status = SuggestionStatus::Yellow;
            return result;
        }

        // RMS analysis
        if (rmsDb > -4.0f) {
            float reduceBy = rmsDb + 6.0f;
            result.text    = busEmoji + "\xF0\x9F\x94\xB4 RMS " + juce::String(rmsDb, 1)
                             + "dB muy caliente \xE2\x80\x94 baja " + juce::String(reduceBy, 1) + " dB";
            result.status  = SuggestionStatus::Red;
            return result;
        }
        if (rmsDb > -6.0f) {
            result.text = busEmoji + "\xF0\x9F\x9F\xA1 RMS " + juce::String(rmsDb, 1)
                      + "dB \xE2\x80\x94 cerca del l" "\xC3\xAD" "mite, vigila";
            result.status = SuggestionStatus::Yellow;
            return result;
        }
        if (rmsDb > -10.0f) {
            result.text = busEmoji + "\xF0\x9F\x9F\xA2 RMS " + juce::String(rmsDb, 1)
                      + "dB \xE2\x80\x94 nivel " "\xC3\xB3" "ptimo \xE2\x9C\x85";
            result.status = SuggestionStatus::Green;
            return result;
        }
        if (rmsDb > -18.0f) {
            float boostBy = -8.0f - rmsDb;
            result.text   = busEmoji + "\xF0\x9F\x9F\xA2 RMS " + juce::String(rmsDb, 1) + "dB \xE2\x80\x94 sube "
                            + juce::String(boostBy, 1) + " dB (target: -8dB)";
            result.status = SuggestionStatus::Green;
            return result;
        }
        result.text = busEmoji + "\xE2\x9A\xAA RMS " + juce::String(rmsDb, 1)
                  + "dB \xE2\x80\x94 se" "\xC3\xB1" "al muy baja, sube ganancia";
        result.status = SuggestionStatus::White;
        return result;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateCoachAdvice — Member of MessengerListComponent
    // ═══════════════════════════════════════════════════════════════════════════
    void MessengerListComponent::updateCoachAdvice(CoachEngine& coach)
    {
        for (int idx = 0; idx < SlotRegistry::kMaxSlots; ++idx) {
            auto& entry = messengers_[idx];
            if (!entry.info.active) continue;

            BusType bus = entry.info.bus;
            switch (bus) {
                case BusType::Drums:
                    entry.busTargetPeak = -8.0f;
                    break;
                case BusType::Bass:
                    entry.busTargetPeak = -10.0f;
                    break;
                case BusType::Guitars:
                    entry.busTargetPeak = -8.0f;
                    break;
                case BusType::Keys:
                    entry.busTargetPeak = -10.0f;
                    break;
                case BusType::Vocals:
                    entry.busTargetPeak = -6.0f;
                    break;
                case BusType::FX:
                    entry.busTargetPeak = -14.0f;
                    break;
                case BusType::Melody:
                    entry.busTargetPeak = -8.0f;
                    break;
                default:
                    entry.busTargetPeak = -8.0f;
                    break;
            }

            // Recommendation active
            const auto* rec = coach.getTrackRecommendation(idx);
            if (rec != nullptr && rec->slotIndex == idx) {
                float delta            = std::abs(rec->delta);
                float change           = std::abs(rec->beforeValue - rec->expectedAfter);
                juce::String direction = (rec->delta < 0) ? "Baja" : "Sube";
                juce::String emoji     = (entry.info.bus != BusType::None) ? getBusEmoji(entry.info.bus) + " "
                                                                           : juce::String();

                switch (rec->status) {
                    case TrackRecommendation::Status::Pending:
                        entry.coachAdviceStatus = SuggestionStatus::Yellow;
                        entry.coachAdviceText = emoji + (rec->delta < -3.0f ? "\xF0\x9F\x94\xB4 " : "\xF0\x9F\x9F\xA1 ")
                                                + direction + " " + juce::String(delta, 1) + " dB (PK "
                                                + juce::String(rec->beforeValue, 1) + "dB)";
                        break;
                    case TrackRecommendation::Status::Applied:
                        entry.coachAdviceStatus = SuggestionStatus::Green;
                        entry.coachAdviceText   = emoji + "\xE2\x9C\x85 Corregido! " + juce::String(rec->beforeValue, 1)
                                                  + " \xE2\x86\x92 " + juce::String(rec->expectedAfter, 1) + " dB";
                        break;
                    case TrackRecommendation::Status::Ignored:
                    case TrackRecommendation::Status::Superseded:
                        entry.coachAdviceStatus = SuggestionStatus::White;
                        entry.coachAdviceText   = emoji + "\xE2\x8F\xB3 Sin cambios \xE2\x80\x94 aplicar?";
                        break;
                    case TrackRecommendation::Status::OverApplied:
                        entry.coachAdviceStatus = SuggestionStatus::Red;
                        entry.coachAdviceText = emoji + "\xE2\x9A\xA0 Exceso! Bajaste " + juce::String(delta, 1)
                        + " dB (" + juce::String(delta - change, 1) + " dB de m" "\xC3\xA1s" ")";
                        break;
                    case TrackRecommendation::Status::UnderApplied:
                        entry.coachAdviceStatus = SuggestionStatus::Red;
                        entry.coachAdviceText = emoji + "\xE2\x9A\xA0 Faltan " + juce::String(delta - change, 1)
                        + " dB \xE2\x80\x94 ajusta m" "\xC3\xA1s";
                        break;
                    default:
                        entry.coachAdviceText   = entry.aiSuggestion;
                        entry.coachAdviceStatus = entry.suggestionStatus;
                        break;
                }
            }
            // ─── Sprint 6: Track Intelligence role-aware (0 tokens, C++ puro) ───
            // Si la pista tiene rol asignado, usamos los advices específicos con
            // targets del ExpectedProfile en lugar del TrackHealth genérico.
            // Prioridad: Gain (afecta el balance completo) > Dynamics > Tonal.
            else if (coachEngine_ != nullptr && entry.trackRole != TrackRole::Unknown) {
                juce::String emoji = (entry.info.bus != BusType::None) ? getBusEmoji(entry.info.bus) + " "
                                                                       : juce::String();

                CoachEngine::TrackGainAdvice gainAdv    = coachEngine_->analyzeTrackGain(idx);
                CoachEngine::TrackDynamicsAdvice dynAdv = coachEngine_->analyzeTrackDynamics(idx);
                CoachEngine::TrackTonalAdvice tonalAdv  = coachEngine_->analyzeTrackTonal(idx);

                // ClippingRisk siempre tiene prioridad máxima (preserva seguridad)
                auto& feed              = coachEngine_->getTrackFeedCore();
                TrackState ts           = feed.getTrackState(idx);
                const bool clippingRisk = ts.hasSignal()
                                          && (ts.health == TrackHealth::ClippingRisk || gainAdv.currentPeak > -0.5f);

                if (clippingRisk) {
                    entry.coachAdviceStatus = SuggestionStatus::Red;
                    float pk                = std::max(gainAdv.currentPeak, ts.getPeakCombinedDb());
                    entry.coachAdviceText =
                        emoji + "\xF0\x9F\x94\xB4 Pico " + juce::String(pk, 1) + "dB \xE2\x80\x94 baja YA";
                }
                else if (gainAdv.isActionable()) {
                    // Sprint 6A: Gain Intelligence con target por rol
                    bool off                = (gainAdv.status == CoachEngine::TrackGainAdvice::Status::OffTarget);
                    entry.coachAdviceStatus = off ? SuggestionStatus::Red : SuggestionStatus::Yellow;
                    juce::String sevEmoji = off ? juce::String("\xF0\x9F\x94\xB4 ") : juce::String("\xF0\x9F\x9F\xA1 ");
                    juce::String arrow    = (gainAdv.suggestedDeltaDb > 0) ? "Sube" : "Baja";
                    juce::String txt      = emoji + sevEmoji + gainAdv.trackName + " \xE2\x80\x94 " + arrow + " "
                                            + juce::String(std::abs(gainAdv.suggestedDeltaDb), 1) + " dB (actual "
                                            + juce::String(gainAdv.currentPeak, 1) + " / target "
                                            + juce::String(gainAdv.peakTarget, 1) + ")";
                    entry.coachAdviceText = txt;
                }
                else if (dynAdv.isActionable()) {
                    // Sprint 6B: Dynamics Intelligence con crest target por rol
                    bool off                = (dynAdv.status == CoachEngine::TrackDynamicsAdvice::Status::OffTarget);
                    entry.coachAdviceStatus = off ? SuggestionStatus::Red : SuggestionStatus::Yellow;
                    juce::String sevEmoji = off ? juce::String("\xF0\x9F\x94\xB4 ") : juce::String("\xF0\x9F\x9F\xA1 ");
                    juce::String kind     = dynAdv.isOvercompressed() ? juce::String("Sobre-comprimido")
                                                                      : juce::String("Muy din\xC3\xA1mico");
                    juce::String detail   = kind + " (crest " + juce::String(dynAdv.currentCrest, 1) + " vs "
                                            + juce::String(dynAdv.crestTarget, 1) + "). " + dynAdv.suggestedAction;
                    entry.coachAdviceText = emoji + sevEmoji + dynAdv.trackName + " \xE2\x80\x94 " + detail;
                }
                else if (tonalAdv.isActionable()) {
                    // Sprint 6C: Tonal Intelligence con spectral target por rol
                    bool off                = (tonalAdv.status == CoachEngine::TrackTonalAdvice::Status::OffTarget);
                    entry.coachAdviceStatus = off ? SuggestionStatus::Red : SuggestionStatus::Yellow;
                    juce::String sevEmoji = off ? juce::String("\xF0\x9F\x94\xB4 ") : juce::String("\xF0\x9F\x9F\xA1 ");
                    const char* regionName = CoachEngine::TrackTonalAdvice::kRegionName(tonalAdv.worstRegion);
                    const char* regionFreq = CoachEngine::TrackTonalAdvice::kRegionFreq(tonalAdv.worstRegion);
                    const char* action  = tonalAdv.hasExcess()
                                              ? CoachEngine::TrackTonalAdvice::kExcessSuggestion(tonalAdv.worstRegion)
                                              : CoachEngine::TrackTonalAdvice::kDeficitSuggestion(tonalAdv.worstRegion);
                    juce::String what   = tonalAdv.hasExcess() ? juce::String("Exceso en ") : juce::String("Falta en ");
                    juce::String detail = what + regionName + " (" + regionFreq + "). " + action;
                    entry.coachAdviceText = emoji + sevEmoji + tonalAdv.trackName + " \xE2\x80\x94 " + detail;
                }
                else {
                    // Las 3 dimensiones OnTarget → pista saludable con target confirmado
                    entry.coachAdviceStatus = SuggestionStatus::Green;
                    entry.coachAdviceText   = emoji + "\xF0\x9F\x9F\xA2 " + gainAdv.trackName + " en target ("
                                              + juce::String(gainAdv.peakTarget, 1) + " dB)";
                }
            }
            else if (coachEngine_ != nullptr) {
                auto& feed    = coachEngine_->getTrackFeedCore();
                TrackState ts = feed.getTrackState(idx);
                if (ts.hasSignal() && ts.health != TrackHealth::Unknown) {
                    juce::String emoji = (entry.info.bus != BusType::None) ? getBusEmoji(entry.info.bus) + " "
                                                                           : juce::String();
                    float pk           = ts.getPeakCombinedDb();

                    switch (ts.health) {
                        case TrackHealth::ClippingRisk:
                            entry.coachAdviceStatus = SuggestionStatus::Red;
                            entry.coachAdviceText =
                                emoji + "\xF0\x9F\x94\xB4 Pico " + juce::String(pk, 1) + "dB \xE2\x80\x94 baja YA";
                            break;
                        case TrackHealth::Overcompressed:
                            entry.coachAdviceStatus = SuggestionStatus::Red;
                            entry.coachAdviceText =
                                emoji + "\xF0\x9F\x94\xB4 Crest " + juce::String(ts.crestFactor, 1) + "dB";
                            break;
                        case TrackHealth::StereoCollapse:
                            entry.coachAdviceStatus = SuggestionStatus::Red;
                            entry.coachAdviceText = emoji + "\xF0\x9F\x94\xB4 Corr " + juce::String(ts.correlation, 2);
                            break;
                        case TrackHealth::NeedsEQ:
                        case TrackHealth::MaskingIssue:
                        case TrackHealth::PhaseIssue:
                            entry.coachAdviceStatus = SuggestionStatus::Yellow;
                            entry.coachAdviceText   = emoji + "\xF0\x9F\x9F\xA1 " + entry.aiSuggestion;
                            break;
                        case TrackHealth::NeedsCompression:
                            entry.coachAdviceStatus = SuggestionStatus::Yellow;
                            entry.coachAdviceText =
                                emoji + "\xF0\x9F\x9F\xA1 Crest " + juce::String(ts.crestFactor, 1) + "dB";
                            break;
                        case TrackHealth::Clean:
                            entry.coachAdviceStatus = SuggestionStatus::Green;
                            entry.coachAdviceText =
                                emoji + "\xF0\x9F\x9F\xA2 " + juce::String(ts.rmsCombined, 1) + "dB";
                            break;
                        case TrackHealth::LowSignal:
                        case TrackHealth::Silent:
                            entry.coachAdviceStatus = SuggestionStatus::White;
                            entry.coachAdviceText = emoji + "\xE2\x9A\xAA Sin se" "\xC3\xB1" "al";
                            break;
                        default:
                            entry.coachAdviceText   = entry.aiSuggestion;
                            entry.coachAdviceStatus = entry.suggestionStatus;
                            break;
                    }
                }
                else {
                    entry.coachAdviceText   = entry.aiSuggestion;
                    entry.coachAdviceStatus = entry.suggestionStatus;
                }
            }
            else {
                entry.coachAdviceText   = entry.aiSuggestion;
                entry.coachAdviceStatus = entry.suggestionStatus;
            }

            // ═══ SPRINT 7: Consolidated health from TrackAdvice ═══════════
            {
                auto ta = coach.getTrackAdvice(idx);
                using TS = CoachEngine::TrackAdvice::Status;
                switch (ta.status) {
                    case TS::OnTarget:   entry.consolidatedHealth = SuggestionStatus::Green;  break;
                    case TS::NearTarget: entry.consolidatedHealth = SuggestionStatus::Yellow; break;
                    case TS::OffTarget:  entry.consolidatedHealth = SuggestionStatus::Red;    break;
                    case TS::NoSignal:   entry.consolidatedHealth = SuggestionStatus::White;  break;
                    default:             entry.consolidatedHealth = SuggestionStatus::None;   break;
                }
            }
        }

        // SPRINT 5: Actualizar top events para el banner
        updateTopEvents(coach.getTrackFeedCore());
    }

} // namespace mixcoach
