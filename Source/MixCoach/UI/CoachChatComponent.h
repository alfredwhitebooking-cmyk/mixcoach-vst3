#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "../../Common/Types.h"

namespace mixcoach {

// ─── Componente del Chat de Mentoría ────────────────────────────────────────
class CoachChatComponent : public juce::Component,
                           public juce::TextEditor::Listener
{
public:
    CoachChatComponent();
    ~CoachChatComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    // Añadir mensaje al chat
    void addMessage(const MentorMessage& msg);
    void clearMessages();

    // TextEditor::Listener
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;

    // Callback cuando el usuario envía un mensaje
    std::function<void(const juce::String&)> onMessageSent;

private:
    juce::TextEditor chatInput_;
    juce::TextEditor chatHistory_;
    juce::Label coachLabel_;
    juce::Label statusLabel_;
    int64_t lastMessageTimestamp_{0};

    void appendFormattedMessage(const MentorMessage& msg);
    juce::Colour getColourForType(MentorMessage::Type type) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CoachChatComponent)
};

} // namespace mixcoach
