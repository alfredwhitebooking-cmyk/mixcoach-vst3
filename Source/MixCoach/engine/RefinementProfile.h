#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

namespace mixcoach {

    // Forward declarations — solo necesitamos CoachEngine para el compute()
    struct DifferenceProfile;
    struct ReferenceProfile;
    struct MixScore;
    class CoachEngine;

    // ═══════════════════════════════════════════════════════════════════════════
    //  RefinementDomain — Los 5 dominios de refinamiento artístico
    //
    //  Se activan DESPUÉS de que los dominios técnicos están en target.
    //  Miden cualidades perceptuales: profundidad, impacto, movimiento,
    //  cohesión y emoción.
    // ═══════════════════════════════════════════════════════════════════════════
    enum class RefinementDomain : uint8_t
    {
        Depth,    // Profundidad percibida, 3D-ness, espacio
        Impact,   // Punch, pegada, contraste transiente
        Movement, // Dinamismo, energía, evolución en el tiempo
        Glue,     // Cohesión, integración de elementos, que suene "uno"
        Emotion   // Impacto emocional vs referencia
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  RefinementScore — Un score de refinamiento con 3 sub-métricas
    //
    //  • score:  0.0-1.0 normalizado
    //  • label:  cualitativo ("Plano", "Moderado", "Profundo")
    //  • subMetric[3]: desglose que explica POR QUÉ
    //  • vsReference: contraste contra la referencia (-1.0 a +1.0)
    //  • interpretation: texto legible para el LLM
    //  • suggestion: acción recomendada
    // ═══════════════════════════════════════════════════════════════════════════
    struct RefinementScore
    {
        RefinementDomain domain = RefinementDomain::Depth;

        // ═══ Score normalizado ═══════════════════════════════════════════════════
        float score = 0.5f; // 0.0 = pobre, 1.0 = excelente

        // ═══ Label cualitativo ══════════════════════════════════════════════════
        juce::String label; // "Plano", "Moderado", "Profundo", etc.

        // ═══ 3 sub-métricas que componen el score (0.0-1.0) ════════════════════
        float subMetric[3] = {0.5f, 0.5f, 0.5f};
        juce::String subMetricLabel[3]; // "Stereo Depth", "Spatial Dynamics", etc.

        // ═══ Comparación contra la referencia ═══════════════════════════════════
        float vsReference = 0.0f; // -1.0 (ref mucho mejor) a +1.0 (mix mucho mejor)
        bool hasReferenceData = false;

        // ═══ Texto interpretativo para LLM y UI ═════════════════════════════════
        juce::String interpretation; // "La mezcla suena plana en el eje Z..."
        juce::String suggestion;     // "Prueba añadir un delay stereo en los pads..."

        // ═══ Helpers ════════════════════════════════════════════════════════════
        [[nodiscard]] bool isActionable() const noexcept
        {
            return score < 0.6f || (hasReferenceData && std::abs(vsReference) > 0.25f);
        }

        [[nodiscard]] const char* domainName() const noexcept;
        [[nodiscard]] const char* emoji() const noexcept;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  RefinementProfile — Perfil completo de refinamiento artístico
    //
    //  Se computa DESPUÉS de que el motor técnico (DifferenceProfile, MixScore)
    //  ha establecido que la mezcla está sólida. Mide 5 cualidades perceptuales
    //  que separan una mezcla "correcta" de una "profesional".
    //
    //  NO requiere nuevos analizadores. TODO es derivado de datos existentes:
    //    • DifferenceProfile → regionEnergy, spectralSimilarity, deltas
    //    • ReferenceProfile  → punchScore, subBalance, brightnessScore
    //    • MixScore          → overall, gain, tonal, dynamics, spatial
    //    • CoachEngine       → per-track advices, MixHistory, bus summaries
    //
    //  Uso:
    //    auto rp  = ReferenceProfile::computeFromMix(dp);
    //    auto ms  = MixScore::compute(engine, analyzer, genre);
    //    auto ref = RefinementProfile::compute(rp, dp, ms, engine);
    //    if (ref.isRelevant) context += ref.toLLMContext();
    // ═══════════════════════════════════════════════════════════════════════════
    struct RefinementProfile
    {
        // ═══ Metadata ═══════════════════════════════════════════════════════════
        int64_t timestampUs = 0;
        bool valid          = false;

        // ═══ 5 scores de refinamiento ═══════════════════════════════════════════
        RefinementScore depth;
        RefinementScore impact;
        RefinementScore movement;
        RefinementScore glue;
        RefinementScore emotion;

        // ═══ Score compuesto (promedio ponderado de los 5 dominios) ═════════════
        float overallRefinement = 0.0f; // 0.0-1.0

        // ═══ Activación ═════════════════════════════════════════════════════════
        // true solo cuando la mezcla técnicamente está sólida:
        //   • MixScore.overall >= 70
        //   • DifferenceProfile.valid
        //   • criticalGaps == 0
        //   • warningGaps <= 3
        bool isRelevant = false;

        // ═══ Fuentes de datos (para trazabilidad) ═══════════════════════════════
        float sourceMixScore       = 0.0f; // MixScore.overall al momento del compute
        int sourceCriticalGaps     = 0;    // Gaps críticos al momento del compute
        int sourceWarningGaps      = 0;    // Gaps warning al momento del compute
        bool sourceHasReference    = false;

        // ═══════════════════════════════════════════════════════════════════════════
        //  Builder
        // ═══════════════════════════════════════════════════════════════════════════

        /** Construye el RefinementProfile completo desde datos existentes.
            @param rp      ReferenceProfile (scores high-level: punch, subBalance, etc.)
            @param dp      DifferenceProfile (regionEnergy, spectralSimilarity, deltas)
            @param ms      MixScore (overall, gain, tonal, dynamics, spatial)
            @param engine  CoachEngine (per-track advices, MixHistory, bus summaries)
            @return RefinementProfile completo con isRelevant seteado */
        static RefinementProfile compute(
            const ReferenceProfile& rp,
            const DifferenceProfile& dp,
            const MixScore& ms,
            CoachEngine& engine) noexcept;

        // ═══════════════════════════════════════════════════════════════════════════
        //  Per-Domain Compute Functions (públicas para testing)
        // ═══════════════════════════════════════════════════════════════════════════

        static RefinementScore computeDepthScore(
            const DifferenceProfile& dp) noexcept;

        static RefinementScore computeImpactScore(
            const ReferenceProfile& rp,
            const DifferenceProfile& dp,
            CoachEngine& engine) noexcept;

        static RefinementScore computeMovementScore(
            const DifferenceProfile& dp,
            CoachEngine& engine) noexcept;

        static RefinementScore computeGlueScore(
            const DifferenceProfile& dp,
            CoachEngine& engine) noexcept;

        static RefinementScore computeEmotionScore(
            const DifferenceProfile& dp) noexcept;

        // ═══════════════════════════════════════════════════════════════════════════
        //  Serialization (JSON via DynamicObject)
        // ═══════════════════════════════════════════════════════════════════════════

        /** Serializa este RefinementProfile a un DynamicObject. */
        void toJson(juce::DynamicObject& obj) const;

        /** Deserializa desde un DynamicObject. */
        static RefinementProfile fromJson(const juce::DynamicObject& obj);

        /** Guarda a un archivo JSON. Retorna true si ok. */
        bool saveToFile(const juce::File& file) const;

        /** Carga desde un archivo JSON. Retorna perfil válido o inválido. */
        static RefinementProfile loadFromFile(const juce::File& file);

        // ═══════════════════════════════════════════════════════════════════════════
        //  Text Helpers
        // ═══════════════════════════════════════════════════════════════════════════

        /** Resumen breve de los 5 scores para debug. */
        [[nodiscard]] juce::String toShortText() const noexcept;

        /** Texto detallado para depuración. */
        [[nodiscard]] juce::String toVerboseText() const noexcept;

        /** Contexto formateado para inyección en el prompt del LLM.
            Se inyecta en buildFullContext() como [REFINEMENT PROFILE].
            Solo produce texto si isRelevant == true. */
        [[nodiscard]] juce::String toLLMContext() const;

        /** Mensaje natural para mostrar en el chat del coach.
            Ejemplo: "La mezcla suena cohesionada y con movimiento.
                      El impacto está por debajo de la referencia..." */
        [[nodiscard]] juce::String toChatMessage() const;

        // ═══════════════════════════════════════════════════════════════════════════
        //  Static Helpers (públicos para testing — pure functions, no internal state)
        // ═══════════════════════════════════════════════════════════════════════════

        /** Determina si el refinement es relevante basado en los scores técnicos.
            isRelevant = ms.overall >= 70 && dp.valid && dp.criticalGaps == 0
                         && dp.warningGaps <= 3 */
        static bool computeIsRelevant(const MixScore& ms, const DifferenceProfile& dp) noexcept;

        /** Computa el score compuesto overall como promedio ponderado de los 5 dominios.
            Pesos: Depth 0.20, Impact 0.20, Movement 0.20, Glue 0.20, Emotion 0.20 */
        static float computeOverall(const RefinementScore scores[5]) noexcept;

        /** Genera la label cualitativa para un score (0.0-1.0).
            @param domain  Dominio (cada dominio usa sus propios thresholds)
            @param score   Valor normalizado 0.0-1.0
            @return String como "Muy plano", "Profundo", etc. */
        static const char* qualitativeLabel(RefinementDomain domain, float score) noexcept;

        /** Genera una interpretación textual automática para un score.
            Combina el dominio, el score, y el vsReference para producir
            una frase que el LLM puede usar directamente. */
        static juce::String autoInterpretation(const RefinementScore& s) noexcept;

        /** Genera una sugerencia automática para un score accionable. */
        static juce::String autoSuggestion(const RefinementScore& s) noexcept;

        /** Helper: normaliza un valor raw a 0.0-1.0 con clamping.
            @param value    Valor raw
            @param ideal    Valor ideal (mapea a 1.0)
            @param tolerance Rango alrededor de ideal considerado "perfecto"
            @param maxDev   Desviación máxima que mapea a 0.0 */
        static float normalizeToScore(float value, float ideal, float tolerance, float maxDev) noexcept;
    };

} // namespace mixcoach
