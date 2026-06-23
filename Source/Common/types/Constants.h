#pragma once
#include <juce_graphics/juce_graphics.h>

namespace mixcoach {

    // ─── Constantes y enums globales ─────────────────────────────────────────────

    // Límites del sistema
    inline constexpr int kMaxTracks           = 128;
    inline constexpr int kFFTSize             = 1024;
    inline constexpr int kNumSpectrumBins     = 512;
    inline constexpr double kAnalysisInterval = 0.5; // segundos

    // Niveles de referencia
    inline constexpr float kHeadroomTarget   = -6.0f;  // dB
    inline constexpr float kClipThreshold    = -0.5f;  // dB
    inline constexpr float kSilenceThreshold = -60.0f; // dB

    // ═══ 30-band spectral energy (per-track analysis via 1024-FFT @ 44100Hz) ═══
    // Each band is defined by [start_bin, end_bin) in the 512-bin spectrum.
    // Bin frequency = bin_index * 44100.0 / 1024.0 ≈ bin * 43.07 Hz
    inline constexpr int kNumSpectralBands        = 30;
    inline constexpr int kSpectralBandBins[30][2] = {
        {0, 1},     //  0: 0-43 Hz       (Sub-low)
        {1, 2},     //  1: 43-86 Hz      (Sub-bass)
        {2, 3},     //  2: 86-129 Hz     (Bass)
        {3, 5},     //  3: 129-215 Hz    (Low bass)
        {5, 7},     //  4: 215-301 Hz    (Low mids)
        {7, 10},    //  5: 301-430 Hz    (Low mids)
        {10, 14},   //  6: 430-603 Hz    (Mid-bass)
        {14, 19},   //  7: 603-818 Hz    (Mids)
        {19, 25},   //  8: 818-1076 Hz   (Mids)
        {25, 32},   //  9: 1076-1378 Hz  (Upper mids)
        {32, 40},   // 10: 1378-1722 Hz  (Upper mids)
        {40, 49},   // 11: 1722-2110 Hz  (Upper mids)
        {49, 59},   // 12: 2110-2541 Hz  (High mids)
        {59, 70},   // 13: 2541-3014 Hz  (High mids)
        {70, 82},   // 14: 3014-3532 Hz  (Presence)
        {82, 95},   // 15: 3532-4091 Hz  (Presence)
        {95, 109},  // 16: 4091-4694 Hz  (Presence)
        {109, 124}, // 17: 4694-5340 Hz  (Presence/high)
        {124, 140}, // 18: 5340-6029 Hz  (High)
        {140, 157}, // 19: 6029-6762 Hz  (High)
        {157, 175}, // 20: 6762-7537 Hz  (High)
        {175, 194}, // 21: 7537-8355 Hz  (High)
        {194, 214}, // 22: 8355-9216 Hz  (Air)
        {214, 235}, // 23: 9216-10121 Hz (Air)
        {235, 257}, // 24: 10121-11069 Hz(Air)
        {257, 280}, // 25: 11069-12060 Hz(Air)
        {280, 304}, // 26: 12060-13095 Hz(Air)
        {304, 329}, // 27: 13095-14173 Hz(Air)
        {329, 355}, // 28: 14173-15294 Hz(Air)
        {355, 382}, // 29: 15294-16458 Hz(Air)
    };

    // ═══ 30-band frequency ranges (Hz) for variable-FFT analysis (reference files) ═══
    // Matches kSpectralBandBins at 44100Hz / 1024-FFT
    inline constexpr float kSpectralBandFreqs[30][2] = {
        {0.0f, 43.0f},        {43.0f, 86.0f},       {86.0f, 129.0f},      {129.0f, 215.0f},     {215.0f, 301.0f},
        {301.0f, 430.0f},     {430.0f, 603.0f},     {603.0f, 818.0f},     {818.0f, 1076.0f},    {1076.0f, 1378.0f},
        {1378.0f, 1722.0f},   {1722.0f, 2110.0f},   {2110.0f, 2541.0f},   {2541.0f, 3014.0f},   {3014.0f, 3532.0f},
        {3532.0f, 4091.0f},   {4091.0f, 4694.0f},   {4694.0f, 5340.0f},   {5340.0f, 6029.0f},   {6029.0f, 6762.0f},
        {6762.0f, 7537.0f},   {7537.0f, 8355.0f},   {8355.0f, 9216.0f},   {9216.0f, 10121.0f},  {10121.0f, 11069.0f},
        {11069.0f, 12060.0f}, {12060.0f, 13095.0f}, {13095.0f, 14173.0f}, {14173.0f, 15294.0f}, {15294.0f, 16458.0f},
    };

    // ═══ 6 regiones espectrales (agrupa las 30-bandas en Sub, Bass, LoMid, HiMid, Pres, Air) ═══
    // Cada region se define como [bandaInicial, bandaFinal) en el array de 30-bandas.
    // Usado por CoachEngine, ReferenceDrivenEngine y ReferenceMatchPanel.
    inline constexpr int kNumRegions        = 6;
    inline constexpr int kRegionBands[6][2] = {
        {0, 2},   // Sub:      bands 0-1   (0-86Hz)
        {2, 5},   // Bass:     bands 2-4   (86-301Hz)
        {5, 10},  // Low-Mid:  bands 5-9   (301-1076Hz)
        {10, 15}, // High-Mid: bands 10-14 (1076-3532Hz)
        {15, 22}, // Presence: bands 15-21 (3532-8355Hz)
        {22, 30}  // Air:      bands 22-29 (8355-16458Hz)
    };

    // Mensajes del sistema
    inline constexpr const char* kAppName    = "MixCoach";
    inline constexpr const char* kAppVersion = "1.0.0";

    // Colores de bus virtual — alineados con UI_REFERENCES/ y visual_design.md
    inline const juce::uint32 kBusColourARGB[] = {
        0xFF8B5CF6, // Violeta  - Drums
        0xFF3B82F6, // Azul     - Bass
        0xFFF97316, // Naranja  - Guitars
        0xFF10B981, // Teal     - Keys/Synths
        0xFFEC4899, // Rosa     - Vocals
        0xFF14B8A6, // Teal FX  - FX/Ambientes
        0xFFA78BFA, // Lavanda  - Melody (piano, cuerdas, pads)
    };

    // Helper para obtener el color como juce::Colour
    inline juce::Colour getBusColour(int index)
    {
        if (index >= 0 && index < static_cast<int>(sizeof(kBusColourARGB) / sizeof(kBusColourARGB[0])))
            return juce::Colour(kBusColourARGB[index]);
        return juce::Colours::grey;
    }

} // namespace mixcoach
