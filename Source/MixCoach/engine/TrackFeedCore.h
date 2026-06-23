#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <functional>
#include "../../Common/types/Types.h"
#include "../../Common/memory/SharedData.h"
#include "../../Common/memory/SlotRegistry.h"
#include "TrackState.h"
#include "TrackRole.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  TrackFeedCore — Event bus + state machine + priority engine
    //
    //  Es el CEREBRO del TrackFeed. Por cada pista:
    //    1. Recibe datos crudos (updateTrackState)
    //    2. Detecta cambios y genera eventos (generateEvents)
    //    3. Computa atención (updateAttention)
    //    4. Mantiene historial de eventos por pista
    //    5. Provee consultas: getRecentEvents, getGlobalEvents, getAttentionScore
    //
    //  Thread safety:
    //    • updateTrackState() debe llamarse desde el background worker
    //    • getRecentEvents() / getGlobalEvents() se llaman desde el message thread
    //    • Se usa un mutex simple (no performance-critical, ~10μs por ciclo)
    // ═══════════════════════════════════════════════════════════════════════════
    class TrackFeedCore
    {
    public:
        TrackFeedCore()  = default;
        ~TrackFeedCore() = default;

        // ═══ NO copia ═══════════════════════════════════════════════════════════
        TrackFeedCore(const TrackFeedCore&)            = delete;
        TrackFeedCore& operator=(const TrackFeedCore&) = delete;

        // ═══ ACTUALIZACIÓN DESDE EL BACKGROUND WORKER ══════════════════════════
        /** Actualiza el estado de una pista desde el TrackAudioResult.
            Detecta cambios vs el estado anterior y genera eventos.
            Thread-safe: usa mutex interno. */
        void updateTrackState(int slotIndex, const TrackAudioResult& result, const SlotInfo& info, TrackRole role);

        /** Actualiza solo el rol de una pista (sin datos de audio).
            Puede generar evento RoleAssigned. */
        void updateTrackRole(int slotIndex, TrackRole role);

        /** Marca una pista como desaparecida (timeout / slot released). */
        void markTrackDisappeared(int slotIndex);

        /** Limpia todos los estados y eventos (reset completo). */
        void clearAll();

        // ═══ CONSULTAS (message thread) ════════════════════════════════════════

        /** Retorna el estado actual de una pista. */
        [[nodiscard]] TrackState getTrackState(int slotIndex) const;

        /** Retorna eventos recientes de una pista (últimos ~60s). */
        [[nodiscard]] std::vector<TrackEvent> getRecentEvents(int trackId, int maxEvents = 16) const;

        /** Retorna eventos GLOBALES de todas las pistas, ordenados por severidad.
            Útil para el CoachEngine: "¿qué está pasando ahora?". */
        [[nodiscard]] std::vector<TrackEvent> getGlobalEvents(int maxEvents = 32) const;

        /** Retorna el attentionScore de una pista (0.0 = baja, 1.0 = urgente). */
        [[nodiscard]] float getAttentionScore(int trackId) const;

        /** Retorna todos los trackIds activos ordenados por attentionScore (mayor primero). */
        [[nodiscard]] std::vector<int> getActiveTracksByPriority() const;

        /** Retorna el conteo de pistas por estado de salud. */
        struct HealthSummary
        {
            int clean    = 0;
            int warning  = 0; // NeedsEQ, NeedsCompression, MaskingIssue, PhaseIssue
            int critical = 0; // ClippingRisk, Overcompressed, StereoCollapse
            int silent   = 0; // LowSignal, Silent
            int unknown  = 0;
        };

        [[nodiscard]] HealthSummary getHealthSummary() const;

        // ═══ EVENTOS GLOBALES (no asociados a una pista específica) ═══════════
        /** Empuja un evento GLOBAL (ej: cambio en gap de referencia, logro, etc.).
            No está asociado a un slotIndex específico. Se incluye en la cache global. */
        void pushGlobalEvent(const TrackEvent& event);

        // ═══ SPRINT 6B: Push de eventos desde análisis externo (rol-aware) ════
        /** Empuja un evento asociado a una pista desde fuera del TrackFeedCore.
            Útil para que CoachEngine integre resultados de analyzeTrackDynamics()
            (rol-aware) que usan thresholds específicos por rol en vez de fijos.
            Thread-safe: usa mutex interno. */
        void pushTrackEvent(int slotIndex, const TrackEvent& event);

        /** Actualiza el health de una pista desde fuera del TrackFeedCore.
            Permite que análisis rol-aware (Sprint 6B) sobreescriban el health
            computado con thresholds fijos por updateTrackState().
            Thread-safe: usa mutex interno. */
        void updateTrackHealth(int slotIndex, TrackHealth health);
        bool checkAndSetCrestCooldown(int slotIndex, int64_t nowUs, int64_t cooldownUs);
        bool checkAndSetGainCooldown(int slotIndex, int64_t nowUs, int64_t cooldownUs);
        bool checkAndSetTonalCooldown(int slotIndex, int64_t nowUs, int64_t cooldownUs);
        bool checkAndSetPhaseCooldown(int slotIndex, int64_t nowUs, int64_t cooldownUs);

        // ═══ CALLBACKS ══════════════════════════════════════════════════════════
        /** Se dispara cuando se genera un evento nuevo.
            Útil para que el CoachEngine reaccione inmediatamente. */
        using EventCallback = std::function<void(const TrackEvent& event)>;

        void setEventCallback(EventCallback callback) { eventCallback_ = callback; }

        // ═══ REFERENCE AWARENESS — gap profile por región espectral ═══════════
        /** Recibe el perfil de gaps contra la referencia (6 regiones espectrales).
            Cada elemento: magnitud del gap en dB (0 = sin gap).
            updateAttention() lo usa para bonificar tracks en regiones con gaps. */
        void setReferenceGapProfile(const std::array<float, 6>& gapProfile) noexcept
        {
            referenceGapProfile_ = gapProfile;
        }

        /** Limpia el perfil de gaps (cuando se descarga la referencia). */
        void clearReferenceGapProfile() noexcept { referenceGapProfile_.fill(0.0f); }

        // ═══ SPRINT 9: Health persistence (backup files) ═══════════════════
        /** Guarda el health + attentionScore de todas las pistas activas a disco.
            Archivo: %%LOCALAPPDATA%%/MixCoach/track_health.json
            Solo persiste slots que tienen health != Unknown y health != Silent. */
        void saveHealthSnapshot();

        /** Restaura health + attentionScore desde el archivo de backup.
            Busca coincidencia por slotIndex + trackName.
            Silencioso si el archivo no existe o está corrupto. */
        void loadHealthSnapshot();

        // ═══ DEBUG / LOGGING ═══════════════════════════════════════════════════
        [[nodiscard]] int getActiveTrackCount() const;
        [[nodiscard]] int getTotalEventCount() const;

    private:
        // ═══ ESTADO INTERNO ════════════════════════════════════════════════════
        mutable juce::CriticalSection lock_;
        std::array<TrackState, SlotRegistry::kMaxSlots> states_{};
        std::array<std::vector<TrackEvent>, SlotRegistry::kMaxSlots> eventHistory_{};
        std::vector<TrackEvent> globalEvents_;     // Eventos globales (referencia, logros, etc.)
        std::vector<TrackEvent> globalEventCache_; // Cache de eventos ordenados
        bool globalCacheDirty_ = false;

        // Perfil de gaps contra la referencia (6 regiones, dB)
        std::array<float, 6> referenceGapProfile_{};

        EventCallback eventCallback_;

        // ═══ GENERACIÓN DE EVENTOS ═════════════════════════════════════════════
        void generateEvents(TrackState& prev, const TrackState& current);
        void pushEvent(int slotIndex, const TrackEvent& event);
        void pruneEventHistory(int slotIndex);
        void rebuildGlobalCache();
        void updateAttention(TrackState& state);

        // ═══ HELPERS DE COOLDOWN ═══════════════════════════════════════════════
        [[nodiscard]] bool canFireEvent(const TrackState& state,
                                        TrackEventType type,
                                        int64_t nowUs,
                                        int64_t cooldownUs = TrackStateDefaults::kCooldownWarningUs) const;

        // ═══ PESOS DE PRIORIDAD (constexpr para evitar magic numbers) ══════════
        static constexpr float kWeightLoudness  = 0.35f;
        static constexpr float kWeightSpectral  = 0.25f;
        static constexpr float kWeightStereo    = 0.15f;
        static constexpr float kWeightRole      = 0.10f;
        static constexpr float kWeightReference = 0.15f;

        // Thresholds para health scoring
        static constexpr float kClippingThreshold       = -0.5f;  // dBFS
        static constexpr float kLowSignalThreshold      = -30.0f; // dBFS
        static constexpr float kCrestOvercompressed     = 4.0f;   // dB
        static constexpr float kCrestTooDynamic         = 24.0f;  // dB
        static constexpr float kPhaseIssueThreshold     = 0.0f;   // correlation
        static constexpr float kStereoCollapseThreshold = 0.3f;   // correlation
        static constexpr float kSpectralSpreadThreshold = 15.0f;  // dB
        static constexpr float kSpectralSpreadMid       = 10.0f;  // dB
        static constexpr float kTransientThreshold      = 3.0f;   // ratio

        // ═══ HELPERS DE SEVERIDAD ══════════════════════════════════════════════
        [[nodiscard]] static float computeSeverity(float value, float threshold, float maxDeviation);
        [[nodiscard]] static float clipSeverity(float value) noexcept;
    };

} // namespace mixcoach
