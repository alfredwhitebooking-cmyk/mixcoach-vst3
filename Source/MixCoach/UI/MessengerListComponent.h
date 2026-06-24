#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include "MixCoachTheme.h"
#include "SmoothValue.h"
#include "../../Common/types/Types.h"
#include "../../Common/types/Constants.h"
#include "../../Common/memory/SlotRegistry.h"
#include "../../Common/memory/SharedData.h"
#include "../engine/TrackState.h"

namespace mixcoach {
    class CoachEngine;
} // namespace mixcoach

namespace mixcoach {
    class TrackFeedCore;
} // namespace mixcoach

#include "../engine/TrackRole.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  HealthFilter — Filtro de salud para la lista de tracks
    //  Cuando está activo, solo se muestran tracks con ese estado de salud.
    // ═══════════════════════════════════════════════════════════════════════════
    enum class HealthFilter : uint8_t
    {
        None,     // Sin filtro (mostrar todos)
        Critical, // Solo tracks rojos (clipping, overcompressed, collapse)
        Warning,  // Solo tracks amarillos (needs EQ, compression, phase)
        Clean,    // Solo tracks verdes
        Silent    // Solo tracks sin señal
    };
    enum class SuggestionStatus : uint8_t
    {
        None,   // Sin datos suficientes
        Green,  // 🟢 RMS óptimo / buena dinámica
        Yellow, // 🟡 Precaución: cerca del límite
        Red,    // 🔴 Problema: clipping, muy comprimido
        White   // ⚪ Sin señal
    };

    struct MessengerEntry
    {
        SlotInfo info;
        float peakLeft          = -100.0f;
        float peakRight         = -100.0f;
        float rmsAvg            = -100.0f;
        float peakHold          = -100.0f;
        uint32_t peakHoldTimeMs = 0;
        bool hasSignal          = false;
        juce::String aiSuggestion;
        SuggestionStatus suggestionStatus = SuggestionStatus::None;

        // ─── Nivel animado con decaimiento fijo (render loop independiente) ─
        float barLevel  = -80.0f;
        float rmsSmooth = -80.0f;

        // ─── Peak hold rendering ─────────────────────────────────────────
        float peakHoldAlpha = 0.0f;

        // ─── Stereo correlation ────────────────────────────────────────────
        float correlation = 0.0f;

        // ═══ Coach advice per track ═══════════════════════════════════════════
        float busTargetPeak = -8.0f;
        juce::String coachAdviceText;
        SuggestionStatus coachAdviceStatus = SuggestionStatus::None;

        // ═══ TrackFeedCore — health + attention + top events ═══════════════
        TrackHealth trackHealth = TrackHealth::Unknown;
        float attentionScore    = 0.0f;
        /**Últimos eventos de TrackFeedCore para tooltip.*/

        // ═══ Sprint 7: Consolidated health from TrackAdvice::Status ═══════
        // Mapeo: OnTarget→Green, NearTarget→Yellow, OffTarget→Red, NoSignal→White
        SuggestionStatus consolidatedHealth = SuggestionStatus::None;

        // ═══ TrackRole asignado por el usuario (para análisis semántico) ═════
        TrackRole trackRole  = TrackRole::Unknown;
        float roleConfidence = 0.0f; // 0.0-1.0, qué tan seguro estamos del rol

        // ═══ Signal order inference — pending confirmation from user ═════
        bool signalOrderPending = false;

        // Sprint 1: rol inferido (nombre/espectral) pendiente de confirmación.
        // Se sincroniza desde CoachEngine::isInferredRole() en syncRolesFromCoach().
        bool roleInferredPending = false;

        // ═══ Sprint 3: Per-Track Analysis (crest, FFT, stereo width) ═══════
        float crestDb        = 0.0f; // Peak - RMS (dB)
        float bandLevelDb[6] = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};
        float avgStereoWidth = 0.0f; // 0.0 (mono) to 1.0 (very wide)

        // ─── Fade-in animation ────────────────────────────────────────────
        float fadeAlpha                        = 1.0f;
        uint32_t fadeStartMs                   = 0;
        static constexpr float kFadeDurationMs = 350.0f;
    };

    // ─── Grupo de buses ─────────────────────────────────────────────────────────
    struct BusGroup
    {
        int count = 0;
        std::array<int, SlotRegistry::kMaxSlots> slotIndices{};
    };

    // Layout constants — card-based track list (now delegated to MixCoachTheme)
    // Usage: MixCoachTheme::cardHeight_list, MixCoachTheme::cardGap, etc.
    // Legacy aliases for backward compatibility during migration
    static constexpr int kCardHeight   = MixCoachTheme::cardHeight_list;
    static constexpr int kCardGap      = MixCoachTheme::cardGap;
    static constexpr int kHeaderHeight = MixCoachTheme::headerHeight;
    static constexpr int kTitleHeight  = MixCoachTheme::titleHeight;
    static constexpr int kCardCorner   = MixCoachTheme::cardCorner;

    // ═══════════════════════════════════════════════════════════════════════════
    //  MessengerListComponent — Track list with VU meters and AI suggestions
    //  ═══ PERSISTENCIA ESTÁTICA: Los datos sobreviven recreaciones del editor ═══
    // ═══════════════════════════════════════════════════════════════════════════
    class MessengerListComponent : public juce::Component, private juce::Timer
    {
    public:
        MessengerListComponent();
        ~MessengerListComponent() override;

        void resized() override;
        void paint(juce::Graphics& g) override;

        void updateMessengers(SlotRegistry& registry, SharedData& sharedData);
        /** Solo telemetría + cache (60 Hz); no repinta ni reordena la lista. */
        void refreshTelemetryFromRegistry(SlotRegistry& registry, SharedData& sharedData);
        void smoothMeters();
        void restoreFromPersistent();

        /** Actualiza consejos del CoachEngine por pista (bus, target, recomendación). */
        void updateCoachAdvice(CoachEngine& coach);

        [[nodiscard]] int getPreferredHeight() const;

        void setSelectedSlot(int slotIndex);

        [[nodiscard]] int getSelectedSlot() const noexcept { return selectedSlot_; }

        std::function<void(int slotIndex)> onSlotSelected;

        // Sprint 1: notifica al padre cuando se confirman roles (para refrescar badge).
        std::function<void()> onRolesConfirmed;

        // ─── Grouping mode ─────────────────────────────────────────────────────
        enum class GroupingMode
        {
            Type,
            Colour,
            Bus
        };
        void setGroupingMode(GroupingMode mode);

        [[nodiscard]] GroupingMode getGroupingMode() const noexcept { return groupingMode_; }

        // ─── Group collapse/expand ────────────────────────────────────────────
        void collapseAll();
        void expandAll();

        [[nodiscard]] bool isGroupCollapsed(int busIdx) const noexcept { return collapsedGroups_[busIdx]; }

        // ═══ TrackRole selector ═══════════════════════════════════════════
        /** Asigna el coach engine para poder actualizar roles desde la UI. */
        void setCoachEngine(CoachEngine* coach) noexcept
        {
            coachEngine_ = coach;
            if (coachEngine_ != nullptr) syncRolesFromCoach();
        }

        /** Sincroniza los roles desde el CoachEngine hacia los messengers locales. */
        void syncRolesFromCoach();
        /** Construye el popup menu de roles categorizado. */
        static juce::PopupMenu buildRoleMenu();
        /** Retorna el rectángulo del role pill para un slot (en coords de MessengerListComponent). */
        [[nodiscard]] juce::Rectangle<int> getRolePillBounds(int slotIndex) const;

        // ═══ SPRINT 5 — TrackFeed Live ═══════════════════════════════════
        /** Actualiza los top events desde TrackFeedCore (llamado desde updateCoachAdvice). */
        void updateTopEvents(TrackFeedCore& feed);

        /** Retorna el HealthFilter activo. */
        [[nodiscard]] HealthFilter getHealthFilter() const noexcept { return healthFilter_; }

        /** Setea el filtro de salud. None = mostrar todos. */
        void setHealthFilter(HealthFilter filter) noexcept
        {
            healthFilter_ = filter;
            repaint();
        }

        /** Limpia el filtro de salud activo. */
        void clearHealthFilter() noexcept
        {
            healthFilter_ = HealthFilter::None;
            repaint();
        }

        /** Retorna true si un slot debe ser filtrado según el healthFilter activo. */
        [[nodiscard]] bool isFilteredOut(int slotIndex) const;

    private:
        /** Encuentra la posición Y de un slot en la lista (para tooltip positioning). */
        [[nodiscard]] int findSlotY(int slotIndex) const;

        /** Computa las bounds de cada health pill para hit testing. */
        struct HealthPillBounds
        {
            juce::Rectangle<int> critical;
            juce::Rectangle<int> warning;
            juce::Rectangle<int> clean;
            juce::Rectangle<int> silent;
            bool valid = false;
        };

        HealthPillBounds lastHealthPillBounds_; // Cache para hit testing
        void timerCallback() override;
        void visibilityChanged() override;
        void drawBusHeader(juce::Graphics& g, juce::Rectangle<int>& bounds, int busIdx, int count);
        void drawTrackCard(juce::Graphics& g, juce::Rectangle<int> bounds, const MessengerEntry& entry, int index);

        void syncTelemetryFromRegistry(SlotRegistry& registry, bool& anyDataOut, SharedData& sharedData);
        /** Reconstruye busGroups_ desde messengers_ actuales. Se llama depsués de syncTelemetryFromRegistry() para
         * reflejar cambios de bus en tiempo real. */
        void rebuildBusGroups();

        void mouseDown(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;
        [[nodiscard]] int hitTestSlot(juce::Point<int> point) const;

        // ─── Datos locales ────────────────────────────────────────────────────
        std::array<MessengerEntry, SlotRegistry::kMaxSlots> messengers_;
        std::array<BusGroup, kNumBuses + 1> busGroups_;
        std::array<bool, kNumBuses + 1> collapsedGroups_{};
        GroupingMode groupingMode_ = GroupingMode::Bus;
        int activeMessengerCount_{0};
        int selectedSlot_ = -1;
        bool isPaused_{false};

        // ═══ TrackRole state ═════════════════════════════════════════════
        CoachEngine* coachEngine_ = nullptr;

        // ═══ SPRINT 5 — TrackFeed Live state ═════════════════════════════
        HealthFilter healthFilter_ = HealthFilter::None;
        bool bannerCollapsed_      = true;  // TrackFeed banner empieza colapsado
        std::vector<TrackEvent> topEvents_; // Top eventos globales para el banner
        int tooltipSlot_ = -1;              // Slot cuyo health dot está siendo hovereado
        mutable juce::Rectangle<int>
            bannerCollapseBtn_; // Bounds del botón collapse/expand (mutable, se actualiza en paint())

        // ═══ SPRINT 7: Banner event clickability ═══════════════════════
        mutable std::vector<juce::Rectangle<int>>
            bannerEventBounds_;            // Hit bounds for each event in banner (mutable, set in paint)
        int clickedEventIdx_ = -1;         // Index into topEvents_ of clicked event, -1 = none
        juce::Point<int> clickedMousePos_; // Mouse position when banner event was clicked (for popup positioning)

        // ─── Hover / selection animations ────────────────────────────────────
        int hoveredSlot_ = -1;
        SmoothValue hoverGlow_{0.0f, 50.0f, 300.0f}; // hover alpha transition

        // ─── Cache de telemetría para smoothMeters() ──────────────────────────
        struct TelemetrySnapshot
        {
            float peakLeft    = -100.0f;
            float peakRight   = -100.0f;
            float rmsAvg      = -100.0f;
            float correlation = 0.0f;
            bool hasSignal    = false;
            bool active       = false;
        };

        std::array<TelemetrySnapshot, SlotRegistry::kMaxSlots> telemetryCache_{};
        juce::Label emptyLabel_;

        // ─── Datos PERSISTENTES (sobreviven recreación del editor) ────────────
        static std::array<MessengerEntry, SlotRegistry::kMaxSlots> s_persistentData_;
        static std::array<BusGroup, kNumBuses + 1> s_persistentGroups_;
        static int s_persistentCount_;
        static bool s_persistentReady_;

        // ─── Constantes de decaimiento fijo (independiente del target) ────
        // Este decay se aplica SIEMPRE en smoothMeters(), incluso si los datos
        // de telemetría no han cambiado. Cada frame la barra decae hacia
        // -infinito. Cuando llegan datos nuevos (de TelemetryBuffer o bg
        // thread), el attack instantáneo la sube de nuevo.
        // smoothMeters() se llama desde editor timer (60fps) + timer interno (120fps)
        // Frecuencia combinada ≈ 180 fps. 30 dB/sec / 180 = 0.167 dB/frame.
        static constexpr float kDecayDbPerFrame = 0.167f; // ~30 dB/sec combinado

        static constexpr int kMaxRowsPerBus = 1000;

        // ═══ Diagnostic logging timestamps ═════════════════════════════════
        uint32_t lastDiagLogMs_     = 0;
        uint32_t lastNoSignalLogMs_ = 0;
    };

} // namespace mixcoach
