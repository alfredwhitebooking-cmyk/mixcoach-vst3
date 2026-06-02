#include "CoachChatComponent.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  Helpers — Text measurement without JUCE 8 TextLayout
// ═══════════════════════════════════════════════════════════════════════════
namespace {

// Get width of a string using GlyphArrangement (JUCE 8 Font doesn't have getStringWidth)
static float getTextWidth(const juce::Font& font, const juce::String& text)
{
    if (text.isEmpty())
        return 0.0f;
    juce::GlyphArrangement ga;
    ga.addLineOfText(font, text, 0.0f, 0.0f);
    return ga.getBoundingBox(0, ga.getNumGlyphs(), true).getWidth();
}

// Measure height of wrapped text (manual word-wrap)
static float measureWrappedHeight(const juce::String& text,
                                  const juce::Font& font,
                                  float maxWidth)
{
    if (text.isEmpty())
        return font.getHeight();

    auto paragraphs = juce::StringArray::fromLines(text);
    float totalH = 0.0f;
    float lineH = font.getHeight();
    float spaceW = getTextWidth(font, " ");

    for (int p = 0; p < paragraphs.size(); ++p) {
        auto words = juce::StringArray::fromTokens(paragraphs[p], " ", "");
        float lineW = 0.0f;

        for (int i = 0; i < words.size(); ++i) {
            float wordW = getTextWidth(font, words[i]);

            if (lineW + (lineW > 0.0f ? spaceW : 0.0f) + wordW > maxWidth && lineW > 0.0f) {
                totalH += lineH + 2.0f;
                lineW = 0.0f;
                --i; // retry on new line
                continue;
            }

            lineW += (lineW > 0.0f ? spaceW : 0.0f) + wordW;
        }

        totalH += lineH + 2.0f;

        if (p < paragraphs.size() - 1)
            totalH += lineH * 0.5f;
    }

    return totalH;
}

// Draw wrapped text line by line
static void drawWrappedText(juce::Graphics& g,
                            const juce::String& text,
                            const juce::Font& font,
                            const juce::Colour& colour,
                            juce::Rectangle<float> area)
{
    if (text.isEmpty())
        return;

    g.setFont(font);
    g.setColour(colour);

    float maxW = area.getWidth();
    float x = area.getX();
    float y = area.getY();
    float lineH = font.getHeight();
    float spaceW = getTextWidth(font, " ");

    auto paragraphs = juce::StringArray::fromLines(text);

    for (int p = 0; p < paragraphs.size(); ++p) {
        auto words = juce::StringArray::fromTokens(paragraphs[p], " ", "");
        juce::String line;
        float lineW = 0.0f;

        for (int i = 0; i < words.size(); ++i) {
            float wordW = getTextWidth(font, words[i]);

            if (lineW + (lineW > 0.0f ? spaceW : 0.0f) + wordW > maxW && lineW > 0.0f) {
                // Draw current line
                g.drawText(line.trimEnd(),
                           juce::Rectangle<float>(x, y, maxW, lineH),
                           juce::Justification::topLeft);
                y += lineH + 2.0f;

                line = words[i];
                lineW = wordW;
            } else {
                if (line.isNotEmpty())
                    line += " ";
                line += words[i];
                lineW += (lineW > 0.0f ? spaceW : 0.0f) + wordW;
            }
        }

        // Draw last line
        if (line.isNotEmpty()) {
            g.drawText(line.trimEnd(),
                       juce::Rectangle<float>(x, y, maxW, lineH),
                       juce::Justification::topLeft);
            y += lineH + 2.0f;
        }

        if (p < paragraphs.size() - 1)
            y += lineH * 0.5f;
    }
}

} // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════════
//  ChatMessagesComponent — Renderiza burbujas de chat
// ═══════════════════════════════════════════════════════════════════════════

ChatMessagesComponent::ChatMessagesComponent()
{
    setOpaque(false);
}

void ChatMessagesComponent::addMessage(const MentorMessage& msg)
{
    ChatBubble bubble;
    bubble.text      = juce::String(msg.text);
    bubble.type      = msg.type;
    bubble.isUser    = false;
    // Generar timestamp actual HH:MM
    auto now = juce::Time::getCurrentTime();
    bubble.timestamp = now.formatted("%H:%M");
    messages_.push_back(bubble);
    resized();
    repaint();
}

void ChatMessagesComponent::addUserMessage(const juce::String& text)
{
    ChatBubble bubble;
    bubble.text      = text;
    bubble.type      = MentorMessage::Type::Info;  // not applicable for user
    bubble.isUser    = true;
    auto now = juce::Time::getCurrentTime();
    bubble.timestamp = now.formatted("%H:%M");
    messages_.push_back(bubble);
    resized();
    repaint();
}

void ChatMessagesComponent::clear()
{
    messages_.clear();
    repaint();
}

int ChatMessagesComponent::getTotalHeight() const
{
    if (messages_.empty())
        return 0;

    float total = 0.0f;
    float maxW = (float)getWidth();

    for (auto& msg : messages_) {
        total += getBubbleHeight(msg, maxW);
        total += 10.0f; // gap between bubbles
    }

    // Scroll padding
    total += 20.0f;

    return (int)std::ceil(total);
}

void ChatMessagesComponent::resized()
{
    int h = getTotalHeight();
    setSize(getWidth(), h);
}

// ═══════════════════════════════════════════════════════════════════════════
//  getBubbleHeight — Calcula altura de una burbuja según su texto
// ═══════════════════════════════════════════════════════════════════════════
float ChatMessagesComponent::getBubbleHeight(const ChatBubble& bubble,
                                              float maxWidth) const
{
    float bubbleMaxW = maxWidth * (bubble.isUser ? 0.75f : 0.85f);
    bubbleMaxW = juce::jlimit(120.0f, 350.0f, bubbleMaxW);

    float textW = bubbleMaxW - 16.0f;  // padding inside bubble
    juce::Font font(juce::FontOptions(12.0f));

    float textH = measureWrappedHeight(bubble.text, font, textW);

    float bubbleH = textH + 24.0f;  // vertical padding + timestamp
    return juce::jmax(40.0f, bubbleH);
}

// ═══════════════════════════════════════════════════════════════════════════
//  paint — Dibuja todas las burbujas
// ═══════════════════════════════════════════════════════════════════════════
void ChatMessagesComponent::paint(juce::Graphics& g)
{
    if (messages_.empty())
        return;

    float y = 10.0f;  // top padding
    float maxW = (float)getWidth();

    for (auto& msg : messages_) {
        float bubbleH = getBubbleHeight(msg, maxW);
        float bubbleMaxW = juce::jlimit(120.0f, 350.0f, maxW * (msg.isUser ? 0.75f : 0.85f));

        if (msg.isUser) {
            float x = maxW - bubbleMaxW - 8.0f;
            drawUserBubble(g, { x, y, bubbleMaxW, bubbleH }, msg);
        } else {
            drawCoachBubble(g, { 8.0f, y, bubbleMaxW, bubbleH }, msg);
        }

        y += bubbleH + 10.0f;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawCoachBubble — Burbuja del coach (izquierda)
// ═══════════════════════════════════════════════════════════════════════════
void ChatMessagesComponent::drawCoachBubble(juce::Graphics& g,
                                             juce::Rectangle<float> bounds,
                                             const ChatBubble& msg)
{
    // ─── Sombra de la burbuja ──────────────────────────────────────────
    auto shadowBounds = bounds.expanded(1.0f, 1.5f);
    g.setColour(juce::Colours::black.withAlpha(0.12f));
    g.fillRoundedRectangle(shadowBounds, 8.0f);

    // ─── Fondo de la burbuja ──────────────────────────────────────────
    g.setColour(juce::Colour(0xFF1A1A2E));
    g.fillRoundedRectangle(bounds, 8.0f);

    // ─── Borde sutil ──────────────────────────────────────────────────
    g.setColour(juce::Colour(0xFF2A2B3E).withAlpha(0.4f));
    g.drawRoundedRectangle(bounds, 8.0f, 0.5f);

    // ─── Tag de tipo (opcional, pequeño inline) ────────────────────────
    juce::String typeLabel;
    juce::Colour typeColour;
    switch (msg.type) {
        case MentorMessage::Type::Tip:
            typeLabel = "TIP";
            typeColour = MixCoachTheme::info();
            break;
        case MentorMessage::Type::Warning:
            typeLabel = "WARN";
            typeColour = MixCoachTheme::warning();
            break;
        case MentorMessage::Type::Achievement:
            typeLabel = "ACHV";
            typeColour = MixCoachTheme::success();
            break;
        case MentorMessage::Type::Question:
            typeLabel = "Q";
            typeColour = MixCoachTheme::accentGlow();
            break;
        default:
            typeLabel.clear();
            break;
    }

    float innerPad = 8.0f;
    auto textArea = bounds.reduced(innerPad, innerPad);

    // ─── Texto del mensaje ────────────────────────────────────────────
    juce::Font font(juce::FontOptions(12.0f));
    float drawY = textArea.getY();

    if (typeLabel.isNotEmpty()) {
        auto tagBounds = textArea.withHeight(14.0f);
        g.setFont(juce::Font(juce::FontOptions(7.0f)).boldened());
        g.setColour(typeColour.withAlpha(0.7f));
        g.drawText(typeLabel, tagBounds, juce::Justification::topLeft);
        drawY += 16.0f;
    }

    // Draw text with word wrap
    auto textBounds = textArea.withTop(drawY).withBottom(textArea.getBottom() - 2.0f);
    drawWrappedText(g, msg.text, font, MixCoachTheme::textPrimary(), textBounds);

    // ─── Timestamp (esquina inferior izquierda) ────────────────────────
    auto tsBounds = bounds.withTop(bounds.getBottom() - 14.0f).reduced(8.0f, 0.0f);
    g.setFont(juce::Font(juce::FontOptions(7.0f)));
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
    g.drawText(msg.timestamp, tsBounds, juce::Justification::bottomLeft);
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawUserBubble — Burbuja del usuario (derecha)
// ═══════════════════════════════════════════════════════════════════════════
void ChatMessagesComponent::drawUserBubble(juce::Graphics& g,
                                            juce::Rectangle<float> bounds,
                                            const ChatBubble& msg)
{
    // ─── Sombra ────────────────────────────────────────────────────────
    auto shadowBounds = bounds.expanded(1.0f, 1.5f);
    g.setColour(juce::Colours::black.withAlpha(0.12f));
    g.fillRoundedRectangle(shadowBounds, 8.0f);

    // ─── Fondo (ligeramente distinto al coach) ─────────────────────────
    g.setColour(juce::Colour(0xFF22233A));
    g.fillRoundedRectangle(bounds, 8.0f);

    // ─── Borde sutil ──────────────────────────────────────────────────
    g.setColour(juce::Colour(0xFF3A3B5E).withAlpha(0.3f));
    g.drawRoundedRectangle(bounds, 8.0f, 0.5f);

    // ─── Texto del mensaje ────────────────────────────────────────────
    float innerPad = 8.0f;
    auto textArea = bounds.reduced(innerPad, innerPad);

    juce::Font font(juce::FontOptions(12.0f));
    auto textBounds = textArea.withBottom(textArea.getBottom() - 16.0f);
    drawWrappedText(g, msg.text, font, MixCoachTheme::textPrimary(), textBounds);

    // ─── ✓✓ checkmark + timestamp (esquina inferior derecha) ───────────
    auto statusBounds = bounds.withTop(bounds.getBottom() - 14.0f).reduced(8.0f, 0.0f);
    g.setFont(juce::Font(juce::FontOptions(7.0f)));
    g.setColour(MixCoachTheme::info().withAlpha(0.6f));
    g.drawText(juce::String("\xE2\x9C\x93\xE2\x9C\x93 ") + msg.timestamp,
               statusBounds, juce::Justification::bottomRight);
}

// ═══════════════════════════════════════════════════════════════════════════
//  SendButton — Botón circular violeta con ícono de avión
// ═══════════════════════════════════════════════════════════════════════════

SendButton::SendButton()
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setSize(28, 28);
}

void SendButton::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // ─── Círculo de fondo ──────────────────────────────────────────────
    g.setColour(MixCoachTheme::accent());
    g.fillEllipse(bounds.reduced(1.0f, 1.0f));

    // ─── Ícono de avión de papel (triángulo) ───────────────────────────
    g.setColour(juce::Colours::white);

    juce::Path planePath;
    planePath.startNewSubPath(7.0f, bounds.getBottom() - 6.0f);
    planePath.lineTo(bounds.getCentreX() + 4.0f, bounds.getCentreY() - 1.0f);
    planePath.lineTo(7.0f, 7.0f);
    planePath.closeSubPath();
    g.fillPath(planePath);

    // Small triangle for the body (darker)
    juce::Path planeBody;
    planeBody.startNewSubPath(7.0f, bounds.getBottom() - 6.0f);
    planeBody.lineTo(bounds.getCentreX() + 1.0f, bounds.getCentreY() + 1.0f);
    planeBody.lineTo(7.0f, 7.0f);
    planeBody.closeSubPath();
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.fillPath(planeBody);

    // Glow sutil en hover
    if (isMouseOver()) {
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.fillEllipse(bounds.reduced(1.0f, 1.0f));
    }
}

void SendButton::mouseUp(const juce::MouseEvent& /*e*/)
{
    if (onClick)
        onClick();
}

// ═══════════════════════════════════════════════════════════════════════════
//  MixCoachPanel Implementation
// ═══════════════════════════════════════════════════════════════════════════

MixCoachPanel::MixCoachPanel()
{
    // ─── Section: PISTAS (columna derecha ~62%) ─────────────────────────────
    tracksSectionLabel_.setText("SESIÓN 3 – PISTAS DETECTADAS, MEDICIÓN Y SUGERENCIAS",
                                juce::dontSendNotification);
    tracksSectionLabel_.setFont(MixCoachTheme::sectionHeaderFont());
    tracksSectionLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentGlow());
    addAndMakeVisible(tracksSectionLabel_);

    // ─── Section: CHAT (columna izquierda ~38%) ─────────────────────────────
    chatSectionLabel_.setText("SESIÓN 1 – CHAT CON IA", juce::dontSendNotification);
    chatSectionLabel_.setFont(MixCoachTheme::sectionHeaderFont());
    chatSectionLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentGlow());
    addAndMakeVisible(chatSectionLabel_);

    // ─── Coach header ────────────────────────────────────────────────────
    // Robot emoji: U+1F916 (\xF0\x9F\xA4\x96 in UTF-8)
    // ¡: U+00A1 (\xC2\xA1 in UTF-8)
    coachTitleLabel_.setText("\xF0\x9F\xA4\x96 \xC2\xA1Hola, Ingeniero!",
                             juce::dontSendNotification);
    coachTitleLabel_.setFont(juce::Font(juce::FontOptions(15.0f)).boldened());
    coachTitleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textBright());
    addAndMakeVisible(coachTitleLabel_);

    coachSubtitleLabel_.setText("Tu asistente de mezcla con IA",
                                juce::dontSendNotification);
    coachSubtitleLabel_.setFont(juce::Font(juce::FontOptions(10.0f)));
    coachSubtitleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    addAndMakeVisible(coachSubtitleLabel_);

    phaseBadge_.setText("FASE ACTUAL: 2 – ORGANIZACIÓN", juce::dontSendNotification);
    phaseBadge_.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
    phaseBadge_.setColour(juce::Label::textColourId, MixCoachTheme::warning());
    phaseBadge_.setColour(juce::Label::backgroundColourId, MixCoachTheme::warning().withAlpha(0.12f));
    addAndMakeVisible(phaseBadge_);

    // ─── Section: REFERENCES (debajo del chat, columna izquierda) ─────────
    referencesSectionLabel_.setText("SESIÓN 2 – REFERENCIAS, ARCHIVOS Y ENLACES",
                                      juce::dontSendNotification);
    referencesSectionLabel_.setFont(MixCoachTheme::sectionHeaderFont());
    referencesSectionLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentGlow());
    addAndMakeVisible(referencesSectionLabel_);

    // ─── Panel de Referencias ──────────────────────────────────────────────
    addAndMakeVisible(refPanel_);

    // ─── Messenger List (panel izquierdo) con scroll ───────────────────────
    messengerViewport_.setViewedComponent(&messengerList_, false);
    messengerViewport_.setScrollBarsShown(true, false);
    messengerViewport_.setScrollBarThickness(6);
    messengerViewport_.getVerticalScrollBar().setColour(
        juce::ScrollBar::thumbColourId, MixCoachTheme::accent().withAlpha(0.3f));
    messengerViewport_.getVerticalScrollBar().setColour(
        juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(messengerViewport_);

    // ─── Divider bar ──────────────────────────────────────────────────────
    dividerBar_.setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    addAndMakeVisible(dividerBar_);

    // ─── Chat Messages (viewport con chatMessages_) ────────────────────────
    chatViewport_.setViewedComponent(&chatMessages_, false);
    chatViewport_.setScrollBarsShown(true, false);
    chatViewport_.setScrollBarThickness(6);
    chatViewport_.getVerticalScrollBar().setColour(
        juce::ScrollBar::thumbColourId, MixCoachTheme::accent().withAlpha(0.3f));
    chatViewport_.getVerticalScrollBar().setColour(
        juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(chatViewport_);

    // ─── Chat Input ────────────────────────────────────────────────────────
    chatInput_.setMultiLine(false);
    chatInput_.setFont(juce::Font(juce::FontOptions(12.0f)));
    chatInput_.setColour(juce::TextEditor::backgroundColourId, MixCoachTheme::bgInput());
    chatInput_.setColour(juce::TextEditor::textColourId, MixCoachTheme::textPrimary());
    chatInput_.setColour(juce::TextEditor::outlineColourId, MixCoachTheme::border());
    chatInput_.setColour(juce::TextEditor::focusedOutlineColourId, MixCoachTheme::accent());
    chatInput_.setTextToShowWhenEmpty("Escribe tu mensaje...", MixCoachTheme::textMuted());
    chatInput_.setIndents(10, 8);
    chatInput_.addListener(this);
    addAndMakeVisible(chatInput_);

    // ─── Send button ──────────────────────────────────────────────────────
    sendButton_.onClick = [this]() {
        auto text = chatInput_.getText().trim();
        if (text.isNotEmpty() && onMessageSent) {
            onMessageSent(text);
            chatInput_.clear();
        }
    };
    addAndMakeVisible(sendButton_);

    // ─── Footer bar ────────────────────────────────────────────────────────
    footerPhaseLabel_.setText("FASE ACTUAL: 2 – ORGANIZACIÓN", juce::dontSendNotification);
    footerPhaseLabel_.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
    footerPhaseLabel_.setColour(juce::Label::textColourId, MixCoachTheme::warning());
    addAndMakeVisible(footerPhaseLabel_);

    footerGenreLabel_.setText("GÉNERO ACTUAL: POP", juce::dontSendNotification);
    footerGenreLabel_.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
    footerGenreLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accentGlow());
    addAndMakeVisible(footerGenreLabel_);

    footerTargetLabel_.setText("TARGET: -14 LUFS", juce::dontSendNotification);
    footerTargetLabel_.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
    footerTargetLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textBright());
    addAndMakeVisible(footerTargetLabel_);

    footerSampleRateLabel_.setText("SAMPLE RATE: 48.0 kHz", juce::dontSendNotification);
    footerSampleRateLabel_.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
    footerSampleRateLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textBright());
    addAndMakeVisible(footerSampleRateLabel_);

    // ─── Wire messenger list selection to external callback ──────────────
    messengerList_.onSlotSelected = [this](int slotIndex) {
        if (onTrackSelected)
            onTrackSelected(slotIndex);
    };
}

void MixCoachPanel::resized()
{
    auto area = getLocalBounds().reduced(6);

    // ─── Footer: FASE + GÉNERO + TARGET + SAMPLE RATE ───────────────────────
    auto footerArea = area.removeFromBottom(18);
    {
        footerPhaseLabel_.setBounds(footerArea.removeFromLeft(160));
        footerArea.removeFromLeft(4);
        footerGenreLabel_.setBounds(footerArea.removeFromLeft(110));
        footerArea.removeFromLeft(4);
        footerTargetLabel_.setBounds(footerArea.removeFromLeft(90));
        footerArea.removeFromLeft(4);
        footerSampleRateLabel_.setBounds(footerArea.removeFromLeft(110));
    }
    area.removeFromBottom(4);

    // Referencia Tab1: izquierda 38% chat+refs | derecha 62% pistas
    constexpr float kLeftColumnRatio = 0.38f;
    int splitX = (int)(area.getWidth() * kLeftColumnRatio);
    auto leftArea = area.removeFromLeft(splitX);
    auto rightArea = area.reduced(4, 0);

    dividerBar_.setBounds(leftArea.getRight() + 1, leftArea.getY(), 4, leftArea.getHeight());

    // ═══ Columna izquierda: SESIÓN 1 chat + SESIÓN 2 referencias ═══════════
    chatSectionLabel_.setBounds(leftArea.removeFromTop(20));
    leftArea.removeFromTop(2);

    auto coachArea = leftArea.removeFromTop(50);
    coachTitleLabel_.setBounds(coachArea.removeFromTop(22).reduced(2, 0));
    coachSubtitleLabel_.setBounds(coachArea.removeFromTop(14).reduced(2, 0));
    phaseBadge_.setBounds(coachArea.removeFromTop(14).reduced(2, 0));
    leftArea.removeFromTop(4);

    // Referencias y entrada de chat desde abajo hacia arriba
    bool hasRefs = refPanel_.getNumReferences() > 0;
    int refPanelHeight = hasRefs ? 100 : 52;
    refPanel_.setBounds(leftArea.removeFromBottom(refPanelHeight));
    leftArea.removeFromBottom(2);
    referencesSectionLabel_.setBounds(leftArea.removeFromBottom(16));
    leftArea.removeFromBottom(4);

    auto inputArea = leftArea.removeFromBottom(32);
    chatInput_.setBounds(inputArea.withTrimmedRight(34));
    sendButton_.setBounds(inputArea.getRight() - 28, inputArea.getY() + 2, 28, 28);
    leftArea.removeFromBottom(4);

    chatViewport_.setBounds(leftArea);
    chatMessages_.setSize(leftArea.getWidth(), chatMessages_.getTotalHeight());

    // ═══ Columna derecha: SESIÓN 3 pistas + toolbar ════════════════════════
    tracksSectionLabel_.setBounds(rightArea.removeFromTop(20));
    rightArea.removeFromTop(2);

    auto toolbarArea = rightArea.removeFromTop(18).reduced(0, 1);
    {
        int tx = toolbarArea.getX() + 56;
        int ty = toolbarArea.getY();
        int th = toolbarArea.getHeight();

        chipTipoBounds_ = { tx, ty, 32, th };
        tx += 34;
        chipColorBounds_ = { tx, ty, 38, th };
        tx += 40;
        chipBusBounds_ = { tx, ty, 28, th };

        int rightX = toolbarArea.getRight();
        expandAllBounds_ = { rightX - 52, ty, 50, th };
        collapseAllBounds_ = { expandAllBounds_.getX() - 62, ty, 58, th };
    }
    rightArea.removeFromTop(2);

    messengerViewport_.setBounds(rightArea);
    messengerList_.setSize(rightArea.getWidth(),
                           juce::jmax(messengerList_.getPreferredHeight(), rightArea.getHeight()));
}

void MixCoachPanel::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();

    // Fondo principal
    g.fillAll(MixCoachTheme::bgDark());

    // ─── Glass gradient overlay sutil ─────────────────────────────────────
    juce::ColourGradient bgGrad(
        MixCoachTheme::glassHighlight().withAlpha(0.5f),
        juce::Point<float>(0.0f, 0.0f),
        MixCoachTheme::bgDarker(),
        juce::Point<float>(0.0f, (float)area.getHeight()),
        false);
    g.setGradientFill(bgGrad);
    g.fillRect(area);

    // ─── Panel backgrounds (izq chat | der pistas) ─────────────────────────
    auto inner = area.reduced(6);
    int splitX = (int)(inner.getWidth() * 0.38f);

    auto leftBounds = inner.removeFromLeft(splitX).toFloat();
    g.setColour(MixCoachTheme::bgPanel().withAlpha(0.35f));
    g.fillRoundedRectangle(leftBounds, 6.0f);

    auto rightBounds = inner.reduced(4, 0).toFloat();
    g.setColour(MixCoachTheme::bgPanel().withAlpha(0.45f));
    g.fillRoundedRectangle(rightBounds, 6.0f);

    int dividerX = (int)leftBounds.getRight() + 1;
    g.setColour(MixCoachTheme::divider().withAlpha(0.3f));
    g.drawVerticalLine(dividerX, leftBounds.getY() + 4, leftBounds.getBottom() - 4);

    g.setColour(MixCoachTheme::border().withAlpha(0.15f));
    g.drawRoundedRectangle(leftBounds, 6.0f, 1.0f);
    g.drawRoundedRectangle(rightBounds, 6.0f, 1.0f);

    // ═══ FILTER TOOLBAR — columna derecha (pistas) ═════════════════════════
    auto innerArea = area.reduced(6);
    innerArea.removeFromLeft(splitX + 4);
    int toolbarY = innerArea.getY() + 22;

    g.setFont(juce::Font(juce::FontOptions(6.5f)).boldened());
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.6f));
    g.drawText("AGRUPAR POR:",
               innerArea.getX() + 2, toolbarY + 1, 56, 16,
               juce::Justification::centredLeft);

    // Helper lambda to draw a chip
    auto drawChip = [&](juce::Rectangle<int> chipRect, bool isActive, const juce::String& text) {
        if (isActive) {
            g.setColour(MixCoachTheme::accent().withAlpha(0.15f));
            g.fillRoundedRectangle(chipRect.toFloat(), 3.0f);
            g.setColour(MixCoachTheme::accent().withAlpha(0.4f));
            g.drawRoundedRectangle(chipRect.toFloat(), 3.0f, 0.5f);
            g.setColour(MixCoachTheme::accentGlow());
        } else {
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
        }
        g.setFont(juce::Font(juce::FontOptions(6.5f)).boldened());
        g.drawText(text, chipRect, juce::Justification::centred);
    };

    drawChip(chipTipoBounds_, activeGroupingChip_ == 0, "TIPO");
    drawChip(chipColorBounds_, activeGroupingChip_ == 1, "COLOR");
    drawChip(chipBusBounds_, activeGroupingChip_ == 2, "BUS");

    // Draw collapse/expand labels
    g.setFont(juce::Font(juce::FontOptions(6.5f)).boldened());
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
    g.drawText("COLAPSAR", collapseAllBounds_, juce::Justification::centred);
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
    g.drawText("EXPANDIR", expandAllBounds_, juce::Justification::centred);
}

void MixCoachPanel::updateMessengers(SlotRegistry& registry)
{
    int oldHeight = messengerList_.getPreferredHeight();
    messengerList_.updateMessengers(registry);
    int newHeight = messengerList_.getPreferredHeight();
    if (oldHeight != newHeight)
        resized();
}

void MixCoachPanel::addMessage(const MentorMessage& msg)
{
    chatMessages_.addMessage(msg);
    chatViewport_.getVerticalScrollBar().setCurrentRange(
        chatMessages_.getTotalHeight(), chatViewport_.getHeight(), juce::dontSendNotification);
}

void MixCoachPanel::addUserMessage(const juce::String& text)
{
    chatMessages_.addUserMessage(text);
    chatViewport_.getVerticalScrollBar().setCurrentRange(
        chatMessages_.getTotalHeight(), chatViewport_.getHeight(), juce::dontSendNotification);
}

void MixCoachPanel::clearMessages()
{
    chatMessages_.clear();
}

// ═══════════════════════════════════════════════════════════════════════════
//  setSelectedTrackSlot — Selección externa (desde Tab 2 -> Tab 1 sync)
// ═══════════════════════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════════════════════
//  mouseDown — Filter toolbar chip clicks + collapse/expand
// ═══════════════════════════════════════════════════════════════════════════
void MixCoachPanel::mouseDown(const juce::MouseEvent& e)
{
    auto pos = e.getPosition();

    // ─── Grouping chips ────────────────────────────────────────────────
    if (chipTipoBounds_.contains(pos)) {
        activeGroupingChip_ = 0;
        messengerList_.setGroupingMode(MessengerListComponent::GroupingMode::Type);
        repaint();
        return;
    }
    if (chipColorBounds_.contains(pos)) {
        activeGroupingChip_ = 1;
        messengerList_.setGroupingMode(MessengerListComponent::GroupingMode::Colour);
        repaint();
        return;
    }
    if (chipBusBounds_.contains(pos)) {
        activeGroupingChip_ = 2;
        messengerList_.setGroupingMode(MessengerListComponent::GroupingMode::Bus);
        repaint();
        return;
    }

    // ─── Collapse/Expand ───────────────────────────────────────────────
    if (collapseAllBounds_.contains(pos)) {
        messengerList_.collapseAll();
        resized(); // Update viewport height
        return;
    }
    if (expandAllBounds_.contains(pos)) {
        messengerList_.expandAll();
        resized();
        return;
    }
}

void MixCoachPanel::setSelectedTrackSlot(int slotIndex)
{
    messengerList_.setSelectedSlot(slotIndex);
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

} // namespace mixcoach
