#include "SlotRegistry.h"
#include "../types/Constants.h"
#include "../types/LogHelper.h"
#include <juce_graphics/juce_graphics.h>
#include <juce_core/juce_core.h>
#include <atomic>

namespace mixcoach {

static void logSlot(const juce::String& action, int slotIndex, const juce::String& detail = {})
{
    auto msg = "[SlotRegistry] " + action + " slot=" + juce::String(slotIndex);
    if (detail.isNotEmpty())
        msg += " | " + detail;
    LogHelper::writeToLog(msg);
}

const juce::Colour SlotRegistry::kSlotColours[8] = {
    juce::Colour(0xFFFF0000),
    juce::Colour(0xFF0000FF),
    juce::Colour(0xFF00FF00),
    juce::Colour(0xFFFFA500),
    juce::Colour(0xFF800080),
    juce::Colour(0xFF00FFFF),
    juce::Colour(0xFFFFFF00),
    juce::Colour(0xFFFF00FF)
};

SlotRegistry::SlotRegistry()
    : nextSlot_{0}
{
    for (auto& slot : slots_) {
        slot.slotIndex = -1;
        slot.active    = false;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Shared Memory — V3 sensor puro (solo identidad, sin telemetría)
// ═══════════════════════════════════════════════════════════════════════════

int SlotRegistry::forceFullSync()
{
    int totalFound = 0;
    if (shm_ != nullptr)
        totalFound = forceFullSyncFromShm();
    return totalFound;
}

int SlotRegistry::forceFullSyncFromShm()
{
    if (shm_ == nullptr) return 0;

    int found = 0;
    for (int i = 0; i < kMaxSlots; ++i) {
        SharedSlotEntry entry;
        if (!shm_->readSlot(i, entry)) continue;

        if (entry.active) {
            auto& local = slots_[i];
            bool wasActive = local.active;

            local.slotIndex = entry.slotIndex;
            local.active    = true;
            local.bus       = static_cast<BusType>(entry.bus);
            local.colour    = juce::Colour(entry.colourARGB);
            local.trackType = entry.trackType;
            local.stale     = false;
            strncpy_s(local.trackName, sizeof(local.trackName), entry.trackName, _TRUNCATE);
            ++found;

            if (!wasActive) {
                logSlot("FORCE_SYNC_NEW", i, "name=" + juce::String(local.trackName));
                if (onSlotRegistered) onSlotRegistered(i);
            }
        } else if (slots_[i].active) {
            logSlot("FORCE_SYNC_RELEASED", i);
            slots_[i].active = false;
            slots_[i].slotIndex = -1;
            slots_[i].trackName[0] = '\0';
            slots_[i].stale = true;
            if (onSlotReleased) onSlotReleased(i);
        }
    }

    if (shm_ != nullptr) {
        lastSharedChangeCount_ = shm_->getChangeCount();
        localChangeCount_      = lastSharedChangeCount_;
        everSynced_            = true;
    }

    logSlot("FORCE_SYNC_DONE", -1, "found=" + juce::String(found));
    return found;
}

// ─── Registro / Liberación ──────────────────────────────────────────────────

int SlotRegistry::registerSlot(const std::string& trackName, const juce::Colour& colour, BusType bus)
{
    int assignedSlot = -1;

    if (shm_ != nullptr) {
        SharedSlotEntry entry;
        entry.active    = 1;
        entry.bus       = static_cast<int>(bus);
        entry.colourARGB = colour.getARGB();
        entry.slotIndex = -1;
        strncpy_s(entry.trackName, kSharedTrackNameLen, trackName.c_str(), _TRUNCATE);

        assignedSlot = shm_->registerSlot(entry);
        if (assignedSlot < 0) {
            logSlot("REGISTER_FAIL", -1, "Shared memory llena");
            return -1;
        }
    }

    if (assignedSlot < 0) {
        for (int i = 0; i < kMaxSlots; ++i) {
            if (!slots_[i].active) {
                assignedSlot = i;
                break;
            }
        }
    }

    if (assignedSlot >= 0 && assignedSlot < kMaxSlots) {
        auto i = assignedSlot;
        slots_[i].slotIndex = i;
        slots_[i].setTrackName(trackName);
        slots_[i].colour = colour;
        slots_[i].bus    = bus;
        slots_[i].trackType = -1;  // Reset: el Messenger lo actualizará después
        slots_[i].active = true;
        slots_[i].stale  = false;
        ++localChangeCount_;
        logSlot("REGISTER", i, "name=" + juce::String(trackName));

        if (onSlotRegistered) onSlotRegistered(i);
        return i;
    }

    logSlot("REGISTER_FAIL", -1, "No hay espacio libre");
    return -1;
}

void SlotRegistry::releaseSlot(int slotIndex)
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots) {
        logSlot("RELEASE", slotIndex, "name=" + juce::String(slots_[slotIndex].trackName));

        if (shm_ != nullptr)
            shm_->releaseSlot(slotIndex);

        slots_[slotIndex].active    = false;
        slots_[slotIndex].slotIndex = -1;
        slots_[slotIndex].trackName[0] = '\0';
        slots_[slotIndex].colour = juce::Colours::grey;
        slots_[slotIndex].bus = BusType::None;
        ++localChangeCount_;
        if (onSlotReleased) onSlotReleased(slotIndex);
    }
}

void SlotRegistry::setActive(int slotIndex, bool active)
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots) {
        slots_[slotIndex].active = active;
        if (!active) slots_[slotIndex].stale = true;
        else         slots_[slotIndex].stale = false;
        ++localChangeCount_;

        // ═══ Write to OS shared memory so MixCoach's checkStaleSlots() sees the change ═══
        if (shm_ != nullptr) {
            SharedSlotEntry entry;
            shm_->readSlot(slotIndex, entry);
            entry.slotIndex = slotIndex;
            entry.active    = active ? 1 : 0;
            entry.bus       = static_cast<int>(slots_[slotIndex].bus);
            entry.colourARGB = slots_[slotIndex].colour.getARGB();
            entry.trackType = slots_[slotIndex].trackType;
            strncpy_s(entry.trackName, kSharedTrackNameLen, slots_[slotIndex].trackName, _TRUNCATE);
            shm_->writeSlot(slotIndex, entry);
        }

        if (onSlotChanged) onSlotChanged(slotIndex);
    }
}

// ─── Consultas ──────────────────────────────────────────────────────────────

int SlotRegistry::activeCount() const noexcept
{
    int count = 0;
    for (const auto& slot : slots_) {
        if (slot.active && !slot.stale) ++count;
    }
    return count;
}

SlotInfo SlotRegistry::getSlotInfo(int slotIndex) const
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots)
        return slots_[slotIndex];
    return SlotInfo{};
}

juce::Colour SlotRegistry::getSlotColour(int slotIndex) const
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots)
        return slots_[slotIndex].colour;
    return juce::Colours::grey;
}

void SlotRegistry::forEachActive(std::function<void(const SlotInfo&)> callback) const
{
    for (const auto& slot : slots_) {
        if (slot.active && !slot.stale) callback(slot);
    }
}

// ─── Actualizaciones (identidad) ────────────────────────────────────────────

void SlotRegistry::updateSlotName(int slotIndex, const std::string& name)
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots) {
        slots_[slotIndex].setTrackName(name);
        ++localChangeCount_;

        if (shm_ != nullptr) {
            SharedSlotEntry entry;
            shm_->readSlot(slotIndex, entry);
            entry.slotIndex = slotIndex;
            entry.active    = 1;
            entry.bus       = static_cast<int>(slots_[slotIndex].bus);
            entry.colourARGB = slots_[slotIndex].colour.getARGB();
            entry.trackType = slots_[slotIndex].trackType;
            strncpy_s(entry.trackName, kSharedTrackNameLen, name.c_str(), _TRUNCATE);
            shm_->writeSlot(slotIndex, entry);
        }

        if (onSlotChanged) onSlotChanged(slotIndex);
    }
}

void SlotRegistry::updateSlotColour(int slotIndex, const juce::Colour& colour)
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots) {
        slots_[slotIndex].colour = colour;
        ++localChangeCount_;

        if (shm_ != nullptr) {
            SharedSlotEntry entry;
            shm_->readSlot(slotIndex, entry);
            entry.slotIndex = slotIndex;
            entry.active    = 1;
            entry.bus       = static_cast<int>(slots_[slotIndex].bus);
            entry.colourARGB = colour.getARGB();
            entry.trackType = slots_[slotIndex].trackType;
            strncpy_s(entry.trackName, kSharedTrackNameLen, slots_[slotIndex].trackName, _TRUNCATE);
            shm_->writeSlot(slotIndex, entry);
        }

        if (onSlotChanged) onSlotChanged(slotIndex);
    }
}

void SlotRegistry::updateSlotBus(int slotIndex, BusType bus)
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots) {
        slots_[slotIndex].bus = bus;
        ++localChangeCount_;

        if (shm_ != nullptr) {
            SharedSlotEntry entry;
            shm_->readSlot(slotIndex, entry);
            entry.slotIndex = slotIndex;
            entry.active    = 1;
            entry.bus       = static_cast<int>(bus);
            entry.colourARGB = slots_[slotIndex].colour.getARGB();
            entry.trackType = slots_[slotIndex].trackType;
            strncpy_s(entry.trackName, kSharedTrackNameLen, slots_[slotIndex].trackName, _TRUNCATE);
            shm_->writeSlot(slotIndex, entry);
        }

        if (onSlotChanged) onSlotChanged(slotIndex);
    }
}

// ─── Actualizar TrackType (V7 Identity Layer) ───────────────────────────────

void SlotRegistry::updateSlotTrackType(int slotIndex, int trackType)
{
    if (slotIndex >= 0 && slotIndex < kMaxSlots) {
        slots_[slotIndex].trackType = trackType;
        ++localChangeCount_;

        if (shm_ != nullptr) {
            SharedSlotEntry entry;
            shm_->readSlot(slotIndex, entry);
            entry.slotIndex = slotIndex;
            entry.active    = 1;
            entry.bus       = static_cast<int>(slots_[slotIndex].bus);
            entry.colourARGB = slots_[slotIndex].colour.getARGB();
            entry.trackType = trackType;
            strncpy_s(entry.trackName, kSharedTrackNameLen, slots_[slotIndex].trackName, _TRUNCATE);
            shm_->writeSlot(slotIndex, entry);
        }

        if (onSlotChanged) onSlotChanged(slotIndex);
    }
}

// ─── Stale detection (V3: basado en active flag, sin telemetría) ────────────

void SlotRegistry::checkStaleSlots()
{
    // En V3, los slots se marcan como stale cuando active=false
    // o cuando forceFullSyncFromShm() detecta que el slot fue liberado.
    // Este método verifica consistencia con shared memory.
    if (shm_ == nullptr) return;

    for (int i = 0; i < kMaxSlots; ++i) {
        SharedSlotEntry entry;
        if (!shm_->readSlot(i, entry)) {
            if (slots_[i].active && !slots_[i].stale) {
                slots_[i].stale = true;
                logSlot("STALE", i, "shared memory no responde");
                if (onSlotChanged) onSlotChanged(i);
            }
            continue;
        }

        if (slots_[i].active && !entry.active && !slots_[i].stale) {
            slots_[i].stale = true;
            logSlot("STALE", i, "slot liberado en SHM");
            if (onSlotChanged) onSlotChanged(i);
        } else if (slots_[i].stale && entry.active) {
            slots_[i].stale = false;
            slots_[i].active = true;
            logSlot("STALE_CLEAR", i, "track reconectado");
            if (onSlotChanged) onSlotChanged(i);
        }
    }
}

uint64_t SlotRegistry::getChangeCount() const noexcept
{
    if (shm_ != nullptr) {
        return shm_->getChangeCount();
    }
    return localChangeCount_;
}

juce::String SlotRegistry::defaultTrackName()
{
    static std::atomic<int> counter{0};
    return "Pista " + juce::String(++counter);
}

} // namespace mixcoach
