#include "StereoWidthMeter.h"
#include <algorithm>
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor
    // ═══════════════════════════════════════════════════════════════════════════
    StereoWidthMeter::StereoWidthMeter() {}

    // ═══════════════════════════════════════════════════════════════════════════
    //  resized
    // ═══════════════════════════════════════════════════════════════════════════
    void StereoWidthMeter::resized()
    {
        // Layout se calcula en paint() — no se necesita almacenar bounds aquí
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setAvgWidth
    // ═══════════════════════════════════════════════════════════════════════════
    void StereoWidthMeter::setAvgWidth(float width)
    {
        rawWidth_ = juce::jlimit(0.0f, 1.0f, width);
        widthSmooth_.setTargetValue(rawWidth_);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setPerBandWidth
    // ═══════════════════════════════════════════════════════════════════════════
    void StereoWidthMeter::setPerBandWidth(const float* perBand)
    {
        if (perBand == nullptr) {
            hasPerBandData_ = false;
            return;
        }
        hasPerBandData_ = true;
        for (int b = 0; b < kNumBands; ++b) perBandValues_[b] = juce::jlimit(0.0f, 1.0f, perBand[b]);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  advanceVisuals
    // ═══════════════════════════════════════════════════════════════════════════
    bool StereoWidthMeter::advanceVisuals(double sr, bool allowRepaint)
    {
        bool dirty = widthSmooth_.advance(sr);
        if (dirty && allowRepaint) repaint();
        return dirty;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getZone — Retorna la zona correspondiente al valor de ancho
    // ═══════════════════════════════════════════════════════════════════════════
    const StereoWidthMeter::WidthZone& StereoWidthMeter::getZone(float width) const noexcept
    {
        for (int i = 0; i < kNumZones; ++i) {
            if (width >= zones_[i].minNorm && width < zones_[i].maxNorm) return zones_[i];
        }
        return zones_[kNumZones - 1];
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getContextTip — Retorna el tip de la zona actual
    // ═══════════════════════════════════════════════════════════════════════════
    const char* StereoWidthMeter::getContextTip(float width) const noexcept
    {
        return getZone(width).tip;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint
    // ═══════════════════════════════════════════════════════════════════════════
    void StereoWidthMeter::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        auto area   = bounds;

        // ─── Background glass panel ─────────────────────────────────────────
        MixCoachTheme::fillGlassPanel(g, bounds, 4.0f);

        // ─── Header ─────────────────────────────────────────────────────────
        auto headerArea = area.removeFromTop(18).reduced(4, 0);
        drawHeader(g, headerArea);

        // ─── Barra principal ────────────────────────────────────────────────
        float barH   = juce::jmin(22.0f, area.getHeight() * 0.35f);
        auto barArea = area.removeFromTop(barH).reduced(2, 1);
        drawBar(g, barArea);

        // ─── Tip contextual ─────────────────────────────────────────────────
        float tipH   = juce::jmin(15.0f, area.getHeight() * 0.18f);
        auto tipArea = area.removeFromTop(tipH).reduced(4, 0);
        drawTip(g, tipArea);

        // ─── Per-band breakdown ─────────────────────────────────────────────
        auto perBandArea = area.reduced(4, 2);
        if (hasPerBandData_) drawPerBand(g, perBandArea);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawHeader — Título + badge de porcentaje + ícono de zona
    // ═══════════════════════════════════════════════════════════════════════════
    void StereoWidthMeter::drawHeader(juce::Graphics& g, juce::Rectangle<float> area)
    {
        float curWidth   = widthSmooth_.getCurrent();
        const auto& zone = getZone(curWidth);

        // ─── Título con emoji y nombre ───────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
        g.setColour(MixCoachTheme::accent().withAlpha(0.85f));
        g.drawText(juce::String::fromUTF8("\xF0\x9F\x8E\xA7 ANCHO ESTEREO"), area, juce::Justification::centredLeft);

        // ─── Badge: porcentaje + nombre de zona + ícono ──────────────────────
        auto badgeArea        = area.removeFromRight(80);
        juce::Colour badgeCol = zone.colour;

        g.setColour(badgeCol.withAlpha(0.12f));
        g.fillRoundedRectangle(badgeArea, 4.0f);
        g.setColour(badgeCol.withAlpha(0.35f));
        g.drawRoundedRectangle(badgeArea, 4.0f, 0.6f);

        g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
        g.setColour(badgeCol);
        g.drawText(juce::String(static_cast<int>(curWidth * 100.0f)) + "%",
                   badgeArea.removeFromLeft(30),
                   juce::Justification::centredLeft);

        // Zone name + icon
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
        g.setColour(badgeCol.withAlpha(0.8f));
        g.drawText(
            juce::String(zone.icon) + " " + juce::String(zone.label), badgeArea, juce::Justification::centredLeft);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawBar — Barra con 4 zonas + marker animado + escala 0%-100%
    // ═══════════════════════════════════════════════════════════════════════════
    void StereoWidthMeter::drawBar(juce::Graphics& g, juce::Rectangle<float> area)
    {
        float barW = area.getWidth();

        // ─── Fondo de la barra ──────────────────────────────────────────────
        g.setColour(MixCoachTheme::bgInput().withAlpha(0.9f));
        g.fillRoundedRectangle(area, 3.0f);

        // ─── Dibujar las 4 zonas ────────────────────────────────────────────
        for (int i = 0; i < kNumZones; ++i) {
            auto& zone    = zones_[i];
            float zoneX   = area.getX() + zone.minNorm * barW;
            float zoneW   = (zone.maxNorm - zone.minNorm) * barW;
            auto zoneArea = juce::Rectangle<float>(zoneX, area.getY(), zoneW, area.getHeight());

            // Fill de zona con gradiente de intensidad
            float alpha = 0.10f + i * 0.04f;
            g.setColour(zone.colour.withAlpha(alpha));
            g.fillRect(zoneArea);

            // Label de zona si hay espacio
            if (zoneW > 30.0f) {
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)).boldened());
                g.setColour(zone.colour.withAlpha(0.55f));
                g.drawText(juce::String(zone.label), zoneArea, juce::Justification::centred);
            }

            // Separador entre zonas
            if (i > 0) {
                g.setColour(juce::Colour(0x66FFFFFF).withAlpha(0.06f));
                g.drawVerticalLine((int)zoneX, area.getY() + 2, area.getBottom() - 2);
            }
        }

        // ─── Marker animado ─────────────────────────────────────────────────
        float curWidth = juce::jlimit(0.0f, 1.0f, widthSmooth_.getCurrent());
        float markerX  = area.getX() + curWidth * barW;
        float markerY  = area.getCentreY();

        const auto& zone     = getZone(curWidth);
        juce::Colour markCol = zone.colour;

        // Glow del marker (doble halo)
        g.setColour(markCol.withAlpha(0.15f));
        g.fillEllipse(markerX - 8.0f, markerY - 8.0f, 16.0f, 16.0f);

        g.setColour(markCol.withAlpha(0.25f));
        g.fillEllipse(markerX - 5.0f, markerY - 5.0f, 10.0f, 10.0f);

        // Marker: diamante sólido
        juce::Path diamond;
        diamond.addTriangle(markerX, markerY - 5.0f, markerX - 4.0f, markerY, markerX, markerY + 5.0f);
        diamond.addTriangle(markerX, markerY - 5.0f, markerX + 4.0f, markerY, markerX, markerY + 5.0f);
        g.setColour(markCol);
        g.fillPath(diamond);
        g.setColour(juce::Colours::white.withAlpha(0.30f));
        g.strokePath(diamond, juce::PathStrokeType(0.5f));

        // ─── Línea de marcador vertical (guía) ──────────────────────────────
        g.setColour(markCol.withAlpha(0.10f));
        g.drawVerticalLine((int)markerX, area.getY() + 2, area.getBottom() - 2);

        // ─── Borde ──────────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::bgSurface().withAlpha(0.25f));
        g.drawRoundedRectangle(area, 3.0f, 0.5f);

        // ─── Labels de escala (0% y 100%) ───────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)));
        g.setColour(MixCoachTheme::textDim().withAlpha(0.4f));
        g.drawText("0%",
                   juce::Rectangle<float>(area.getX() + 2, area.getBottom() - 10, 16, 8),
                   juce::Justification::centredLeft);
        g.drawText("100%",
                   juce::Rectangle<float>(area.getRight() - 22, area.getBottom() - 10, 20, 8),
                   juce::Justification::centredRight);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawTip — Tip contextual que explica qué significa el valor actual
    // ═══════════════════════════════════════════════════════════════════════════
    void StereoWidthMeter::drawTip(juce::Graphics& g, juce::Rectangle<float> area)
    {
        float curWidth   = widthSmooth_.getCurrent();
        const auto& zone = getZone(curWidth);

        // ─── Fondo sutil ─────────────────────────────────────────────────────
        g.setColour(zone.colour.withAlpha(0.05f));
        g.fillRoundedRectangle(area, 3.0f);

        // ─── Borde izquierdo de acento ──────────────────────────────────────
        g.setColour(zone.colour.withAlpha(0.25f));
        g.fillRoundedRectangle(juce::Rectangle<float>(area.getX(), area.getY(), 2.0f, area.getHeight()), 1.0f);

        // ─── Texto del tip ──────────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)));
        g.setColour(zone.colour.withAlpha(0.85f));
        g.drawText(juce::String(zone.icon) + " " + juce::String(zone.tip),
                   area.reduced(6, 0),
                   juce::Justification::centredLeft);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawPerBand — Mini barras por banda de frecuencia (SUB, BAJ, MED, etc.)
    // ═══════════════════════════════════════════════════════════════════════════
    void StereoWidthMeter::drawPerBand(juce::Graphics& g, juce::Rectangle<float> area)
    {
        if (!hasPerBandData_) return;

        // ─── Label de sección ────────────────────────────────────────────────
        auto labelArea = area.removeFromTop(10);
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened());
        g.setColour(MixCoachTheme::textDim().withAlpha(0.5f));
        g.drawText(
            juce::String::fromUTF8("[CHART] POR FRECUENCIA"), labelArea, juce::Justification::centredLeft);

        // ─── Barras ──────────────────────────────────────────────────────────
        float gap       = 2.0f;
        float totalGaps = (kNumBands - 1) * gap;
        float barW      = (area.getWidth() - totalGaps) / (float)kNumBands;
        float barH      = area.getHeight();

        // Texto de ayuda arriba de cada barra
        static const char* kBandHints[kNumBands] = {"Ultra", "Graves", "Cuerpo", "Presen.", "Brillo", "Aire"};

        for (int b = 0; b < kNumBands; ++b) {
            float val = juce::jlimit(0.0f, 1.0f, perBandValues_[b]);
            float bx  = area.getX() + b * (barW + gap);

            // ─── Mini label de ayuda (arriba) ──────────────────────────────
            auto hintArea = juce::Rectangle<float>(bx, area.getY() - 9, barW, 8);
            g.setFont(juce::Font(juce::FontOptions(5.0f)));
            g.setColour(MixCoachTheme::textDim().withAlpha(0.25f));
            g.drawText(juce::String(kBandHints[b]), hintArea, juce::Justification::centred);

            // ─── Background de la barra ────────────────────────────────────
            auto bandArea = juce::Rectangle<float>(bx, area.getY(), barW, barH);
            g.setColour(MixCoachTheme::bgInput().withAlpha(0.5f));
            g.fillRoundedRectangle(bandArea, 2.0f);

            // ─── Fill según valor y zona ──────────────────────────────────
            if (val > 0.01f) {
                float fillH   = val * barH;
                auto fillArea = bandArea.withTop(bandArea.getBottom() - fillH);

                juce::Colour bandCol;
                if (val < 0.15f) bandCol = MixCoachTheme::textDim(); // gris (mono)
                else if (val < 0.40f)
                    bandCol = MixCoachTheme::success(); // verde (natural)
                else if (val < 0.70f)
                    bandCol = MixCoachTheme::info(); // azul (amplio)
                else
                    bandCol = MixCoachTheme::meterOrange(); // naranja (exceso)

                g.setColour(bandCol.withAlpha(0.65f));
                g.fillRoundedRectangle(fillArea, 1.5f);

                // Cap glow
                auto capH    = juce::jmax(1.0f, fillH * 0.08f);
                auto capGlow = fillArea.withHeight(capH);
                g.setColour(juce::Colours::white.withAlpha(0.10f));
                g.fillRect(capGlow);
            }

            // ─── Label de banda (abajo) ────────────────────────────────────
            auto bandLabelArea = bandArea.removeFromBottom(7);
            g.setFont(juce::Font(juce::FontOptions(5.5f)));
            g.setColour(MixCoachTheme::textDim().withAlpha(0.5f));
            g.drawText(juce::String(kBandLabels[b]), bandLabelArea, juce::Justification::centred);

            // ─── Borde sutil ──────────────────────────────────────────────
            g.setColour(MixCoachTheme::bgSurface().withAlpha(0.15f));
            g.drawRoundedRectangle(bandArea, 2.0f, 0.3f);
        }
    }

} // namespace mixcoach
