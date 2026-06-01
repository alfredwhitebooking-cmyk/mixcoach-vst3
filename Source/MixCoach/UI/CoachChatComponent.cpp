#include "CoachChatComponent.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  MixCoachPanel Implementation
// ═══════════════════════════════════════════════════════════════════════════

MixCoachPanel::MixCoachPanel()
{
    // ─── Header ─────────────────────────────────────────────────────────────
    coachHeader_.setText(juce::String(juce::CharPointer_UTF8("\xF0\x9F\x8E\x9B MixCoach \xE2\x80\x94 Mentor Inteligente")), juce::dontSendNotification);
    coachHeader_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTitle)).boldened());
    coachHeader_.setJustificationType(juce::Justification::centredLeft);
    coachHeader_.setColour(juce::Label::textColourId, MixCoachTheme::textBright());
    addAndMakeVisible(coachHeader_);

    // ─── Panel de Referencias ──────────────────────────────────────────────
    addAndMakeVisible(refPanel_);

    // ─── Messenger List (panel derecho) ────────────────────────────────────
    addAndMakeVisible(messengerList_);

    // ─── Divider bar ──────────────────────────────────────────────────────
    dividerBar_.setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    addAndMakeVisible(dividerBar_);

    // ─── Chat History ──────────────────────────────────────────────────────
    chatHistory_.setMultiLine(true);
    chatHistory_.setReadOnly(true);
    chatHistory_.setScrollbarsShown(true);
    chatHistory_.setCaretVisible(false);
    chatHistory_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)));
    chatHistory_.setColour(juce::TextEditor::backgroundColourId, MixCoachTheme::bgDarker());
    chatHistory_.setColour(juce::TextEditor::textColourId, MixCoachTheme::textPrimary());
    chatHistory_.setColour(juce::TextEditor::outlineColourId, MixCoachTheme::border().withAlpha(0.3f));
    addAndMakeVisible(chatHistory_);

    // ─── Chat Input ────────────────────────────────────────────────────────
    chatInput_.setMultiLine(false);
    chatInput_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)));
    chatInput_.setColour(juce::TextEditor::backgroundColourId, MixCoachTheme::bgDarker());
    chatInput_.setColour(juce::TextEditor::textColourId, MixCoachTheme::textPrimary());
    chatInput_.setColour(juce::TextEditor::outlineColourId, MixCoachTheme::border());
    chatInput_.setColour(juce::TextEditor::focusedOutlineColourId, MixCoachTheme::accent());
    chatInput_.setTextToShowWhenEmpty("Preguntale algo al mentor...", MixCoachTheme::textMuted());
    chatInput_.setIndents(8, 6);
    chatInput_.addListener(this);
    addAndMakeVisible(chatInput_);

    // ─── Status ─────────────────────────────────────────────────────────────
    statusLabel_.setText(juce::String(juce::CharPointer_UTF8("\xE2\x97\x89 Conectado  |  Fase: Gain Staging  |  IA activa")), juce::dontSendNotification);
    statusLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    statusLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
    addAndMakeVisible(statusLabel_);
}

void MixCoachPanel::resized()
{
    auto area = getLocalBounds().reduced(6);

    // ─── Header ─────────────────────────────────────────────────────────────
    auto headerArea = area.removeFromTop(28);
    coachHeader_.setBounds(headerArea);

    // ─── Status bar ─────────────────────────────────────────────────────────
    auto statusArea = area.removeFromBottom(16);
    statusLabel_.setBounds(statusArea);

    // ─── Split vertical: Left (chat + refs) | Right (messenger list) ────────
    int splitX = (int)(area.getWidth() * 0.58f);

    auto leftArea = area.removeFromLeft(splitX);
    auto rightArea = area.reduced(4, 0);

    // ─── Divider ───────────────────────────────────────────────────────────
    dividerBar_.setBounds(leftArea.getRight(), leftArea.getY(), 4, leftArea.getHeight());

    // ─── Right side: Messenger List ────────────────────────────────────────
    messengerList_.setBounds(rightArea);

    // ─── Left side: Referencias ─────────────────────────────────────────────
    bool hasRefs = refPanel_.getNumReferences() > 0;
    int refPanelHeight = hasRefs ? 130 : 108;
    auto refArea = leftArea.removeFromTop(refPanelHeight);
    refPanel_.setBounds(refArea);

    // ─── Left side: Chat Input ──────────────────────────────────────────────
    chatInput_.setBounds(leftArea.removeFromBottom(32));

    // ─── Left side: Chat History (ocupa el resto) ───────────────────────────
    chatHistory_.setBounds(leftArea);
}

void MixCoachPanel::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();

    // Fondo principal
    g.fillAll(MixCoachTheme::bgDark());

    // Glass overlay
    juce::ColourGradient bgGrad(
        MixCoachTheme::glassHighlight(),
        juce::Point<float>(0.0f, 0.0f),
        MixCoachTheme::bgDarker(),
        juce::Point<float>(0.0f, (float)area.getHeight()),
        false);
    g.setGradientFill(bgGrad);
    g.fillRect(area);

    // Header bottom line
    g.setColour(MixCoachTheme::accent().withAlpha(0.3f));
    g.drawHorizontalLine(32, 6.0f, (float)(area.getWidth() - 6));

    // Split vertical line (divider visual)
    int splitX = (int)(area.getWidth() * 0.58f) + 6;
    g.setColour(MixCoachTheme::border().withAlpha(0.3f));
    g.drawVerticalLine(splitX, 36.0f, (float)(area.getHeight() - 20));
}

void MixCoachPanel::updateMessengers(SlotRegistry& registry)
{
    messengerList_.updateMessengers(registry);
}

void MixCoachPanel::addMessage(const MentorMessage& msg)
{
    appendFormattedMessage(msg);
}

void MixCoachPanel::clearMessages()
{
    chatHistory_.clear();
}

void MixCoachPanel::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    if (&editor == &chatInput_) {
        auto text = chatInput_.getText().trim();
        if (text.isNotEmpty() && onMessageSent) {
            onMessageSent(text);
            chatInput_.clear();
        }
    }
}

void MixCoachPanel::appendFormattedMessage(const MentorMessage& msg)
{
    juce::String prefix;
    juce::String typeTag;
    switch (msg.type) {
        case MentorMessage::Type::Tip:
            prefix = juce::CharPointer_UTF8("\xF0\x9F\x92\xA1 ");
            typeTag = "[TIP] ";
            break;
        case MentorMessage::Type::Warning:
            prefix = juce::CharPointer_UTF8("\xE2\x9A\xA0\xEF\xB8\x8F ");
            typeTag = "[!] ";
            break;
        case MentorMessage::Type::Achievement:
            prefix = juce::CharPointer_UTF8("\xF0\x9F\x8F\x86 ");
            typeTag = "[LOGRO] ";
            break;
        case MentorMessage::Type::Question:
            prefix = juce::CharPointer_UTF8("\xE2\x9D\x93 ");
            typeTag = "[?] ";
            break;
        default:
            prefix = juce::CharPointer_UTF8("\xF0\x9F\xA4\x96 ");
            typeTag = "[INFO] ";
            break;
    }

    if (!msg.context.empty()) {
        typeTag += "[" + juce::String(msg.context) + "] ";
    }

    chatHistory_.setCaretPosition(chatHistory_.getTotalNumChars());
    auto fullText = prefix + juce::String(msg.text) + "\n\n";
    chatHistory_.insertTextAtCaret(fullText);
    chatHistory_.moveCaretToEnd();
}

} // namespace mixcoach
