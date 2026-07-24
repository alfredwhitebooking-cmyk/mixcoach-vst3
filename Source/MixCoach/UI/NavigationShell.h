#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "TabBarComponent.h"
#include "CoachRoomState.h"
#include "CoachChatComponent.h"
#include "AnalyzersPanelComponent.h"
#include "EndOfSessionComponent.h"
#include "ProgressScreen.h"
#include "WelcomeComponent.h"
#include "FocusOverlay.h"
#include "WalkthroughOverlay.h"
#include "PhaseProgressBar.h"
#include "../engine/CoachingNarrativeDirector.h"
#include "BackgroundEffectsComponent.h"
#include "../../Common/memory/SharedData.h"
#include "../../Common/audio/DiagnosticBridge.h"
#include "../../Common/types/Types.h"
#include "../engine/PanelRevealManager.h"
#include "../engine/ExperienceManager.h"
#include "../engine/SceneManager.h"
#include "../engine/LlmCommandInterpreter.h"
#include "../core/UrlDownloader.h"
#include "../core/PluginProcessor.h"

namespace mixcoach {

    class MixCoachAudioProcessor;

    // =======================================================================
    //  NavigationShell — Shell principal con sidebar + contenido
    //
    //  3 secciones: CoachRoom (chat + reference + mix map), Analyzers, Stats
    //  Layout: sidebar lateral izquierda (64px) + panel de contenido
    //  Incluye crossfade transition entre paneles (150ms).
    // =======================================================================
    class NavigationShell : public juce::Component,
                          private juce::Timer
    {
    public:
        NavigationShell(MixCoachAudioProcessor& processor, SharedData& sharedData);
        ~NavigationShell() override;

        /** Salva el estado actual de UI (flags, coachRoomState, userName) al processor.
            Se llama desde visibilityChanged() al ocultar y desde el destructor. */
        void saveCurrentUIFlags();

        void resized() override;
        void timerCallback() override;
    /** Pausa/restaura el timer y salva estado al ocultar/mostrar. */
    void visibilityChanged() override;        void mouseDown(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;

        // --- Content panels access ---
        MixCoachPanel& getCoachPanel() { return *coachPanel_; }
        AnalyzersPanelComponent& getAnalyzersPanel() { return *analyzersPanel_; }
        EndOfSessionComponent& getReportPanel() { return *reportPanel_; }
        ReferencePanelComponent& getRefPanel() { return coachPanel_->getRefPanel(); }

        /** Refresca el panel de reporte con datos actuales del motor. */
        void refreshReport();

        // Actualizacion completa de todos los paneles
        void updateAllPanels(SlotRegistry& registry, SharedData& sharedData, double sampleRate);

        // Actualizacion de master meters (desde AudioAnalyzer)
        void updateMasterMeters(const AudioAnalyzer& analyzer) { coachPanel_->updateMasterMeters(analyzer); }

        // Smooth de meters y analyzers SIN lock (60fps, no necesita SlotRegistry)
        void smoothMeters() { coachPanel_->smoothMeters(); }
        void smoothAnalyzersPanel(double sampleRateHz = 60.0);

        /** Retorna la seccion activa del sidebar. */
        TabBarComponent::Tab getActiveTab() const noexcept
        {
            return tabBar_.getActiveTab();
        }

    /** Setea el tab activo. */
    void setActiveTab(TabBarComponent::Tab tab)
    {
        tabBar_.setActiveTab(tab);
        switchContent(tab);
    }

    // ═══ Chat-Commanded UI: PanelRevealManager ═══════════════════════════════
    /** Procesa un mensaje del Coach y revela paneles/resalta tracks según keywords.
        Llamar desde el callback de mensajes del coach (setMessagePushedCallback). */
    void processCoachMessage(const juce::String& text);

    /** Retorna el PanelRevealManager para acceso desde PluginEditor. */
    [[nodiscard]] PanelRevealManager& getRevealManager() noexcept { return revealManager_; }

    /** Revela un panel específico (desbloquea su tab y muestra animación). */
    void revealPanel(PanelId panelId);

    // ═══ Progressive Disclosure — CoachRoomState ═══════════════════════════════
    void setCoachRoomState(CoachRoomState newState);

    // ═══ ExperienceManager — Orquestador UX 2.0 ═══════════════════════════════
    [[nodiscard]] ExperienceManager& getExperienceManager() noexcept { return experienceManager_; }

    [[nodiscard]] CoachRoomState getCoachRoomState() const noexcept { return coachRoomState_; }

    /** Retorna true si estamos en modo setup (pre-FullUI, solo chat + paneles inline). */
    /** Retorna el LlmCommandInterpreter para acceso externo (PluginEditorTimer, etc.). */
    [[nodiscard]] LlmCommandInterpreter& getCommandInterpreter() noexcept { return commandInterpreter_; }

    /** Actualiza la footer bar de TODOS los paneles con datos en tiempo real del motor.
        Llama a MixCoachPanel::updateFooterInfo(). */
    void updateFooterInfo(const juce::String& phaseName,
                          const juce::String& genre,
                          const juce::String& target,
                          const juce::String& sampleRate,
                          int expLevel);

    // ═══ Auto-return (Fase 5: "Coach abre Tools → regresa al chat") ═══════
    /** Inicia el temporizador de auto-return. Cuando el LLM cambia a Tools
        o Session, el sistema vuelve automáticamente al Coach tras X segundos
        si el usuario no interactúa. */
    void startAutoReturn();

    /** Igual que startAutoReturn pero con un delay personalizado.
        @param delaySecs  Segundos antes de auto-return (default 3.0). */
    void startAutoReturn(float delaySecs);

    /** Cancela el auto-return pendiente. Se llama cuando el usuario
        interactúa manualmente (cambia de tab, escribe en el chat). */
    void cancelAutoReturn();

    // ═══ Auto-switch tab (Fase 5+: "Coach cambia tabs automáticamente") ═══
    /** Cambia al tab indicado y programa auto-return al Coach.
        Muestra un toast con el tiempo restante.
        @param tab         Tab al que cambiar (Tools, Session, etc.)
        @param delaySecs   Segundos antes de volver al Coach (default 3.0) */
    void autoSwitchTab(TabBarComponent::Tab tab, float delaySecs = kAutoSwitchDelay);

    /** Retorna el tiempo restante de auto-return en segundos, o <= 0 si inactivo. */
    [[nodiscard]] float getAutoReturnRemaining() const noexcept { return autoReturnCountdown_; }

    /** Retorna true si el auto-return está activo y contando. */
    [[nodiscard]] bool isAutoReturnActive() const noexcept { return autoReturnActive_; }

    // ═══ SceneManager — Director de la experiencia (Fase 3) ═══════════
    /** Aplica una SceneDef al estado actual de la UI.
        Gestiona visibilidad de paneles, sidebar, auto-return, y celebraciones. */
    void applyScene(const SceneDef& scene);

    /** Procesa un evento del director y aplica la transición resultante.
        Es el punto de entrada único desde PluginEditor para enviar eventos
        del motor (PhaseChanged, ReferenceLoaded, etc.) al SceneManager.
        @param event  Evento del director (tipo + payload opcional) */
    void processDirectorEvent(const DirectorEvent& event)
    {
        auto sceneDef = sceneManager_.processEvent(event);
        applyScene(sceneDef);
    }

    // ═══ WalkthroughOverlay — Tutorial interactivo (4.3) ═════════════════
    /** Muestra el walkthrough de bienvenida para nuevos usuarios.
        Solo se muestra en la primera sesi\xC3\xB3n (UserProfile.firstSession == true). */
    void showWalkthrough();

    /** Salta el walkthrough y lo marca como completado. */
    void skipWalkthrough();

    /** Reinicia el walkthrough desde el paso 0 (para testing/settings). */
    void restartWalkthrough();

    /** Retorna true si el walkthrough est\xC3\xA1 activo. */
    [[nodiscard]] bool isWalkthroughActive() const noexcept
    {
        return walkthroughOverlay_.isWalkthroughActive();
    }

    // ═══ FocusOverlay — Enfoque visual (SCENE 9) ═══════════════════════════
    /** Muestra el overlay enfocando un grupo (bus) completo.
        Oscurece todo excepto el grupo, con glow y label.
        @param busType  Tipo de bus a enfocar (Drums, Bass, etc.) */
    void showFocusOverlay(BusType busType);

    /** Muestra el overlay enfocando una pista individual.
        Oscurece todo excepto la pista, con glow y label.
        @param slotIndex  Índice del slot a enfocar */
    void showFocusOverlay(int slotIndex);

    /** Limpia el overlay de enfoque con fade-out. */
    void clearFocus();

    /** Retorna true si el overlay de enfoque está activo. */
    [[nodiscard]] bool isFocusActive() const noexcept
    {
        return focusOverlay_.isFocusActive();
    }

    /** Retorna el FocusOverlay para acceso externo (PluginEditor, etc.). */
    [[nodiscard]] FocusOverlay& getFocusOverlay() noexcept { return focusOverlay_; }

    /** Retorna el SceneManager para acceso desde PluginEditor. */
    [[nodiscard]] SceneManager& getSceneManager() noexcept { return sceneManager_; }

    // ==== SetupFadeAnim struct (publico para acceso en tests) ====
    /** Animacion de fade-in para transiciones entre pantallas de setup.
        - El nuevo contenido se aplica inmediatamente con alpha 0
        - Luego fade-in alpha 0->1 durante el 70% restante
        - 30% pausa (~90ms alpha 0), 70% fade-in (~210ms alpha 0->1)
        - Duracion total: ~300ms (18 frames a 60fps) con ease-out quad */
    struct SetupFadeAnim {
        bool active = false;
        float progress = 0.0f;
        static constexpr float kFrames      = 18.0f;
        static constexpr float kStep        = 1.0f / kFrames;
        static constexpr float kFadeInStart = 0.3f;
    };

    /** Inicia una transición fade entre pantallas de setup.
        Aplica el nuevo estado inmediatamente con alpha 0,
        luego la animación del timerCallback lo fadea a alpha 1.
        @param newState  Nuevo estado a mostrar (ya debe estar aplicado visualmente) */
    void startSetupTransition();

    /** Retorna true si hay una animación de transición setup activa. */
    [[nodiscard]] bool isSetupFadeActive() const noexcept { return setupFadeAnim_.active; }

    // ═══ Fase 2: Chat como centro — postear eventos UI al chat ═══════════
    /** Setea la expresión emocional del avatar del Coach.
        Propaga a MixCoachPanel para que el avatar refleje emociones
        desde ExperienceManager, eventos del motor, o comandos del LLM. */
    void setAvatarExpression(AvatarExpression exp);

    /** Activa/desactiva la animación de saludo con la mano del avatar.
        Se usa en P2 (selección de modo) para que el robot salude al usuario. */
    void setAvatarWave(bool active);

    /** Dispara una animación de cabeceo (asentimiento) en el avatar.
        Se usa en P3 (selección de género) y P5 (checklist completado).
        @param durationMs  Duración del cabeceo en ms (default 500). */
    void setAvatarNod(int64_t durationMs = 500);

    // ═══ CoachEngine & Director access ════════════════════════════════════
    /** Retorna el CoachEngine desde el processor. */
    [[nodiscard]] CoachEngine* getCoachEngine() const noexcept
    {
        return processorRef_.getCoachEngine();
    }

    /** Retorna el CoachingNarrativeDirector. */
    [[nodiscard]] CoachingNarrativeDirector* getNarrativeDirector() const noexcept
    {
        return narrativeDirector_.get();
    }

    /** Lee la informaci\xC3\xB3n de transporte actual del DAW anfitri\xC3\xB3n.
        Retorna un struct con BPM, posici\xC3\xB3n en segundos, time signature,
        y estado de reproducci\xC3\xB3n. \xC3\x9Atil para que el coach pueda
        referirse al contexto musical (ej: "en el coro a 0:45"). */
    [[nodiscard]] MixCoachAudioProcessor::TransportInfo getTransportInfo() const noexcept
    {
        return processorRef_.readTransportInfo();
    }

    /** Retorna la PhaseProgressBar (dots + XP) para acceso desde el Director. */
    [[nodiscard]] PhaseProgressBar& getPhaseProgressBar() noexcept { return phaseProgressBar_; }
    [[nodiscard]] const PhaseProgressBar& getPhaseProgressBar() const noexcept { return phaseProgressBar_; }

    /** Dispara una celebración de experiencia (XP + animación). */
    void celebrate(const juce::String& achievement)
    {
        experienceManager_.celebrate(achievement);
    }

    // ═══ Spectrum Highlight — Propaga frecuencia destacada al Spectrograph ═══
    /** Resalta una región de frecuencia en el SpectrographComponent.
        Se llama desde CoachingNarrativeDirector::doShowEvidence() cuando se
        detecta un problema con frecuencia específica (ej: masking en 2500Hz).
        El SpectrographComponent dibuja una banda glow pulsante + label.
        @param freqHz     Frecuencia central (Hz)
        @param label      Etiqueta descriptiva (ej: "2500 Hz — Masking") */
    void setSpectrumHighlight(float freqHz, const juce::String& label);

    /** Hace que el robot señale hacia el MixMap (derecha).
        Se usa en P6 al mostrar el mapa de sesión por primera vez. */
    void setAvatarPoint();

    /** Publica un evento visual en el chat (icono + mensaje).
        Ej: postUIEvent("\xF0\x9F\x91\x81", "Panel Reference revelado")
        Se usa desde processCoachMessage, ExperienceManager, y revealPanel. */
    void postUIEvent(const juce::String& icon, const juce::String& message);

    // ═══ Evento: panel revelado (unifica engine + chat + LLM + SceneManager paths) ═══
    /** Callback que se dispara cuando un panel se revela por PRIMERA vez.
        Se llama desde revealPanel() después de markPanelRevealed() y la guarda
        de primera vez. Unifica los 4 caminos:
          • Engine (ExperienceManager → revealPanel)
          • Chat (processCoachMessage → revealPanel)
          • LLM (LlmCommandInterpreter → revealPanel)
          • SceneManager (applyScene → setVisible → ahora también pasa por revealPanel)
        ExperienceManager escucha este callback para sus celebraciones. */
    std::function<void(PanelId panelId)> onRevealPanel;

private:
    // ═══ CallAfterDelay seguro: previene use-after-free ═══════
    template<typename Fn>
    void callAfterDelaySafe(int ms, Fn&& fn) {
        juce::Component::SafePointer<NavigationShell> safeThis(this);
        juce::Timer::callAfterDelay(ms, [safeThis, fn = std::forward<Fn>(fn)]() {
            if (safeThis == nullptr) return;
            fn();
        });
    }

    // ─── SceneManager (Director de la experiencia) ────────────────────────
    SceneManager sceneManager_;
    // ─── PanelRevealManager (Chat-Commanded UI) ────────────────────────────
    PanelRevealManager revealManager_;

    // ─── ExperienceManager (UX 2.0 orquestador) ──────────────────────────
    ExperienceManager experienceManager_;

    // ─── LlmCommandInterpreter (Fase 10: LLM controla UI via JSON) ───────
    LlmCommandInterpreter commandInterpreter_;

    // ─── Estado de progressive disclosure ───────────────────────────────────
    CoachRoomState coachRoomState_{CoachRoomState::Welcome};

    // ─── Suppress coach message flag (usado al retroceder en onboarding) ───
    // Cuando el back button dispara un Backward event, este flag evita
    // que applyScene() postee de nuevo el mensaje de bienvenida del coach.
    bool suppressCoachMessage_ = false;

    // ─── Auto-return timer state (Fase 5) ───────────────────────────────────
    bool autoReturnActive_ = false;
    float autoReturnCountdown_ = 0.0f;
    float autoReturnTimeoutOverride_ = 0.0f;             // 0 = usar kAutoReturnDelay
    static constexpr float kAutoReturnDelay = 4.0f;      // segundos antes de auto-return (default)
    static constexpr float kAutoSwitchDelay = 3.0f;     // segundos antes de auto-return para autoSwitchTab

    // ═══ Auto-return respetuoso (Fase 5+) ═══════════════════════════════════
    // Si el usuario interactúa 2+ veces mientras auto-return está activo,
    // se cancela permanentemente para que pueda explorar sin interrupciones.
    // Se resetea cuando el usuario vuelve manualmente al chat.
    int autoReturnRestartCount_ = 0;
    bool autoReturnPermanentlyCancelled_ = false;
    static constexpr int kAutoReturnMaxRestarts = 2;     // interacciones antes de cancelar
        // --- Panel switches al cambiar seccion ---
        void switchContent(TabBarComponent::Tab tab);

        /** Actualiza el estado de bloqueo de los tabs segun la progresion de la sesion.
            Session tab: desbloqueado cuando el Coach conoce la sesion (>= DeepAnalysis).
            Tools tab: desbloqueado cuando el Coach ha mostrado evidencia (diagnostics). */
        void updateTabLockState();

        // --- Background effects (partículas + orbes glow) ---
        BackgroundEffectsComponent backgroundEffects_;

        // --- PhaseProgressBar (dots + XP) ---
        PhaseProgressBar phaseProgressBar_;

        // --- TabBar + previous tab tracking ---
        TabBarComponent tabBar_;
        TabBarComponent::Tab previousTab_ = TabBarComponent::Coach;

        // --- FocusOverlay (SCENE 9) ---
        FocusOverlay focusOverlay_;

        // --- Welcome (STATE 0) ---
        std::unique_ptr<WelcomeComponent> welcomeComponent_;

        // --- Content panels ---
        std::unique_ptr<MixCoachPanel> coachPanel_;
        std::unique_ptr<AnalyzersPanelComponent> analyzersPanel_;
        std::unique_ptr<ProgressScreen> progressScreen_;
        std::unique_ptr<EndOfSessionComponent> reportPanel_;
        // --- Processor reference ---
        mixcoach::MixCoachAudioProcessor& processorRef_;
        SharedData& sharedData_;

        /** Conecta el EndOfSessionComponent al CoachEngine + AudioAnalyzer. */
        void wireReportPanel();

        /** Refresca los datos del ProgressScreen desde el engine. */
        void refreshProgress();

        // ═══ SceneManager helpers (Fase 3: pool lifecycle) ═══════════════
        /** Mapea PanelId → Component* para lookup rápido en applyScene. */
        [[nodiscard]] juce::Component* getPanelComponent(PanelId panelId) const noexcept;

        /** Resetea el estado de un panel al mostrarlo (pool lifecycle). */
        void resetPanelState(PanelId panelId, const juce::String& context);

        // --- Crossfade transition helpers ---
        struct CrossfadeState {
            juce::Component* outgoing = nullptr;
            juce::Component* incoming = nullptr;
            float progress = 0.0f;
            bool active = false;
        };
        CrossfadeState crossfade_;
        void startCrossfade(juce::Component* from, juce::Component* to);
        bool advanceCrossfade();

        // ═══ TabBar fade-in animation (Setup mode entrance) ════════════════
        bool tabBarFadeInActive_{false};
        float tabBarFadeInProgress_{0.0f};
        static constexpr float kTabBarFadeFrames = 12.0f; // 200ms at 60fps
        static constexpr float kTabBarFadeStep = 1.0f / kTabBarFadeFrames;

        // ═══ Plugin scan: flag para evitar escaneos duplicados ════════════
        bool pluginsScanned_ = false;

        // ═══ Messenger track detection (STATE 4 → STATE 5) ═══════════
        // Flag para evitar transiciones repetidas MessengerStage→MixMapStage.
        // Se setea a true cuando se detecta la primera pista activa durante
        // MessengerStage, y se resetea al salir del setup.
        bool messengerTracksDetected_ = false;

        // ═══ Gap #2: Space — previene re-pregunta al re-entrar al estado
        bool inSpaceQuestion_ = false;

        // ═══ WalkthroughOverlay — Tutorial interactivo ═════════════════
        WalkthroughOverlay walkthroughOverlay_;

        // ═══ Gap C: MasterCheck — previene re-diálogo al re-entrar al estado
        bool inMasterCheckDialog_ = false;

        // ═══ Space: previene re-wire de onApplyReverb al re-entrar al estado
        bool reverbWired_ = false;

        // ═══ Overrun monitoring — detecta congestión IPC ═══════════════════
        // Último overrun count conocido por slot (para detectar incrementos).
        // Se compara en timerCallback cada ~1s para mostrar toast "⚠️ Congestión IPC".
        std::array<uint32_t, SlotRegistry::kMaxSlots> overrunLastCounts_{};
        int overrunThrottleFrames_ = 0; // Evita spam de toasts (min 5s entre each)
        bool overrunInitialized_ = false; // true tras primer populate (evita falso positivo)

        // ═══ CoachingNarrativeDirector — Orquestador del loop de coaching ═══
        std::unique_ptr<CoachingNarrativeDirector> narrativeDirector_;

        // ═══ UrlDownloader — Descarga de URLs de referencia (YouTube/Spotify) ═══
        std::unique_ptr<UrlDownloader> urlDownloader_;

        // ═══ Toast overlay — notificación temporal con fade-out ═══════════
        // postUIEvent() muestra un mensaje flotante sobre el chat.
        // 1s visible + 0.5s fade-out (alpha 1.0 → 0.0).
        // Se auto-oculta al completar el fade.
        struct ToastState {
            bool active = false;
            int framesVisible = 0;    // contador de frames desde que se mostró
            float alpha = 0.0f;        // alpha actual (0.0 → 1.0 → 0.0)
            bool fadingOut = false;    // true después de 1s, inicia fade
            static constexpr int kVisibleFrames = 60;   // 1s a 60fps
            static constexpr int kFadeFrames = 30;      // 0.5s a 60fps
        };
        ToastState toastState_;
        juce::Label toastLabel_;

        // ═══ Back button — Navegación hacia atrás en el onboarding ═══════
        juce::Label backButtonLabel_;  // "← Atrás" — visible durante setup states
        bool backButtonHovered_ = false;  // Estado hover para glow subtle

        // ═══ SetupFadeAnim — instancia privada (struct definido en pública) ═══
        SetupFadeAnim setupFadeAnim_;

        // ═══ Panel Reveal Animation System (fade-in + slide-up) ═══════════
        // Se dispara desde revealManager_.onPanelRevealed cuando un panel se
        // revela por primera vez. La animación consiste en:
        //   - Fade: alpha 0 → 1 (ease-out quad)
        //   - Slide: translate 20px down → 0px (ease-out quad)
        //   - Duración: 200ms (~12 frames a 60fps)
        struct PanelRevealAnim {
            juce::Component* component = nullptr;
            float progress = 0.0f;
            bool active = false;
            static constexpr float kDuration = 12.0f; // frames at 60fps
            static constexpr float kStep = 1.0f / kDuration;
            static constexpr float kSlidePx = 20.0f;
        };
        static constexpr int kMaxRevealAnims = 6;
        PanelRevealAnim revealAnims_[kMaxRevealAnims];

        /** Escanea los directorios VST3 del sistema para detectar plugins instalados
            y registrarlos en el PluginSuggestionsProvider del CoachEngine.
            Se llama después de que el usuario carga una referencia o completa el setup.
            Los resultados se muestran como mensaje del sistema en el chat.
            Usa el flag pluginsScanned_ para evitar escaneos duplicados. */
        void scanUserPlugins();

        /** Inicia una animación slide-up + fade-in para un componente de panel.
            Busca un slot libre en revealAnims_ y configura la animación.
            Retorna true si encontró slot. */
        bool startRevealAnimation(juce::Component* component);

        /** Avanza todas las animaciones activas. Llama desde timerCallback().
            Retorna true si alguna animación sigue activa. */
        bool advanceRevealAnimations();
    };

} // namespace mixcoach
