#include "AnalogVUMeter.h"

namespace mixcoach {

AnalogVUMeter::AnalogVUMeter()
{
    smoothedLevel_.reset(-80.0f);
}

void AnalogVUMeter::setLevel(float levelDb)
{
    smoothedLevel_.setTarget(juce::jlimit(-80.0f, 6.0f, levelDb));
    repaint();
}

void AnalogVUMeter::resized()
{
}

void AnalogVUMeter::drawScale(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    auto w = bounds.getWidth();
    auto h = bounds.getHeight();

    float cx = bounds.getCentreX();
    float cy = bounds.getBottom() + 4.0f;
    float radius = h * 1.1f;

    float startAngle = 0.75f * juce::MathConstants<float>::pi;
    float endAngle   = 0.25f * juce::MathConstants<float>::pi;

    juce::Path arcPath;
    arcPath.addCentredArc(cx, cy, radius, radius,
                          0.0f, startAngle, endAngle, true);

    g.setColour(MixCoachTheme::border().withAlpha(0.3f));
    g.strokePath(arcPath, juce::PathStrokeType(2.0f));

    struct VUTick { float db; bool major; };
    VUTick ticks[] = {
        { -20.0f, true }, { -18.0f, false }, { -16.0f, false }, { -14.0f, false },
        { -12.0f, true }, { -10.0f, false }, { -8.0f, false },  { -6.0f, true },
        { -4.0f, false }, { -2.0f, false },  { 0.0f, true },
        { +1.0f, false }, { +2.0f, false },  { +3.0f, true }
    };

    float angleRange = startAngle - endAngle;
    float dbRange = 23.0f;

    g.setFont(juce::Font(juce::FontOptions(7.0f)));

    for (auto& tick : ticks) {
        float t = (tick.db + 20.0f) / dbRange;
        float angle = startAngle - t * angleRange;

        float tickLen = tick.major ? 8.0f : 4.0f;
        float innerR = radius - 10.0f;
        float x1 = cx + innerR * std::cos(angle);
        float y1 = cy - innerR * std::sin(angle);
        float x2 = cx + (innerR - tickLen) * std::cos(angle);
        float y2 = cy - (innerR - tickLen) * std::sin(angle);

        juce::Colour tickColour;
        if (tick.db >= 0.0f)
            tickColour = MixCoachTheme::meterRed();
        else if (tick.db >= -2.0f)
            tickColour = MixCoachTheme::meterOrange();
        else if (tick.db >= -6.0f)
            tickColour = MixCoachTheme::meterYellow();
        else
            tickColour = MixCoachTheme::meterGreen();

        g.setColour(tickColour.withAlpha(tick.major ? 0.7f : 0.4f));
        g.drawLine(x1, y1, x2, y2, tick.major ? 1.5f : 0.8f);

        if (tick.major) {
            float labelR = innerR - tickLen - 6.0f;
            float lx = cx + labelR * std::cos(angle);
            float ly = cy - labelR * std::sin(angle);
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.drawText(juce::String((int)tick.db),
                       juce::Rectangle<float>(lx - 10, ly - 5, 20, 10),
                       juce::Justification::centred);
        }
    }
}

void AnalogVUMeter::drawNeedle(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    float dbValue = smoothedLevel_.getCurrent();

    float cx = bounds.getCentreX();
    float cy = bounds.getBottom() + 4.0f;
    float radius = bounds.getHeight() * 1.0f;

    float startAngle = 0.75f * juce::MathConstants<float>::pi;
    float angleRange = 0.5f * juce::MathConstants<float>::pi;

    float dbRange = 23.0f;
    float t = juce::jlimit(0.0f, 1.0f, (dbValue + 20.0f) / dbRange);
    float angle = startAngle - t * angleRange;

    float needleLen = radius - 14.0f;
    float nx = cx + needleLen * std::cos(angle);
    float ny = cy - needleLen * std::sin(angle);

    g.setColour(juce::Colour(0xFF00B4D8).withAlpha(0.08f));
    g.drawLine(cx, cy + 2, nx, ny, 6.0f);

    juce::Colour needleColour;
    if (dbValue >= 0.0f)
        needleColour = MixCoachTheme::meterRed();
    else if (dbValue >= -2.0f)
        needleColour = MixCoachTheme::meterOrange();
    else if (dbValue >= -6.0f)
        needleColour = MixCoachTheme::meterYellow();
    else
        needleColour = MixCoachTheme::meterGreen();

    g.setColour(needleColour);
    g.drawLine(cx, cy + 2, nx, ny, 2.0f);

    g.fillEllipse(nx - 2.0f, ny - 2.0f, 4.0f, 4.0f);

    g.setColour(MixCoachTheme::textBright().withAlpha(0.8f));
    g.fillEllipse(cx - 4.0f, cy - 4.0f, 8.0f, 8.0f);
    g.setColour(MixCoachTheme::bgDarker());
    g.fillEllipse(cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
}

void AnalogVUMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(MixCoachTheme::bgPanel());
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(MixCoachTheme::border().withAlpha(0.3f));
    g.drawRoundedRectangle(bounds, 4.0f, 0.5f);

    auto content = bounds.reduced(4, 4);

    if (!labelText_.isEmpty()) {
        auto labelArea = content.removeFromTop(14).reduced(2, 0);
        g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
        g.setColour(meterColour_);
        g.drawText(labelText_, labelArea, juce::Justification::centred);
    }

    drawScale(g, content);
    drawNeedle(g, content);

    auto valArea = bounds.withHeight(14).withBottomY(bounds.getBottom() - 2);
    float val = smoothedLevel_.getCurrent();
    juce::String valStr = (val < -60.0f) ? "--.-" : juce::String(val, 1);
    g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
    g.setColour(MixCoachTheme::textDim());
    g.drawText(valStr + " dB", valArea, juce::Justification::centred);
}

} // namespace mixcoach
