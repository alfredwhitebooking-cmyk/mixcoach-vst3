#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

namespace mixcoach {

    // Forward declarations — evitamos includes pesados en el header
    struct DifferenceProfile;
    class ReferenceAnalyzer;

    // ═══════════════════════════════════════════════════════════════════════════
    //  ReferenceProfile — Perfil completo de una referencia o mezcla.
    //
    //  Unifica en un solo struct:
    //    • Metadata: timestamp, nombre, ruta, género, duración
    //    • Raw metrics: LUFS, TruePeak, CrestFactor, StereoWidth, Correlation, etc.
    //    • 5 high-level scores normalizados (0.0–1.0)
    //    • Serialización JSON para cache persistente
    //
    //  Uso:
    //    // Desde la mezcla actual
    //    auto mixProfile = ReferenceProfile::computeFromMix(differenceProfile);
    //
    //    // Desde la referencia analizada (para cache)
    //    auto refProfile = ReferenceProfile::computeFromReference(refAnalyzer);
    //
    //    // Persistencia
    //    refProfile.toJson(obj);
    //    auto loaded = ReferenceProfile::fromJson(obj);
    // ═══════════════════════════════════════════════════════════════════════════
    struct ReferenceProfile
    {
        // ═══════════════════════════════════════════════════════════════════════
        //  Metadata
        // ═══════════════════════════════════════════════════════════════════════
        int64_t timestampUs      = 0;
        juce::String referenceName;
        juce::String filePath;
        juce::String genreGuess;
        double durationSeconds   = 0.0;
        int sampleRate           = 0;
        bool valid               = false;

        // ═══════════════════════════════════════════════════════════════════════
        //  Raw Metrics (desde ReferenceAnalyzer o AudioAnalyzer)
        // ═══════════════════════════════════════════════════════════════════════
        float integratedLUFS     = -100.0f;
        float shortTermLUFS      = -100.0f;
        float momentaryLUFS      = -100.0f;
        float truePeakDBTP       = -100.0f;
        float crestFactor        = 0.0f;
        float stereoWidth        = 0.0f;    // 0.0=mono, 1.0=full stereo
        float correlation        = 0.0f;    // -1.0 a +1.0
        float loudnessRange      = 0.0f;    // LU (LRA)
        float spectralCentroidHz = 0.0f;

        // ═══ 6 regiones espectrales (Sub, Bass, LoMid, HiMid, Presence, Air) ═══
        float regionEnergy[6] = {-100.0f, -100.0f, -100.0f, -100.0f,
                                  -100.0f, -100.0f};

        // ═══════════════════════════════════════════════════════════════════════
        //  High-Level Scores (0.0–1.0, normalizados)
        // ═══════════════════════════════════════════════════════════════════════
        float subBalance        = 0.5f; // 0.0=pocos graves  1.0=muchos graves
        float punchScore        = 0.5f; // 0.0=comprimido    1.0=muy punchy
        float densityScore      = 0.5f; // 0.0=hueco         1.0=denso/lleno
        float brightnessScore   = 0.5f; // 0.0=oscuro        1.0=muy brillante
        float dynamicScore      = 0.5f; // 0.0=comprimido    1.0=muy dinámico

        // ═══════════════════════════════════════════════════════════════════════
        //  Builders
        // ═══════════════════════════════════════════════════════════════════════

        /** Construye desde un DifferenceProfile (mezcla actual).
            Lee mixRegionEnergy, mixCrestFactor, mixLoudnessRange,
            mixSpectralCentroidHz y las raw metrics. */
        static ReferenceProfile computeFromMix(const DifferenceProfile& dp) noexcept;

        /** Construye desde un ReferenceAnalyzer (referencia cargada).
            Lee LUFS, TruePeak, LoudnessRange, band energies, y estima
            stereo width desde el buffer de audio. */
        static ReferenceProfile computeFromReference(const ReferenceAnalyzer& ref) noexcept;

        /** Construye desde datos crudos (útil para testing). */
        static ReferenceProfile computeFromData(
            float crestFactor, float loudnessRange,
            float spectralCentroidHz, const float regionEnergy[6],
            float integratedLUFS = -100.0f,
            float truePeak = -100.0f,
            float stereoWidth = 0.0f,
            float correlation = 0.0f) noexcept;

        // ═══════════════════════════════════════════════════════════════════════
        //  Per-Score Compute Functions (públicas para testing)
        // ═══════════════════════════════════════════════════════════════════════

        static float computeSubBalance(const float regionEnergy[6]) noexcept;
        static float computePunchScore(float crestFactor) noexcept;
        static float computeDensityScore(
            float crestFactor, float loudnessRange,
            const float regionEnergy[6]) noexcept;
        static float computeBrightnessScore(
            float spectralCentroidHz, const float regionEnergy[6]) noexcept;
        static float computeDynamicScore(
            float crestFactor, float loudnessRange) noexcept;

        // ═══════════════════════════════════════════════════════════════════════
        //  Serialization (JSON via DynamicObject)
        // ═══════════════════════════════════════════════════════════════════════

        /** Serializa este ReferenceProfile a un DynamicObject. */
        void toJson(juce::DynamicObject& obj) const;

        /** Deserializa desde un DynamicObject. */
        static ReferenceProfile fromJson(const juce::DynamicObject& obj);

        /** Guarda a un archivo JSON. Retorna true si ok. */
        bool saveToFile(const juce::File& file) const;

        /** Carga desde un archivo JSON. Retorna perfil válido o inválido. */
        static ReferenceProfile loadFromFile(const juce::File& file);

        // ═══════════════════════════════════════════════════════════════════════
        //  Text Helpers
        // ═══════════════════════════════════════════════════════════════════════

        /** Resumen breve de los 5 scores. */
        [[nodiscard]] juce::String toShortText() const noexcept;

        /** Resumen detallado con valores crudos + scores. */
        [[nodiscard]] juce::String toVerboseText() const noexcept;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  ReferenceProfileCache — Persistencia en disco de perfiles analizados
    //
    //  Cachea ReferenceProfiles en archivos JSON con key = hash de la ruta.
    //  Evita re-analizar el mismo archivo de referencia cada vez que se carga.
    //
    //  Uso:
    //    ReferenceProfileCache cache(cacheDir);
    //    auto profile = cache.load(hash);       // Carga si existe
    //    if (!profile.valid)
    //        profile = computeFromReference(ref);  // Analiza
    //    cache.save(hash, profile);                 // Cachea
    // ═══════════════════════════════════════════════════════════════════════════
    class ReferenceProfileCache
    {
    public:
        /** @param cacheDirectory Directorio donde se guardan los JSON de cache. */
        explicit ReferenceProfileCache(const juce::File& cacheDirectory);

        /** Guarda un perfil en el cache. */
        void save(const juce::String& key, const ReferenceProfile& profile);

        /** Carga un perfil del cache. Retorna ReferenceProfile inválido si no existe. */
        [[nodiscard]] ReferenceProfile load(const juce::String& key) const;

        /** Retorna true si existe un perfil en cache para esta key. */
        [[nodiscard]] bool has(const juce::String& key) const;

        /** Elimina un perfil del cache. */
        void remove(const juce::String& key);

        /** Limpia TODO el cache. */
        void clearAll();

        /** Genera una cache key desde una ruta de archivo (hash SHA). */
        static juce::String makeKey(const juce::String& filePath);

        /** Retorna el directorio de cache. */
        [[nodiscard]] juce::File getCacheDirectory() const noexcept { return cacheDir_; }

    private:
        juce::File cacheDir_;
        static juce::String sanitizeKey(const juce::String& key);
    };

} // namespace mixcoach
