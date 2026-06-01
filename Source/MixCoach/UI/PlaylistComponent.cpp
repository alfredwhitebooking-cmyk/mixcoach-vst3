#include "PlaylistComponent.h"

namespace mixcoach {

PlaylistComponent::PlaylistComponent()
{
    headerLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x93\x8B Playlist"),
                         juce::dontSendNotification);
    headerLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    headerLabel_.setJustificationType(juce::Justification::centredLeft);
    headerLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(headerLabel_);
}

void PlaylistComponent::updateList(SlotRegistry& registry)
{
    int count = 0;
    registry.forEachActive([&](const SlotInfo& info) {
        if (count >= kMaxEntries) return;
        int idx = info.slotIndex;
        entries_[count].slotIndex = idx;
        entries_[count].info = info;
        entries_[count].selected = (idx == selectedSlot_);
        entries_[count].hasSignal = false;

        auto& telem = registry.getTelemetry(idx);
        auto latest = telem.latest();
        entries_[count].peakLeft = latest.peakLeft;
        entries_[count].peakRight = latest.peakRight;
        entries_[count].hasSignal = (latest.peakLeft > -60.0f || latest.peakRight > -60.0f);

        count++;
    });

    activeCount_ = count;
    repaint();
}

void PlaylistComponent::setSelectedSlot(int slotIndex)
{
    if (selectedSlot_ == slotIndex) return;
    selectedSlot_ = slotIndex;

    for (int i = 0; i < activeCount_; ++i) {
        entries_[i].selected = (entries_[i].slotIndex == slotIndex);
    }
    repaint();
}

void PlaylistComponent::mouseDown(const juce::MouseEvent& e)
{
    auto area = getLocalBounds();
    area.removeFromTop(20);

    int entryHeight = 32;

    for (int i = 0; i < activeCount_; ++i) {
        auto entryBounds = juce::Rectangle<int>(
            area.getX(), area.getY() + i * entryHeight,
            area.getWidth(), entryHeight);
        if (entryBounds.contains(e.getPosition())) {
            if (onSlotSelected)
                onSlotSelected(entries_[i].slotIndex);
            return;
        }
    }
}

void PlaylistComponent::resized()
{
    auto area = getLocalBounds().reduced(4, 2);
    headerLabel_.setBounds(area.removeFromTop(18));
}

void PlaylistComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

    auto area = getLocalBounds().reduced(4, 2);
    auto headerArea = area.removeFromTop(18);
    juce::ignoreUnused(headerArea);

    if (activeCount_ == 0) {
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
        g.drawText("\xF0\x9F\x94\x8C No hay Messengers conectados",
                   area, juce::Justification::centred);
        return;
    }

    int entryHeight = 32;
    for (int i = 0; i < activeCount_; ++i) {
        auto entryBounds = juce::Rectangle<int>(
            area.getX(), area.getY() + i * entryHeight,
            area.getWidth(), entryHeight);
        drawEntry(g, entryBounds, entries_[i], i);
    }
}

void PlaylistComponent::drawEntry(juce::Graphics& g, juce::Rectangle<int> bounds,
                                   const TrackEntry& entry, int /*index*/)
{
    auto b = bounds.toFloat().reduced(1.0f, 2.0f);

    if (entry.selected && selectedSlot_ == entry.slotIndex) {
        g.setColour(MixCoachTheme::accent().withAlpha(0.12f));
        g.fillRoundedRectangle(b, 4.0f);
        g.setColour(MixCoachTheme::accent().withAlpha(0.4f));
        g.drawRoundedRectangle(b, 4.0f, 1.0f);
    } else {
        g.setColour(MixCoachTheme::bgPanel().withAlpha(0.5f));
        g.fillRoundedRectangle(b, 4.0f);
    }

    auto colourBar = b.removeFromLeft(4);
    g.setColour(entry.info.colour);
    g.fillRoundedRectangle(colourBar, 2.0f);

    b.removeFromLeft(4);

    auto dotArea = b.removeFromLeft(8).reduced(3, 12);
    if (entry.hasSignal) {
        g.setColour(MixCoachTheme::success().withAlpha(0.8f));
    } else {
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
    }
    g.fillEllipse(dotArea.toFloat());

    b.removeFromLeft(2);

    auto name = juce::String(entry.info.trackName);
    if (name.isEmpty()) name = "Track " + juce::String(entry.slotIndex);
    g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
    g.setColour(entry.selected ? MixCoachTheme::textBright() : MixCoachTheme::textPrimary());
    g.drawText(name, b.reduced(1, 0).toNearestInt(), juce::Justification::centredLeft);

    if (entry.hasSignal && b.getHeight() > 20) {
        auto miniMeter = b.removeFromBottom(4).reduced(2, 0).toFloat();
        g.setColour(MixCoachTheme::bgDarker());
        g.fillRoundedRectangle(miniMeter, 1.0f);

        float norm = juce::jlimit(0.0f, 1.0f, (entry.peakLeft + 60.0f) / 66.0f);
        if (norm > 0.01f) {
            auto fillWidth = miniMeter.getWidth() * norm;
            g.setColour(entry.peakLeft > -6.0f ? MixCoachTheme::error()
                        : entry.peakLeft > -12.0f ? MixCoachTheme::warning()
                        : MixCoachTheme::success());
            g.fillRoundedRectangle(miniMeter.withWidth(fillWidth), 1.0f);
        }
    }
}

} // namespace mixcoach
