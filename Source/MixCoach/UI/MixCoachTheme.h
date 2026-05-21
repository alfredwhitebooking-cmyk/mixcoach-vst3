#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace mixcoach {

// ─── Colores y estilos del tema visual ───────────────────────────────────────
struct MixCoachTheme
{
    // Fondo
    static juce::Colour bgDarker()   { return juce::Colour(0xFF0F0F1A); }
    static juce::Colour bgDark()     { return juce::Colour(0xFF1A1A2E); }
    static juce::Colour bgPanel()    { return juce::Colour(0xFF16213E); }

    // Texto
    static juce::Colour textPrimary(){ return juce::Colour(0xFFE0E0E0); }
    static juce::Colour textDim()    { return juce::Colour(0xFF888888); }

    // Acentuación
    static juce::Colour accent()     { return juce::Colour(0xFF3498DB); }
    static juce::Colour accentDim()  { return juce::Colour(0xFF2980B9); }

    // Estados
    static juce::Colour success()    { return juce::Colour(0xFF2ECC71); }
    static juce::Colour warning()    { return juce::Colour(0xFFF39C12); }
    static juce::Colour error()      { return juce::Colour(0xFFE74C3C); }
    static juce::Colour info()       { return juce::Colour(0xFF3498DB); }

    // Bordes
    static juce::Colour border()     { return juce::Colour(0xFF2C2C3E); }

    // Tamaños de fuente
    static constexpr float fontSizeTitle   = 22.0f;
    static constexpr float fontSizeHeader  = 16.0f;
    static constexpr float fontSizeBody    = 14.0f;
    static constexpr float fontSizeSmall   = 11.0f;
};

} // namespace mixcoach
