#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <functional>

namespace mixcoach {

    // Forward declarations
    class PluginSuggestionsProvider;

    // ═══════════════════════════════════════════════════════════════════════════
    //  PluginScanner — Escanea el sistema en busca de plugins VST3 instalados
    //  y los cruza con la PluginDatabase para marcar cuáles tiene el usuario.
    //
    //  Funcionamiento:
    //    1. Escanea directorios VST3 estándar del sistema
    //    2. Lee Contents/Resources/moduleinfo.json de cada bundle .vst3
    //    3. Cruza los nombres encontrados con PluginDatabase
    //    4. Inyecta los resultados en PluginSuggestionsProvider
    //
    //  Uso (desde CoachEngine):
    //    PluginScanner scanner;
    //    scanner.onPluginDetected = [this](const juce::String& pluginId) {
    //        pluginSuggestionsProvider_.addKnownPlugin(pluginId);
    //    };
    //    scanner.scanAll();
    //    auto summary = scanner.buildSummary();
    //    respondWith(summary);
    // ═══════════════════════════════════════════════════════════════════════════
    class PluginScanner
    {
    public:
        PluginScanner() = default;
        ~PluginScanner() = default;

        // ═══ Callbacks ═══════════════════════════════════════════════════════
        /** Se dispara cuando se detecta un plugin conocido en la base de datos.
            @param pluginId  ID del plugin en PluginDatabase */
        std::function<void(const juce::String& pluginId)> onPluginDetected;

        /** Se dispara cuando se detecta un plugin NO conocido (no está en DB).
            @param displayName  Nombre mostrable del plugin encontrado */
        std::function<void(const juce::String& displayName)> onUnknownPluginDetected;

        // ═══ Escaneo ═════════════════════════════════════════════════════════
        /** Escanea todos los directorios VST3 estándar del sistema.
            Busca bundles .vst3, lee moduleinfo.json y cruza con PluginDatabase.
            @return Número total de plugins detectados (conocidos + desconocidos) */
        int scanAll();

        /** Escanea un directorio específico en busca de bundles .vst3.
            @param directory  Directorio a escanear
            @return Número de bundles .vst3 encontrados en este directorio */
        int scanDirectory(const juce::File& directory);

        // ═══ Resultados ══════════════════════════════════════════════════════
        /** Retorna los IDs de plugins conocidos detectados. */
        [[nodiscard]] const std::vector<juce::String>& getDetectedPluginIds() const noexcept
        {
            return detectedPluginIds_;
        }

        /** Retorna los nombres de plugins desconocidos detectados. */
        [[nodiscard]] const std::vector<juce::String>& getUnknownPlugins() const noexcept
        {
            return unknownPluginNames_;
        }

        /** Retorna true si se encontró al menos un plugin. */
        [[nodiscard]] bool hasDetectedPlugins() const noexcept
        {
            return !detectedPluginIds_.empty() || !unknownPluginNames_.empty();
        }

        /** Retorna el número total de bundles .vst3 escaneados. */
        [[nodiscard]] int getScannedBundleCount() const noexcept { return scannedBundleCount_; }

        /** Limpia todos los resultados de escaneo. */
        void clearResults();

        /** Construye un mensaje formateado con el resumen de plugins detectados
            para mostrar en el chat del coach.
            Ejemplo:
              "🎛 Plugins detectados (12 encontrados):
               ⭐ FabFilter Pro-Q 3, Valhalla VintageVerb, FabFilter Pro-L 2
               🟢 TDR Nova, Youlean Loudness Meter, Ozone Imager
               🎛 Fruity Parametric EQ 2, Fruity Limiter, Maximus
               ❓ 3 plugins desconocidos (no en mi base de datos)"
            @return Texto formateado listo para addSystemMessage() */
        [[nodiscard]] juce::String buildSummary() const;

        /** Retorna un resumen breve (1 línea) para mostrar en la UI del chat.
            Ej: "12 plugins detectados (3 desconocidos)" */
        [[nodiscard]] juce::String buildShortSummary() const;

        // ═══ Directorios VST3 estándar ═══════════════════════════════════════
        /** Retorna los directorios estándar de VST3 en el sistema actual.
            En Windows: C:\Program Files\Common Files\VST3
            En macOS: ~/Library/Audio/Plug-Ins/VST3, /Library/Audio/Plug-Ins/VST3
            En Linux: ~/.vst3, /usr/lib/vst3 */
        [[nodiscard]] static std::vector<juce::File> getStandardVST3Directories();

    private:
        /** Intenta leer moduleinfo.json de un bundle .vst3 y extraer nombres.
            @param vst3Bundle  Directorio del bundle .vst3
            @return true si se pudo leer y extraer al menos un nombre */
        bool readModuleInfo(const juce::File& vst3Bundle);

        /** Intenta extraer el nombre del plugin desde el nombre del bundle .vst3
            (fallback cuando no hay moduleinfo.json o no se puede leer).
            Retorna el nombre del directorio sin extensión .vst3. */
        static juce::String extractNameFromBundle(const juce::File& vst3Bundle) noexcept;

        /** Cruza un nombre de plugin con la PluginDatabase.
            @param pluginName  Nombre a buscar (puede ser parcial)
            @return ID del plugin si se encontró, vacío si no */
        juce::String crossReferenceName(const juce::String& pluginName) const;

        // ─── Cache de resultados ───────────────────────────────────────────
        std::vector<juce::String> detectedPluginIds_;
        std::vector<juce::String> unknownPluginNames_;
        int scannedBundleCount_ = 0;
    };

} // namespace mixcoach
