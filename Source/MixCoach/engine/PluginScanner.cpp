#include "PluginScanner.h"
#include "PluginDatabase.h"
#include "PluginSuggestionsProvider.h"
#include "../../Common/types/LogHelper.h"
#include <algorithm>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  getStandardVST3Directories — Directorios VST3 según plataforma
    // ═══════════════════════════════════════════════════════════════════════════
    std::vector<juce::File> PluginScanner::getStandardVST3Directories()
    {
        std::vector<juce::File> dirs;

#ifdef JUCE_WINDOWS
        // Windows: C:\Program Files\Common Files\VST3
        dirs.push_back(juce::File("C:\\Program Files\\Common Files\\VST3"));
        // También revisar Program Files (x86) para plugins de 32 bits
        dirs.push_back(juce::File("C:\\Program Files (x86)\\Common Files\\VST3"));
#elif defined(JUCE_MAC)
        // macOS: /Library/Audio/Plug-Ins/VST3 (sistema) + ~/Library/Audio/Plug-Ins/VST3 (usuario)
        dirs.push_back(juce::File("/Library/Audio/Plug-Ins/VST3"));
        dirs.push_back(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                           .getChildFile("Library/Audio/Plug-Ins/VST3"));
#else
        // Linux: ~/.vst3 + /usr/lib/vst3
        dirs.push_back(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                           .getChildFile(".vst3"));
        dirs.push_back(juce::File("/usr/lib/vst3"));
        dirs.push_back(juce::File("/usr/local/lib/vst3"));
#endif

        // Filtrar directorios que no existen
        dirs.erase(std::remove_if(dirs.begin(), dirs.end(),
                                  [](const juce::File& d) { return !d.isDirectory(); }),
                   dirs.end());

        return dirs;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  scanAll — Escanea todos los directorios VST3 estándar
    // ═══════════════════════════════════════════════════════════════════════════
    int PluginScanner::scanAll()
    {
        clearResults();

        auto dirs = getStandardVST3Directories();
        int total = 0;

        LogHelper::writeToLog("[PluginScanner] Escaneando " + juce::String((int)dirs.size())
                              + " directorios VST3...");

        for (const auto& dir : dirs) {
            int count = scanDirectory(dir);
            total += count;
            LogHelper::writeToLog("[PluginScanner] " + dir.getFullPathName()
                                  + ": " + juce::String(count) + " bundles");
        }

        LogHelper::writeToLog("[PluginScanner] Total: " + juce::String(scannedBundleCount_)
                              + " bundles, " + juce::String((int)detectedPluginIds_.size())
                              + " conocidos, " + juce::String((int)unknownPluginNames_.size())
                              + " desconocidos");

        return scannedBundleCount_;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  scanDirectory — Escanea un directorio en busca de bundles .vst3
    // ═══════════════════════════════════════════════════════════════════════════
    int PluginScanner::scanDirectory(const juce::File& directory)
    {
        if (!directory.isDirectory()) return 0;

        juce::Array<juce::File> vst3Bundles;
        directory.findChildFiles(vst3Bundles, juce::File::findDirectories, false, "*.vst3");

        int count = 0;
        for (const auto& bundle : vst3Bundles) {
            if (readModuleInfo(bundle)) {
                ++count;
            }
            ++scannedBundleCount_;
        }

        return count;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  readModuleInfo — Lee moduleinfo.json de un bundle .vst3
    //
    //  moduleinfo.json está en: bundle.vst3/Contents/Resources/moduleinfo.json
    //  Contiene metadatos del plugin incluyendo name, vendor, version, class info.
    //
    //  Si no existe moduleinfo.json o no se puede parsear, intenta extraer
    //  el nombre desde el nombre del bundle como fallback.
    // ═══════════════════════════════════════════════════════════════════════════
    bool PluginScanner::readModuleInfo(const juce::File& vst3Bundle)
    {
        if (!vst3Bundle.isDirectory()) return false;

        // Buscar moduleinfo.json
        auto moduleInfoFile = vst3Bundle.getChildFile("Contents")
                                  .getChildFile("Resources")
                                  .getChildFile("moduleinfo.json");

        juce::StringArray foundNames;

        if (moduleInfoFile.existsAsFile()) {
            // Leer y parsear JSON
            try {
                juce::var json = juce::JSON::parse(moduleInfoFile);

                if (json.isObject()) {
                    auto* root = json.getDynamicObject();
                    if (root != nullptr) {
                        // Buscar el nombre del plugin en varias ubicaciones posibles
                        // 1. "name" directo
                        auto nameVar = root->getProperty("name");
                        if (nameVar.isString() && nameVar.toString().isNotEmpty()) {
                            foundNames.add(nameVar.toString());
                        }

                        // 2. "plugins" array con objetos que tienen "name"
                        auto pluginsVar = root->getProperty("plugins");
                        if (pluginsVar.isArray()) {
                            for (auto& p : *pluginsVar.getArray()) {
                                if (p.isObject()) {
                                    auto* pObj = p.getDynamicObject();
                                    if (pObj != nullptr) {
                                        auto pName = pObj->getProperty("name");
                                        if (pName.isString() && pName.toString().isNotEmpty()) {
                                            foundNames.addIfNotAlreadyThere(pName.toString());
                                        }
                                        // Algunos formatos usan "title" en vez de "name"
                                        auto pTitle = pObj->getProperty("title");
                                        if (pTitle.isString() && pTitle.toString().isNotEmpty()) {
                                            foundNames.addIfNotAlreadyThere(pTitle.toString());
                                        }
                                    }
                                }
                            }
                        }

                        // 3. "manufacturer" o "vendor" para contexto
                        auto vendorVar = root->getProperty("manufacturer");
                        if (!vendorVar.isString() || vendorVar.toString().isEmpty()) {
                            vendorVar = root->getProperty("vendor");
                        }
                        juce::String vendor;
                        if (vendorVar.isString() && vendorVar.toString().isNotEmpty()) {
                            vendor = vendorVar.toString();
                        }

                        // 4. "version" para depuración
                        juce::ignoreUnused(vendor);
                    }
                }
            } catch (const std::exception& e) {
                LogHelper::writeToLog("[PluginScanner] Error parseando moduleinfo.json de "
                                      + vst3Bundle.getFileName() + ": " + juce::String(e.what()));
            }
        }

        // Si no se encontraron nombres en moduleinfo.json, usar el nombre del bundle
        if (foundNames.isEmpty()) {
            juce::String bundleName = extractNameFromBundle(vst3Bundle);
            if (bundleName.isNotEmpty()) {
                foundNames.add(bundleName);
            }
        }

        // Cruzar cada nombre encontrado con PluginDatabase
        bool anyMatched = false;
        for (const auto& name : foundNames) {
            juce::String pluginId = crossReferenceName(name);
            if (pluginId.isNotEmpty()) {
                // Plugin conocido
                detectedPluginIds_.push_back(pluginId);
                if (onPluginDetected) onPluginDetected(pluginId);
                anyMatched = true;
            } else {
                // Plugin desconocido — registrar como desconocido
                unknownPluginNames_.push_back(name);
                if (onUnknownPluginDetected) onUnknownPluginDetected(name);
            }
        }

        return anyMatched || !foundNames.isEmpty();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  extractNameFromBundle — Extrae nombre legible del nombre del bundle .vst3
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String PluginScanner::extractNameFromBundle(const juce::File& vst3Bundle) noexcept
    {
        juce::String name = vst3Bundle.getFileNameWithoutExtension();
        // Limpiar sufijos comunes
        if (name.endsWithIgnoreCase("-win") || name.endsWithIgnoreCase("-mac")
            || name.endsWithIgnoreCase("-linux")) {
            name = name.dropLastCharacters(4);
        }
        // Remover números de versión al final (ej: "Plugin.v.1.0.0" → "Plugin")
        // y reemplazar separadores
        name = name.trim();
        return name;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  crossReferenceName — Cruza un nombre de plugin con PluginDatabase
    //
    //  Estrategia de matching:
    //    1. Exact match de plugin.name (case-insensitive)
    //    2. Partial match: nombre del plugin contiene el nombre escaneado
    //    3. Partial match inverso: nombre escaneado contiene el nombre del plugin
    //    4. Fuzzy match por palabras clave
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String PluginScanner::crossReferenceName(const juce::String& pluginName) const
    {
        if (pluginName.isEmpty()) return {};

        auto& db = PluginDatabase::getInstance();
        if (!db.isLoaded()) return {};

        juce::String lowerName = pluginName.trim().toLowerCase();

        auto allPlugins = db.getAllPlugins();

        // Paso 1: Exact match (case-insensitive)
        for (const auto& entry : allPlugins) {
            if (entry.name.toLowerCase().trim() == lowerName) {
                return entry.id;
            }
        }

        // Paso 2: Partial match (nombre escaneado contiene nombre del plugin)
        for (const auto& entry : allPlugins) {
            juce::String entryLower = entry.name.toLowerCase().trim();
            if (lowerName.contains(entryLower) || entryLower.contains(lowerName)) {
                return entry.id;
            }
        }

        // Paso 3: Match por palabras clave (si el nombre escaneado contiene
        // al menos 2 palabras del nombre del plugin)
        for (const auto& entry : allPlugins) {
            auto entryWords = juce::StringArray::fromTokens(entry.name, " ", "\"");
            int matches = 0;
            for (const auto& word : entryWords) {
                juce::String wordLower = word.toLowerCase().trim();
                if (wordLower.length() > 2 && lowerName.contains(wordLower)) {
                    ++matches;
                }
            }
            // Si coincide al menos la mitad de las palabras (mín 2)
            if (matches >= 2 && matches >= entryWords.size() / 2) {
                return entry.id;
            }
        }

        // No se encontró match
        return {};
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  clearResults — Limpia todos los resultados de escaneo
    // ═══════════════════════════════════════════════════════════════════════════
    void PluginScanner::clearResults()
    {
        detectedPluginIds_.clear();
        unknownPluginNames_.clear();
        scannedBundleCount_ = 0;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  buildSummary — Construye mensaje formateado con resumen de plugins
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String PluginScanner::buildSummary() const
    {
        if (scannedBundleCount_ == 0) {
            return "[SEARCH] No se encontraron plugins VST3 en los directorios "
                   "est\xC3\xA1ndar del sistema.";
        }

        juce::String msg;
        msg << "[COACH] **Plugins detectados** ("
            << scannedBundleCount_ << " bundles escaneados)\n\n";

        // Agrupar por tier
        auto& db = PluginDatabase::getInstance();

        std::vector<const PluginEntry*> premium;
        std::vector<const PluginEntry*> freePlugins;
        std::vector<const PluginEntry*> nativePlugins;

        for (const auto& id : detectedPluginIds_) {
            const auto* entry = db.getById(id);
            if (entry == nullptr) continue;

            switch (entry->tier) {
                case PluginTier::Premium: premium.push_back(entry); break;
                case PluginTier::Free:    freePlugins.push_back(entry); break;
                case PluginTier::Native:  nativePlugins.push_back(entry); break;
                default: break;
            }
        }

        // Profesionales
        if (!premium.empty()) {
            msg << "\xE2\xAD\x90 **Premium:** ";
            for (size_t i = 0; i < premium.size(); ++i) {
                if (i > 0) msg << ", ";
                msg << premium[i]->name;
            }
            msg << "\n";
        }

        // Gratis
        if (!freePlugins.empty()) {
            msg << "\xF0\x9F\x9F\xA2 **Gratis:** ";
            for (size_t i = 0; i < freePlugins.size(); ++i) {
                if (i > 0) msg << ", ";
                msg << freePlugins[i]->name;
            }
            msg << "\n";
        }

        // Nativos
        if (!nativePlugins.empty()) {
            msg << "[COACH] **Nativos:** ";
            for (size_t i = 0; i < nativePlugins.size(); ++i) {
                if (i > 0) msg << ", ";
                msg << nativePlugins[i]->name;
            }
            msg << "\n";
        }

        // Desconocidos
        if (!unknownPluginNames_.empty()) {
            msg << "\n[QUESTION] **Desconocidos** (no est\xC3\xA1n en mi base de datos): ";
            for (size_t i = 0; i < unknownPluginNames_.size(); ++i) {
                if (i > 0) msg << ", ";
                msg << unknownPluginNames_[i];
            }
            msg << "\n";
        }

        // Footer
        msg << "\n\xF0\x9F\x92\xA1 A partir de ahora priorizar\xC3\xA9 estos plugins "
               "en mis sugerencias.";

        return msg;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  buildShortSummary — Resumen breve de una línea
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String PluginScanner::buildShortSummary() const
    {
        if (scannedBundleCount_ == 0) return "0 plugins detectados";

        int known = (int)detectedPluginIds_.size();
        int unknown = (int)unknownPluginNames_.size();

        if (unknown > 0) {
            return juce::String(scannedBundleCount_) + " plugins detectados ("
                   + juce::String(known) + " conocidos, "
                   + juce::String(unknown) + " desconocidos)";
        }

        return juce::String(scannedBundleCount_) + " plugins detectados";
    }

} // namespace mixcoach
