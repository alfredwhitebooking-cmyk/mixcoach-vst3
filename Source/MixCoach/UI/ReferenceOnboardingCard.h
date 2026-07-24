#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "DropZoneComponent.h"
#include "../audio/ReferenceAnalyzer.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  ReferenceOnboardingCard — Tarjeta inline dentro del chat para cargar
    //  una referencia (archivo o URL). Aparece cuando el Coach la solicita
    //  en ReferenceStage.
    //
    //  Layout:
    //    [DropZoneComponent reutilizado — ocupa todo el ancho]
    //    [File Info: nombre, duración, sample rate, bit depth] (visible tras carga)
    //    [Botón "Analizar referencia"] (visible tras carga)
    // ═══════════════════════════════════════════════════════════════════════════
    class ReferenceOnboardingCard : public juce::Component
    {
    public:
        ReferenceOnboardingCard();
        ~ReferenceOnboardingCard() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;

        /** Muestra la info del archivo cargado. Se llama desde fuera cuando
            el usuario arrastra un archivo o pega una URL. */
        void setFileInfo(const juce::String& fileName,
                         double durationSecs,
                         int sampleRate,
                         int bitDepth,
                         bool isURL);

        /** Resetea la tarjeta al estado inicial (DropZone vacío). */
        void reset();

        /** Retorna true si hay un archivo/URL cargado y listo para analizar. */
        bool isReadyToAnalyze() const noexcept { return hasFile_; }

        /** Retorna la ruta completa del archivo cargado (vacío si es URL). */
        juce::String getFilePath() const noexcept { return filePath_; }

        /** Retorna la URL cargada (vacío si es archivo). */
        juce::String getURL() const noexcept { return url_; }

        // ─── Callbacks ─────────────────────────────────────────────────────────
        std::function<void(const juce::String&)> onFileDropped;
        std::function<void(const juce::String&)> onURLAdded;
        /** Disparado cuando el usuario hace clic en "Analizar referencia". */
        std::function<void()> onAnalyzeClicked;

    private:
    juce::Label headerLabel_;
    DropZoneComponent dropZone_;
    juce::Label fileInfoLabel_;
    juce::TextButton analyzeButton_;
    juce::TextButton clearButton_;

        bool hasFile_ = false;
        bool isURL_   = false;
        juce::String fileName_;
        juce::String filePath_;
        juce::String url_;
        juce::String fileInfoText_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReferenceOnboardingCard)
    };

} // namespace mixcoach
