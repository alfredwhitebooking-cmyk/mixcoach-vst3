#pragma once
#include <juce_core/juce_core.h>
#include "Types.h"
#include "SlotRegistry.h"

namespace mixcoach {

// ─── Datos compartidos entre plugins ────────────────────────────────────────
class SharedData : public juce::ReferenceCountedObject
{
public:
    static SharedData& getInstance()
    {
        static SharedData instance;
        return instance;
    }

    SharedData() = default;
    ~SharedData() override = default;

    // Acceso al registro de slots
    SlotRegistry& getSlotRegistry() noexcept { return slotRegistry_; }
    const SlotRegistry& getSlotRegistry() const noexcept { return slotRegistry_; }

    // Fase activa de mentoría
    void setCurrentPhase(MentorPhase phase) noexcept { currentPhase_ = phase; }
    MentorPhase getCurrentPhase() const noexcept { return currentPhase_; }

    // Mensajes del chat
    void pushMessage(const MentorMessage& msg);
    [[nodiscard]] int getMessageCount() const noexcept;
    [[nodiscard]] MentorMessage getMessage(int index) const;

private:
    SlotRegistry slotRegistry_;
    MentorPhase currentPhase_{MentorPhase::Welcome};
    static constexpr int kMaxMessages = 256;
    std::array<MentorMessage, kMaxMessages> messages_{};
    int messageCount_{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SharedData)
};

} // namespace mixcoach
