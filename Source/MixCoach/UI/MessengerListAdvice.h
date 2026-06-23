#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Common/types/Types.h"
#include "../../Common/memory/SlotRegistry.h"

namespace mixcoach {

// Forward declarations
class CoachEngine;
class MessengerListComponent;
enum class SuggestionStatus : uint8_t;

// ═══ TrackSuggestion — Resultado del analizador por track ═══════════
struct TrackSuggestion {
    juce::String      text;
    SuggestionStatus  status;
};

// ═══ Helpers de sugerencia por track ═══════════════════════════════
juce::String getBusEmoji(BusType bus) noexcept;
TrackSuggestion analyzeTrackSuggestion(float peakDb, float rmsDb, bool hasSignal,
                                         const SlotInfo& info);

// ═══ Coach advice — actualiza entradas desde CoachEngine ═══════════
void updateCoachAdviceInternal(MessengerListComponent& component, CoachEngine& coach);

} // namespace mixcoach
