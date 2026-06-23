#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include "../../Common/types/Types.h"
#include "../../Common/memory/SlotRegistry.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  Métricas por fase (V5 Progresión Granular)
// ═══════════════════════════════════════════════════════════════════════════

// ─── OrganizacionMetrics — ¿Cuántas pistas tienen rol, nombre y bus?
struct OrganizacionMetrics {
    int   totalTracks      = 0;
    int   identifiedTracks = 0;     // Pistas con rol asignado
    int   bussedTracks     = 0;     // Pistas con bus (BusType != None)
    int   namedTracks      = 0;     // Pistas con nombre no genérico
    bool  routingValidated = false; // Usuario confirmó el mapa
    bool  hasData          = false;
};

// ─── GainStagingMetrics — ¿Hay clipping? ¿Headroom saludable?
struct GainStagingMetrics {
    int   clippingCount    = 0;
    int   nearClipCount    = 0;
    int   lowSignalCount   = 0;
    float maxGlobalPeak    = -100.0f;
    int   activeCount      = 0;
    bool  hasData          = false;
};

// ─── BalanceMetrics — ¿Están los niveles relativos balanceados?
struct BalanceMetrics {
    float avgPeakBalance   = 0.0f;  // Diferencia promedio entre pares de instrumentos
    int   unbalancedPairs  = 0;     // Pares con > 6dB de diferencia
    int   totalPairs       = 0;
    bool  hasData          = false;
};

// ─── TonalMetrics — Balance espectral (EQ phase)
struct TonalMetrics {
    bool  spectralChecked  = false;
    int   excessGraves     = 0;     // Pistas con exceso de graves
    int   excessPresence   = 0;     // Pistas con exceso de presencia
    int   faltaPresencia   = 0;     // Pistas con falta de agudos
    int   maskingPairs     = 0;     // Pares con enmascaramiento
    bool  spectralTiltOk   = false; // Pendiente espectral aceptable
    bool  hasData          = false;
};

// ─── DynamicsMetrics — Crest factor, compresión
struct DynamicsMetrics {
    float avgCrestFactor  = 0.0f;
    int   overcompressed  = 0;      // Crest < 4dB
    int   undercompressed = 0;      // Crest > 18dB
    int   healthyDynamics = 0;      // Crest entre 6-14dB
    bool  hasData         = false;
};

// ─── EspacioMetrics — Estéreo, reverb, automatización
struct EspacioMetrics {
    float avgCorrelation   = 0.0f;
    float avgStereoWidth   = 0.0f;
    bool  hasSpatialFx     = false; // Reverb/delay aplicados
    bool  hasAutomation    = false; // Automatización presente
    bool  hasData          = false;
};

// ─── MasterCheckMetrics — Referencia + veredicto final
struct MasterCheckMetrics {
    bool  referenceLoaded  = false;
    bool  referenceAudio   = false;
    float matchScore       = 0.0f;
    int   gapsResolved     = 0;
    int   totalGaps        = 0;
    bool  hasData          = false;
    // Veredicto
    bool  lufsTargetMet    = false;
    bool  truePeakOk       = false;
    bool  correlationOk    = false;
};

// ═══════════════════════════════════════════════════════════════════════════
//  Gestor de fases de mentoría progresiva (V5 Progresión Granular)
// ═══════════════════════════════════════════════════════════════════════════
class PhaseManager
{
public:
    explicit PhaseManager(SlotRegistry& registry);

    // ═══ V4 Dual Mode: setear modo para bifurcar comportamiento ═══════
    void setCoachMode(CoachMode mode) noexcept { coachMode_ = mode; }
    [[nodiscard]] CoachMode getCoachMode() const noexcept { return coachMode_; }
    [[nodiscard]] bool isMixMode() const noexcept { return coachMode_ == CoachMode::Mix; }
    [[nodiscard]] bool isMasterMode() const noexcept { return coachMode_ == CoachMode::Master; }

    // Gestión de fase actual
    void               setPhase(MentorPhase phase);
    [[nodiscard]] MentorPhase getCurrentPhase() const noexcept { return currentPhase_; }
    void               advanceToNextPhase();

    // Verificaciones por fase
    [[nodiscard]] bool isPhaseComplete(MentorPhase phase) const;
    [[nodiscard]] float getPhaseProgress(MentorPhase phase) const;

    // Evaluación y avance automático (llamado desde periodicAnalysis)
    void setOrganizacionMetrics(const OrganizacionMetrics& m) noexcept { orgMetrics_ = m; }
    void setGainStagingMetrics(const GainStagingMetrics& m) noexcept { gainMetrics_ = m; }
    void setBalanceMetrics(const BalanceMetrics& m) noexcept { balanceMetrics_ = m; }
    void setTonalMetrics(const TonalMetrics& m) noexcept { tonalMetrics_ = m; }
    void setDynamicsMetrics(const DynamicsMetrics& m) noexcept { dynamicsMetrics_ = m; }
    void setEspacioMetrics(const EspacioMetrics& m) noexcept { espacioMetrics_ = m; }
    void setMasterCheckMetrics(const MasterCheckMetrics& m) noexcept { masterCheckMetrics_ = m; }

    // ═══ Master Mode metrics adicionales ════════════════════════════
    struct MasterPhaseMetrics {
        float integratedLUFS    = -100.0f;
        float truePeakDBTP      = -100.0f;
        float lra               = 0.0f;
        float correlation       = 0.0f;
        float avgStereoWidth    = 0.0f;
        float loudnessGap       = 0.0f;
        bool  hasData           = false;
    };
    void setMasterPhaseMetrics(const MasterPhaseMetrics& m) noexcept { masterMetrics_ = m; }
    void setMasterDestinationTarget(float lufsTarget, float truePeakTarget) noexcept {
        masterLufsTarget_ = lufsTarget;
        masterTruePeakTarget_ = truePeakTarget;
    }

    /** Evalúa si la fase actual está completa y avanza automáticamente. */
    bool evaluateAndAutoAdvance();

    // Eventos
    [[nodiscard]] int getAchievementCount() const noexcept { return static_cast<int>(achievements_.size()); }
    bool              unlockAchievement(Achievement achievement);

    // Criterios de cada fase
    [[nodiscard]] static int        minTracksForPhase(MentorPhase phase);
    [[nodiscard]] static const char* phaseDescription(MentorPhase phase);

    [[nodiscard]] const char* getPhaseName(MentorPhase phase) const noexcept;
    [[nodiscard]] const char* getPhaseDescription(MentorPhase phase) const noexcept;

private:
    SlotRegistry&   registry_;
    MentorPhase     currentPhase_{MentorPhase::Organizacion};
    CoachMode       coachMode_{CoachMode::Mix};

    // Métricas por fase (Mix Mode)
    OrganizacionMetrics  orgMetrics_;
    GainStagingMetrics   gainMetrics_;
    BalanceMetrics       balanceMetrics_;
    TonalMetrics         tonalMetrics_;
    DynamicsMetrics      dynamicsMetrics_;
    EspacioMetrics       espacioMetrics_;
    MasterCheckMetrics   masterCheckMetrics_;

    // Métricas Master Mode
    MasterPhaseMetrics masterMetrics_;
    float masterLufsTarget_ = -14.0f;
    float masterTruePeakTarget_ = -1.0f;

    std::vector<Achievement> achievements_;

    [[nodiscard]] bool hasAchievement(Achievement ach) const;
};

} // namespace mixcoach
