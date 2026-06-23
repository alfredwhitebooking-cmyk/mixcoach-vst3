#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include "../../Common/types/Types.h"
#include "../../Common/memory/SharedData.h"
#include "../../Common/memory/SlotRegistry.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  ImpactData — Impacto medible de un cambio del usuario en el master
    // ═══════════════════════════════════════════════════════════════════════════
    struct ImpactData
    {
        bool valid = false;
        // Correlación estéreo (master)
        float correlationBefore = 0.0f;
        float correlationAfter  = 0.0f;
        // LUFS Short-Term (master)
        float lufsBefore = -100.0f;
        float lufsAfter  = -100.0f;
        // Stereo Width promedio (master)
        float stereoWidthBefore = 0.0f;
        float stereoWidthAfter  = 0.0f;
        // Crest factor del master (peak - LUFS momentary)
        float crestBefore = 0.0f;
        float crestAfter  = 0.0f;
        // Centroide espectral ratio (actual/esperado)
        float centroidBefore = 0.0f;
        float centroidAfter  = 0.0f;
        // True Peak (master)
        float truePeakBefore = -100.0f;
        float truePeakAfter  = -100.0f;
        // LUFS Range (master)
        float lraBefore = 0.0f;
        float lraAfter  = 0.0f;

        /** Genera un resumen legible del impacto para incluir en el chat.
            Retorna string vacío si no hay cambios significativos. */
        [[nodiscard]] juce::String toImpactMessage() const;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  WorkflowAction — Qué está haciendo el usuario con una pista
    // ═══════════════════════════════════════════════════════════════════════════
    enum class WorkflowAction : uint8_t
    {
        None,
        FaderUp,        // Subió el fader (nivel general subió)
        FaderDown,      // Bajó el fader (nivel general bajó)
        EQBoost,        // Realzó frecuencia(s) específicas
        EQCut,          // Cortó frecuencia(s) específicas
        CompressionOn,  // Aplicó compresión (crest bajó significativamente)
        CompressionOff, // Redujo compresión (crest subió significativamente)
        PanLeft,        // Movió panorama a la izquierda
        PanRight,       // Movió panorama a la derecha
        StereoWiden,    // Ensanchó imagen estéreo
        Muted,          // Silenció la pista
        Unmuted,        // Reactivó audio de la pista
        TrackAdded,     // Nueva pista apareció
        TrackRemoved    // Pista desapareció
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  WorkflowEvent — Un cambio detectado, con mensaje listo para el chat
    // ═══════════════════════════════════════════════════════════════════════════
    struct WorkflowEvent
    {
        WorkflowAction action = WorkflowAction::None;
        int slotIndex         = -1;
        juce::String trackName;
        float magnitude = 0.0f; // Magnitud del cambio (dB o ratio)

        // Detalle del cambio
        juce::String metricChanged; // "Peak L", "Crest", "Sub (43-86Hz)", etc.
        float beforeValue = 0.0f;
        float afterValue  = 0.0f;

        // Región espectral afectada (para EQ)
        juce::String regionLabel;   // "Sub", "Bass", "Low-Mid", "High-Mid", "Presence", "Air"
        float frequencyHint = 0.0f; // Frecuencia aproximada

        int64_t timestampUs = 0;

        // ═══ Impacto medible en el master después del cambio ═══════════════════
        ImpactData impact;

        // ═══ Mensaje contextual listo para el chat ═════════════════════════════
        [[nodiscard]] juce::String toChatMessage(const juce::String& trackName) const;

        // ═══ Descripción corta para incluir en contexto del LLM ═══════════════
        [[nodiscard]] juce::String toShortSummary() const;

        // ═══ Para depuración ═══════════════════════════════════════════════════
        [[nodiscard]] juce::String toString() const;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  WorkflowDetector — Detecta en tiempo real qué está haciendo el usuario
    //
    //  Toma snapshots de telemetría por pista en cada ciclo y compara contra
    //  la línea base para inferir acciones del usuario.
    //
    //  Reglas de inferencia:
    //    • Fader: nivel general (peak/RMS) cambia + formas espectrales se
    //      mantienen similares (ratio > 0.85 entre perfiles)
    //    • EQ: formas espectrales cambian + nivel general se mantiene
    //      (ratio < 0.70, nivel dentro de ±1.5dB)
    //    • Compresión: crest factor cambia >3dB, transient ratio cambia
    //    • Pan: balance L/R cambia, o stereo width cambia
    //    • Mute: señal cae de activa a silencio o viceversa
    //
    //  Thread safety: usa CriticalSection interno. Llamar desde un solo thread
    //  (message thread en backgroundRunLoop o desde periodicAnalysis).
    // ═══════════════════════════════════════════════════════════════════════════
    class WorkflowDetector
    {
    public:
        WorkflowDetector()  = default;
        ~WorkflowDetector() = default;

        // ═══ Baseline — la "memoria" de cómo sonaba cada pista ═════════════════
        struct TrackBaseline
        {
            float peakCombined   = -100.0f;
            float rmsCombined    = -100.0f;
            float crestFactor    = 0.0f;
            float correlation    = 0.0f;
            float lrBalance      = 0.0f; // peakLeft - peakRight (dB)
            float avgStereoWidth = 0.0f;
            float transientRatio = 0.0f;
            float attackTimeMs   = 0.0f;
            float releaseTimeMs  = 0.0f;
            float sustainLevelDb = -100.0f;

            // 30-band spectral energy
            float bandEnergies[30] = {};

            // 6-region profile (Sub, Bass, LoMid, HiMid, Pres, Air)
            float regionProfile[6] = {};

            bool valid = false;
        };

        // ═══ Snapshot del master — estado actual de todo lo que no es por pista ═══
        struct MasterSnapshot
        {
            float peakL          = -100.0f;
            float peakR          = -100.0f;
            float rmsL           = -100.0f;
            float rmsR           = -100.0f;
            float correlation    = 0.0f;
            float lufsMomentary  = -100.0f;
            float lufsShortTerm  = -100.0f;
            float lufsIntegrated = -100.0f;
            float truePeakDBTP   = -100.0f;
            float lra            = 0.0f;
            float avgStereoWidth = 0.0f;
            float crestFactor    = 0.0f;
            float centroidRatio  = 0.0f;
            int64_t timestampUs  = 0;
            bool valid           = false;
        };

        /** Captura el snapshot actual del master ANTES de procesar cambios
            por pista. Debe llamarse una vez por ciclo, antes de los
            updateTracking() individuales.
            @param master  Métricas actuales del bus master
            @param centroidRatio Ratio del centroide espectral (actual/esperado) */
        void captureMasterSnapshot(const MasterSnapshot& master);

        // ═══ Actualiza el tracking para una pista con telemetría nueva ═════════
        void updateTracking(int slotIndex, const TrackAudioResult& result, const juce::String& trackName);

        // ═══ Detecta cambios vs baseline y retorna eventos NUEVOS ═════════════
        // (con respeto a cooldowns para no saturar el chat)
        std::vector<WorkflowEvent> detectChanges(int64_t nowUs);

        // ═══ Reset ═════════════════════════════════════════════════════════════
        void resetTrack(int slotIndex);
        void resetAll();

        // ═══ Para AiCoachAdapter: eventos recientes ════════════════════════════
        [[nodiscard]] std::vector<WorkflowEvent> getRecentEvents(int maxEvents = 16) const;

        // ═══ Cooldowns (microsegundos) ═════════════════════════════════════════
        static constexpr int64_t kFaderCooldownUs    = 30 * 1000 * 1000; // 30s
        static constexpr int64_t kEQCooldownUs       = 60 * 1000 * 1000; // 60s
        static constexpr int64_t kDynamicsCooldownUs = 90 * 1000 * 1000; // 90s
        static constexpr int64_t kPanCooldownUs      = 45 * 1000 * 1000; // 45s
        static constexpr int64_t kMuteCooldownUs     = 20 * 1000 * 1000; // 20s

        // Thresholds de detección
        static constexpr float kFaderThresholdDb      = 2.5f;   // Cambio mínimo en nivel
        static constexpr float kFaderConfirmDb        = 4.0f;   // Confirmación: cambio grande
        static constexpr float kEQLevelStabilityDb    = 1.8f;   // Nivel debe cambiar menos de esto
        static constexpr float kEQSpectralThreshold   = 3.0f;   // dB de cambio en región
        static constexpr float kCrestThresholdDb      = 3.0f;   // Cambio mínimo en crest
        static constexpr float kCrestConfirmDb        = 5.0f;   // Confirmación: cambio grande
        static constexpr float kPanThresholdDb        = 2.5f;   // Cambio en balance L/R
        static constexpr float kStereoThreshold       = 0.15f;  // Cambio en stereo width
        static constexpr float kTransientThreshold    = 1.5f;   // Cambio en transient ratio
        static constexpr float kSilenceThresholdDb    = -65.0f; // dBFS — umbral de silencio
        static constexpr float kSpectralSimilarity    = 0.80f;  // Ratio de similitud espectral
        static constexpr float kSpectralDissimilarity = 0.70f;  // Ratio de diferencia espectral

    private:
        // ═══ Estado de tracking por pista ══════════════════════════════════════
        struct TrackState
        {
            TrackBaseline baseline; // Línea base (rolling, se actualiza gradualmente)
            TrackBaseline previous; // Snapshot anterior (comparación directa)
            bool wasActive              = false;
            int64_t lastFaderEventUs    = 0;
            int64_t lastEQEventUs       = 0;
            int64_t lastDynamicsEventUs = 0;
            int64_t lastPanEventUs      = 0;
            int64_t lastMuteEventUs     = 0;
            juce::String trackName;
        };

        std::array<TrackState, SlotRegistry::kMaxSlots> tracks_;
        mutable juce::CriticalSection lock_;

        // ═══ Estado del master (antes y después del ciclo actual) ════════════
        MasterSnapshot masterBefore_;  // Capturado en captureMasterSnapshot() al inicio del ciclo
        MasterSnapshot masterCurrent_; // Se actualiza con cada captureMasterSnapshot() para el próximo ciclo

        // ═══ Buffer circular de eventos recientes ══════════════════════════════
        std::vector<WorkflowEvent> recentEvents_;
        static constexpr int kMaxRecentEvents = 64;

        // ═══ Computa el impacto en el master del último cambio detectado ═══════
        ImpactData computeImpact(const MasterSnapshot& before, const MasterSnapshot& after) const;

        // ═══ Helpers de detección individual ═══════════════════════════════════
        void detectFaderChange(TrackState& track, int slotIndex, int64_t nowUs, std::vector<WorkflowEvent>& events);
        void detectEQChange(TrackState& track, int slotIndex, int64_t nowUs, std::vector<WorkflowEvent>& events);
        void detectDynamicsChange(TrackState& track, int slotIndex, int64_t nowUs, std::vector<WorkflowEvent>& events);
        void detectPanChange(TrackState& track, int slotIndex, int64_t nowUs, std::vector<WorkflowEvent>& events);
        void detectMuteChange(TrackState& track, int slotIndex, int64_t nowUs, std::vector<WorkflowEvent>& events);

        // ═══ Helpers espectrales ═══════════════════════════════════════════════
        /** Computa similitud coseno entre dos perfiles espectrales de 30-bandas.
            1.0 = idénticos, 0.0 = completamente diferentes.
            Solo considera bandas con energía significativa (> -60dB) para evitar
            falsos positivos cuando ambos espectros están en silencio. */
        static float spectralSimilarity(const float a[30], const float b[30]);

        /** Encuentra la región (0-5) con mayor cambio y su magnitud en dB. */
        static int findWorstRegion(const float baseline[6], const float current[6], float& magnitudeDb);

        /** Convierte 30-bandas a perfil de 6 regiones. */
        static void computeRegionProfile(const float bandEnergies[30], float regionProfile[6]);

        // ═══ Helpers de nombre de región ═══════════════════════════════════════
        static const char* regionName(int idx) noexcept;
        static const char* regionFreqRange(int idx) noexcept;
        static float regionCenterFreq(int idx) noexcept;
    };

} // namespace mixcoach
