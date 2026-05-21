#include "SharedData.h"

namespace mixcoach {

void SharedData::pushMessage(const MentorMessage& msg)
{
    if (messageCount_ < kMaxMessages) {
        messages_[messageCount_++] = msg;
    }
}

int SharedData::getMessageCount() const noexcept
{
    return messageCount_;
}

MentorMessage SharedData::getMessage(int index) const
{
    if (index >= 0 && index < messageCount_)
        return messages_[index];
    return MentorMessage{};
}

} // namespace mixcoach
