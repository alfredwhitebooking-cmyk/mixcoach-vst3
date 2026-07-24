#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>
#include <memory>
#include <shared_mutex>
#include <atomic>
#include <unordered_map>
#include "PluginSuggestion.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  PluginDatabase — Singleton que carga y consulta la base de datos de
    //  plugins (plugin_db.json). Proporciona búsqueda por problema, tier,
    //  y genera sugerencias configurables.
    //
    //  Uso:
    //    PluginDatabase& db = PluginDatabase::getInstance();
    //    auto suggestions = db.getSuggestions(ProblemType::Clipping);
    //    auto top = suggestions[0]; // Mejor sugerencia: Fruity Balance
    // ═══════════════════════════════════════════════════════════════════════════
    class PluginDatabase
    {
    public:
        /** Retorna la instancia singleton. */
        static PluginDatabase& getInstance();

        /** Carga (o recarga) la base de datos desde plugin_db.json.
            @param jsonPath  Ruta al archivo JSON. Si vacío, busca en
                             "plugins/plugin_db.json" relativo al ejecutable.
            @return true si se cargó correctamente */
        bool load(const juce::String& jsonPath = {});

        /** Retorna true si la base de datos está cargada. */
        [[nodiscard]] bool isLoaded() const noexcept { return loaded_.load(std::memory_order_acquire); }

        /** Retorna la lista completa de plugins cargados. */
        [[nodiscard]] const std::vector<PluginEntry>& getAllPlugins() const noexcept { return plugins_; }

        /** Busca plugins compatibles con un tipo de problema.
            @param problem  El tipo de problema a resolver
            @param maxResults  Máximo de resultados (default = 3)
            @return Vector de plugins, ordenados por rating descendente */
        [[nodiscard]] std::vector<PluginEntry> findByProblem(ProblemType problem,
                                                              int maxResults = 3) const;

        /** Busca plugins por tier (Native/Free/Premium).
            @param tier     Categoría de plugin
            @param problem  Opcional: filtrar también por problema
            @return Vector de plugins en ese tier */
        [[nodiscard]] std::vector<PluginEntry> findByTier(PluginTier tier,
                                                          ProblemType problem = ProblemType::Unknown) const;

        /** Retorna 1 sugerencia por tier para un problema (ideal para mostrar en el chat).
            Orden: Native → Free → Premium.
            @param problem  Problema a resolver
            @return Vector con 1 plugin de cada tier, o menos si no hay suficientes */
        [[nodiscard]] std::vector<PluginSuggestion> getSuggestionsByTier(ProblemType problem) const;

        /** Retorna la mejor sugerencia global para un problema (la de mayor rating).
            @param problem  Problema a resolver
            @return PluginSuggestion, o inválida si no encuentra */
        [[nodiscard]] PluginSuggestion getBestSuggestion(ProblemType problem) const;

        /** Obtiene un plugin por su ID.
            @param id  ID del plugin (ej: "fruity_balance")
            @return Puntero al plugin, o nullptr si no se encuentra */
        [[nodiscard]] const PluginEntry* getById(const juce::String& id) const;

        /** Genera un texto formateado para mostrar en el chat con las sugerencias.
            Ejemplo:
              🎛 Nativo  → Fruity Balance (-1.5 dB)
              🟢 Gratis  → Melda MUtility (gain -1.5 dB)
              ⭐ Premium → FabFilter Pro-G (salida -1.5 dB)
            @param suggestions  Lista de sugerencias
            @param trackName    Nombre de la pista (opcional)
            @return Texto formateado listo para addSystemMessage() */
        [[nodiscard]] juce::String formatSuggestions(const std::vector<PluginSuggestion>& suggestions,
                                                      const juce::String& trackName = {}) const;

        /** Convierte un DynamicObject (de JSON) a PluginEntry.
            @param obj  Objeto JSON del plugin
            @return PluginEntry poblado */
        static PluginEntry parsePluginEntry(const juce::DynamicObject& obj);

        /** Convierte un var JSON a ProblemType. */
        static ProblemType parseProblemType(const juce::var& val) noexcept;

        /** Interpola placeholders {delta}, {freq}, {frequencyHz}, {channel}
            en un actionText con valores reales.
            Ejemplo: "reduce {delta} dB en {freq}" → "reduce -1.5 dB en 58 Hz" */
        static juce::String interpolateAction(const juce::String& actionText,
                                               float delta,
                                               float frequencyHz) noexcept;

    private:
        PluginDatabase() = default;
        ~PluginDatabase() = default;
        PluginDatabase(const PluginDatabase&) = delete;
        PluginDatabase& operator=(const PluginDatabase&) = delete;

        std::atomic<bool> loaded_{false};
        std::vector<PluginEntry> plugins_;
        /** Cache de findByProblem(). Mapea ProblemType → plugins ordenados por rating.
            Se reconstruye en loadFromFile() bajo unique_lock.
            Se consulta en findByProblem() bajo shared_lock. */
        mutable std::unordered_map<ProblemType, std::vector<PluginEntry>> problemCache_;
        mutable std::shared_mutex mutex_;

        /** Carga el JSON desde un archivo. */
        bool loadFromFile(const juce::File& file);

        /** Parsea el array "plugins" del JSON en PluginEntry[]. */
        void parsePluginsArray(const juce::Array<juce::var>& arr);

        /** Reconstruye problemCache_ iterando plugins_ una sola vez.
            Debe llamarse bajo unique_lock (desde loadFromFile()).
            Agrupa plugins por ProblemType compatible y ordena por rating. */
        void rebuildCache() noexcept;
    };

} // namespace mixcoach
