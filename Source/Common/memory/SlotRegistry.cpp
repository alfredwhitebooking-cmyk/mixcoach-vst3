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
        if (detail.isNotEmpty()) msg += " | " + detail;
        LogHelper::writeToLog(msg);
    }

    const juce::Colour SlotRegistry::kSlotColours[8] = {juce::Colour(0xFFFF0000),
                                                        juce::Colour(0xFF0000FF),
                                                        juce::Colour(0xFF00FF00),
                                                        juce::Colour(0xFFFFA500),
                                                        juce::Colour(0xFF800080),
                                                        juce::Colour(0xFF00FFFF),
                                                        juce::Colour(0xFFFFFF00),
                                                        juce::Colour(0xFFFF00FF)};

    SlotRegistry::SlotRegistry() :
        nextSlot_{0}
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
        if (shm_ != nullptr) totalFound = forceFullSyncFromShm();
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
                auto& local    = slots_[i];
                bool wasActive = local.active;

                local.slotIndex = entry.slotIndex;
                local.active    = true;
                local.bus       = static_cast<BusType>(entry.bus);
                local.colour    = juce::Colour(entry.colourARGB);
                local.trackType = entry.trackType;
                local.muted     = entry.muted != 0;
                local.soloed    = entry.soloed != 0;
                local.faderDb   = entry.faderDb;
                local.panValue  = entry.panValue;
                local.stale     = false;
                strncpy_s(local.trackName, sizeof(local.trackName), entry.trackName, _TRUNCATE);
                ++found;

                if (!wasActive) {
                    logSlot("FORCE_SYNC_NEW", i, "name=" + juce::String(local.trackName));
                    if (onSlotRegistered) onSlotRegistered(i);
                }
            }
            else if (slots_[i].active) {
                logSlot("FORCE_SYNC_RELEASED", i);
                slots_[i].active       = false;
                slots_[i].slotIndex    = -1;
                slots_[i].trackName[0] = '\0';
                slots_[i].stale        = true;
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
            entry.active     = 1;
            entry.bus        = static_cast<int>(bus);
            entry.colourARGB = colour.getARGB();
            entry.slotIndex  = -1;
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
            auto i              = assignedSlot;
            slots_[i].slotIndex = i;
            slots_[i].setTrackName(trackName);
            slots_[i].colour    = colour;
            slots_[i].bus       = bus;
            slots_[i].trackType = -1; // Reset: el Messenger lo actualizará después
            slots_[i].active    = true;
            slots_[i].stale     = false;
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

            if (shm_ != nullptr) shm_->releaseSlot(slotIndex);

            slots_[slotIndex].active       = false;
            slots_[slotIndex].slotIndex    = -1;
            slots_[slotIndex].trackName[0] = '\0';
            slots_[slotIndex].colour       = juce::Colours::grey;
            slots_[slotIndex].bus          = BusType::None;
            ++localChangeCount_;
            if (onSlotReleased) onSlotReleased(slotIndex);
        }
    }

    void SlotRegistry::setActive(int slotIndex, bool active)
    {
        if (slotIndex >= 0 && slotIndex < kMaxSlots) {
            slots_[slotIndex].active = active;
            if (!active) slots_[slotIndex].stale = true;
            else
                slots_[slotIndex].stale = false;
            ++localChangeCount_;

            // ═══ Write to OS shared memory so MixCoach's checkStaleSlots() sees the change ═══
            if (shm_ != nullptr) {
                SharedSlotEntry entry;
                shm_->readSlot(slotIndex, entry);
                entry.slotIndex  = slotIndex;
                entry.active     = active ? 1 : 0;
                entry.bus        = static_cast<int>(slots_[slotIndex].bus);
                entry.colourARGB = slots_[slotIndex].colour.getARGB();
                entry.trackType  = slots_[slotIndex].trackType;
                entry.muted      = slots_[slotIndex].muted ? 1 : 0;
                entry.soloed     = slots_[slotIndex].soloed ? 1 : 0;
                entry.faderDb    = slots_[slotIndex].faderDb;
                entry.panValue   = slots_[slotIndex].panValue;
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
        if (slotIndex >= 0 && slotIndex < kMaxSlots) return slots_[slotIndex];
        return SlotInfo{};
    }

    juce::Colour SlotRegistry::getSlotColour(int slotIndex) const
    {
        if (slotIndex >= 0 && slotIndex < kMaxSlots) return slots_[slotIndex].colour;
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
                entry.slotIndex  = slotIndex;
                entry.active     = 1;
                entry.bus        = static_cast<int>(slots_[slotIndex].bus);
                entry.colourARGB = slots_[slotIndex].colour.getARGB();
                entry.trackType  = slots_[slotIndex].trackType;
                entry.muted      = slots_[slotIndex].muted ? 1 : 0;
                entry.soloed     = slots_[slotIndex].soloed ? 1 : 0;
                entry.faderDb    = slots_[slotIndex].faderDb;
                entry.panValue   = slots_[slotIndex].panValue;
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
                entry.slotIndex  = slotIndex;
                entry.active     = 1;
                entry.bus        = static_cast<int>(slots_[slotIndex].bus);
                entry.colourARGB = colour.getARGB();
                entry.trackType  = slots_[slotIndex].trackType;
                entry.muted      = slots_[slotIndex].muted ? 1 : 0;
                entry.soloed     = slots_[slotIndex].soloed ? 1 : 0;
                entry.faderDb    = slots_[slotIndex].faderDb;
                entry.panValue   = slots_[slotIndex].panValue;
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
                entry.slotIndex  = slotIndex;
                entry.active     = 1;
                entry.bus        = static_cast<int>(bus);
                entry.colourARGB = slots_[slotIndex].colour.getARGB();
                entry.trackType  = slots_[slotIndex].trackType;
                entry.muted      = slots_[slotIndex].muted ? 1 : 0;
                entry.soloed     = slots_[slotIndex].soloed ? 1 : 0;
                entry.faderDb    = slots_[slotIndex].faderDb;
                entry.panValue   = slots_[slotIndex].panValue;
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
                entry.slotIndex  = slotIndex;
                entry.active     = 1;
                entry.bus        = static_cast<int>(slots_[slotIndex].bus);
                entry.colourARGB = slots_[slotIndex].colour.getARGB();
                entry.trackType  = trackType;
                entry.muted      = slots_[slotIndex].muted ? 1 : 0;
                entry.soloed     = slots_[slotIndex].soloed ? 1 : 0;
                entry.faderDb    = slots_[slotIndex].faderDb;
                entry.panValue   = slots_[slotIndex].panValue;
                strncpy_s(entry.trackName, kSharedTrackNameLen, slots_[slotIndex].trackName, _TRUNCATE);
                shm_->writeSlot(slotIndex, entry);
            }

            if (onSlotChanged) onSlotChanged(slotIndex);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateSlotMuted — Actualiza estado de mute (V8)
    // ═══════════════════════════════════════════════════════════════════════════

    void SlotRegistry::updateSlotMuted(int slotIndex, bool muted)
    {
        if (slotIndex >= 0 && slotIndex < kMaxSlots) {
            slots_[slotIndex].muted = muted;
            ++localChangeCount_;

            if (shm_ != nullptr) {
                SharedSlotEntry entry;
                shm_->readSlot(slotIndex, entry);
                entry.slotIndex  = slotIndex;
                entry.active     = slots_[slotIndex].active ? 1 : 0;
                entry.bus        = static_cast<int>(slots_[slotIndex].bus);
                entry.colourARGB = slots_[slotIndex].colour.getARGB();
                entry.trackType  = slots_[slotIndex].trackType;
                entry.muted      = muted ? 1 : 0;
                entry.soloed     = slots_[slotIndex].soloed ? 1 : 0;
                entry.faderDb    = slots_[slotIndex].faderDb;
                entry.panValue   = slots_[slotIndex].panValue;
                strncpy_s(entry.trackName, kSharedTrackNameLen, slots_[slotIndex].trackName, _TRUNCATE);
                shm_->writeSlot(slotIndex, entry);
            }

            if (onSlotChanged) onSlotChanged(slotIndex);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateSlotSoloed — Actualiza estado de solo (V8)
    // ═══════════════════════════════════════════════════════════════════════════

    void SlotRegistry::updateSlotSoloed(int slotIndex, bool soloed)
    {
        if (slotIndex >= 0 && slotIndex < kMaxSlots) {
            slots_[slotIndex].soloed = soloed;
            ++localChangeCount_;

            if (shm_ != nullptr) {
                SharedSlotEntry entry;
                shm_->readSlot(slotIndex, entry);
                entry.slotIndex  = slotIndex;
                entry.active     = slots_[slotIndex].active ? 1 : 0;
                entry.bus        = static_cast<int>(slots_[slotIndex].bus);
                entry.colourARGB = slots_[slotIndex].colour.getARGB();
                entry.trackType  = slots_[slotIndex].trackType;
                entry.muted      = slots_[slotIndex].muted ? 1 : 0;
                entry.soloed     = soloed ? 1 : 0;
                entry.faderDb    = slots_[slotIndex].faderDb;
                entry.panValue   = slots_[slotIndex].panValue;
                strncpy_s(entry.trackName, kSharedTrackNameLen, slots_[slotIndex].trackName, _TRUNCATE);
                shm_->writeSlot(slotIndex, entry);
            }

            if (onSlotChanged) onSlotChanged(slotIndex);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  V10: Heartbeat lock-free — sin spinlock
    // ═══════════════════════════════════════════════════════════════════════════

    void SlotRegistry::setActiveHeartbeat(int slotIndex, int64_t timestampMs) noexcept
    {
        if (slotIndex < 0 || slotIndex >= kMaxSlots) return;

        // Actualizar estado local
        slots_[slotIndex].active = true;
        slots_[slotIndex].stale  = false;
        ++localChangeCount_;

        // Escribir heartbeat lock-free a shared memory
        if (shm_ != nullptr) {
            shm_->writeHeartbeat(slotIndex, timestampMs);
        }
    }

    int64_t SlotRegistry::getHeartbeat(int slotIndex) const noexcept
    {
        if (slotIndex < 0 || slotIndex >= kMaxSlots) return 0;
        if (shm_ == nullptr) return 0;
        return shm_->readHeartbeat(slotIndex);
    }

    // ─── Stale detection (V10: basado en heartbeat timestamps, lock-free) ──────
    // En V10, el audio thread del Messenger solo escribe un heartbeat timestamp
    // (InterlockedExchange64, sin spinlock). MixCoach lee estos timestamps
    // para detectar slots vivos sin adquirir el spinlock.
    //
    // Si un slot activo no actualiza su heartbeat por > kHeartbeatTimeoutMs,
    // se marca como stale (Messenger probablemente se cerró o crasheó).
    // Si no hay shared memory disponible (modo local), caemos al legacy
    // basado en el flag active local.
    static constexpr int kHeartbeatTimeoutMs = 5000; // 5s sin heartbeat = stale

    void SlotRegistry::checkStaleSlots()
    {
        if (shm_ == nullptr) {
            // Modo local (sin shared memory): legacy basado en active flag
            for (int i = 0; i < kMaxSlots; ++i) {
                if (!slots_[i].active && !slots_[i].stale) continue;
                if (slots_[i].active) {
                    // En modo local, los slots nunca se marcan stale automáticamente
                    // porque no hay forma de detectar si el Messenger se desconectó
                }
            }
            return;
        }

        uint32_t now = juce::Time::getMillisecondCounter();

        for (int i = 0; i < kMaxSlots; ++i) {
            // Leer heartbeat lock-free (sin spinlock)
            int64_t hb = shm_->readHeartbeat(i);
            bool hbActive = (hb > 0 && (now - static_cast<uint32_t>(hb)) < kHeartbeatTimeoutMs);

            if (slots_[i].active && !slots_[i].stale) {
                if (!hbActive && hb > 0) {
                    // Slot activo pero heartbeat expiró → stale
                    slots_[i].stale = true;
                    logSlot("STALE", i, "heartbeat expirado (" + juce::String(now - (uint32_t)hb) + "ms sin senal)");
                    if (onSlotChanged) onSlotChanged(i);
                }
            }
            else if (slots_[i].stale && hbActive) {
                // Slot stale pero heartbeat renovado → reconectado
                slots_[i].stale  = false;
                slots_[i].active = true;
                logSlot("STALE_CLEAR", i, "heartbeat renovado — track reconectado");
                if (onSlotChanged) onSlotChanged(i);
            }

            // También verificar el active flag legacy para backward compat
            SharedSlotEntry entry;
            if (shm_->readSlot(i, entry)) {
                if (slots_[i].active && !entry.active && !slots_[i].stale && !hbActive) {
                    slots_[i].stale = true;
                    logSlot("STALE", i, "slot liberado en SHM (legacy)");
                    if (onSlotChanged) onSlotChanged(i);
                }

                // Recuperación: slot estaba stale pero SHM lo muestra activo de nuevo
                // (por ej. registro directo vía SHM sin heartbeat de Messenger).
                // ═══ FIX: No limpiar stale si el heartbeat fue la causa ═══════
                // Si hb > 0, la stale se detectó por heartbeat expirado, y la
                // recuperación debe venir del heartbeat (hbActive), no del entry SHM.
                // Si hb == 0, no hay heartbeat mechanism — solo legacy SHM —
                // entonces la reactivación SHM sí debe limpiar stale.
                if (slots_[i].stale && entry.active && hb == 0) {
                    slots_[i].stale = false;
                    slots_[i].active = true;
                    logSlot("STALE_CLEAR", i, "slot reactivado en SHM (legacy, sin heartbeat)");
                    if (onSlotChanged) onSlotChanged(i);
                }
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

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateSlotFaderDb — Actualiza fader level en dB (V9)
    // ═══════════════════════════════════════════════════════════════════════════

    void SlotRegistry::updateSlotFaderDb(int slotIndex, float faderDb)
    {
        if (slotIndex >= 0 && slotIndex < kMaxSlots) {
            slots_[slotIndex].faderDb = faderDb;
            ++localChangeCount_;

            if (shm_ != nullptr) {
                SharedSlotEntry entry;
                shm_->readSlot(slotIndex, entry);
                entry.slotIndex  = slotIndex;
                entry.active     = slots_[slotIndex].active ? 1 : 0;
                entry.bus        = static_cast<int>(slots_[slotIndex].bus);
                entry.colourARGB = slots_[slotIndex].colour.getARGB();
                entry.trackType  = slots_[slotIndex].trackType;
                entry.muted      = slots_[slotIndex].muted ? 1 : 0;
                entry.soloed     = slots_[slotIndex].soloed ? 1 : 0;
                entry.faderDb    = faderDb;
                entry.panValue   = slots_[slotIndex].panValue;
                strncpy_s(entry.trackName, kSharedTrackNameLen, slots_[slotIndex].trackName, _TRUNCATE);
                shm_->writeSlot(slotIndex, entry);
            }

            if (onSlotChanged) onSlotChanged(slotIndex);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateSlotPanValue — Actualiza pan value (V9)
    // ═══════════════════════════════════════════════════════════════════════════

    void SlotRegistry::updateSlotPanValue(int slotIndex, float panValue)
    {
        if (slotIndex >= 0 && slotIndex < kMaxSlots) {
            slots_[slotIndex].panValue = panValue;
            ++localChangeCount_;

            if (shm_ != nullptr) {
                SharedSlotEntry entry;
                shm_->readSlot(slotIndex, entry);
                entry.slotIndex  = slotIndex;
                entry.active     = slots_[slotIndex].active ? 1 : 0;
                entry.bus        = static_cast<int>(slots_[slotIndex].bus);
                entry.colourARGB = slots_[slotIndex].colour.getARGB();
                entry.trackType  = slots_[slotIndex].trackType;
                entry.muted      = slots_[slotIndex].muted ? 1 : 0;
                entry.soloed     = slots_[slotIndex].soloed ? 1 : 0;
                entry.faderDb    = slots_[slotIndex].faderDb;
                entry.panValue   = panValue;
                strncpy_s(entry.trackName, kSharedTrackNameLen, slots_[slotIndex].trackName, _TRUNCATE);
                shm_->writeSlot(slotIndex, entry);
            }

            if (onSlotChanged) onSlotChanged(slotIndex);
        }
    }

    juce::String SlotRegistry::defaultTrackName()
    {
        static std::atomic<int> counter{0};
        return "Pista " + juce::String(++counter);
    }

} // namespace mixcoach
