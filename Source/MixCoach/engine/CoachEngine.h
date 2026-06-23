#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <functional>
#include <vector>
#include "../../Common/types/Types.h"
#include "../../Common/memory/SharedData.h"
#include "../../Common/memory/SlotRegistry.h"
#include "../../Common/audio/AudioAnalysis.h"
#include "../../Common/audio/LoudnessAnalyzer.h"
#include "../audio/AudioAnalyzer.h"
#include "../audio/ReferenceAnalyzer.h"
#include "PhaseManager.h"
#include "TrackRole.h"
#include "SpectralProfiler.h"
#include "SemanticComparator.h"
#include "AnalyzerInterpreter.h"
#include "PlanManager.h"
#include "TrackFeedCore.h"
#include "ReferenceDrivenEngine.h"
#include "WorkflowDetector.h"
#include "DifferenceProfile.h"
#include "CorrectionLearner.h"

namespace mixcoach {

// ─── IdentityProgress (Sprint 1 — forward-declared avant class CoachEngine) ─
// Progreso de la capa de identidad: cuántas pistas tienen rol asignado y
// cuántas están confirmadas por el usuario. Definido fuera de CoachEngine
// para que MSVC compile correctamente (mismo patrón que ReferenceProgress).
struct IdentityProgress {
    int totalActive     = 0;  // Pistas activas en SlotRegistry
    int identified      = 0;  // Pistas con TrackRole != Unknown
    int confirmed       = 0;  // Pistas confirmadas (no inferidas o ya confirmadas)
    int pendingInferred = 0;  // Pistas inferidas pendientes de confirmación

    /** Retorna true si todas las pistas identificadas están confirmadas. */
    [[nodiscard]] bool allConfirmed() const noexcept {
        return totalActive > 0 && pendingInferred == 0 && identified > 0;
    }
};

// ─── ReferenceProgress (forward-declared avant class CoachEngine) ──────
// Progreso contra la referencia para Reference-Driven Mode.
// Definido fuera de CoachEngine para que MSVC compile correctamente.
struct ReferenceProgress {
    float currentMatch   = 0.0f;  // 0.0-1.0 que tan cerca estamos
    float previousMatch  = 0.0f;  // Valor de la medición anterior
    float delta          = 0.0f;  // + = mejorando, - = empeorando
    int   totalGaps      = 0;     // Gaps activos
    int   resolvedGaps   = 0;     // Gaps que se resolvieron desde el inicio
    int   criticalGaps   = 0;     // Gaps críticos actuales
    int   warningGaps    = 0;     // Gaps warning actuales
    bool  hasReference   = false; // Hay referencia cargada?
    bool  hasAudio       = false; // Hay fingerprint de audio?

    /** Retorna emoji de tendencia: ▲ mejorando, ▼ empeorando, ➡ estable. */
    [[nodiscard]] const char* trendEmoji() const noexcept {
        if (delta > 0.03f) return "\xE2\x96\xB2";   // ▲
        if (delta < -0.03f) return "\xE2\x96\xBC";  // ▼
        return "\xE2\x9E\xA1";                      // ➡
    }

    /** Retorna label de tendencia: "improving", "worsening", "stable". */
    [[nodiscard]] const char* trendLabel() const noexcept {
        if (delta > 0.03f) return "improving";
        if (delta < -0.03f) return "worsening";
        return "stable";
    }

    /** Retorna texto formateado para el LLM context. */
    [[nodiscard]] juce::String toLLMContext() const
    {
        if (!hasAudio || !hasReference)
            return {};
        juce::String s;
        s += "[REFERENCE MATCH PROGRESS]\n";
        s += "  Overall match: " + juce::String(static_cast<int>(currentMatch * 100.0f)) + "%\n";
        s += "  Trend: " + juce::String(trendEmoji()) + " " + juce::String(trendLabel());
        if (delta != 0.0f)
            s += " (" + juce::String(delta * 100.0f, 1) + "%)";
        s += "\n";
        s += "  Gaps: " + juce::String(totalGaps) + " total, "
             + juce::String(criticalGaps) + " critical, "
             + juce::String(warningGaps) + " warning, "
             + juce::String(resolvedGaps) + " resolved\n";
        return s;
    }
};



// ─── Estado de análisis por pista V2 (solo RMS + Peak) ───────────────────
struct TrackAnalysisState {
    float lastPeakDb        = -100.0f;
    float lastRmsDb         = -100.0f;
    int64_t lastWarningUs   = 0;   // cooldown por track
    bool   wasClipping      = false;
    bool   wasLowSignal     = false;
    float lastCrestFactor   = 0.0f;
    float lastLufsShort     = -100.0f;
    float lastCorrelation   = 0.0f;
    // ═══ (reserved for future IIR temporal smoothing of per-track LUFS) ═══
};

// ═══════════════════════════════════════════════════════════════════════════
//  TrackRecommendation — Recomendación activa para Loop de Corrección
//  Soporta 4 dominios: Gain (fader), Tonal (EQ), Dynamics (compresión), Spatial (paneo)
//  Cada dominio usa su propia métrica de verificación.
// ═══════════════════════════════════════════════════════════════════════════
struct TrackRecommendation {
    int     slotIndex       = -1;
    int64_t timestampUs     = 0;
    juce::String trackName;
    juce::String action;
    float   beforeValue     = 0.0f;
    float   expectedAfter   = 0.0f;
    float   delta           = 0.0f;

    // ─── Dominio de la recomendación ──────────────────────────────────
    enum class Domain : uint8_t {
        Gain,       // Fader gain / nivel (verifica: peak)
        Tonal,      // EQ / balance espectral (verifica: bandEnergy[b])
        Dynamics,   // Compresión / crest (verifica: crestDb)
        Spatial     // Paneo / estéreo (verifica: correlation o LR balance)
    };
    Domain domain = Domain::Gain;

    // ─── Parámetros de verificación ───────────────────────────────────
    // Qué métrica usar para verificar la corrección
    juce::String verifyMetric;   // "peak", "band_0".."band_5", "crest", "correlation", "lr_balance"
    float verifyInitial  = 0.0f; // Valor al momento de la recomendación
    float verifyCurrent  = 0.0f; // Valor actual (actualizado en verifyTrackCorrections)

    // Parámetros específicos (para EQ)
    float frequencyHz    = 0.0f; // Frecuencia central (Hz) para EQ
    int   spectralBand   = -1;   // Índice de banda espectral (0-5), -1 si no aplica

    enum class Status : uint8_t {
        Pending, Applied, OverApplied, UnderApplied, Ignored, Superseded
    };
    Status status = Status::Pending;
    bool   feedbackSent = false;
    uint8_t verifyRetries = 0;  // Contador de re-verificaciones (max 3)
    juce::String feedbackMessage;

    // ─── Helpers ──────────────────────────────────────────────────────
    [[nodiscard]] static const char* domainName(Domain d) noexcept {
        switch (d) {
            case Domain::Gain:     return "gain";
            case Domain::Tonal:    return "tonal";
            case Domain::Dynamics: return "dynamics";
            case Domain::Spatial:  return "spatial";
            default:               return "unknown";
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════
//  ReferenceFingerprint — Análisis espectral y de loudness de un archivo
//  de referencia, extraído al cargar un WAV/MP3/FLAC en el ReferencePanel.
//  El CoachEngine lo usa para comparar la mezcla actual contra la referencia.
// ═══════════════════════════════════════════════════════════════════════════
struct ReferenceFingerprint {
    // 30 bandas de frecuencia (definidas en Constants.h kSpectralBandBins / kNumSpectralBands)
    // Mismas bandas que el análisis per-track en backgroundRunLoop
    float bandEnergies[30] = { -100.0f, -100.0f, -100.0f, -100.0f,
                               -100.0f, -100.0f, -100.0f, -100.0f,
                               -100.0f, -100.0f, -100.0f, -100.0f,
                               -100.0f, -100.0f, -100.0f, -100.0f,
                               -100.0f, -100.0f, -100.0f, -100.0f,
                               -100.0f, -100.0f, -100.0f, -100.0f,
                               -100.0f, -100.0f, -100.0f, -100.0f,
                               -100.0f, -100.0f };

    // Loudness profile
    float lufsMomentary  = -100.0f;
    float lufsShortTerm  = -100.0f;
    float lufsIntegrated = -100.0f;
    float lufsRange      = 0.0f;

    // Crest factor (Peak - RMS)
    float crestFactor    = 0.0f;

    // Correlación estéreo promedio
    float correlation    = 0.0f;

    // True peak
    float truePeakDBTP   = -100.0f;

    // Centroide espectral estimado desde bandEnergies[30] (Hz)
    // Se computa al analizar la referencia y se usa para calibrar
    // el expected centroid en vez del valor fijo por género.
    float spectralCentroidHz = 0.0f;

    bool valid = false;
};

// ═══════════════════════════════════════════════════════════════════════════
//  ReferenceMatchData — Datos de comparación mix vs referencia
//  Se computa en CoachEngine y se envía al ReferencePanelComponent
//  para visualización del matching espectral y de loudness.
// ═══════════════════════════════════════════════════════════════════════════
struct ReferenceMatchData {
    // 6 regiones espectrales (Sub, Bass, LoMid, HiMid, Pres, Air)
    float mixRegionEnergy[6] = { -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f };
    float refRegionEnergy[6] = { -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f };

    // LUFS
    float mixLUFS     = -100.0f;
    float refLUFS     = -100.0f;

    // Crest factor
    float mixCrest    = 0.0f;
    float refCrest    = 0.0f;

    // Correlación estéreo
    float mixCorrelation = 0.0f;
    float refCorrelation = 0.0f;

    // Nombre de la referencia activa
    juce::String refName;

    bool valid = false;  // false si no hay referencia cargada o datos insuficientes
};

// ═══════════════════════════════════════════════════════════════════════════
//  ReferenceMetadata — Metadatos de la referencia (archivo o URL)
// ═══════════════════════════════════════════════════════════════════════════
struct ReferenceMetadata {
    enum class Type { None, File, URL };

    Type        type    = Type::None;
    juce::String name;   // Nombre del archivo o nombre inferido de la URL
    juce::String path;   // Ruta del archivo o URL limpia

    // Para URLs:
    juce::String platform; // "YouTube", "Spotify", "SoundCloud", etc.
    juce::String genre;    // Género inferido (pop, rock, edm, etc.)

    bool valid() const { return type != Type::None && name.isNotEmpty(); }
};

// ═══════════════════════════════════════════════════════════════════════════
//  BusGroupSummary — Métricas agregadas por familia de instrumentos
//  Computado por computeBusSummaries() a partir de telemetría individual
//  Agrupa tracks por BusType y calcula promedios/sumas para análisis de mezcla
// ═══════════════════════════════════════════════════════════════════════════
struct BusGroupSummary {
    BusType     busType       = BusType::None;
    int         trackCount    = 0;
    float       peakMax       = -100.0f;   // Máximo peak del grupo
    float       rmsSum        = -100.0f;   // RMS sumado (lineal) en dB
    float       avgCrest      = 0.0f;
    float       avgCorrelation = 0.0f;
    float       avgBandEnergies[30] = { -100.0f, -100.0f, -100.0f, -100.0f,
                                        -100.0f, -100.0f, -100.0f, -100.0f,
                                        -100.0f, -100.0f, -100.0f, -100.0f,
                                        -100.0f, -100.0f, -100.0f, -100.0f,
                                        -100.0f, -100.0f, -100.0f, -100.0f,
                                        -100.0f, -100.0f, -100.0f, -100.0f,
                                        -100.0f, -100.0f, -100.0f, -100.0f,
                                        -100.0f, -100.0f };
    juce::String loudestTrackName;
    float       loudestTrackPeak = -100.0f;

    [[nodiscard]] bool hasData() const noexcept { return trackCount > 0 && peakMax > -90.0f; }
};

// ═══════════════════════════════════════════════════════════════════════════
//  SessionContext — Fotografía completa de la sesión en un solo struct
//  Se usa para:
//    • Dar contexto al LLM antes de generar respuestas
//    • Mostrar resumen de sesión en el chat (/status, /map)
//    • Alimentar el WorkflowDetector con información agregada
//
//  Se construye llamando a CoachEngine::buildSessionContext().
// ═══════════════════════════════════════════════════════════════════════════
struct SessionContext {
    // ═══ Modo y género ═══════════════════════════════════════════════════════
    CoachMode        coachMode      = CoachMode::Mix;
    juce::String     genre;                         // Género de la sesión

    // ═══ Fase y progreso ════════════════════════════════════════════════════
    MentorPhase      currentPhase   = MentorPhase::Organizacion;
    float            phaseProgress  = 0.0f;         // 0.0-1.0
    int              achievementCount = 0;

    // ═══ Conteo de pistas por categoría ═════════════════════════════════════
    int              drumTracks     = 0;
    int              bassTracks     = 0;
    int              guitarTracks   = 0;
    int              keysTracks     = 0;
    int              vocalTracks    = 0;
    int              fxTracks       = 0;
    int              melodyTracks   = 0;
    int              unknownTracks  = 0;
    int              totalTracks    = 0;

    // ═══ Organización ═══════════════════════════════════════════════════════
    int              bussedTracks   = 0;             // Con bus asignado
    int              namedTracks    = 0;             // Con nombre no genérico
    int              rolesAssigned  = 0;             // TrackRole != Unknown

    // ═══ Gain staging ═══════════════════════════════════════════════════════
    int              clippingTracks = 0;             // Peak > -0.5 dB
    int              nearClipTracks = 0;             // Peak entre -0.5 y -3.0
    int              lowSignalTracks = 0;            // Peak < -30 dB
    int              healthyTracks  = 0;             // En rango óptimo

    // ═══ Master metrics ═════════════════════════════════════════════════════
    float            masterPeak     = -100.0f;       // dBFS
    float            masterRMS      = -100.0f;
    float            masterCrest    = 0.0f;          // dB
    float            masterCorrelation = 0.0f;
    float            masterLUFS     = -100.0f;       // Short-term
    float            masterIntegratedLUFS = -100.0f;
    float            masterTruePeak = -100.0f;       // dBTP
    float            masterStereoWidth = 0.0f;

    // ═══ Referencia ═════════════════════════════════════════════════════════
    bool             referenceLoaded   = false;
    bool             referenceAudio    = false;      // Archivo de audio (no URL)
    juce::String     referenceName;
    juce::String     referenceGenre;
    float            matchScore        = 0.0f;       // 0.0-1.0
    int              referenceGaps     = 0;

    // ═══ Correcciones ═══════════════════════════════════════════════════════
    int              pendingCorrections = 0;
    int              appliedCorrections = 0;

    // ═══ Issues activos ═════════════════════════════════════════════════════
    int              criticalIssues  = 0;            // severity >= 0.8
    int              warningIssues   = 0;            // severity 0.4-0.7
    int              infoIssues      = 0;            // severity < 0.4

    // ═══ Workflow ═══════════════════════════════════════════════════════════
    int64_t          sessionDurationUs = 0;           // Tiempo desde el inicio
    int              recentUserActions = 0;           // Acciones detectadas en últimos 60s

    /** Genera un resumen formateado para mostrar en el chat (/status mejorado). */
    [[nodiscard]] juce::String toChatMessage() const;

    /** Genera un bloque de contexto estructurado para el LLM.
        Incluye toda la información relevante para que el LLM entienda
        el estado actual de la sesión sin tener que "investigar". */
    [[nodiscard]] juce::String toLLMContext() const;

    /** Retorna true si hay al menos una pista con datos. */
    [[nodiscard]] bool hasData() const noexcept { return totalTracks > 0; }
};

// ═══════════════════════════════════════════════════════════════════════════
//  SessionMapEntry — Una pista en el mapa jerárquico de sesión
//  Serializable a JSON para persistencia en session_memory.json
// ═══════════════════════════════════════════════════════════════════════════
struct SessionMapEntry {
    juce::String  trackName;
    juce::String  roleName;      // Display name ("Kick", "Vocal", etc.)
    int           slotIndex  = -1;
    int           busType    = 0; // BusType as int for JSON serialization
    int           trackType  = -1; // TrackType del Messenger (V7 Identity Layer)
    float         confidence = 0.0f; // Confianza de la inferencia (0.0-1.0)
    float         peakDb     = -100.0f;
    bool          hasSignal  = false;

    /** Serializa esta entrada a un DynamicObject. */
    void toJson(juce::DynamicObject& obj) const;

    /** Deserializa desde un DynamicObject. */
    static SessionMapEntry fromJson(const juce::DynamicObject& obj);
};

// ═══════════════════════════════════════════════════════════════════════════
//  SessionMapCategory — Una categoría de instrumentos (Drums, Bass, Vocals...)
// ═══════════════════════════════════════════════════════════════════════════
struct SessionMapCategory {
    juce::String name;          // "BATERIA", "BAJO", "VOCES", etc.
    juce::String emoji;         // Emoji para display
    std::vector<SessionMapEntry> tracks;

    /** Serializa esta categoría a un DynamicObject. */
    void toJson(juce::DynamicObject& obj) const;

    /** Deserializa desde un DynamicObject. */
    static SessionMapCategory fromJson(const juce::DynamicObject& obj);
};

// ═══════════════════════════════════════════════════════════════════════════
//  SessionMap — Mapa jerárquico completo de la sesión
//  Agrupa pistas por RoleCategory con roles, nombres, buses y niveles.
//  Es persistente: se guarda en session_memory.json y se restaura
//  al cargar la sesión.
// ═══════════════════════════════════════════════════════════════════════════
struct SessionMap {
    int64_t  timestampUs  = 0;
    int      totalTracks  = 0;
    std::vector<SessionMapCategory> categories;

    /** Retorna true si hay al menos una pista en el mapa. */
    [[nodiscard]] bool hasData() const noexcept { return totalTracks > 0; }

    /** Serializa el mapa completo a un DynamicObject. */
    void toJson(juce::DynamicObject& obj) const;

    /** Deserializa desde un DynamicObject. */
    static SessionMap fromJson(const juce::DynamicObject& obj);

    /** Genera el árbol textual formateado (emojis + conectores).
        Reemplaza la lógica anterior de buildSessionMapText(). */
    [[nodiscard]] juce::String toText() const;
};

// ═══════════════════════════════════════════════════════════════════════════
//  Motor de IA (sistema experto basado en datos reales de telemetría)
//  Ahora recibe AudioAnalyzer del Master para análisis globales
// ═══════════════════════════════════════════════════════════════════════════
class CoachEngine
{
public:
    // ═══ Helpers de bus (shared between CoachEngine.cpp and CoachEngineSetup.cpp) ═══
    static juce::String getBusIcon(BusType bus) noexcept;

    // ═══ Callback para cambios en tracks (registrado por AiCoachAdapter) ═══
    using TrackChangeCallback = std::function<void(int slotIndex,
                                                    const juce::String& trackName,
                                                    const juce::String& description,
                                                    float beforeValue,
                                                    float afterValue)>;
    void setTrackChangeCallback(TrackChangeCallback callback) { trackChangeCallback_ = callback; }

    // ═══ Callback para obtener el historial de sesión (desde AiCoachAdapter) ═══
    using SessionQueryCallback = std::function<juce::String()>;
    void setSessionQueryCallback(SessionQueryCallback callback) { sessionQueryCallback_ = callback; }

    // ═══ Callback para respuestas vía LLM ═══
    // Retorna: true si aceptó el mensaje y llamará al callback de respuesta,
    //          false si no puede procesarlo (cae a respuestas por reglas).
    using LlmResponseCallback = std::function<bool(const juce::String& userMessage,
                                                     std::function<void(const juce::String&)>)>;
    void setLlmResponseCallback(LlmResponseCallback callback) { llmResponseCallback_ = callback; }

    // ═══ Callback para respuestas vía LLM con STREAMING ═══
    // Retorna: true si aceptó el mensaje, false si no puede procesarlo.
    // onToken se llama por cada token recibido (en message thread).
    // onComplete se llama cuando termina (success, fullResponse).
    using LlmStreamingCallback = std::function<bool(
        const juce::String& userMessage,
        std::function<void(const juce::String& token)> onToken,
        std::function<void(const juce::String& fullResponse)> onComplete)>;
    void setLlmStreamingCallback(LlmStreamingCallback callback) { llmStreamingCallback_ = callback; }

    void setLlmEnabled(bool enabled) noexcept { llmEnabled_ = enabled; }
    void setProactiveAnalysisEnabled(bool enabled) noexcept { proactiveAnalysisEnabled_ = enabled; }
    [[nodiscard]] bool isProactiveAnalysisEnabled() const noexcept { return proactiveAnalysisEnabled_; }

    // ═══ Callback para actualizar el DiagnosticBridge (overlay visual) ═══
    // Se dispara después de handleUserMessage() y periodicAnalysis() para
    // que los overlays del spectrograph se actualicen con baja latencia.
    // Reemplaza el timer lento (~5s) que usaba pushDiagnosticBridge().
    using DiagnosticUpdateCallback = std::function<void()>;
    void setDiagnosticUpdateCallback(DiagnosticUpdateCallback callback) { diagnosticUpdateCb_ = callback; }

    // ═══ Callback para mensajes del coach al chat UI ═══
    // El bool indica si es mensaje del sistema (true) o respuesta LLM (false).
    // Los mensajes del sistema se renderizan en formato compacto y dimmed.
    using MessagePushedCallback = std::function<void(const juce::String&, bool isSystem)>;
    void setMessagePushedCallback(MessagePushedCallback callback) { messagePushedCallback_ = callback; }

    // ═══ Callbacks para streaming de tokens del LLM al chat UI ═══
    /** Se llama cuando empieza un nuevo mensaje en streaming (abre la burbuja). */
    using StreamStartedCallback = std::function<void()>;
    /** Se llama por cada token recibido. */
    using StreamTokenCallback = std::function<void(const juce::String& token)>;
    /** Se llama cuando el streaming termina (finaliza la burbuja). */
    using StreamEndedCallback = std::function<void()>;

    void setStreamingCallbacks(StreamStartedCallback onStarted,
                               StreamTokenCallback onToken,
                               StreamEndedCallback onEnded)
    {
        streamStartedCb_ = onStarted;
        streamTokenCb_ = onToken;
        streamEndedCb_ = onEnded;
    }

    // ═══ Centroid Info — centroide espectral actual vs esperado por género ═══
    struct CentroidInfo {
        float actualHz   = 0.0f;
        float expectedHz = 0.0f;
        juce::String genre;
        bool valid() const noexcept { return actualHz > 0.0f && expectedHz > 0.0f; }
    };

    /** Retorna el centroide espectral esperado (Hz) para un género.
        Útil para computar centroidRatio per-track y en overlay visual. */
    static float expectedCentroidForGenre(const juce::String& genre) noexcept;

    /** Retorna el centroide espectral actual y el esperado para el género.
        Útil para el overlay visual en el SpectrographComponent. */
    [[nodiscard]] CentroidInfo getCentroidInfo(const juce::String& genre = {}) const;

    // ═══ Per-track LUFS approximation helper ═══════════════════════════
    /** Computa una aproximacion de LUFS per-pista usando RMS + K-weighting
        estimado desde bandEnergies[30]. Retorna -100.0f si no hay datos.
        La aproximacion es:
          lufsMomentary ≈ monoSumRMS(dB) + highFreqBoost(0-2.5dB)
        El highFreqBoost simula el pre-filter K-weighting de EBU R128. */
    static float computePerTrackLUFS(const TrackAudioResult& result) noexcept;

    /**
     * Retorna la lista de interpretaciones actuales de los analizadores (Nivel 4).
     * Útil para generar mensajes de coach enriquecidos con consecuencias y acciones.
     */
    std::vector<AnalyzerInterpretation> getCurrentInterpretations(const juce::String& genre = {}) const;

    // ═══ Callback para datos de matching espectral (ReferenceMatchPanel) ═══
    using MatchDataCallback = std::function<void(const DifferenceProfile&)>;
    void setMatchDataCallback(MatchDataCallback callback) { matchDataCallback_ = callback; }
    [[nodiscard]] bool isLlmEnabled() const noexcept { return llmEnabled_; }

    // ═══ Callback para consultas LLM durante el setup (FASE 0) ═══
    using SetupLlmCallback = std::function<void(const juce::String& prompt,
                                                  std::function<void(const juce::String&)>)>;
    void setSetupLlmCallback(SetupLlmCallback callback) { setupLlmCallback_ = callback; }
    [[nodiscard]] bool hasSetupLlm() const noexcept { return setupLlmCallback_ != nullptr; }

    // ═══ Callback para sugerencia inteligente de roles via LLM ═══
    // Se usa cuando hay track names pero no roles asignados.
    // El callback recibe un prompt con nombres de pista + genero,
    // y el LLM responde con sugerencias de TrackRole.
    using RoleSuggestionCallback = std::function<void(const juce::String& prompt,
                                                       std::function<void(const juce::String&)>)>;
    void setRoleSuggestionCallback(RoleSuggestionCallback callback) { roleSuggestionCallback_ = callback; }
    [[nodiscard]] bool hasRoleSuggestionCallback() const noexcept { return roleSuggestionCallback_ != nullptr; }

    /** Envia los nombres de las pistas desconocidas al LLM para que sugiera roles.
        Construye un prompt con nombres + genero, llama al callback, y procesa la respuesta.
        Solo se ejecuta una vez por sesion (flag roleSuggestionRequested_).
        Ademas, si hay suficientes pistas con nombre, pide al LLM que detecte
        el patron de naming del usuario y lo almacena en namingPattern_. */
    void requestLLMRoleSuggestions();

    // ═══ Naming Pattern Detection V12 — detecta patrones de naming del usuario ═══
    /** Almacena el patron de naming detectado por el LLM.
        Ej: "Usuario nombra pistas como [Instrumento]_[Numero] (Kick_01, Snare_02)"
        Se llena automaticamente desde requestLLMRoleSuggestions(). */
    juce::String namingPattern_;

    /** Flag para evitar deteccion repetida de patron de naming.
        Se setea a true despues de la primera deteccion. */
    bool namingPatternDetected_ = false;

    /** Retorna el patron de naming detectado por el LLM (vacío si aun no detectado). */
    [[nodiscard]] const juce::String& getNamingPattern() const noexcept { return namingPattern_; }

    /** Retorna true si ya se detecto el patron de naming del usuario. */
    [[nodiscard]] bool hasNamingPattern() const noexcept {
        return namingPatternDetected_ && namingPattern_.isNotEmpty();
    }

    // ═══ IDENTITY LAYER — Inferencia inteligente de roles (V3 combinada) ═══
    // Resultado de inferencia con método y confianza
    struct NameInferenceResult {
        TrackRole role       = TrackRole::Unknown;
        float     confidence = 0.0f;  // 0.0-1.0
        bool      fromName   = false; // true = inferido del nombre, false = espectral
        [[nodiscard]] bool isValid() const noexcept {
            return role != TrackRole::Unknown && role != TrackRole::Master;
        }
    };

    /** Infiere TrackRole desde el nombre de la pista usando keywords multilingüe (EN+ES).
        Busca palabras clave, prefijos y sufijos comunes.
        Retorna Unknown si no puede determinar con al menos 0.3 de confianza. */
    static NameInferenceResult inferTrackRoleFromName(const juce::String& trackName) noexcept;

    /** Estrategia combinada: nombre primero (≥0.7 conf) → espectral como fallback.
        Si el nombre da confianza baja (<0.7) pero espectral da un rol, usa espectral.
        Si ambos fallan, retorna Unknown.
        @param trackName  Nombre de la pista (de Messenger)
        @param spectral   Perfil espectral (de SpectralProfiler::computeProfile) */
    static NameInferenceResult inferTrackRoleCombined(const juce::String& trackName,
                                                       const TrackSpectralProfile& spectral) noexcept;

    /** Muestra resumen de identidad en el chat: "Detecté: 4 drums, 1 bass..."
        Agrupa por categoría, muestra confianza visual (✅ ⚠️ ❌),
        y pregunta si es correcto. Se llama desde advanceFromSetup() y
        cuando se infieren roles nuevos. */
    void showIdentitySummary();

    // ═══ Nombre del ingeniero (guardado localmente para flujo returning user) ═══
    void setEngineerName(const juce::String& name) { engineerName_ = name; }
    [[nodiscard]] const juce::String& getEngineerName() const noexcept { return engineerName_; }
    [[nodiscard]] bool hasEngineerName() const noexcept { return engineerName_.isNotEmpty(); }

    // ═══ Callback para guardar el nombre del ingeniero ═══
    // Se llama desde detectAndSetEngineerName() en handleUserMessage().
    using EngineerNameCallback = std::function<void(const juce::String& name)>;
    void setEngineerNameCallback(EngineerNameCallback callback) { engineerNameCallback_ = callback; }

    // ═══ FASE 0: Setup Steps — Diálogo interactivo de bienvenida ═══════
    // V4 Dual Mode: el primer paso ahora es elegir Mix Mode o Master Mode
    enum class SetupStep : uint8_t {
        NotStarted,          // No greeting sent yet
        WaitingForName,      // Preguntar nombre del ingeniero
        WaitingForMode,      // V4: Preguntar Mix o Master
        WaitingForGenre,     // Asked for genre (Mix Mode)
        WaitingForConfirm,   // Genre set, asking to scan/confirm
        WaitingForDestination, // V4: Preguntar destino (Master Mode only)
        Complete             // Setup done, ready to advance
    };

    CoachEngine(PhaseManager& phaseManager, SharedData& sharedData, AudioAnalyzer& audioAnalyzer);

    // ─── Interfaz pública ─────────────────────────────────────────────────
    void handleUserMessage(const juce::String& message);
    void generateProactiveTip();
    void periodicAnalysis();
    /** Fast Layer (~2s): Solo verifica peak/RMS/correlation por pista.
        Detecta cambios rapidos del usuario sin esperar el ciclo profundo de 8s.
        Se llama desde el timer del editor a mayor frecuencia que periodicAnalysis(). */
    void fastTrackAnalysis();
    void checkProgress();
    void executeCommand(const juce::String& command);
    void announceNewTrack(int slotIndex, const juce::String& trackName, const juce::Colour& colour);

    // ═══ Proactividad — El coach interrumpe sin esperar preguntas ══════
    /** Marca que el usuario interactuó (resetea el timer de inactividad). */
    void setUserInteracted() noexcept
    {
        lastUserInteractionTimeUs_ = juce::Time::getMillisecondCounter() * 1000;
    }
    /** Envía un tip proactivo si el usuario está inactivo y hay algo relevante. */
    void checkAndSendProactiveTip();

    // ═══ Perfiles por género — Targets de mezcla según estilo musical ═════
    struct GenreTargetProfile {
        float targetIntegratedLUFS;    // LUFS target (e.g. -8.0 for Reggaeton)
        float lufsTolerance;           // ± LUFS before warning
        float targetCrestFactor;       // Target crest factor in dB
        float crestTolerance;          // ± dB before warning
        float targetHeadroomDb;        // Target master peak headroom
        const char* description;       // One-line sonic signature
        // Spectral tilt (dB offset per broad region, relative to flat)
        float subBassOffset;    // 0-86Hz
        float bassOffset;       // 86-301Hz
        float lowMidOffset;     // 301-1076Hz
        float highMidOffset;    // 1076-3532Hz
        float presenceOffset;   // 3532-8355Hz
        float airOffset;        // 8355-16458Hz
    };

    /** Retorna el perfil objetivo para un género dado. Si no se encuentra, retorna perfil genérico. */
    static const GenreTargetProfile& getGenreProfile(const juce::String& genre);
    /** Retorna la lista de géneros conocidos. */
    static juce::StringArray getKnownGenres();

    // ═══ Setup state (público para AiCoachAdapter y editor) ═══════════
    /** Inicia el diálogo de bienvenida. Pregunta género musical al usuario. */
    void startSetupDialogue();
    [[nodiscard]] SetupStep getSetupStep() const noexcept { return setupStep_; }
    [[nodiscard]] const juce::String& getSetupGenre() const noexcept { return setupGenre_; }
    void forceSetupComplete(); // For /skip or cuando el usuario quiera saltarse el setup

    // ═══ Sprint 2: Confirmar Mapa — Marca el ruteo como validado y avanza fase ═══
    /** Called when the user confirms the mix map (all bus assignments correct).
        Updates OrganizacionMetrics with routingValidated=true and triggers
        evaluateAndAutoAdvance(). If the phase advances (Organizacion → GainStaging),
        sends a celebratory message and phase guidance. */
    void onMapConfirmed();

    // ═══ Referencias — Recibir datos desde el ReferencePanelComponent ═════
    void setReferenceAudio(const juce::String& filePath);
    void setReferenceURL(const juce::String& name, const juce::String& url);
    void clearReferences();

    /** Limpia el tracking de gaps previos (al cambiar de referencia o reiniciar). */
    void clearReferenceGapTracking() noexcept { previousReferenceGaps_.clear(); }
    [[nodiscard]] juce::String getReferenceName() const;
    [[nodiscard]] juce::String getReferenceGenre() const { return referenceMetadata_.genre; }
    [[nodiscard]] juce::String getReferencePlatform() const { return referenceMetadata_.platform; }
    [[nodiscard]] bool hasReference() const { return referenceMetadata_.valid(); }
    [[nodiscard]] bool hasReferenceAudio() const { return referenceFingerprint_.valid; }
    /** Retorna la comparación más reciente mix vs referencia (ReferenceAnalyzer). */
    [[nodiscard]] const ReferenceComparison& getReferenceComparison() const noexcept {
        return lastReferenceComparison_;
    }

    /** Retorna el AudioAnalyzer (master) para acceso a LUFS, correlación, etc. */
    [[nodiscard]] AudioAnalyzer& getAudioAnalyzer() noexcept { return audioAnalyzer_; }
    [[nodiscard]] const AudioAnalyzer& getAudioAnalyzer() const noexcept { return audioAnalyzer_; }

    /** Retorna la similitud espectral vs referencia (0.0-1.0).
        Retorna 0.0 si no hay referencia disponible. */
    [[nodiscard]] float getSpectralSimilarity() const noexcept {
        return lastReferenceComparison_.spectralSimilarity;
    }
    /** Retorna el ReferenceAnalyzer para acceso externo. */
    [[nodiscard]] ReferenceAnalyzer& getReferenceAnalyzer() noexcept {
        return refAnalyzer_;
    }
    [[nodiscard]] const ReferenceFingerprint& getReferenceFingerprint() const { return referenceFingerprint_; }

    /** Retorna el fingerprint activo (seccion seleccionada o global si -1).
        Nota: referenceSections_[0] = "Full" fingerprint, 
        referenceSections_[1..N] = detected sections.
        activeSectionIndex_ 0..N-1 maps to referenceSections_[1..N]. */
    [[nodiscard]] const ReferenceFingerprint& getActiveFingerprint() const {
        // activeSectionIndex_ 0..N-1 → referenceSections_[activeSectionIndex_ + 1]
        int idx = activeSectionIndex_ + 1;
        if (activeSectionIndex_ >= 0 && idx < (int)referenceSections_.size())
            return referenceSections_[idx].fingerprint;
        return referenceFingerprint_;
    }

    // ═══ Referencia: acceso para background worker ═══════════════════════════
    /** Retorna true si hay una referencia pendiente de analizar. */
    [[nodiscard]] bool hasPendingReference() const noexcept {
        return pendingReferencePath_.isNotEmpty();
    }

    /** Consume la ruta de referencia pendiente y la limpia.
        Thread-safe: solo lectura/escritura atómica de juce::String desde un solo thread.
        El bg worker o el timer pueden consumirla. */
    juce::String consumePendingReferencePath() noexcept {
        juce::String path = pendingReferencePath_;
        pendingReferencePath_.clear();
        return path;
    }

    /** Ejecuta analyzeReferenceFile() con proteccion SEH.
        Puede llamarse desde background worker o desde el timer. */
    void applyReferenceAnalysis(const juce::String& filePath);

    /** Ejecuta analyzeReferenceBuffer() con proteccion SEH.
        Usa el buffer ya cargado por ReferenceAudioPlayer para evitar
        una segunda lectura de disco. */
    void applyReferenceAnalysis(const float* bufferL, const float* bufferR,
                                 int64_t numSamples, int numChannels,
                                 double sampleRate,
                                 const juce::String& filePath);

    // ═══ Secciones de referencia — acceso público ════════════════════════════
    /** Fingerprint de una seccion individual de la referencia (ej: coro, verso). */
    struct SectionFingerprint {
        float startSeconds = 0.0f;
        float endSeconds = 0.0f;
        ReferenceFingerprint fingerprint;
        juce::String label; // "Full", "Section 1", "Section 2", ...
    };

    /** Cambia la seccion activa. -1 = fingerprint global. */
    void setActiveSection(int index) noexcept { activeSectionIndex_ = index; }
    /** Retorna la seccion activa (-1 = fingerprint global). */
    int getActiveSection() const noexcept { return activeSectionIndex_; }
    /** Retorna las secciones calculadas de la referencia. */
    [[nodiscard]] const std::vector<SectionFingerprint>& getReferenceSections() const { return referenceSections_; }
    [[nodiscard]] std::array<BusGroupSummary, kNumBuses + 1> getBusSummaries() const { return computeBusSummaries(); }

    // ═══ ReferenceDrivenEngine — Gap analysis contra referencia ═════════
    /** Ejecuta el ReferenceDrivenEngine y retorna gaps priorizados.
        Retorna lista vacía si no hay referencia de audio cargada. */
    std::vector<DomainGap> getReferenceGaps() const;

    /** Actualiza el plan de progreso contra referencia.
        Debe llamarse periódicamente desde periodicAnalysis(). */
    void updateProgressPlan();

    /** Acceso al PlanManager para el AiCoachAdapter. */
    [[nodiscard]] PlanManager& getPlanManager() noexcept { return planManager_; }
    [[nodiscard]] const PlanManager& getPlanManager() const noexcept { return planManager_; }

    /** Acceso al PhaseManager para UI que necesita mostrar/actualizar métricas de fase. */
    [[nodiscard]] PhaseManager& getPhaseManager() noexcept { return phaseManager_; }
    [[nodiscard]] const PhaseManager& getPhaseManager() const noexcept { return phaseManager_; }

    /** Acceso al SharedData para UI que necesita leer SlotRegistry directamente. */
    [[nodiscard]] SharedData& getSharedData() noexcept { return sharedData_; }
    [[nodiscard]] const SharedData& getSharedData() const noexcept { return sharedData_; }

private:
    PlanManager planManager_;

    PhaseManager&    phaseManager_;
    SharedData&      sharedData_;
    AudioAnalyzer&   audioAnalyzer_;

    // ═══ Referencias almacenadas ═══════════════════════════════════════════
    ReferenceMetadata    referenceMetadata_;
    ReferenceFingerprint referenceFingerprint_;

    // ═══ Reference-Driven Mode ═══════════════════════════════════════════
    bool referenceDrivenMode_ = false;
    ReferenceProgress referenceProgress_;

    // Historial de match para detección de tendencia (últimos N valores)
    static constexpr int kMaxProgressHistory = 20;
    std::array<float, kMaxProgressHistory> progressHistory_{};
    int progressHistoryIndex_ = 0;
    int progressHistoryCount_ = 0;

    // Timestamps para cooldown del análisis reference-driven
    int64_t lastReferenceDrivenAnalysisUs_{0};
    static constexpr int64_t kReferenceDrivenAnalysisIntervalUs = 30 * 1000 * 1000; // 30s
    static constexpr int64_t kReferenceDrivenProgressCooldownUs = 60 * 1000 * 1000;  // 60s entre mensajes de progreso

    // ─── Secciones de referencia ─────────────────────────────────────────

    // Ruta de referencia pendiente de analizar (diferido a periodicAnalysis)
    juce::String pendingReferencePath_;

    /** Fingerprints por seccion (8 segmentos de la referencia). */
    std::vector<SectionFingerprint> referenceSections_;

    /** Indice de la seccion activa para comparacion (-1 = fingerprint global). */
    int activeSectionIndex_ = -1;

    // ═══ ReferenceAnalyzer — comparación completa mix vs referencia ════════
    ReferenceAnalyzer refAnalyzer_;
    ReferenceComparison lastReferenceComparison_;

    /** Infiere automaticamente TrackRole para slots que aun tienen
        TrackRole::Unknown usando SpectralProfiler::inferTrackRole().
        Solo sobreescribe roles no asignados. Se llama desde periodicAnalysis(). */
    void inferTrackRoles();

    // ═══ SILENCE + LOAD ORDER INFERENCE V13 ═══════════════════════════════
    // Detecta el orden en que las pistas emiten su PRIMERA señal de audio
    // y usa esta informacion para inferir roles cuando el nombre es generico
    // ("Pista X") y no hay TrackType explicito.
    void detectFirstSignal();
    void inferBySilenceOrder();


    /** Computa la comparación completa mix vs referencia usando ReferenceAnalyzer.
        Almacena el resultado en lastReferenceComparison_ y genera mensajes
        de coach si hay diferencias significativas. */
    void computeReferenceComparison();

    // ═══ Analizar archivo de audio → fingerprint espectral ═══════════════
    void analyzeReferenceFile(const juce::String& filePath);

    /** Analiza un buffer ya cargado en memoria (evita segunda lectura de disco).
        ReferenceAudioPlayer ya cargo el archivo — reusamos ese buffer. */
    void analyzeReferenceBuffer(const float* bufferL, const float* bufferR,
                                 int64_t numSamples, int numChannels,
                                 double sampleRate,
                                 const juce::String& filePath);

    /** Computa y envía datos de matching espectral al ReferenceMatchPanel.
        Extrae 6 regiones desde el fingerprint activo y el master mix,
        y dispara el callback matchDataCallback_ con los datos. */
    void computeAndSendMatchData();

    /** Analiza secciones de la referencia (divide en 8 segmentos iguales). */
    void computeReferenceSections(const juce::AudioBuffer<float>& fileBuffer,
                                   double sampleRate,
                                   int numChannels);

    // ─── Helpers (públicos para UI callbacks) ─────────────────────────────
public:
    void respondWith(const juce::String& text, MentorMessage::Type type);
    void respondWithPremium(const juce::String& text, MentorMessage::Type type);
private:
    void respondWithContext(const juce::String& text, const juce::String& context, MentorMessage::Type type);

    /** Responde con feedback de corrección (burbuja premium, isSystem=false).
        Específico para el loop de corrección: mensajes como "Veo que bajaste el kick 2dB"
        se renderizan como burbuja premium (no sistema compacto) para que el usuario
        los vea claramente en el chat. */
    void respondWithCorrectionFeedback(const juce::String& text);

    /** Responde con texto del LLM (no es mensaje del sistema, se renderiza como burbuja premium). */
    void respondWithLLM(const juce::String& text);
    TrackTelemetry getLatestTelemetry(int slotIndex) const;

    // ═══ FASE 0 — Diálogo de setup ═════════════════════════════════════
    // V4 Dual Mode: detectar modo y destino
    void detectAndSetEngineerName(const juce::String& message);
    void detectAndSetMode(const juce::String& message);
    void detectAndSetDestination(const juce::String& message);
    void detectAndSetGenre(const juce::String& message);
    juce::String scanAndShowResults() const;
    void advanceFromSetup();
    void sendFallbackWelcome();
    // ═══ Guía activa por fase — Mensajes contextuales al avanzar ═══════
    /** Envía un mensaje al chat con guía práctica para la fase actual.
        Explica qué hacer, por qué es importante, y da ejemplos concretos.
        Se llama desde advanceFromSetup() y desde el comando /next. */
    void sendPhaseGuidance(MentorPhase phase);
    SetupStep setupStep_{SetupStep::NotStarted};
    juce::String setupGenre_;
    juce::String engineerName_;  // Nombre del ingeniero (loaded from session or set during setup)
    bool setupGreetingSent_{false};
    DiagnosticUpdateCallback diagnosticUpdateCb_;   // Opcional, para baja latencia en overlays
    TrackChangeCallback trackChangeCallback_;  // Opcional, usado por AiCoachAdapter
    SessionQueryCallback sessionQueryCallback_;  // Opcional, para /session command
    MessagePushedCallback messagePushedCallback_;
    MatchDataCallback matchDataCallback_;
    LlmResponseCallback llmResponseCallback_;
    LlmStreamingCallback llmStreamingCallback_;
    SetupLlmCallback setupLlmCallback_;
    RoleSuggestionCallback roleSuggestionCallback_;
    bool roleSuggestionRequested_{false};
    EngineerNameCallback engineerNameCallback_;

    // ─── Streaming UI callbacks (set by editor) ─────────────────────────-
    StreamStartedCallback streamStartedCb_;
    StreamTokenCallback   streamTokenCb_;
    StreamEndedCallback   streamEndedCb_;
    bool llmEnabled_{true};  // Habilitado por defecto — el router al LLM en handleUserMessage() usa esta flag
    bool proactiveAnalysisEnabled_{true}; // Análisis automático (tips proactivos + consolidados)

    // ─── Helper de espectro: energía promedio en un rango de bins ─────────
    [[nodiscard]] float spectrumBandEnergy(const float* spectrum, int startBin, int endBin) const noexcept;

    // ─── Resetear estados de pista (para /reset) ──────────────────────────
    void resetTrackStates();

    // ═══ ANÁLISIS POR FASE ════════════════════════════════════════════════

    void analyzeGainStagingReal();
    void analyzeOrganisationReal();
    void analyzeTonalBalanceReal();
    void analyzeDynamicsReal();
    void analyzePhaseReal();
    void analyzeOverallMixReal();
    void analyzeSpectralMaskingReal();
    void analyzePairwiseMasking();
    void analyzePrePostFxComparison();
    void analyzeUnmonitoredTracks();
    void analyzeBusBalance();

    // ═══ MASTER MODE ANALYSIS — Pipeline para Master Mode ═══════════════
    /** Analiza loudness del master vs destino seleccionado (LUFS, True Peak, LRA). */
    void analyzeMasterLoudness();
    /** Analiza balance espectral del master (6 regiones, sin hablar de pistas). */
    void analyzeMasterSpectral();
    /** Analiza correlación estéreo y ancho espectral del master. */
    void analyzeMasterStereo();

    // ─── Agregación por bus ───────────────────────────────────────────────
    [[nodiscard]] std::array<BusGroupSummary, kNumBuses + 1> computeBusSummaries() const;

public:
    // ═══ Recomendaciones (público para UI) ═════════════════════════════
    [[nodiscard]] const TrackRecommendation* getTrackRecommendation(int slotIndex) const;

    // ─── Loop de Corrección ──────────────────────────────────────────────
    void storeRecommendation(int slotIndex, const juce::String& trackName,
                             TrackRecommendation::Domain domain,
                             const juce::String& action, float beforeValue,
                             float expectedAfter, float delta,
                             const juce::String& verifyMetric = "peak",
                             float frequencyHz = 0.0f,
                             int spectralBand = -1);
    void storeRecommendationFromSemanticAnalysis();
    void verifyTrackCorrections();

    /** Detecta cambios en pistas SIN recomendación activa y envía
        observaciones conversacionales. Por ejemplo, si el usuario baja
        un fader sin que se lo hayamos sugerido, el coach lo nota.
        Se llama desde periodicAnalysis() o fastTrackAnalysis().
        @param focusSlotIndex  Si >= 0, solo verifica esa pista (feedback dirigido).
                               Si -1, escanea todas las pistas activas. */
    void detectUnpromptedChanges(int focusSlotIndex = -1);

    // ─── Follow-up: genera nueva recomendación correctiva post-verificación ──
    /** Después de verificar una corrección, si quedó fuera del target ideal,
        genera una recomendación de seguimiento para ajustar fino.
        Se llama desde verifyTrackCorrections() automáticamente. */
    void generateFollowUp(int slotIndex, const TrackRecommendation& oldRec);

    // ─── CorrectionHistoryEntry — registro de corrección completada ─────
    struct CorrectionHistoryEntry {
        int64_t     timestampUs   = 0;
        int         slotIndex     = -1;
        juce::String trackName;
        juce::String action;
        TrackRecommendation::Domain domain = TrackRecommendation::Domain::Gain;
        TrackRecommendation::Status finalStatus = TrackRecommendation::Status::Superseded;
        float       beforeValue   = 0.0f;
        float       afterValue    = 0.0f;
        float       appliedRatio  = 0.0f;
        bool        hadFollowUp   = false;
    };

    /** Retorna el historial de correcciones completadas. */
    [[nodiscard]] const std::vector<CorrectionHistoryEntry>& getCorrectionHistory() const noexcept;

    /** Cuenta correcciones exitosas vs ignoradas para un slot. */
    struct SlotCorrectionStats {
        int total      = 0;
        int applied    = 0;
        int ignored    = 0;
        int overApplied = 0;
        int underApplied = 0;
    };
    [[nodiscard]] SlotCorrectionStats getSlotCorrectionStats(int slotIndex) const noexcept;

    /** Retorna la recomendación más urgente entre todas las activas (Pending).
        Orden: Clipping > Phase > Crest > Tonal > Gain. */
    [[nodiscard]] const TrackRecommendation* getMostUrgentRecommendation() const;

    /** Marca una recomendación como reconocida por el usuario (Dismissed). */
    void dismissRecommendation(int slotIndex);

    /** Maneja comandos de usuario para el loop de corrección. */
    void handleCorrectionCommand(const juce::String& command);

    // ─── Adaptive thresholds — se ajustan según historial del usuario ─────
    struct AdaptiveThresholds {
        float underApplyRatio  = 0.15f;
        float goodStartRatio   = 0.35f;
        float underApplyTarget = 0.80f;
        float overApplyTarget  = 1.20f;
        int   maxRetriesBeforeIgnore = 3;
    };

    [[nodiscard]] const AdaptiveThresholds& getAdaptiveThresholds() const noexcept {
        return adaptiveThresholds_;
    }
    void recalcAdaptiveThresholds();

    // ─── Cooldowns ───────────────────────────────────────────────────────
    static constexpr int64_t kWarningCooldownUs     = 60 * 1000 * 1000;    // 60s entre warnings del mismo tipo
    static constexpr int64_t kTrackCooldownUs       = 120 * 1000 * 1000;   // 2min entre warnings de la misma pista
    static constexpr int64_t kFastAnalysisIntervalUs = 2 * 1000 * 1000;    // 2s entre fast analysis (peak/RMS/corr)
    static constexpr int64_t kAnalysisIntervalUs    = 8 * 1000 * 1000;    // 8s entre análisis periódicos
    static constexpr int64_t kCorrectionVerifyUs    = 12 * 1000 * 1000;   // 12s para verificar correcciones
    static constexpr int64_t kProactiveTipIntervalUs = 90 * 1000 * 1000;  // 90s entre tips proactivos
    static constexpr int64_t kIdleThresholdUs       = 150 * 1000 * 1000;  // 2.5min sin interacción → considerado "inactivo"
    static constexpr int64_t kCelebrationCooldownUs = 120 * 1000 * 1000;  // 2min entre celebraciones

    int64_t lastPeriodicAnalysisUs_{0};
    int64_t lastFastAnalysisUs_{0};
    int64_t lastCorrectionVerifyUs_{0};
    int64_t lastSemanticAnalysisUs_{0};
    int64_t lastUnpromptedChangeCheckUs_{0};  // Timer separado para detectUnpromptedChanges()
    int64_t lastPeakWarningUs_{0};
    int64_t lastCrestWarningUs_{0};
    int64_t lastPhaseWarningUs_{0};
    int64_t lastHeadroomWarningUs_{0};
    int64_t lastTonalWarningUs_{0};
    int64_t lastDynamicWarningUs_{0};
    int64_t lastLoudnessWarningUs_{0};
    int64_t lastMaskingWarningUs_{0};
    int64_t lastPairwiseMaskingWarningUs_{0};
    int64_t lastPrePostWarningUs_{0};
    int64_t lastUnmonitoredWarningUs_{0};
    int64_t lastBusBalanceWarningUs_{0};
    int64_t lastTransientWarningUs_{0};   // Cooldown para detección de transientes
    int64_t lastCrestBandWarningUs_{0};   // Cooldown para crest por banda
    int64_t lastStereoWidthWarningUs_{0}; // Cooldown para ancho estéreo
    int64_t lastGainAdviceUs_{0};         // Sprint 6A: cooldown para gain advice
    static constexpr int64_t kGainAdviceCooldownUs = 120 * 1000 * 1000; // 2min entre mensajes de gain
    int64_t lastDynamicsAdviceUs_{0};     // Sprint 6B: cooldown para dynamics advice
    static constexpr int64_t kDynamicsAdviceCooldownUs = 120 * 1000 * 1000; // 2min entre mensajes de dynamics
    int64_t lastTonalAdviceUs_{0};        // Sprint 6C: cooldown para tonal advice

    int64_t lastPhaseAdviceUs_{0};         // Sprint 8: cooldown para phase advice
    static constexpr int64_t kPhaseAdviceCooldownUs = 120 * 1000 * 1000;
    static constexpr int64_t kTonalAdviceCooldownUs = 120 * 1000 * 1000; // 2min entre mensajes de tonal
    int64_t lastReferenceGapMessageUs_{0}; // Cooldown para mensajes de gap contra referencia
    int64_t lastReferenceGapImprovedUs_{0}; // Cooldown para eventos de mejora vs referencia
    int64_t lastReferenceGapWorsenedUs_{0}; // Cooldown para eventos de empeoramiento vs referencia

    // Gaps ANTERIORES para detectar cambios (improvement vs worsening)
    std::vector<DomainGap> previousReferenceGaps_;

    // Match anterior (para delta simple en ReferenceProgress)
    float previousReferenceMatch_ = 0.0f;

    // Sprint 4: flags para bienvenida y mensajes periódicos
    bool referenceDrivenWelcomeSent_ = false;
    int64_t lastReferenceDrivenStatusUs_ = 0;
    static constexpr int64_t kReferenceDrivenStatusIntervalUs = 90 * 1000 * 1000; // 90s entre mensajes estables

    static constexpr int64_t kReferenceReanalysisIntervalUs = 30 * 1000 * 1000; // 30s entre re-análisis de referencia
    static constexpr int64_t kReferenceImprovementCooldownUs = 120 * 1000 * 1000; // 2min entre eventos de mejora
    static constexpr int64_t kReferenceWorsenedCooldownUs   = 60 * 1000 * 1000;  // 1min entre eventos de empeoramiento
    static constexpr float   kReferenceMinImprovementDb     = 1.0f; // Mínimo cambio en gap para generar evento (dB/LU)

    // ═══ Resultados del último análisis Gain Staging (para PhaseManager auto-advance) ═══
    struct LastGainStagingResult {
        int   clippingCount    = 0;
        int   lowSignalCount   = 0;
        float maxGlobalPeak    = -100.0f;
    } lastGainStagingResult_;

    std::array<TrackAnalysisState, SlotRegistry::kMaxSlots> trackStates_;
    std::array<TrackRecommendation, SlotRegistry::kMaxSlots> recommendations_{};
    AdaptiveThresholds adaptiveThresholds_;
    std::vector<CorrectionHistoryEntry> correctionHistory_;
    static constexpr int kMaxCorrectionHistory = 200;

    // ═══ Signal Order UI — acceso público para la UI de MessengerList ═══
    /** Retorna true si el rol de este slot fue inferido via signal-order y aún no ha sido confirmado. */
    [[nodiscard]] bool isSignalOrderInference(int slotIndex) const noexcept {
        return slotIndex >= 0 && slotIndex < SlotRegistry::kMaxSlots
            && signalOrderApplied_[slotIndex] && !signalOrderConfirmed_[slotIndex];
    }
    /** Marca una inferencia por signal-order como confirmada por el usuario. */
    void confirmSignalOrderRole(int slotIndex) noexcept {
        if (slotIndex >= 0 && slotIndex < SlotRegistry::kMaxSlots)
            signalOrderConfirmed_[slotIndex] = true;
    }
    /** Retorna el conteo de señales detectadas en el orden actual. */
    int getSignalOrderCount() const noexcept { return signalOrderCount_; }
    /** Retorna el array de índices de slots en orden de primera señal. */
    const int* getSignalOrderIndices() const noexcept { return signalOrderIndices_; }

    // ═══ Sprint 1: Generalized role confirmation ───────────────────────
    // Cubre los 3 orígenes de inferencia: signal-order, nombre y espectral.
    // Antes solo existía confirmación para signal-order. Ahora cualquier rol
    // inferido (trackRoleWasInferred_==true) puede ser confirmado por el usuario.
    /** Retorna true si el rol de este slot fue inferido y aún no confirmado. */
    [[nodiscard]] bool isInferredRole(int slotIndex) const noexcept {
        return slotIndex >= 0 && slotIndex < SlotRegistry::kMaxSlots
            && trackRoleWasInferred_[slotIndex]
            && !trackRoleConfirmed_[slotIndex];
    }
    /** Retorna true si el rol de este slot está confirmado (por signal-order,
     *  por setTrackRoleWithLearning, o por confirmRole()). */
    [[nodiscard]] bool isRoleConfirmed(int slotIndex) const noexcept {
        if (slotIndex < 0 || slotIndex >= SlotRegistry::kMaxSlots) return false;
        const TrackRole role = trackRoles_[slotIndex];
        if (role == TrackRole::Unknown || role == TrackRole::Master) return false;
        // Confirmado si: fue seteado manualmente (!inferred), o marcado explícitamente.
        return !trackRoleWasInferred_[slotIndex] || trackRoleConfirmed_[slotIndex];
    }
    /** Confirma el rol inferido de una pista (cualquier origen de inferencia). */
    void confirmRole(int slotIndex) noexcept {
        if (slotIndex >= 0 && slotIndex < SlotRegistry::kMaxSlots) {
            trackRoleConfirmed_[slotIndex] = true;
            signalOrderConfirmed_[slotIndex] = true; // compatibilidad con el flujo existente
        }
    }
    /** Confirma TODOS los roles inferidos pendientes. Avanza de fase si confirmó >0.
     *  Retorna el número de roles confirmados. */
    int confirmAllInferredRoles() noexcept;
    /** Desmarca la confirmación de una pista (override manual futuro). */
    void clearRoleConfirmation(int slotIndex) noexcept {
        if (slotIndex >= 0 && slotIndex < SlotRegistry::kMaxSlots)
            trackRoleConfirmed_[slotIndex] = false;
    }
    /** Retorna el progreso de identidad agregado (para el badge 🎯 X/Y). */
    [[nodiscard]] IdentityProgress getIdentityProgress() const noexcept;

    // ═══ Track roles asignados por usuario (o inferidos) ═════════════
    std::array<TrackRole, SlotRegistry::kMaxSlots> trackRoles_{};

    // ═══ CorrectionLearner — Aprende de correcciones del usuario ═══
    CorrectionLearner correctionLearner_;
    /** Retorna el CorrectionLearner. */
    [[nodiscard]] CorrectionLearner& getCorrectionLearner() noexcept { return correctionLearner_; }
    [[nodiscard]] const CorrectionLearner& getCorrectionLearner() const noexcept { return correctionLearner_; }

    // ═══ Tracking de inferencia (para detectar correcciones) ═══
    std::array<bool, SlotRegistry::kMaxSlots> trackRoleWasInferred_{};
    // Sprint 1: confirmación generalizada de roles inferidos (cualquier origen).
    // true cuando el usuario confirma el rol (clic ⚡ individual o "Confirmar todo").
    std::array<bool, SlotRegistry::kMaxSlots> trackRoleConfirmed_{};
    std::array<std::vector<juce::String>, SlotRegistry::kMaxSlots> lastInferenceKeywords_{};

    /** Asigna un rol a una pista. */
    void setTrackRole(int slotIndex, TrackRole role) noexcept {
        if (slotIndex >= 0 && slotIndex < SlotRegistry::kMaxSlots)
            trackRoles_[slotIndex] = role;
    }
    /** Asigna un rol y aprende de la corrección si el usuario cambió un rol inferido. */
    void setTrackRoleWithLearning(int slotIndex, TrackRole role) noexcept;
    /** Obtiene el rol de una pista. */
    [[nodiscard]] TrackRole getTrackRole(int slotIndex) const noexcept {
        if (slotIndex >= 0 && slotIndex < SlotRegistry::kMaxSlots)
            return trackRoles_[slotIndex];
        return TrackRole::Unknown;
    }
    /** Obtiene el array completo de roles (para SemanticComparator). */
    [[nodiscard]] const std::array<TrackRole, SlotRegistry::kMaxSlots>& getTrackRoles() const noexcept { return trackRoles_; }

    /** Ejecuta análisis semántico completo y retorna diffs. */
    std::vector<SemanticDiff> runSemanticAnalysis() const;

    /** Genera sugerencias dinámicas para los chips del chat, basadas en el
        análisis actual de la mezcla (score, issues, fase, referencias). */
    std::vector<juce::String> getDynamicSuggestions() const;

    // ═══ Modo de operación (V4 Dual Mode) ═══════════════════════════════
    CoachMode coachMode_{CoachMode::Mix};
    MasterDestination masterDestination_{MasterDestination::StreamingGeneral};


    /** Setea el modo de operación (Mix o Master).
        Esto cambia el comportamiento del setup, los análisis y el prompt del LLM. */
    void setCoachMode(CoachMode mode) noexcept { coachMode_ = mode; }
    [[nodiscard]] CoachMode getCoachMode() const noexcept { return coachMode_; }
    [[nodiscard]] bool isMixMode() const noexcept { return coachMode_ == CoachMode::Mix; }
    [[nodiscard]] bool isMasterMode() const noexcept { return coachMode_ == CoachMode::Master; }

    /** Setea el destino de masterización (solo relevante en Master Mode). */
    void setMasterDestination(MasterDestination dest) noexcept { masterDestination_ = dest; }
    [[nodiscard]] MasterDestination getMasterDestination() const noexcept { return masterDestination_; }
    [[nodiscard]] float getDestinationLUFS() const noexcept {
        return ::mixcoach::getDestinationLUFS(masterDestination_);
    }
    [[nodiscard]] float getDestinationTruePeak() const noexcept {
        return ::mixcoach::getDestinationTruePeak(masterDestination_);
    }

    // ═══ REFERENCE-DRIVEN MODE ═══════════════════════════════════════
    // When enabled, the reference becomes the NORTH STAR for ALL coaching.
    // Every recommendation is measured against the reference.
    // Progress is tracked as % match (0.0-1.0) and trend (improving/worsening/stable).

    /** Activa o desactiva el Reference-Driven Mode.
        @param enabled  true = la referencia es el norte absoluto */
    void setReferenceDrivenMode(bool enabled) noexcept {
        referenceDrivenMode_ = enabled;
        if (!enabled)
            referenceDrivenWelcomeSent_ = false; // reset para posible re-activación
    }
    [[nodiscard]] bool isReferenceDrivenMode() const noexcept { return referenceDrivenMode_; }

    /** Computa el progreso actual contra la referencia y actualiza el historial.
        Debe llamarse periódicamente desde periodicAnalysis().
        Retorna el ReferenceProgress actual. */
    ReferenceProgress computeReferenceMatchProgress();

    /** Retorna el último ReferenceProgress computado. */
    [[nodiscard]] const ReferenceProgress& getReferenceProgress() const noexcept {
        return referenceProgress_;
    }

    /** Retorna el historial de match (últimos N valores). */
    [[nodiscard]] const std::array<float, kMaxProgressHistory>& getReferenceProgressHistory() const noexcept {
        return progressHistory_;
    }
    /** Retorna cuántos valores válidos hay en el historial de progreso. */
    [[nodiscard]] int getReferenceProgressHistoryCount() const noexcept {
        return juce::jmin(progressHistoryCount_, kMaxProgressHistory);
    }

    /** Resetea todo el tracking de progreso contra referencia.
        Limpia historial, gaps, delta y cooldowns. */
    void resetReferenceProgress() noexcept;

    /** Envía un análisis de progreso contra referencia al LLM (o al chat si no hay LLM).
        Se dispara automáticamente cuando el match mejora o empeora significativamente.
        Incluye: match actual, tendencia, gaps que mejoraron/empeoraron, 
        y sugerencia de siguiente paso. */
    void sendReferenceDrivenAnalysis();

    /** Resetea cooldowns compartidos entre Mix y Master Mode.
        Se llama al cambiar de modo para evitar interferencia cross-mode. */
    void resetMasterCooldowns() noexcept;

    /** En Mix Mode: la referencia se normaliza a -6 dBFS para no perseguir loudness.
        En Master Mode: la referencia se usa a volumen real. */
    [[nodiscard]] bool shouldNormalizeReference() const noexcept {
        return coachMode_ == CoachMode::Mix;
    }

    // ═══ Proactividad — timers de interacción y tips ═══════════════════
    int64_t lastUserInteractionTimeUs_{0};
    int64_t lastProactiveTipTimeUs_{0};
    int64_t lastProactiveLlmTipTimeUs_{0};
    int64_t lastCelebrationTimeUs_{0};
    int     previousClippingCount_{0};   // ← para detectar "antes había clipping, ahora no = mejora"

    // ═══ TrackFeedCore — Estado unificado de pistas + event bus + priority ═══
    /** Envía un tip proactivo al LLM basado en los eventos más severos del TrackFeedCore.
        Se llama desde periodicAnalysis(). Usa cooldown de 90s para no saturar. */
    void sendProactiveLlmTip();

    // ═══════════════════════════════════════════════════════════════════════════
    //  CEREBRO QUE PIENSA COMO INGENIERO
    //  Capa de Decisión Unificada: recoge TODOS los issues, los prioriza,
    //  identifica pistas óptimas, y envía UN mensaje consolidado al LLM.
    // ═══════════════════════════════════════════════════════════════════════════

    /** Un issue detectado en una pista, con severidad y opciones A/B. */
    struct TrackIssue {
        int     slotIndex     = -1;
        juce::String trackName;
        juce::String trackRole;       // Rol inferido o asignado
        juce::String domain;          // "gain", "tonal", "dynamics", "spatial", "masking"
        juce::String issueType;       // "CLIPPING", "SOBRECOMPRIMIDO", "FASE_INVERTIDA", etc.
        float   severity       = 0.0f; // 0.0-1.0
        bool    isCritical     = false;
        bool    isOptimal      = false;  // true = esta pista está bien
        float   currentValue   = 0.0f;
        float   targetValue    = 0.0f;
        juce::String description;       // "Kick está recortando a +0.3 dB"
        juce::String optionA;           // "Opción A: baja el fader 3 dB"
        juce::String optionB;           // "Opción B: baja el output del compresor 2 dB"
        juce::String actionVerb;        // "subir", "reducir", "comprimir", "expandir"
        float   suggestedDelta  = 0.0f; // Cuánto ajustar (dB o unidades)
        float   frequencyHz     = 0.0f; // Frecuencia sugerida (para EQ)
    };

    /** Recolecta TODOS los issues de TODAS las pistas en un solo pase.
        También identifica pistas que están en rango óptimo.
        Retorna issues ordenados por severidad descendente. */
    std::vector<TrackIssue> collectAllIssues();

    /** Detecta pares de pistas con enmascaramiento espectral entre tracks.
        Compara todas las combinaciones de pares de pistas activas y crea
        TrackIssue con domain="masking" para los pares conflictivos.
        Se llama desde collectAllIssues(). */
    void detectSpectralMaskingPairs(std::vector<TrackIssue>& issues);

    /** Identifica pistas que NO necesitan procesamiento (están en rango óptimo).
        Retorna los slotIndex de las pistas que están saludables. */
    std::vector<int> identifyOptimalTracks();

    /** Genera 2 opciones de solución para un issue.
        @param issue  El issue detectado (se modifican optionA/optionB/suggestedDelta)
        @param genre  Género musical para contexto de target */
    static void generateOptionsForIssue(TrackIssue& issue, const juce::String& genre = {});

    /** Envía un análisis CONSOLIDADO al LLM con los issues priorizados
        y las pistas óptimas. Reemplaza los mensajes individuales con
        una sola comunicación integrada.
        Cooldown: 120s (kConsolidatedAnalysisIntervalUs). */
    void sendConsolidatedAnalysis();

    static constexpr int64_t kConsolidatedAnalysisIntervalUs = 120 * 1000 * 1000; // 120s
    int64_t lastConsolidatedAnalysisUs_{0};

    /** Construye el SessionMap completo — estructura jerárquica de la sesión
        con roles, nombres, buses y niveles, agrupado por categoría.
        Se construye desde datos vivos (SlotRegistry, trackRoles_, telemetría).
        Nota: el método es const pero modifica sessionMap_ (miembro mutable). */
    [[nodiscard]] SessionMap buildSessionMap() const;

    /** Retorna el SessionMap cachead (último construido). */
    [[nodiscard]] const SessionMap& getCachedSessionMap() const noexcept { return sessionMap_; }

    /** Establece el SessionMap desde datos cargados (ej: session_memory.json). */
    void setCachedSessionMap(const SessionMap& map) noexcept { sessionMap_ = map; }

    /** Guarda el SessionMap actual en un DynamicObject (para session_memory.json). */
    void saveSessionMapToJson(juce::DynamicObject& obj) const;

    /** Carga el SessionMap desde un DynamicObject (desde session_memory.json). */
    void loadSessionMapFromJson(const juce::DynamicObject& obj);

    /** Construye el DifferenceProfile completo — unifica Current vs Reference vs Delta
        en un solo struct persistente. Combina datos de ReferenceFingerprint,
        ReferenceComparison, AudioAnalyzer y DomainGap en un solo objeto. */
    [[nodiscard]] DifferenceProfile buildDifferenceProfile() const;

    /** Retorna el DifferenceProfile cachead (último construido). */
    [[nodiscard]] const DifferenceProfile& getCachedDifferenceProfile() const noexcept {
        return differenceProfile_;
    }

    /** Establece el DifferenceProfile desde datos cargados (ej: session_memory.json). */
    void setCachedDifferenceProfile(const DifferenceProfile& dp) noexcept {
        differenceProfile_ = dp;
    }

    /** Guarda el DifferenceProfile actual en un DynamicObject (para session_memory.json). */
    void saveDifferenceProfileToJson(juce::DynamicObject& obj) const;

    /** Carga el DifferenceProfile desde un DynamicObject (desde session_memory.json). */
    void loadDifferenceProfileFromJson(const juce::DynamicObject& obj);

    /** Construye un árbol textual del mapa de sesión con roles inferidos,
        agrupado por categoría (Drums, Bass, Vocals, etc.). */
    [[nodiscard]] juce::String buildSessionMapText() const;

    /** Construye el SessionContext completo — fotografía de toda la sesión.
        Agrega datos de PhaseManager, AudioAnalyzer, SlotRegistry, trackRoles_,
        ReferenceAnalyzer, TrackFeedCore y WorkflowDetector en un solo struct.
        @param issues  Opcional: lista de issues de collectAllIssues() para
                       poblar criticalIssues/warningIssues/infoIssues. */
    [[nodiscard]] SessionContext buildSessionContext(const std::vector<TrackIssue>* issues = nullptr) const;

    /** Versión textual del SessionContext para mostrar en el chat (/status).
        Más rico que buildSessionMapText() porque incluye métricas del master,
        conteo por categoría, estado de referencia, y resumen de issues.
        @param issues  Opcional: lista de issues para poblar conteo de issues. */
    [[nodiscard]] juce::String buildSessionContextText(const std::vector<TrackIssue>* issues = nullptr) const;

    /** Retorna el TrackFeedCore para acceso externo (UI, AiCoachAdapter, etc.). */
    [[nodiscard]] TrackFeedCore& getTrackFeedCore() noexcept { return *trackFeedCore_; }
    [[nodiscard]] const TrackFeedCore& getTrackFeedCore() const noexcept { return *trackFeedCore_; }

    // ═══ WorkflowDetector — detección de acciones del usuario ════════════
    [[nodiscard]] WorkflowDetector& getWorkflowDetector() noexcept { return workflowDetector_; }
    [[nodiscard]] const WorkflowDetector& getWorkflowDetector() const noexcept { return workflowDetector_; }

    // Helpers de análisis (legacy)
    void analyzeGainStaging();
    void analyzeOrganisation();
    void analyzeTonalBalance();
    void analyzeDynamics();
    void analyzeSpatial();

    // ═══ WorkflowDetector — detección de acciones del usuario en tiempo real ═══
    WorkflowDetector workflowDetector_;

private:
    // ═══ DifferenceProfile cache — mutable porque buildDifferenceProfile() y
    // saveDifferenceProfileToJson() son const pero actualizan el cache
    mutable DifferenceProfile differenceProfile_;

    // ═══ SessionMap cache — mutable porque buildSessionMapText() y
    // saveSessionMapToJson() son const pero actualizan el cache
    mutable SessionMap sessionMap_;

    // ═══ TrackFeedCore — Instancia central del event bus + priority engine ═══
    std::unique_ptr<TrackFeedCore> trackFeedCore_;

    // ═══ Signal Order detection — rastrea el orden de primera señal ═══════
    int64_t firstSignalTimestampsUs_[SlotRegistry::kMaxSlots]{};
    int     signalOrderIndices_[SlotRegistry::kMaxSlots]{};
    int     signalOrderCount_{0};
    bool    signalOrderApplied_[SlotRegistry::kMaxSlots]{}; // Tracks con rol inferido via signal order
    bool    signalOrderConfirmed_[SlotRegistry::kMaxSlots]{}; // Confirmado por el usuario
    static constexpr float  kSignalThresholdDb  = -55.0f;
    static constexpr int    kMaxSignalOrder     = 10; // Primeras 10 posiciones (incluye vocal)


    /** Sincroniza TODAS las pistas activas desde SharedData → TrackFeedCore.
        Se llama al inicio de periodicAnalysis() para tener datos frescos
        para los análisis existentes. */
    void syncTrackFeedCore();

public:
// ═══════════════════════════════════════════════════════════════════════════
//  TrackGainAdvice — Análisis de ganancia por pista (Sprint 6A)
//  Compara el nivel actual contra el target del rol (ExpectedProfile)
//  y genera un mensaje accionable para el usuario.
//  Sin IA — reglas en C++, 0 tokens, instantáneo.
// ═══════════════════════════════════════════════════════════════════════════
struct TrackGainAdvice {
    int         slotIndex     = -1;
    juce::String trackName;
    TrackRole   role          = TrackRole::Unknown;

    // Valores actuales
    float       currentPeak   = -100.0f;
    float       currentRMS    = -100.0f;
    float       currentLUFS   = -100.0f;
    float       currentCrest  = 0.0f;

    // Targets del rol
    float       peakTarget    = -8.0f;
    float       crestTarget   = 10.0f;
    float       peakTolerance = 4.0f;

    // Desviación vs target (dB, positivo = más fuerte que target)
    float       peakDeviation = 0.0f;

    // Cambio sugerido (dB, positivo = subir fader)
    float       suggestedDeltaDb = 0.0f;

    enum class Status : uint8_t {
        OnTarget,     // Dentro del rango ± tolerance
        NearTarget,   // Cerca del target (± 2*tolerance)
        OffTarget,    // Fuera del rango aceptable
        NoSignal,     // Sin señal detectable
        UnknownRole   // Rol no especificado
    };
    Status status = Status::UnknownRole;

    // Mensaje accionable para el usuario
    juce::String message;

    [[nodiscard]] bool isActionable() const noexcept {
        return status == Status::OffTarget || status == Status::NearTarget;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
//  TrackDynamicsAdvice — Análisis de dinámica por pista (Sprint 6B)
//  Compara el crest factor actual contra el target del rol (ExpectedProfile)
//  y sugiere ajustes de compresión/expansión.
//  Sin IA — reglas en C++, 0 tokens, instantáneo.
// ═══════════════════════════════════════════════════════════════════════════
struct TrackDynamicsAdvice {
    int         slotIndex     = -1;
    juce::String trackName;
    TrackRole   role          = TrackRole::Unknown;

    // Valores actuales
    float       currentCrest  = 0.0f;      // Crest factor actual (dB)
    float       currentPeak   = -100.0f;
    float       currentRMS    = -100.0f;

    // Targets del rol
    float       crestTarget   = 10.0f;     // Crest target del rol (dB)
    float       crestTolerance = 6.0f;     // ± dB de tolerancia

    // Desviación (dB, positivo = más dinámico que target)
    float       crestDeviation = 0.0f;

    // Acción sugerida (texto legible, ej: "Baja el ratio del compresor")
    juce::String suggestedAction;

    enum class Status : uint8_t {
        OnTarget,     // crest in range ± tolerance
        NearTarget,   // crest cerca del límite
        OffTarget,    // crest fuera de rango (sobre-comprimido o muy dinámico)
        NoSignal,     // Sin señal detectable
        UnknownRole   // Rol no especificado
    };
    Status status = Status::UnknownRole;

    // Subtipo: indica si es sobre-compresión o falta de compresión
    enum class SubType : uint8_t {
        None,
        Overcompressed,   // crest < (target - tolerance): poca dinámica
        TooDynamic        // crest > (target + tolerance): mucha dinámica
    };
    SubType subType = SubType::None;

    // Mensaje accionable para el usuario
    juce::String message;

    [[nodiscard]] bool isActionable() const noexcept {
        return status == Status::OffTarget || status == Status::NearTarget;
    }

    [[nodiscard]] bool isOvercompressed() const noexcept {
        return subType == SubType::Overcompressed;
    }

    [[nodiscard]] bool isTooDynamic() const noexcept {
        return subType == SubType::TooDynamic;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
//  TrackTonalAdvice — Análisis de balance espectral por pista (Sprint 6C)
//  Compara la energía en 6 regiones (Sub, Bass, LoMid, HiMid, Pres, Air)
//  contra el spectralOffset esperado del rol (ExpectedProfile).
//  Detecta exceso o falta de energía por región y sugiere EQ.
//  Sin IA — reglas en C++, 0 tokens, instantáneo.
// ═══════════════════════════════════════════════════════════════════════════
struct TrackTonalAdvice {
    int         slotIndex     = -1;
    juce::String trackName;
    TrackRole   role          = TrackRole::Unknown;

    // Energía actual por región (6 regiones, dB)
    float       regionEnergy[6]     = { -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f };
    // Energía esperada por región (desde peakDb + spectralOffset, dB)
    float       regionExpected[6]   = { -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f };
    // Desviación por región (positivo = más energía de la esperada, dB)
    float       regionDeviation[6]  = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    // Peak actual usado como referencia
    float       currentPeak         = -100.0f;

    // Región con mayor desviación
    int         worstRegion         = -1;
    float       worstDeviation      = 0.0f;
    bool        isExcess            = false;  // true = exceso, false = déficit

    enum class Status : uint8_t {
        OnTarget,     // Todas las regiones dentro de tolerancia
        NearTarget,   // Alguna región cerca del límite
        OffTarget,    // Una o más regiones significativamente fuera
        NoSignal,     // Sin señal
        UnknownRole   // Rol no especificado
    };
    Status status = Status::UnknownRole;

    // Tolerancia por región (dB, desde ±6 hasta ±10 según la región)
    static constexpr float kToleranceDb = 6.0f;
    // Tolerancia x2 para NearTarget
    static constexpr float kNearToleranceDb = 12.0f;

    // Índices de región
    static constexpr int kRegionSub    = 0;
    static constexpr int kRegionBass   = 1;
    static constexpr int kRegionLoMid  = 2;
    static constexpr int kRegionHiMid  = 3;
    static constexpr int kRegionPres   = 4;
    static constexpr int kRegionAir    = 5;

    // Nombres de región
    static constexpr const char* kRegionName(int r) noexcept {
        switch (r) {
            case 0: return "Sub";
            case 1: return "Bass";
            case 2: return "LoMid";
            case 3: return "HiMid";
            case 4: return "Pres";
            case 5: return "Air";
            default: return "?";
        }
    }

    // Rangos de frecuencia por región
    static constexpr const char* kRegionFreq(int r) noexcept {
        switch (r) {
            case 0: return "20-86 Hz";
            case 1: return "86-301 Hz";
            case 2: return "301-1076 Hz";
            case 3: return "1076-3532 Hz";
            case 4: return "3532-8355 Hz";
            case 5: return "8355-16458 Hz";
            default: return "";
        }
    }

    // Sugerencia de acción para exceso
    static constexpr const char* kExcessSuggestion(int r) noexcept {
        switch (r) {
            case 0: return "reduce 50-100 Hz";
            case 1: return "reduce 100-300 Hz";
            case 2: return "reduce 300-1000 Hz";
            case 3: return "reduce 1-3 kHz";
            case 4: return "reduce 3-8 kHz";
            case 5: return "reduce 8-16 kHz";
            default: return "";
        }
    }

    // Sugerencia de acción para déficit
    static constexpr const char* kDeficitSuggestion(int r) noexcept {
        switch (r) {
            case 0: return "refuerza 50-100 Hz";
            case 1: return "refuerza 100-300 Hz";
            case 2: return "refuerza 300-1000 Hz";
            case 3: return "refuerza 1-3 kHz";
            case 4: return "refuerza 3-8 kHz";
            case 5: return "refuerza 8-16 kHz";
            default: return "";
        }
    }

    // Mensaje accionable
    juce::String message;

    [[nodiscard]] bool isActionable() const noexcept {
        return status == Status::OffTarget || status == Status::NearTarget;
    }

    /** Retorna si hay exceso de energía en la región peor. */
    [[nodiscard]] bool hasExcess() const noexcept { return isExcess; }

    /** Retorna si hay déficit de energía. */
    [[nodiscard]] bool hasDeficit() const noexcept { return worstRegion >= 0 && !isExcess; }
};


// ═════════════════════
//  TrackPhaseAdvice — Análisis de fase estéreo por pista (Sprint 8)
//  Compara la correlación actual de la pista contra umbrales por rol
//  y detecta problemas de fase / colapso estéreo.
//  Sin IA — reglas en C++, 0 tokens, instantáneo.
// ═════════════════════
struct TrackPhaseAdvice {
    int         slotIndex     = -1;
    juce::String trackName;
    TrackRole   role          = TrackRole::Unknown;

    // Valores actuales
    float       currentCorrelation = 0.0f;
    float       currentPeak        = -100.0f;

    float       correlationDeviation = 0.0f;

    enum class Status : uint8_t {
        OnTarget,     // Correlación normal (> 0.3)
        NearTarget,   // Correlación baja pero no crítica (0.0 a 0.3)
        OffTarget,    // Correlación negativa o peligrosamente baja (< 0.0)
        NoSignal,     // Sin señal
        UnknownRole   // Rol no especificado
    };
    Status status = Status::UnknownRole;

    juce::String message;

    [[nodiscard]] bool isActionable() const noexcept {
        return status == Status::OffTarget || status == Status::NearTarget;
    }
};
    // ═══ SPRINT 6A: Per-track gain analysis ═══════════════════════════════
public:
    /** Analiza la ganancia de una pista individual contra el target del rol.
        Usa ExpectedProfile de TrackRole.h para obtener targets por rol.
        Retorna TrackGainAdvice con status, delta, y mensaje accionable.
        @param slotIndex  Índice del slot a analizar
        @return TrackGainAdvice con recomendación */
    [[nodiscard]] TrackGainAdvice analyzeTrackGain(int slotIndex);

    /** Analiza TODAS las pistas activas y retorna un vector con los advices.
        Filtra solo pistas con señal y rol conocido.
        Ordenado por severidad (OffTarget primero). */
    [[nodiscard]] std::vector<TrackGainAdvice> analyzeAllTracksGain();

    // ═══ SPRINT 6B: Per-track dynamics analysis ═══════════════════════════
    /** Analiza la dinámica de una pista individual contra el crest target del rol.
        Usa ExpectedProfile de TrackRole.h para obtener targets.
        Retorna TrackDynamicsAdvice con status, subtipo y acción sugerida.
        @param slotIndex  Índice del slot a analizar
        @return TrackDynamicsAdvice con recomendación */
    [[nodiscard]] TrackDynamicsAdvice analyzeTrackDynamics(int slotIndex);

    /** Analiza TODAS las pistas activas y retorna vectorde advices de dinámica.
        Filtra solo pistas con señal y rol conocido.
        Ordenado por severidad (Overcompressed/TooDynamic primero). */
    [[nodiscard]] std::vector<TrackDynamicsAdvice> analyzeAllTracksDynamics();

    // ═══ SPRINT 6C: Per-track tonal analysis ═══════════════════════════════
    /** Analiza el balance espectral de una pista contra el spectralOffset del rol.
        Convierte 30 bandEnergies en 6 regiones (Sub, Bass, LoMid, HiMid, Pres, Air)
        y compara contra el perfil esperado del rol.
        @param slotIndex  Índice del slot a analizar
        @return TrackTonalAdvice con status y mensaje accionable */
    [[nodiscard]] TrackTonalAdvice analyzeTrackTonal(int slotIndex);

    /** Analiza TODAS las pistas activas y retorna vector de advices tonales.
        Filtra solo pistas con señal y rol conocido.
        Ordenado por severidad (OffTarget primero). */
    [[nodiscard]] std::vector<TrackTonalAdvice> analyzeAllTracksTonal();

    // ═════════════════════
    //  SPRINT 8: Per-track phase analysis
    // ═════════════════════
    [[nodiscard]] TrackPhaseAdvice analyzeTrackPhase(int slotIndex);

    [[nodiscard]] std::vector<TrackPhaseAdvice> analyzeAllTracksPhase();

private:
    void updateWorkflowDetector();
};

} // namespace mixcoach