#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#include <vector>

namespace mixcoach {

    // Forward declarations
    class PanelRevealManager;
    class ExperienceManager;
    class NavigationShell;

    // ═══════════════════════════════════════════════════════════════════════════
    //  LlmCommandInterpreter — Intérprete de comandos UI desde el LLM
    //
    //  FASE 10: El LLM emite comandos JSON estructurados para controlar la UI
    //  directamente, en lugar de depender solo de keyword detection.
    //
    //  El LLM incluye un bloque ```json { "ui": [...] } ``` al final de su
    //  respuesta. El interpreter extrae los comandos, los ejecuta a través de
    //  callbacks, y devuelve el texto limpio (sin JSON).
    //
    //  NavigationShell crea y cablea los callbacks una vez, y pasa el
    //  interpreter a AiCoachAdapter para que procese las respuestas del LLM.
    // ═══════════════════════════════════════════════════════════════════════════
    class LlmCommandInterpreter
    {
    public:
        LlmCommandInterpreter() = default;

        // ═══════════════════════════════════════════════════════════════════════
        //  Acciones de UI que el LLM puede solicitar
        // ═══════════════════════════════════════════════════════════════════════
        enum class Action : uint8_t
        {
            RevealPanel,      // Revelar un panel por primera vez
            SetCoachState,    // Cambiar CoachRoomState
            SwitchTab,        // Cambiar tab activo (Coach/Tools/Session)
            HighlightTrack,   // Resaltar un track por nombre
            Celebrate,        // Disparar animación de celebración
            SetMode,          // Cambiar modo Mix/Master
            ReturnToCoach,    // Volver al tab Coach
            AdvancePhase,     // Avanzar a la siguiente fase
            ShowReport,       // Mostrar overlay de reporte
            ShowSuggestions,  // Mostrar sugerencias rápidas como chips

            // ═══ Gap #2: Comandos de evidencia visual ═══════════════════
            SpectrumHighlight, // Resaltar región de frecuencia en el espectro
            MixmapHighlight,   // Resaltar track/bus en el MixMap
            AvatarEmotion,     // Cambiar expresión emocional del avatar
            ShowIssueCard,     // Mostrar tarjeta de issue inline en el chat

            // ═══ Auto-open Analyzer: Seleccionar sub-analiador específico ═══
            SelectAnalyzer     // Navegar a un sub-analiador en Tools (spectrum/vectorscope/crest/stereo/dna)
        };

        // ─── Comando parseado ──────────────────────────────────────────────
        struct UiCommand
        {
            Action action;

            // Parámetros opcionales (dependen del action)
            juce::String panel;            // RevealPanel: "reference","messengers","mixmap","tools","session","report"
            juce::String state;            // SetCoachState: "welcome","intention","genre","reference","messenger","mixmap","gain","balance","eq","compression","space","automation"
            juce::String tab;              // SwitchTab: "coach","tools","session"
            juce::String track;            // HighlightTrack: nombre del track
            int domain = -1;               // HighlightTrack: 0=gain,1=tonal,2=dynamics,3=spatial
            juce::String message;          // Celebrate: mensaje de logro
            bool isMixMode = true;         // SetMode: true=Modo Mix, false=Modo Master
            std::vector<juce::String> suggestions; // ShowSuggestions: lista de textos

            // ═══ Gap #2: Parámetros de evidencia visual ═══════════════
            float frequencyHz   = -1.0f;    // SpectrumHighlight: frecuencia central (Hz)
            float bandwidthHz   = 0.0f;     // SpectrumHighlight: ancho de banda (Hz, 0=auto)
            juce::String analyzer;           // SelectAnalyzer: "spectrum" | "vectorscope" | "crest" | "stereo" | "dna"
            juce::String emotion;            // AvatarEmotion: "happy","serious","thinking","surprised","encouraging","neutral"
            juce::String severity;           // ShowIssueCard: "info","warning","critical"
            juce::String issueType;          // ShowIssueCard: tipo de issue ("CLIP","EQ","DYN","PHASE","MASK")
            juce::String description;        // ShowIssueCard: descripción detallada
            juce::String bus;                // MixmapHighlight: nombre del bus ("Drums","Bass", etc.)
            juce::String label;              // SpectrumHighlight: etiqueta para el badge
        };

        // ═══════════════════════════════════════════════════════════════════════
        //  Callbacks — cableados por NavigationShell para ejecutar acciones
        // ═══════════════════════════════════════════════════════════════════════

        // ─── Callbacks por tipo de acción ──────────────────────────────────
        std::function<void(const juce::String& panelId)> onRevealPanel;
        std::function<void(const juce::String& state)> onSetCoachState;
        std::function<void(const juce::String& tab)> onSwitchTab;
        std::function<void(const juce::String& track, int domain)> onHighlightTrack;
        std::function<void(const juce::String& message)> onCelebrate;
        std::function<void(bool isMixMode)> onSetMode;
        std::function<void()> onReturnToCoach;
        std::function<void()> onAdvancePhase;
        std::function<void()> onShowReport;
        std::function<void(const std::vector<juce::String>& suggestions)> onShowSuggestions;

        // ═══ Auto-open Analyzer: Callback para seleccionar sub-analiador ═══
        /** SelectAnalyzer: Navega a un sub-analiador específico en el panel Tools.
            Cambia automáticamente al tab Tools y activa/desactiva toggles
            para mostrar el analizador solicitado.
            @param analyzer  "spectrum" | "vectorscope" | "crest" | "stereo" | "dna" */
        std::function<void(const juce::String& analyzer)> onSelectAnalyzer;

        // ═══ Gap #2: Callbacks de evidencia visual ══════════════════════════
        /** SpectrumHighlight: Resalta una región de frecuencia en el spectrograph.
            @param frequencyHz  Frecuencia central (Hz)
            @param bandwidthHz  Ancho de banda (0=auto 1/3 octava)
            @param label        Etiqueta descriptiva */
        std::function<void(float frequencyHz, float bandwidthHz, const juce::String& label)> onSpectrumHighlight;

        /** MixmapHighlight: Resalta un track o grupo en el MixMap.
            @param track  Nombre del track (vacío si se usa bus)
            @param bus    Nombre del bus ("Drums", "Bass", etc.) */
        std::function<void(const juce::String& track, const juce::String& bus)> onMixmapHighlight;

        /** AvatarEmotion: Cambia la expresión emocional del avatar del Coach.
            @param emotion  "happy", "serious", "thinking", "surprised", "encouraging", "neutral" */
        std::function<void(const juce::String& emotion)> onAvatarEmotion;

        /** ShowIssueCard: Muestra una tarjeta de issue inline en el chat.
            @param track       Nombre del track asociado
            @param severity    "info", "warning", "critical"
            @param issueType   Tipo de issue ("CLIP", "EQ", "DYN", "PHASE", "MASK")
            @param description Descripción detallada */
        std::function<void(const juce::String& track, const juce::String& severity,
                           const juce::String& issueType, const juce::String& description)> onShowIssueCard;

        // ═══════════════════════════════════════════════════════════════════════
        //  API pública
        // ═══════════════════════════════════════════════════════════════════════

        /** Procesa una respuesta completa del LLM.
            Extrae comandos JSON del bloque ```json...```, los ejecuta,
            y retorna el texto limpio (sin JSON) para mostrar en el chat.
            @param llmResponse  La respuesta completa del LLM (con posible JSON)
            @return Texto limpio para mostrar al usuario */
        juce::String processResponse(const juce::String& llmResponse);
        // ═══ Gap #3: Telemetría de comandos UI ═══════════════════════════════

        /** Conecta un CommandTelemetryCollector para registrar telemetría.
            El collector debe vivir fuera del interpreter (ej: en CoachEngine o AiCoachAdapter).
            @param collector  Puntero al collector. nullptr para desconectar. */
        void setTelemetryCollector(class CommandTelemetryCollector* collector) noexcept
        {
            telemetryCollector_ = collector;
        }

        /** Retorna el telemetry collector conectado (o nullptr). */
        [[nodiscard]] class CommandTelemetryCollector* getTelemetryCollector() const noexcept
        {
            return telemetryCollector_;
        }

        /** Parsea comandos JSON de un string (usado internamente y para testing).
            @param jsonBlock  El contenido del bloque JSON (sin ```json)
            @return Lista de comandos parseados */
        static std::vector<UiCommand> parseCommands(const juce::String& jsonBlock);

        /** Ejecuta un comando a través de los callbacks cableados.
            @param cmd  El comando a ejecutar
            @return true si el comando se ejecutó correctamente */
        bool executeCommand(const UiCommand& cmd);

        /** Retorna true si hay al menos un callback cableado. */
        [[nodiscard]] bool hasAnyCallbacks() const noexcept;

    private:
        /** Convierte string de acción del JSON a enum Action. */
        static Action actionFromString(const juce::String& str);

        /** Convierte string de coach state a CoachRoomState. */
        static bool isValidState(const juce::String& state);

        /** Convierte string de panel a PanelId. */
        static bool isValidPanel(const juce::String& panel);

        // ═══ Gap #3: Telemetría ═════════════════════════════════════════
        class CommandTelemetryCollector* telemetryCollector_ = nullptr;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  Formateador de instrucciones para el prompt del LLM
    //  Se inyecta en buildSystemPrompt() para enseñar al LLM a emitir comandos
    // ═══════════════════════════════════════════════════════════════════════════
    struct LlmCommandPrompt
    {
        /** Construye el bloque de instrucciones de comandos UI para el system prompt. */
        static juce::String buildCommandInstructions();
    };

} // namespace mixcoach
