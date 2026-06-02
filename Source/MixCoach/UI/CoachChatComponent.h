#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "DividerBar.h"
#include "MessengerListComponent.h"
#include "ReferencePanelComponent.h"
#include "../../Common/types/Types.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  ChatBubble — Datos de un mensaje individual
// ═══════════════════════════════════════════════════════════════════════════
struct ChatBubble {
    juce::String  text;
    MentorMessage::Type type;
    juce::String  timestamp;
    bool          isUser;
};

// ═══════════════════════════════════════════════════════════════════════════
//  ChatMessagesComponent — Renderiza burbujas de chat en paint()
//  Cada burbuja se dibuja con estilo diferenciado: coach vs usuario.
//  Altura calculada dinámicamente según el texto y el ancho disponible.
// ═══════════════════════════════════════════════════════════════════════════
class ChatMessagesComponent : public juce::Component {
public:
    ChatMessagesComponent();

    void addMessage(const MentorMessage& msg);
    void addUserMessage(const juce::String& text);
    void clear();

    void resized() override;
    int  getTotalHeight() const;
    void paint(juce::Graphics& g) override;

private:
    std::vector<ChatBubble> messages_;

    float getBubbleHeight(const ChatBubble& bubble, float maxWidth) const;

    void drawCoachBubble(juce::Graphics& g, juce::Rectangle<float> bubbleBounds,
                         const ChatBubble& msg);
    void drawUserBubble(juce::Graphics& g, juce::Rectangle<float> bubbleBounds,
                        const ChatBubble& msg);
};

// ═══════════════════════════════════════════════════════════════════════════
//  SendButton — Botón circular violeta con ícono de avión de papel
// ═══════════════════════════════════════════════════════════════════════════
class SendButton : public juce::Component {
public:
    SendButton();
    void paint(juce::Graphics& g) override;
    std::function<void()> onClick;
private:
    void mouseUp(const juce::MouseEvent& e) override;
};

// ═══════════════════════════════════════════════════════════════════════════
//  MixCoachPanel — Tab AI Coach (MixCoach_Tab1_AICoach.png)
//  Izquierda ~38%: SESIÓN 1 chat + SESIÓN 2 referencias
//  Derecha ~62%: SESIÓN 3 pistas, toolbar AGRUPAR POR, lista con meters
// ═══════════════════════════════════════════════════════════════════════════
class MixCoachPanel : public juce::Component,
                      public juce::TextEditor::Listener
{
public:
    MixCoachPanel();
    ~MixCoachPanel() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void updateMessengers(SlotRegistry& registry);
    void refreshMessengerTelemetry(SlotRegistry& registry)
    {
        messengerList_.refreshTelemetryFromRegistry(registry);
    }
    void smoothMeters() { messengerList_.smoothMeters(); }

    void addMessage(const MentorMessage& msg);
    void addUserMessage(const juce::String& text);
    void clearMessages();

    std::function<void(const juce::String&)> onMessageSent;
    std::function<void(const juce::String&)> onSuggestionClicked;

    void setSelectedTrackSlot(int slotIndex);
    int getSelectedTrackSlot() const noexcept { return messengerList_.getSelectedSlot(); }
    std::function<void(int slotIndex)> onTrackSelected;

private:
    // ─── Paneles ──────────────────────────────────────────────────────────
    ReferencePanelComponent    refPanel_;
    juce::Viewport             messengerViewport_;
    MessengerListComponent     messengerList_;
    DividerBar                 dividerBar_;

    // ─── Chat bubble components ─────────────────────────────────────────
    juce::Viewport             chatViewport_;
    ChatMessagesComponent      chatMessages_;
    juce::TextEditor           chatInput_;
    SendButton                 sendButton_;

    // ─── Coach header ────────────────────────────────────────────────────
    juce::Label       coachTitleLabel_;    // "¡Hola, Ingeniero!"
    juce::Label       coachSubtitleLabel_; // "Tu asistente de mezcla AI"
    juce::Label       phaseBadge_;         // "FASE ACTUAL: X – NOMBRE"

    // ─── Section labels (SESIÓN format) ──────────────────────────────────
    juce::Label       tracksSectionLabel_;    // SESIÓN 3 – pistas (columna derecha)
    juce::Label       chatSectionLabel_;      // SESIÓN 1 – chat (columna izquierda)
    juce::Label       referencesSectionLabel_;// SESIÓN 2 – referencias

    // ─── Footer bar ───────────────────────────────────────────────────────
    juce::Label       footerPhaseLabel_;      // "FASE ACTUAL: 1 – DIAGNÓSTICO"
    juce::Label       footerGenreLabel_;      // "GÉNERO ACTUAL: POP"
    juce::Label       footerTargetLabel_;     // "TARGET: -14 LUFS"
    juce::Label       footerSampleRateLabel_; // "SAMPLE RATE: 48.0 kHz"

    // ─── Filter toolbar state ─────────────────────────────────────────────
    juce::Rectangle<int> chipTipoBounds_;
    juce::Rectangle<int> chipColorBounds_;
    juce::Rectangle<int> chipBusBounds_;
    juce::Rectangle<int> collapseAllBounds_;
    juce::Rectangle<int> expandAllBounds_;
    int activeGroupingChip_ = 2; // 0=TIPO, 1=COLOR, 2=BUS (default)

    void mouseDown(const juce::MouseEvent& e) override;

    // TextEditor::Listener
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixCoachPanel)
};

} // namespace mixcoach
