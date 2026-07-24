#pragma once
#include <juce_core/juce_core.h>

namespace mixcoach {

    // Forward declarations
    struct EvidenceConfig;
    struct TrackProblemData;

    // ═══════════════════════════════════════════════════════════════════════════
    //  EvidenceViewMapper — Mapeo estructural TrackProblemData → EvidenceConfig
    //
    //  Reemplaza el keyword matching de PanelRevealManager por un mapeo
    //  tipado desde los datos del problema detectado. Cada dominio (gain,
    //  tonal, dynamics, spatial, loudness, automation) se mapea a una
    //  vista de evidencia específica (VU, Spectrum, Crest, Vectorscope,
    //  LUFS) con configuración contextual: título, mensaje de sistema,
    //  frecuencia a resaltar, slot a resaltar.
    //
    //  Uso:
    //    TrackProblemData problem = {...};
    //    auto config = EvidenceViewMapper::mapFromProblem(problem);
    //    evidenceHost_->setActiveView(config.view, config.panelTitle);
    //    spectrograph_->setHighlightFreq(config.highlightFreqHz);
    //
    //  Diseñado para integrarse con CoachingNarrativeDirector:
    //    Director.runProblemLoop(problem) → mapper → CoachingEvidenceHost
    // ═══════════════════════════════════════════════════════════════════════════
    struct EvidenceViewMapper
    {
        /** Mapea un TrackProblemData a su EvidenceConfig correspondiente.
            
            El mapping se basa en `domain` y subtipos:
            
            | domain      | EvidenceView | Panel Title   | Highlight       |
            |-------------|-------------|---------------|-----------------|
            | gain        | VU          | Gain Staging  | slotIndex       |
            | tonal       | Spectrum    | EQ            | frequencyHz     |
            | dynamics    | Crest       | Compression   | severity        |
            | spatial     | Vectorscope | Space         | correlation     |
            | loudness    | LUFS        | Master Check  | target LUFS     |
            | automation  | LUFS        | Automation    | section label   |
            | (default)   | None        | —             | —               |
            
            @param problem  Datos del problema detectado (domain, issueType,
                            frequencyHz, slotIndex, severity, etc.)
            @return EvidenceConfig completo con view, título, mensaje de sistema,
                    highlightFreqHz, y highlightSlot.
                    Retorna EvidenceView::None si el dominio no es reconocido. */
        static EvidenceConfig mapFromProblem(const TrackProblemData& problem);

        /** Versión simplificada para mapeo solo por domain string.
            
            Útil cuando no se tiene un TrackProblemData completo pero se
            conoce el dominio (ej. desde ChatMessageSequencer).
            
            @param domain  String del dominio ("gain", "tonal", etc.)
            @return EvidenceConfig básico (sin highlight) o None si no se reconoce. */
        static EvidenceConfig mapFromDomain(const juce::String& domain);
    };

} // namespace mixcoach
