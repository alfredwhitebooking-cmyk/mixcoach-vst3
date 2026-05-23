#pragma once
#include <juce_graphics/juce_graphics.h>

namespace mixcoach {

// ─── Constantes y enums globales ─────────────────────────────────────────────

// Límites del sistema
inline constexpr int    kMaxTracks        = 64;
inline constexpr int    kFFTSize          = 512;
inline constexpr int    kNumSpectrumBins  = 256;
inline constexpr double kAnalysisInterval = 0.5;   // segundos

// Niveles de referencia
inline constexpr float  kHeadroomTarget   = -6.0f; // dB
inline constexpr float  kClipThreshold    = -0.5f; // dB
inline constexpr float  kSilenceThreshold = -60.0f;// dB

// Mensajes del sistema
inline constexpr const char* kAppName    = "MixCoach";
inline constexpr const char* kAppVersion = "1.0.0";

// Colores de bus virtual (usar uint32 y convertir a Colour en runtime)
// Nota: no se puede usar constexpr con juce::Colour porque su constructor no es constexpr
inline const juce::uint32 kBusColourARGB[] = {
    0xFFE74C3C,  // Rojo     - Drums
    0xFF3498DB,  // Azul     - Bass
    0xFF2ECC71,  // Verde    - Guitarras
    0xFFF39C12,  // Naranja  - Teclados
    0xFF9B59B6,  // Púrpura  - Vocals
    0xFF1ABC9C,  // Turquesa - FX/Ambientes
};

// Helper para obtener el color como juce::Colour
inline juce::Colour getBusColour(int index) {
    if (index >= 0 && index < static_cast<int>(sizeof(kBusColourARGB) / sizeof(kBusColourARGB[0])))
        return juce::Colour(kBusColourARGB[index]);
    return juce::Colours::grey;
}

} // namespace mixcoach
