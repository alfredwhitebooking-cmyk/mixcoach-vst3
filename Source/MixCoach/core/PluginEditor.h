#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <memory>
#include "PluginProcessor.h"

#include "../ui/NavigationShell.h"
#include "../ui/WalkthroughOverlay.h"
#include "../ui/MixCoachTheme.h"
#include "../ui/SmoothValue.h"
#include "../../Common/memory/SharedData.h"

namespace mixcoach {

    // ─── MixCoach Plugin Editor — Inicialización segura ─────────────────────────
    // INCREMENTO 1: El background worker (MixCoachBgService) ahora vive en
    // PluginProcessor, NO aquí. La telemetría nunca se detiene al cerrar la UI.
    // El editor solo lee snapshots del servicio a través del processor.
    class MixCoachAudioProcessorEditor :
        public juce::AudioProcessorEditor,
        private juce::Timer,
        private juce::ChangeListener
    {
    public:
        MixCoachAudioProcessorEditor(MixCoachAudioProcessor& processor, SharedData* sharedData);
        ~MixCoachAudioProcessorEditor() override;

        void resized() override;
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;

        /** Expone el tabbed component para que el processor pueda recolectar referencias. */
        NavigationShell* getTabbedComponent() const { return tabbedComponent_.get(); }

        /** True si la UI completa está construida. */
        bool isFullUIBuilt() const noexcept { return fullUIBuilt_; }

    private:
        void timerCallback() override;
    /** Pausa/restaura el timer segun la visibilidad del componente. */
    void visibilityChanged() override;
        void changeListenerCallback(juce::ChangeBroadcaster* source) override;
        void buildFullUI();
        /** Polls sharedData availability deffered via callAfterDelay.
            Replaces the old timer-based polling from the constructor.
            Starts the 60fps timer ONLY after buildFullUI() completes. */
        void retryInitSharedData();
        void revealAnalyzersTab();
        void maybeRevealAnalyzersTabFromMessage(const juce::String& text);
        [[nodiscard]] bool isWelcomeActive() const noexcept;

    public:
        // ─── Públicos para safeTimerLogic (función libre en PluginEditor.cpp) ─
        void initSharedData();
        void detectNewMessengers();

        /** Señaliza análisis de referencia al background service (en processor).
            Thread-safe: delega a MixCoachBgService en el processor. */
        void signalBgRefAnalysis(const juce::String& path) noexcept
        {
            processorRef_.getBgService().requestRefAnalysis(path);
        }

    private:
        // Callback seguro con SafePointer para evitar crash si el editor
        // es destruido mientras el ChangeBroadcaster tiene mensajes pendientes
        void handleChangeBroadcast();

        MixCoachAudioProcessor& processorRef_;
        SharedData* sharedData_;

        // UI completa (creada LAZY cuando sharedData esté disponible)
        std::unique_ptr<NavigationShell> tabbedComponent_;
        std::unique_ptr<WalkthroughOverlay> walkthroughOverlay_;

        bool fullUIBuilt_{false};
        uint32_t lastInitAttemptMs_{0}; // backoff para reintentos

        int lastActiveSlotCount_{0};

        uint32_t uiBuiltTimeMs_{0};

        // Momento de creación del editor (para logging de diagnóstico)
        uint32_t editorCreatedMs_{0};

        // Safety timeout: solo se dispara una vez
        bool safetyTimeoutFired_{false};

        // Contador de ticks del timer (para logging a 30fps)
        int timerTickCount_{0};

        // ─── Background cached image (dot grid, dibujado una vez) ──────────────
        juce::Image bgCache_;
        bool bgCacheValid_    = false;
        int lastCachedWidth_  = 0;
        int lastCachedHeight_ = 0;

        void rebuildBgCache();

        // ═══ Flag antichoque: se marca true al inicio del destructor ═══════════
        // Todas las callbacks (timer, changeListener) deben verificar esta flag
        // y retornar inmediatamente si es true, para evitar accesos a miembros
        // parcialmente destruidos durante la eliminación del editor.
        //
        // ⚠ Debe ser std::atomic porque backgroundRunLoop() lo lee desde el
        //    background thread mientras el destructor (message thread) lo escribe.
        std::atomic<bool> editorBeingDestroyed_{false};

        // ─── Header interactive element bounds (for mouseDown hit detection) ───
        juce::Rectangle<int> headerTab1Bounds_;   // "AI COACH" tab
        juce::Rectangle<int> headerTab2Bounds_;   // "ANALYZERS" tab
        juce::Rectangle<int> headerVerifyBounds_; // "VERIFICAR PROGRESO" button
        juce::Rectangle<int> headerMenuIconBounds_;
        juce::Rectangle<int> headerHelpIconBounds_;
        juce::Rectangle<int> headerSettingsIconBounds_;
        int headerActiveTab_{0}; // 0 = AI COACH, 1 = ANALYZERS

        // ═══ Header animation & hover state ══════════════════════════════════
        SmoothValue headerTabAnim_{0.0f, 5.0f, 150.0f}; // 0=tab1, 1=tab2
        int hoveredHeaderElement_{-1}; // -1=none, 0=tab1, 1=tab2, 2=verify, 3=menuIcon, 4=helpIcon, 5=settingsIcon
        bool analyzersTabVisible_{false};

        // Placeholder que se muestra mientras se inicializa la UI
        juce::Label placeholderLabel_;



        // ═══ INCREMENTO 1: Background worker eliminado ─────────────────────
        // MixCoachBgService ahora vive en PluginProcessor.
        // El editor solo lee snapshots via processor_.getBgService().getLatestSnapshots().
        // Ya no hay bgLock_, scheduling atoms, per-slot state, FFT, ni CSV logging aquí.
        // Todo eso está en Source/MixCoach/core/MixCoachBgService.h/.cpp

        // Flags estáticos: persisten entre recreaciones del editor
        static std::array<bool, SlotRegistry::kMaxSlots> s_announcedSlots_;

        // Fuerza una apertura consistente de la UI premium de bienvenida.
        juce::ComponentBoundsConstrainer editorBoundsConstrainer_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixCoachAudioProcessorEditor)
    };

} // namespace mixcoach
