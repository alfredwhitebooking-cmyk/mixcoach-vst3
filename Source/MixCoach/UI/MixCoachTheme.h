#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Common/types/Constants.h"

// ─── Font helper ───────────────────────────────────────────────────────
// ⚠ NO usar fuentes personalizadas ("Inter", "Roboto", etc.). FL Studio
//    (Delphi) crashea al serializar propiedades TQuickFont.Name cuando
//    detecta nombres de fuente desconocidos.
static juce::Font interFont(float size)
{
    return juce::Font(juce::FontOptions(size));
}

namespace mixcoach {

    // ===========================================================================
    //  MIXCOACH THEME — PALETA DEFINITIVA Y GRADIENTES DIGITALES PREMIUM
    // ===========================================================================
    struct MixCoachTheme
    {
        // ─── Backgrounds (Chasis de Aluminio y Metal Anodizado) ────────────────
        static juce::Colour bgCanvas() { return juce::Colour(0xFF04060A); }

        static juce::Colour bgMessenger() { return juce::Colour(0xFF05070D); }

        static juce::Colour bgDarker() { return juce::Colour(0xFF060A10); }

        static juce::Colour bgDark() { return juce::Colour(0xFF0A1018); }

        static juce::Colour bgPanel() { return juce::Colour(0xFF0E1420); }

        static juce::Colour bgPanelTab1() { return juce::Colour(0xFF091018); }

        static juce::Colour bgPanelTab2() { return juce::Colour(0xFF0A1018); }

        static juce::Colour bgCard() { return juce::Colour(0xFF090D15); }

        static juce::Colour bgSurface() { return juce::Colour(0xFF101F30); }

        static juce::Colour bgInput() { return juce::Colour(0xFF08101A); }

        static juce::Colour rowDivider() { return juce::Colour(0xFF0E1A2B); }

        // ─── Premium Mastering Suite Gradients ─────────────────────────────────
        static juce::Colour gradientDark() { return juce::Colour(0xFF03060A); }

        static juce::Colour gradientMid() { return juce::Colour(0xFF0A1428); }

        static juce::Colour gradientLight() { return juce::Colour(0xFF121E36); }

        static juce::ColourGradient getGradientBackground(float height = 600.0f)
        {
            juce::ColourGradient grad;
            grad.point1 = juce::Point<float>(0, 0);
            grad.point2 = juce::Point<float>(0, height);
            grad.addColour(0.0, gradientDark());
            grad.addColour(0.5, gradientMid());
            grad.addColour(1.0, gradientLight());
            return grad;
        }

        // ─── Text (Tipografía nítida y jerarquizada) ───────────────────────────
        static juce::Colour textPrimary() { return juce::Colour(0xFFFFFFFF); }

        static juce::Colour textSecondary() { return juce::Colour(0xFFCBD5E1); }

        static juce::Colour textBright() { return juce::Colour(0xFFF8FAFC); }

        static juce::Colour textDim() { return juce::Colour(0xFF94A3B8); }

        static juce::Colour textMuted() { return juce::Colour(0xFF64748B); }

        static juce::Colour textAccent() { return juce::Colour(0xFF9D8EC4); } // Lavanda de la referencia

        // ─── Brand / Accents (Identidad Visual) ────────────────────────────────
        static juce::Colour accent() { return juce::Colour(0xFFA855F7); } // Púrpura Premium vibrante

        static juce::Colour accentDim() { return accent().darker(0.3f); } // ~#7537C5 — derivado armónico

        static juce::Colour accentGlow() { return accent().brighter(0.15f); } // ~#C084FC — resplandor lavanda

        static juce::Colour accentBg() { return juce::Colour(0x1AA855F7); }

        static juce::Colour accentNeon() { return juce::Colour(0xFF00E5FF); }

        // ─── Analyzer / Spectrum Colors ────────────────────────────────────────
        static juce::Colour accentCyan() { return juce::Colour(0xFF00B7FF); }

        static juce::Colour accentCyanBright() { return juce::Colour(0xFF5CE1FF); }

        static juce::Colour accentCyanDim() { return juce::Colour(0xFF0086BC); }

        // ─── Dividers ──────────────────────────────────────────────────────────
        static juce::Colour divider() { return juce::Colour(0xFF1E293B); }

        static juce::Colour dividerBar() { return juce::Colour(0xFF111827); }

        // ─── Status Colors ─────────────────────────────────────────────────────
        static juce::Colour success() { return juce::Colour(0xFF10B981); }

        static juce::Colour warning() { return juce::Colour(0xFFF59E0B); }

        static juce::Colour error() { return juce::Colour(0xFFEF4444); }

        static juce::Colour info() { return juce::Colour(0xFF3B82F6); }

        static juce::Colour masterBus() { return juce::Colour(0xFFEF4444); }

        // ─── Borders ───────────────────────────────────────────────────────────
        static juce::Colour border() { return juce::Colour(0xFF334155).withAlpha(0.25f); }

        static juce::Colour borderBright() { return juce::Colour(0xFF475569).withAlpha(0.40f); }

        static juce::Colour borderCard() { return juce::Colour(0xFF1E293B); }

        // ─── Digital Meter Colors (Escala balística lineal para gradientes suaves)
        static juce::Colour meterGreen() { return juce::Colour(0xFF04D939); } // Verde neón limpio

        static juce::Colour meterLime() { return juce::Colour(0xFF86EF4D); } // Transición suave a amarillo

        static juce::Colour meterYellow() { return juce::Colour(0xFFFFD400); } // Amarillo técnico de advertencia

        static juce::Colour meterOrange() { return juce::Colour(0xFFFF7A00); } // Naranja pre-clipping

        static juce::Colour meterRed() { return juce::Colour(0xFFFF2222); } // Rojo clipping sólido

        static juce::Colour meterBlue() { return juce::Colour(0xFF2563EB); }

        // ─── Channel Colors (L/R) ──────────────────────────────────────────────
        static juce::Colour channelLeft() { return juce::Colour(0xFF3B82F6); }

        static juce::Colour channelRight() { return juce::Colour(0xFF10B981); }

        // ─── Loudness ──────────────────────────────────────────────────────────
        static juce::Colour lufsMomentary() { return accentCyan(); }

        static juce::Colour lufsShort() { return accentCyanDim(); }

        static juce::Colour lufsIntegrated() { return accent(); }

        static juce::Colour truePeak() { return error(); }

        // ─── VU Meter Colors (Hardware Analógico Premium) ──────────────────────
        static juce::Colour vuFace() { return juce::Colour(0xFFEED087); } // Papel ámbar vintage central

        static juce::Colour vuNeedle() { return juce::Colour(0xFF111111); } // Negro carbón mecánico

        static juce::Colour vuRed() { return juce::Colour(0xFFD32F2F); } // Rojo carmín técnico calibrado

        static juce::Colour vuText() { return juce::Colour(0xFF1A1A1A); }

        static juce::Colour vuTextRed() { return juce::Colour(0xFFD32F2F); }

        static juce::Colour vuBg() { return juce::Colour(0xFF0D0D0D); } // Fondo del chasis negro profundo

        static juce::Colour vuBorder() { return juce::Colour(0xFF151515); }

        // ─── Glass / Panel Effects ─────────────────────────────────────────────
        static juce::Colour glassHighlight() { return juce::Colours::white.withAlpha(0.02f); }

        static juce::Colour glassShadow() { return juce::Colours::black.withAlpha(0.50f); }

        static juce::Colour glassEdge() { return juce::Colours::white.withAlpha(0.04f); }

        static juce::Colour busColour(int busIndex) { return getBusColour(busIndex); }

        static juce::Font sectionHeaderFont()
        {
            return juce::Font(juce::FontOptions(fontSizeSectionHeader)).boldened();
        }

        static void drawSectionHeader(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& text)
        {
            g.setFont(sectionHeaderFont());
            g.setColour(accentGlow());
            g.drawText(text, bounds, juce::Justification::centredLeft);
        }

        static void fillGlassPanel(juce::Graphics& g, juce::Rectangle<float> bounds, float radius = 6.0f)
        {
            // Drop shadow exterior del panel
            g.setColour(glassShadow().withAlpha(0.35f));
            g.fillRoundedRectangle(bounds.translated(0.0f, 2.0f), radius);

            // Fondo oscuro sólido
            g.setColour(bgDark());
            g.fillRoundedRectangle(bounds, radius);

            // Gradiente interior superior (iluminación sutil)
            auto highlight = bounds.withHeight(bounds.getHeight() * 0.25f);
            juce::ColourGradient topGrad(glassHighlight(),
                                         juce::Point<float>(0.0f, highlight.getY()),
                                         juce::Colours::transparentBlack,
                                         juce::Point<float>(0.0f, highlight.getBottom()),
                                         false);
            g.setGradientFill(topGrad);
            g.fillRoundedRectangle(highlight, radius);

            // Borde fino perimetral de precisión
            g.setColour(juce::Colour(0xFF2E3A4E).withAlpha(0.4f));
            g.drawRoundedRectangle(bounds, radius, 1.0f);
        }

        // ─── Rediseño del Medidor Digital: Multi-stop Líquido de Alta Fidelidad ───
        static void drawGradientMeter(
            juce::Graphics& g, juce::Rectangle<float> bounds, float level, bool vertical = true, float radius = 1.5f)
        {
            // Fondo del riel del medidor (Vacío)
            g.setColour(bgDarker());
            if (radius > 0.0f) g.fillRoundedRectangle(bounds, radius);
            else
                g.fillRect(bounds);

            float fill = juce::jlimit(0.0f, 1.0f, level);
            if (fill <= 0.0f) return;

            // Calcular el rectángulo de llenado dinámico
            auto fillBounds = vertical ? bounds.withTop(bounds.getBottom() - bounds.getHeight() * fill)
                                       : bounds.withWidth(bounds.getWidth() * fill);

            // Inyección de Gradiente Multi-Stop Fluido (Look moderno estilo iZotope/FabFilter)
            juce::ColourGradient meterGrad;
            if (vertical) {
                meterGrad = juce::ColourGradient(meterGreen(),
                                                 bounds.getCentreX(),
                                                 bounds.getBottom(),
                                                 meterRed(),
                                                 bounds.getCentreX(),
                                                 bounds.getY(),
                                                 false);

                meterGrad.addColour(0.55, meterLime());
                meterGrad.addColour(0.75, meterYellow());
                meterGrad.addColour(0.88, meterOrange());
            }
            else {
                meterGrad = juce::ColourGradient(meterGreen(),
                                                 bounds.getX(),
                                                 bounds.getCentreY(),
                                                 meterRed(),
                                                 bounds.getRight(),
                                                 bounds.getCentreY(),
                                                 false);

                meterGrad.addColour(0.55, meterLime());
                meterGrad.addColour(0.75, meterYellow());
                meterGrad.addColour(0.88, meterOrange());
            }

            g.setGradientFill(meterGrad);

            g.saveState();
            g.reduceClipRegion(fillBounds.toNearestInt());
            if (radius > 0.0f) g.fillRoundedRectangle(bounds, radius);
            else
                g.fillRect(bounds);
            g.restoreState();

            // Resplandor (Glow) de intensidad en el frente de la barra
            auto glowBounds =
                vertical
                    ? fillBounds.withHeight(juce::jmin(3.0f, fillBounds.getHeight()))
                    : fillBounds.withWidth(juce::jmin(3.0f, fillBounds.getWidth())).withX(fillBounds.getRight() - 3.0f);

            g.setColour(juce::Colours::white.withAlpha(0.35f));
            if (radius > 0.0f) g.fillRoundedRectangle(glowBounds, radius);
            else
                g.fillRect(glowBounds);
        }

        // ─── Font sizes ───────────────────────────────────────────────────────
        static constexpr float fontSizeTitle         = 18.0f;
        static constexpr float fontSizeHeader        = 13.0f;
        static constexpr float fontSizeBody          = 12.0f;
        static constexpr float fontSizeSmall         = 10.0f;
        static constexpr float fontSizeTiny          = 8.0f;
        static constexpr float fontSizeSectionHeader = 11.0f;
        static constexpr float fontSizeExtraTiny     = 8.0f; // Footer labels, status bars, hints (floor 8px)
        static constexpr float fontSizeMicro         = 8.0f; // Tags, timestamps, badges, role pills (floor 8px)
        static constexpr float fontSizeNano          = 8.0f; // Toolbar chips, compact badges, crest labels (floor 8px)
        static constexpr float fontSizePico          = 8.0f; // Ultra-compact (confidence icons, tick labels) (floor 8px)

        // ─── Role category colours ───────────────────────────────────────────
        static juce::Colour roleDrums() { return juce::Colour(0xFF8B5CF6); } // Purple

        static juce::Colour roleBass() { return juce::Colour(0xFF3B82F6); } // Blue

        static juce::Colour roleGuitars() { return juce::Colour(0xFFF97316); } // Orange

        static juce::Colour roleKeys() { return juce::Colour(0xFF10B981); } // Green/teal

        static juce::Colour roleVocals() { return juce::Colour(0xFFEC4899); } // Pink

        static juce::Colour roleFX() { return juce::Colour(0xFF14B8A6); } // Teal

        static juce::Colour roleMelody() { return juce::Colour(0xFFA78BFA); } // Lavender

        static juce::Colour roleUnknown() { return juce::Colour(0xFF5C5F73); } // Grey

        static juce::Colour roleColourForCategory(int category) noexcept
        {
            switch (category) {
                case 0:
                    return roleDrums();
                case 1:
                    return roleBass();
                case 2:
                    return roleGuitars();
                case 3:
                    return roleKeys();
                case 4:
                    return roleVocals();
                case 5:
                    return roleFX();
                case 6:
                    return roleMelody();
                default:
                    return roleUnknown();
            }
        }

        // ─── Spectrum / Region colours (6-band: Sub→Air) ────────────────────
        static juce::Colour specSub() { return juce::Colour(0xFF8B5CF6); } // Violet

        static juce::Colour specBass() { return juce::Colour(0xFF3B82F6); } // Blue

        static juce::Colour specLoMid() { return juce::Colour(0xFF10B981); } // Green

        static juce::Colour specHiMid() { return juce::Colour(0xFFF59E0B); } // Amber

        static juce::Colour specPres() { return juce::Colour(0xFFF97316); } // Orange

        static juce::Colour specAir() { return juce::Colour(0xFFEF4444); } // Red

        static juce::Colour specColour(int band) noexcept
        {
            switch (band) {
                case 0:
                    return specSub();
                case 1:
                    return specBass();
                case 2:
                    return specLoMid();
                case 3:
                    return specHiMid();
                case 4:
                    return specPres();
                case 5:
                    return specAir();
                default:
                    return accentGlow();
            }
        }

        // ─── Tooltip colours ────────────────────────────────────────────────
        static juce::Colour tooltipBg() { return juce::Colour(0xFF1A1A2E).withAlpha(0.97f); }

        static juce::Colour tooltipBorder() { return juce::Colour(0xFF2E2F3E).withAlpha(0.60f); }

        static juce::Colour tooltipText() { return juce::Colour(0xFFD1D1E0); }

        static juce::Colour tooltipTextDim() { return juce::Colour(0xFF8B8FA3); }

        static juce::Colour tooltipTextMuted() { return juce::Colour(0xFF5C5F73); }

        // ─── Spacing tokens ──────────────────────────────────────────────────
        // Tokens de espaciado unificados. Reemplazan raw integers (2, 4, 6, 8, 12, 16, 20, 24)
        // en resized()/paint() y layout de todos los componentes UI.
        //
        // Jerarquía: XXS → XS → SM → MD → LG → XL → XXL → XXXL
        static constexpr int spacingXXS  = 2;
        static constexpr int spacingXS   = 4;
        static constexpr int spacingSM   = 6;
        static constexpr int spacingMD   = 8;
        static constexpr int spacingLG   = 12;
        static constexpr int spacingXL   = 16;
        static constexpr int spacingXXL  = 20;
        static constexpr int spacingXXXL = 24;

        // ─── Corner radius tokens ────────────────────────────────────────────
        static constexpr float cornerRadius_small  = 4.0f;  // Buttons, pills, badges
        static constexpr float cornerRadius_medium = 6.0f;  // Cards, panels
        static constexpr float cornerRadius_large  = 8.0f;  // Dialog, tooltip
        static constexpr float cornerRadius_pill   = 12.0f; // Pill-shaped (tall/narrow)

        // ─── Card/Layout tokens ──────────────────────────────────────────────
        // Alturas de cards y gaps para listas de tracks. Dos valores distintos
        // según el nivel de detalle de cada vista.
        static constexpr int cardHeight_list      = 46; // Messenger track list
        static constexpr int cardHeight_dashboard = 38; // TrackDashboard summary
        static constexpr int cardGap              = 3;  // Gap entre cards
        static constexpr int headerHeight         = 22; // Bus header height
        static constexpr int titleHeight          = 22; // Section title height
        static constexpr int cardCorner           = 6;  // Card corner radius (int)
    };

    // ─── Global text colour helpers (formerly local palettes in various files) ──
    // These replace the local kTextBright, kTextDim, kTextMuted constants
    // found in MessengerListDrawing.cpp and other files by providing the
    // SAME hex values so the visual appearance doesn't change.
    inline juce::Colour textBright()
    {
        return juce::Colour(0xFFF1F1F6);
    }

    inline juce::Colour textDim()
    {
        return juce::Colour(0xFF8B8FA3);
    }

    inline juce::Colour textMuted()
    {
        return juce::Colour(0xFF5C5F73);
    }

} // namespace mixcoach
