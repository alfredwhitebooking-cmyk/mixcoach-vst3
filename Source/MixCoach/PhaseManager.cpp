#include "PhaseManager.h"
#include <algorithm>

namespace mixcoach {

PhaseManager::PhaseManager(SlotRegistry& registry)
    : registry_(registry)
{
}

void PhaseManager::setPhase(MentorPhase phase)
{
    currentPhase_ = phase;
}

void PhaseManager::advanceToNextPhase()
{
    auto next = static_cast<int>(currentPhase_) + 1;
    if (next <= static_cast<int>(MentorPhase::Spatial)) {
        currentPhase_ = static_cast<MentorPhase>(next);
    }
}

bool PhaseManager::isPhaseComplete(MentorPhase phase) const
{
    // Cada fase se completa cuando se cumplen sus criterios
    switch (phase) {
        case MentorPhase::Welcome:
            return true; // La bienvenida se completa automáticamente

        case MentorPhase::GainStaging:
            // Completada cuando no hay clipping y hay headroom adecuado
            return registry_.activeCount() > 0;

        case MentorPhase::Organisation:
            // Completada cuando hay al menos 3 pistas organizadas
            return registry_.activeCount() >= 3;

        case MentorPhase::TonalBalance:
        case MentorPhase::Dynamics:
        case MentorPhase::Spatial:
            return true; // Se completan cuando el usuario las trabaja

        default:
            return false;
    }
}

float PhaseManager::getPhaseProgress(MentorPhase phase) const
{
    switch (phase) {
        case MentorPhase::Welcome:
            return 1.0f;
        case MentorPhase::GainStaging:
            return registry_.activeCount() > 0 ? 0.5f : 0.0f;
        case MentorPhase::Organisation:
            return juce::jlimit(0.0f, 1.0f, registry_.activeCount() / 10.0f);
        case MentorPhase::TonalBalance:
            return 0.0f;
        case MentorPhase::Dynamics:
            return 0.0f;
        case MentorPhase::Spatial:
            return 0.0f;
        default:
            return 0.0f;
    }
}

bool PhaseManager::unlockAchievement(Achievement achievement)
{
    if (!hasAchievement(achievement)) {
        achievements_.push_back(achievement);
        return true;
    }
    return false;
}

bool PhaseManager::hasAchievement(Achievement ach) const
{
    return std::find(achievements_.begin(), achievements_.end(), ach)
           != achievements_.end();
}

int PhaseManager::minTracksForPhase(MentorPhase phase)
{
    switch (phase) {
        case MentorPhase::Welcome:       return 0;
        case MentorPhase::GainStaging:   return 1;
        case MentorPhase::Organisation:  return 3;
        case MentorPhase::TonalBalance:  return 5;
        case MentorPhase::Dynamics:      return 8;
        case MentorPhase::Spatial:       return 10;
        default:                         return 0;
    }
}

const char* PhaseManager::phaseDescription(MentorPhase phase)
{
    switch (phase) {
        case MentorPhase::Welcome:
            return "Configura tu género musical y contexto de mezcla";
        case MentorPhase::GainStaging:
            return "Ajusta niveles para evitar clipping y mantener -6dB de headroom";
        case MentorPhase::Organisation:
            return "Nombra, colorea y organiza tus pistas en buses virtuales";
        case MentorPhase::TonalBalance:
            return "Analiza el balance espectral contra referencias del género";
        case MentorPhase::Dynamics:
            return "Controla picos y dinámica con compresores y limitadores";
        case MentorPhase::Spatial:
            return "Trabaja profundidad, panoramas y efectos espaciales";
        default:
            return "";
    }
}

} // namespace mixcoach
