#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Common/memory/SlotRegistry.h"
#include "../../Common/memory/SharedData.h"
#include "../../Common/types/Types.h"
#include "../../Common/types/Constants.h"
#include "MixCoachTheme.h"
#include "SmoothValue.h"
#include "../engine/TrackRole.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  MixMapComponent — Mapa de Mezcla Visual (V4)
    //
    //  Renderiza un árbol jerárquico de pistas agrupadas por bus:
    //
    //    ═══ DRUM BUS (3) ════════════════════════════════════════
    //      ● Kick (Kick)           [MONO]  ▂▄▆█▄▂  → Drums → Master
    //      ● Snare (Snare)         [CENTER]▂▃▅█▇▄  → Drums → Master
    //      ● HiHat (HiHat)         [WIDE]  ▁▂▄▆█▇  → Drums → Master
    //
    //    ═══ BASS BUS (1) ════════════════════════════════════════
    //      ● 808 (808 Bass)        [MONO]  ██▅▂▁▁  → Bass → Master
    //
    //  Datos:
    //    • TrackAudioResult de SharedData (peak, correlation, spectral bands)
    //    • SlotInfo de SlotRegistry (nombre, color, bus)
    //    • TrackRole inferido por CoachEngine
    //
    //  Clasifica:
    //    • Posición estéreo desde correlation + avgStereoWidth
    //      (MONO / CENTER / WIDE / SPREAD / ⚠PHASE)
    //    • Rango frecuencial desde bandEnergies[30] → 6 regiones
    //      barra ▂▄▆█▄▂ que muestra energía por región
    // ═══════════════════════════════════════════════════════════════════════════
    class MixMapComponent : public juce::Component, private juce::Timer
    {
    public:
        MixMapComponent();
        ~MixMapComponent() override;

        /** Actualiza todos los datos del mapa de mezcla.
            Se llama desde MixCoachPanel::updateMessengers(). */
        void updateData(SlotRegistry& registry,
                        SharedData& sharedData,
                        const std::array<TrackRole, SlotRegistry::kMaxSlots>& trackRoles);

        void paint(juce::Graphics& g) override;
        void resized() override;
        void visibilityChanged() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;

        /** Altura total del contenido del mapa (para el viewport). */
        int getPreferredHeight() const;

        // ─── Selection ────────────────────────────────────────────────────
        /** Callback cuando el usuario hace clic en una pista del mapa.
            Se pasa el slotIndex de la pista seleccionada (-1 si clic fuera de pistas). */
        // ─── Hover tracking ────────────────────────────────────────────────
        /** Retorna el slotIndex sobre el que está el mouse (-1 si ninguno). */
        [[nodiscard]] int getHoveredSlot() const noexcept { return hoveredSlotIndex_; }

        std::function<void(int slotIndex)> onTrackSelected;

        /** Retorna el slotIndex actualmente seleccionado (-1 si ninguno). */
        [[nodiscard]] int getSelectedSlot() const noexcept { return selectedSlotIndex_; }

        /** Setea la selección externamente (ej: desde la lista de pistas). */
        void setSelectedSlot(int slotIndex) noexcept
        {
            selectedSlotIndex_ = slotIndex;
            repaint();
        }

        // ─── Issue badges per track ────────────────────────────────────────
        /** Información de issues por slot, computada desde collectAllIssues(). */
        struct TrackIssueBadge
        {
            bool hasIssues    = false;
            bool isOptimal    = false; // Track está en rango óptimo (SALUDABLE)
            bool isCritical   = false; // Al menos un issue crítico
            float maxSeverity = 0.0f;  // 0.0-1.0, severidad máxima
            juce::String shortType;    // Issue type abreviado ("CLIP", "EQ", "DYN", "PHASE", "MASK")
            int issueCount = 0;        // Número total de issues
        };

        /** Actualiza los issue badges desde CoachEngine::collectAllIssues().
            Se llama desde MixCoachPanel::updateCoachAdvice(). */
        void setTrackIssues(const std::array<TrackIssueBadge, SlotRegistry::kMaxSlots>& badges) noexcept
        {
            trackIssueBadges_ = badges;
            repaint();
        }

        // ─── Confirm Map ────────────────────────────────────────────────────
        /** Callback cuando el usuario hace clic en "Confirmar Mapa". */
        std::function<void()> onConfirmMap;

        /** Retorna true si todas las pistas activas tienen un bus asignado
            (ninguna con BusType::None). El mapa se considera "completo". */
        [[nodiscard]] bool isMapComplete() const noexcept
        {
            for (const auto& group : busGroups_)
                if (group.bus == BusType::None && !group.tracks.empty()) return false;
            return totalTracks_ > 0;
        }

        /** Marca el mapa como confirmado (oculta el botón). */
        void setMapConfirmed(bool confirmed) noexcept
        {
            mapConfirmed_ = confirmed;
            repaint();
        }

        /** Retorna true si el mapa ya fue confirmado. */
        [[nodiscard]] bool isMapConfirmed() const noexcept { return mapConfirmed_; }

    private:
        // ─── Datos de una pista individual ────────────────────────────────
        struct TrackNode
        {
            int slotIndex = -1;
            juce::String name;
            juce::Colour colour   = juce::Colours::grey;
            BusType bus           = BusType::None;
            TrackRole role        = TrackRole::Unknown;
            float peakCombined    = -100.0f;
            float rmsCombined     = -100.0f;
            float correlation     = 0.0f;
            float avgStereoWidth  = 0.0f;
            float regionEnergy[6] = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};
            float roleConfidence  = 0.0f; // 0.0-1.0 desde inferencia
            bool active           = false;
            bool hasSignal        = false;
        };

        // ─── Grupo de pistas por bus ─────────────────────────────────────
        struct BusGroup
        {
            BusType bus;
            juce::Colour colour = juce::Colours::grey;
            juce::String name;
            std::vector<TrackNode> tracks;
        };

        std::vector<BusGroup> busGroups_;
        int busGroupCount_ = 0;
        int totalTracks_   = 0;

        // ─── Clasificaciones semánticas ───────────────────────────────────
        enum class StereoPos
        {
            Mono,      // avgWidth < 0.1 → cuasi-mono
            Center,    // corr > 0.8, width < 0.2 → centrado
            Narrow,    // corr > 0.5, width < 0.3 → ligeramente estéreo
            Wide,      // width 0.3-0.5 → claramente estéreo
            Spread,    // width > 0.5 → muy amplio
            PhaseIssue // corr < 0.0 → problema de fase
        };

        enum class FreqRange
        {
            SubBass, // Sub + Bass dominan (regiones 0-1)
            BassMid, // Bass + LoMid dominan (regiones 1-2)
            Mid,     // LoMid + HiMid dominan (regiones 2-3)
            MidHigh, // HiMid + Pres dominan (regiones 3-4)
            High,    // Pres + Air dominan (regiones 4-5)
            Full,    // Distribución uniforme
            Silent   // Sin señal
        };

        static StereoPos classifyStereo(float correlation, float avgWidth) noexcept;
        static FreqRange classifyFrequency(const float regionEnergy[6]) noexcept;

        static juce::String stereoLabel(StereoPos pos) noexcept;
        static juce::Colour stereoColor(StereoPos pos) noexcept;
        static juce::String freqLabel(FreqRange range) noexcept;
        static juce::Colour freqColor(FreqRange range) noexcept;
        static const char* roleDisplayName(TrackRole role) noexcept;

        // ─── Render helpers ──────────────────────────────────────────────
        void drawBusHeader(juce::Graphics& g, const BusGroup& group, juce::Rectangle<int> area, int index);
        void drawTrackRow(juce::Graphics& g, const TrackNode& node, juce::Rectangle<int> area, bool even);
        void drawLevelBar(juce::Graphics& g, juce::Rectangle<int> area, float levelDb) const;
        void drawFreqBar(juce::Graphics& g, juce::Rectangle<int> area, const float regionEnergy[6]) const;
        void drawStereoBadge(juce::Graphics& g, juce::Rectangle<int> area, StereoPos pos) const;
        void drawMasterSection(juce::Graphics& g, juce::Rectangle<int> area);
        void drawRoutingArrow(juce::Graphics& g,
                              juce::Point<float> from,
                              juce::Point<float> to,
                              juce::Colour colour,
                              float alpha = 0.3f) const;

        // ─── Layout constants ────────────────────────────────────────────
        static constexpr int kRowH      = 34;
        static constexpr int kHeaderH   = 26;
        static constexpr int kIndent    = 18; // Indentación de tracks bajo bus
        static constexpr int kDotSize   = 10; // Círculo de color del rol
        static constexpr int kLevelBarW = 48; // Ancho de la barra de nivel animada
        static constexpr int kFreqBarW  = 90; // Ancho de la barra de frecuencia
        static constexpr int kFreqBarH  = 12; // Alto de la barra de frecuencia
        static constexpr int kBadgeW    = 52; // Ancho del badge de estéreo
        static constexpr float kCr      = 4.0f;

        // ─── Animation ───────────────────────────────────────────────────
        void timerCallback() override;
        std::array<SmoothValue, SlotRegistry::kMaxSlots> levelSmooth_;

        // ─── Issue badges per track ──────────────────────────────────────
        std::array<TrackIssueBadge, SlotRegistry::kMaxSlots> trackIssueBadges_{};

        // ─── Tooltip rendering ────────────────────────────────────────────
        void drawTrackTooltip(juce::Graphics& g, juce::Point<int> mousePos) const;

        // ─── Confirm Map button bounds (set during paint, used in mouseDown) ─
        juce::Rectangle<int> confirmButtonBounds_;

        /** true si el usuario ya confirmó el mapa de mezcla (oculta el botón de confirmar). */
        bool mapConfirmed_ = false;

        // ─── Selection state ───────────────────────────────────────────────
        int selectedSlotIndex_ = -1;
        int hoveredSlotIndex_  = -1;

        // ─── Row bounds cache (for hit testing) ────────────────────────────
        // Cada entrada: { slotIndex, bounds }
        std::vector<std::pair<int, juce::Rectangle<int>>> trackRowBounds_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixMapComponent)
    };

} // namespace mixcoach
