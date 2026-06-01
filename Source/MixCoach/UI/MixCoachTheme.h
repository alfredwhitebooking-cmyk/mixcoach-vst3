#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace mixcoach {

// ─── Colores y estilos del tema visual profesional ───────────────────────────
struct MixCoachTheme
{
    // ─── Fondos (dark glass profesional) ──────────────────────────────────
    static juce::Colour bgDarker()   { return juce::Colour(0xFF0A0A12); }  // casi negro
    static juce::Colour bgDark()     { return juce::Colour(0xFF11111E); }  // panel oscuro
    static juce::Colour bgPanel()    { return juce::Colour(0xFF181825); }  // panel principal
    static juce::Colour bgGlass()    { return juce::Colour(0xFF1E1E32); }  // efecto glass
    static juce::Colour bgSurface()  { return juce::Colour(0xFF22223A); }  // superficie elevada
    static juce::Colour bgCanvas()    { return juce::Colour(0xFF0D0D1A); }  // fondo canvas profundo

    // ─── Texto ─────────────────────────────────────────────────────────────
    static juce::Colour textPrimary(){ return juce::Colour(0xFFEEEEF0); }
    static juce::Colour textBright() { return juce::Colour(0xFFFFFFFF); }
    static juce::Colour textDim()    { return juce::Colour(0xFF7878A0); }
    static juce::Colour textMuted()  { return juce::Colour(0xFF48486A); }

    // ─── Acabados (IK Multimedia / iZotope style) ──────────────────────────
    static juce::Colour accent()     { return juce::Colour(0xFF00B4D8); }  // cyan eléctrico
    static juce::Colour accentDim()  { return juce::Colour(0xFF0096B0); }
    static juce::Colour accentGlow() { return juce::Colour(0xFF00E5FF); }  // glow

    static juce::Colour accent2()    { return juce::Colour(0xFF7B2FFF); }  // violeta
    static juce::Colour accent3()    { return juce::Colour(0xFFFF2D55); }  // rosa/rojo
    static juce::Colour accentAI()   { return juce::Colour(0xFF00E5FF); }  // cyan para marca AI
    static juce::Colour accentAIGlow(){ return juce::Colour(0xFF66F9FF); }  // glow AI más brillante

    // ─── Divisores ───────────────────────────────────────────────────────────
    static juce::Colour divider()    { return juce::Colour(0xFF2A2A4A); }  // línea divisoria

    // ─── Estados ────────────────────────────────────────────────────────────
    static juce::Colour success()    { return juce::Colour(0xFF00E676); }
    static juce::Colour warning()    { return juce::Colour(0xFFFFAB00); }
    static juce::Colour error()      { return juce::Colour(0xFFFF1744); }
    static juce::Colour info()       { return juce::Colour(0xFF00B4D8); }

    // ─── Bordes ─────────────────────────────────────────────────────────────
    static juce::Colour border()     { return juce::Colour(0xFF2A2A4A); }
    static juce::Colour borderBright(){ return juce::Colour(0xFF3A3A5A); }
    static juce::Colour dividerBar() { return juce::Colour(0xFF1A1A3A); }

    // ─── Niveles de medidor (IK Multimedia style) ──────────────────────────
    static juce::Colour meterGreen()    { return juce::Colour(0xFF00E676); }

    // ─── Colores de canal (L/R) ──────────────────────────────────────────
    static juce::Colour channelLeft()   { return juce::Colour(0xFF3498DB); }  // azul
    static juce::Colour channelRight()  { return juce::Colour(0xFF2ECC71); }  // verde
    static juce::Colour meterYellow()   { return juce::Colour(0xFFFFAB00); }
    static juce::Colour meterOrange()   { return juce::Colour(0xFFFF6D00); }
    static juce::Colour meterRed()      { return juce::Colour(0xFFFF1744); }
    static juce::Colour meterBlue()     { return juce::Colour(0xFF00B4D8); }

    // ─── Loudness colores ──────────────────────────────────────────────────
    static juce::Colour lufsMomentary() { return juce::Colour(0xFF00E676); }
    static juce::Colour lufsShort()     { return juce::Colour(0xFF00B4D8); }
    static juce::Colour lufsIntegrated(){ return juce::Colour(0xFF7B2FFF); }
    static juce::Colour truePeak()      { return juce::Colour(0xFFFF2D55); }

    // ─── Glass effects ──────────────────────────────────────────────────────
    static juce::Colour glassHighlight(){ return juce::Colours::white.withAlpha(0.04f); }
    static juce::Colour glassShadow()   { return juce::Colours::black.withAlpha(0.3f); }

    // ─── Métodos helpers para gradientes ─────────────────────────────────────
    static void fillGlassPanel(juce::Graphics& g, juce::Rectangle<float> bounds, float radius = 4.0f)
    {
        // Fondo oscuro
        g.setColour(bgPanel());
        g.fillRoundedRectangle(bounds, radius);

        // Highlight superior (glass effect)
        auto highlight = bounds.withHeight(bounds.getHeight() * 0.4f);
        juce::ColourGradient topGrad(
            glassHighlight(),
            juce::Point<float>(0.0f, highlight.getY()),
            juce::Colour(0x00000000),
            juce::Point<float>(0.0f, highlight.getBottom()),
            false);
        g.setGradientFill(topGrad);
        g.fillRoundedRectangle(highlight, radius);

        // Borde sutil
        g.setColour(border().withAlpha(0.5f));
        g.drawRoundedRectangle(bounds, radius, 1.0f);
    }

    static void drawGradientMeter(juce::Graphics& g, juce::Rectangle<float> bounds,
                                   float level, bool vertical = true,
                                   float radius = 2.0f)
    {
        // Fondo del meter
        g.setColour(bgDarker());
        g.fillRoundedRectangle(bounds, radius);

        // Calcular fill
        float fill;
        if (vertical)
            fill = juce::jlimit(0.0f, 1.0f, level);
        else
            fill = juce::jlimit(0.0f, 1.0f, level);

        if (fill <= 0.0f) return;

        // Color por nivel
        juce::Colour meterColour;
        if (fill > 0.85f)      meterColour = meterRed();
        else if (fill > 0.7f)  meterColour = meterOrange();
        else if (fill > 0.5f)  meterColour = meterYellow();
        else                   meterColour = meterGreen();

        auto fillBounds = vertical
            ? bounds.withTop(bounds.getBottom() - bounds.getHeight() * fill)
            : bounds.withWidth(bounds.getWidth() * fill);

        g.setColour(meterColour);
        g.fillRoundedRectangle(fillBounds, radius);

        // Glow superior
        auto glowBounds = fillBounds.withHeight(juce::jmax(2.0f, fillBounds.getHeight() * 0.15f));
        juce::ColourGradient glow(
            juce::Colours::white.withAlpha(0.3f),
            juce::Point<float>(glowBounds.getCentreX(), glowBounds.getY()),
            juce::Colour(0x00000000),
            juce::Point<float>(glowBounds.getCentreX(), glowBounds.getBottom()),
            false);
        g.setGradientFill(glow);
        g.fillRoundedRectangle(glowBounds, radius);
    }

    // ─── Tamaños de fuente ──────────────────────────────────────────────────
    static constexpr float fontSizeTitle   = 20.0f;
    static constexpr float fontSizeHeader  = 15.0f;
    static constexpr float fontSizeBody    = 13.0f;
    static constexpr float fontSizeSmall   = 10.0f;
    static constexpr float fontSizeTiny    = 8.0f;
};

} // namespace mixcoach
