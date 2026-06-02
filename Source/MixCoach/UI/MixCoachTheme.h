#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Common/types/Constants.h"

namespace mixcoach {

// ─── Colores y estilos del tema visual profesional ───────────────────────────
//  Fuente de verdad: UI_REFERENCES/ + workspace_memory/visual_design.md
struct MixCoachTheme
{
    // ─── Fondos (dark theme referencia) ───────────────────────────────────
    static juce::Colour bgCanvas()    { return juce::Colour(0xFF0A0A0F); }
    static juce::Colour bgDarker()    { return juce::Colour(0xFF0F0F18); }
    static juce::Colour bgDark()      { return juce::Colour(0xFF141420); }
    static juce::Colour bgPanel()     { return juce::Colour(0xFF1A1A2E); }
    static juce::Colour bgSurface()   { return juce::Colour(0xFF1E1F2C); }
    static juce::Colour bgCard()      { return juce::Colour(0xFF222332); }
    static juce::Colour bgInput()     { return juce::Colour(0xFF12131E); }
    static juce::Colour rowDivider()  { return juce::Colour(0xFF1A1A2E); }

    // ─── Texto ─────────────────────────────────────────────────────────────
    static juce::Colour textPrimary() { return juce::Colour(0xFFF1F1F6); }
    static juce::Colour textBright()  { return juce::Colour(0xFFFFFFFF); }
    static juce::Colour textDim()     { return juce::Colour(0xFFA1A1AA); }
    static juce::Colour textMuted()   { return juce::Colour(0xFF6B7280); }
    static juce::Colour textAccent()  { return juce::Colour(0xFFA78BFA); }

    // ─── Marca y acentos ───────────────────────────────────────────────────
    static juce::Colour accent()      { return juce::Colour(0xFF7C3AED); }
    static juce::Colour accentDim()   { return juce::Colour(0xFF6D28D9); }
    static juce::Colour accentGlow()  { return juce::Colour(0xFFA78BFA); }
    static juce::Colour accentBg()    { return juce::Colour(0x147C3AED); }

    static juce::Colour accent2()     { return juce::Colour(0xFF8B5CF6); }
    static juce::Colour accent3()     { return juce::Colour(0xFFEC4899); }
    static juce::Colour accentAI()    { return juce::Colour(0xFFA78BFA); }
    static juce::Colour accentAIGlow(){ return juce::Colour(0xFFC4B5FD); }

    /** Sugerencias IA, barras LUFS MDR */
    static juce::Colour accentCyan()       { return juce::Colour(0xFF00B4D8); }
    /** Espectro RTA (gradiente alto) */
    static juce::Colour accentCyanBright() { return juce::Colour(0xFF00E5FF); }

    // ─── Divisores ───────────────────────────────────────────────────────────
    static juce::Colour divider()     { return juce::Colour(0xFF25262E); }
    static juce::Colour dividerBar()  { return juce::Colour(0xFF1E1F2A); }

    // ─── Estados ────────────────────────────────────────────────────────────
    static juce::Colour success()     { return juce::Colour(0xFF22C55E); }
    static juce::Colour warning()     { return juce::Colour(0xFFEAB308); }
    static juce::Colour error()       { return juce::Colour(0xFFEF4444); }
    static juce::Colour info()        { return juce::Colour(0xFF3B82F6); }
    static juce::Colour masterBus()   { return juce::Colour(0xFFEF4444); }

    // ─── Bordes ─────────────────────────────────────────────────────────────
    static juce::Colour border()      { return juce::Colour(0xFF2A2B38); }
    static juce::Colour borderBright(){ return juce::Colour(0xFF3A3B4A); }
    static juce::Colour borderCard()  { return juce::Colour(0xFF2E2F3E); }

    // ─── Niveles de medidor ──────────────────────────────────────────────
    static juce::Colour meterGreen()    { return juce::Colour(0xFF22C55E); }
    static juce::Colour meterLime()     { return juce::Colour(0xFF84CC16); }
    static juce::Colour meterYellow()   { return juce::Colour(0xFFEAB308); }
    static juce::Colour meterOrange()   { return juce::Colour(0xFFF97316); }
    static juce::Colour meterRed()      { return juce::Colour(0xFFEF4444); }
    static juce::Colour meterBlue()     { return juce::Colour(0xFF3B82F6); }

    // ─── Colores de canal (L/R) ──────────────────────────────────────────
    static juce::Colour channelLeft()   { return juce::Colour(0xFF60A5FA); }
    static juce::Colour channelRight()  { return juce::Colour(0xFF34D399); }

    // ─── Loudness ──────────────────────────────────────────────────────────
    static juce::Colour lufsMomentary() { return accentCyan(); }
    static juce::Colour lufsShort()     { return juce::Colour(0xFF3B82F6); }
    static juce::Colour lufsIntegrated(){ return juce::Colour(0xFF7C3AED); }
    static juce::Colour truePeak()      { return juce::Colour(0xFFEF4444); }

    // ─── Glass effects ──────────────────────────────────────────────────────
    static juce::Colour glassHighlight(){ return juce::Colours::white.withAlpha(0.03f); }
    static juce::Colour glassShadow()   { return juce::Colours::black.withAlpha(0.25f); }
    static juce::Colour glassEdge()     { return juce::Colours::white.withAlpha(0.05f); }

    /** Colores de bus desde Constants.h (índice = BusType cuando >= 0). */
    static juce::Colour busColour(int busIndex)
    {
        return getBusColour(busIndex);
    }

    static juce::Font sectionHeaderFont()
    {
        return juce::Font(juce::FontOptions(fontSizeSectionHeader)).boldened();
    }

    static void drawSectionHeader(juce::Graphics& g, juce::Rectangle<int> bounds,
                                const juce::String& text)
    {
        g.setFont(sectionHeaderFont());
        g.setColour(accentGlow());
        g.drawText(text, bounds, juce::Justification::centredLeft);
    }

    static void fillGlassPanel(juce::Graphics& g, juce::Rectangle<float> bounds, float radius = 6.0f)
    {
        auto shadowBounds = bounds.expanded(2.0f);
        g.setColour(glassShadow().withAlpha(0.15f));
        g.fillRoundedRectangle(shadowBounds, radius + 1.0f);

        g.setColour(bgPanel());
        g.fillRoundedRectangle(bounds, radius);

        auto highlight = bounds.withHeight(bounds.getHeight() * 0.35f);
        juce::ColourGradient topGrad(
            glassHighlight(),
            juce::Point<float>(0.0f, highlight.getY()),
            juce::Colour(0x00000000),
            juce::Point<float>(0.0f, highlight.getBottom()),
            false);
        g.setGradientFill(topGrad);
        g.fillRoundedRectangle(highlight.toFloat(), radius);

        g.setColour(border().withAlpha(0.4f));
        g.drawRoundedRectangle(bounds, radius, 1.0f);

        g.setColour(glassEdge().withAlpha(0.3f));
        g.drawHorizontalLine((int)bounds.getY() + 1,
                             (int)bounds.getX() + 4,
                             (int)bounds.getRight() - 4);
    }

    static void drawGradientMeter(juce::Graphics& g, juce::Rectangle<float> bounds,
                                   float level, bool vertical = true,
                                   float radius = 2.0f)
    {
        g.setColour(bgDarker());
        g.fillRoundedRectangle(bounds, radius);

        float fill = juce::jlimit(0.0f, 1.0f, level);
        if (fill <= 0.0f) return;

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

        auto glowBounds = fillBounds.withHeight(juce::jmax(2.0f, fillBounds.getHeight() * 0.12f));
        juce::ColourGradient glow(
            juce::Colours::white.withAlpha(0.25f),
            juce::Point<float>(glowBounds.getCentreX(), glowBounds.getY()),
            juce::Colour(0x00000000),
            juce::Point<float>(glowBounds.getCentreX(), glowBounds.getBottom()),
            false);
        g.setGradientFill(glow);
        g.fillRoundedRectangle(glowBounds, radius);
    }

    static constexpr float fontSizeTitle          = 18.0f;
    static constexpr float fontSizeHeader         = 13.0f;
    static constexpr float fontSizeBody           = 12.0f;
    static constexpr float fontSizeSmall          = 10.0f;
    static constexpr float fontSizeTiny           = 8.0f;
    static constexpr float fontSizeSectionHeader  = 11.0f;
};

} // namespace mixcoach
