#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "../engine/CoachingStageManager.h"

namespace mixcoach {

    // Forward declarations
    class PluginSuggestionsProvider;

    // ═══════════════════════════════════════════════════════════════════════════
    //  CoachingGuideWidget — Panel interactivo de guía de coaching
    //
    //  Muestra:
    //    1. Timeline vertical de las 6 etapas (con iconos + checkmarks)
    //    2. Detalle de la etapa actual (descripción + checklist + progreso)
    //    3. 3-tier suggestion pills: Ajusta (🎛 Nativo) / Verifica (🟢 Gratis)
    //       / Mejora (⭐ Profesional)
    //    4. Botón de avance "Siguiente etapa" / "Ver consejos"
    //
    //  Se posiciona en el panel derecho (messenger area) durante el coaching.
    // ═══════════════════════════════════════════════════════════════════════════
    class CoachingGuideWidget : public juce::Component
    {
    public:
        CoachingGuideWidget();
        ~CoachingGuideWidget() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;

        // ─── Data updates ──────────────────────────────────────────────────
        /** Actualiza el widget con el estado actual del CoachingStageManager. */
        void updateFromStageManager(const CoachingStageManager& manager);

        /** Actualización simplificada: establece la etapa actual y progreso directamente.
            Usado cuando no hay acceso directo al CoachingStageManager.
            @param stage            Etapa de coaching actual
            @param stageProgress    Progreso 0.0-1.0 de la etapa
            @param overallProgress  Progreso 0.0-1.0 global
            @param provider         Opcional: PluginSuggestionsProvider para
                                    sugerencias reales de la DB. Si es nullptr,
                                    usa defaults hardcodeados. */
        void setStageDirectly(CoachingStage stage,
                              float stageProgress,
                              float overallProgress,
                              const PluginSuggestionsProvider* provider = nullptr);

        /** Actualiza las sugerencias 3-tier específicas para la etapa actual.
            @param ajustaSuggestion   Pista/tooltip para "Ajusta" (Native)
            @param verificaSuggestion Pista/tooltip para "Verifica" (Free)
            @param mejoraSuggestion   Pista/tooltip para "Mejora" (Premium) */
        void setTierSuggestions(const juce::String& ajustaSuggestion,
                                const juce::String& verificaSuggestion,
                                const juce::String& mejoraSuggestion);

        /** Muestra sugerencias 3-tier por defecto basadas en la etapa actual. */
        void setDefaultTierSuggestions(CoachingStage stage);

        /** Actualiza las sugerencias 3-tier consultando PluginSuggestionsProvider
            para obtener plugins reales de la base de datos según la etapa actual.
            Si la DB no está cargada o no encuentra resultados, cae a defaults.
            @param provider  Referencia al PluginSuggestionsProvider del CoachEngine
            @param stage     Etapa de coaching actual */
        void setStageSuggestionsFromProvider(const PluginSuggestionsProvider& provider,
                                              CoachingStage stage);

        // ═══ Live meter data (actualizado en tiempo real desde el timer) ═══
        /** Datos de medidores en tiempo real que se muestran en el widget.
            Se actualiza desde NavigationShell::updateAllPanels() cada ~100ms. */
        struct LiveMeterData {
            float peakDb = -80.0f;
            float rmsDb = -80.0f;
            float correlation = 1.0f;
            float crestFactor = 8.0f;
            float stereoWidth = 0.5f;
            float spectralCentroidHz = 1000.0f;
            int activeTrackCount = 0;
            int totalTrackCount = 0;
            // ─── Automation phase metering ────────────────────────────────
            float lufsIntegrated = -14.0f;  // LUFS integrated loudness
            float loudnessRange = 10.0f;    // LRA (Loudness Range) in dB
            // MixScore no se incluye aquí porque MixScore::compute()
            // es costoso y se calcula bajo demanda. Se muestra en el
            // reporte final (EndOfSessionComponent) en su lugar.
        };

        /** Actualiza los datos de medidores en tiempo real.
            Se llama desde NavigationShell::updateAllPanels() para que el
            widget muestre valores vivos de la sesión actual.
            @param data  Estructura con los valores actuales de los medidores */
        void setLiveMeterData(const LiveMeterData& data)
        {
            liveMeterData_ = data;
            repaint();
        }

        // ─── Callbacks ─────────────────────────────────────────────────────
        std::function<void()> onAdvanceStage;
        std::function<void()> onRequestHelp;
        std::function<void(int tierIndex)> onTierClicked; // 0=Ajusta, 1=Verifica, 2=Mejora

        // ─── Helpers ───────────────────────────────────────────────────────
        [[nodiscard]] CoachingStage getCurrentStage() const noexcept { return currentStage_; }
        [[nodiscard]] float getStageProgress() const noexcept { return stageProgress_; }
        [[nodiscard]] bool isComplete() const noexcept { return allComplete_; }
        [[nodiscard]] bool isAwaitingApproval() const noexcept { return awaitingApproval_; }

    private:
        // ─── State ─────────────────────────────────────────────────────────
        CoachingStage currentStage_{CoachingStage::GainStaging};
        float stageProgress_ = 0.0f;    // 0.0 - 1.0 for current stage
        float overallProgress_ = 0.0f;   // 0.0 - 1.0 overall
        bool stageCompletedFlags_[static_cast<int>(CoachingStage::COUNT)]{};
        bool awaitingApproval_ = false;
        bool allComplete_ = false;

        // ─── 3-tier suggestion text ────────────────────────────────────────
        juce::String ajustaSuggestion_;   // "Ajusta" (Native)
        juce::String verificaSuggestion_; // "Verifica" (Free)
        juce::String mejoraSuggestion_;   // "Mejora" (Premium)

        // ─── Live meter data (actualizado ~10fps desde timer) ───────────────
        LiveMeterData liveMeterData_;

        // ─── Layout constants ──────────────────────────────────────────────
        static constexpr int kTimelineItemH = 32;   // Height per stage in timeline
        static constexpr int kTimelineGap = 2;      // Gap between stages in timeline
        static constexpr int kSectionGap = 6;       // Gap between sections
        static constexpr int kProgressBarH = 6;     // Progress bar height
        static constexpr int kTierButtonH = 30;     // Height per tier button
        static constexpr int kActionButtonH = 26;   // Bottom action button height

        // ─── Hit-test bounds (computed in resized) ─────────────────────────
        juce::Rectangle<float> advanceBtnBounds_;
        juce::Rectangle<float> helpBtnBounds_;
        juce::Rectangle<float> ajustaBtnBounds_;
        juce::Rectangle<float> verificaBtnBounds_;
        juce::Rectangle<float> mejoraBtnBounds_;

        // ─── Hover state ───────────────────────────────────────────────────
        int hoveredStage_ = -1;         // -1 = none, 0-5 = stage index
        int hoveredTier_ = -1;          // -1 = none, 0=Ajusta, 1=Verifica, 2=Mejora
        bool hoveringAdvance_ = false;
        bool hoveringHelp_ = false;

        // ─── Paint helpers ─────────────────────────────────────────────────
        void drawStageTimeline(juce::Graphics& g, juce::Rectangle<int> area);
        void drawStageDetail(juce::Graphics& g, juce::Rectangle<int> area);
        void drawTierSuggestions(juce::Graphics& g, juce::Rectangle<int> area);
        void drawBottomActions(juce::Graphics& g, juce::Rectangle<int> area);
        void drawProgressBar(juce::Graphics& g, juce::Rectangle<float> area);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CoachingGuideWidget)
    };

} // namespace mixcoach
