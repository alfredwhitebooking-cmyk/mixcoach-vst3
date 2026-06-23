#include "VerticalGradientMeter.h"
#include <cmath>

namespace mixcoach {

float VerticalGradientMeter::dbToNorm(float db) noexcept
{
    // Escala balística unificada sobre un rango de 66dB (+6 a -60)
    return juce::jlimit(0.0f, 1.0f, (db - kMinDb) / (kMaxDb - kMinDb));
}

float VerticalGradientMeter::normToY(float norm, juce::Rectangle<float> meterBounds) noexcept
{
    return meterBounds.getBottom() - norm * meterBounds.getHeight();
}

juce::Colour VerticalGradientMeter::colourForDb(float db) noexcept
{
    float norm = dbToNorm(db);
    
    // Mapeo dinámico multi-stop continuo para consultas analíticas rápidas
    if (norm < 0.55f) return MixCoachTheme::meterGreen().interpolatedWith(MixCoachTheme::meterLime(), norm / 0.55f);
    if (norm < 0.75f) return MixCoachTheme::meterLime().interpolatedWith(MixCoachTheme::meterYellow(), (norm - 0.55f) / 0.20f);
    if (norm < 0.88f) return MixCoachTheme::meterYellow().interpolatedWith(MixCoachTheme::meterOrange(), (norm - 0.75f) / 0.13f);
    
    return MixCoachTheme::meterOrange().interpolatedWith(MixCoachTheme::meterRed(), (norm - 0.88f) / 0.12f);
}

void VerticalGradientMeter::drawDbScale(juce::Graphics& g, juce::Rectangle<float> bounds,
                                        bool topIsZero)
{
    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(bounds, 2.0f);

    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)));
    const int marks[] = { 6, 0, -3, -6, -12, -18, -24, -30, -40, -50, -60 };

    for (int db : marks)
    {
        const float norm = dbToNorm((float)db);
        const float y = topIsZero ? normToY(norm, bounds) : bounds.getY() + norm * bounds.getHeight();

        // Línea divisoria técnica sutil
        g.setColour(MixCoachTheme::rowDivider().withAlpha(db == 0 ? 0.55f : 0.20f));
        g.drawHorizontalLine((int)y, bounds.getX(), bounds.getRight());

        // Tipografía de marcas numéricas protegida contra desbordamientos
        g.setColour(db > 0 ? MixCoachTheme::error().withAlpha(0.6f) : MixCoachTheme::textMuted().withAlpha(0.4f));
        g.drawText(juce::String(db),
                   juce::Rectangle<float>(bounds.getX(), y - 4.5f, bounds.getWidth(), 9.0f),
                   juce::Justification::centred);
    }
}

void VerticalGradientMeter::drawGradientBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                                            float levelDb, float radius)
{
    // Fondo de canal oscuro anodizado
    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(bounds, radius);

    const float norm = dbToNorm(levelDb);
    if (norm <= 0.005f)
        return;

    auto fillTop = normToY(norm, bounds);
    auto fillBounds = bounds.withTop(fillTop);

    // Fijar el gradiente a las dimensiones estáticas absolutas de la pista (Riel)
    // Esto garantiza que el color refleje fielmente el valor en dB sin importar el tamaño del fill
    juce::ColourGradient grad(
        MixCoachTheme::meterGreen(),  bounds.getCentreX(), bounds.getBottom(),
        MixCoachTheme::meterRed(),    bounds.getCentreX(), bounds.getY(), false);

    grad.addColour(0.55f, MixCoachTheme::meterLime());
    grad.addColour(0.75f, MixCoachTheme::meterYellow());
    grad.addColour(0.88f, MixCoachTheme::meterOrange());

    g.setGradientFill(grad);
    
    // Renderizado seguro por máscara de recorte para preservar los bordes redondeados inferiores
    g.saveState();
    g.reduceClipRegion(fillBounds.toNearestInt());
    g.fillRoundedRectangle(bounds, radius);
    g.restoreState();

    // Resplandor superior técnico (Glow frontal de impacto)
    auto shine = fillBounds.withHeight(juce::jmin(2.5f, fillBounds.getHeight()));
    g.setColour(juce::Colours::white.withAlpha(0.30f));
    g.fillRect(shine);
}

void VerticalGradientMeter::drawSolidBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                                         float levelDb, juce::Colour colour, float radius)
{
    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(bounds, radius);

    const float norm = dbToNorm(levelDb);
    if (norm <= 0.005f)
        return;

    auto fillBounds = bounds.withTop(normToY(norm, bounds));
    g.setColour(colour.withAlpha(0.85f));
    g.fillRoundedRectangle(fillBounds, radius);
}

void VerticalGradientMeter::drawPeakTriangle(juce::Graphics& g, juce::Rectangle<float> scaleBounds,
                                             float peakHoldDb, juce::Colour colour, bool alignLeft)
{
    if (peakHoldDb <= kMinDb + 0.5f)
        return;

    const float y = normToY(dbToNorm(peakHoldDb), scaleBounds);
    const float triSize = 3.0f;
    
    juce::Path tri;
    
    if (alignLeft)
    {
        const float x = scaleBounds.getX() - 1.5f;
        tri.addTriangle(x, y,
                        x - triSize, y - triSize + 0.5f,
                        x - triSize, y + triSize - 0.5f);
    }
    else
    {
        const float x = scaleBounds.getRight() + 1.5f;
        tri.addTriangle(x, y,
                        x + triSize, y - triSize + 0.5f,
                        x + triSize, y + triSize - 0.5f);
    }

    g.setColour(peakHoldDb >= 0.0f ? MixCoachTheme::error() : colour.withAlpha(0.9f));
    g.fillPath(tri);
}

void VerticalGradientMeter::drawPeakReadout(juce::Graphics& g, juce::Rectangle<float> bounds,
                                            float peakDb, juce::Colour colour)
{
    juce::String text = peakDb <= kMinDb + 1.0f ? "--.-" : ((peakDb >= 0.0f ? "+" : "") + juce::String(peakDb, 1));
    
    g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
    g.setColour(peakDb >= 0.0f ? MixCoachTheme::error() : colour);
    g.drawText(text, bounds, juce::Justification::centred);
}

void MeterChannelBallistics::setLevelDb(float db) noexcept
{
    displayDb = juce::jlimit(VerticalGradientMeter::kMinDb,
                             VerticalGradientMeter::kMaxDb + 3.0f, db);

    if (db >= peakHoldDb)
    {
        peakHoldDb = db;
        peakHoldFrames = 45; // Sostener el pico por ~750ms antes de comenzar la caída
    }
}

void MeterChannelBallistics::tickHold() noexcept
{
    if (peakHoldFrames > 0)
    {
        --peakHoldFrames;
    }
    else if (peakHoldDb > VerticalGradientMeter::kMinDb)
    {
        // Caída lineal en decibelios (0.55 dB por frame), mucho más natural
        peakHoldDb -= 0.55f;
        if (peakHoldDb < VerticalGradientMeter::kMinDb)
            peakHoldDb = VerticalGradientMeter::kMinDb;
    }
}

} // namespace mixcoach
