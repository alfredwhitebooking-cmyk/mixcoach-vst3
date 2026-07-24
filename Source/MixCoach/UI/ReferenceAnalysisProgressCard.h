#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "SmoothValue.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  ReferenceAnalysisProgressCard — Barra de progreso animada 0→100%
    //  que aparece al hacer clic en "Analizar referencia".
    //
    //  Animación:
    //    - SmoothValue para progreso (0.0 → 1.0) en ~3s con ease-out
    //    - Texto de estado cambia por etapas: "Leyendo archivo..." → "Analizando
    //      espectro..." → "Calculando LUFS..." → "Listo"
    //    - Al llegar a 100%: colapsa y muestra resumen con métricas
    // ═══════════════════════════════════════════════════════════════════════════
    class ReferenceAnalysisProgressCard : public juce::Component,
                                          private juce::Timer
    {
    public:
        ReferenceAnalysisProgressCard();
        ~ReferenceAnalysisProgressCard() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;

        /** Inicia la animación de progreso desde 0 hasta completar. */
        void startAnimation();

        /** Fija el progreso externo (0.0–1.0) recibido desde ReferenceAnalyzer.
            Si se llama, la animación se sincroniza con el progreso real. */
        void setExternalProgress(float progress);

        /** Muestra el resumen final con todas las métricas clave.
            @param integratedLUFS  LUFS integrado (loudness general)
            @param loudnessRange   Rango de loudness en LU (LRA)
            @param truePeak        True Peak en dBTP
            @param crestFactor     Crest factor (peak - RMS) en dB
            @param correlation     Correlación estéreo (-1 a 1)
            @param spectralCentroidHz  Centroide espectral en Hz
            @param bandEnergies    30 bandas de energía espectral (para Sub/Air descriptors) */
        void showSummary(float integratedLUFS,
                         float loudnessRange,
                         float truePeak,
                         float crestFactor,
                         float correlation,
                         float spectralCentroidHz,
                         const float* bandEnergies = nullptr);

        /** Resetea la tarjeta al estado inicial. */
        void reset();

        bool isAnimating() const noexcept { return animating_; }
        bool isComplete() const noexcept { return complete_; }

        /** Disparado cuando el progreso alcanza un nuevo hito (0-4).
            stage 0 = "Leyendo archivo..." (15%)
            stage 1 = "Analizando espectro..." (35%)
            stage 2 = "Calculando LUFS..." (55%)
            stage 3 = "Procesando imagen estéreo..." (75%)
            stage 4 = "Análisis completado" (100%) */
        std::function<void(int stageIndex)> onStageChanged;

        std::function<void()> onComplete;

    private:
        void timerCallback() override;

        SmoothValue progressSmooth_{0.0f, 100.0f, 200.0f};
        float targetProgress_ = 0.0f;
        float displayedProgress_ = 0.0f;
        bool animating_ = false;
        bool complete_  = false;
        bool usingExternalProgress_ = false;

        // Etapas de texto (5 etapas con animación más suave)
        int stageIndex_ = 0;
        static constexpr int kNumStages = 5;
        static constexpr const char* kStageText[kNumStages] = {
            "Leyendo archivo de referencia...",
            "Analizando espectro de frecuencias...",
            "Calculando LUFS y rango dinámico...",
            "Procesando imagen estéreo...",
            "Análisis completado"
        };
        static constexpr float kStageThresholds[kNumStages] = {
            0.15f, 0.35f, 0.55f, 0.75f, 1.0f
        };

        // ═══ Checklist progresivo — aparecen durante la animación ════════
        static constexpr int kNumChecklistItems = 5;
        static constexpr const char* kChecklistTexts[kNumChecklistItems] = {
            "Archivo cargado",
            "Balance espectral",
            "Loudness (LUFS)",
            "Ancho estéreo",
            "Dinámica (Crest)"
        };
        static constexpr float kChecklistThresholds[kNumChecklistItems] = {
            0.10f, 0.30f, 0.50f, 0.70f, 0.85f
        };
        int visibleChecklistItems_ = 0;

        // ═══ Resumen — Métricas clave ══════════════════════════════════════
        bool showSummary_ = false;
        // Las lecturas de silencio o incompletas se presentan como una acción
        // concreta, nunca como valores técnicos crudos para el usuario.
        bool summaryHasUsableSignal_ = true;
        float summaryLUFS_  = 0.0f;
        float summaryRange_ = 0.0f;
        float summaryPeak_  = 0.0f;
        float summaryCrest_ = 0.0f;
        float summaryCorrelation_ = 0.0f;
        float summaryCentroidHz_  = 0.0f;

        // Energías de banda para descriptores cualitativos
        float summaryBandEnergies_[30] = {-100.0f};
        bool hasBandData_ = false;

        // ═══ Helpers cualitativos ═════════════════════════════════════════
        /** Retorna etiqueta textual cualitativa para sub/bass (0.0-1.0). */
        static juce::String subLabel(const float be[30]) noexcept;
        /** Retorna etiqueta textual cualitativa para brillo/aire desde centroid. */
        static juce::String airLabel(float centroidHz) noexcept;
        /** Retorna etiqueta textual cualitativa para dinámica/transientes desde crest. */
        static juce::String transientLabel(float crestDb) noexcept;
        /** Retorna etiqueta textual cualitativa para stereo width desde correlación. */
        static juce::String widthLabel(float correlation) noexcept;
        /** Retorna un score 0.0-1.0 para énergia en región Sub (bandas 0-4). */
        static float subEnergyScore(const float be[30]) noexcept;

        static constexpr float kAnimDurationSecs = 3.0f;  // 3s for 5 stages with smooth ease-out

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReferenceAnalysisProgressCard)
    };

} // namespace mixcoach
