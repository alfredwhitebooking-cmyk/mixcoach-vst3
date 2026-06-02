#include "MeterComponent.h"
#include <cmath>

namespace mixcoach {

namespace {

float maxPeak(const TrackTelemetry& t) noexcept
{
    return juce::jmax(t.peakLeft, t.peakRight);
}

float avgRms(const TrackTelemetry& t) noexcept
{
    return (t.rmsLeft + t.rmsRight) * 0.5f;
}

float integratedLufs(const TrackTelemetry& t) noexcept
{
    if (t.lufsIntegrated > -99.0f)
        return t.lufsIntegrated;
    if (t.lufsShortTerm > -99.0f)
        return t.lufsShortTerm;
    return -100.0f;
}

void splitStereoLufs(const TrackTelemetry& t, float& outL, float& outR) noexcept
{
    const float base = (t.lufsMomentary > -99.0f) ? t.lufsMomentary
                      : (t.lufsShortTerm > -99.0f) ? t.lufsShortTerm
                      : integratedLufs(t);

    if (base <= -99.0f)
    {
        outL = outR = -100.0f;
        return;
    }

    const float avgR = avgRms(t);
    const float bias = 0.35f;
    outL = juce::jlimit(VerticalGradientMeter::kMinDb, 6.0f,
                        base + (t.rmsLeft - avgR) * bias);
    outR = juce::jlimit(VerticalGradientMeter::kMinDb, 6.0f,
                        base + (t.rmsRight - avgR) * bias);
}

juce::Font monoValueFont(float size)
{
    return juce::Font(juce::FontOptions(size)).boldened();
}

} // namespace

MeterComponent::MeterComponent()
{
    setOpaque(true);
}

void MeterComponent::updateData(const TrackTelemetry& telem)
{
    leftPeak_.tickHold();
    rightPeak_.tickHold();
    lufsLeftHold_.tickHold();
    lufsRightHold_.tickHold();

    leftBar_.setTargetValue(telem.peakLeft);
    rightBar_.setTargetValue(telem.peakRight);
    leftPeak_.setLevelDb(telem.peakLeft);
    rightPeak_.setLevelDb(telem.peakRight);

    const float peak = (telem.lufsTruePeak > -99.0f && telem.lufsTruePeak > maxPeak(telem))
        ? telem.lufsTruePeak : maxPeak(telem);
    peakSmooth_.setTargetValue(peak);
    rmsSmooth_.setTargetValue(avgRms(telem));

    const float lufsI = integratedLufs(telem);
    if (lufsI > -99.0f)
        lufsSmooth_.setTargetValue(lufsI);

    drSmooth_.setTargetValue(juce::jmax(0.0f, telem.loudnessRange));

    float lufsL = -100.0f, lufsR = -100.0f;
    splitStereoLufs(telem, lufsL, lufsR);
    lufsReadoutLeft_ = lufsL;
    lufsReadoutRight_ = lufsR;

    if (lufsL > -99.0f)
    {
        lufsLeftBar_.setTargetValue(lufsL);
        lufsLeftHold_.setLevelDb(lufsL);
    }
    if (lufsR > -99.0f)
    {
        lufsRightBar_.setTargetValue(lufsR);
        lufsRightHold_.setLevelDb(lufsR);
    }
}

bool MeterComponent::advanceVisuals(double sampleRateHz, bool allowRepaint)
{
    bool dirty = false;
    auto tick = [&](SmoothValue& s) { dirty |= s.advance(sampleRateHz); };

    tick(leftBar_);
    tick(rightBar_);
    tick(peakSmooth_);
    tick(rmsSmooth_);
    tick(lufsSmooth_);
    tick(drSmooth_);
    tick(lufsLeftBar_);
    tick(lufsRightBar_);

    leftPeak_.tickHold();
    rightPeak_.tickHold();
    lufsLeftHold_.tickHold();
    lufsRightHold_.tickHold();

    if (dirty && allowRepaint)
        repaint();

    return dirty;
}

void MeterComponent::resized() {}

void MeterComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(MixCoachTheme::bgPanel().withAlpha(0.55f));
    g.fillRoundedRectangle(bounds, 4.0f);

    auto area = getLocalBounds().reduced(4, 3);
    const int totalW = area.getWidth();
    const int stereoW = (int) (totalW * 0.38f);
    const int lufsW   = (int) (totalW * 0.30f);
    const int numericW = totalW - stereoW - lufsW;

    paintStereoColumn(g, area.removeFromLeft(stereoW));
    paintNumericColumn(g, area.removeFromLeft(numericW));
    paintLufsMdrColumn(g, area);
}

void MeterComponent::paintStereoColumn(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
    g.setColour(MixCoachTheme::textDim());
    const int labelH = 12;
    auto header = area.removeFromTop(labelH);
    const int colW = area.getWidth() / 2;
    g.drawText("L", header.removeFromLeft(colW), juce::Justification::centred);
    g.drawText("R", header, juce::Justification::centred);

    area.removeFromTop(2);
    const int readoutH = 14;
    auto readoutRow = area.removeFromBottom(readoutH);
    area.removeFromBottom(2);

    const int scaleW = juce::jmax(16, (int) (area.getWidth() * 0.22f));
    auto scaleArea = area.removeFromLeft(scaleW).toFloat();
    auto metersArea = area.toFloat();

    VerticalGradientMeter::drawDbScale(g, scaleArea);

    const float gap = 4.0f;
    const float barW = (metersArea.getWidth() - gap) * 0.5f;
    auto lBounds = metersArea.removeFromLeft(barW);
    metersArea.removeFromLeft(gap);
    auto rBounds = metersArea;

    VerticalGradientMeter::drawGradientBar(g, lBounds, leftBar_.getCurrent());
    VerticalGradientMeter::drawGradientBar(g, rBounds, rightBar_.getCurrent());

    const auto peakColour = MixCoachTheme::meterOrange();
    VerticalGradientMeter::drawPeakTriangle(g, scaleArea, leftPeak_.peakHoldDb, peakColour);
    VerticalGradientMeter::drawPeakTriangle(g, scaleArea, rightPeak_.peakHoldDb, peakColour);

    const int half = readoutRow.getWidth() / 2;
    VerticalGradientMeter::drawPeakReadout(g, readoutRow.removeFromLeft(half).toFloat(),
                                           leftPeak_.peakHoldDb, peakColour);
    VerticalGradientMeter::drawPeakReadout(g, readoutRow.toFloat(),
                                           rightPeak_.peakHoldDb, peakColour);
}

void MeterComponent::paintNumericColumn(juce::Graphics& g, juce::Rectangle<int> area)
{
    area = area.reduced(4, 6);
    const int rowH = area.getHeight() / 4;

    auto row1 = area.removeFromTop(rowH).toFloat();
    auto row2 = area.removeFromTop(rowH).toFloat();
    auto row3 = area.removeFromTop(rowH).toFloat();
    auto row4 = area.toFloat();

    drawMetricRow(g, row1, "PEAK", peakSmooth_.getCurrent(), "dB");
    drawMetricRow(g, row2, "RMS", rmsSmooth_.getCurrent(), "dB");
    drawMetricRow(g, row3, "LUFS (I)", lufsSmooth_.getCurrent(), "LUFS", true);
    drawMetricRow(g, row4, "DR", drSmooth_.getCurrent(), "dB");
}

void MeterComponent::drawMetricRow(juce::Graphics& g, juce::Rectangle<float> row,
                                   const juce::String& label, float valueDb,
                                   const juce::String& suffix, bool isLufs)
{
    auto labelArea = row.removeFromTop(row.getHeight() * 0.32f);
    g.setFont(juce::Font(juce::FontOptions(7.0f)).boldened());
    g.setColour(MixCoachTheme::textDim());
    g.drawText(label, labelArea, juce::Justification::centredLeft);

    juce::String val;
    if (valueDb <= -99.0f)
        val = "--.-";
    else
        val = juce::String(valueDb, 1);

    if (suffix.isNotEmpty())
        val += (isLufs ? " " : " ") + suffix;

    g.setFont(monoValueFont(13.0f));
    g.setColour(MixCoachTheme::textBright());
    g.drawText(val, row, juce::Justification::centredLeft);
}

void MeterComponent::paintLufsMdrColumn(juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setFont(juce::Font(juce::FontOptions(7.0f)).boldened());
    g.setColour(MixCoachTheme::textDim());
    g.drawText("LUFS MDR", area.removeFromTop(11), juce::Justification::centredLeft);

    auto header = area.removeFromTop(12);
    const int colW = area.getWidth() / 2;
    g.drawText("L", header.removeFromLeft(colW), juce::Justification::centred);
    g.drawText("R", header, juce::Justification::centred);

    area.removeFromTop(2);
    const int readoutH = 13;
    auto readoutRow = area.removeFromBottom(readoutH);
    area.removeFromBottom(2);

    const int scaleW = juce::jmax(14, (int) (area.getWidth() * 0.24f));
    auto scaleArea = area.removeFromLeft(scaleW).toFloat();
    auto metersArea = area.toFloat().reduced(2.0f, 0.0f);

    VerticalGradientMeter::drawDbScale(g, scaleArea);

    const float gap = 3.0f;
    const float barW = (metersArea.getWidth() - gap) * 0.42f;
    auto lBounds = metersArea.removeFromLeft(barW);
    metersArea.removeFromLeft(gap);
    auto rBounds = metersArea.removeFromLeft(barW);

    const auto cyan = MixCoachTheme::accentCyan();
    VerticalGradientMeter::drawSolidBar(g, lBounds, lufsLeftBar_.getCurrent(), cyan, 2.0f);
    VerticalGradientMeter::drawSolidBar(g, rBounds, lufsRightBar_.getCurrent(), cyan, 2.0f);

    VerticalGradientMeter::drawPeakTriangle(g, scaleArea, lufsLeftHold_.peakHoldDb, cyan);
    VerticalGradientMeter::drawPeakTriangle(g, scaleArea, lufsRightHold_.peakHoldDb, cyan);

    const int half = readoutRow.getWidth() / 2;
    VerticalGradientMeter::drawPeakReadout(g, readoutRow.removeFromLeft(half).toFloat(),
                                           lufsReadoutLeft_, cyan);
    VerticalGradientMeter::drawPeakReadout(g, readoutRow.toFloat(),
                                           lufsReadoutRight_, cyan);
}

} // namespace mixcoach
