#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include "../Common/Types.h"
#include "../Common/SlotRegistry.h"

namespace mixcoach {

// ─── Gestor de fases de mentoría progresiva ─────────────────────────────────
class PhaseManager
{
public:
    explicit PhaseManager(SlotRegistry& registry);

    // Gestión de fase actual
    void               setPhase(MentorPhase phase);
    [[nodiscard]] MentorPhase getCurrentPhase() const noexcept { return currentPhase_; }
    void               advanceToNextPhase();

    // Verificaciones por fase
    [[nodiscard]] bool isPhaseComplete(MentorPhase phase) const;
    [[nodiscard]] float getPhaseProgress(MentorPhase phase) const;

    // Eventos
    [[nodiscard]] int getAchievementCount() const noexcept { return static_cast<int>(achievements_.size()); }
    bool              unlockAchievement(Achievement achievement);

    // Criterios de cada fase
    [[nodiscard]] static int        minTracksForPhase(MentorPhase phase);
    [[nodiscard]] static const char* phaseDescription(MentorPhase phase);

private:
    SlotRegistry&   registry_;
    MentorPhase     currentPhase_{MentorPhase::Welcome};
    std::vector<Achievement> achievements_;

    // Helper
    [[nodiscard]] bool hasAchievement(Achievement ach) const;
};

} // namespace mixcoach
