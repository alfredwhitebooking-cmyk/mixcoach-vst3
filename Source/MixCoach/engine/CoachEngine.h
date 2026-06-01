#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include "../../Common/types/Types.h"
#include "../../Common/memory/SharedData.h"
#include "../../Common/memory/SlotRegistry.h"
#include "PhaseManager.h"

namespace mixcoach {

// ─── Estado de análisis por pista (evita mensajes duplicados) ───────────────
struct TrackAnalysisState {
    float lastPeakDb       = -100.0f;
    float lastRmsDb        = -100.0f;
    float lastCrestFactor  = 0.0f;
    float lastCorrelation  = 1.0f;
    float lastLufsShort    = -100.0f;
    int64_t lastWarningUs  = 0;   // cooldown por track
    bool   wasClipping     = false;
    bool   wasLowSignal    = false;
};

// ═══════════════════════════════════════════════════════════════════════════
//  Motor de IA (sistema experto basado en datos reales de telemetría)
// ═══════════════════════════════════════════════════════════════════════════
class CoachEngine
{
public:
    CoachEngine(PhaseManager& phaseManager, SharedData& sharedData);

    // ─── Interfaz pública ─────────────────────────────────────────────────
    void handleUserMessage(const juce::String& message);
    void generateProactiveTip();       // Tip único (llamado bajo demanda)
    void periodicAnalysis();           // Análisis completo (llamado desde timer, ~cada 5s)
    void checkProgress();
    void executeCommand(const juce::String& command);
    void announceNewTrack(int slotIndex, const juce::String& trackName, const juce::Colour& colour);

private:
    PhaseManager& phaseManager_;
    SharedData&   sharedData_;

    // ─── Helpers ───────────────────────────────────────────────────────────
    void respondWith(const juce::String& text, MentorMessage::Type type);
    void respondWithContext(const juce::String& text, const juce::String& context, MentorMessage::Type type);
    TrackTelemetry getLatestTelemetry(int slotIndex) const;

    // ═══ ANÁLISIS POR FASE (leen datos reales de telemetría) ═══════════════

    // Gain Staging — peaks, clipping, headroom
    void analyzeGainStagingReal();

    // Organización — conteo de pistas, buses asignados
    void analyzeOrganisationReal();

    // Balance Tonal — espectro, comparación de bandas
    void analyzeTonalBalanceReal();

    // Dinámica — crest factor, LUFS, loudness range
    void analyzeDynamicsReal();

    // Espacial / Fase — correlación, panoramas
    void analyzePhaseReal();

    // Análisis global (se ejecuta siempre, independientemente de la fase)
    void analyzeOverallMixReal();

    // Enmascaramiento espectral — pares de pistas que compiten en frecuencia
    void analyzeSpectralMaskingReal();

    // ─── Cooldowns por tipo de advertencia (evitar spam) ─────────────────
    static constexpr int64_t kWarningCooldownUs = 60 * 1000 * 1000; // 60s por tipo
    static constexpr int64_t kTrackCooldownUs   = 120 * 1000 * 1000; // 120s por track
    static constexpr int64_t kAnalysisIntervalUs = 8 * 1000 * 1000;  // 8s entre análisis

    int64_t lastPeriodicAnalysisUs_{0};
    int64_t lastPeakWarningUs_{0};
    int64_t lastCrestWarningUs_{0};
    int64_t lastPhaseWarningUs_{0};
    int64_t lastHeadroomWarningUs_{0};
    int64_t lastTonalWarningUs_{0};
    int64_t lastDynamicWarningUs_{0};
    int64_t lastLoudnessWarningUs_{0};
    int64_t lastMaskingWarningUs_{0};

    // Estado de análisis por slot
    std::array<TrackAnalysisState, SlotRegistry::kMaxSlots> trackStates_;

    // Helpers de análisis (legacy, mantienen compatibilidad)
    void analyzeGainStaging();
    void analyzeOrganisation();
    void analyzeTonalBalance();
    void analyzeDynamics();
    void analyzeSpatial();
};

} // namespace mixcoach
