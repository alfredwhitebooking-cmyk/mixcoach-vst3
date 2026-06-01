#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "DividerBar.h"
#include "MessengerListComponent.h"
#include "ReferencePanelComponent.h"
#include "../../Common/types/Types.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  MixCoachPanel — Pestaña principal con split vertical
//  Izquierda: Referencias + Chat con IA
//  Derecha:   Lista de Messengers con sugerencias IA por pista
// ═══════════════════════════════════════════════════════════════════════════
class MixCoachPanel : public juce::Component,
                      public juce::TextEditor::Listener
{
public:
    MixCoachPanel();
    ~MixCoachPanel() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    // Actualizar lista de Messengers desde el SlotRegistry
    void updateMessengers(SlotRegistry& registry);

    // Añadir mensaje al chat
    void addMessage(const MentorMessage& msg);
    void clearMessages();

    // Callbacks
    std::function<void(const juce::String&)> onMessageSent;
    std::function<void(const juce::String&)> onSuggestionClicked;

private:
    // ─── Paneles ──────────────────────────────────────────────────────────
    ReferencePanelComponent    refPanel_;
    MessengerListComponent     messengerList_;
    DividerBar                 dividerBar_;

    // ─── Componentes del chat ──────────────────────────────────────────────
    juce::Label       coachHeader_;
    juce::TextEditor  chatHistory_;
    juce::TextEditor  chatInput_;
    juce::Label       statusLabel_;

    // TextEditor::Listener
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;

    void appendFormattedMessage(const MentorMessage& msg);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixCoachPanel)
};

} // namespace mixcoach
