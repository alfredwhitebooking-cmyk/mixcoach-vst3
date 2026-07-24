#pragma once
#include <juce_core/juce_core.h>
#include "PluginSuggestion.h"
#include "PluginDatabase.h"

namespace mixcoach {

    // Forward declarations
    class CoachEngine;

    // ═══════════════════════════════════════════════════════════════════════════
    //  PluginSuggestionsProvider — Puente entre la detección de problemas
    //  del CoachEngine y las sugerencias de plugins de PluginDatabase.
    //
    //  Responsabilidades:
    //    1. Mapear TrackGainAdvice/TrackDynamicsAdvice/etc. a ProblemType
    //    2. Consultar PluginDatabase para obtener sugerencias
    //    3. Interpolar valores reales (delta, frecuencia) en configs
    //    4. Generar mensajes formateados para el chat
    //    5. Mantener un "inventario detectado" de plugins del usuario
    //
    //  Uso (desde CoachEngine):
    //    PluginSuggestionsProvider provider;
    //    provider.setKnownPlugin("fabfilter_pro_q4"); // Usuario confirmó que lo tiene
    //    auto msg = provider.buildMessageForProblem(ProblemType::Clipping, "Kick", 0.6, -1.5);
    //    respondWith(msg, MentorMessage::Type::Tip);
    // ═══════════════════════════════════════════════════════════════════════════
    class PluginSuggestionsProvider
    {
    public:
        PluginSuggestionsProvider();
        ~PluginSuggestionsProvider() = default;

        // ─── Configuración ─────────────────────────────────────────────────

        /** Setea la ruta al plugin_db.json (opcional, default busca automáticamente). */
        void setDatabasePath(const juce::String& path) { dbPath_ = path; }

        /** Inicializa la base de datos. Llamar antes de usar. */
        void initialize();

        /** Retorna true si la base de datos está cargada. */
        [[nodiscard]] bool isReady() const noexcept { return ready_; }

        // ─── Inventario de plugins del usuario ─────────────────────────────

        /** Marca un plugin como "el usuario lo tiene instalado".
            @param pluginId  ID del plugin (ej: "fabfilter_pro_q4") */
        void addKnownPlugin(const juce::String& pluginId);

        /** Marca múltiples plugins como conocidos.
            @param pluginIds  Lista de IDs de plugins */
        void addKnownPlugins(const std::vector<juce::String>& pluginIds);

        /** Consulta si un plugin es conocido del usuario.
            @param pluginId  ID del plugin
            @return true si el usuario lo tiene */
        [[nodiscard]] bool hasPlugin(const juce::String& pluginId) const noexcept;

        /** Retorna la lista de IDs de plugins conocidos. */
        [[nodiscard]] const std::vector<juce::String>& getKnownPlugins() const noexcept
        {
            return knownPluginIds_;
        }

        /** Limpia el inventario de plugins conocidos. */
        void clearKnownPlugins() noexcept { knownPluginIds_.clear(); }

        // ─── Generación de sugerencias ─────────────────────────────────────

        /** Construye un mensaje completo de chat con sugerencias de plugins
            para un problema detectado.
            @param problem     Tipo de problema
            @param trackName   Nombre de la pista (puede ser vacío)
            @param delta       Valor del cambio sugerido (para interpolación en config)
            @param frequencyHz Frecuencia (para problemas de EQ)
            @return Mensaje formateado listo para enviar al chat */
        [[nodiscard]] juce::String buildMessageForProblem(ProblemType problem,
                                                           const juce::String& trackName = {},
                                                           float delta = 0.0f,
                                                           float frequencyHz = 0.0f) const;

        /** Versión que acepta un vector de sugerencias pre-construido.
            Útil cuando se quiere personalizar el orden o incluir solo ciertos tiers. */
        [[nodiscard]] juce::String buildMessageFromSuggestions(
            const std::vector<PluginSuggestion>& suggestions,
            const juce::String& trackName = {}) const;

        /** Interpola los placeholders en una actionText con valores reales.
            Ejemplo: "reduce {delta} dB en {frequencyHz} Hz"
            → "reduce -1.5 dB en 58 Hz"
            @param actionText  Texto de acción con placeholders
            @param delta       Valor delta (dB)
            @param frequencyHz Frecuencia (Hz)
            @return Texto interpolado */
        [[nodiscard]] static juce::String interpolateAction(
            const juce::String& actionText,
            float delta = 0.0f,
            float frequencyHz = 0.0f);

        /** Mapea un domain string del CoachEngine a ProblemType.
            @param domain  Domain del TrackIssue ("gain", "tonal", etc.)
            @param issueType Subtipo ("CLIPPING", "SOBRECOMPRIMIDO", etc.)
            @return ProblemType correspondiente */
        [[nodiscard]] static ProblemType domainToProblemType(
            const juce::String& domain,
            const juce::String& issueType = {});

        /** Mapea TrackGainAdvice status a ProblemType. */
        [[nodiscard]] static ProblemType gainAdviceToProblemType(
            float peakDeviation,
            float peakDb);

        /** Mapea TrackDynamicsAdvice a ProblemType. */
        [[nodiscard]] static ProblemType dynamicsAdviceToProblemType(
            bool isOvercompressed,
            bool isTooDynamic);

        // ─── Data access for TrackPluginSuggestion bridge ────────────────────

        /** Obtiene sugerencias de plugin para un ProblemType específico.
            Las actionText se interpolan con delta y frequencyHz.
            Retorna hasta 1 sugerencia por tier (Native → Free → Premium).
            @param problem     Tipo de problema
            @param delta       Valor delta (dB) para interpolar en actionText
            @param frequencyHz Frecuencia (Hz) para interpolar en actionText
            @return Vector de PluginSuggestion listos para convertir a UI */
        [[nodiscard]] std::vector<PluginSuggestion> getSuggestionsForProblem(
            ProblemType problem,
            float delta = 0.0f,
            float frequencyHz = 0.0f) const;

    private:
        bool ready_ = false;
        juce::String dbPath_;

        // Inventario de plugins que el usuario tiene instalados
        std::vector<juce::String> knownPluginIds_;
    };

} // namespace mixcoach
