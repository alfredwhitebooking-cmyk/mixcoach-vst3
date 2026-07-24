#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include "SmoothValue.h"
#include "../engine/TrackRole.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  MixMapDetailPanel — Panel lateral que muestra información detallada
    //  de la pista seleccionada en el MixMap. Aparece al hacer clic en un nodo.
    //
    //  Layout:
    //    [Header: icono + nombre + "× cerrar" + badges]
    //    [Sección NIVEL: barra RMS + valores dB + target]
    //    [Sección PAN: slider L-C-R]
    //    [Sección EQ: mini curva con 6 regiones]
    //    [Sección RUTA: Track → Bus → Master]
    //    [Botón "Escuchar en solo"]
    // ═══════════════════════════════════════════════════════════════════════════
    class MixMapDetailPanel : public juce::Component,
                             private juce::Timer
    {
    public:
        MixMapDetailPanel();
        ~MixMapDetailPanel() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        void mouseDown(const juce::MouseEvent& e) override;

        /** Actualiza los datos de la pista seleccionada.
            slotIndex: -1 para ocultar el panel. */
        void setTrackData(int slotIndex,
                          const juce::String& trackName,
                          const juce::String& roleName,
                          float peakDb,
                          float rmsDb,
                          float correlation,
                          float stereoWidth,
                          const float* bandEnergies,  // 30 bands
                          float targetLevelDb,
                          float roleConfidence);

        /** Establece los datos del perfil de referencia (target EQ) para
            superponer como línea punteada sobre la curva actual. */
        void setReferenceData(const float* refBandEnergies, bool hasReference);

        void setVisible(bool show) override;

        void clear();

        bool hasTrack() const noexcept { return slotIndex_ >= 0; }
        int getSlotIndex() const noexcept { return slotIndex_; }

        /** Callback invoked ~10x/sec via internal timer to refresh live data.
            El callback debe llamar a setTrackData() con datos frescos. */
        std::function<void()> onRefreshData;

        std::function<void()> onClose;
        std::function<void(int slotIndex)> onRequestSolo;

        /** Pin toggle: cuando está pinneado, el panel no se cierra al hacer clic fuera
            ni al seleccionar otra pista. */
        void setPinned(bool pinned) noexcept { isPinned_ = pinned; repaint(); }
        [[nodiscard]] bool isPinned() const noexcept { return isPinned_; }
        std::function<void(bool)> onTogglePinned;

        // ═══ Layout constants (publicas para que el layout padre sincronice bounds) ═══
        // BUG #8: Antes eran private, pero el layout en performSprint1Layout() usaba
        // un magic number (280) que no coincidia, causando clipping de labels.
        static constexpr int kPanelWidth = 300;
        static constexpr int kHeaderH = 32;
        static constexpr int kSectionLabelH = 18;
        static constexpr int kMeterH = 24;
        static constexpr int kPanH = 22;
        static constexpr int kEqH = 130;
        static constexpr int kRouteH = 40;
        static constexpr int kSoloH = 24;
        static constexpr int kPadding = 8;

    private:
        void timerCallback() override;
    /** Pausa/restaura el timer segun la visibilidad del componente. */
    void visibilityChanged() override;

    private:
        // ─── Render helpers ────────────────────────────────────────────────
        void drawLevelSection(juce::Graphics& g, juce::Rectangle<int> area);
        void drawPanSection(juce::Graphics& g, juce::Rectangle<int> area);
        void drawEqSection(juce::Graphics& g, juce::Rectangle<int> area);
        void drawRouteSection(juce::Graphics& g, juce::Rectangle<int> area);
        int slotIndex_ = -1;
        juce::String trackName_;
        juce::String roleName_;
        float peakDb_ = -100.0f;
        float rmsDb_ = -100.0f;
        float correlation_ = 0.0f;
        float stereoWidth_ = 0.0f;
        float bandEnergies_[30] = {};
        float refBandEnergies_[30] = {};
        bool hasReferenceData_ = false;
        static constexpr int kNumBands = 30;
        float targetLevelDb_ = -18.0f;
        float roleConfidence_ = 0.0f;

        SmoothValue rmsSmooth_{-80.0f, 30.0f, 200.0f};

        // Bounds para hit testing
        juce::Rectangle<float> closeBounds_;
        juce::Rectangle<float> pinBounds_;
        juce::Rectangle<float> soloBounds_;
        bool isPinned_ = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixMapDetailPanel)
    };

} // namespace mixcoach
