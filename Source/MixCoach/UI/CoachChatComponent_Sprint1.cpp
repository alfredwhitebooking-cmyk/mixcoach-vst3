#include "CoachChatComponent.h"
#include "QuickReplyBar.h"
#include "ReferenceOnboardingCard.h"
#include "ReferenceAnalysisProgressCard.h"
#include "SessionScanCard.h"
#include "SessionPrepChecklist.h"
#include "MixMapDetailPanel.h"
#include "TrackProblemCard.h"
#include "TrackProblemBuilder.h"
#include "GenreSelectionGrid.h"
#include "../engine/PanelRevealManager.h"
#include "../engine/CoachEngine.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  CoachRoomState method + legacy setters (replaced 8 individual flags)
    // ═══════════════════════════════════════════════════════════════════════════

    void MixCoachPanel::setCoachRoomState(CoachRoomState state)
    {
        coachRoomState_ = state;
        updateChatPlaceholder(state);
        bool isSetup = isPreFullUI(state);
        bool isCoaching = isCoachingState(state);

        // ─── General visibility: setup vs coaching ───────────────────────
        // Durante coaching: SOLO el panel de fase activo + chat son visibles.
        // Setup panels, messengers, meters, reference, y toolbar se ocultan.
        refPanel_.setVisible(!isCoaching);
        referencesSectionLabel_.setVisible(!isCoaching);
        masterMeterPanel_.setVisible(!isCoaching);
        tracksSectionLabel_.setVisible(!isCoaching);
        messengerList_.setVisible(!isCoaching);
        ollamaStatusLabel_.setVisible(!isCoaching);
        ollamaRetryBtn_.setVisible(!isCoaching);
        messengerViewport_.setVisible(!isCoaching);
        dividerBar_.setVisible(!isCoaching);
        messengerPlaceholder_.setVisible(!isCoaching);
        mixMapComponent_.setVisible(!isCoaching);
    evidencePanel_.setCoachRoomState(state);

    // Unificar en CoachingEvidenceHost para split-view
    if (isCoaching) {
        coachingEvidenceHost_.setActivePhase(state);
        coachingEvidenceHost_.setVisible(true);
        // El host gestiona la evidencia interna; ocultar legacy
        evidencePanel_.setVisible(false);
    } else {
        coachingEvidenceHost_.setVisible(false);
        evidencePanel_.setVisible(false);
    }

        // ─── P2 (Intention): Modo como chips inline en QuickReplyBar ─────
        // ═══ CHAT-CONTROLLED UX: NO hay tarjetas ModeSelectionCard.
        // El Coach dice "¿Qué haremos hoy?" y aparecen chips en el chat.
        // La selección se maneja desde NavigationShell::onSuggestionClicked.
        if (state == CoachRoomState::Intention) {
            chatViewport_.setVisible(true);
            chatInput_.setVisible(true);
            sendButton_.setVisible(true);
            // setupAvatar_ se gestiona desde NavigationShell
        } else {
            modeCardMix_.setVisible(false);
            modeCardMaster_.setVisible(false);
        }

        // ─── P3 (Genre): Género como chips inline en QuickReplyBar ──────
        // ═══ CHAT-CONTROLLED UX: NO hay genre grid. Chips desde el chat.
        if (state == CoachRoomState::Genre) {
            chatViewport_.setVisible(true);
            chatInput_.setVisible(true);
            sendButton_.setVisible(true);
            // El grid de género no se muestra — los chips aparecen en el chat
            genreSelectionGrid_.setVisible(false);

            // NOTA: Con el chip redesign, la selecci\xC3\xB3n es inmediata
            //       via onGenreSelected. Ya no hay bot\xC3\xB3n Confirmar.
            if (genreSelectionGrid_.onGenreSelected == nullptr) {
                genreSelectionGrid_.onGenreSelected =
                    [this](const juce::String& genreKey, const juce::String& genreLabel) {
                        if (setupAvatar_.isVisible()) {
                            setupAvatar_.setExpressionWithDecay(AvatarExpression::Happy, 3000);
                            setupAvatar_.triggerNod(600);
                        }
                        if (onGenreCardSelected)
                            onGenreCardSelected(genreKey, genreLabel);
                    };
            }
            if (genreSelectionGrid_.onGenreSelected == nullptr) {
                genreSelectionGrid_.onGenreSelected =
                    [this](const juce::String&, const juce::String& genreLabel) {
                        if (setupAvatar_.isVisible())
                            setupAvatar_.triggerNod(400);
                        addUserMessage(juce::String(juce::CharPointer_UTF8("\xE2\x96\xB8 ")) + genreLabel);  // ▸
                    };
            }
        } else {
            genreSelectionGrid_.setVisible(false);
        }

        // ─── P4 (Reference): Inline reference drop zone ──────────────────
        // ═══ Chat-controlled UX: el chat SIEMPRE visible.
        if (state == CoachRoomState::ReferenceStage) {
            addAndMakeVisible(refOnboardingCard_);
            refOnboardingCard_.reset();
            refOnboardingCard_.setVisible(true);
            chatViewport_.setVisible(true);
            chatInput_.setVisible(true);
            sendButton_.setVisible(true);

            if (refOnboardingCard_.onFileDropped == nullptr) {
                refOnboardingCard_.onFileDropped = [this](const juce::String& path) {
                    if (onReferenceFileAdded) onReferenceFileAdded(path);
                };
            }
            if (refOnboardingCard_.onURLAdded == nullptr) {
                refOnboardingCard_.onURLAdded = [this](const juce::String& url) {
                    if (onReferenceURLAdded) onReferenceURLAdded(url, url);
                };
            }
            if (refOnboardingCard_.onAnalyzeClicked == nullptr) {
                refOnboardingCard_.onAnalyzeClicked = [this]() {
                    if (onStartReferenceAnalysis) onStartReferenceAnalysis();
                };
            }
        } else {
            refOnboardingCard_.setVisible(false);
            refOnboardingCard_.onFileDropped = nullptr;
            refOnboardingCard_.onURLAdded = nullptr;
            refOnboardingCard_.onAnalyzeClicked = nullptr;
        }

        // ─── P6 (MixMapStage): Full-screen mix map + avatar ──────────────
        // ═══ Chat-controlled UX: el chat SIEMPRE visible.
        if (state == CoachRoomState::MixMapStage) {
            addAndMakeVisible(mixMapComponent_);
            mixMapComponent_.setVisible(true);
            mixMapComponent_.toFront(true);
            chatViewport_.setVisible(true);
            chatInput_.setVisible(true);
            sendButton_.setVisible(true);
            setupAvatar_.setExpression(AvatarExpression::Happy);
            setupAvatar_.setVisible(true);
            setupAvatar_.toFront(true);
        } else if (state != CoachRoomState::Welcome) {
            // Only hide mix map if not in a mix map state
            showMixMap_ = false;
        }

        if (state == CoachRoomState::MessengerStage) {
            addSystemMessage(juce::String(juce::CharPointer_UTF8("[INFO] **Messengers detectados**\n\nLos Messengers estan activos en tu sesion.")));
        }

        resized();
        repaint();
    }

    // ─── Legacy setters (kept for NavigationShell compatibility) ───────

    void MixCoachPanel::setWelcomeMode(bool welcome)
    {
        setCoachRoomState(welcome ? CoachRoomState::Welcome : CoachRoomState::GainStaging);
    }

    void MixCoachPanel::setShowModeCards(bool show)
    {
        if (show) {
            setCoachRoomState(CoachRoomState::Intention);

            // ═══ CHAT-CONTROLLED UX: QuickReplyBar chips en vez de ModeSelectionCard ═══
            // El Coach ya dijo "¿Qué haremos hoy?" y aparecen chips Mezclar/Masterizar.
            // No hay tarjetas ModeSelectionCard — el clic en chip va directo a
            // NavigationShell::onSuggestionClicked sin intermediarios.
            showQuickReplies({"Mezclar", "Masterizar"});

            // Wire onReplySelected → onSuggestionClicked (mecanismo probado)
            quickReplyBar_.onReplySelected = [this](const juce::String& text) {
                LogHelper::writeToLog("[FLOW] QuickReplyBar chip clicked: " + text);
                if (onSuggestionClicked) {
                    LogHelper::writeToLog("[FLOW] onSuggestionClicked called with: " + text);
                    onSuggestionClicked(text);
                }
                // Limpiar callback para evitar doble disparo
                quickReplyBar_.onReplySelected = nullptr;
                hideQuickReplies();
            };

            // Show the setup avatar and greeting
            addAndMakeVisible(setupAvatar_);
            setupAvatar_.setExpression(AvatarExpression::Neutral);
            setupAvatar_.setVisible(true);
            greetingLabel_.setVisible(true);

            // ═══ OCULTAR TODO excepto avatar, greeting y chips ═══
            chatInput_.setVisible(false);
            sendButton_.setVisible(false);
            chatViewport_.setVisible(false);
            refPanel_.setVisible(false);
            referencesSectionLabel_.setVisible(false);
            masterMeterPanel_.setVisible(false);
            tracksSectionLabel_.setVisible(false);
            messengerList_.setVisible(false);
            ollamaStatusLabel_.setVisible(false);
            ollamaRetryBtn_.setVisible(false);
            messengerViewport_.setVisible(false);
            dividerBar_.setVisible(false);
            messengerPlaceholder_.setVisible(false);
            mixMapComponent_.setVisible(false);
            evidencePanel_.setVisible(false);
            coachingEvidenceHost_.setVisible(false);
            coachingGuide_.setVisible(false);
            genreSelectionGrid_.setVisible(false);
            sessionPrepChecklist_.setVisible(false);
            modeCardMix_.setVisible(false);
            modeCardMaster_.setVisible(false);

            // ═══ Desactivar split layout (solo fondo oscuro) ═══
            showSplitLayout_ = false;
        } else {
            modeCardMix_.setVisible(false);
            modeCardMaster_.setVisible(false);
            setupAvatar_.setVisible(false);
            greetingLabel_.setVisible(false);
            hideQuickReplies();

            // Restaurar input + chat viewport
            chatInput_.setVisible(true);
            sendButton_.setVisible(true);
            chatViewport_.setVisible(true);
            // ═══ Reactivar split layout para la UI completa ═══
            showSplitLayout_ = true;
        }
        resized();
        repaint();
    }

    void MixCoachPanel::setShowGenreCards(bool show)
    {
        if (show) {
            // ═══ MISMO PATRÓN QUE setShowModeCards(true) ═══
            setCoachRoomState(CoachRoomState::Genre);

            // ─── Init avatar ───────────────────────────────────────────────────
            addAndMakeVisible(setupAvatar_);
            setupAvatar_.setExpression(AvatarExpression::Thinking);
            setupAvatar_.setVisible(true);

            // ─── Show greeting ─────────────────────────────────────────────────
            greetingLabel_.setVisible(true);

            // ─── Show genre grid (like mode cards) ───────────────────────────
            addAndMakeVisible(genreSelectionGrid_);
            genreSelectionGrid_.reset();
            genreSelectionGrid_.restartAnimations();
            genreSelectionGrid_.setVisible(true);

            // ═══ OCULTAR TODO excepto avatar, greeting y genre grid ═══
            chatInput_.setVisible(false);
            sendButton_.setVisible(false);
            chatViewport_.setVisible(false);

            // Ocultar todos los paneles que sobran
            refPanel_.setVisible(false);
            referencesSectionLabel_.setVisible(false);
            masterMeterPanel_.setVisible(false);
            tracksSectionLabel_.setVisible(false);
            messengerList_.setVisible(false);
            ollamaStatusLabel_.setVisible(false);
            ollamaRetryBtn_.setVisible(false);
            messengerViewport_.setVisible(false);
            dividerBar_.setVisible(false);
            messengerPlaceholder_.setVisible(false);
            mixMapComponent_.setVisible(false);
            evidencePanel_.setVisible(false);
            coachingEvidenceHost_.setVisible(false);
            coachingGuide_.setVisible(false);
            modeCardMix_.setVisible(false);
            modeCardMaster_.setVisible(false);
            sessionPrepChecklist_.setVisible(false);
            if (quickReplyBar_.isVisible()) {
                quickReplyBar_.setVisible(false);
            }

            // ═══ Desactivar split layout (solo fondo oscuro) ═══
            showSplitLayout_ = false;
        } else {
            genreSelectionGrid_.setVisible(false);
            setupAvatar_.setVisible(false);
            greetingLabel_.setVisible(false);

            // Restaurar input + chat viewport
            chatInput_.setVisible(true);
            sendButton_.setVisible(true);
            chatViewport_.setVisible(true);

            // ═══ Reactivar split layout ═══
            showSplitLayout_ = true;
        }
        resized();
        repaint();
    }

    void MixCoachPanel::setShowSessionPrepCard(bool show,
                                                CoachEngine* coach,
                                                SlotRegistry* registry)
    {
        if (show && coach != nullptr && registry != nullptr) {
            // ═══ FULL-SCREEN como Mode/Genre/Reference ═══
            coachRoomState_ = CoachRoomState::SessionPrep;

            // ═══ LIMPIAR sugerencias de la etapa anterior (ReferenceStage) ═══
            //    Si no se limpian, los chips "Saltar referencia" siguen activos
            //    y se renderizan ALREDEDOR de la tarjeta centrada, creando
            //    textos fantasma como "ecesito", "no obtuve", "siguiente".
            suggestions_.clear();
            showSuggestionsOverride_ = false;

            // ─── Init avatar ───────────────────────────────────────────────────
            addAndMakeVisible(setupAvatar_);
            setupAvatar_.setExpression(AvatarExpression::Encouraging);
            setupAvatar_.setVisible(true);

            // ─── Show greeting ─────────────────────────────────────────────────
            greetingLabel_.setVisible(true);

            // ─── Show session prep checklist ─────────────────────────────────
            addAndMakeVisible(sessionPrepChecklist_);
            sessionPrepChecklist_.setRefs(coach, registry);
            sessionPrepChecklist_.setVisible(true);

            sessionPrepChecklist_.onContinue = [this]() {
                if (onSessionPrepConfirmed) onSessionPrepConfirmed();
            };

            // ═══ OCULTAR TODO excepto avatar, greeting y checklist ═══
            chatInput_.setVisible(false);
            sendButton_.setVisible(false);
            chatViewport_.setVisible(false);

            // Ocultar todos los paneles que sobran
            refPanel_.setVisible(false);
            referencesSectionLabel_.setVisible(false);
            masterMeterPanel_.setVisible(false);
            tracksSectionLabel_.setVisible(false);
            messengerList_.setVisible(false);
            ollamaStatusLabel_.setVisible(false);
            ollamaRetryBtn_.setVisible(false);
            messengerViewport_.setVisible(false);
            dividerBar_.setVisible(false);
            messengerPlaceholder_.setVisible(false);
            mixMapComponent_.setVisible(false);
            evidencePanel_.setVisible(false);
            coachingEvidenceHost_.setVisible(false);
            coachingGuide_.setVisible(false);
            modeCardMix_.setVisible(false);
            modeCardMaster_.setVisible(false);
            genreSelectionGrid_.setVisible(false);
            refOnboardingCard_.setVisible(false);
            if (quickReplyBar_.isVisible()) {
                quickReplyBar_.setVisible(false);
            }

            // ═══ Desactivar split layout (solo fondo oscuro) ═══
            showSplitLayout_ = false;
        } else {
            sessionPrepChecklist_.setVisible(false);
            setupAvatar_.setVisible(false);
            greetingLabel_.setVisible(false);

            // Restaurar input + chat viewport
            chatInput_.setVisible(true);
            sendButton_.setVisible(true);
            chatViewport_.setVisible(true);

            // ═══ Reactivar split layout ═══
            showSplitLayout_ = true;
        }
        resized();
        repaint();
    }

    void MixCoachPanel::setShowReferenceCards(bool show)
    {
        if (show) {
            // ═══ MISMO PATRÓN QUE setShowModeCards / setShowGenreCards ═══
            setCoachRoomState(CoachRoomState::ReferenceStage);

            // ─── Init avatar ───────────────────────────────────────────────────
            addAndMakeVisible(setupAvatar_);
            setupAvatar_.setExpression(AvatarExpression::Encouraging);
            setupAvatar_.setVisible(true);

            // ─── Show greeting ─────────────────────────────────────────────────
            greetingLabel_.setVisible(true);

            // ─── Show reference onboarding card ──────────────────────────────
            addAndMakeVisible(refOnboardingCard_);
            refOnboardingCard_.reset();
            refOnboardingCard_.setVisible(true);

            // ═══ OCULTAR TODO excepto avatar, greeting y reference card ═══
            chatInput_.setVisible(false);
            sendButton_.setVisible(false);
            chatViewport_.setVisible(false);

            // Ocultar todos los paneles que sobran
            refPanel_.setVisible(false);
            referencesSectionLabel_.setVisible(false);
            masterMeterPanel_.setVisible(false);
            tracksSectionLabel_.setVisible(false);
            messengerList_.setVisible(false);
            ollamaStatusLabel_.setVisible(false);
            ollamaRetryBtn_.setVisible(false);
            messengerViewport_.setVisible(false);
            dividerBar_.setVisible(false);
            messengerPlaceholder_.setVisible(false);
            mixMapComponent_.setVisible(false);
            evidencePanel_.setVisible(false);
            coachingEvidenceHost_.setVisible(false);
            coachingGuide_.setVisible(false);
            modeCardMix_.setVisible(false);
            modeCardMaster_.setVisible(false);
            genreSelectionGrid_.setVisible(false);
            sessionPrepChecklist_.setVisible(false);
            if (quickReplyBar_.isVisible()) {
                quickReplyBar_.setVisible(false);
            }

            // ═══ Desactivar split layout (solo fondo oscuro) ═══
            showSplitLayout_ = false;
        } else {
            refOnboardingCard_.setVisible(false);
            refOnboardingCard_.onFileDropped = nullptr;
            refOnboardingCard_.onURLAdded = nullptr;
            refOnboardingCard_.onAnalyzeClicked = nullptr;
            setupAvatar_.setVisible(false);
            greetingLabel_.setVisible(false);

            // Restaurar input + chat viewport
            chatInput_.setVisible(true);
            sendButton_.setVisible(true);
            chatViewport_.setVisible(true);

            // ═══ Reactivar split layout ═══
            showSplitLayout_ = true;
        }
        resized();
        repaint();
    }

    void MixCoachPanel::setInlineReferenceDropZone(bool show)
    {
        if (show)
            setCoachRoomState(CoachRoomState::ReferenceStage);
        else {
            refOnboardingCard_.setVisible(false);
            refOnboardingCard_.onFileDropped = nullptr;
            refOnboardingCard_.onURLAdded = nullptr;
            refOnboardingCard_.onAnalyzeClicked = nullptr;
            resized();
            repaint();
        }
    }

    void MixCoachPanel::setInlineMessengerStatus(bool show,
                                                  int activeCount,
                                                  const std::vector<juce::String>& trackNames)
    {
        juce::ignoreUnused(trackNames);
        if (show && activeCount > 0) {
            coachRoomState_ = CoachRoomState::MessengerStage;
            addSystemMessage(juce::String(juce::CharPointer_UTF8("[INFO] **"))
                             + juce::String(activeCount)
                             + (activeCount == 1 ? " track" : " tracks")
                             + juce::String(juce::CharPointer_UTF8(" detectados**\n\nSe han encontrado "))
                             + juce::String(activeCount)
                             + juce::String(juce::CharPointer_UTF8(" Messengers activos en tu sesi\xC3\xB3n.")));
        }
        resized();
        repaint();
    }

    void MixCoachPanel::setShowSuggestionsOverride(bool show)
    {
        showSuggestionsOverride_ = show;
        resized();
        repaint();
    }

    void MixCoachPanel::setShowCoachingGuide(bool show)
    {
        showCoachingGuide_ = show;
        if (show) {
            // addAndMakeVisible ya se hizo en el constructor — solo mostrar el componente.
            coachingGuide_.setVisible(true);
            coachingGuide_.resized();
            coachingGuide_.repaint();
        } else {
            coachingGuide_.setVisible(false);
        }
        resized();
        repaint();
    }

    // ─── QuickReplyBar ─────────────────────────────────────────────────────

    void MixCoachPanel::showQuickReplies(const std::vector<juce::String>& replies)
    {
        addAndMakeVisible(quickReplyBar_);
        quickReplyBar_.setReplies(replies);
        if (!replies.empty()) {
            quickReplyBar_.setVisible(true);
        }
        resized();
        repaint();
    }

    void MixCoachPanel::hideQuickReplies()
    {
        if (quickReplyBar_.hasReplies() && !quickReplyBar_.isFadingOut()) {
            quickReplyBar_.onFadeOutComplete = [this]() {
                quickReplyBar_.setVisible(false);
                resized();
                repaint();
            };
            quickReplyBar_.startFadeOut();
        } else {
            quickReplyBar_.clearReplies();
            quickReplyBar_.setVisible(false);
            resized();
            repaint();
        }
    }

    // ─── Reference Analysis Progress ────────────────────────────────────────

    void MixCoachPanel::showReferenceAnalysisProgress(bool show)
    {
        if (show) {
            addAndMakeVisible(refAnalysisProgressCard_);
            refAnalysisProgressCard_.reset();
            refAnalysisProgressCard_.startAnimation();
            refAnalysisProgressCard_.setVisible(true);
        } else {
            refAnalysisProgressCard_.setVisible(false);
        }
        resized();
        repaint();
    }

    // ─── Avatar Expressions ────────────────────────────────────────────────

    void MixCoachPanel::setAvatarExpression(AvatarExpression exp)
    {
        if (setupAvatar_.isVisible())
            setupAvatar_.setExpressionWithDecay(exp, 3000);
    }

    void MixCoachPanel::setAvatarWave(bool active)
    {
        if (setupAvatar_.isVisible() && active) {
            setupAvatar_.setExpression(AvatarExpression::Happy);
            setupAvatar_.setWaveActive(true, 3000);
        } else if (!active && setupAvatar_.isVisible()) {
            setupAvatar_.setWaveActive(false, 0);
        }
    }

    void MixCoachPanel::setAvatarNod(int64_t durationMs)
    {
        if (setupAvatar_.isVisible())
            setupAvatar_.triggerNod(durationMs);
    }

    void MixCoachPanel::setAvatarPoint()
    {
        if (setupAvatar_.isVisible()) {
            setupAvatar_.setExpression(AvatarExpression::Happy);
            setupAvatar_.setGaze(0.5f, 0.0f);
            setupAvatar_.setWaveActive(true, 2000);
        }
    }

    // ─── Greeting label for mode selection ──────────────────────────────────
    //  NOTA: Adem\xC3\xA1s de mostrar/ocultar, POSICIONA el label directamente
    //  para que sea visible INMEDIATAMENTE, sin depender de que resized()
    //  ejecute el bloque correcto del layout (que puede fallar cuando el estado
    //  del panel no coincide con el estado l\xC3\xB3gico de NavigationShell).

    void MixCoachPanel::setGreetingText(const juce::String& text)
    {
        if (text.isEmpty()) {
            greetingLabel_.setVisible(false);
            return;
        }
        greetingLabel_.setText(text, juce::dontSendNotification);

        // ─── Posicionar el label en el centro superior de la pantalla ──────
        // setBounds ANTES de setVisible para evitar flicker
        {
            int labelH = 48;
            int labelW = juce::jmin(getWidth() - 80, 600);
            int labelX = (getWidth() - labelW) / 2;
            int labelY = 40;  // Margen superior
            greetingLabel_.setBounds(labelX, labelY, labelW, labelH);
            greetingLabel_.setFont(juce::Font(juce::FontOptions(juce::jmin(24.0f, getWidth() / 28.0f))).boldened());
            greetingLabel_.toFront(true);
        }
        greetingLabel_.setVisible(true);

        resized();
        repaint();
    }

    // ─── Session Scan Card ──────────────────────────────────────────────────

    void MixCoachPanel::showSessionScanCard(int totalTracks,
                                              const std::vector<juce::String>& trackNames)
    {
        addAndMakeVisible(sessionScanCard_);
        sessionScanCard_.reset();
        sessionScanCard_.startScan(totalTracks, trackNames);
        sessionScanCard_.setVisible(true);
        sessionScanCard_.toFront(true);

        sessionScanCard_.onComplete = [this]() {
            if (onSessionScanComplete) onSessionScanComplete();
        };

        resized();
        repaint();
    }

    void MixCoachPanel::hideSessionScanCard()
    {
        sessionScanCard_.reset();
        sessionScanCard_.setVisible(false);
        resized();
        repaint();
    }

    // ─── Track Problem Cards ────────────────────────────────────────────────

    void MixCoachPanel::updateTrackProblemCards(CoachEngine& coach)
    {
        auto groups = buildTrackProblemGroups(coach);

        if (groups.empty()) return;

        // Cooldown: 120s entre actualizaciones de tarjetas de problemas
        auto now = juce::Time::getMillisecondCounter() * 1000;
        static constexpr int64_t kTrackProblemCardCooldownUs = 120 * 1000 * 1000;

        if (now - lastTrackProblemCardsUs_ >= kTrackProblemCardCooldownUs) {
            int totalTracks = 0;
            for (const auto& g : groups)
                totalTracks += (int)g.tracks.size();

            juce::String introMsg;
            introMsg += juce::String(juce::CharPointer_UTF8("\xE2\x97\x89 **Problemas detectados**\n\n"));  // ◉
            introMsg += juce::String(juce::CharPointer_UTF8("Encontr\xC3\xA9 **"))
                        + juce::String(totalTracks)
                        + juce::String(juce::CharPointer_UTF8("** pistas con problemas en "))
                        + juce::String((int)groups.size())
                        + juce::String(juce::CharPointer_UTF8(" categor\xC3\xAD" "as."));

            addSystemMessage(introMsg);

            for (const auto& g : groups)
                chatMessages_.addTrackGroupCard(g);

            lastTrackProblemCardsUs_ = now;

            resized();
            repaint();
        }
    }

    void MixCoachPanel::showTrackProblemCard(const TrackProblemGroup& group)
    {
        addAndMakeVisible(trackProblemCard_);
        trackProblemCard_.setGroup(group);
        showTrackProblemCard_ = true;
        trackProblemCard_.setVisible(true);
        resized();
        repaint();
    }

    void MixCoachPanel::hideTrackProblemCard()
    {
        showTrackProblemCard_ = false;
        trackProblemCard_.setVisible(false);
        trackProblemCard_.clear();
        resized();
        repaint();
    }

    // ─── Panel Navigation ──────────────────────────────────────────────────

    void MixCoachPanel::setRevealManager(PanelRevealManager* manager)
    {
        juce::ignoreUnused(manager);
    }

    juce::StringArray MixCoachPanel::getAppliedPlugins() const
    {
        return appliedPlugins_;
    }

    void MixCoachPanel::showPanelMixMap()
    {
        showMixMap_ = true;
        activeGroupingChip_ = 3;
        resized();
        repaint();
    }

    void MixCoachPanel::showPanelReference()
    {
        refPanel_.setVisible(true);
        referencesSectionLabel_.setVisible(true);
        resized();
        repaint();
    }

    void MixCoachPanel::showPanelMessengers()
    {
        messengerList_.setVisible(true);
        tracksSectionLabel_.setVisible(true);
        resized();
        repaint();
    }

    juce::Component* MixCoachPanel::getPanelComponent(PanelId panelId) const noexcept
    {
        switch (panelId) {
            case PanelId::Coach:      return const_cast<MixCoachPanel*>(this);
            case PanelId::Reference:  return const_cast<ReferencePanelComponent*>(&refPanel_);
            case PanelId::Messengers: return const_cast<MessengerListComponent*>(&messengerList_);
            case PanelId::MixMap:     return const_cast<MixMapComponent*>(&mixMapComponent_);
            default:                  return nullptr;
        }
    }

    void MixCoachPanel::highlightTrackMention(const juce::String& track)
    {
        if (track.isEmpty() || coachEngine_ == nullptr) return;
        juce::String lowerTrack = track.toLowerCase();

        int foundSlot = -1;
        auto& registry = coachEngine_->getSharedData().getSlotRegistry();
        registry.forEachActive([&](const SlotInfo& info) {
            juce::String name(info.trackName);
            if (name.toLowerCase().contains(lowerTrack))
                foundSlot = info.slotIndex;
        });

        if (foundSlot >= 0) {
            messengerList_.setSelectedSlot(foundSlot);
            setSelectedTrackSlot(foundSlot);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  performSprint1Layout — Posiciona componentes UI según CoachRoomState
    //
    //  NOTA: Ahora usa coachRoomState_ en vez de 8 booleanos individuales.
    //  Es llamado desde el final de MixCoachPanel::resized().
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachPanel::performSprint1Layout()
    {
        auto bounds = getLocalBounds();

        // ═══ Calcular leftArea según CoachRoomState ══════════════════════════
        bool isSetup = isPreFullUI(coachRoomState_);
        juce::Rectangle<int> leftArea;

        if (isSetup) {
            auto chatArea = bounds.reduced(6);
            chatArea.removeFromBottom(18);  // footer
            chatArea.removeFromBottom(4);   // gap
            bool showSuggestions = !suggestions_.empty()
                && (showSuggestionsOverride_ || chatMessages_.isEmpty());
            // ═══ Durante Intention: NO remover espacio del input (QuickReplyBar + chips) ═══
            bool modeCardsActive = (coachRoomState_ == CoachRoomState::Intention);
            if (showSuggestions) {
                chatArea.removeFromBottom(44 + 4 + 24 + 2);  // 74px: chips + input + gaps
            } else if (!modeCardsActive) {
                chatArea.removeFromBottom(34 + 8);           // 42px: input + gap (solo si NO modeCards)
            }
            // Si modeCardsActive, NO se remueve espacio — el viewport usa toda el área
            leftArea = chatArea;
        } else {
            const int leftColW = juce::jmax(260, bounds.getWidth() * 38 / 100);
            leftArea = bounds.removeFromLeft(leftColW);
        }

        // ─── QuickReplyBar: sobre el input del chat ───────────────────────
        if (quickReplyBar_.isVisible()) {
            const int qrbH = 28;
            const int qrbGap = 2;
            juce::Rectangle<int> qrbArea;

            if (isSetup) {
                int fullW = getWidth() - 8;
                int qrbX = 4;
                int qrbY = getHeight() - 80;
                qrbArea = {qrbX, qrbY, fullW, qrbH};
            } else {
                auto area = getLocalBounds();
                const int colW = juce::jmax(260, area.getWidth() * 38 / 100);
                auto colArea = area.removeFromLeft(colW);
                qrbArea = colArea.removeFromBottom(qrbH + qrbGap + 34);
                leftArea.removeFromBottom(qrbH + qrbGap + 34);
            }

            quickReplyBar_.setBounds(qrbArea.reduced(2, 1));
            quickReplyBar_.toFront(true);
        }

        // ─── SessionScanCard: bottom of left column (P5 scan animation) ──
        if (sessionScanCard_.isVisible()) {
            int visTracks = sessionScanCard_.getVisibleTrackCount();
            int scanH = sessionScanCard_.isAnimating()
                        ? juce::jmin(200 + visTracks * 13, 400)
                        : 130;
            sessionScanCard_.setBounds(
                leftArea.getX() + 4,
                juce::jmax(leftArea.getY() + 100, leftArea.getBottom() - scanH - 20),
                leftArea.getWidth() - 8,
                scanH);
            sessionScanCard_.toFront(true);
        }

        // ─── ReferenceOnboardingCard: full-screen centered (P4) ─────────
        //    MISMO PATRÓN que Mode/Genre cards: centered, full-screen, dark background.
        //    El chat viewport está oculto (setShowReferenceCards lo oculta).
        //    Avatar + greeting + reference card, nothing else.
        if (coachRoomState_ == CoachRoomState::ReferenceStage && refOnboardingCard_.isVisible() && !showSplitLayout_) {
            int cardW  = juce::jmin(480, getWidth() - 120);
            int cardH  = 160;
            int avatarSize = juce::jmin(80, getWidth() / 8);
            int avatarGap  = 20;
            int totalBlockW = avatarSize + avatarGap + cardW;
            int blockX = juce::jmax(4, (getWidth() - totalBlockW) / 2);

            // ═══ Centrar el bloque más arriba que el medio ═══
            int greetingH = 44;
            int greetingGap = 12;
            int totalBlockH = greetingH + greetingGap + cardH;
            int blockTop = (getHeight() - totalBlockH) / 2 - 30;  // 30px más arriba del centro
            if (blockTop < 30) blockTop = 30;

            // Greeting label centered above the block
            if (greetingLabel_.isVisible()) {
                int greetingW = totalBlockW;
                greetingLabel_.setBounds(blockX, blockTop, greetingW, greetingH);
                greetingLabel_.setFont(juce::Font(juce::FontOptions(juce::jmin(22.0f, getWidth() / 26.0f))).boldened());
                greetingLabel_.toFront(true);
            }

            int blockY = blockTop + greetingH + greetingGap;

            // Avatar to the left of the reference card
            int avatarY = blockY + (cardH - avatarSize) / 2;
            setupAvatar_.setBounds(blockX, avatarY, avatarSize, avatarSize);
            setupAvatar_.setVisible(true);
            setupAvatar_.toFront(true);

            // Reference card to the right of avatar
            refOnboardingCard_.setBounds(
                blockX + avatarSize + avatarGap,
                blockY,
                cardW,
                cardH);
            refOnboardingCard_.toFront(true);
        }

        // ─── ReferenceAnalysisProgressCard: CENTERED below onboarding card ──
        if (refAnalysisProgressCard_.isVisible()) {
            int progressH = refAnalysisProgressCard_.isAnimating() ? 60 : 130;
            int cardW = juce::jmin(520, getWidth() - 120);
            int cardX = (getWidth() - cardW) / 2;
            int refBottom = refOnboardingCard_.isVisible()
                ? refOnboardingCard_.getBottom() + 8
                : getHeight() / 2;
            refAnalysisProgressCard_.setBounds(
                cardX,
                refBottom,
                cardW,
                progressH);
            refAnalysisProgressCard_.toFront(true);
        }

        // ─── SessionPrepChecklist: full-screen centered (P5) ────────────────
        //    MISMO PATRÓN que Mode/Genre/Reference cards: centered, full-screen.
        //    El chat viewport está oculto (setShowSessionPrepCard lo oculta).
        //    Avatar + greeting + checklist, nothing else.
        if (coachRoomState_ == CoachRoomState::SessionPrep && sessionPrepChecklist_.isVisible() && !showSplitLayout_) {
            int prepW  = juce::jmin(500, getWidth() - 80);
            int prepH  = juce::jmin(320, getHeight() * 3 / 5);
            int avatarSize = juce::jmin(80, getWidth() / 8);
            int avatarGap  = 20;
            int totalBlockW = avatarSize + avatarGap + prepW;
            int blockX = juce::jmax(4, (getWidth() - totalBlockW) / 2);

            // ═══ Centrar el bloque verticalmente ═══
            int greetingH = 44;
            int greetingGap = 10;
            int totalBlockH = greetingH + greetingGap + prepH;
            int blockTop = (getHeight() - totalBlockH) / 2 - 20;  // 20px arriba del centro
            if (blockTop < 20) blockTop = 20;

            // Greeting label centered above the block
            if (greetingLabel_.isVisible()) {
                int greetingW = totalBlockW;
                greetingLabel_.setBounds(blockX, blockTop, greetingW, greetingH);
                greetingLabel_.setFont(juce::Font(juce::FontOptions(juce::jmin(22.0f, getWidth() / 26.0f))).boldened());
                greetingLabel_.toFront(true);
            }

            int blockY = blockTop + greetingH + greetingGap;

            // Avatar to the left of the checklist
            int avatarY = blockY + (prepH - avatarSize) / 2;
            setupAvatar_.setBounds(blockX, avatarY, avatarSize, avatarSize);
            setupAvatar_.setVisible(true);
            setupAvatar_.toFront(true);

            // Checklist to the right of avatar
            sessionPrepChecklist_.setBounds(
                blockX + avatarSize + avatarGap,
                blockY,
                prepW,
                prepH);
            sessionPrepChecklist_.toFront(true);
        }

        // ─── Intention: Avatar + greeting centrados (chips vía QuickReplyBar) ──
        //    El chat viewport está oculto, chips visibles en QuickReplyBar.
        //    Solo avatar + greeting centrados, sin tarjetas.
        if (coachRoomState_ == CoachRoomState::Intention) {
            int avatarSize = juce::jmin(110, getWidth() / 7);
            int totalBlockW = avatarSize;
            int blockX = juce::jmax(4, (getWidth() - totalBlockW) / 2);

            // ═══ Centrar ═══
            int greetingH = 48;
            int totalBlockH = greetingH + 20;
            int blockTop = (getHeight() - totalBlockH) / 2 - 40;
            if (blockTop < 16) blockTop = 16;

            // Greeting centered
            if (greetingLabel_.isVisible()) {
                int greetingW = juce::jmin(400, getWidth() - 80);
                greetingLabel_.setBounds((getWidth() - greetingW) / 2, blockTop, greetingW, greetingH);
                greetingLabel_.setFont(juce::Font(juce::FontOptions(juce::jmin(28.0f, getWidth() / 24.0f))).boldened());
                greetingLabel_.toFront(true);
            }

            // Avatar centered below greeting
            if (setupAvatar_.isVisible()) {
                int avatarX = (getWidth() - avatarSize) / 2;
                int avatarY = blockTop + greetingH + 12;
                setupAvatar_.setBounds(avatarX, avatarY, avatarSize, avatarSize);
                setupAvatar_.setVisible(true);
                setupAvatar_.toFront(true);
            }
        }

        // ─── Genre grid: centered block during Genre (P3) ───────────────
        //    MISMO PATRÓN que ModeSelectionCard: centered, full-screen, clean background.
        //    El chat viewport está oculto (setShowGenreCards lo oculta).
        //    Avatar + greeting + genre grid, nothing else.
        if (coachRoomState_ == CoachRoomState::Genre && genreSelectionGrid_.isVisible()) {
            // ═══ TAMAÑOS — género usa grid de chips flow, no tarjetas fijas ═══
            int gridW  = juce::jmin(520, getWidth() - 120);
            int gridH  = juce::jmin(280, getHeight() * 2 / 5);
            int avatarSize = juce::jmin(90, getWidth() / 7);
            int avatarGap  = 20;
            int totalBlockW = avatarSize + avatarGap + gridW;
            int blockX = juce::jmax(4, (getWidth() - totalBlockW) / 2);

            // ═══ Centrar el bloque verticalmente ═══
            int greetingH = 48;
            int greetingGap = 14;
            int totalBlockH = greetingH + greetingGap + gridH;
            int blockTop = (getHeight() - totalBlockH) / 2;
            if (blockTop < 20) blockTop = 20;

            // Greeting label centered above the block
            if (greetingLabel_.isVisible()) {
                int greetingW = totalBlockW;
                greetingLabel_.setBounds(blockX, blockTop, greetingW, greetingH);
                greetingLabel_.setFont(juce::Font(juce::FontOptions(juce::jmin(26.0f, getWidth() / 24.0f))).boldened());
                greetingLabel_.toFront(true);
            }

            int blockY = blockTop + greetingH + greetingGap;

            // Avatar to the left of the genre grid
            int avatarY = blockY + (gridH - avatarSize) / 2;
            setupAvatar_.setBounds(blockX, avatarY, avatarSize, avatarSize);
            setupAvatar_.setVisible(true);
            setupAvatar_.toFront(true);

            // Genre grid to the right of avatar
            int gridX = blockX + avatarSize + avatarGap;
            genreSelectionGrid_.setBounds(gridX, blockY, gridW, gridH);
            genreSelectionGrid_.toFront(true);
        }

        // ═══ MixMapComponent: alongside chat (P6) ═════════════════════════
        //    Como en el prototipo HTML: mapa visible junto al chat.
        if (coachRoomState_ == CoachRoomState::MixMapStage && mixMapComponent_.isVisible()) {
            int mapW = juce::jmin(600, getWidth() - 60);
            int mapH = juce::jmin(300, getHeight() / 2);
            int avatarSize = 60;
            int totalW = avatarSize + 16 + mapW;
            int groupX = (getWidth() - totalW) / 2;
            int groupY = juce::jmax(leftArea.getBottom() + 8, leftArea.getY() + leftArea.getHeight() * 2 / 3);

            setupAvatar_.setBounds(groupX, groupY + (mapH - avatarSize) / 2, avatarSize, avatarSize);
            setupAvatar_.setVisible(true);
            setupAvatar_.toFront(true);

            mixMapComponent_.setBounds(
                groupX + avatarSize + 16,
                groupY,
                mapW,
                mapH);
            mixMapComponent_.toFront(true);
        }

        // ═══ MixMapDetailPanel: overlay en right panel ────────────────────
        if (mixMapDetailPanel_.isVisible()) {
            auto rightArea = bounds;
            // Sincronizar con kPanelWidth=300 del componente
            int panelW = juce::jmin(MixMapDetailPanel::kPanelWidth, rightArea.getWidth() / 3);
            mixMapDetailPanel_.setVisible(true); // addAndMakeVisible ya en constructor
            mixMapDetailPanel_.setBounds(
                rightArea.getRight() - panelW,
                rightArea.getY(),
                panelW,
                rightArea.getHeight());
            mixMapDetailPanel_.toFront(true); // overlay debe estar al frente
        }

        // ═══ CoachingGuideWidget: panel derecho, sobre los tracks (P7) ───
        if (isCoachingState(coachRoomState_) && coachingGuide_.isVisible()) {
            auto rightArea = bounds;
            int guideW = juce::jmin(280, rightArea.getWidth() - 8);
            int guideH = juce::jmin(380, rightArea.getHeight() / 2);
            coachingGuide_.setVisible(true); // addAndMakeVisible ya en constructor
            coachingGuide_.setBounds(
                rightArea.getX() + 4,
                rightArea.getY() + 4,
                guideW,
                guideH);
            coachingGuide_.toFront(true);
        }
    }

} // namespace mixcoach

