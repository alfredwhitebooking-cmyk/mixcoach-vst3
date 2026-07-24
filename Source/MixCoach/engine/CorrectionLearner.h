#pragma once
#include <juce_core/juce_core.h>
#include <map>
#include <vector>
#include "TrackRole.h"
#include "SpectralProfiler.h"

namespace mixcoach {

    // Forward declarations
    class SlotRegistry;
    class SharedData;
    struct CorrectionCardData;

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
    struct KeywordCorrection
    {
        juce::String keyword;         // "kick", "snare", "pad", etc.
        TrackRole targetRole;         // A qué rol se corrigió
        int count             = 0;    // Cuántas veces se corrigió a este rol
        float confidenceBoost = 0.0f; // Boost calculado (count * 0.1f, max 0.3f)
    };

    // ─── Corrección espectral: un perfil espectral que fue corregido ───────────
    // Usado por SpectralProfiler::inferTrackRole() para priorizar roles corregidos.
    struct SpectralCorrection
    {
        // Métricas clave del perfil espectral (suficientes para matching)
        float bandLevelDb[6]; // Perfil de 6 bandas (dBFS)
        float crestDb        = 0.0f;
        float correlation    = 0.0f;
        float transientRatio = 0.0f;
        float avgStereoWidth = 0.0f;

        TrackRole correctedRole = TrackRole::Unknown;
        int correctionCount     = 0;

        [[nodiscard]] float similarityTo(const TrackSpectralProfile& p) const noexcept;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  CorrectionLearner — Clase principal
    // ═══════════════════════════════════════════════════════════════════════════
    class CorrectionLearner
    {
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
        // ═══ Transport window tracking (invalidate by section change) ═══════
        /** Registra la posición del transporte cuando se propone una corrección.
            Se llama desde CoachingNarrativeDirector::onOptionSelected() para
            recordar en qué punto de la canción estábamos cuando recomendamos algo.
            @param positionSec  Posición actual del transporte en segundos
            @param bpm          Beats per minute actuales
            @param tsNum        Time signature numerator (ej: 4)
            @param tsDen        Time signature denominator (ej: 4) */
        void startTransportWindow(double positionSec, double bpm, int tsNum, int tsDen) noexcept;

        /** Verifica si el transporte se movió más de 2 compases desde startTransportWindow().
            Si devuelve true, la verificación debe invalidarse porque la canción
            cambió de sección y los datos anteriores ya no son representativos.
            @param currentPositionSec  Posición actual del transporte
            @param currentBpm          BPM actual (puede haber cambiado)
            @param tsNum               Time signature numerator actual
            @param tsDen               Time signature denominator actual
            @return true si el salto > 2 compases, false si está dentro del rango */
        [[nodiscard]] bool isSectionChanged(double currentPositionSec, double currentBpm,
                                              int tsNum, int tsDen) const noexcept;

        /** Retorna true si hay una ventana de transporte activa (se llamó startTransportWindow). */
        [[nodiscard]] bool hasTransportWindow() const noexcept { return transportValid_; }

        [[nodiscard]] float getKeywordBoost(const juce::String& keyword, TrackRole candidateRole) const noexcept;

        /** Retorna el rol corregido más probable para un perfil espectral.
            Si el usuario ha corregido un perfil similar antes, retorna
            el rol corregido con alta confianza.
            Retorna Unknown si no hay match significativo. */
        [[nodiscard]] TrackRole getSpectralCorrection(const TrackSpectralProfile& profile,
                                                      float& confidence) const noexcept;

        // ─── Persistencia ────────────────────────────────────────────────────

        void toJson(juce::DynamicObject& obj) const;
        void fromJson(const juce::DynamicObject& obj);

        // ─── Estado ───────────────────────────────────────────────────────────
        [[nodiscard]] bool hasKeywordData() const noexcept { return !keywordCorrections_.empty(); }

        [[nodiscard]] bool hasSpectralData() const noexcept { return !spectralCorrections_.empty(); }

        [[nodiscard]] int getTotalCorrections() const noexcept;

        /** Registra una corrección de mezcla (EQ, compresión, gain, etc.).
            A diferencia de recordCorrection (que aprende roles), este método
            trackea qué correcciones se aplicaron durante la sesión para
            mejorar futuras recomendaciones. */
        /** Verifica una corrección de mezcla: lee el valor actual del track,
            compara contra el target, determina el resultado (Verified/Partial/Failed)
            y actualiza CorrectionCardData.status, verifiedAfter, verifiedDelta y
            feedbackMessage en el lugar.
            @param data      Datos de la corrección (se modifica in-place)
            @param registry  SlotRegistry para buscar el slot por nombre
            @param shared    SharedData para leer TrackAudioResult
            @param callback  Función opcional para leer TrackAudioResult específico */
        void verifyMixCorrection(CorrectionCardData& data,
                                  const SlotRegistry& registry,
                                  const SharedData& shared,
                                  std::function<float(int slotIndex, const juce::String& metric)> readValueCallback = nullptr);

        void recordMixCorrection(const juce::String& trackName,
                                  const juce::String& domain,
                                  float beforeValue,
                                  float afterValue);

        // ═══════════════════════════════════════════════════════════════════
        //  ═══ V4b: Verify tracking — cuántas verificaciones se intentaron
        //  y cuántas fueron invalidadas por cambio de sección. Esto alimenta
        //  la métrica de confianza del reporte final.
        // ═══════════════════════════════════════════════════════════════════

        /** Registra un intento de verificación (llamado desde CoachingNarrativeDirector
            antes de iniciar verify loop). */
        void recordVerifyAttempt() noexcept { ++totalVerifyAttempts_; }

        /** Registra una verificación invalidada por cambio de sección. */
        void recordVerifyInvalidation() noexcept { ++verifyInvalidatedBySection_; }

        /** Retorna el total de intentos de verificación en esta sesión. */
        [[nodiscard]] int getTotalVerifyAttempts() const noexcept { return totalVerifyAttempts_; }

        /** Retorna cuántas verificaciones fueron invalidadas por cambio de sección. */
        [[nodiscard]] int getVerifyInvalidatedCount() const noexcept { return verifyInvalidatedBySection_; }

        /** Retorna la confianza del sistema en los verify (0-100).
            Fórmula: (total - invalidated) / total * 100
            Si no hay verificaciones, retorna 100 (default optimista). */
        [[nodiscard]] int getVerifyConfidencePercent() const noexcept
        {
            if (totalVerifyAttempts_ == 0) return 100;
            float ratio = 1.0f - (float)verifyInvalidatedBySection_ / (float)totalVerifyAttempts_;
            return juce::jlimit(0, 100, (int)(ratio * 100.0f));
        }

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

        // ═══ Transport window tracking — fields ════════════════════════════════
        double transportWindowSec_ = -1.0;
        double transportWindowBpm_ = 120.0;
        int transportWindowTsNum_ = 4;
        int transportWindowTsDen_ = 4;
        bool transportValid_ = false;

        // ═══ V4b: Verify tracking counters ═══════════════════════════════════
        int totalVerifyAttempts_ = 0;
        int verifyInvalidatedBySection_ = 0;

        static constexpr int kMinCorrectionsForBoost = 2;     // Min correcciones para aplicar boost
        static constexpr float kBoostPerCorrection   = 0.10f; // +0.1 por corrección
        static constexpr float kMaxBoost             = 0.30f; // Máximo boost acumulado
        static constexpr int kMaxSpectralCorrections = 50;    // Límite de entradas espectrales
    };

} // namespace mixcoach
