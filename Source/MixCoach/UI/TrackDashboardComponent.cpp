#include "TrackDashboardComponent.h"

namespace mixcoach {

TrackDashboardComponent::TrackDashboardComponent()
{
    titleLabel_.setText("📊 Dashboard de Pistas", juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(16.0f)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(titleLabel_);

    infoLabel_.setText("Conecta los plugins Messenger en tus pistas para ver sus datos aquí.",
                       juce::dontSendNotification);
    infoLabel_.setFont(juce::Font(juce::FontOptions(13.0f)));
    infoLabel_.setJustificationType(juce::Justification::centred);
    infoLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
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
    area.removeFromTop(24); // title

    int trackCount = 0;
    for (int i = 0; i < SlotRegistry::kMaxSlots; ++i) {
        if (trackInfo_[i].active) {
            auto trackArea = area.removeFromTop(40).reduced(4, 2);
            drawTrackRow(g, trackArea, trackInfo_[i], trackTelemetry_[i]);
            ++trackCount;
        }
    }

    if (trackCount == 0) {
        g.setColour(MixCoachTheme::textDim());
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.drawText(infoLabel_.getText(), area.toNearestInt(), juce::Justification::centred);
    }
}

void TrackDashboardComponent::drawTrackRow(juce::Graphics& g, juce::Rectangle<int> bounds,
                                            const SlotInfo& info,
                                            const TrackTelemetry& telemetry)
{
    // Fondo de la fila
    g.setColour(MixCoachTheme::bgPanel().withAlpha(0.5f));
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

    // Indicador de color
    g.setColour(info.colour);
    g.fillRoundedRectangle(bounds.removeFromLeft(4).toFloat(), 2.0f);

    // Nombre
    g.setColour(MixCoachTheme::textPrimary());
    g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
    g.drawText(info.trackName, bounds.removeFromLeft(120), juce::Justification::centredLeft);

    // Niveles
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    g.setColour(MixCoachTheme::textDim());
    g.drawText("P: " + juce::String(telemetry.peakLeft, 1) + "dB",
               bounds.removeFromLeft(70), juce::Justification::centredLeft);
    g.drawText("R: " + juce::String(telemetry.rmsLeft, 1) + "dB",
               bounds.removeFromLeft(70), juce::Justification::centredLeft);

    // Correlation
    auto corrColour = (std::abs(telemetry.correlation) < 0.3f)
        ? MixCoachTheme::error()
        : (telemetry.correlation < 0.0f ? MixCoachTheme::warning() : MixCoachTheme::success());

    g.setColour(corrColour);
    g.drawText("φ: " + juce::String(telemetry.correlation, 2),
               bounds.removeFromLeft(80), juce::Justification::centredLeft);
}

} // namespace mixcoach
