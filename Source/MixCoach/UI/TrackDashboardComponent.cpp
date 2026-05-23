#include "TrackDashboardComponent.h"

namespace mixcoach {

TrackDashboardComponent::TrackDashboardComponent()
{
    titleLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x93\x8B Dashboard de Pistas"), juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeHeader)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textBright());
    addAndMakeVisible(titleLabel_);

    infoLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x94\x8C Conecta los plugins Messenger en tus pistas para ver sus datos aqui."), juce::dontSendNotification);
    infoLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)));
    infoLabel_.setJustificationType(juce::Justification::centred);
    infoLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
    addAndMakeVisible(infoLabel_);
}

void TrackDashboardComponent::updateTrackData(const SlotInfo& info, const TrackTelemetry& telemetry)
{
    trackInfo_[info.slotIndex] = info;
    trackTelemetry_[info.slotIndex] = telemetry;
    repaint();
}

void TrackDashboardComponent::resized()
{
    auto area = getLocalBounds().reduced(8);
    titleLabel_.setBounds(area.removeFromTop(24));
    infoLabel_.setBounds(area);
}

void TrackDashboardComponent::paint(juce::Graphics& g)
{
    g.fillAll(MixCoachTheme::bgDark());

    auto area = getLocalBounds().reduced(8);
    area.removeFromTop(24);

    int trackCount = 0;
    for (int i = 0; i < SlotRegistry::kMaxSlots; ++i) {
        if (trackInfo_[i].active) {
            auto trackArea = area.removeFromTop(52).reduced(4, 3);
            drawTrackCard(g, trackArea, trackInfo_[i], trackTelemetry_[i]);
            ++trackCount;
        }
    }

    if (trackCount == 0) {
        infoLabel_.setBounds(area);
    }
}

void TrackDashboardComponent::drawTrackCard(juce::Graphics& g, juce::Rectangle<int> bounds,
                                              const SlotInfo& info,
                                              const TrackTelemetry& telemetry)
{
    // Glass card background
    MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), 6.0f);

    // Color accent bar (left side)
    auto accentBar = bounds.removeFromLeft(5);
    g.setColour(info.colour);
    g.fillRoundedRectangle(accentBar.toFloat(), 2.0f);

    bounds.removeFromLeft(6);

    // ─── Track name ────────────────────────────────────────────────────────
    g.setColour(MixCoachTheme::textPrimary());
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)).boldened());
    auto nameArea = bounds.removeFromLeft(140);
    g.drawText(info.trackName, nameArea, juce::Justification::centredLeft);

    // ─── Level meter ───────────────────────────────────────────────────────
    auto meterArea = bounds.removeFromLeft(120).reduced(2, 6);
    float norm = juce::jlimit(0.0f, 1.0f, (telemetry.peakLeft + 60.0f) / 66.0f);

    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(meterArea.toFloat(), 3.0f);

    juce::Colour meterColour;
    if (telemetry.peakLeft > -6.0f)      meterColour = MixCoachTheme::meterRed();
    else if (telemetry.peakLeft > -12.0f) meterColour = MixCoachTheme::meterYellow();
    else if (telemetry.peakLeft > -18.0f) meterColour = MixCoachTheme::meterGreen();
    else                                  meterColour = MixCoachTheme::meterBlue();

    g.setColour(meterColour);
    auto fillBar = meterArea.withWidth((int)(meterArea.getWidth() * norm));
    if (fillBar.getWidth() > 1)
        g.fillRoundedRectangle(fillBar.toFloat(), 3.0f);

    // ─── Stats ─────────────────────────────────────────────────────────────
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
    g.setColour(MixCoachTheme::textDim());

    auto statsArea = bounds.reduced(2, 4);

    // Peak
    g.drawText("P: " + juce::String(telemetry.peakLeft, 1) + " dB",
               statsArea.removeFromLeft(65), juce::Justification::centredLeft);

    // RMS
    auto rms = (telemetry.rmsLeft + telemetry.rmsRight) * 0.5f;
    g.drawText("R: " + juce::String(rms, 1) + " dB",
               statsArea.removeFromLeft(65), juce::Justification::centredLeft);

    // Correlation
    auto corrColour = (std::abs(telemetry.correlation) < 0.3f)
        ? MixCoachTheme::error()
        : (telemetry.correlation < 0.0f ? MixCoachTheme::warning() : MixCoachTheme::success());
    g.setColour(corrColour);
    g.drawText("\xCF\x86: " + juce::String(telemetry.correlation, 2),
               statsArea.removeFromLeft(65), juce::Justification::centredLeft);

    // Signal indicator
    if (telemetry.peakLeft > -60.0f) {
        g.setColour(MixCoachTheme::success().withAlpha(0.7f));
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
        g.drawText(juce::CharPointer_UTF8("\xE2\x97\x89 Audio"),
                   statsArea.removeFromLeft(50), juce::Justification::centredLeft);
    }
}

} // namespace mixcoach
