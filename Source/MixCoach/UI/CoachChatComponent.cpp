#include "CoachChatComponent.h"

namespace mixcoach {

CoachChatComponent::CoachChatComponent()
{
    coachLabel_.setText("🎧 MixCoach Mentor", juce::dontSendNotification);
    coachLabel_.setFont(juce::Font(juce::FontOptions(18.0f)).boldened());
    coachLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(coachLabel_);

    chatHistory_.setMultiLine(true);
    chatHistory_.setReadOnly(true);
    chatHistory_.setScrollbarsShown(true);
    chatHistory_.setCaretVisible(false);
    chatHistory_.setFont(juce::Font(juce::FontOptions(14.0f)));
    chatHistory_.setColour(juce::TextEditor::backgroundColourId, MixCoachTheme::bgDark());
    chatHistory_.setColour(juce::TextEditor::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(chatHistory_);

    chatInput_.setMultiLine(false);
    chatInput_.setFont(juce::Font(juce::FontOptions(14.0f)));
    chatInput_.setColour(juce::TextEditor::backgroundColourId, MixCoachTheme::bgDarker());
    chatInput_.setColour(juce::TextEditor::textColourId, MixCoachTheme::textPrimary());
    chatInput_.setColour(juce::TextEditor::outlineColourId, MixCoachTheme::accent());
    chatInput_.setTextToShowWhenEmpty("Pregúntale algo al mentor...", MixCoachTheme::textDim());
    chatInput_.addListener(this);
    addAndMakeVisible(chatInput_);

    statusLabel_.setText("✅ Listo para ayudarte", juce::dontSendNotification);
    statusLabel_.setFont(juce::Font(juce::FontOptions(12.0f)));
    statusLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    addAndMakeVisible(statusLabel_);
}

void CoachChatComponent::resized()
{
    auto area = getLocalBounds().reduced(8);

    auto headerArea = area.removeFromTop(32);
    coachLabel_.setBounds(headerArea);

    statusLabel_.setBounds(area.removeFromBottom(20));
    chatInput_.setBounds(area.removeFromBottom(36));
    chatHistory_.setBounds(area);
}

void CoachChatComponent::paint(juce::Graphics& g)
{
    g.fillAll(MixCoachTheme::bgDark());
}

void CoachChatComponent::addMessage(const MentorMessage& msg)
{
    appendFormattedMessage(msg);
    lastMessageTimestamp_ = msg.timestamp;
}

void CoachChatComponent::clearMessages()
{
    chatHistory_.clear();
}

void CoachChatComponent::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    if (&editor == &chatInput_) {
        auto text = chatInput_.getText().trim();
        if (text.isNotEmpty() && onMessageSent) {
            onMessageSent(text);
            chatInput_.clear();
        }
    }
}

void CoachChatComponent::appendFormattedMessage(const MentorMessage& msg)
{
    juce::String prefix;
    switch (msg.type) {
        case MentorMessage::Type::Tip:         prefix = "💡 "; break;
        case MentorMessage::Type::Warning:     prefix = "⚠️  "; break;
        case MentorMessage::Type::Achievement: prefix = "🏆 "; break;
        case MentorMessage::Type::Question:    prefix = "❓ "; break;
        default:                               prefix = "🤖 "; break;
    }

    if (!msg.context.empty()) {
        prefix += "[" + juce::String(msg.context) + "] ";
    }

    chatHistory_.setCaretPosition(chatHistory_.getTotalNumChars());
    chatHistory_.insertTextAtCaret(prefix + juce::String(msg.text) + "\n\n");
    chatHistory_.moveCaretToEnd();
}

juce::Colour CoachChatComponent::getColourForType(MentorMessage::Type type) const
{
    switch (type) {
        case MentorMessage::Type::Tip:         return MixCoachTheme::accent();
        case MentorMessage::Type::Warning:     return MixCoachTheme::warning();
        case MentorMessage::Type::Achievement: return MixCoachTheme::success();
        case MentorMessage::Type::Question:    return MixCoachTheme::info();
        default:                               return MixCoachTheme::textPrimary();
    }
}

} // namespace mixcoach
