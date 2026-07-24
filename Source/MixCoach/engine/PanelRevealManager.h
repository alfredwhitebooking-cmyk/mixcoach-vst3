#pragma once
#include <juce_core/juce_core.h>
#include <set>
#include <vector>
#include <functional>
#include "PluginSuggestion.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  AnalysisScope — Contexto de revelación progresiva
    //  Cada scope define qué paneles pueden revelarse automáticamente.
    //  Un panel nunca se revela sin una decisión que lo justifique.
    // ═══════════════════════════════════════════════════════════════════════════
    enum class AnalysisScope : uint8_t
    {
        Setup,    // Onboarding: solo Coach, Reference, Messengers
        Coaching, // Loop narrativo: Coach + analyzer de fase + Session
        Expert,   // Usuario explícito: todos los paneles permitidos
        Report,   // Reporte final: solo Coach + Report
    };

    inline const char* analysisScopeLabel(AnalysisScope scope) noexcept
    {
        switch (scope) {
            case AnalysisScope::Setup:    return "Setup";
            case AnalysisScope::Coaching: return "Coaching";
            case AnalysisScope::Expert:   return "Expert";
            case AnalysisScope::Report:   return "Report";
            default:                      return "Unknown";
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  PanelId — Identificadores de paneles que pueden ser revelados
    // ═══════════════════════════════════════════════════════════════════════════
    enum class PanelId : uint8_t
    {
        Coach,      // Always visible (default)
        Reference,  // Reference panel (DropZone + file list)
        Messengers, // Messenger list (track list with meters)
        MixMap,     // Session map / routing tree
        Tools,      // Analyzers (Spectrum, Phase, VU, etc.)
        Session,    // Progress / timeline
        Report,     // End of session report
        Count
    };

    /** Retorna un label textual para debug. */
    inline const char* panelIdLabel(PanelId id) noexcept
    {
        switch (id) {
            case PanelId::Coach:      return "Coach";
            case PanelId::Reference:  return "Reference";
            case PanelId::Messengers: return "Messengers";
            case PanelId::MixMap:     return "MixMap";
            case PanelId::Tools:      return "Tools";
            case PanelId::Session:    return "Session";
            case PanelId::Report:     return "Report";
            default:                  return "Unknown";
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  TrackHighlightInfo — Información para resaltar un track
    // ═══════════════════════════════════════════════════════════════════════════
    struct TrackHighlightInfo
    {
        int slotIndex = -1;       // Which slot to highlight (-1 = unresolved)
        int domain    = -1;       // 0=gain, 1=tonal, 2=dynamics, 3=spatial, 4=masking
        float severity = 0.0f;    // 0.0-1.0
        juce::String keyword;     // The keyword that triggered this highlight
        juce::String description; // Short description for tooltip
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  PanelRevealResult — Resultado de procesar un mensaje del Coach
    // ═══════════════════════════════════════════════════════════════════════════
    struct PanelRevealResult
    {
        std::vector<PanelId> panelsToReveal;           // Paneles que se revelan por PRIMERA vez
        std::vector<TrackHighlightInfo> tracksToHighlight; // Tracks a resaltar
        bool clearPreviousHighlights = true;           // Si debe limpiar highlights anteriores
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  PanelRevealManager — Chat-Commanded UI Manager
    //
    //  Procesa los mensajes del Coach y determina qué paneles revelar
    //  y qué tracks resaltar mediante keyword matching.
    //
    //  Uso:
    //    auto result = manager.processMessage(textoDelCoach);
    //    for (auto& panel : result.panelsToReveal)
    //        navigationShell->revealPanel(panel);
    //    for (auto& track : result.tracksToHighlight)
    //        messengerList->highlightTrack(track);
    // ═══════════════════════════════════════════════════════════════════════════
    class PanelRevealManager
    {
    public:
        PanelRevealManager();

        /** Procesa un mensaje del Coach y retorna qué revelar/resaltar.
            @param text  El mensaje completo del Coach (en minúsculas para matching)
            @return PanelRevealResult con paneles a revelar y tracks a resaltar */
        PanelRevealResult processMessage(const juce::String& text);

        /** Consulta si un panel ya fue revelado. */
        [[nodiscard]] bool isPanelRevealed(PanelId panel) const noexcept;

        /** Marca un panel como revelado (llamado por NavigationShell). */
        void markPanelRevealed(PanelId panel) noexcept;

        /** Retorna el set de paneles ya revelados. */
        [[nodiscard]] const std::set<PanelId>& getRevealedPanels() const noexcept;

        /** Procesa un problema detectado estructuralmente (reemplaza keyword matching).
            Dado un ProblemType y opcionalmente una frecuencia y track,
            determina qué panel revelar, qué vista de analyzer abrir,
            y qué track resaltar.
            @param type         Tipo de problema detectado por el motor
            @param slotIndex    Slot de la pista afectada (-1 si no aplica)
            @param highlightHz  Frecuencia específica a resaltar (0 = usar default del mapping)
            @return Resultado con panels a revelar y tracks a resaltar */
        PanelRevealResult processProblemType(ProblemType type,
                                              int slotIndex = -1,
                                              float highlightHz = 0.0f);

        /** Limpia el estado de highlights (llamar antes de enviar nuevo mensaje). */
        void clearHighlights() noexcept;

        /** Actualiza la cache de nombres de tracks desde SlotRegistry.
            Permite hacer match por nombre de track además de keywords. */
        void updateTrackNames(const std::vector<juce::String>& trackNames);

        /** Callback cuando un panel se revela por PRIMERA vez.
            @param panel  El panel revelado
            @param isFirstReveal  Siempre true aquí (solo se dispara la primera vez) */
        std::function<void(PanelId panel, bool isFirstReveal)> onPanelRevealed;

        /** Resetea todos los paneles (para debug/testing). */
        void resetAllPanels() noexcept;

        // ═══ Progressive Revelation Guard ═══════════════════════════════════

        /** Valida si un panel puede revelarse en el scope actual.
            @param scope    Contexto actual (Setup, Coaching, Expert, Report)
            @param panel    Panel a revelar
            @return true si el panel puede revelarse en este scope */
        [[nodiscard]] bool validateReveal(AnalysisScope scope, PanelId panel) const noexcept;

        /** Retorna el scope actual. */
        [[nodiscard]] AnalysisScope getCurrentScope() const noexcept { return currentScope_; }

        /** Cambia el scope actual. Llamado por NavigationShell al cambiar de estado. */
        void setCurrentScope(AnalysisScope scope) noexcept { currentScope_ = scope; }

    private:
        std::set<PanelId> revealedPanels_;
        std::vector<juce::String> trackNames_; // Cache de nombres de tracks
        AnalysisScope currentScope_ = AnalysisScope::Setup; // Siempre empezar en Setup

        // ─── Keyword → Panel mapping rules ─────────────────────────────────
        struct KeywordRule
        {
            const char* keyword;
            PanelId panel;
        };

        static const KeywordRule kKeywordRules[];
        // ─── Track keyword → domain mapping ────────────────────────────────
        struct TrackKeywordRule
        {
            const char* keyword;
            int domain; // 0=gain, 1=tonal, 2=dynamics, 3=spatial, 4=masking
        };

        static const TrackKeywordRule kTrackKeywordRules[];
        /** Encuentra paneles que matchean keywords en el texto. */
        std::vector<PanelId> findPanelMatches(const juce::String& lowerText) const;

        /** Encuentra menciones de tracks en el texto. */
        std::vector<TrackHighlightInfo> findTrackMatches(const juce::String& lowerText) const;
    };

} // namespace mixcoach
