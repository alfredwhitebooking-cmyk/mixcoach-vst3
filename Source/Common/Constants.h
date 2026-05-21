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

// Colores de bus virtual
inline const juce::Colour kBusColours[] = {
    juce::Colour(0xFFE74C3C),  // Rojo     - Drums
    juce::Colour(0xFF3498DB),  // Azul     - Bass
    juce::Colour(0xFF2ECC71),  // Verde    - Guitarras
    juce::Colour(0xFFF39C12),  // Naranja  - Teclados
    juce::Colour(0xFF9B59B6),  // Púrpura  - Vocals
    juce::Colour(0xFF1ABC9C),  // Turquesa - FX/Ambientes
};

} // namespace mixcoach
