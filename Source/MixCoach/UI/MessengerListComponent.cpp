#include "MessengerListComponent.h"
#include "../../Common/types/Constants.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  MessengerListComponent Implementation
//  Lista agrupada por buses con cabeceras de sección coloreadas
// ═══════════════════════════════════════════════════════════════════════════

MessengerListComponent::MessengerListComponent()
{
    titleLabel_.setText(juce::String(juce::CharPointer_UTF8("\xF0\x9F\x93\xA1 Pistas Detectadas")), juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeHeader)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accent());
    addAndMakeVisible(titleLabel_);

    emptyLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x94\x8C Conecta plugins Messenger en tus pistas para verlas aqui.\n\nCada pista mostrara su nivel, bus y una sugerencia de IA."), juce::dontSendNotification);
    emptyLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)));
    emptyLabel_.setJustificationType(juce::Justification::centred);
    emptyLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
    addAndMakeVisible(emptyLabel_);
}

void MessengerListComponent::resized()
{
    auto area = getLocalBounds().reduced(4);
    titleLabel_.setBounds(area.removeFromTop(22));
    emptyLabel_.setBounds(area);
}

// Fast synchronous scan of messengers. Called when the component becomes visible or when the registry changes.
void MessengerListComponent::updateMessengers(SlotRegistry& registry)
{
    // Store pointer for future visibility checks
    registryPtr = &registry;

    if (isPaused_) return;

    const uint64_t currentChangeCount = registry.getChangeCount();

    if (displayVersion_ == currentChangeCount)
    {
        // Update peak meters etc.
        registry.forEachActive([&](const SlotInfo& info)
        {
            int idx = info.slotIndex;
            if (idx >= 0 && idx < SlotRegistry::kMaxSlots)
            {
                auto& telem = registry.getTelemetry(idx);
                auto latest = telem.latest();
                messengers_[idx].peakLeft  = latest.peakLeft;
                messengers_[idx].peakRight = latest.peakRight;
                messengers_[idx].rmsAvg    = (latest.rmsLeft + latest.rmsRight) * 0.5f;
                // Signal present if peak is above -60 dB (i.e., audible)
                messengers_[idx].hasSignal = (latest.peakLeft > -60.0f || latest.peakRight > -60.0f);
            }
        });
        repaint();
        return;
    }

    // Reset layout
    for (auto& group : busGroups_) group.count = 0;
    activeMessengerCount_ = 0;

    // Clear previous messenger entries.
    for (auto& entry : messengers_)
    {
        entry = MessengerEntry{};
    }
    // Populate from registry.
    registry.forEachActive([&](const SlotInfo& info)
    {
        int idx = info.slotIndex;
        if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;
        auto& entry = messengers_[idx];
        entry.info = info;

        auto& telem = registry.getTelemetry(idx);
        auto latest = telem.latest();
        entry.peakLeft = latest.peakLeft;
        entry.peakRight = latest.peakRight;
        entry.rmsAvg = (latest.rmsLeft + latest.rmsRight) * 0.5f;
        // Signal present if peak is above -60 dB (i.e., audible)
        entry.hasSignal = (latest.peakLeft > -60.0f || latest.peakRight > -60.0f);
        
        // Increment active count.
        ++activeMessengerCount_;
        // Assign to bus group.
        int busIdx = static_cast<int>(info.bus);
        if (busIdx < 0) busIdx = kNumBuses; // Unassigned group.
        auto& group = busGroups_[busIdx];
        if (group.count < SlotRegistry::kMaxSlots)
        {
            group.slotIndices[group.count++] = idx;
        }
    });
    // Update display version to current change count.
    displayVersion_ = currentChangeCount;
    repaint();
}

// Background scanner implementation


// Destructor implementation
MessengerListComponent::~MessengerListComponent()
{
    // No background thread to stop
    // No background thread to stop; timer not used
    // stopTimer(); // removed as timer is not used
}

void MessengerListComponent::setRegistry(SlotRegistry* reg) noexcept
{
    registryPtr = reg;
}

void MessengerListComponent::visibilityChanged()
{
    // Use isShowing() to correctly detect when the component is actually hidden/minimized.
    isPaused_ = !isShowing();
    if (!isPaused_ && registryPtr)
    {
        updateMessengers(*registryPtr);
    }
}
void MessengerListComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds();

    MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), 6.0f);

    auto area = bounds.reduced(4);
    auto titleArea = area.removeFromTop(22);

    auto badgeArea = titleArea.removeFromRight(80);
    g.setColour(MixCoachTheme::accent().withAlpha(0.2f));
    g.fillRoundedRectangle(badgeArea.toFloat(), 8.0f);
    g.setColour(MixCoachTheme::accent());
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
    g.drawText(juce::String(activeMessengerCount_) + " activo" +
               (activeMessengerCount_ != 1 ? "s" : ""),
               badgeArea, juce::Justification::centred);

    if (activeMessengerCount_ == 0) {
        emptyLabel_.setVisible(true);
        return;
    }
    emptyLabel_.setVisible(false);

    // Increased limit to display all detected messengers per bus (no practical cap).

    const int kHeaderHeight = 20;

    for (int busIdx = 0; busIdx <= kNumBuses; ++busIdx) {
        auto& group = busGroups_[busIdx];
        if (group.count == 0) continue;

        if (area.getHeight() < kHeaderHeight + kRowHeight) break;
        drawBusHeader(g, area, busIdx, group.count);

        int rowsInGroup = juce::jmin(group.count, kMaxRowsPerBus);
        for (int r = 0; r < rowsInGroup; ++r) {
            if (area.getHeight() < kRowHeight) break;
            int slotIdx = group.slotIndices[r];
            auto rowArea = area.removeFromTop(kRowHeight).reduced(2, 2);
            drawMessengerRow(g, rowArea, messengers_[slotIdx], r);
        }

        if (group.count > kMaxRowsPerBus) {
            if (area.getHeight() < 14) break;
            auto moreArea = area.removeFromTop(14).reduced(4, 0);
            g.setColour(MixCoachTheme::textMuted());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
            g.drawText("+ " + juce::String(group.count - kMaxRowsPerBus) + " mas...",
                       moreArea, juce::Justification::centredRight);
        }
    }
}

// ─── Cabecera de sección de bus ─────────────────────────────────────────────
void MessengerListComponent::drawBusHeader(juce::Graphics& g, juce::Rectangle<int>& bounds,
                                            int busIdx, int count)
{
    auto headerArea = bounds.removeFromTop(20).reduced(2, 0);

    juce::Colour busColour;
    juce::String busName;
    const char* busIcon;

    if (busIdx == kNumBuses) {
        busColour = MixCoachTheme::textMuted();
        busName = "SIN ASIGNAR";
        busIcon = "\xE2\x97\x8B";
    } else {
        busColour = getBusColour(busIdx);
        busName = juce::String(busNames[busIdx]).toUpperCase();
        static const char* icons[] = {
            "\xF0\x9F\xA5\x81",
            "\xF0\x9F\x8E\xB8",
            "\xF0\x9F\x8E\xA8",
            "\xF0\x9F\x8E\xB9",
            "\xF0\x9F\x8E\xA4",
            "\xF0\x9F\x94\x80"
        };
        busIcon = icons[busIdx];
    }

    auto accentBar = headerArea.removeFromLeft(3);
    g.setColour(busColour);
    g.fillRoundedRectangle(accentBar.toFloat(), 1.5f);

    auto iconArea = headerArea.removeFromLeft(18);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.setColour(busColour);
    g.drawText(juce::String(busIcon), iconArea.reduced(1, 1), juce::Justification::centred);

    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    g.setColour(busColour);
    auto nameArea = headerArea.removeFromLeft(80);
    g.drawText(busName, nameArea, juce::Justification::centredLeft);

    auto badgeArea = headerArea.removeFromLeft(30).reduced(0, 2);
    g.setColour(busColour.withAlpha(0.15f));
    g.fillRoundedRectangle(badgeArea.toFloat(), 6.0f);
    g.setColour(busColour);
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
    g.drawText(juce::String(count), badgeArea, juce::Justification::centred);

    auto lineY = headerArea.getBottom();
    g.setColour(busColour.withAlpha(0.15f));
    g.drawHorizontalLine(lineY, 4.0f, (float)getWidth() - 4);
}

// ─── Fila de Messenger — Diseño profesional tipo IK Multimedia ──────────────
void MessengerListComponent::drawMessengerRow(juce::Graphics& g, juce::Rectangle<int> bounds,
                                                const MessengerEntry& entry, int index)
{
    juce::ignoreUnused(index);

    g.setColour(MixCoachTheme::bgDarker().withAlpha(0.6f));
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

    auto topGlow = bounds.withHeight(2);
    g.setColour(juce::Colours::white.withAlpha(0.03f));
    g.fillRect(topGlow);

    auto colourBar = bounds.removeFromLeft(4);
    g.setColour(entry.info.colour);
    g.fillRoundedRectangle(colourBar.toFloat(), 2.0f);

    bounds.removeFromLeft(4);

    auto statusDot = bounds.removeFromLeft(10).reduced(0, 12);
    if (entry.hasSignal) {
        float pulse = 0.6f + 0.4f * std::sin(juce::Time::getMillisecondCounter() * 0.006f);
        g.setColour(MixCoachTheme::success().withAlpha(pulse));
    } else {
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
    }
    g.fillEllipse(statusDot.toFloat());

    bounds.removeFromLeft(2);

    auto routeArea = bounds.removeFromRight(86).reduced(2, 8);
    auto statsArea = bounds.removeFromRight(92).reduced(2, 5);
    auto barArea = bounds.removeFromRight(58).reduced(2, 12);

    auto nameArea = bounds.reduced(0, 4);
    auto nameTop = nameArea.removeFromTop(22);
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)).boldened());
    g.setColour(MixCoachTheme::textBright());
    auto name = juce::String(entry.info.trackName).trim();
    if (name.isEmpty())
        name = "Pista " + juce::String(entry.info.slotIndex + 1);
    g.drawFittedText(name, nameTop, juce::Justification::centredLeft, 1);

    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    g.setColour(MixCoachTheme::textMuted());
    g.drawText("Slot " + juce::String(entry.info.slotIndex + 1) +
               (entry.hasSignal ? "  |  recibiendo audio" : "  |  conectado"),
               nameArea, juce::Justification::centredLeft);

    float norm = juce::jlimit(0.0f, 1.0f, (entry.peakLeft + 60.0f) / 66.0f);

    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(barArea.toFloat(), 2.0f);

    juce::Colour barColour;
    if (entry.peakLeft > -6.0f)
        barColour = MixCoachTheme::error();
    else if (entry.peakLeft > -12.0f)
        barColour = MixCoachTheme::warning();
    else if (entry.peakLeft > -18.0f)
        barColour = MixCoachTheme::success();
    else
        barColour = MixCoachTheme::meterBlue();

    auto fillBar = barArea.withRight(barArea.getX() + (int)(barArea.getWidth() * norm));
    if (fillBar.getWidth() > 1) {
        g.setColour(barColour);
        g.fillRoundedRectangle(fillBar.toFloat(), 2.0f);
        if (norm > 0.1f) {
            auto glowBar = fillBar.withLeft(fillBar.getRight() - juce::jmax(3, fillBar.getWidth() / 4));
            juce::ColourGradient barGlowGrad(
                juce::Colours::white.withAlpha(0.3f),
                (float)glowBar.getX(), 0.0f,
                juce::Colour(0x00000000),
                (float)glowBar.getRight(), 0.0f,
                false);
            g.setGradientFill(barGlowGrad);
            g.fillRect(glowBar);
        }
    }

    auto rmsArea = statsArea.removeFromTop(statsArea.getHeight() / 2);
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    g.setColour(MixCoachTheme::textDim());
    g.drawText("RMS " + juce::String(entry.rmsAvg, 1) + " dB", rmsArea, juce::Justification::centredLeft);

    g.setColour(barColour);
    g.drawText("PK  " + juce::String(entry.peakLeft, 1) + " dB", statsArea, juce::Justification::centredLeft);

    if (entry.info.bus != BusType::None) {
        int busIdx = static_cast<int>(entry.info.bus);
        g.setColour(getBusColour(busIdx).withAlpha(0.2f));
        g.fillRoundedRectangle(routeArea.toFloat(), 4.0f);
        g.setColour(getBusColour(busIdx));
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
        g.drawFittedText("Ruta\n" + juce::String(busNames[busIdx]), routeArea, juce::Justification::centred, 2);
    } else {
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
        g.drawFittedText("Ruta\nSin bus", routeArea, juce::Justification::centred, 2);
    }
}

} // namespace mixcoach
