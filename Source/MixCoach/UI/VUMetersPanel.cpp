#include "VUMetersPanel.h"

namespace mixcoach {

VUMetersPanel::VUMetersPanel()
{
    headerLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x93\x8A VU Meters"),
                         juce::dontSendNotification);
    headerLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    headerLabel_.setJustificationType(juce::Justification::centredLeft);
    headerLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(headerLabel_);

    for (auto& m : meters_)
        m.level.reset(-60.0f);
}

void VUMetersPanel::setLevel(int idx, float db)
{
    if (idx < 0 || idx >= 4) return;
    auto& m = meters_[idx];
    m.level.setTargetValue(db);

    // Peak hold
    if (db > m.peakHold)
    {
        m.peakHold = db;
        m.holdTimer = 30;  // ~0.5s at 60fps
    }
}

bool VUMetersPanel::advanceMeters(double sampleRateHz, bool allowRepaint)
{
    bool dirty = false;
    for (int i = 0; i < 4; ++i)
    {
        auto& m = meters_[i];
        dirty |= m.level.advance(sampleRateHz);

        // Peak decay
        if (m.holdTimer > 0)
            --m.holdTimer;
        else if (m.peakHold > -60.0f)
            m.peakHold += (-60.0f - m.peakHold) * 0.04f;
    }

    if (dirty && allowRepaint)
        repaint();
    return dirty;
}

void VUMetersPanel::resized()
{
    if (headerLabel_.isVisible())
        headerLabel_.setBounds(getLocalBounds().reduced(4, 2).removeFromTop(18));
}

void VUMetersPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), 6.0f);

    auto area = bounds.reduced(3);
    area.removeFromTop(18);  // espacio para headerLabel_

    // Grid 2×2
    const int gap = 3;
    const int w = (area.getWidth() - gap) / 2;
    const int h = (area.getHeight() - gap) / 2;
    const int x0 = area.getX();
    const int y0 = area.getY();

    for (int i = 0; i < 4; ++i)
    {
        int col = i % 2;
        int row = i / 2;
        auto cell = juce::Rectangle<int>(
            x0 + col * (w + gap),
            y0 + row * (h + gap),
            w, h);
        drawMeter(g, cell, i, meters_[i].level.getCurrent(), meters_[i].peakHold);
    }
}

void VUMetersPanel::drawMeter(juce::Graphics& g, juce::Rectangle<int> bounds,
                               int idx, float levelDb, float peakDb)
{
    auto b = bounds.toFloat().reduced(1.0f, 2.0f);
    const float labelH = 16.0f;
    const float scaleW = 18.0f;

    // ─── Label (L/R/M/S) ───────────────────────────────────────────────
    auto labelArea = b.removeFromTop(labelH);
    g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
    g.setColour(MixCoachTheme::textBright());
    g.drawText(labels_[idx], labelArea, juce::Justification::centred);

    b.removeFromTop(2);

    // ─── Scale ─────────────────────────────────────────────────────────
    auto scaleArea = b.removeFromLeft(scaleW);
    VerticalGradientMeter::drawDbScale(g, scaleArea);

    b.removeFromLeft(2);

    // ─── Gradient bar ──────────────────────────────────────────────────
    VerticalGradientMeter::drawGradientBar(g, b, levelDb, 2.0f);

    // ─── Peak triangle ─────────────────────────────────────────────────
    if (peakDb > -59.0f)
        VerticalGradientMeter::drawPeakTriangle(g, scaleArea, peakDb,
                                                MixCoachTheme::meterOrange());

    // ─── Peak readout (debajo de la barra, sobre fondo oscuro) ─────────
    auto readoutBg = juce::Rectangle<float>(
        b.getX(), b.getBottom() + 1.0f, b.getWidth(), 11.0f);
    g.setColour(MixCoachTheme::bgDarker().withAlpha(0.6f));
    g.fillRoundedRectangle(readoutBg, 2.0f);
    juce::String val = (peakDb > -59.0f)
        ? juce::String(peakDb, 1) + " dB"
        : "--.- dB";
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened());
    g.setColour(MixCoachTheme::textMuted());
    g.drawText(val, readoutBg, juce::Justification::centred);
}

} // namespace mixcoach
