#include "EvidenceViewMapper.h"
#include "../UI/TrackProblemCard.h"
#include "CoachingNarrativeTypes.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Domain → EvidenceConfig mapping table
    //
    //  Cada dominio musical se mapea a:
    //    - Una vista de analyzer (VU, Spectrum, Crest, Vectorscope, LUFS)
    //    - Un título contextual para el panel derecho
    //    - Un mensaje de sistema que el Coach muestra al abrir la evidencia
    //    - Frecuencia/slot a resaltar (si aplica)
    //
    //  Los threshold de activación se basan en issueType:
    //    - "CLIPPING", "EXCESS", "PHASE_ISSUE" → critical → mensaje fuerte
    //    - "NEAR_TARGET", "NEAR_LIMIT"        → warning → mensaje suave
    //    - "ON_TARGET", "OPTIMAL"             → info   → mensaje de confirmación
    // ═══════════════════════════════════════════════════════════════════════════

    // ─── Threshold helpers ─────────────────────────────────────────────────
    namespace {

        /** Determina si el issueType es crítico (rojo) o warning (amarillo). */
        bool isCriticalIssue(const juce::String& issueType) noexcept
        {
            return issueType == "CLIPPING"
                || issueType == "EXCESS"
                || issueType == "PHASE_ISSUE"
                || issueType == "OFF_TARGET"
                || issueType == "SOBRECOMPRIMIDO"
                || issueType == "HARD_CLIPPING";
        }

        /** Retorna el mensaje de sistema contextual para abrir la evidencia. */
        juce::String buildSystemMessage(const juce::String& domain,
                                         const juce::String& issueType,
                                         const juce::String& trackName,
                                         float frequencyHz) noexcept
        {
            bool critical = isCriticalIssue(issueType);

            if (domain == "gain")
            {
                if (critical)
                    return "\\xF0\\x9F\\x94\\x8D **Alerta de clipping** en "
                           + trackName + ". Abriendo medidores de ganancia...";
                return "\\xF0\\x9F\\x93\\x8A Revisando niveles de "
                       + trackName + "...";
            }

            if (domain == "tonal")
            {
                if (frequencyHz > 0.0f)
                {
                    juce::String freqStr = juce::String(frequencyHz / 1000.0f, 1) + "kHz";
                    if (critical)
                        return "\\xF0\\x9F\\x94\\x8D **Exceso en " + freqStr
                               + "** en " + trackName + ". Abriendo espectro...";
                    return "\\xF0\\x9F\\x93\\xA1 Analizando equilibrio tonal de "
                           + trackName + " en " + freqStr + "...";
                }
                return "\\xF0\\x9F\\x93\\xA1 Revisando espectro de "
                       + trackName + "...";
            }

            if (domain == "dynamics")
            {
                if (critical)
                    return "\\xF0\\x9F\\x94\\x8D **Rango din\\xC3\\xA1mico comprimido** en "
                           + trackName + ". Abriendo medidor de crest...";
                return "\\xF0\\x9F\\x93\\x88 Analizando din\\xC3\\xA1mica de "
                       + trackName + "...";
            }

            if (domain == "spatial")
            {
                if (critical)
                    return "\\xF0\\x9F\\x94\\x8D **Problema de fase** en "
                           + trackName + ". Abriendo vectorscope...";
                return "\\xF0\\x9F\\x94\\x8A Revisando imagen est\\xC3\\xA9reo de "
                       + trackName + "...";
            }

            if (domain == "loudness")
            {
                return "\\xF0\\x9F\\x93\\x8A Midiendo sonoridad general vs referencia...";
            }

            if (domain == "automation")
            {
                return "\\xF0\\x9F\\x94\\x84 Revisando variaci\\xC3\\xB3n de niveles por secci\\xC3\\xB3n...";
            }

            return "\\xF0\\x9F\\x94\\x8D Abriendo analizadores...";
        }

        /** Retorna el título del panel según dominio y subtipo. */
        juce::String getPanelTitle(const juce::String& domain,
                                    const juce::String& issueType) noexcept
        {
            if (domain == "gain")
            {
                if (issueType == "CLIPPING")
                    return "Gain Staging \\xE2\\x80\\x94 Alerta de clipping";
                if (issueType == "LOW_SIGNAL")
                    return "Gain Staging \\xE2\\x80\\x94 Se\\xC3\\xB1al baja";
                return "Gain Staging \\xE2\\x80\\x94 Niveles";
            }

            if (domain == "tonal")
                return "EQ \\xE2\\x80\\x94 Espectro";

            if (domain == "dynamics")
            {
                if (issueType == "HIGH_CREST" || issueType == "SOBRECOMPRIMIDO")
                    return "Compresi\\xC3\\xB3n \\xE2\\x80\\x94 Crest alto";
                if (issueType == "LOW_CREST")
                    return "Compresi\\xC3\\xB3n \\xE2\\x80\\x94 Crest bajo";
                return "Compresi\\xC3\\xB3n \\xE2\\x80\\x94 Din\\xC3\\xA1mica";
            }

            if (domain == "spatial")
                return "Espacio \\xE2\\x80\\x94 Correlaci\\xC3\\xB3n";

            if (domain == "loudness")
                return "Master Check \\xE2\\x80\\x94 LUFS";

            if (domain == "automation")
                return "Automatizaci\\xC3\\xB3n \\xE2\\x80\\x94 L\\xC3\\xADnea temporal";

            return "Evidencia";
        }

    } // anonymous namespace

    // ═══════════════════════════════════════════════════════════════════════════
    //  EvidenceViewMapper::mapFromProblem
    // ═══════════════════════════════════════════════════════════════════════════
    EvidenceConfig EvidenceViewMapper::mapFromProblem(const TrackProblemData& problem)
    {
        EvidenceConfig config;

        // ─── Domain: gain → VU meter ─────────────────────────────────────
        if (problem.domain == "gain")
        {
            config.view           = EvidenceView::VU;
            config.panelTitle     = getPanelTitle(problem.domain, problem.issueType);
            config.systemMessage  = buildSystemMessage(problem.domain, problem.issueType,
                                                        problem.trackName, problem.frequencyHz);
            config.highlightSlot  = problem.slotIndex;  // Resaltar pista con clipping
            config.highlightFreqHz = 0.0f;
            return config;
        }

        // ─── Domain: tonal → Spectrum con highlight frequency ────────────
        if (problem.domain == "tonal")
        {
            config.view           = EvidenceView::Spectrum;
            config.panelTitle     = getPanelTitle(problem.domain, problem.issueType);
            config.systemMessage  = buildSystemMessage(problem.domain, problem.issueType,
                                                        problem.trackName, problem.frequencyHz);
            config.highlightFreqHz = problem.frequencyHz;  // Línea roja en el spectrograph
            config.highlightSlot  = problem.slotIndex;
            return config;
        }

        // ─── Domain: dynamics → Crest gauge ─────────────────────────────
        if (problem.domain == "dynamics")
        {
            config.view           = EvidenceView::Crest;
            config.panelTitle     = getPanelTitle(problem.domain, problem.issueType);
            config.systemMessage  = buildSystemMessage(problem.domain, problem.issueType,
                                                        problem.trackName, problem.frequencyHz);
            config.highlightFreqHz = 0.0f;
            config.highlightSlot  = problem.slotIndex;
            return config;
        }

        // ─── Domain: spatial → Vectorscope ──────────────────────────────
        if (problem.domain == "spatial")
        {
            config.view           = EvidenceView::Vectorscope;
            config.panelTitle     = getPanelTitle(problem.domain, problem.issueType);
            config.systemMessage  = buildSystemMessage(problem.domain, problem.issueType,
                                                        problem.trackName, problem.frequencyHz);
            config.highlightFreqHz = 0.0f;
            config.highlightSlot  = problem.slotIndex;
            return config;
        }

        // ─── Domain: loudness → LUFS meter ──────────────────────────────
        if (problem.domain == "loudness")
        {
            config.view           = EvidenceView::LUFS;
            config.panelTitle     = getPanelTitle(problem.domain, problem.issueType);
            config.systemMessage  = buildSystemMessage(problem.domain, problem.issueType,
                                                        problem.trackName, problem.frequencyHz);
            config.highlightFreqHz = 0.0f;
            config.highlightSlot  = problem.slotIndex;
            return config;
        }

        // ─── Domain: automation → LUFS timeline ─────────────────────────
        if (problem.domain == "automation")
        {
            config.view           = EvidenceView::LUFS;
            config.panelTitle     = getPanelTitle(problem.domain, problem.issueType);
            config.systemMessage  = buildSystemMessage(problem.domain, problem.issueType,
                                                        problem.trackName, problem.frequencyHz);
            config.highlightFreqHz = 0.0f;
            config.highlightSlot  = problem.slotIndex;
            return config;
        }

        // ─── Domain: phase → Vectorscope (como spatial) ─────────────────
        if (problem.domain == "phase")
        {
            config.view           = EvidenceView::Vectorscope;
            config.panelTitle     = "Fase \\xE2\\x80\\x94 Correlaci\\xC3\\xB3n";
            config.systemMessage  = buildSystemMessage(problem.domain, problem.issueType,
                                                        problem.trackName, problem.frequencyHz);
            config.highlightFreqHz = 0.0f;
            config.highlightSlot  = problem.slotIndex;
            return config;
        }

        // ─── Fallback: dominio no reconocido ─────────────────────────────
        config.view           = EvidenceView::None;
        config.panelTitle     = "Evidencia";
        config.systemMessage  = "\\xF0\\x9F\\x94\\x8D Abriendo analizadores...";
        config.highlightFreqHz = 0.0f;
        config.highlightSlot  = -1;
        return config;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  EvidenceViewMapper::mapFromDomain
    // ═══════════════════════════════════════════════════════════════════════════
    EvidenceConfig EvidenceViewMapper::mapFromDomain(const juce::String& domain)
    {
        EvidenceConfig config;

        if (domain == "gain")
        {
            config.view          = EvidenceView::VU;
            config.panelTitle    = "Gain Staging";
            config.systemMessage = "\\xF0\\x9F\\x93\\x8A Revisando niveles de ganancia...";
        }
        else if (domain == "tonal")
        {
            config.view          = EvidenceView::Spectrum;
            config.panelTitle    = "EQ \\xE2\\x80\\x94 Espectro";
            config.systemMessage = "\\xF0\\x9F\\x93\\xA1 Analizando equilibrio tonal...";
        }
        else if (domain == "dynamics")
        {
            config.view          = EvidenceView::Crest;
            config.panelTitle    = "Compresi\\xC3\\xB3n \\xE2\\x80\\x94 Din\\xC3\\xA1mica";
            config.systemMessage = "\\xF0\\x9F\\x93\\x88 Revisando rango din\\xC3\\xA1mico...";
        }
        else if (domain == "spatial" || domain == "phase")
        {
            config.view          = EvidenceView::Vectorscope;
            config.panelTitle    = domain == "phase"
                                        ? "Fase \\xE2\\x80\\x94 Correlaci\\xC3\\xB3n"
                                        : "Espacio \\xE2\\x80\\x94 Imagen est\\xC3\\xA9reo";
            config.systemMessage = "\\xF0\\x9F\\x94\\x8A Revisando correlaci\\xC3\\xB3n de fase...";
        }
        else if (domain == "loudness")
        {
            config.view          = EvidenceView::LUFS;
            config.panelTitle    = "Master Check \\xE2\\x80\\x94 LUFS";
            config.systemMessage = "\\xF0\\x9F\\x93\\x8A Midiendo sonoridad general...";
        }
        else if (domain == "automation")
        {
            config.view          = EvidenceView::LUFS;
            config.panelTitle    = "Automatizaci\\xC3\\xB3n \\xE2\\x80\\x94 L\\xC3\\xADnea temporal";
            config.systemMessage = "\\xF0\\x9F\\x94\\x84 Revisando variaci\\xC3\\xB3n de niveles...";
        }
        else
        {
            config.view          = EvidenceView::None;
            config.panelTitle    = "Evidencia";
            config.systemMessage = "\\xF0\\x9F\\x94\\x8D Abriendo analizadores...";
        }

        config.highlightFreqHz = 0.0f;
        config.highlightSlot  = -1;
        return config;
    }

} // namespace mixcoach
