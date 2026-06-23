#pragma once
#include <juce_core/juce_core.h>
#include <map>
#include <vector>
#include "TrackRole.h"
#include "SpectralProfiler.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  CorrectionLearner — Aprende de las correcciones del usuario
//
//  Propósito: Cuando el usuario corrige manualmente un rol inferido
//  (ej: cambia "Snare" → "Clap"), el sistema aprende de esa corrección
//  para mejorar futuras inferencias.
//
//  Dos modos de aprendizaje:
//    1. Por keywords: si la inferencia fue por nombre y el usuario
//       corrige, se ajustan los pesos de las keywords.
//    2. Por espectro: si la inferencia fue espectral y el usuario
//       corrige, se guarda el perfil espectral corregido.
//
//  Persistencia: Se guarda en session_memory.json via toJson/fromJson.
// ═══════════════════════════════════════════════════════════════════════════

// ─── Corrección por keyword: qué keyword llevó a qué rol, y cuántas veces ──
// Usado por inferTrackRoleFromName() para ajustar confianza.
struct KeywordCorrection {
    juce::String keyword;       // "kick", "snare", "pad", etc.
    TrackRole    targetRole;    // A qué rol se corrigió
    int          count = 0;     // Cuántas veces se corrigió a este rol
    float        confidenceBoost = 0.0f; // Boost calculado (count * 0.1f, max 0.3f)
};

// ─── Corrección espectral: un perfil espectral que fue corregido ───────────
// Usado por SpectralProfiler::inferTrackRole() para priorizar roles corregidos.
struct SpectralCorrection {
    // Métricas clave del perfil espectral (suficientes para matching)
    float bandLevelDb[6];       // Perfil de 6 bandas (dBFS)
    float crestDb      = 0.0f;
    float correlation  = 0.0f;
    float transientRatio = 0.0f;
    float avgStereoWidth = 0.0f;

    TrackRole correctedRole = TrackRole::Unknown;
    int       correctionCount = 0;

    [[nodiscard]] float similarityTo(const TrackSpectralProfile& p) const noexcept;
};

// ═══════════════════════════════════════════════════════════════════════════
//  CorrectionLearner — Clase principal
// ═══════════════════════════════════════════════════════════════════════════
class CorrectionLearner {
public:
    CorrectionLearner() = default;

    // ─── Registrar correcciones ──────────────────────────────────────────

    /** Registra una corrección del usuario: cambió de oldRole a newRole.
        @param oldRole       El rol que el sistema había inferido
        @param newRole       El rol que el usuario seleccionó manualmente
        @param inferredFromName  true si la inferencia original fue por nombre
        @param keywords      Lista de keywords que activaron la inferencia
                             (ej: ["hasSnare", "hasClap"]), vacío si no aplica
        @param spectral      Perfil espectral en el momento de la corrección
                             (usado si inferredFromName=false) */
    void recordCorrection(TrackRole oldRole,
                          TrackRole newRole,
                          bool inferredFromName,
                          const std::vector<juce::String>& keywords,
                          const TrackSpectralProfile& spectral);

    // ─── Consultar ajustes ───────────────────────────────────────────────

    /** Retorna el boost de confianza (0.0-0.3) para un par keyword+rol.
        Si el usuario ha corregido "snare" → "Clap" varias veces,
        devuelve un boost que hace que inferTrackRoleFromName() prefiera Clap
        cuando detecte "snare" en el nombre. */
    [[nodiscard]] float getKeywordBoost(const juce::String& keyword,
                                         TrackRole candidateRole) const noexcept;

    /** Retorna el rol corregido más probable para un perfil espectral.
        Si el usuario ha corregido un perfil similar antes, retorna
        el rol corregido con alta confianza.
        Retorna Unknown si no hay match significativo. */
    [[nodiscard]] TrackRole getSpectralCorrection(
        const TrackSpectralProfile& profile, float& confidence) const noexcept;

    // ─── Persistencia ────────────────────────────────────────────────────

    void toJson(juce::DynamicObject& obj) const;
    void fromJson(const juce::DynamicObject& obj);

    // ─── Estado ───────────────────────────────────────────────────────────
    [[nodiscard]] bool hasKeywordData() const noexcept {
        return !keywordCorrections_.empty();
    }
    [[nodiscard]] bool hasSpectralData() const noexcept {
        return !spectralCorrections_.empty();
    }
    [[nodiscard]] int getTotalCorrections() const noexcept;

    /** Debug: imprime estado actual de aprendizaje. */
    [[nodiscard]] juce::String dumpState() const;

    /** Similaridad entre dos perfiles espectrales (0.0-1.0).
        Normalizada: 1.0 = idénticos, 0.0 = completamente diferentes. */
    static float spectralSimilarity(const TrackSpectralProfile& a,
                                    const float bBandLevel[6],
                                    float bCrestDb,
                                    float bCorrelation,
                                    float bTransientRatio,
                                    float bAvgStereoWidth) noexcept;

private:
    // keyword → (targetRole → count)
    std::map<juce::String, std::map<TrackRole, int>> keywordCorrections_;
    std::vector<SpectralCorrection> spectralCorrections_;

    static constexpr int   kMinCorrectionsForBoost = 2;   // Min correcciones para aplicar boost
    static constexpr float kBoostPerCorrection      = 0.10f; // +0.1 por corrección
    static constexpr float kMaxBoost               = 0.30f; // Máximo boost acumulado
    static constexpr int   kMaxSpectralCorrections  = 50;   // Límite de entradas espectrales
};

} // namespace mixcoach
