#pragma once
#include <juce_core/juce_core.h>
#include "../../Common/types/Types.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  ReferenceFingerprint — Análisis espectral y de loudness de un archivo
    //  de referencia, extraído al cargar un WAV/MP3/FLAC en el ReferencePanel.
    //  El CoachEngine lo usa para comparar la mezcla actual contra la referencia.
    // ═══════════════════════════════════════════════════════════════════════════
    #define SPECTRAL_DATA_DEFINED_REFERENCEFINGERPRINT
    struct ReferenceFingerprint
    {
        // 30 bandas de frecuencia (definidas en Constants.h kSpectralBandBins / kNumSpectralBands)
        // Mismas bandas que el análisis per-track en backgroundRunLoop
        float bandEnergies[30] = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f,
                                  -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f,
                                  -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f,
                                  -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};

        // Loudness profile
        float lufsMomentary  = -100.0f;
        float lufsShortTerm  = -100.0f;
        float lufsIntegrated = -100.0f;
        float lufsRange      = 0.0f;

        // Crest factor (Peak - RMS)
        float crestFactor = 0.0f;

        // Correlación estéreo promedio
        float correlation = 0.0f;

        // True peak
        float truePeakDBTP = -100.0f;

        // Centroide espectral estimado desde bandEnergies[30] (Hz)
        float spectralCentroidHz = 0.0f;

        bool valid = false;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  SectionFingerprint — Fingerprint de una seccion individual de la
    //  referencia (ej: coro, verso). Contiene start/end time + fingerprint.
    // ═══════════════════════════════════════════════════════════════════════════
    struct SectionFingerprint
    {
        float startSeconds = 0.0f;
        float endSeconds   = 0.0f;
        ReferenceFingerprint fingerprint;
        juce::String label; // "Full", "Section 1", "Section 2", ...
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  ReferenceMatchData — Datos de comparación mix vs referencia
    //  Se computa en CoachEngine y se envía al ReferencePanelComponent
    //  para visualización del matching espectral y de loudness.
    // ═══════════════════════════════════════════════════════════════════════════
    #define SPECTRAL_DATA_DEFINED_REFERENCEMATCHDATA
    struct ReferenceMatchData
    {
        // 6 regiones espectrales (Sub, Bass, LoMid, HiMid, Pres, Air)
        float mixRegionEnergy[6] = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};
        float refRegionEnergy[6] = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};

        // LUFS
        float mixLUFS = -100.0f;
        float refLUFS = -100.0f;

        // Crest factor
        float mixCrest = 0.0f;
        float refCrest = 0.0f;

        // Correlación estéreo
        float mixCorrelation = 0.0f;
        float refCorrelation = 0.0f;

        // Nombre de la referencia activa
        juce::String refName;

        bool valid = false; // false si no hay referencia cargada o datos insuficientes
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  ReferenceMetadata — Metadatos de la referencia (archivo o URL)
    // ═══════════════════════════════════════════════════════════════════════════
    #define SPECTRAL_DATA_DEFINED_REFERENCEMETADATA
    struct ReferenceMetadata
    {
        enum class Type
        {
            None,
            File,
            URL
        };

        Type type = Type::None;
        juce::String name; // Nombre del archivo o nombre inferido de la URL
        juce::String path; // Ruta del archivo o URL limpia

        // Para URLs:
        juce::String platform; // "YouTube", "Spotify", "SoundCloud", etc.
        juce::String genre;    // Género inferido (pop, rock, edm, etc.)

        bool valid() const { return type != Type::None && name.isNotEmpty(); }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  BusGroupSummary — Métricas agregadas por familia de instrumentos
    //  Computado por computeBusSummaries() a partir de telemetría individual
    //  Agrupa tracks por BusType y calcula promedios/sumas para análisis de mezcla
    // ═══════════════════════════════════════════════════════════════════════════
    #define SPECTRAL_DATA_DEFINED_BUSGROUPSUMMARY
    struct BusGroupSummary
    {
        BusType busType           = BusType::None;
        int trackCount            = 0;
        float peakMax             = -100.0f; // Máximo peak del grupo
        float rmsSum              = -100.0f; // RMS sumado (lineal) en dB
        float avgCrest            = 0.0f;
        float avgCorrelation      = 0.0f;
        float avgBandEnergies[30] = {-100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f,
                                     -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f,
                                     -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f,
                                     -100.0f, -100.0f, -100.0f, -100.0f, -100.0f, -100.0f};
        juce::String loudestTrackName;
        float loudestTrackPeak = -100.0f;

        [[nodiscard]] bool hasData() const noexcept { return trackCount > 0 && peakMax > -90.0f; }
    };

} // namespace mixcoach
