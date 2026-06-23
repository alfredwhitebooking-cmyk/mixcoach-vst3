#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MessengerListComponent.h"

namespace mixcoach {

// ═══ Paint entry point ════════════════════════════════════════════
void paintMessengerList(MessengerListComponent& component, juce::Graphics& g);

// ═══ Bus header draw ══════════════════════════════════════════════
void drawBusHeaderInternal(juce::Graphics& g, juce::Rectangle<int>& bounds,
                            int busIdx, int count);

// ═══ Track card draw ══════════════════════════════════════════════
void drawTrackCardInternal(juce::Graphics& g, juce::Rectangle<int> bounds,
                            const MessengerEntry& entry, int index,
                            int selectedSlot, int hoveredSlot, float hoverGlow);

// ═══ Computes preferred height ════════════════════════════════════
int getPreferredHeightInternal(const MessengerListComponent& component);

// ═══ Hit test ═════════════════════════════════════════════════════
int hitTestSlotInternal(const MessengerListComponent& component, juce::Point<int> point);

} // namespace mixcoach
