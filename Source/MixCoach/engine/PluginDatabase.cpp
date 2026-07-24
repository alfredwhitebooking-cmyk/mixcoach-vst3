#include "PluginDatabase.h"
#include "../../Common/types/LogHelper.h"
#include <juce_core/juce_core.h>
#include <shared_mutex>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Singleton
    // ═══════════════════════════════════════════════════════════════════════════
    PluginDatabase& PluginDatabase::getInstance()
    {
        static PluginDatabase instance;
        return instance;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  load — Carga el JSON desde disco
    // ═══════════════════════════════════════════════════════════════════════════
    bool PluginDatabase::load(const juce::String& jsonPath)
    {
        juce::File file;

        if (jsonPath.isNotEmpty()) {
            file = juce::File(jsonPath);
        }

        // Fallback 1: buscar en el directorio del ejecutable
        if (!file.existsAsFile()) {
            file = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                       .getParentDirectory()
                       .getChildFile("plugins/plugin_db.json");
        }

        // Fallback 2: buscar relativo al CWD (desarrollo)
        if (!file.existsAsFile()) {
            file = juce::File::getCurrentWorkingDirectory()
                       .getChildFile("plugins/plugin_db.json");
        }

        // Fallback 3: buscar relativo al directorio raiz del proyecto
        if (!file.existsAsFile()) {
            file = juce::File::getCurrentWorkingDirectory()
                       .getParentDirectory()
                       .getChildFile("plugins/plugin_db.json");
        }

        if (!file.existsAsFile()) {
            LogHelper::writeToLog("[PluginDatabase] ERROR: plugin_db.json not found at "
                                  + file.getFullPathName());
            loaded_.store(false, std::memory_order_release);
            return false;
        }

        // Lock exclusivo durante la escritura — evita que lectores concurrentes
        // vean un estado parcial mientras se reconstruye plugins_.
        size_t pluginCount = 0;
        {
            std::unique_lock<std::shared_mutex> writeLock(mutex_);
            bool success = loadFromFile(file);
            pluginCount = plugins_.size();
            if (!success) {
                loaded_.store(false, std::memory_order_release);
                return false;
            }
        }

        LogHelper::writeToLog("[PluginDatabase] Loaded " + juce::String((int)pluginCount)
                              + " plugins from " + file.getFileName());
        return true;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  loadFromFile — Debe llamarse bajo unique_lock (desde load()).
    // ═══════════════════════════════════════════════════════════════════════════
    bool PluginDatabase::loadFromFile(const juce::File& file)
    {
        // ASSERT: mutex_ already locked (unique_lock from load())
        try {
            juce::var json = juce::JSON::parse(file);

            if (!json.isObject()) {
                LogHelper::writeToLog("[PluginDatabase] JSON parse failed or not an object");
                return false;
            }

            auto* root = json.getDynamicObject();
            if (root == nullptr) return false;

            // Cargar el array de plugins
            auto pluginsVar = root->getProperty("plugins");
            if (!pluginsVar.isArray()) {
                LogHelper::writeToLog("[PluginDatabase] No 'plugins' array found in JSON");
                return false;
            }

            plugins_.clear();
            if (auto* arr = pluginsVar.getArray())
                parsePluginsArray(*arr);
            rebuildCache();
            loaded_.store(true, std::memory_order_release);
            return true;

        } catch (const std::exception& e) {
            LogHelper::writeToLog("[PluginDatabase] Exception: " + juce::String(e.what()));
            return false;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  parsePluginsArray
    // ═══════════════════════════════════════════════════════════════════════════
    void PluginDatabase::parsePluginsArray(const juce::Array<juce::var>& arr)
    {
        for (auto& item : arr) {
            if (!item.isObject()) continue;
            auto* obj = item.getDynamicObject();
            if (obj == nullptr) continue;

            PluginEntry entry = parsePluginEntry(*obj);
            plugins_.push_back(std::move(entry));
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  parsePluginEntry — Convierte un objeto JSON a PluginEntry
    // ═══════════════════════════════════════════════════════════════════════════
    PluginEntry PluginDatabase::parsePluginEntry(const juce::DynamicObject& obj)
    {
        PluginEntry entry;
        entry.id          = obj.getProperty("id").toString();
        entry.name        = obj.getProperty("name").toString();
        entry.developer   = obj.getProperty("developer").toString();
        entry.tier        = pluginTierFromString(obj.getProperty("tier").toString());
        entry.daw         = obj.getProperty("daw").toString();
        {
            auto r = obj.getProperty("rating");
            entry.rating = r.isDouble() ? (float)r.operator double() : 0.0f;
        }
        entry.icon        = obj.getProperty("icon").toString();
        entry.description = obj.getProperty("description").toString();

        // Compatibilidad (array de strings → vector<ProblemType>)
        auto compatArr = obj.getProperty("compatibility").getArray();
        if (compatArr != nullptr) {
            for (auto& c : *compatArr) {
                ProblemType pt = parseProblemType(c);
                if (pt != ProblemType::Unknown)
                    entry.compatibility.push_back(pt);
            }
        }

        // Default config (objeto JSON → PluginConfig)
        auto defaultConfigObj = obj.getProperty("defaultConfig").getDynamicObject();
        if (defaultConfigObj != nullptr) {
            PluginConfig cfg;
            // No hay actionText para defaultConfig (es estructural)
            for (auto& key : defaultConfigObj->getProperties()) {
                double val = defaultConfigObj->getProperty(key.name).operator double();
                cfg.params[key.name.toString()] = (float)val;
                cfg.paramLabels.add(key.name.toString());
            }
            entry.defaultConfig = cfg;
        }

        // Configs por problema (objeto de objetos)
        auto configsObj = obj.getProperty("configs").getDynamicObject();
        if (configsObj != nullptr) {
            for (auto& cfgKey : configsObj->getProperties()) {
                juce::String problemStr = cfgKey.name.toString();
                ProblemType pt = parseProblemType(juce::var(problemStr));
                if (pt != ProblemType::Unknown) {
                    auto* cfgObj = configsObj->getProperty(cfgKey.name).getDynamicObject();
                    if (cfgObj != nullptr) {
                        PluginConfig cfg;
                        cfg.actionText = cfgObj->getProperty("action").toString();

                        auto paramsObj = cfgObj->getProperty("params").getDynamicObject();
                        if (paramsObj != nullptr) {
                            for (auto& p : paramsObj->getProperties()) {
                                double val = paramsObj->getProperty(p.name).operator double();
                                cfg.params[p.name.toString()] = (float)val;
                                cfg.paramLabels.add(p.name.toString());
                            }
                        }
                        entry.configs[pt] = cfg;
                    }
                }
            }
        }

        return entry;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  parseProblemType
    // ═══════════════════════════════════════════════════════════════════════════
    ProblemType PluginDatabase::parseProblemType(const juce::var& val) noexcept
    {
        if (val.isString())
            return problemTypeFromString(val.toString());
        return ProblemType::Unknown;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  rebuildCache — Reconstruye problemCache_ iterando plugins_ una sola vez.
    //  Debe llamarse bajo unique_lock (desde loadFromFile()).
    // ═══════════════════════════════════════════════════════════════════════════
    void PluginDatabase::rebuildCache() noexcept
    {
        problemCache_.clear();

        for (const auto& plugin : plugins_) {
            for (auto pt : plugin.compatibility) {
                if (pt != ProblemType::Unknown)
                    problemCache_[pt].push_back(plugin);
            }
        }

        // Ordenar cada lista del cache por rating descendente
        for (auto& [problem, entries] : problemCache_) {
            juce::ignoreUnused(problem);
            std::sort(entries.begin(), entries.end(),
                      [](const PluginEntry& a, const PluginEntry& b) {
                          return a.rating > b.rating;
                      });
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  findByProblem — Busca plugins compatibles con un problema.
    //  Usa el cache pre-ordenado para O(1) lookup + slice O(maxResults).
    // ═══════════════════════════════════════════════════════════════════════════
    std::vector<PluginEntry> PluginDatabase::findByProblem(ProblemType problem,
                                                            int maxResults) const
    {
        std::shared_lock<std::shared_mutex> readLock(mutex_);

        auto it = problemCache_.find(problem);
        if (it == problemCache_.end() || it->second.empty())
            return {};

        const auto& sorted = it->second;
        int count = juce::jmin((int)sorted.size(), maxResults);
        return std::vector<PluginEntry>(sorted.begin(), sorted.begin() + count);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  findByTier
    // ═══════════════════════════════════════════════════════════════════════════
    std::vector<PluginEntry> PluginDatabase::findByTier(PluginTier tier,
                                                        ProblemType problem) const
    {
        std::shared_lock<std::shared_mutex> readLock(mutex_);
        std::vector<PluginEntry> results;
        for (const auto& plugin : plugins_) {
            if (plugin.tier != tier) continue;
            if (problem != ProblemType::Unknown && !plugin.isCompatible(problem)) continue;
            results.push_back(plugin);
        }

        // Ordenar por rating descendente
        std::sort(results.begin(), results.end(),
                  [](const PluginEntry& a, const PluginEntry& b) {
                      return a.rating > b.rating;
                  });

        return results;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getSuggestionsByTier — 1 sugerencia por tier (Native → Free → Premium)
    // ═══════════════════════════════════════════════════════════════════════════
    std::vector<PluginSuggestion> PluginDatabase::getSuggestionsByTier(ProblemType problem) const
    {
        // Lock compartido — permite lecturas concurrentes pero bloquea si load() escribe.
        std::shared_lock<std::shared_mutex> readLock(mutex_);
        std::vector<PluginSuggestion> results;

        PluginTier tiers[] = { PluginTier::Native, PluginTier::Free, PluginTier::Premium };

        for (auto tier : tiers) {
            // Buscar directamente en plugins_ (no en findByTier() que retorna copias)
            // Esto evita el dangling pointer al tomar direcciones de elementos locales.
            const PluginEntry* bestForTier = nullptr;
            for (const auto& plugin : plugins_) {
                if (plugin.tier != tier) continue;
                if (!plugin.isCompatible(problem)) continue;
                if (bestForTier == nullptr || plugin.rating > bestForTier->rating)
                    bestForTier = &plugin;
            }

            if (bestForTier == nullptr) continue;

            PluginSuggestion sug;
            sug.plugin  = bestForTier;
            sug.problem = problem;
            sug.config  = &bestForTier->getConfig(problem);
            results.push_back(sug);
        }

        return results;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getBestSuggestion
    // ═══════════════════════════════════════════════════════════════════════════
    PluginSuggestion PluginDatabase::getBestSuggestion(ProblemType problem) const
    {
        std::shared_lock<std::shared_mutex> readLock(mutex_);

        // Buscar directamente en plugins_ para evitar dangling pointer
        // de findByProblem() que retorna copias locales.
        const PluginEntry* best = nullptr;
        for (const auto& plugin : plugins_) {
            if (!plugin.isCompatible(problem)) continue;
            if (best == nullptr || plugin.rating > best->rating)
                best = &plugin;
        }

        if (best == nullptr) return {};

        PluginSuggestion sug;
        sug.plugin  = best;
        sug.problem = problem;
        sug.config  = &best->getConfig(problem);
        return sug;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getById
    // ═══════════════════════════════════════════════════════════════════════════
    const PluginEntry* PluginDatabase::getById(const juce::String& id) const
    {
        std::shared_lock<std::shared_mutex> readLock(mutex_);
        for (const auto& plugin : plugins_) {
            if (plugin.id == id) return &plugin;
        }
        return nullptr;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  formatSuggestions — Genera texto formateado para el chat
    //
    //  Formato:
    //    🎛 Nativo  → Fruity Balance (-1.5 dB)
    //    🟢 Gratis  → TDR Nova (Q=1.3, +2.0 dB @ {freq} Hz)
    //    ⭐ Premium → FabFilter Pro-Q 4 (Q=1.6, -3.0 dB @ {freq} Hz, MID)
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String PluginDatabase::formatSuggestions(
        const std::vector<PluginSuggestion>& suggestions,
        const juce::String& trackName) const
    {
        if (suggestions.empty()) return {};

        juce::String msg;

        // Header
        if (trackName.isNotEmpty()) {
            msg << "[CHANGE] **Sugerencias para " << trackName << "**\n\n";
        } else {
            msg << "[CHANGE] **\xC2\xBF" "C\xC3\xB3mo quieres solucionarlo?**\n\n";
        }

        for (size_t i = 0; i < suggestions.size(); ++i) {
            const auto& sug = suggestions[i];
            if (sug.plugin == nullptr || sug.config == nullptr) continue;

            // Línea de tier + nombre + rating
            // Usar plugin->tier para el ícono y label original (Native/Free/Premium)
            // Si el usuario ya tiene el plugin (effectiveTier == UserHas),
            // se agrega "(instalado)" al label preservando el contexto del tier.
            PluginTier originalTier = sug.plugin->tier;
            const char* tierIcon = "";
            switch (originalTier) {
                case PluginTier::Native:  tierIcon = "[COACH]"; break; // 🎛
                case PluginTier::Free:    tierIcon = "\xF0\x9F\x9F\xA2"; break; // 🟢
                case PluginTier::Premium: tierIcon = "\xE2\xAD\x90";     break; // ⭐
                case PluginTier::UserHas: tierIcon = "[BOLT]";     break; // ⚡
                default: break;
            }

            msg << tierIcon << " " << pluginTierLabel(originalTier);
            if (sug.effectiveTier == PluginTier::UserHas)
                msg << " (instalado)";
            msg << "  **" << sug.plugin->name << "**";

            // Stars de rating
            int fullStars = (int)sug.plugin->rating;
            msg << " " << juce::String::repeatedString("\xE2\x98\x85", fullStars)
                << juce::String::repeatedString("\xE2\x98\x86", 5 - fullStars);

            msg << "\n";

            // Acción sugerida con interpolación de parámetros reales
            juce::String action = sug.config->actionText;
            if (action.isNotEmpty()) {
                action = interpolateAction(action, sug.delta, sug.frequencyHz);
                msg << "    " << action << "\n";
            }

            if (i < suggestions.size() - 1)
                msg << "\n";
        }

        // Footer con opción "Ya tengo otro plugin"
        msg << "\n[BOLT] **Ya tengo otro plugin** \xC2\xB7 dime cu\xC3\xA1l tienes";

        return msg;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  interpolateAction — Reemplaza placeholders en actionText con valores reales
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String PluginDatabase::interpolateAction(const juce::String& actionText,
                                                    float delta,
                                                    float frequencyHz) noexcept
    {
        juce::String result = actionText;

        // Always replace {delta}, even when zero (avoids literal "{delta}" in output for zero-delta actions)
        result = result.replace("{delta}", juce::String(delta, 1));

        if (frequencyHz > 0.0f) {
            juce::String freqStr;
            if (frequencyHz >= 1000.0f)
                freqStr = juce::String(frequencyHz / 1000.0f, 1) + " kHz";
            else
                freqStr = juce::String((int)frequencyHz) + " Hz";
            result = result.replace("{frequencyHz}", freqStr);
            result = result.replace("{freq}", freqStr);
        }

        result = result.replace("{channel}", "izquierdo");

        return result;
    }

} // namespace mixcoach
