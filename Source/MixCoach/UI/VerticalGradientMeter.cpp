#include "VerticalGradientMeter.h"
#include <cmath>

namespace mixcoach {

float VerticalGradientMeter::dbToNorm(float db) noexcept
{
    return juce::jlimit(0.0f, 1.0f, (db - kMinDb) / (kMaxDb - kMinDb));
}

float VerticalGradientMeter::normToY(float norm, juce::Rectangle<float> meterBounds) noexcept
{
    return meterBounds.getBottom() - norm * meterBounds.getHeight();
}

juce::Colour VerticalGradientMeter::colourForDb(float db) noexcept
{
    // Returns the color at a given position in the gradient range (-60 to 0 dB)
    // Gradient: 0%-50% green, 50%-70% lime, 70%-85% yellow, 85%-95% orange, 95%-100% red
    // Maps -60dB→0% (bottom) to 0dB→100% (top)
    float norm = juce::jlimit(0.0f, 1.0f, (db - kMinDb) / (kMaxDb - kMinDb));
    
    if (norm < 0.50f) return MixCoachTheme::meterGreen();
    if (norm < 0.70f) return MixCoachTheme::meterLime();
    if (norm < 0.85f) return MixCoachTheme::meterYellow();
    if (norm < 0.95f) return MixCoachTheme::meterOrange();
    return MixCoachTheme::meterRed();
}

void VerticalGradientMeter::drawDbScale(juce::Graphics& g, juce::Rectangle<float> bounds,
                                        bool topIsZero)
{
    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(bounds, 2.0f);

    g.setFont(juce::Font(juce::FontOptions(6.5f)));
    // Escala exacta del visual design: 6, 0, -6, -12, -18, -24, -30, -36, -42, -48, -60
    const int marks[] = { 6, 0, -6, -12, -18, -24, -30, -36, -42, -48, -60 };

    for (int db : marks)
    {
        const float norm = dbToNorm((float) db);
        const float y = topIsZero ? normToY(norm, bounds) : bounds.getY() + norm * bounds.getHeight();

        g.setColour(MixCoachTheme::rowDivider().withAlpha(0.55f));
        g.drawHorizontalLine((int) y, bounds.getX() + 1.0f, bounds.getRight() - 1.0f);

        g.setColour(MixCoachTheme::textMuted().withAlpha(0.75f));
        g.drawText(juce::String(db),
                   juce::Rectangle<float>(bounds.getX(), y - 5.0f, bounds.getWidth(), 9.0f),
                   juce::Justification::centred);
    }
}

void VerticalGradientMeter::drawGradientBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                                            float levelDb, float radius)
{
    g.setColour(MixCoachTheme::bgCanvas());
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(MixCoachTheme::border().withAlpha(0.25f));
    g.drawRoundedRectangle(bounds, radius, 0.5f);

    const float norm = dbToNorm(levelDb);
    if (norm <= 0.001f)
        return;

    auto fillTop = normToY(norm, bounds);
    auto fillBounds = bounds.withTop(fillTop);

    // Fixed multi-stop gradient: green→lime→yellow→orange→red (bottom→top)
    // Colors placed at 0%, 50%, 70%, 85%, 95%, 100% of bar height
    const float topNorm = (levelDb - kMinDb) / (kMaxDb - kMinDb);
    const float botNorm = 0.0f;
    
    // Mapear las posiciones del gradiente al rango visible del fill
    // El gradiente es fijo en todo el rango -60..0 dB, el fill recorta
    juce::ColourGradient grad;
    grad.isRadial = false;
    grad.point1 = juce::Point<float>(fillBounds.getCentreX(), fillBounds.getY());     // top
    grad.point2 = juce::Point<float>(fillBounds.getCentreX(), fillBounds.getBottom()); // bottom
    
    grad.addColour(0.00f, MixCoachTheme::meterRed());      // top → red
    grad.addColour(0.05f, MixCoachTheme::meterRed());       // 5%
    grad.addColour(0.15f, MixCoachTheme::meterOrange());    // 15%
    grad.addColour(0.30f, MixCoachTheme::meterYellow());    // 30%
    grad.addColour(0.50f, MixCoachTheme::meterLime());      // 50%
    grad.addColour(1.00f, MixCoachTheme::meterGreen());     // bottom → green
    
    g.setGradientFill(grad);
    g.fillRoundedRectangle(fillBounds, radius);

    auto shine = fillBounds.withHeight(juce::jmax(2.0f, fillBounds.getHeight() * 0.08f));
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.fillRoundedRectangle(shine, radius);
}

void VerticalGradientMeter::drawSolidBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                                         float levelDb, juce::Colour colour, float radius)
{
    g.setColour(MixCoachTheme::bgCanvas());
    g.fillRoundedRectangle(bounds, radius);

    const float norm = dbToNorm(levelDb);
    if (norm <= 0.001f)
        return;

    auto fillBounds = bounds.withTop(normToY(norm, bounds));
    g.setColour(colour.withAlpha(0.92f));
    g.fillRoundedRectangle(fillBounds, radius);
}

void VerticalGradientMeter::drawPeakTriangle(juce::Graphics& g,
                                             juce::Rectangle<float> scaleBounds,
                                             float peakHoldDb, juce::Colour colour)
{
    if (peakHoldDb <= kMinDb + 0.5f)
        return;

    const float y = normToY(dbToNorm(peakHoldDb), scaleBounds);
    // ◀ peak triangle a la izquierda del scale (visual design)
    const float x = scaleBounds.getX() - 1.0f;

    juce::Path tri;
    tri.addTriangle(x, y,
                    x + 5.0f, y - 3.0f,
                    x + 5.0f, y + 3.0f);
    g.setColour(colour);
    g.fillPath(tri);
}

void VerticalGradientMeter::drawPeakReadout(juce::Graphics& g, juce::Rectangle<float> bounds,
                                            float peakDb, juce::Colour colour)
{
    juce::String text = peakDb <= kMinDb + 1.0f ? "--.-" : juce::String(peakDb, 1);
    g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
    g.setColour(colour);
    g.drawText(text, bounds, juce::Justification::centred);
}

void MeterChannelBallistics::setLevelDb(float db) noexcept
{
    displayDb = juce::jlimit(VerticalGradientMeter::kMinDb,
                             VerticalGradientMeter::kMaxDb + 3.0f, db);

    if (db >= peakHoldDb)
    {
        peakHoldDb = db;
        peakHoldFrames = 45;
    }
}

void MeterChannelBallistics::tickHold() noexcept
{
    if (peakHoldFrames > 0)
        --peakHoldFrames;
    else if (peakHoldDb > VerticalGradientMeter::kMinDb)
        peakHoldDb += (VerticalGradientMeter::kMinDb - peakHoldDb) * 0.04f;
}

} // namespace mixcoach
