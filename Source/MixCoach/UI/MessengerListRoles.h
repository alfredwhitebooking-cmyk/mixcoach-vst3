#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Common/memory/SlotRegistry.h"
#include "../engine/TrackRole.h"

namespace mixcoach {

    class MessengerListComponent;

    // ═══ Role colour helper ═══════════════════════════════════════════
    juce::Colour roleColourForCategory(RoleCategory cat) noexcept;

    // ═══ Role menu ID mapping ═════════════════════════════════════════
    TrackRole roleFromMenuId(int menuId);

    // ═══ Build role popup menu ════════════════════════════════════════
    juce::PopupMenu buildRoleMenuStatic();

    // ═══ Sync roles from CoachEngine → messengers_ ═══════════════════
    void syncRolesFromCoachInternal(MessengerListComponent& component);

} // namespace mixcoach
