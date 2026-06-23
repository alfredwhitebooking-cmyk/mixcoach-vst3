#include "PhaseManager.h"
#include "../../Common/types/LogHelper.h"
#include <algorithm>

namespace mixcoach {

    PhaseManager::PhaseManager(SlotRegistry& registry) :
        registry_(registry)
    {}

    void PhaseManager::setPhase(MentorPhase phase)
    {
        currentPhase_ = phase;
    }

    void PhaseManager::advanceToNextPhase()
    {
        auto next = static_cast<int>(currentPhase_) + 1;
        if (next <= static_cast<int>(MentorPhase::MasterCheck)) {
            currentPhase_ = static_cast<MentorPhase>(next);
        }
    }

    bool PhaseManager::evaluateAndAutoAdvance()
    {
        if (currentPhase_ >= MentorPhase::MasterCheck) return false; // Already at last phase

        if (isPhaseComplete(currentPhase_)) {
            auto completedPhase = currentPhase_;
            advanceToNextPhase();

            LogHelper::writeToLog("[PhaseManager] Auto-advance: "
                                  + juce::String(phaseNames[static_cast<int>(completedPhase)]) + " -> "
                                  + juce::String(phaseNames[static_cast<int>(currentPhase_)]));
            return true;
        }
        return false;
    }

    bool PhaseManager::isPhaseComplete(MentorPhase phase) const
    {
        switch (phase) {
            case MentorPhase::Organizacion: {
                // V2: Complete when: tracks identified, all bussed, and routing validated
                if (!orgMetrics_.hasData) return registry_.activeCount() > 0; // Fallback si no hay métricas

                return orgMetrics_.identifiedTracks > 0 && orgMetrics_.bussedTracks >= orgMetrics_.totalTracks
                       && orgMetrics_.routingValidated;
            }

            case MentorPhase::GainStaging: {
                // Complete when: no clipping, headroom between -18dB and -3dB
                if (!gainMetrics_.hasData) return false;

                if (gainMetrics_.clippingCount > 0) return false;

                if (gainMetrics_.maxGlobalPeak < -18.0f) return false;

                if (gainMetrics_.maxGlobalPeak > -3.0f) return false;

                return true;
            }

            case MentorPhase::Balance: {
                // Complete when: basic balance achieved (no extreme level differences)
                if (!balanceMetrics_.hasData) return false;

                if (balanceMetrics_.totalPairs < 1) return false;

                float ratio = (float)balanceMetrics_.unbalancedPairs / (float)std::max(balanceMetrics_.totalPairs, 1);
                return ratio < 0.3f; // Menos del 30% de pares desbalanceados
            }

            case MentorPhase::EQ: {
                // Complete when: spectral check done, no extreme tonal issues
                if (!tonalMetrics_.hasData) return false;

                return tonalMetrics_.spectralChecked && tonalMetrics_.spectralTiltOk;
            }

            case MentorPhase::Compresion: {
                // Complete when: avg crest in healthy range
                if (!dynamicsMetrics_.hasData) return false;

                return dynamicsMetrics_.avgCrestFactor >= 6.0f && dynamicsMetrics_.avgCrestFactor <= 14.0f;
            }

            case MentorPhase::Espacio: {
                // Complete when: spatial FX applied, correlation healthy
                if (!espacioMetrics_.hasData) return false;

                return espacioMetrics_.avgCorrelation > 0.3f && espacioMetrics_.avgCorrelation < 0.8f;
            }

            case MentorPhase::MasterCheck: {
                // Always return true — final phase
                return true;
            }

            default:
                return false;
        }
    }

    float PhaseManager::getPhaseProgress(MentorPhase phase) const
    {
        switch (phase) {
            case MentorPhase::Organizacion:
                return 1.0f;

            case MentorPhase::GainStaging: {
                if (!gainMetrics_.hasData || gainMetrics_.activeCount < 1) return 0.0f;

                float clipScore = (gainMetrics_.clippingCount == 0) ? 0.4f : 0.0f;
                float headroomScore =
                    (gainMetrics_.maxGlobalPeak > -18.0f && gainMetrics_.maxGlobalPeak < -3.0f) ? 0.3f : 0.0f;
                float nearClipScore = (gainMetrics_.nearClipCount == 0) ? 0.3f : 0.0f;

                return juce::jlimit(0.0f, 1.0f, clipScore + headroomScore + nearClipScore);
            }

            case MentorPhase::Balance: {
                if (!balanceMetrics_.hasData || balanceMetrics_.totalPairs < 1) return 0.0f;

                float ratio = (float)balanceMetrics_.unbalancedPairs / (float)std::max(balanceMetrics_.totalPairs, 1);
                return juce::jlimit(0.0f, 1.0f, 1.0f - ratio);
            }

            case MentorPhase::EQ: {
                if (!tonalMetrics_.hasData) return 0.0f;

                float score = 0.3f; // Base: entró a la fase
                if (tonalMetrics_.excessGraves == 0) score += 0.2f;
                if (tonalMetrics_.excessPresence == 0) score += 0.2f;
                if (tonalMetrics_.faltaPresencia == 0) score += 0.2f;
                if (tonalMetrics_.maskingPairs == 0) score += 0.1f;

                return juce::jlimit(0.0f, 1.0f, score);
            }

            case MentorPhase::Compresion: {
                if (!dynamicsMetrics_.hasData) return 0.0f;

                if (dynamicsMetrics_.healthyDynamics == 0) return 0.0f;

                float healthyRatio =
                    (float)dynamicsMetrics_.healthyDynamics
                    / (float)std::max(dynamicsMetrics_.healthyDynamics + dynamicsMetrics_.overcompressed
                                          + dynamicsMetrics_.undercompressed,
                                      1);
                return juce::jlimit(0.0f, 1.0f, healthyRatio);
            }

            case MentorPhase::Espacio: {
                if (!espacioMetrics_.hasData) return 0.0f;

                float score = 0.3f;
                if (espacioMetrics_.hasSpatialFx) score += 0.3f;
                if (espacioMetrics_.hasAutomation) score += 0.2f;
                if (espacioMetrics_.avgCorrelation > 0.3f && espacioMetrics_.avgCorrelation < 0.8f) score += 0.2f;

                return juce::jlimit(0.0f, 1.0f, score);
            }

            case MentorPhase::MasterCheck: {
                if (!masterCheckMetrics_.hasData) return 0.0f;

                float score = 0.2f; // Base
                if (masterCheckMetrics_.referenceLoaded) {
                    score += 0.2f;
                    if (masterCheckMetrics_.gapsResolved > 0 && masterCheckMetrics_.totalGaps > 0)
                        score += (float)masterCheckMetrics_.gapsResolved / (float)masterCheckMetrics_.totalGaps * 0.2f;
                }
                if (masterCheckMetrics_.lufsTargetMet) score += 0.2f;
                if (masterCheckMetrics_.truePeakOk) score += 0.1f;
                if (masterCheckMetrics_.correlationOk) score += 0.1f;

                return juce::jlimit(0.0f, 1.0f, score);
            }

            default:
                return 0.0f;
        }
    }

    bool PhaseManager::unlockAchievement(Achievement achievement)
    {
        if (!hasAchievement(achievement)) {
            achievements_.push_back(achievement);
            LogHelper::writeToLog("[PhaseManager] Achievement unlocked: "
                                  + juce::String(achievementNames[static_cast<int>(achievement)]));
            return true;
        }
        return false;
    }

    bool PhaseManager::hasAchievement(Achievement ach) const
    {
        return std::find(achievements_.begin(), achievements_.end(), ach) != achievements_.end();
    }

    int PhaseManager::minTracksForPhase(MentorPhase phase)
    {
        switch (phase) {
            case MentorPhase::Organizacion:
                return 0;
            case MentorPhase::GainStaging:
                return 1;
            case MentorPhase::Balance:
                return 3;
            case MentorPhase::EQ:
                return 3;
            case MentorPhase::Compresion:
                return 5;
            case MentorPhase::Espacio:
                return 3;
            case MentorPhase::MasterCheck:
                return 3;
            default:
                return 0;
        }
    }

    const char* PhaseManager::phaseDescription(MentorPhase phase)
    {
        // ═══ MIX MODE: descripciones de mezcla ═════════════════════════════
        switch (phase) {
            case MentorPhase::Organizacion:
                return "Activa Messengers, asigna roles a cada pista y organiza buses y colores";
            case MentorPhase::GainStaging:
                return "Ajusta niveles para evitar clipping y tener headroom saludable (-18dB a -3dB)";
            case MentorPhase::Balance:
                return "Balancea faders y paneo: niveles relativos entre instrumentos, sin procesar a\xC3\xBA"
                       "n";
            case MentorPhase::EQ:
                return "Corrige balance tonal: EQ por pista y bus, carving espectral, elimina enmascaramiento";
            case MentorPhase::Compresion:
                return "Controla din\xC3\xA1"
                       "mica: compresores, saturadores, crest factor saludable";
            case MentorPhase::Espacio:
                return "Crea profundidad: reverb, delay, ancho est\xC3\xA9"
                       "reo, automatizaci\xC3\xB3"
                       "n, panoramas";
            case MentorPhase::MasterCheck:
                return "Verificaci\xC3\xB3"
                       "n final: referencia, LUFS target, compatibilidad mono, veredicto";
            default:
                return "";
        }
    }

    const char* PhaseManager::getPhaseDescription(MentorPhase phase) const noexcept
    {
        // ═══ MASTER MODE: descripciones de masterización ═══════════════════
        if (isMasterMode()) {
            switch (phase) {
                case MentorPhase::Organizacion:
                    return "Configura el destino de masterizaci\xC3\xB3"
                           "n y analiza el master bus";
                case MentorPhase::GainStaging:
                    return "Mide niveles: LUFS moment\xC3\xA1"
                           "neo, True Peak, crest factor del master";
                case MentorPhase::Balance:
                    return "Eval\xC3\xBA"
                           "a balance espectral global: sub, graves, medios, agudos";
                case MentorPhase::EQ:
                    return "Aplica EQ sutiles al master para corregir balance tonal";
                case MentorPhase::Compresion:
                    return "Compresi\xC3\xB3"
                           "n suave, saturaci\xC3\xB3"
                           "n y limitaci\xC3\xB3"
                           "n para alcanzar target LUFS";
                case MentorPhase::Espacio:
                    return "Ensancha est\xC3\xA9"
                           "reo, correlaci\xC3\xB3"
                           "n, profundidad y automatizaci\xC3\xB3"
                           "n final";
                case MentorPhase::MasterCheck:
                    return "Compara con referencia, verifica LUFS/TP/LRA, da el veredicto";
                default:
                    return "";
            }
        }

        // ═══ MIX MODE: delegar a la estática ═══════════════════════════════
        return phaseDescription(phase);
    }

    const char* PhaseManager::getPhaseName(MentorPhase phase) const noexcept
    {
        if (isMasterMode()) return masterPhaseNames[static_cast<int>(phase)];
        return phaseNames[static_cast<int>(phase)];
    }

} // namespace mixcoach
