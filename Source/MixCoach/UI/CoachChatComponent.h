#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "CoachRoomState.h"
#include "AutomationPanel.h"
#include "DividerBar.h"
#include "MasterMeterPanel.h"
#include "MessengerListComponent.h"
#include "ReferencePanelComponent.h"
#include "MixMapComponent.h"
#include "TrackProblemCard.h"
#include "RobotAvatarComponent.h"
#include "TabBarComponent.h"
#include "QuickReplyBar.h"
#include "ModeSelectionCard.h"
#include "GenreSelectionGrid.h"
#include "ReferenceOnboardingCard.h"
#include "ReferenceAnalysisProgressCard.h"
#include "SessionScanCard.h"
#include "SessionPrepChecklist.h"
#include "TrackProblemBuilder.h"
#include "CoachingGuideWidget.h"
#include "MixMapDetailPanel.h"
#include "CoachingEvidenceHost.h"
#include "GainStagingPanel.h"
#include "EQPanel.h"
#include "CompressionPanel.h"
#include "SpacePanel.h"
#include "MasterCheckPanel.h"
#include "PluginConfirmationPanel.h"
#include "../engine/PanelRevealManager.h"
#include "../../Common/types/Types.h"
#include "../engine/TrackRole.h"
#include "../engine/CoachEngine.h"

namespace mixcoach {
    struct TrackTelemetry;
    class AudioAnalyzer;
    class SharedData;
    class SlotRegistry;

    // ═══════════════════════════════════════════════════════════════════════════
    //  EvidencePanel — Mini-visor contextual para split-view coaching
    //  Aparece en la columna derecha durante coaching, junto al panel de fase.
    //  Muestra visualizaciones según el estado: VU (Gain), Spectrum (EQ),
    //  Crest (Comp), Vectorscope (Space), Match Score (MasterCheck).
    // ═══════════════════════════════════════════════════════════════════════════
    class EvidencePanel : public juce::Component
    {
    public:
        EvidencePanel();
        void paint(juce::Graphics& g) override;

        /** Actualiza los datos de metering desde el AudioAnalyzer. */
        void updateFromAnalyzer(const AudioAnalyzer& analyzer);

        /** Actualiza datos de crest factor desde TrackDynamicsAnalyzer. */
        void updateCrestData(float peak, float rms, float crestFactor);

        /** Actualiza datos de vectorscope/correlación. */
        void updateSpatialData(float correlation, float width);

        /** Actualiza el match score para MasterCheck. */
        void updateMatchScore(float score);

        /** Setea el estado de coach para escoger la visualización. */
        void setCoachRoomState(CoachRoomState state) { coachState_ = state; }

        /** Cache de banda espectral para dibujar spectrum simple. */
        float bandEnergies_[60] = {0.0f};
        int numBands_ = 0;
        float highlightedFreq_ = 0.0f;
        juce::String highlightLabel_;

    private:
        void drawVuMeter(juce::Graphics& g, juce::Rectangle<float> area);
        void drawMiniSpectrum(juce::Graphics& g, juce::Rectangle<float> area);
        void drawCrestGauge(juce::Graphics& g, juce::Rectangle<float> area);
        void drawVectorscope(juce::Graphics& g, juce::Rectangle<float> area);
        void drawMatchScore(juce::Graphics& g, juce::Rectangle<float> area);

        CoachRoomState coachState_ = CoachRoomState::GainStaging;

        // ─── Meter data cache ───────────────────────────────────────────────
        float vuLeft_ = -80.0f, vuRight_ = -80.0f;
        float peakLeft_ = -80.0f, peakRight_ = -80.0f;

        // ─── Crest data cache ───────────────────────────────────────────────
        float crestPeak_ = -80.0f, crestRms_ = -80.0f;
        float crestFactor_ = 0.0f;

        // ─── Spatial data cache ─────────────────────────────────────────────
        float correlation_ = 1.0f;
        float stereoWidth_ = 0.5f;

        // ─── Match score cache ──────────────────────────────────────────────
        float matchScore_ = 0.0f;
    };
} // namespace mixcoach

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  PluginSuggestionGroup — Grupo de sugerencias de plugins por tier
    // ═══════════════════════════════════════════════════════════════════════════
    struct PluginSuggestionGroup
    {
        juce::String problemTitle;
        juce::String trackName;
        float severity = 0.0f;

        struct TierSuggestion {
            juce::String pluginName;
            juce::String tier;
            juce::String actionText;
            bool isUserHas = false;
        };

        std::vector<TierSuggestion> suggestions;

        [[nodiscard]] bool isValid() const noexcept
        {
            return !suggestions.empty();
        }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  MasterCheckCardData — Datos para la tarjeta de comparación final
    // ═══════════════════════════════════════════════════════════════════════════
    struct MasterCheckCardData
    {
        float matchScore = 0.0f;
        int sessionDurationMinutes = 0;

        struct Gap {
            juce::String bandName;
            float deviationDb;
            juce::String recommendation;
        };

        std::vector<Gap> gaps;
        juce::String overallRecommendation;

        [[nodiscard]] bool isValid() const noexcept
        {
            return matchScore > 0.0f;
        }

        [[nodiscard]] juce::String ratingEmoji() const noexcept
        {
            if (matchScore >= 0.90f) return "++";  // excelente
            if (matchScore >= 0.75f) return "OK";   // bueno
            if (matchScore >= 0.50f) return "--";   // regular
            return "·";                              // bajo
        }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  ReverbCardData — Datos para la tarjeta inline de sugerencia de reverb
    // ═══════════════════════════════════════════════════════════════════════════
    struct ReverbCardData
    {
        float preDelayMs = 40.0f;
        float decaySec = 1.8f;
        float highCutHz = 7000.0f;
        float mixPct = 18.0f;
        juce::String genre;
        juce::String algorithm;
        std::vector<juce::String> trackNames;
        std::vector<juce::String> trackRoles;

        [[nodiscard]] bool isValid() const noexcept
        {
            return !trackNames.empty() && decaySec > 0.0f && genre.isNotEmpty();
        }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  CorrectionCardData — Datos para la tarjeta de corrección/verificación
    // ═══════════════════════════════════════════════════════════════════════════
    struct CorrectionCardData
    {
        juce::String trackName;
        juce::String problemTitle;
        juce::String action;
        juce::String domain;
        float beforeValue = 0.0f;
        float targetValue = 0.0f;
        float frequencyHz = 0.0f;
        int spectralBand  = -1;
        juce::String verifyMetric;

        enum class Status : uint8_t
        {
            Pending, Applied, Verifying, Verified, Partial, Failed, OverApplied, Skipped, InvalidatedBySection
        };
        Status status = Status::Pending;

        float verifiedDelta = 0.0f;
        float verifiedAfter = 0.0f;
        juce::String metricLabel;  // "Peak", "RMS", "Crest", "Correlation" — qué métrica se verificó
        juce::String feedbackMessage;
        juce::String verifiedTrackName; // Nombre del track verificado ("Kick", "Voz", etc.)

        [[nodiscard]] bool isValid() const noexcept { return trackName.isNotEmpty() && action.isNotEmpty(); }
        [[nodiscard]] bool isResolved() const noexcept { return status >= Status::Verified; }
        [[nodiscard]] static const char* statusIcon(Status s) noexcept
        {
            switch (s) {
                case Status::Pending:      return "·";    // pendiente
                case Status::Applied:      return ">";    // aplicado
                case Status::Verifying:    return "?";    // verificando
                case Status::Verified:     return "OK";   // verificado
                case Status::Partial:      return "~";    // parcial
                case Status::Failed:       return "X";    // fallido
                case Status::OverApplied:  return "++";   // sobregirado
                case Status::Skipped:      return "-";    // saltado
                case Status::InvalidatedBySection: return "!"; // invalidado
                default:                           return "";
            }
        }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  ChatBubble — Datos de un mensaje individual
    // ═══════════════════════════════════════════════════════════════════════════
    /**
     * Estructura que cachea los bounds de cada tarjeta de plugin renderizada,
     * para poder hacer hit-test en mouseDown.
     */
    struct PluginCardHitArea
    {
        juce::Rectangle<float> containerBounds;
        std::vector<juce::Rectangle<float>> cardBounds;
        std::vector<juce::String> pluginNames;
    };

    struct ChatBubble
    {
        // ─── Entrance animation (slide-up + fade-in + scale) ──────────────────
        bool animateIn = false;
        int64_t animStartMs = 0;
        static constexpr int kAnimDurationMs = 250;
        static constexpr float kAnimSlidePx = 8.0f;
        static constexpr float kAnimStartScale = 0.97f;
        juce::String text;
        juce::String tag; // Etiqueta opcional ("TIP", "WARN", etc.) — vacío = sin tag
        juce::String timestamp;
        bool isUser;
        bool isSystem = false; // true = mensaje automático del sistema (compacto, dimmed)

        // ─── Track group card ──────────────────────────────────────────────────
        bool isTrackGroupCard = false;
        TrackProblemGroup trackGroup;
        std::vector<TrackProblemData> trackProblems;

        // ─── Reverb card ───────────────────────────────────────────────────────
        bool isReverbCard = false;
        ReverbCardData reverbData;

        // ─── Plugin suggestion card (3 tiers: Native/Free/Premium) ───────────────
        bool isPluginSuggestionCard = false;
        PluginSuggestionGroup pluginSuggestionData;

        // ─── Master Check card (match score + gaps) ─────────────────────────────
        bool isMasterCheckCard = false;
        MasterCheckCardData masterCheckData;

        // ─── Correction card (Correction→Verify cycle) ─────────────────────────
        bool isCorrectionCard = false;
        CorrectionCardData correctionData;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  ChatMessagesComponent — Renderiza burbujas de chat en paint()
    //  Cada burbuja se dibuja con estilo diferenciado: coach vs usuario.
    //  Altura calculada dinámicamente según el texto y el ancho disponible.
    // ═══════════════════════════════════════════════════════════════════════════
    class ChatMessagesComponent : public juce::Component,
                                      private juce::Timer
    {
    public:
        ChatMessagesComponent();

        void addMessage(const juce::String& text, const juce::String& tag = {});
        void addUserMessage(const juce::String& text);
        void addSystemMessage(const juce::String& text);
        void addTrackGroupCard(const TrackProblemGroup& group);
        void addReverbCard(const ReverbCardData& data);
        void addPluginSuggestionCard(const PluginSuggestionGroup& group);
        void addMasterCheckCard(const MasterCheckCardData& data);
        void addCorrectionCard(const CorrectionCardData& data);
        void clear();

        // ─── Toggle mensajes del sistema ─────────────────────────────────────
        void setShowSystemMessages(bool show) noexcept
        {
            showSystemMessages_ = show;
            resized();
            repaint();
        }

        bool getShowSystemMessages() const noexcept { return showSystemMessages_; }

        int getMessageCount() const noexcept { return (int)messages_.size(); }
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

        // ═══ FASE 5: Option cards clicables ═══════════════════════════════
        /** Callback cuando se hace clic en una tarjeta de plugin.
            Recibe el nombre del plugin seleccionado y el tier label. */
        std::function<void(const juce::String& pluginName, const juce::String& tierLabel)> onPluginCardClicked;

        void mouseDown(const juce::MouseEvent& e) override
        {
            // Hit-test contra las tarjetas de plugin cacheadas en paint()
            if (!onPluginCardClicked || pluginCardHitAreas_.empty())
                return;

            auto pos = e.position;
            for (const auto& snap : pluginCardHitAreas_) {
                if (!snap.containerBounds.contains(pos))
                    continue;

                for (size_t ci = 0; ci < snap.cardBounds.size() && ci < snap.pluginNames.size(); ++ci) {
                    if (snap.cardBounds[ci].contains(pos)) {
                        auto& name = snap.pluginNames[ci];
                        if (name.isNotEmpty()) {
                            onPluginCardClicked(name, {}); // tier label se infiere del index
                            return;
                        }
                    }
                }
            }
        }

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

        // ═══ FASE 5: Option cards hit cache ═══════════════════════════════
        std::vector<PluginCardHitArea> pluginCardHitAreas_;

        float getBubbleHeight(const ChatBubble& bubble, float maxWidth) const;

        void drawCoachBubble(juce::Graphics& g, juce::Rectangle<float> bubbleBounds, const ChatBubble& msg);
        /** Dibuja la burbuja del coach con un cursor intermitente al final del texto. */
        void drawCoachBubbleStreaming(juce::Graphics& g, juce::Rectangle<float> bubbleBounds, const ChatBubble& msg);
        void drawUserBubble(juce::Graphics& g, juce::Rectangle<float> bubbleBounds, const ChatBubble& msg);
        /** Dibuja un mensaje del sistema en formato compacto y dimmed (sin burbuja). */
        void drawSystemMessage(juce::Graphics& g, juce::Rectangle<float> bounds, const ChatBubble& msg);
        void drawWelcomeCard(juce::Graphics& g, juce::Rectangle<float> bounds);
        void drawTypingIndicator(juce::Graphics& g, juce::Rectangle<float> bounds);

        // juce::Timer
        void timerCallback() override;
        /** Pausa/restaura el timer segun la visibilidad. */
        void visibilityChanged() override;
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

    // ═══════════════════════════════════════════════════════════════════════════
    //  Free function declarations for inline card rendering
    //  (defined in ChatMessagesComponent_InlineCards.cpp)
    // ═══════════════════════════════════════════════════════════════════════════
    float drawReverbCard(juce::Graphics& g, juce::Rectangle<float> bounds, const ChatBubble& msg);
    float drawTrackGroupCard(juce::Graphics& g, juce::Rectangle<float> bounds, const ChatBubble& msg, bool isGrouped);
    float drawPluginSuggestionCard(juce::Graphics& g, juce::Rectangle<float> bounds, const ChatBubble& msg, std::vector<PluginCardHitArea>* hitCache = nullptr);
    float drawMasterCheckCard(juce::Graphics& g, juce::Rectangle<float> bounds, const ChatBubble& msg);
    float drawCorrectionCard(juce::Graphics& g, juce::Rectangle<float> bounds, const ChatBubble& msg);

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
        void lookAndFeelChanged() override;

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

        // ─── Inline card helpers ─────────────────────────────────────────
        void addTrackGroupCard(const TrackProblemGroup& group);
        void addReverbCard(const ReverbCardData& data);
        void addPluginSuggestionCard(const PluginSuggestionGroup& group);
        void addMasterCheckCard(const MasterCheckCardData& data);
        void addCorrectionCard(const CorrectionCardData& data);

        // ─── NavigationShell integration methods ───────────────────────────
        /** @return referencia al MessengerListComponent para acceso directo. */
        MessengerListComponent& getMessengerList() { return messengerList_; }

        /** Accessor for SessionPrepChecklist to wire item-checked callbacks. */
        SessionPrepChecklist& getSessionPrepChecklist() { return sessionPrepChecklist_; }

        /** @return referencia al MixMapComponent para acceso directo. */
        MixMapComponent& getMixMapComponent() { return mixMapComponent_; }

        /** @return pointer al componente según PanelId (para NavigationShell::getPanelComponent). */
        juce::Component* getPanelComponent(PanelId panelId) const noexcept;

        /** Establece el texto del saludo visible en el modo Intention. */
        void setGreetingText(const juce::String& text);

        /** Cambia la expresión facial del avatar (animación). */
        void setAvatarExpression(AvatarExpression exp);

        /** Activa/desactiva la animación de saludo del avatar. */
        void setAvatarWave(bool active);

        /** Activa animación de asentir del avatar por duración en ms. */
        void setAvatarNod(int64_t durationMs);

        /** Activa animación de señalar del avatar. */
        void setAvatarPoint();

        // ─── Setup flow methods ────────────────────────────────────────────
        CoachRoomState getCoachRoomState() const noexcept { return coachRoomState_; }

        // ─── Legacy wrappers — mantienen compatibilidad con NavigationShell ─
        bool isWelcomeMode() const noexcept { return isPreFullUI(coachRoomState_); }
        void setWelcomeMode(bool welcome);
        bool isShowModeCards() const noexcept { return coachRoomState_ == CoachRoomState::Intention; }
        void setShowModeCards(bool show);
        bool isShowGenreCards() const noexcept { return coachRoomState_ == CoachRoomState::Genre; }
        void setShowGenreCards(bool show);
        bool isShowSessionPrepCard() const noexcept { return coachRoomState_ == CoachRoomState::SessionPrep; }
        void setShowSessionPrepCard(bool show, CoachEngine* coach, SlotRegistry* registry);
        /** Overload: ocultar la tarjeta sin necesidad de punteros. */
        void setShowSessionPrepCard(bool show) { setShowSessionPrepCard(show, nullptr, nullptr); }
        bool isShowReferenceCards() const noexcept { return coachRoomState_ == CoachRoomState::ReferenceStage && refOnboardingCard_.isVisible(); }
        void setShowReferenceCards(bool show);
        bool isInlineReferenceDropZone() const noexcept { return coachRoomState_ == CoachRoomState::ReferenceStage; }
        void setInlineReferenceDropZone(bool show);
        bool isInlineMessengerStatus() const noexcept { return coachRoomState_ == CoachRoomState::MessengerStage; }
        void setInlineMessengerStatus(bool detected, int count, const std::vector<juce::String>& trackNames);
        void setShowSuggestionsOverride(bool show);
        bool isShowSuggestionsOverride() const noexcept { return showSuggestionsOverride_; }
        void setShowCoachingGuide(bool show);
        bool isShowCoachingGuide() const noexcept { return showCoachingGuide_; }
        bool isShowTrackProblemCard() const noexcept { return showTrackProblemCard_; }
        bool isShowMixMap() const noexcept { return showMixMap_; }
        void showQuickReplies(const std::vector<juce::String>& replies);
        void hideQuickReplies();
        void showReferenceAnalysisProgress(bool show);
        void showSessionScanCard(int trackCount, const std::vector<juce::String>& trackNames);
        void hideSessionScanCard();
        void showPanelMixMap();
        void showPanelReference();
        void showPanelMessengers();
        void updateTrackProblemCards(CoachEngine& coach);
        void showTrackProblemCard(const TrackProblemGroup& group);
        void hideTrackProblemCard();
        void highlightTrackMention(const juce::String& track);
        void setRevealManager(PanelRevealManager* manager);
        void performSprint1Layout();

        /** Acceso al EvidencePanel desde NavigationShell para alimentar datos. */
        EvidencePanel& getEvidencePanel() noexcept { return evidencePanel_; }

        /** Acceso al CoachingEvidenceHost desde NavigationShell. */
        CoachingEvidenceHost& getCoachingEvidenceHost() noexcept { return coachingEvidenceHost_; }

        // ─── Subcomponent accessors: inline cards, coaching, plugins ────────
        CoachingGuideWidget& getCoachingGuide() { return coachingGuide_; }
        ReferenceOnboardingCard& getRefOnboardingCard() { return refOnboardingCard_; }
        ReferenceAnalysisProgressCard& getRefAnalysisProgressCard() { return refAnalysisProgressCard_; }
        QuickReplyBar& getQuickReplyBar() { return quickReplyBar_; }
        bool hasQuickReplies() const { return quickReplyBar_.hasReplies(); }
        MixMapDetailPanel& getMixMapDetailPanel() { return mixMapDetailPanel_; }

        // ═══ Phase panel accessors ═══════════════════════════════════════
        GainStagingPanel& getGainStagingPanel() noexcept { return gainStagingPanel_; }
        const GainStagingPanel& getGainStagingPanel() const noexcept { return gainStagingPanel_; }
        EQPanel& getEQPanel() noexcept { return eqPanel_; }
        const EQPanel& getEQPanel() const noexcept { return eqPanel_; }
        CompressionPanel& getCompressionPanel() noexcept { return compressionPanel_; }
        const CompressionPanel& getCompressionPanel() const noexcept { return compressionPanel_; }
        SpacePanel& getSpacePanel() noexcept { return spacePanel_; }
        const SpacePanel& getSpacePanel() const noexcept { return spacePanel_; }
        MasterCheckPanel& getMasterCheckPanel() noexcept { return masterCheckPanel_; }
        const MasterCheckPanel& getMasterCheckPanel() const noexcept { return masterCheckPanel_; }

        /** Acceso al PluginConfirmationPanel (Incremento 3d). */
        PluginConfirmationPanel& getPluginConfirmationPanel() noexcept { return pluginConfirmationPanel_; }
        const PluginConfirmationPanel& getPluginConfirmationPanel() const noexcept { return pluginConfirmationPanel_; }

        AutomationPanel& getAutomationPanel() noexcept { return automationPanel_; }
        const AutomationPanel& getAutomationPanel() const noexcept { return automationPanel_; }

        /** Refresca la tabla de Gain Staging desde el engine.
            Toma los datos de CoachEngine::analyzeAllTracksGain() y pobla el panel. */
        void refreshGainStagingPanel(CoachEngine& coach);
        juce::StringArray getAppliedPlugins() const;

        // ─── Callbacks para setup flow ───────────────────────────────────────
        std::function<void()> onStartReferenceAnalysis;
        std::function<void()> onReferenceAnalysisComplete;
        std::function<void()> onMixMapConfirmed;
        std::function<void()> onSessionPrepConfirmed;
        std::function<void()> onSkipSetup;
        std::function<void(const juce::String&, const juce::String&)> onGenreCardSelected;
        std::function<bool(const juce::String&)> onCheckReferenceCache;
        std::function<void(const juce::String&)> onTrackHighlightRequest;
        std::function<void()> onSessionScanComplete;
        std::function<void(CorrectionCardData&)> onCorrectionApplied;

        /** Callback cuando se hace clic en una opción de plugin inline.
            NavigationShell lo cablea a narrativeDirector_->onOptionSelected()
            para disparar el verify loop directamente (sin round-trip al LLM). */
        std::function<void(const juce::String& pluginName)> onPluginCardClicked;

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
            // Deshabilitar input durante typing del coach (verify/celebrate)
            chatInput_.setEnabled(!isTyping);
            resized();
        }

        /** Controla si el input de chat está habilitado.
            Se usa desde ChatMessageSequencer para deshabilitar durante verificación. */
        void setChatInputEnabled(bool enabled)
        {
            chatInput_.setEnabled(enabled);
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
        void setCoachRoomState(CoachRoomState state);

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

        /** Callback para notificar a NavigationShell que el smooth scroll
            necesita el timer activo (se dispara desde scrollChatToBottom()
            cuando inicia una animación de scroll suave). */
        std::function<void()> onSmoothScrollNeeded;

        /** Actualiza el estado de la conexión con Ollama en la UI. */
        void setOllamaStatus(bool connected, const juce::String& modelName);

    private:
        /** Ajusta el placeholder de chat al paso actual de la conversación guiada. */
        void updateChatPlaceholder(CoachRoomState state);

        /** Helper: desplaza el viewport del chat al fondo con animación suave.
            Usa SmoothScrollState con ease-out quad (~300ms) en vez de salto instantáneo.
            Si el usuario ya está cerca del fondo (< 30px), salta directo (no anima).
            Si el scroll manual está activo (usuario scrolleó hacia arriba), no anima. */
        void scrollChatToBottom()
        {
            // Forzar viewport a recalcular sus scroll ranges basados en el nuevo tamaño del contenido
            chatViewport_.resized();

            auto& scrollbar       = chatViewport_.getVerticalScrollBar();
            double contentHeight  = (double)chatMessages_.getTotalHeight();
            double viewportHeight = (double)chatViewport_.getHeight();
            double maxScroll      = juce::jmax(0.0, contentHeight - viewportHeight);
            double currentPos     = scrollbar.getCurrentRangeStart();
            double distToBottom   = maxScroll - currentPos;

            // Si está muy cerca del fondo (< 30px) o es el primer mensaje, salto directo
            if (distToBottom < 30.0 || chatMessages_.getMessageCount() <= 1) {
                scrollbar.setCurrentRange(maxScroll, viewportHeight, juce::sendNotificationSync);
                return;
            }

            // Iniciar scroll suave
            smoothScroll_.active = true;
            smoothScroll_.currentPos = currentPos;
            smoothScroll_.targetPos = maxScroll;
            smoothScroll_.elapsedFrames = 0;

            // Notificar a NavigationShell que necesita el timer
            if (onSmoothScrollNeeded)
                onSmoothScrollNeeded();
        }

        // ─── Estado del flujo UI (reemplaza 6 flags de setup) ─────────────
        CoachRoomState coachRoomState_ = CoachRoomState::Welcome;
        bool showCoachingGuide_ = false;          // NavigationShell lo controla independientemente
        bool showSuggestionsOverride_ = false;     // Override para chips de sugerencia
        int64_t lastTrackProblemCardsUs_ = 0;   // Timestamp cooldown (120s)
        bool showTrackProblemCard_ = false;

        // ─── Setup flow subcomponents ──────────────────────────────────────
        ModeSelectionCard modeCardMix_;
        ModeSelectionCard modeCardMaster_;
        GenreSelectionGrid genreSelectionGrid_;
        RobotAvatarComponent setupAvatar_;
        juce::Label greetingLabel_;  // "\u00a1Bienvenido [name]! \u00bfQu\u00e9 haremos hoy?" — visible solo en Intention
        ReferenceOnboardingCard refOnboardingCard_;
        ReferenceAnalysisProgressCard refAnalysisProgressCard_;
        SessionPrepChecklist sessionPrepChecklist_;
        SessionScanCard sessionScanCard_;
        TrackProblemCard trackProblemCard_;
        QuickReplyBar quickReplyBar_;
        CoachingGuideWidget coachingGuide_;
        MixMapDetailPanel mixMapDetailPanel_;

        // ═══ Phase panels ═══════════════════════════════════════════════
        GainStagingPanel gainStagingPanel_;
        EQPanel eqPanel_;
        CompressionPanel compressionPanel_;
        SpacePanel spacePanel_;
        MasterCheckPanel masterCheckPanel_;
        AutomationPanel automationPanel_;

        // ═══ Incremento 3d: Panel de confirmación de inserts por pista ═══
        PluginConfirmationPanel pluginConfirmationPanel_;

        // ═══ CoachingEvidenceHost — Unifica panel derecho en coaching ═══
        CoachingEvidenceHost coachingEvidenceHost_;

        // ─── Paneles ──────────────────────────────────────────────────────────
        MasterMeterPanel masterMeterPanel_;
        ReferencePanelComponent refPanel_;
        juce::Viewport messengerViewport_;
        MessengerListComponent messengerList_;
        DividerBar dividerBar_;

        // ─── Placeholder para área de tracks vacía (sin Messengers) ────────────
        juce::Label messengerPlaceholder_;

        // ═══ Smooth Scroll API (público para NavigationShell) ══════════════
      public:
        /** Avanza la animación de scroll suave.
            Se llama desde NavigationShell::timerCallback() cada frame.
            Retorna true si la animación sigue activa. */
        bool advanceSmoothScroll();

      private:
        struct SmoothScrollState {
            bool active = false;
            double currentPos = 0.0;    // Posición actual del scroll
            double targetPos = 0.0;      // Posición destino (fondo del chat)
            static constexpr int kDurationFrames = 18;  // ~300ms at 60fps
            int elapsedFrames = 0;
        };
        SmoothScrollState smoothScroll_;

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

        // ─── Evidence panel para split-view coaching ─────────────────────
        EvidencePanel evidencePanel_;

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
        juce::Label footerPersonaLabel_;    // Personalidad del coach (clickable)
        juce::Label footerPlatformLabel_;   // Plataforma target LUFS (clickable)
        juce::Label footerLlmStatusLabel_;  // Estado del LLM (🟢Conectado / 🟡Fallback / 🔴Offline)

        /** Actualiza el label de estado LLM en el footer. */
        void updateLlmStatusLabel(mixcoach::CoachEngine::LlmStatus status);

        /** Actualiza el label de plataforma target LUFS en el footer. */
        void updatePlatformLabel(mixcoach::MasterDestination dest);

        // ─── Suggestion chips state ───────────────────────────────────────────
        std::vector<juce::String> suggestions_;
        std::vector<juce::Rectangle<int>> suggestionChipBounds_;

        // ─── Toggle state for system messages ──────────────────────────────
        bool showSystemMessages_ = false;
        juce::Rectangle<int> systemToggleBounds_;
        int hoveredSystemToggle_ = -1;

        // ─── Applied plugins tracking (para EndOfSessionComponent) ─────────
        juce::StringArray appliedPlugins_;

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

        // ─── Split layout flag: true dibuja panel izquierdo+derecho con divider.
        //     false = solo fondo oscuro (usado durante mode cards).
        bool showSplitLayout_ = true;

        // ─── Suggestion chip hover state ───────────────────────────────────
        int hoveredSuggestionChip_ = -1;

        void mouseDown(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;

        // ─── Bidirectional selection sync between MixMap and MessengerList ──────
        void syncSelectionToMixMap(int slotIndex);
        void syncSelectionToMessengerList(int slotIndex);

        // TextEditor::Listener
        void textEditorReturnKeyPressed(juce::TextEditor& editor) override;

        // ═══ Atajos 4.1: Keyboard shortcuts (Ctrl+Enter=/ok, Ctrl+/=/why) ═════
        bool keyPressed(const juce::KeyPress& key) override;

        // ═══ Atajos 4.1: Microphone button ═══════════════════════════════════
        juce::TextButton micButton_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixCoachPanel)
    };

} // namespace mixcoach
