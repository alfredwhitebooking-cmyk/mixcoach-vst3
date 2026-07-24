#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"

namespace mixcoach {
    class PluginSuggestionsProvider;


    // ═══════════════════════════════════════════════════════════════════════════
    //  TrackPluginSuggestion — Sugerencia de plugin para un problema
    // ═══════════════════════════════════════════════════════════════════════════
    struct TrackPluginSuggestion
    {
        enum class Tier : uint8_t { Native, Free, Premium, UserHas };

        Tier tier = Tier::Native;
        juce::String pluginName;      // "Fruity Balance"
        juce::String actionText;      // "-1.5 dB"
        juce::String extraInfo;       // URL o descripción opcional

        [[nodiscard]] static const char* tierIcon(Tier t) noexcept
        {
            switch (t) {
                case Tier::Native:  return "[COACH]";  // 🎛
                case Tier::Free:    return "\xF0\x9F\x9F\xA2";  // 🟢
                case Tier::Premium: return "\xE2\xAD\x90";      // ⭐
                case Tier::UserHas: return "[BOLT]";      // ⚡
                default:            return "";
            }
        }
        [[nodiscard]] static const char* tierLabel(Tier t) noexcept
        {
            switch (t) {
                case Tier::Native:  return "Nativo";
                case Tier::Free:    return "Gratis";
                case Tier::Premium: return "Profesional";
                case Tier::UserHas: return "Ya tienes";
                default:            return "";
            }
        }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  TrackProblemData — Datos de un problema detectado en una pista
    // ═══════════════════════════════════════════════════════════════════════════
    struct TrackProblemData
    {
        int slotIndex = -1;
        juce::String trackName;
        juce::String roleName;         // "Kick", "Vocal", etc.
        juce::String problemType;      // "Exceso en 2.5kHz", "Demasiado subgrave"
        float severity = 0.5f;         // 0.0=info, 0.5=warning, 1.0=critical
        float priorityScore = 0.0f;    // 0-1 from MixPriorityEngine
        juce::String maskingInfo;      // "Con Kick, Bass"

        // Sugerencias de plugin (3 tiers) para este problema
        std::vector<TrackPluginSuggestion> pluginSuggestions;

        // ─── Domain y subtipo original (para reconstruir ProblemType en clicks) ───
        juce::String domain;        // "gain", "tonal", "dynamics", etc.
        juce::String issueType;     // "CLIPPING", "EXCESS", etc.
        float delta = 0.0f;         // dB sugerido (para interpolar en buildMessageForProblem)
        float frequencyHz = 0.0f;   // Hz sugerido
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  TrackProblemGroup — Grupo de problemas por familia/bus
    // ═══════════════════════════════════════════════════════════════════════════
    struct TrackProblemGroup
    {
        juce::String groupName;             // "Batería", "Vocales", etc.
        juce::String icon;                  // Emoji UTF-8
        juce::Colour colour;                // Color del grupo
        std::vector<TrackProblemData> tracks;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  TrackProblemCard — Componente que renderiza una tarjeta de problemas
    //  agrupados por familia. Se usa como child de MixCoachPanel o como
    //  render helper dentro de ChatMessagesComponent.
    //
    //  Layout:
    //    [Header de grupo: icono + "Grupo: Batería" + "● 3 pistas"]
    //    [Por cada track: icono + nombre + badge + descripción + dots]
    //      [Al expandir "Ver más": 3 pills de sugerencias de plugins]
    //    [Footer: tip del coach + botón "Ir a Tools"]
    // ═══════════════════════════════════════════════════════════════════════════
    class TrackProblemCard : public juce::Component
    {
    public:
        TrackProblemCard();

        void paint(juce::Graphics& g) override;
        void resized() override;

        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;
        void mouseDown(const juce::MouseEvent& e) override;

        /** Establece los datos del grupo de problemas. */
        void setGroup(const TrackProblemGroup& group);

        /** Limpia el card. */
        void clear();

        bool hasData() const noexcept { return !group_.tracks.empty(); }

        /** Callback para abrir Tools tab. */
        std::function<void()> onGoToTools;

        /** Callback para solicitar ayuda en una pista específica. */
        std::function<void(int slotIndex)> onRequestHelp;

        /** Callback cuando se hace clic en una sugerencia de plugin.
            @param trackIndex    Índice dentro del grupo (0..N)
            @param suggestionIndex  Índice dentro de pluginSuggestions del track
            @param track         Referencia al TrackProblemData del track clickeado */
        std::function<void(int trackIndex, int suggestionIndex, const TrackProblemData& track)> onPluginClicked;

        // ═══ Acciones Directas — Botones en cada TrackProblemCard ═══════════
        /** El usuario hizo clic en "✅ Aplicado".
            @param slotIndex   Índice de la pista donde se aplicó la corrección
            @param domain      Dominio: "gain", "tonal", "dynamics" */
        std::function<void(int slotIndex, const juce::String& domain)> onActionApplied;

        /** El usuario hizo clic en "⏭️ Omitir".
            @param slotIndex   Índice de la pista omitida */
        std::function<void(int slotIndex)> onActionSkipped;

        /** El usuario hizo clic en "🔍 Explícame".
            @param slotIndex   Índice de la pista sobre la que quiere más info
            @param problemType Tipo de problema a explicar */
        std::function<void(int slotIndex, const juce::String& problemType)> onActionExplain;

        /** Altura recomendada según los datos actuales. */
        int getPreferredHeight() const;

    private:
        TrackProblemGroup group_;
        int hoveredTrack_ = -1;
        bool hoveringSuggestion_ = false;
        int expandedTrack_ = -1;  // Track whose plugin suggestions are shown

        // ═══ updateLayout() centraliza todo el layout ═══
        // Antes: layout duplicado entre paint() y resized().
        // Ahora: updateLayout() calcula TODOS los bounds en un solo lugar,
        // paint() solo lee de bounds cacheados (sin removeFromLeft/removeFromTop).
        // Esto elimina la posibilidad de desync entre hit-test y render.
        void updateLayout();

        // Bounds cacheados (calculados en updateLayout, usados en paint + mouse*)
        juce::Rectangle<float> headerIconBounds_;
        juce::Rectangle<float> headerNameBounds_;
        juce::Rectangle<float> headerBadgeBounds_;
        std::vector<juce::Rectangle<float>> roleIconBounds_;
        std::vector<juce::Rectangle<float>> trackNameBounds_;
        std::vector<juce::Rectangle<float>> badgeAreaBounds_;
        std::vector<juce::Rectangle<float>> dotsAreaBounds_;
        juce::Rectangle<float> footerTextBounds_;
        juce::Rectangle<float> goToToolsBounds_;
        std::vector<juce::Rectangle<float>> trackBounds_;
        std::vector<juce::Rectangle<float>> expandBounds_;
        std::vector<juce::Rectangle<float>> pluginSuggestionBounds_;

        // ═══ Acciones Directas — Bounds de los 3 botones en el footer ═════
        juce::Rectangle<float> actionAppliedBounds_;
        juce::Rectangle<float> actionSkippedBounds_;
        juce::Rectangle<float> actionExplainBounds_;

        static constexpr int kHeaderHeight = 22;
        static constexpr int kTrackRowHeight = 24;
        static constexpr int kFooterHeight = 20;
        static constexpr int kActionFooterHeight = 28;  // Altura extra para botones de acción
        static constexpr int kCardGap = 2;
        static constexpr int kSuggestionHeight = 20;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackProblemCard)
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  populatePluginSuggestions — Bridge entre PluginSuggestionsProvider (engine)
    //  y TrackPluginSuggestion (UI). Convierte las sugerencias del motor a
    //  structs renderizables en TrackProblemCard y ChatMessagesComponent.
    //
    //  Uso típico (desde NavigationShell o CoachEngine):
    //    for (auto& track : group.tracks) {
    //        populatePluginSuggestions(
    //            coachEngine_->getPluginSuggestionsProvider(),
    //            track,
    //            "gain",  // domain
    //            "CLIPPING", // issueType
    //            -1.5f,   // delta (dB)
    //            0.0f);   // frequencyHz
    //    }
    //  @param provider  PluginSuggestionsProvider (desde CoachEngine)
    //  @param track     TrackProblemData a poblar (modifica pluginSuggestions)
    //  @param domain    Domain string ("gain", "tonal", "dynamics", etc.)
    //  @param issueType Issue type string ("CLIPPING", "SOBRECOMPRIMIDO", etc.)
    //  @param delta     Delta en dB para interpolar en actionText
    //  @param frequencyHz Frecuencia en Hz para interpolar en actionText */
    void populatePluginSuggestions(
        const PluginSuggestionsProvider& provider,
        TrackProblemData& track,
        const juce::String& domain,
        const juce::String& issueType = {},
        float delta = 0.0f,
        float frequencyHz = 0.0f);

} // namespace mixcoach
