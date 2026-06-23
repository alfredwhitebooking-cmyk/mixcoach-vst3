#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "DividerBar.h"
#include "MasterMeterPanel.h"
#include "MessengerListComponent.h"
#include "ReferencePanelComponent.h"
#include "MixMapComponent.h"
#include "../../Common/types/Types.h"
#include "../engine/TrackRole.h"

namespace mixcoach {
    class CoachEngine;
} // namespace mixcoach

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  ChatBubble — Datos de un mensaje individual
    // ═══════════════════════════════════════════════════════════════════════════
    struct ChatBubble
    {
        juce::String text;
        juce::String tag; // Etiqueta opcional ("TIP", "WARN", etc.) — vacío = sin tag
        juce::String timestamp;
        bool isUser;
        bool isSystem = false; // true = mensaje automático del sistema (compacto, dimmed)
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  ChatMessagesComponent — Renderiza burbujas de chat en paint()
    //  Cada burbuja se dibuja con estilo diferenciado: coach vs usuario.
    //  Altura calculada dinámicamente según el texto y el ancho disponible.
    // ═══════════════════════════════════════════════════════════════════════════
    class ChatMessagesComponent : public juce::Component
    {
    public:
        ChatMessagesComponent();

        void addMessage(const juce::String& text, const juce::String& tag = {});
        void addUserMessage(const juce::String& text);
        void addSystemMessage(const juce::String& text);
        void clear();

        // ─── Toggle mensajes del sistema ─────────────────────────────────────
        void setShowSystemMessages(bool show) noexcept
        {
            showSystemMessages_ = show;
            resized();
            repaint();
        }

        bool getShowSystemMessages() const noexcept { return showSystemMessages_; }

        int getNumSystemMessages() const noexcept;

        // ─── Streaming: texto incremental del LLM ────────────────────────────
        /** Inicia un nuevo mensaje del coach en streaming.
            Crea una burbuja vacía que se irá llenando con appendToStream(). */
        void startStreamingMessage();

        /** Añade texto al mensaje en streaming actual.
            Se llama desde el message thread por cada token recibido. */
        void appendToStreamingMessage(const juce::String& text);

        /** Finaliza el mensaje en streaming (elimina el cursor intermitente). */
        void finalizeStreamingMessage();

        /** Retorna true si hay un mensaje en streaming activo. */
        bool hasStreamingMessage() const noexcept { return streamingActive_; }

        void setTypingIndicator(bool isTyping);

        bool isTyping() const { return isTyping_; }

        bool isEmpty() const { return messages_.empty(); }

        void resized() override;
        int getTotalHeight() const;
        void paint(juce::Graphics& g) override;

    private:
        std::vector<ChatBubble> messages_;
        bool isTyping_         = false;
        int64_t typingStartMs_ = 0;

        // ─── Toggle estado del sistema ───────────────────────────────────────
        bool showSystemMessages_ = false;

        // ─── Estado de streaming ─────────────────────────────────────────────
        bool streamingActive_      = false;
        int streamingMessageIndex_ = -1; // Índice en messages_ de la burbuja streaming
        int64_t streamingStartMs_  = 0;

        float getBubbleHeight(const ChatBubble& bubble, float maxWidth) const;

        void drawCoachBubble(juce::Graphics& g, juce::Rectangle<float> bubbleBounds, const ChatBubble& msg);
        /** Dibuja la burbuja del coach con un cursor intermitente al final del texto. */
        void drawCoachBubbleStreaming(juce::Graphics& g, juce::Rectangle<float> bubbleBounds, const ChatBubble& msg);
        void drawUserBubble(juce::Graphics& g, juce::Rectangle<float> bubbleBounds, const ChatBubble& msg);
        /** Dibuja un mensaje del sistema en formato compacto y dimmed (sin burbuja). */
        void drawSystemMessage(juce::Graphics& g, juce::Rectangle<float> bounds, const ChatBubble& msg);
        void drawWelcomeCard(juce::Graphics& g, juce::Rectangle<float> bounds);
        void drawTypingIndicator(juce::Graphics& g, juce::Rectangle<float> bounds);
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  SendButton — Botón circular violeta con ícono de avión de papel
    // ═══════════════════════════════════════════════════════════════════════════
    class SendButton : public juce::Component
    {
    public:
        SendButton();
        void paint(juce::Graphics& g) override;
        std::function<void()> onClick;

    private:
        void mouseEnter(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;
        bool hovered_ = false;
    };

    // ─── Phase name helper ─────────────────────────────────────────────────────
    inline juce::String getPhaseName(MentorPhase phase)
    {
        switch (phase) {
            case MentorPhase::Organizacion:
                return "ORGANIZACIÓN";
            case MentorPhase::GainStaging:
                return "GAIN STAGING";
            case MentorPhase::Balance:
                return "BALANCE";
            case MentorPhase::EQ:
                return "EQ";
            case MentorPhase::Compresion:
                return "COMPRESIÓN";
            case MentorPhase::Espacio:
                return "ESPACIO";
            case MentorPhase::MasterCheck:
                return "MASTER CHECK";
            default:
                return "";
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  MixCoachPanel — Tab AI Coach (MixCoach_Tab1_AICoach.png)
    //  Izquierda ~38%: SESIÓN 1 chat + SESIÓN 2 referencias
    //  Derecha ~62%: SESIÓN 3 pistas, toolbar AGRUPAR POR, lista con meters
    // ═══════════════════════════════════════════════════════════════════════════
    class MixCoachPanel : public juce::Component, public juce::TextEditor::Listener
    {
    public:
        MixCoachPanel();
        ~MixCoachPanel() override = default;

        void resized() override;
        void paint(juce::Graphics& g) override;

        void updateMessengers(SlotRegistry& registry, SharedData& sharedData);

        void updateMasterMeters(const AudioAnalyzer& analyzer) { masterMeterPanel_.updateMeters(analyzer); }

        void refreshMessengerTelemetry(SlotRegistry& registry, SharedData& sharedData)
        {
            messengerList_.refreshTelemetryFromRegistry(registry, sharedData);
        }

        void smoothMeters()
        {
            messengerList_.smoothMeters();
            masterMeterPanel_.advanceVisuals(60.0);
        }

        void updateCoachAdvice(CoachEngine& coach);
        // No inline — necesita acceso a CoachEngine::getTrackRoles()
        // definido en CoachChatComponent.cpp

        void addMessage(const juce::String& text, const juce::String& tag = {});
        void addUserMessage(const juce::String& text);
        void addSystemMessage(const juce::String& text);
        void clearMessages();

        // ─── Toggle mensajes del sistema (pasa a ChatMessagesComponent) ────
        void setShowSystemMessages(bool show);

        bool getShowSystemMessages() const { return showSystemMessages_; }

        void setSuggestions(const std::vector<juce::String>& suggestions)
        {
            suggestions_ = suggestions;
            repaint();
        }

        void setAiTyping(bool isTyping)
        {
            chatMessages_.setTypingIndicator(isTyping);
            resized();
        }

        // ─── Streaming: delegar a ChatMessagesComponent ────────────────────
        void startStreamingMessage()
        {
            chatMessages_.startStreamingMessage();
            resized();
            scrollChatToBottom();
        }

        void appendStreamingToken(const juce::String& text)
        {
            chatMessages_.appendToStreamingMessage(text);
            // Siempre scroll al fondo para que el mensaje más reciente sea visible
            scrollChatToBottom();
        }

        void finalizeStreamingMessage()
        {
            chatMessages_.finalizeStreamingMessage();
            resized();
            scrollChatToBottom();
        }

        std::function<void(const juce::String&)> onMessageSent;
        std::function<void(const juce::String&)> onSuggestionClicked;
        std::function<void(const juce::String&)> onReferenceFileAdded;
        std::function<void(const juce::String&, const juce::String&)> onReferenceURLAdded;
        std::function<void()> onReferenceCleared;
        std::function<void(int refIndex)> onPlayReference;
        /** Callback para seek: en segundos. */
        std::function<void(double)> onSeekReference;

        /** Callback cuando el usuario selecciona una seccion (indice, -1 = global). */
        std::function<void(int sectionIndex)> onSectionSelected;

        /** Callback cuando el usuario selecciona una referencia para comparacion espectral. */
        std::function<void(int refIndex)> onReferenceSelected;

        ReferencePanelComponent& getRefPanel() { return refPanel_; }

        void setRefPanelPlaybackState(int refIndex, bool isPlaying)
        {
            refPanel_.setPlayingRefIndex(isPlaying ? refIndex : -1);
            refPanel_.repaint();
        }

        void setSelectedTrackSlot(int slotIndex);

        int getSelectedTrackSlot() const noexcept { return messengerList_.getSelectedSlot(); }

        std::function<void(int slotIndex)> onTrackSelected;

        /** Actualiza la UI del nivel de experiencia desde una fuente externa (ej: sesión cargada). */
        void setExperienceLevel(int levelIndex);

        /** Actualiza la footer bar con datos en tiempo real desde el motor. */
        /** Actualiza el badge de modo (V4 Dual Mode). */
        void setCoachMode(bool isMasterMode);

        void updateFooterInfo(const juce::String& phaseName,
                              const juce::String& genre,
                              const juce::String& target,
                              const juce::String& sampleRate,
                              int expLevel);

        /** Callback cuando el usuario cambia el nivel de experiencia desde la UI. */
        std::function<void(int expLevelIndex)> onExperienceLevelChanged;
        int currentExpLevel_{1}; // 0=Novice, 1=Intermediate, 2=Advanced, 3=Expert

        /** Callback cuando el usuario pide reconectar Ollama. */
        std::function<void()> onRetryOllama;

        /** Actualiza el estado de la conexión con Ollama en la UI. */
        void setOllamaStatus(bool connected, const juce::String& modelName);

    private:
        /** Helper: desplaza el viewport del chat al fondo para mostrar el mensaje más reciente.
            Primero fuerza un resized() en el viewport para que recalcule los rangos
            del scrollbar (importante cuando el contenido cambia de tamaño).
            Luego mueve el scroll exactamente al final del contenido. */
        void scrollChatToBottom()
        {
            // Forzar viewport a recalcular sus scroll ranges basados en el nuevo tamaño del contenido
            chatViewport_.resized();

            // Posicionar el scrollbar al final del contenido
            auto& scrollbar       = chatViewport_.getVerticalScrollBar();
            double contentHeight  = (double)chatMessages_.getTotalHeight();
            double viewportHeight = (double)chatViewport_.getHeight();
            double maxScroll      = juce::jmax(0.0, contentHeight - viewportHeight);
            scrollbar.setCurrentRange(maxScroll, viewportHeight, juce::sendNotificationSync);
        }

        // ─── Paneles ──────────────────────────────────────────────────────────
        MasterMeterPanel masterMeterPanel_;
        ReferencePanelComponent refPanel_;
        juce::Viewport messengerViewport_;
        MessengerListComponent messengerList_;
        DividerBar dividerBar_;

        // ─── Mix Map component ───────────────────────────────────────────────
        MixMapComponent mixMapComponent_;

        // ─── Coach engine pointer (para chips y badges) ──────────────────────
        CoachEngine* coachEngine_ = nullptr;

        // ─── Track roles cache (desde CoachEngine) ───────────────────────────
        const std::array<TrackRole, SlotRegistry::kMaxSlots>* trackRoles_ = nullptr;

        // ─── Chat bubble components ─────────────────────────────────────────
        juce::Viewport chatViewport_;
        ChatMessagesComponent chatMessages_;
        juce::TextEditor chatInput_;
        SendButton sendButton_;

        // ─── Section labels (SESIÓN format) ──────────────────────────────────
        juce::Label tracksSectionLabel_;     // SESIÓN 3 – pistas (columna derecha)
        juce::Label referencesSectionLabel_; // SESIÓN 2 – referencias

        // ─── Ollama status (Phi-3 local, sin API key) ──────────────────
        juce::Label ollamaStatusLabel_;   // "🤖 Phi-3: conectado"
        juce::TextButton ollamaRetryBtn_; // Botón ↳ para reconectar

        // ─── Footer bar ───────────────────────────────────────────────────────
        juce::Label footerModeLabel_;       // "MODO: MASTER" — V4 Dual Mode indicator
        juce::Label footerPhaseLabel_;      // "FASE ACTUAL: 1 – DIAGNÓSTICO"
        juce::Label footerGenreLabel_;      // "GÉNERO ACTUAL: POP"
        juce::Label footerTargetLabel_;     // "TARGET: -14 LUFS"
        juce::Label footerSampleRateLabel_; // "SAMPLE RATE: 48.0 kHz"
        juce::Label footerExpLevelLabel_;   // "NIVEL: INTERMEDIATE" (clickable)

        // ─── Suggestion chips state ───────────────────────────────────────────
        std::vector<juce::String> suggestions_;
        std::vector<juce::Rectangle<int>> suggestionChipBounds_;

        // ─── Toggle state for system messages ──────────────────────────────
        bool showSystemMessages_ = false;
        juce::Rectangle<int> systemToggleBounds_;
        int hoveredSystemToggle_ = -1;

        // ─── Filter toolbar state (V4: + MIX MAP toggle) ──────────────────
        juce::Rectangle<int> chipTipoBounds_;
        juce::Rectangle<int> chipColorBounds_;
        juce::Rectangle<int> chipBusBounds_;
        juce::Rectangle<int> chipMapBounds_;          // MIX MAP toggle
        juce::Rectangle<int> chipConfirmRolesBounds_; // Sprint 1: CONFIRMAR roles
        juce::Rectangle<int> collapseAllBounds_;
        juce::Rectangle<int> expandAllBounds_;
        int activeGroupingChip_ = 2; // 0=TIPO, 1=COLOR, 2=BUS, 3=MAP (default BUS)
        bool showMixMap_        = false;

        // ─── Suggestion chip hover state ───────────────────────────────────
        int hoveredSuggestionChip_ = -1;

        void mouseDown(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;

        // ─── Bidirectional selection sync between MixMap and MessengerList ──────
        void syncSelectionToMixMap(int slotIndex);
        void syncSelectionToMessengerList(int slotIndex);

        // TextEditor::Listener
        void textEditorReturnKeyPressed(juce::TextEditor& editor) override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixCoachPanel)
    };

} // namespace mixcoach
