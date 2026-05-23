#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "ReferencePanelComponent.h"
#include "../../Common/Types.h"
#include "../../Common/Constants.h"
#include "../../Common/SlotRegistry.h"

namespace mixcoach {

// ─── Divider bar con pintado personalizado ──────────────────────────────────
class DividerBar : public juce::Component {
public:
    void paint(juce::Graphics& g) override {
        g.fillAll(MixCoachTheme::border());
    }
};

// ─── Entry de Messenger en la lista ─────────────────────────────────────────
struct MessengerEntry {
    SlotInfo    info;
    float       peakLeft   = -100.0f;
    float       peakRight  = -100.0f;
    float       rmsAvg     = -100.0f;
    bool        hasSignal  = false;
    juce::String aiSuggestion;  // Sugerencia de la IA para esta pista
};

// ─── Grupo de buses ─────────────────────────────────────────────────────────
struct BusGroup {
    int count = 0;
    std::array<int, SlotRegistry::kMaxSlots> slotIndices{};
};

// ═══════════════════════════════════════════════════════════════════════════
//  MessengerListComponent — Panel derecho: lista de Messengers con AI
//  Agrupados por buses con cabeceras de sección coloreadas
// ═══════════════════════════════════════════════════════════════════════════
class MessengerListComponent : public juce::Component
{
public:
    MessengerListComponent();
    ~MessengerListComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void updateMessengers(SlotRegistry& registry);
    void setDisplayVersion(uint64_t version) { displayVersion_ = version; }
    [[nodiscard]] uint64_t getDisplayVersion() const { return displayVersion_; }

private:
    void drawBusHeader(juce::Graphics& g, juce::Rectangle<int>& bounds,
                       int busIdx, int count);
    void drawMessengerRow(juce::Graphics& g, juce::Rectangle<int> bounds,
                          const MessengerEntry& entry, int index);

    std::array<MessengerEntry, SlotRegistry::kMaxSlots> messengers_{};
    std::array<BusGroup, kNumBuses + 1> busGroups_{}; // +1 for "Sin Bus"
    int activeMessengerCount_{0};
    uint64_t displayVersion_{0}; // Versión de datos para evitar repaints innecesarios
    juce::Label titleLabel_;
    juce::Label emptyLabel_;

    static constexpr int kMaxRowsPerBus = 5;
};

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
