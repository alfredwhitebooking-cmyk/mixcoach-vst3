#pragma once
#include <juce_graphics/juce_graphics.h>

namespace mixcoach {

// ─── Constantes y enums globales ─────────────────────────────────────────────

// Límites del sistema
inline constexpr int    kMaxTracks        = 64;
inline constexpr int    kFFTSize          = 1024;
inline constexpr int    kNumSpectrumBins  = 512;
inline constexpr double kAnalysisInterval = 0.5;   // segundos

// Niveles de referencia
inline constexpr float  kHeadroomTarget   = -6.0f; // dB
inline constexpr float  kClipThreshold    = -0.5f; // dB
inline constexpr float  kSilenceThreshold = -60.0f;// dB

// Mensajes del sistema
inline constexpr const char* kAppName    = "MixCoach";
inline constexpr const char* kAppVersion = "1.0.0";

// Colores de bus virtual — alineados con UI_REFERENCES/ y visual_design.md
inline const juce::uint32 kBusColourARGB[] = {
    0xFF8B5CF6,  // Violeta  - Drums
    0xFF3B82F6,  // Azul     - Bass
    0xFFF97316,  // Naranja  - Guitars
    0xFF10B981,  // Teal     - Keys/Synths
    0xFFEC4899,  // Rosa     - Vocals
    0xFF14B8A6,  // Teal FX  - FX/Ambientes
};

// Helper para obtener el color como juce::Colour
inline juce::Colour getBusColour(int index) {
    if (index >= 0 && index < static_cast<int>(sizeof(kBusColourARGB) / sizeof(kBusColourARGB[0])))
        return juce::Colour(kBusColourARGB[index]);
    return juce::Colours::grey;
}

} // namespace mixcoach
