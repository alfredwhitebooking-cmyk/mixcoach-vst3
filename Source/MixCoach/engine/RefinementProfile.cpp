#include "RefinementProfile.h"
#include "DifferenceProfile.h"
#include "ReferenceProfile.h"
#include "MixScore.h"
#include "CoachEngine.h"
#include "TrackRole.h"
#include "TrackDynamicsAnalyzer.h"
#include "TrackPhaseAnalyzer.h"
#include <algorithm>
#include <cmath>
#include <vector>

// ═══ Comparison operators for TrackAdvice status enums ═══
namespace mixcoach {
bool operator!=(CoachEngine::TrackDynamicsAdvice::Status a, CoachEngine::TrackDynamicsAdvice::Status b) noexcept
{
    return static_cast<int>(a) != static_cast<int>(b);
}
bool operator!=(CoachEngine::TrackPhaseAdvice::Status a, CoachEngine::TrackPhaseAdvice::Status b) noexcept
{
    return static_cast<int>(a) != static_cast<int>(b);
}

    // ═══════════════════════════════════════════════════════════════════════════
    //  Static Helpers
    // ═══════════════════════════════════════════════════════════════════════════

    float RefinementProfile::normalizeToScore(float value, float ideal, float tolerance, float maxDev) noexcept
    {
        float deviation = std::abs(value - ideal);
        if (deviation <= tolerance) return 1.0f;
        if (deviation >= maxDev) return 0.0f;
        return 1.0f - (deviation - tolerance) / (maxDev - tolerance);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  RefinementScore::domainName / emoji
    // ═══════════════════════════════════════════════════════════════════════════

    const char* RefinementScore::domainName() const noexcept
    {
        switch (domain) {
            case RefinementDomain::Depth:    return "Depth";
            case RefinementDomain::Impact:   return "Impact";
            case RefinementDomain::Movement: return "Movement";
            case RefinementDomain::Glue:     return "Glue";
            case RefinementDomain::Emotion:  return "Emotion";
            default:                         return "Unknown";
        }
    }

    const char* RefinementScore::emoji() const noexcept
    {
        if (score >= 0.80f) return "\xF0\x9F\x9F\xA2";  // 🟢 Excelente
        if (score >= 0.60f) return "\xF0\x9F\x9F\xA1";  // 🟡 Buena
        if (score >= 0.40f) return "\xF0\x9F\x94\xB6";  // 🔶 Moderada
        return "\xF0\x9F\x94\xB4";                        // 🔴 Necesita atención
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Qualitative Labels
    // ═══════════════════════════════════════════════════════════════════════════

    const char* RefinementProfile::qualitativeLabel(RefinementDomain domain, float score) noexcept
    {
        // Cada dominio usa sus propios thresholds semánticos
        switch (domain) {
            case RefinementDomain::Depth:
                if (score < 0.33f) return "\"Plana\"";
                if (score < 0.66f) return "\"Moderada\"";
                return "\"Profunda\"";

            case RefinementDomain::Impact:
                if (score < 0.33f) return "\"Baja\"";
                if (score < 0.66f) return "\"Moderada\"";
                return "\"Fuerte\"";

            case RefinementDomain::Movement:
                if (score < 0.33f) return "\"Est\xC3\xA1" "tica\"";
                if (score < 0.66f) return "\"Moderada\"";
                return "\"Viva\"";

            case RefinementDomain::Glue:
                if (score < 0.33f) return "\"Fragmentada\"";
                if (score < 0.66f) return "\"Cohesionada\"";
                return "\"Integrada\"";

            case RefinementDomain::Emotion:
                if (score < 0.33f) return "\"Distante\"";
                if (score < 0.66f) return "\"Cercana\"";
                return "\"Conectada\"";

            default:
                return "\"-\"";
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  computeIsRelevant — El refinement solo se activa si la mezcla está
    //  técnicamente sólida: score >= 70, sin gaps críticos, máx 3 warnings.
    // ═══════════════════════════════════════════════════════════════════════════

    bool RefinementProfile::computeIsRelevant(const MixScore& ms, const DifferenceProfile& dp) noexcept
    {
        return dp.valid
            && ms.overall >= 70
            && dp.criticalGaps == 0
            && dp.warningGaps <= 3
            && ms.gain > 30     // Al menos algo de gain health
            && ms.tonal > 30;   // Algo de tonal health
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  computeOverall — Promedio ponderado de los 5 dominios
    //  Pesos iguales (0.20 cada uno) porque todos son igualmente importantes
    //  para el refinamiento artístico.
    // ═══════════════════════════════════════════════════════════════════════════

    float RefinementProfile::computeOverall(const RefinementScore scores[5]) noexcept
    {
        float sum = 0.0f;
        for (int i = 0; i < 5; ++i)
            sum += scores[i].score;
        return sum / 5.0f;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  autoInterpretation — Genera texto interpretativo automático
    //  ═══════════════════════════════════════════════════════════════════════════

    juce::String RefinementProfile::autoInterpretation(const RefinementScore& s) noexcept
    {
        juce::String text;

        // Evaluación del score absoluto
        if (s.score >= 0.80f) {
            switch (s.domain) {
                case RefinementDomain::Depth:    text = "La mezcla tiene excelente profundidad y tridimensionalidad."; break;
                case RefinementDomain::Impact:   text = "La mezcla tiene gran pegada y contraste din\xC3\xA1" "mico."; break;
                case RefinementDomain::Movement: text = "La mezcla es muy viva y evoluciona naturalmente."; break;
                case RefinementDomain::Glue:     text = "Los elementos suenan muy cohesionados, como un todo."; break;
                case RefinementDomain::Emotion:  text = "La mezcla transmite la emoci\xC3\xB3" "n deseada con claridad."; break;
            }
        }
        else if (s.score >= 0.60f) {
            switch (s.domain) {
                case RefinementDomain::Depth:    text = "La mezcla tiene profundidad moderada, con espacio percibible."; break;
                case RefinementDomain::Impact:   text = "La pegada es moderada, con contraste aceptable."; break;
                case RefinementDomain::Movement: text = "La mezcla tiene movimiento moderado, con algunos cambios de energ\xC3\xAD" "a."; break;
                case RefinementDomain::Glue:     text = "Los elementos est\xC3\xA1" "n moderadamente cohesionados."; break;
                case RefinementDomain::Emotion:  text = "La mezcla se acerca a la emoci\xC3\xB3" "n deseada, pero no del todo."; break;
            }
        }
        else {
            switch (s.domain) {
                case RefinementDomain::Depth:    text = "La mezcla suena plana, sin profundidad percibible."; break;
                case RefinementDomain::Impact:   text = "La mezcla carece de pegada, suena comprimida o sin contraste."; break;
                case RefinementDomain::Movement: text = "La mezcla es est\xC3\xA1" "tica, sin evoluci\xC3\xB3" "n de energ\xC3\xAD" "a."; break;
                case RefinementDomain::Glue:     text = "Los elementos suenan separados, sin cohesi\xC3\xB3" "n."; break;
                case RefinementDomain::Emotion:  text = "La mezcla no logra transmitir la emoci\xC3\xB3" "n deseada."; break;
            }
        }

        // Añadir comparación con referencia si existe
        if (s.hasReferenceData && std::abs(s.vsReference) > 0.10f) {
            if (s.vsReference > 0.0f) {
                text += " La referencia tiene menos " + juce::String(s.domainName()).toLowerCase() + " que la mezcla.";
            } else {
                text += " La referencia tiene m\xC3\xA1" "s " + juce::String(s.domainName()).toLowerCase() + " que la mezcla.";
            }
        }

        return text;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  autoSuggestion — Genera sugerencia automática para score accionable
    // ═══════════════════════════════════════════════════════════════════════════

    juce::String RefinementProfile::autoSuggestion(const RefinementScore& s) noexcept
    {
        if (!s.isActionable() || s.score >= 0.80f) return {};

        switch (s.domain) {
            case RefinementDomain::Depth:
                if (s.subMetric[0] < 0.5f) return "Prueba ensanchar el est\xC3\xA9" "reo de los pads o las guitarras con un stereo imager o duplicando pistas con paneo opuesto.";
                if (s.subMetric[1] < 0.5f) return "Prueba a\xC3\xB1" "adir un delay est\xC3\xA9" "reo sutil (pong 30-50ms) en las pistas de ambiente para crear m\xC3\xA1" "s profundidad.";
                return "Prueba saturar ligeramente las frecuencias altas (8-12kHz) con un excitador arm\xC3\xB3" "nico para dar aire y profundidad.";

            case RefinementDomain::Impact:
                if (s.subMetric[0] < 0.5f) return "Prueba un compressor con attack de 10-20ms y ratio 4:1 en el bus de drums para aumentar la pegada.";
                if (s.subMetric[1] < 0.5f) return "Prueba a\xC3\xB1" "adir 2-3dB en 60-100Hz con un bell en el kick para darle m\xC3\xA1" "s peso al golpe.";
                return "Prueba automatizar el volumen de los elementos percusivos para crear m\xC3\xA1" "s contraste entre golpes y sostenidos.";

            case RefinementDomain::Movement:
                if (s.subMetric[0] < 0.5f) return "Prueba automatizar el volumen de introducciones o breakdowns para crear contraste entre secciones.";
                if (s.subMetric[1] < 0.5f) return "Prueba reducir 1-2dB de compresi\xC3\xB3" "n en el master bus para recuperar rango din\xC3\xA1" "mico natural.";
                return "Prueba a\xC3\xB1" "adir filtros autom\xC3\xA1" "ticos (HPF/LPF) en transiciones entre secciones para crear movimiento.";

            case RefinementDomain::Glue:
                if (s.subMetric[0] < 0.5f) return "Prueba un compresor de bus con ratio 2:1, attack 30ms, release 100ms para pegar los elementos.";
                if (s.subMetric[1] < 0.5f) return "Prueba revisar la correlaci\xC3\xB3" "n de fase entre buses: si un bus est\xC3\xA1" " muy fuera de fase, revisa los wideners est\xC3\xA9" "reo.";
                return "Prueba un EQ sutil en el bus maestro (1-2dB de shelf a 60Hz o 10kHz) para alinear el balance general.";

            case RefinementDomain::Emotion:
                if (s.subMetric[0] < 0.5f) return "Prueba comparar el balance espectral contra la referencia: sube 2-3dB en las regiones donde la referencia tiene m\xC3\xA1" "s energ\xC3\xAD" "a.";
                if (s.subMetric[1] < 0.5f) return "Prueba ajustar la din\xC3\xA1" "mica general: la referencia puede tener m\xC3\xA1" "s o menos compresi\xC3\xB3" "n que la mezcla actual.";
                return "Prueba ecualizar el rango medio (500Hz-3kHz) para alinearlo con la referencia, ya que define el car\xC3\xA1" "cter emocional de la mezcla.";

            default:
                return {};
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Compute: Depth Score
    //  Sub-métricas:
    //    ① Stereo Width: rp.stereoWidth (0-1), ideal ~0.5 (ni mono ni phase-cancel)
    //    ② Spatial Breathing: dp.mixCorrelation, ideal ~0.6 (ni muy correlacionado ni out-of-phase)
    //    ③ High-End Air: dp.mixRegionEnergy[4..5] (Presence+Air), más energía = más profundidad
    // ═══════════════════════════════════════════════════════════════════════════

    RefinementScore RefinementProfile::computeDepthScore(const DifferenceProfile& dp) noexcept
    {
        RefinementScore s;
        s.domain = RefinementDomain::Depth;

        // ① Stereo Width: estimamos desde correlation: width ≈ 1 - |correlation|
        float estStereoWidth = 1.0f - std::abs(dp.mixCorrelation);
        s.subMetric[0] = juce::jlimit(0.0f, 1.0f, estStereoWidth);
        s.subMetricLabel[0] = "Stereo Width";

        // ② Spatial Breathing: correlación que respira es ideal (0.4-0.7)
        //    Valores cercanos a 0.0 (out-of-phase) o 1.0 (mono) reducen profundidad
        s.subMetric[1] = 1.0f - std::abs(dp.mixCorrelation - 0.55f) / 0.55f;
        s.subMetric[1] = juce::jlimit(0.0f, 1.0f, s.subMetric[1]);
        s.subMetricLabel[1] = "Spatial Breathing";

        // ③ High-End Air: energía en Presence+Air normalizada
        //    dBFS típico: -40 a -10 → normalizamos a 0-1
        float presenceEnergy = dp.mixRegionEnergy[4] > -90.0f
            ? juce::jmap(juce::jlimit(-40.0f, -10.0f, dp.mixRegionEnergy[4]), -40.0f, -10.0f, 0.0f, 1.0f)
            : 0.0f;
        float airEnergy = dp.mixRegionEnergy[5] > -90.0f
            ? juce::jmap(juce::jlimit(-40.0f, -10.0f, dp.mixRegionEnergy[5]), -40.0f, -10.0f, 0.0f, 1.0f)
            : 0.0f;
        s.subMetric[2] = (presenceEnergy + airEnergy) * 0.5f;
        s.subMetricLabel[2] = "High-End Air";

        // Score = weighted average
        s.score = s.subMetric[0] * 0.35f + s.subMetric[1] * 0.35f + s.subMetric[2] * 0.30f;
        s.score = juce::jlimit(0.0f, 1.0f, s.score);

        // vsReference: comparar mixRegionEnergy[4..5] vs ref
        if (dp.mixRegionEnergy[4] > -90.0f && dp.refRegionEnergy[4] > -90.0f
            && dp.mixRegionEnergy[5] > -90.0f && dp.refRegionEnergy[5] > -90.0f)
        {
            float mixAir  = (dp.mixRegionEnergy[4] + dp.mixRegionEnergy[5]) * 0.5f;
            float refAir  = (dp.refRegionEnergy[4] + dp.refRegionEnergy[5]) * 0.5f;
            float airDiff = mixAir - refAir;
            s.vsReference = juce::jmap(juce::jlimit(-6.0f, 6.0f, airDiff), -6.0f, 6.0f, -1.0f, 1.0f);
            s.hasReferenceData = true;
        }

        s.label = qualitativeLabel(s.domain, s.score);
        s.interpretation = autoInterpretation(s);
        s.suggestion = autoSuggestion(s);

        return s;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Compute: Impact Score
    //  Sub-métricas:
    //    ① Transient Punch: rp.punchScore (crest factor 6→14dB mapeado a 0→1)
    //    ② Low-End Thump: rp.subBalance (energía Sub+Bass relativa al resto)
    //    ③ Dynamic Contrast: diferencia de crest entre tracks percusivos y sostenidos
    // ═══════════════════════════════════════════════════════════════════════════

    RefinementScore RefinementProfile::computeImpactScore(
        const ReferenceProfile& rp,
        const DifferenceProfile& dp,
        CoachEngine& engine) noexcept
    {
        RefinementScore s;
        s.domain = RefinementDomain::Impact;

        // ① Transient Punch: punchScore directamente del ReferenceProfile
        s.subMetric[0] = juce::jlimit(0.0f, 1.0f, rp.punchScore);
        s.subMetricLabel[0] = "Transient Punch";

        // ② Low-End Thump: subBalance del ReferenceProfile
        s.subMetric[1] = juce::jlimit(0.0f, 1.0f, rp.subBalance);
        s.subMetricLabel[1] = "Low-End Thump";

        // ③ Dynamic Contrast: crest de percusivos (Kick, Snare) vs sostenidos (Pad, Strings)
        //    Si no hay tracks con rol o no hay datos, usar valor por defecto
        s.subMetric[2] = 0.5f;
        s.subMetricLabel[2] = "Dynamic Contrast";
        {
            float percussiveCrestSum = 0.0f;
            int percussiveCount = 0;
            float sustainedCrestSum = 0.0f;
            int sustainedCount = 0;

            auto& registry = engine.getSharedData().getSlotRegistry();
            registry.forEachActive([&](const SlotInfo& info) {
                TrackRole role = engine.getTrackRole(info.slotIndex);
                auto advice = engine.analyzeTrackDynamics(info.slotIndex);

                // Clasificar como percusivo o sostenido según el rol
                bool isPercussive = (role == TrackRole::Kick || role == TrackRole::Kick808
                    || role == TrackRole::ReggaetonKick || role == TrackRole::Snare
                    || role == TrackRole::SnareTrap || role == TrackRole::Tom
                    || role == TrackRole::TomFloor || role == TrackRole::Clap
                    || role == TrackRole::HiHat || role == TrackRole::HiHatOpen
                    || role == TrackRole::Percussion || role == TrackRole::Crash
                    || role == TrackRole::Ride);

                bool isSustained = (role == TrackRole::SynthPad || role == TrackRole::Strings
                    || role == TrackRole::Brass || role == TrackRole::Winds
                    || role == TrackRole::FxAmbience || role == TrackRole::FxNoise);

                if (advice.status != CoachEngine::TrackDynamicsAdvice::Status::NoSignal) {
                    if (isPercussive) {
                        percussiveCrestSum += advice.currentCrest;
                        ++percussiveCount;
                    }
                    else if (isSustained) {
                        sustainedCrestSum += advice.currentCrest;
                        ++sustainedCount;
                    }
                }
            });

            if (percussiveCount > 0 && sustainedCount > 0) {
                float avgPercCrest = percussiveCrestSum / percussiveCount;
                float avgSustCrest = sustainedCrestSum / sustainedCount;
                float contrast = avgPercCrest - avgSustCrest;

                // Ideal: percusivos tienen 4-10dB más de crest que sostenidos
                s.subMetric[2] = juce::jmap(juce::jlimit(2.0f, 12.0f, contrast), 2.0f, 12.0f, 0.0f, 1.0f);
            }
        }

        // Score = weighted average
        s.score = s.subMetric[0] * 0.45f + s.subMetric[1] * 0.30f + s.subMetric[2] * 0.25f;
        s.score = juce::jlimit(0.0f, 1.0f, s.score);

        // vsReference: comparar crestFactor contra referencia
        if (dp.mixCrestFactor > 0.0f && dp.refCrestFactor > 0.0f) {
            float crestDiff = dp.mixCrestFactor - dp.refCrestFactor;
            s.vsReference = juce::jmap(juce::jlimit(-8.0f, 8.0f, crestDiff), -8.0f, 8.0f, -1.0f, 1.0f);
            s.hasReferenceData = true;
        }

        s.label = qualitativeLabel(s.domain, s.score);
        s.interpretation = autoInterpretation(s);
        s.suggestion = autoSuggestion(s);

        return s;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Compute: Movement Score
    //  Sub-métricas:
    //    ① Dynamic Range: rp.dynamicScore (crest + LRA combinados)
    //    ② Loudness Breathing: dp.mixLoudnessRange, ideal 6-10 LU
    //    ③ Mix Activity: count de MixHistory entries recientes
    // ═══════════════════════════════════════════════════════════════════════════

    RefinementScore RefinementProfile::computeMovementScore(
        const DifferenceProfile& dp,
        CoachEngine& engine) noexcept
    {
        RefinementScore s;
        s.domain = RefinementDomain::Movement;

        // ① Dynamic Range: usar dynamicScore previo o estimar desde LRA
        //    Si no tenemos ReferenceProfile, usar loudnessRange directamente
        s.subMetric[0] = juce::jmap(juce::jlimit(3.0f, 12.0f, dp.mixLoudnessRange), 3.0f, 12.0f, 0.0f, 1.0f);
        s.subMetricLabel[0] = "Dynamic Range";

        // ② Loudness Breathing: LRA ideal entre 6-10 LU para la mayoría de géneros
        s.subMetric[1] = normalizeToScore(dp.mixLoudnessRange, 8.0f, 2.0f, 6.0f);
        s.subMetricLabel[1] = "Loudness Breathing";

        // ③ Mix Activity: count de MixHistory reciente (más cambios = más viva)
        {
            auto mixHistory = engine.getMixHistory(30);
            int historyCount = static_cast<int>(mixHistory.size());

            // Normalizar: 0 cambios = 0.0, 20+ cambios = 1.0
            s.subMetric[2] = juce::jlimit(0.0f, 1.0f, historyCount / 20.0f);
            s.subMetricLabel[2] = "Mix Activity";
        }

        // Score = weighted average
        s.score = s.subMetric[0] * 0.35f + s.subMetric[1] * 0.35f + s.subMetric[2] * 0.30f;
        s.score = juce::jlimit(0.0f, 1.0f, s.score);

        // vsReference: comparar LRA contra referencia
        if (dp.mixLoudnessRange > 0.0f && dp.refLoudnessRange > 0.0f) {
            float lraDiff = dp.mixLoudnessRange - dp.refLoudnessRange;
            s.vsReference = juce::jmap(juce::jlimit(-6.0f, 6.0f, lraDiff), -6.0f, 6.0f, -1.0f, 1.0f);
            s.hasReferenceData = true;
        }

        s.label = qualitativeLabel(s.domain, s.score);
        s.interpretation = autoInterpretation(s);
        s.suggestion = autoSuggestion(s);

        return s;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Compute: Glue Score
    //  Sub-métricas:
    //    ① Spectral Fill: rp.densityScore (crest + LRA + spectral gap)
    //    ② Bus Coherence: correlación promedio entre buses
    //    ③ Masking Freedom: ausencia de pares con masking (invertido)
    // ═══════════════════════════════════════════════════════════════════════════

    RefinementScore RefinementProfile::computeGlueScore(
        const DifferenceProfile& dp,
        CoachEngine& engine) noexcept
    {
        RefinementScore s;
        s.domain = RefinementDomain::Glue;

        // ① Spectral Fill: estimar desde mixRegionEnergy[6]
        //    Qué tan uniforme es la energía a través del espectro
        {
            float maxEnergy = -100.0f, minEnergy = 0.0f;
            int validBands = 0;
            for (int i = 0; i < 6; ++i) {
                if (dp.mixRegionEnergy[i] > -80.0f) {
                    if (validBands == 0) {
                        maxEnergy = minEnergy = dp.mixRegionEnergy[i];
                    } else {
                        if (dp.mixRegionEnergy[i] > maxEnergy) maxEnergy = dp.mixRegionEnergy[i];
                        if (dp.mixRegionEnergy[i] < minEnergy) minEnergy = dp.mixRegionEnergy[i];
                    }
                    ++validBands;
                }
            }
            if (validBands >= 3) {
                float gap = maxEnergy - minEnergy;
                // Menor gap = espectro más lleno = mejor glue
                s.subMetric[0] = 1.0f - juce::jmap(juce::jlimit(3.0f, 18.0f, gap), 3.0f, 18.0f, 0.0f, 1.0f);
            } else {
                s.subMetric[0] = 0.5f;
            }
            s.subMetricLabel[0] = "Spectral Fill";
        }

        // ② Bus Coherence: promedio de correlación de los buses con datos
        {
            auto summaries = engine.getBusSummaries();
            float corrSum = 0.0f;
            int corrCount = 0;

            for (int b = 0; b <= kNumBuses; ++b) {
                if (summaries[b].hasData()) {
                    corrSum += summaries[b].avgCorrelation;
                    ++corrCount;
                }
            }

            if (corrCount > 0) {
                float avgCorr = corrSum / corrCount;
                // Ideal: correlación entre 0.5-0.9 por bus (coherente pero no mono)
                s.subMetric[1] = normalizeToScore(avgCorr, 0.75f, 0.15f, 0.40f);
            } else {
                s.subMetric[1] = 0.5f;
            }
            s.subMetricLabel[1] = "Bus Coherence";
        }

        // ③ Masking Freedom: usar collectAllIssues para contar pares con masking
        {
            auto issues = engine.collectAllIssues();
            int maskingCount = 0;
            for (const auto& issue : issues) {
                if (issue.domain == "masking") ++maskingCount;
            }

            // 0 pares con masking = 1.0 (perfecto), 6+ pares = 0.0 (malo)
            s.subMetric[2] = 1.0f - juce::jmin(1.0f, maskingCount / 6.0f);
            s.subMetricLabel[2] = "Masking Freedom";
        }

        // Score = weighted average
        s.score = s.subMetric[0] * 0.35f + s.subMetric[1] * 0.35f + s.subMetric[2] * 0.30f;
        s.score = juce::jlimit(0.0f, 1.0f, s.score);

        // vsReference: comparar spectralSimilarity como proxy de glue
        if (dp.spectralSimilarity > 0.0f) {
            float diff = dp.spectralSimilarity - 0.5f;
            s.vsReference = juce::jmap(juce::jlimit(-0.4f, 0.4f, diff), -0.4f, 0.4f, -1.0f, 1.0f);
            s.hasReferenceData = true;
        }

        s.label = qualitativeLabel(s.domain, s.score);
        s.interpretation = autoInterpretation(s);
        s.suggestion = autoSuggestion(s);

        return s;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Compute: Emotion Score
    //  Sub-métricas:
    //    ① Spectral Match: dp.spectralSimilarity (0-1)
    //    ② Dynamic Resonance: dp.deltaScore (0-1)
    //    ③ Mid-Range Alignment: inverso de delta en LoMid+HiMid+Presence
    // ═══════════════════════════════════════════════════════════════════════════

    RefinementScore RefinementProfile::computeEmotionScore(const DifferenceProfile& dp) noexcept
    {
        RefinementScore s;
        s.domain = RefinementDomain::Emotion;

        // ① Spectral Match: qué tan cerca está el espectro de la referencia
        s.subMetric[0] = juce::jlimit(0.0f, 1.0f, dp.spectralSimilarity);
        s.subMetricLabel[0] = "Spectral Match";

        // ② Dynamic Resonance: deltaScore como proxy de resonancia dinámica
        s.subMetric[1] = juce::jlimit(0.0f, 1.0f, dp.deltaScore);
        s.subMetricLabel[1] = "Dynamic Resonance";

        // ③ Mid-Range Alignment: región que más define la emoción (voces, guitarras)
        //    deltaRegionEnergy[2..4] = LoMid, HiMid, Presence
        {
            float midDeltaSum = 0.0f;
            int midCount = 0;
            for (int r = 2; r <= 4; ++r) {
                float absDelta = std::abs(dp.deltaRegionEnergy[r]);
                if (absDelta < 90.0f) {
                    midDeltaSum += absDelta;
                    ++midCount;
                }
            }

            if (midCount > 0) {
                float avgMidDelta = midDeltaSum / midCount;
                // 0 dB delta = 1.0 (perfecto), 8+ dB delta = 0.0 (muy diferente)
                s.subMetric[2] = 1.0f - juce::jmin(1.0f, avgMidDelta / 8.0f);
            } else {
                s.subMetric[2] = 0.5f;
            }
            s.subMetricLabel[2] = "Mid-Range Alignment";
        }

        // Score = weighted average
        s.score = s.subMetric[0] * 0.40f + s.subMetric[1] * 0.30f + s.subMetric[2] * 0.30f;
        s.score = juce::jlimit(0.0f, 1.0f, s.score);

        // vsReference: usar spectralSimilarity como indicador principal
        if (dp.spectralSimilarity > 0.0f) {
            float diff = dp.spectralSimilarity - 0.5f;
            s.vsReference = juce::jmap(juce::jlimit(-0.4f, 0.4f, diff), -0.4f, 0.4f, -1.0f, 1.0f);
            s.hasReferenceData = true;
        }

        s.label = qualitativeLabel(s.domain, s.score);
        s.interpretation = autoInterpretation(s);
        s.suggestion = autoSuggestion(s);

        return s;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  RefinementProfile::compute — Punto de entrada principal
    // ═══════════════════════════════════════════════════════════════════════════

    RefinementProfile RefinementProfile::compute(
        const ReferenceProfile& rp,
        const DifferenceProfile& dp,
        const MixScore& ms,
        CoachEngine& engine) noexcept
    {
        RefinementProfile ref;

        ref.timestampUs = juce::Time::getMillisecondCounter() * 1000;
        ref.valid = dp.valid;

        // Guardar fuentes para trazabilidad
        ref.sourceMixScore = static_cast<float>(ms.overall);
        ref.sourceCriticalGaps = dp.criticalGaps;
        ref.sourceWarningGaps = dp.warningGaps;
        ref.sourceHasReference = dp.valid && dp.refIntegratedLUFS > -90.0f;

        // Computar los 5 scores
        ref.depth   = computeDepthScore(dp);
        ref.impact  = computeImpactScore(rp, dp, engine);
        ref.movement = computeMovementScore(dp, engine);
        ref.glue    = computeGlueScore(dp, engine);
        ref.emotion = computeEmotionScore(dp);

        // Score compuesto
        RefinementScore scores[5] = {ref.depth, ref.impact, ref.movement, ref.glue, ref.emotion};
        ref.overallRefinement = computeOverall(scores);

        // Relevancia
        ref.isRelevant = computeIsRelevant(ms, dp);

        return ref;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  toShortText — Resumen breve para debug
    // ═══════════════════════════════════════════════════════════════════════════

    juce::String RefinementProfile::toShortText() const noexcept
    {
        if (!valid) return "[REFINEMENT] (invalid)\n";

        juce::String s;
        s += "[REFINEMENT] ";
        s += "Depth "   + juce::String(depth.score, 2)   + " | ";
        s += "Impact "  + juce::String(impact.score, 2)  + " | ";
        s += "Move "    + juce::String(movement.score, 2) + " | ";
        s += "Glue "    + juce::String(glue.score, 2)    + " | ";
        s += "Emo "     + juce::String(emotion.score, 2)  + " | ";
        s += "Overall " + juce::String(overallRefinement, 2);
        if (isRelevant) s += " [RELEVANT]";
        return s + "\n";
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  toVerboseText — Texto detallado para depuración
    // ═══════════════════════════════════════════════════════════════════════════

    juce::String RefinementProfile::toVerboseText() const noexcept
    {
        if (!valid) return "[REFINEMENT PROFILE] INVALID\n";

        auto formatScore = [](const RefinementScore& s) -> juce::String {
            juce::String out;
            out += "  " + juce::String(s.domainName()) + ": " + juce::String(s.score, 3) + " "
                   + s.label + "  vs ref: " + juce::String(s.vsReference, 2) + "\n";
            for (int i = 0; i < 3; ++i) {
                out += "    \xC2\xB7 " + s.subMetricLabel[i] + ": " + juce::String(s.subMetric[i], 3) + "\n";
            }
            return out;
        };

        juce::String s;
        s += "╔══════════════════════════════════════════════════════════╗\n";
        s += "║          REFINEMENT PROFILE (Verbose)                  ║\n";
        s += "╚══════════════════════════════════════════════════════════╝\n";
        s += "isRelevant: " + juce::String(isRelevant ? "true" : "false");
        s += " | Source: MixScore " + juce::String(sourceMixScore, 0)
             + " critGaps=" + juce::String(sourceCriticalGaps)
             + " warnGaps=" + juce::String(sourceWarningGaps) + "\n\n";

        s += "── Depth ────────────────────────────────────────────\n";
        s += formatScore(depth);
        s += "── Impact ──────────────────────────────────────────\n";
        s += formatScore(impact);
        s += "── Movement ────────────────────────────────────────\n";
        s += formatScore(movement);
        s += "── Glue ────────────────────────────────────────────\n";
        s += formatScore(glue);
        s += "── Emotion ─────────────────────────────────────────\n";
        s += formatScore(emotion);

        s += "\nOverall Refinement: " + juce::String(overallRefinement, 3) + "\n";

        return s;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  toLLMContext — Contexto formateado para inyección en el prompt del LLM
    //  Solo produce texto si isRelevant == true.
    // ═══════════════════════════════════════════════════════════════════════════

    juce::String RefinementProfile::toLLMContext() const
    {
        if (!valid || !isRelevant) return {};

        auto formatScore = [](const RefinementScore& s) -> juce::String {
            juce::String out;
            out += "  " + juce::String(s.domainName()) + ": " + juce::String(s.score, 2)
                   + " " + s.label;
            if (s.hasReferenceData && std::abs(s.vsReference) > 0.05f) {
                out += "  vs ref: " + juce::String(s.vsReference, 2);
            }
            out += "\n";
            for (int i = 0; i < 3; ++i) {
                out += "    - " + s.subMetricLabel[i] + ": " + juce::String(s.subMetric[i], 2) + "\n";
            }
            if (s.interpretation.isNotEmpty())
                out += "    -> " + s.interpretation + "\n";
            if (s.suggestion.isNotEmpty() && s.isActionable())
                out += "    ! " + s.suggestion + "\n";
            return out;
        };

        juce::String ctx;
        ctx += "\n[REFINEMENT PROFILE]\n";
        ctx += "  Source: " + juce::String(static_cast<int>(sourceMixScore)) + "/100 | "
               + juce::String(sourceCriticalGaps) + " critical, " + juce::String(sourceWarningGaps) + " warning\n";
        ctx += "  isRelevant: true (technical mix is solid)\n\n";

        ctx += formatScore(depth);
        ctx += "\n";
        ctx += formatScore(impact);
        ctx += "\n";
        ctx += formatScore(movement);
        ctx += "\n";
        ctx += formatScore(glue);
        ctx += "\n";
        ctx += formatScore(emotion);
        ctx += "\n";

        ctx += "  Overall Refinement: " + juce::String(static_cast<int>(overallRefinement * 100.0f)) + "/100\n";
        ctx += "[END REFINEMENT PROFILE]\n\n";

        return ctx;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  toChatMessage — Mensaje natural para mostrar en el chat del coach
    // ═══════════════════════════════════════════════════════════════════════════

    juce::String RefinementProfile::toChatMessage() const
    {
        if (!valid || !isRelevant) return {};

        juce::String msg;

        // Determinar los dominios más débiles
        struct ScorePair {
            RefinementDomain domain;
            float score;
        };
        ScorePair pairs[5] = {
            {RefinementDomain::Depth,   depth.score},
            {RefinementDomain::Impact,  impact.score},
            {RefinementDomain::Movement, movement.score},
            {RefinementDomain::Glue,    glue.score},
            {RefinementDomain::Emotion, emotion.score}
        };

        // Ordenar por score ascendente (peor primero)
        std::sort(pairs, pairs + 5, [](const ScorePair& a, const ScorePair& b) {
            return a.score < b.score;
        });

        // Mensaje principal
        msg += "[ART] **Refinamiento: " + juce::String(static_cast<int>(overallRefinement * 100.0f)) + "/100**\n";
        msg += "La mezcla est\xC3\xA1" " t\xC3\xA9" "cnicamente s\xC3\xB3" "lida. Aqu\xC3\xAD" " van algunos aspectos art\xC3\xAD" "sticos:\n\n";

        // Mostrar el dominio más débil con sugerencia
        const auto& weakest = pairs[0];
        const RefinementScore* weakScore = nullptr;
        switch (weakest.domain) {
            case RefinementDomain::Depth:   weakScore = &depth;   break;
            case RefinementDomain::Impact:  weakScore = &impact;  break;
            case RefinementDomain::Movement: weakScore = &movement; break;
            case RefinementDomain::Glue:    weakScore = &glue;    break;
            case RefinementDomain::Emotion: weakScore = &emotion; break;
        }

        if (weakScore != nullptr) {
            msg += "\xF0\x9F\x93\x8C **" + juce::String(weakScore->domainName()) + "** "
                   + juce::String(weakScore->label) + " ("
                   + juce::String(static_cast<int>(weakScore->score * 100.0f)) + "/100)\n";
            msg += weakScore->interpretation + "\n";
            if (weakScore->suggestion.isNotEmpty()) {
                msg += "\xF0\x9F\x92\xA1 " + weakScore->suggestion + "\n";
            }
        }

        // Si el segundo más débil también es accionable, mencionarlo brevemente
        if (pairs[1].score < 0.60f) {
            const RefinementScore* secondWeak = nullptr;
            switch (pairs[1].domain) {
                case RefinementDomain::Depth:   secondWeak = &depth;   break;
                case RefinementDomain::Impact:  secondWeak = &impact;  break;
                case RefinementDomain::Movement: secondWeak = &movement; break;
                case RefinementDomain::Glue:    secondWeak = &glue;    break;
                case RefinementDomain::Emotion: secondWeak = &emotion; break;
            }
            if (secondWeak != nullptr) {
                msg += "\n\xF0\x9F\x94\xB9 Tambi\xC3\xA9" "n **" + juce::String(secondWeak->domainName())
                       + "** est\xC3\xA1" " en " + secondWeak->label + " ("
                       + juce::String(static_cast<int>(secondWeak->score * 100.0f)) + "/100). ";
                msg += secondWeak->interpretation + "\n";
            }
        }

        // Celebrar si algún dominio está fuerte
        const auto& strongest = pairs[4];
        if (strongest.score >= 0.80f) {
            const RefinementScore* strongScore = nullptr;
            switch (strongest.domain) {
                case RefinementDomain::Depth:   strongScore = &depth;   break;
                case RefinementDomain::Impact:  strongScore = &impact;  break;
                case RefinementDomain::Movement: strongScore = &movement; break;
                case RefinementDomain::Glue:    strongScore = &glue;    break;
                case RefinementDomain::Emotion: strongScore = &emotion; break;
            }
            if (strongScore != nullptr) {
                msg += "\n\xE2\x9C\xA8 El punto fuerte es **" + juce::String(strongScore->domainName())
                       + "** (" + strongScore->label + "). " + strongScore->interpretation + "\n";
            }
        }

        return msg;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Serialization — JSON (via juce::DynamicObject)
    // ═══════════════════════════════════════════════════════════════════════════

    #define ID(s) juce::Identifier(juce::String(s))

    static void scoreToJson(juce::DynamicObject& obj, const RefinementScore& s, const char* prefix)
    {
        juce::String p(prefix);
        obj.setProperty(ID(p + "_score"),      juce::var((double)s.score));
        obj.setProperty(ID(p + "_label"),      juce::var(s.label));
        obj.setProperty(ID(p + "_vsRef"),      juce::var((double)s.vsReference));
        obj.setProperty(ID(p + "_hasRefData"), juce::var(s.hasReferenceData));
        obj.setProperty(ID(p + "_interp"),     juce::var(s.interpretation));
        obj.setProperty(ID(p + "_suggest"),    juce::var(s.suggestion));

        // Sub-métricas
        juce::Array<juce::var> subArr, subLabelArr;
        for (int i = 0; i < 3; ++i) {
            subArr.add(juce::var((double)s.subMetric[i]));
            subLabelArr.add(juce::var(s.subMetricLabel[i]));
        }
        obj.setProperty(ID(p + "_sub"),       subArr);
        obj.setProperty(ID(p + "_subLabels"), subLabelArr);
    }

    static RefinementScore scoreFromJson(const juce::DynamicObject& obj, const char* prefix, RefinementDomain domain)
    {
        RefinementScore s;
        s.domain = domain;

        auto rd = [&](const juce::String& key, double def) -> double {
            juce::var v = obj.getProperty(ID(key));
            if (!v.isVoid() && !v.isObject() && !v.isArray() && !v.isString()) return (double)v;
            return def;
        };
        auto rs = [&](const juce::String& key, const char* def) -> juce::String {
            juce::var v = obj.getProperty(ID(key));
            return v.isString() ? v.toString() : juce::String(def);
        };

        juce::String p(prefix);
        s.score            = (float)rd(p + "_score", 0.5);
        s.label            = rs(p + "_label", "");
        s.vsReference      = (float)rd(p + "_vsRef", 0.0);
        s.hasReferenceData = (bool)rd(p + "_hasRefData", 0.0);
        s.interpretation   = rs(p + "_interp", "");
        s.suggestion       = rs(p + "_suggest", "");

        // Sub-métricas
        {
            juce::var arrVar = obj.getProperty(ID(p + "_sub"));
            if (arrVar.isArray()) {
                auto* arr = arrVar.getArray();
                for (int i = 0; i < 3 && i < arr->size(); ++i)
                    s.subMetric[i] = (float)(double)(*arr)[i];
            }
            juce::var labelVar = obj.getProperty(ID(p + "_subLabels"));
            if (labelVar.isArray()) {
                auto* arr = labelVar.getArray();
                for (int i = 0; i < 3 && i < arr->size(); ++i)
                    s.subMetricLabel[i] = (*arr)[i].toString();
            }
        }

        return s;
    }

    void RefinementProfile::toJson(juce::DynamicObject& obj) const
    {
        obj.setProperty(ID("valid"),              juce::var(valid));
        obj.setProperty(ID("timestampUs"),         juce::var(static_cast<juce::int64>(timestampUs)));
        obj.setProperty(ID("overallRefinement"),   juce::var((double)overallRefinement));
        obj.setProperty(ID("isRelevant"),          juce::var(isRelevant));
        obj.setProperty(ID("sourceMixScore"),      juce::var((double)sourceMixScore));
        obj.setProperty(ID("sourceCriticalGaps"),  juce::var(sourceCriticalGaps));
        obj.setProperty(ID("sourceWarningGaps"),   juce::var(sourceWarningGaps));
        obj.setProperty(ID("sourceHasReference"),  juce::var(sourceHasReference));

        scoreToJson(obj, depth,   "depth");
        scoreToJson(obj, impact,  "impact");
        scoreToJson(obj, movement, "movement");
        scoreToJson(obj, glue,    "glue");
        scoreToJson(obj, emotion, "emotion");
    }

    RefinementProfile RefinementProfile::fromJson(const juce::DynamicObject& obj)
    {
        RefinementProfile ref;

        auto rd = [&](const char* key, double def) -> double {
            juce::var v = obj.getProperty(ID(key));
            if (!v.isVoid() && !v.isObject() && !v.isArray() && !v.isString()) return (double)v;
            return def;
        };
        auto ri = [&](const char* key, int def) -> int {
            return static_cast<int>(rd(key, static_cast<double>(def)));
        };

        ref.valid              = (bool)rd("valid", 0.0);
        ref.timestampUs        = (int64_t)rd("timestampUs", 0.0);
        ref.overallRefinement  = (float)rd("overallRefinement", 0.0);
        ref.isRelevant         = (bool)rd("isRelevant", 0.0);
        ref.sourceMixScore     = (float)rd("sourceMixScore", 0.0);
        ref.sourceCriticalGaps = ri("sourceCriticalGaps", 0);
        ref.sourceWarningGaps  = ri("sourceWarningGaps", 0);
        ref.sourceHasReference = (bool)rd("sourceHasReference", 0.0);

        ref.depth   = scoreFromJson(obj, "depth",   RefinementDomain::Depth);
        ref.impact  = scoreFromJson(obj, "impact",  RefinementDomain::Impact);
        ref.movement = scoreFromJson(obj, "movement", RefinementDomain::Movement);
        ref.glue    = scoreFromJson(obj, "glue",    RefinementDomain::Glue);
        ref.emotion = scoreFromJson(obj, "emotion", RefinementDomain::Emotion);

        return ref;
    }

    bool RefinementProfile::saveToFile(const juce::File& file) const
    {
        try {
            juce::DynamicObject obj;
            toJson(obj);
            juce::File parent = file.getParentDirectory();
            if (!parent.exists()) parent.createDirectory();
            juce::FileOutputStream fos(file);
            if (fos.openedOk()) {
                fos.setPosition(0);
                fos.truncate();
                juce::JSON::writeToStream(fos, juce::var(&obj));
                return true;
            }
        } catch (...) {}
        return false;
    }

    RefinementProfile RefinementProfile::loadFromFile(const juce::File& file)
    {
        if (!file.existsAsFile()) return RefinementProfile{};
        try {
            juce::FileInputStream fis(file);
            if (fis.openedOk()) {
                auto parsed = juce::JSON::parse(fis);
                if (auto* obj = parsed.getDynamicObject())
                    return fromJson(*obj);
            }
        } catch (...) {}
        return RefinementProfile{};
    }

} // namespace mixcoach
