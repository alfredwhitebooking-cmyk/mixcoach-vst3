#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include "TrackRole.h"
#include "SpectralProfiler.h"
#include "../../Common/memory/SharedData.h"
#include "../../Common/memory/SlotRegistry.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  SemanticIssue — Un problema o acierto semántico específico
//  Cada issue describe en lenguaje natural qué ocurre y qué hacer.
// ═══════════════════════════════════════════════════════════════════════════
struct SemanticIssue {
    enum class Severity : uint8_t {
        Praise,     // ✅ Logro, está bien
        Info,       // ℹ️ Informativo
        Warning,    // ⚠️ Requiere atención
        Critical    // 🔴 Debe corregirse
    };

    enum class Domain : uint8_t {
        Gain,           // Ganancia / nivel
        Dynamics,       // Crest/compresión
        Spectral,       // EQ / balance tonal
        StereoWidth,    // Imagen estéreo
        Phase,          // Correlación / fase
        Transient,      // Transientes / ataque
        Masking,        // Enmascaramiento
        Headroom,       // Margen dinámico
        MonoCompat,     // Compatibilidad mono
        Fundamental     // Frecuencia fundamental
    };

    Severity severity   = Severity::Info;
    Domain   domain     = Domain::Gain;
    juce::String trackName;
    TrackRole  role     = TrackRole::Unknown;

    // Mensaje descriptivo del problema (en español o inglés)
    juce::String message;

    // Valor actual y esperado (para dar precisión)
    float actualValue   = 0.0f;
    float expectedValue = 0.0f;
    float deviation     = 0.0f;   // actual - expected (dB o ratio)

    // Rango de frecuencia afectado (para overlay visual en el Spectrograph)
    float affectedLowHz  = 20.0f;   // Frecuencia inferior del rango afectado
    float affectedHighHz = 20000.0f; // Frecuencia superior del rango afectado

    // Recomendación concreta y ejecutable
    juce::String recommendation;

    [[nodiscard]] bool isProblem() const noexcept {
        return severity == Severity::Warning || severity == Severity::Critical;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
//  SemanticDiff — Resultado completo de la comparación para una pista
// ═══════════════════════════════════════════════════════════════════════════
struct SemanticDiff {
    int slotIndex = -1;
    juce::String trackName;
    TrackRole  role     = TrackRole::Unknown;
    TrackSpectralProfile profile;   // Lo que REALMENTE es
    ExpectedProfile      expected;  // Lo que DEBERÍA ser

    std::vector<SemanticIssue> issues;
    std::vector<SemanticIssue> praises;

    // Banda espectral enfocada por este diff (-1 = no especifica)
    int spectralFocusBand = -1;

    [[nodiscard]] int problemCount() const noexcept {
        int c = 0;
        for (const auto& iss : issues)
            if (iss.isProblem()) c++;
        return c;
    }

    [[nodiscard]] bool hasProblems() const noexcept { return problemCount() > 0; }

    // Construye un resumen textual de 1-2 líneas para incluir en prompts LLM
    [[nodiscard]] juce::String toTextSummary() const;
};

// ═══════════════════════════════════════════════════════════════════════════
//  SemanticComparator — Compara telemetría real contra perfiles esperados
//  Produce análisis semántico que tanto el sistema experto como el LLM
//  pueden usar directamente para dar consejos precisos.
// ═══════════════════════════════════════════════════════════════════════════
class SemanticComparator {
public:
    /** Compara una pista individual contra su perfil esperado. */
    static SemanticDiff compareTrack(int slotIndex,
                                      const juce::String& trackName,
                                      TrackRole role,
                                      const TrackSpectralProfile& profile);

    /** Escanea todas las pistas activas y produce diffs semánticos. */
    static std::vector<SemanticDiff> compareAllTracks(const SlotRegistry& registry,
                                                       const SharedData& sharedData,
                                                       const std::array<TrackRole, SlotRegistry::kMaxSlots>& roles);

private:
    // ─── Comparaciones individuales ───────────────────────────────
    static void checkGain(SemanticDiff& diff, const TrackSpectralProfile& p, const ExpectedProfile& e);
    static void checkDynamics(SemanticDiff& diff, const TrackSpectralProfile& p, const ExpectedProfile& e);
    static void checkSpectralBalance(SemanticDiff& diff, const TrackSpectralProfile& p, const ExpectedProfile& e);
    static void checkStereoWidth(SemanticDiff& diff, const TrackSpectralProfile& p, const ExpectedProfile& e);
    static void checkTransient(SemanticDiff& diff, const TrackSpectralProfile& p, const ExpectedProfile& e);

    // ─── Helpers ──────────────────────────────────────────────────
    static juce::String formatDb(float value);
    static juce::String roleDisplayName(TrackRole role);
    static SemanticIssue makeIssue(SemanticIssue::Severity sev,
                                    SemanticIssue::Domain dom,
                                    const juce::String& trackName,
                                    TrackRole role,
                                    const juce::String& message,
                                    float actual, float expected,
                                    const juce::String& recommendation);
    static SemanticIssue makePraise(SemanticIssue::Severity sev,
                                     SemanticIssue::Domain dom,
                                     const juce::String& trackName,
                                     TrackRole role,
                                     const juce::String& message);
};

} // namespace mixcoach
