#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Common/memory/SlotRegistry.h"
#include "../../Common/memory/SharedData.h"

namespace mixcoach {

class MessengerListComponent;

// ═══ Telemetry sync — lee datos de SharedAudioMemory ══════════════
void syncTelemetryFromRegistryInternal(MessengerListComponent& component,
                                        SlotRegistry& registry, bool& anyDataOut,
                                        SharedData& sharedData);

// ═══ Rebuild bus groups from messenger data ══════════════════════
void rebuildBusGroupsInternal(MessengerListComponent& component);

// ═══ Smooth meters — fixed decay per frame ═══════════════════════
void smoothMetersInternal(MessengerListComponent& component);

// ═══ Restore from persistent static data ════════════════════════
void restoreFromPersistentInternal(MessengerListComponent& component);

} // namespace mixcoach
