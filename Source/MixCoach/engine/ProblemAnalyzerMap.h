#pragma once
#include "CoachingNarrativeTypes.h"
#include "EvidenceViewMapper.h"
#include "PluginSuggestion.h"
#include "PanelRevealManager.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  AnalyzerMapping — Mapeo de un ProblemType a su panel + vista
    // ═══════════════════════════════════════════════════════════════════════════
    struct AnalyzerMapping
    {
        PanelId panelId = PanelId::Tools;  // Panel a revelar
        const char* viewId = "vu";         // ID de la vista analyzer
        const char* explanationPhrase = ""; // Frase de explicación para highlight
        float highlightFreq = 0.0f;         // Frecuencia a resaltar (0 = sin highlight)
        const char* highlightLabel = "";   // Etiqueta para el highlight

        bool hasHighlight() const noexcept { return highlightFreq > 0.0f && highlightLabel[0] != '\0'; }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  ProblemAnalyzerMap — Legacy wrapper (backward compat)
    //
    //  Implementa el mapeo ProblemType → PanelId directamente.
    //  Se mantiene para no romper referencias existentes en PanelRevealManager.
    //
    //  NUEVO CÓDIGO: usa EvidenceViewMapper.mapFromDomain() directamente.
    // ═══════════════════════════════════════════════════════════════════════════
    class ProblemAnalyzerMap
    {
    public:
        /** Retorna el mapeo completo para un ProblemType. */
        static AnalyzerMapping lookup(ProblemType type) noexcept
        {
            AnalyzerMapping mapping;

            switch (type) {
                case ProblemType::Gain:
                case ProblemType::Clipping:
                    mapping.panelId = PanelId::Tools;
                    mapping.viewId = "vu";
                    mapping.explanationPhrase = "Revisando niveles de ganancia...";
                    mapping.highlightLabel = "nivel";
                    break;

                case ProblemType::Masking:
                case ProblemType::TonalExcess:
                case ProblemType::TonalDeficit:
                    mapping.panelId = PanelId::Tools;
                    mapping.viewId = "spectrum";
                    mapping.explanationPhrase = "Analizando equilibrio espectral...";
                    mapping.highlightFreq = 2500.0f; // frecuencia media típica
                    mapping.highlightLabel = "rango conflictivo";
                    break;

                case ProblemType::DynamicsOvercompressed:
                case ProblemType::DynamicsTooDynamic:
                    mapping.panelId = PanelId::Tools;
                    mapping.viewId = "crest";
                    mapping.explanationPhrase = "Midiendo rango dinámico...";
                    mapping.highlightLabel = "crest factor";
                    break;

                case ProblemType::Phase:
                    mapping.panelId = PanelId::Tools;
                    mapping.viewId = "vectorscope";
                    mapping.explanationPhrase = "Verificando correlación de fase...";
                    mapping.highlightLabel = "correlación";
                    break;

                case ProblemType::Spatial:
                case ProblemType::Reverb:
                    mapping.panelId = PanelId::Tools;
                    mapping.viewId = "vectorscope";
                    mapping.explanationPhrase = "Analizando imagen estéreo...";
                    mapping.highlightLabel = "ancho estéreo";
                    break;

                case ProblemType::Saturation:
                    mapping.panelId = PanelId::Tools;
                    mapping.viewId = "spectrum";
                    mapping.explanationPhrase = "Revisando saturación armónica...";
                    mapping.highlightFreq = 5000.0f;
                    mapping.highlightLabel = "armónicos";
                    break;

                case ProblemType::Limiting:
                    mapping.panelId = PanelId::Tools;
                    mapping.viewId = "lufs";
                    mapping.explanationPhrase = "Midiendo nivel de sonoridad...";
                    mapping.highlightLabel = "LUFS target";
                    break;

                default:
                    mapping.panelId = PanelId::Tools;
                    mapping.viewId = "vu";
                    mapping.explanationPhrase = "Revisando señal...";
                    break;
            }

            return mapping;
        }

        /** Overload: retorna el viewId como string. */
        static const char* lookupViewId(ProblemType type) noexcept
        {
            return lookup(type).viewId;
        }

        /** Overload: retorna el panel a abrir. */
        static PanelId lookupPanel(ProblemType type) noexcept
        {
            return lookup(type).panelId;
        }

        /** Construye un mensaje de evidencia para el chat usando EvidenceViewMapper. */
        static juce::String buildEvidenceMessage(ProblemType type,
                                                  const juce::String& trackName,
                                                  float highlightHz = 0.0f)
        {
            // Convertir ProblemType a domain string para EvidenceViewMapper
            juce::String domain;
            switch (type) {
                case ProblemType::Gain:
                case ProblemType::Clipping:
                    domain = "gain"; break;
                case ProblemType::Masking:
                case ProblemType::TonalExcess:
                case ProblemType::TonalDeficit:
                case ProblemType::Saturation:
                    domain = "tonal"; break;
                case ProblemType::DynamicsOvercompressed:
                case ProblemType::DynamicsTooDynamic:
                    domain = "dynamics"; break;
                case ProblemType::Phase:
                case ProblemType::Spatial:
                case ProblemType::Reverb:
                    domain = "spatial"; break;
                case ProblemType::Limiting:
                    domain = "loudness"; break;
                default:
                    domain = "gain"; break;
            }

            // Usar EvidenceViewMapper para obtener el mensaje contextual
            auto config = EvidenceViewMapper::mapFromDomain(domain);
            if (trackName.isNotEmpty() && highlightHz > 0.0f)
                config.systemMessage += " (" + trackName + " @ " + juce::String(highlightHz / 1000.0f, 1) + "kHz)";
            return config.systemMessage;
        }
    };

} // namespace mixcoach
