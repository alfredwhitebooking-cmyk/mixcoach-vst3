#pragma once
#include <cstdint>
#include <juce_graphics/juce_graphics.h>
#include <array>
#include <string>
#include <functional>
#include "../types/Types.h"
#include "SharedMemory.h"

namespace mixcoach {

    // ─── Registro de slots (pistas) V2 ─────────────────────────────────────────
    // Simplificado: SIN backup files, SIN meta files, SIN AudioRingBuffer.
    // Comunicación solo via SharedMemory (CreateFileMapping).
    class SlotRegistry
    {
    public:
        static constexpr int kMaxSlots = 128;

        SlotRegistry();

        // Registrar / liberar slots
        int registerSlot(const std::string& trackName, const juce::Colour& colour, BusType bus = BusType::None);
        void releaseSlot(int slotIndex);
        void setActive(int slotIndex, bool active);

        // ═══ V10: Heartbeat lock-free — SIN spinlock ═════════════════════
        /** Marca un slot como activo SIN adquirir el spinlock de shared memory.
            Solo actualiza el estado local + escribe un heartbeat timestamp.
            Diseñado para ser llamado desde el audio thread de Messenger
            (~94 veces/segundo/pista a 48kHz).
            @param slotIndex  Índice del slot a mantener activo
            @param timestampMs  Heartbeat timestamp (getMillisecondCounter()) */
        void setActiveHeartbeat(int slotIndex, int64_t timestampMs) noexcept;

        /** Retorna el timestamp del último heartbeat para un slot.
            @param slotIndex  Índice del slot
            @return Timestamp en ms, o 0 si no hay heartbeat */
        int64_t getHeartbeat(int slotIndex) const noexcept;

        // Consultas
        [[nodiscard]] int activeCount() const noexcept;

        [[nodiscard]] int totalSlots() const noexcept { return kMaxSlots; }

        [[nodiscard]] SlotInfo getSlotInfo(int slotIndex) const;
        [[nodiscard]] juce::Colour getSlotColour(int slotIndex) const;

        // Iterar slots activos
        void forEachActive(std::function<void(const SlotInfo&)> callback) const;

        // Actualizar propiedades del slot (desde Messenger UI)
        void updateSlotName(int slotIndex, const std::string& name);
        void updateSlotColour(int slotIndex, const juce::Colour& colour);
        void updateSlotBus(int slotIndex, BusType bus);
        void updateSlotTrackType(int slotIndex, int trackType);
        void updateSlotMuted(int slotIndex, bool muted);
        void updateSlotSoloed(int slotIndex, bool soloed);
        void updateSlotFaderDb(int slotIndex, float faderDb);
        void updateSlotPanValue(int slotIndex, float panValue);

        // ─── Shared Memory (IPC) ──────────────────────────────────────────────
        void setSharedMemory(SharedMemoryManager* shm) noexcept { shm_ = shm; }

        int forceFullSync();
        [[nodiscard]] uint64_t getChangeCount() const noexcept;

        [[nodiscard]] bool hasEverSynced() const noexcept { return everSynced_; }

        // ─── Stale data detection ────────────────────────────────────────────
        // Un slot se marca como stale si active=false en shared memory
        // (Messenger se desconectó). No hay telemetría que chequear.
        void checkStaleSlots();

        // ─── Observer callbacks ───────────────────────────────────────────────
        std::function<void(int slotIndex)> onSlotChanged{nullptr};
        std::function<void(int slotIndex)> onSlotRegistered{nullptr};
        std::function<void(int slotIndex)> onSlotReleased{nullptr};

        // ─── Nombre por defecto ──────────────────────────────────────────────
        [[nodiscard]] static juce::String defaultTrackName();

    private:
        std::array<SlotInfo, kMaxSlots> slots_{};
        int nextSlot_{0};
        uint64_t localChangeCount_{0};

        SharedMemoryManager* shm_ = nullptr;
        uint64_t lastSharedChangeCount_{0};
        bool everSynced_{false};

        static const juce::Colour kSlotColours[8];

        int forceFullSyncFromShm();
    };

} // namespace mixcoach
