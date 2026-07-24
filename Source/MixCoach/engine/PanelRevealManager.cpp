#include "PanelRevealManager.h"
#include "ProblemAnalyzerMap.h"
#include <algorithm>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Keyword Rules — Mapean palabras clave a paneles
    // ═══════════════════════════════════════════════════════════════════════════
    const PanelRevealManager::KeywordRule PanelRevealManager::kKeywordRules[] = {
        // ─── Reference ──────────────────────────────────────────────────────
        { "referencia",     PanelId::Reference },
        { "reference",      PanelId::Reference },
        { "carga tu",       PanelId::Reference },
        { "arrastra",       PanelId::Reference },
        { "comparar con",   PanelId::Reference },
        // ─── Messengers ─────────────────────────────────────────────────────
        { "messenger",      PanelId::Messengers },
        { "inserta",        PanelId::Messengers },
        { "pista",          PanelId::Messengers },
        { "track",          PanelId::Messengers },
        { "canal",          PanelId::Messengers },
        { "slots",          PanelId::Messengers },
        // ─── MixMap ─────────────────────────────────────────────────────────
        { "mapa",           PanelId::MixMap },
        { "routing",        PanelId::MixMap },
        { "bus",            PanelId::MixMap },
        { "conexión",       PanelId::MixMap },
        // ─── Tools / Analyzers ──────────────────────────────────────────────
        { "analizador",     PanelId::Tools },
        { "espectro",       PanelId::Tools },
        { "frecuencia",     PanelId::Tools },
        { "fase",           PanelId::Tools },
        { "meter",          PanelId::Tools },
        { "lufs",           PanelId::Tools },
        // ─── Session / Progress ────────────────────────────────────────────
        { "progreso",       PanelId::Session },
        { "historial",      PanelId::Session },
        { "sesión",         PanelId::Session },
        { "sesion",         PanelId::Session },
        // ─── Report ────────────────────────────────────────────────────────
        { "reporte",        PanelId::Report },
        { "resumen",        PanelId::Report },
        { "resultados",     PanelId::Report },
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  Track Keyword Rules — Keywords de tracks con dominio sugerido
    // ═══════════════════════════════════════════════════════════════════════════
    const PanelRevealManager::TrackKeywordRule PanelRevealManager::kTrackKeywordRules[] = {
        // Drums — gain domain
        { "kick",   0 }, // gain
        { "snare",  1 }, // tonal
        { "hihat",  1 }, // tonal
        { "hi-hat", 1 }, // tonal
        { "hat",    1 }, // tonal
        { "clap",   2 }, // dynamics
        { "tom",    1 }, // tonal
        { "ride",   1 }, // tonal
        { "crash",  1 }, // tonal
        // Bass — tonal domain
        { "bass",   1 }, // tonal
        { "808",    1 }, // tonal
        // Guitars — tonal
        { "guitar", 1 }, // tonal
        // Keys — tonal
        { "synth",  1 }, // tonal
        { "piano",  1 }, // tonal
        { "keys",   1 }, // tonal
        // Vocals — tonal
        { "voz",    1 }, // tonal
        { "vocal",  1 }, // tonal
        { "voice",  1 }, // tonal
        { "backing", 1 }, // tonal
        // FX — spatial
        { "fx",     3 }, // spatial
        { "reverb", 3 }, // spatial
        { "delay",  3 }, // spatial
        // Master — gain (for master bus reference)
        { "master", 0 }, // gain
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor
    // ═══════════════════════════════════════════════════════════════════════════
    PanelRevealManager::PanelRevealManager()
    {
        // Coach tab is always revealed by default
        revealedPanels_.insert(PanelId::Coach);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  processMessage — Procesa un mensaje del Coach
    // ═══════════════════════════════════════════════════════════════════════════
    PanelRevealResult PanelRevealManager::processMessage(const juce::String& text)
    {
        PanelRevealResult result;
        result.clearPreviousHighlights = true;

        juce::String lowerText = text.toLowerCase();

        // ─── 1. Find panel matches ──────────────────────────────────────────
        auto panelMatches = findPanelMatches(lowerText);
        for (auto panel : panelMatches) {
            if (revealedPanels_.find(panel) == revealedPanels_.end()) {
                result.panelsToReveal.push_back(panel);
            }
        }

        // ─── 2. Find track matches ──────────────────────────────────────────
        result.tracksToHighlight = findTrackMatches(lowerText);

        return result;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  isPanelRevealed
    // ═══════════════════════════════════════════════════════════════════════════
    bool PanelRevealManager::isPanelRevealed(PanelId panel) const noexcept
    {
        return revealedPanels_.find(panel) != revealedPanels_.end();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  markPanelRevealed
    // ═══════════════════════════════════════════════════════════════════════════
    void PanelRevealManager::markPanelRevealed(PanelId panel) noexcept
    {
        if (revealedPanels_.find(panel) == revealedPanels_.end()) {
            revealedPanels_.insert(panel);
            if (onPanelRevealed) {
                onPanelRevealed(panel, true);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getRevealedPanels
    // ═══════════════════════════════════════════════════════════════════════════
    const std::set<PanelId>& PanelRevealManager::getRevealedPanels() const noexcept
    {
        return revealedPanels_;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  clearHighlights
    // ═══════════════════════════════════════════════════════════════════════════
    void PanelRevealManager::clearHighlights() noexcept
    {
        // Future: track highlight state cleanup
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateTrackNames
    // ═══════════════════════════════════════════════════════════════════════════
    void PanelRevealManager::updateTrackNames(const std::vector<juce::String>& trackNames)
    {
        trackNames_ = trackNames;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  resetAllPanels
    // ═══════════════════════════════════════════════════════════════════════════
    void PanelRevealManager::resetAllPanels() noexcept
    {
        revealedPanels_.clear();
        revealedPanels_.insert(PanelId::Coach); // Coach siempre visible
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  findPanelMatches
    // ═══════════════════════════════════════════════════════════════════════════
    std::vector<PanelId> PanelRevealManager::findPanelMatches(const juce::String& lowerText) const
    {
        std::vector<PanelId> matches;

        int numRules = sizeof(kKeywordRules) / sizeof(kKeywordRules[0]);
        for (int i = 0; i < numRules; ++i) {
            const auto& rule = kKeywordRules[i];
            if (lowerText.containsWholeWord(juce::String(rule.keyword))) {
                // Avoid duplicates
                if (std::find(matches.begin(), matches.end(), rule.panel) == matches.end()) {
                    matches.push_back(rule.panel);
                }
            }
        }

        // ─── Also check against known track names for Messengers panel ──────
        // If the coach mentions a track by name, the Messengers panel should
        // already be revealed. But if not, this is a hint to reveal it.
        if (!trackNames_.empty()) {
            for (const auto& trackName : trackNames_) {
                juce::String lowerName = trackName.toLowerCase();
                if (lowerName.isNotEmpty() && lowerText.containsWholeWord(lowerName)) {
                    if (std::find(matches.begin(), matches.end(), PanelId::Messengers)
                        == matches.end()) {
                        matches.push_back(PanelId::Messengers);
                    }
                    break;
                }
            }
        }

        return matches;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  processProblemType — Procesa un problema estructural detectado por el motor
    //  Reemplaza el keyword matching con un lookup determinístico vía ProblemAnalyzerMap.
    // ═══════════════════════════════════════════════════════════════════════════
    PanelRevealResult PanelRevealManager::processProblemType(ProblemType type,
                                                              int slotIndex,
                                                              float highlightHz)
    {
        PanelRevealResult result;
        result.clearPreviousHighlights = true;

        // ─── 1. Obtener el mapeo estructural ────────────────────────────────
        auto mapping = ProblemAnalyzerMap::lookup(type);

        // ─── 2. Revelar el panel correspondiente ────────────────────────────
        if (revealedPanels_.find(mapping.panelId) == revealedPanels_.end()) {
            result.panelsToReveal.push_back(mapping.panelId);
        }

        // ─── 3. Resaltar el track afectado ──────────────────────────────────
        if (slotIndex >= 0) {
            // Determinar dominio según el ProblemType
            int domain = -1;
            switch (type) {
                case ProblemType::Gain:
                case ProblemType::Clipping:      domain = 0; break; // gain
                case ProblemType::Masking:
                case ProblemType::TonalExcess:
                case ProblemType::TonalDeficit:  domain = 1; break; // tonal
                case ProblemType::DynamicsOvercompressed:
                case ProblemType::DynamicsTooDynamic: domain = 2; break; // dynamics
                case ProblemType::Phase:
                case ProblemType::Spatial:
                case ProblemType::Reverb:        domain = 3; break; // spatial
                default:                         domain = -1; break;
            }

            TrackHighlightInfo highlight;
            highlight.slotIndex = slotIndex;
            highlight.domain    = domain;
            highlight.severity  = 0.7f;
            highlight.keyword   = problemTypeToString(type);
            highlight.description = mapping.explanationPhrase;
            result.tracksToHighlight.push_back(highlight);
        }

        return result;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  findTrackMatches
    // ═══════════════════════════════════════════════════════════════════════════
    std::vector<TrackHighlightInfo> PanelRevealManager::findTrackMatches(const juce::String& lowerText) const
    {
        std::vector<TrackHighlightInfo> matches;

        // ─── 1. Check against known track keywords ──────────────────────────
        int numTrackRules = sizeof(kTrackKeywordRules) / sizeof(kTrackKeywordRules[0]);
        for (int i = 0; i < numTrackRules; ++i) {
            const auto& rule = kTrackKeywordRules[i];
            juce::String keyword(rule.keyword);

            if (lowerText.containsWholeWord(keyword)) {
                // Check if we already matched this keyword (avoid duplicates)
                bool alreadyMatched = false;
                for (const auto& existing : matches) {
                    if (existing.keyword == keyword) {
                        alreadyMatched = true;
                        break;
                    }
                }
                if (!alreadyMatched) {
                    TrackHighlightInfo info;
                    info.domain   = rule.domain;
                    info.keyword  = keyword;
                    info.slotIndex = -1; // Will be resolved by NavigationShell/MessengerList
                    matches.push_back(info);
                }
            }
        }

        // ─── 2. Check against known track names (from SlotRegistry) ─────────
        for (const auto& trackName : trackNames_) {
            juce::String lowerName = trackName.toLowerCase();
            if (lowerName.isNotEmpty() && lowerText.containsWholeWord(lowerName)) {
                bool alreadyMatched = false;
                for (const auto& existing : matches) {
                    if (existing.keyword == lowerName) {
                        alreadyMatched = true;
                        break;
                    }
                }
                if (!alreadyMatched) {
                    TrackHighlightInfo info;
                    info.domain    = -1; // Unknown domain (general highlight)
                    info.keyword   = lowerName;
                    info.slotIndex = -1; // Will be resolved by slot name match
                    matches.push_back(info);
                }
            }
        }

        return matches;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  validateReveal — Progressive Revelation Guard
    //
    //  Reglas: ningún analizador se revela sin una decisión que lo justifique.
    //  Setup: solo Coach, Reference, Messengers (onboarding)
    //  Coaching: Coach + Tools (analyzer de fase) + Session
    //  Expert: todos los paneles (usuario explícito)
    //  Report: solo Coach + Report
    // ═══════════════════════════════════════════════════════════════════════════
    bool PanelRevealManager::validateReveal(AnalysisScope scope, PanelId panel) const noexcept
    {
        // Siempre se puede revelar Coach
        if (panel == PanelId::Coach)
            return true;

        switch (scope) {
            case AnalysisScope::Setup:
                // En Setup se revelan Reference, Messengers y MixMap (flujo onboarding)
                return (panel == PanelId::Reference || panel == PanelId::Messengers
                     || panel == PanelId::MixMap);

            case AnalysisScope::Coaching:
                // En Coaching se revelan Tools (analyzer de fase), Session, Reference, Messengers
                return (panel == PanelId::Tools || panel == PanelId::Session
                     || panel == PanelId::Reference || panel == PanelId::Messengers);

            case AnalysisScope::Expert:
                // Expert: todos los paneles permitidos
                return true;

            case AnalysisScope::Report:
                // Report: solo Coach y Report
                return (panel == PanelId::Report);
        }

        return false;
    }

} // namespace mixcoach
