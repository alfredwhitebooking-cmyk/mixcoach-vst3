#include "TrackDashboardComponent.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  Constructor
// ═══════════════════════════════════════════════════════════════════════════
TrackDashboardComponent::TrackDashboardComponent()
{
    for (auto& bus : buses_) {
        bus.trackCount = 0;
        bus.avgPeakDb  = -100.0f;
        bus.avgRmsDb   = -100.0f;
        bus.crestDb    = 0.0f;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  updateDashboard — Itera slots activos, computa agregados por bus
// ═══════════════════════════════════════════════════════════════════════════
void TrackDashboardComponent::updateDashboard(SlotRegistry& registry, SharedData& sharedData)
{
    // Resetear agregados
    master_ = MasterSummary{};
    for (auto& bus : buses_) {
        bus.trackCount = 0;
        bus.avgPeakDb  = -100.0f;
        bus.avgRmsDb   = -100.0f;
        bus.crestDb    = 0.0f;
    }

    // Variables temporales para promediar por bus
    struct BusAccum {
        int   count = 0;
        float peakSum = 0.0f;
        float rmsSum  = 0.0f;
    };
    std::array<BusAccum, kNumBuses + 1> accum{};

    registry.forEachActive([&](const SlotInfo& info)
    {
        int idx = info.slotIndex;
        if (idx < 0 || idx >= SlotRegistry::kMaxSlots) return;

        master_.totalTracks++;

        TrackAudioResult audio = sharedData.getTrackAudioResult(idx);

        float peak = audio.getPeakCombined();
        float rms  = audio.getRmsCombined();

        // Master clip/warning counts
        if (peak > -0.5f)
            master_.clippingTracks++;
        else if (peak > -6.0f)
            master_.warningTracks++;
        if (peak > -60.0f)
            master_.signalTracks++;

        // Agrupar por bus
        int busIdx = static_cast<int>(info.bus);
        if (busIdx < 0 || busIdx >= kNumBuses)
            busIdx = kNumBuses; // UNASSIGNED

        auto& a = accum[busIdx];
        a.count++;
        a.peakSum += peak;
        a.rmsSum  += rms;
    });

    // Computar promedios por bus
    bool hasAnyData = false;
    for (int i = 0; i <= kNumBuses; ++i) {
        auto& a = accum[i];
        auto& b = buses_[i];
        b.busIndex = i;
        b.trackCount = a.count;
        if (a.count > 0) {
            b.avgPeakDb = a.peakSum / (float)a.count;
            b.avgRmsDb  = a.rmsSum  / (float)a.count;
            b.crestDb   = b.avgPeakDb - b.avgRmsDb;
            hasAnyData = true;
        }
    }

    if (hasAnyData)
        repaint();
}

// ═══════════════════════════════════════════════════════════════════════════
//  resized
// ═══════════════════════════════════════════════════════════════════════════
void TrackDashboardComponent::resized()
{
}

// ═══════════════════════════════════════════════════════════════════════════
//  paint — Dibuja el dashboard completo
// ═══════════════════════════════════════════════════════════════════════════
void TrackDashboardComponent::paint(juce::Graphics& g)
{
    // Glass panel background consistente con otros paneles
    MixCoachTheme::fillGlassPanel(g, getLocalBounds().toFloat(), 6.0f);

    if (master_.totalTracks == 0) {
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.setColour(MixCoachTheme::textMuted());
        g.drawText(juce::CharPointer_UTF8("\xF0\x9F\x94\x8C Conecta los plugins Messenger en tus pistas para ver datos agregados aqui."),
                   getLocalBounds(), juce::Justification::centred);
        return;
    }

    auto area = getLocalBounds().reduced(6, 4);

    // ═══ HEADER ═══════════════════════════════════════════════════════════
    auto headerBounds = area.removeFromTop(kHeaderHeight);
    drawHeader(g, headerBounds);

    // ═══ BUS CARDS ════════════════════════════════════════════════════════
    for (int i = 0; i <= kNumBuses; ++i) {
        auto& bus = buses_[i];
        if (bus.trackCount == 0) continue;
        if (area.getHeight() < kCardHeight + kCardGap) break;

        auto cardBounds = area.removeFromTop(kCardHeight).reduced(0, kCardGap / 2);
        drawBusCard(g, cardBounds, bus);
        area.removeFromTop(1);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawHeader — 📊 MASTER DASHBOARD + track count + clip warning
// ═══════════════════════════════════════════════════════════════════════════
void TrackDashboardComponent::drawHeader(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // ─── Title ──────────────────────────────────────────────────────────────
    g.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
    g.setColour(MixCoachTheme::accent());
    auto titleArea = bounds.removeFromLeft(140);
    g.drawText(juce::CharPointer_UTF8("\xF0\x9F\x93\x8A MASTER DASHBOARD"),
               titleArea, juce::Justification::centredLeft);

    // ─── Track count badge ─────────────────────────────────────────────────
    auto badgeArea = bounds.removeFromLeft(60).reduced(0, 3);
    g.setColour(MixCoachTheme::accentCyan().withAlpha(0.12f));
    g.fillRoundedRectangle(badgeArea.toFloat(), 6.0f);
    g.setColour(MixCoachTheme::accentCyan());
    g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
    g.drawText(juce::String(master_.totalTracks) + " tracks", badgeArea, juce::Justification::centred);

    // ─── Clip warning ─────────────────────────────────────────────────────
    int clipCount = master_.clippingTracks;
    int warnCount = master_.warningTracks;

    if (clipCount > 0) {
        auto clipArea = bounds.removeFromRight(110).reduced(2, 3);
        g.setColour(MixCoachTheme::meterRed().withAlpha(0.12f));
        g.fillRoundedRectangle(clipArea.toFloat(), 6.0f);
        g.setColour(MixCoachTheme::meterRed());
        g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
        g.drawText(juce::String(juce::CharPointer_UTF8("\xE2\x9A\xA0 ")) + juce::String(clipCount) + " clipping",
                   clipArea, juce::Justification::centred);
    } else if (warnCount > 0) {
        auto warnArea = bounds.removeFromRight(110).reduced(2, 3);
        g.setColour(MixCoachTheme::warning().withAlpha(0.10f));
        g.fillRoundedRectangle(warnArea.toFloat(), 6.0f);
        g.setColour(MixCoachTheme::warning());
        g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
        g.drawText(juce::String(juce::CharPointer_UTF8("\xE2\x97\x89 ")) + juce::String(warnCount) + " near limit",
                   warnArea, juce::Justification::centred);
    } else {
        auto okArea = bounds.removeFromRight(90).reduced(2, 3);
        g.setColour(MixCoachTheme::success().withAlpha(0.10f));
        g.fillRoundedRectangle(okArea.toFloat(), 6.0f);
        g.setColour(MixCoachTheme::success());
        g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
        g.drawText(juce::CharPointer_UTF8("\xE2\x9C\x93 All clear"), okArea, juce::Justification::centred);
    }

    // ─── Signal count ──────────────────────────────────────────────────────
    auto sigArea = bounds.removeFromRight(80).reduced(2, 3);
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)));
    g.drawText(juce::String(master_.signalTracks) + " with signal", sigArea, juce::Justification::centredRight);
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawBusCard — Tarjeta de resumen por bus
// ═══════════════════════════════════════════════════════════════════════════
void TrackDashboardComponent::drawBusCard(juce::Graphics& g, juce::Rectangle<int> bounds,
                                           const BusSummary& bus)
{
    auto cardBounds = bounds.toFloat();

    // ─── Glass card background ───────────────────────────────────────────
    MixCoachTheme::fillGlassPanel(g, cardBounds, 4.0f);

    // ─── Colour accent bar (left, 3px) ───────────────────────────────────
    juce::Colour busColour = (bus.busIndex >= 0 && bus.busIndex < kNumBuses)
        ? getBusColour(bus.busIndex)
        : MixCoachTheme::textMuted();
    auto accentBar = bounds.removeFromLeft(3);
    g.setColour(busColour.withAlpha(0.7f));
    g.fillRoundedRectangle(accentBar.toFloat().reduced(0, 4), 1.5f);
    bounds.removeFromLeft(4);

    // ─── Bus name (iconic abbreviation) ──────────────────────────────────
    juce::String busAbbr;
    if (bus.busIndex < kNumBuses) {
        // Use first 3-4 chars of bus name
        busAbbr = juce::String(busNames[bus.busIndex]).substring(0, 4).toUpperCase().trimEnd();
    } else {
        busAbbr = "???";
    }

    auto nameArea = bounds.removeFromLeft(52).reduced(0, 4);
    g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
    g.setColour(busColour);
    g.drawText(busAbbr, nameArea, juce::Justification::centredLeft);

    // ─── Track count badge ───────────────────────────────────────────────
    auto badgeArea = bounds.removeFromLeft(24).reduced(0, 8);
    g.setColour(busColour.withAlpha(0.12f));
    g.fillRoundedRectangle(badgeArea.toFloat(), 4.0f);
    g.setColour(busColour.withAlpha(0.8f));
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened());
    g.drawText(juce::String(bus.trackCount), badgeArea, juce::Justification::centred);

    bounds.removeFromLeft(4);

    // ─── Horizontal meter bar ────────────────────────────────────────────
    auto meterArea = bounds.removeFromLeft(80).reduced(0, 8);

    // Background del meter
    g.setColour(MixCoachTheme::bgDarker().withAlpha(0.6f));
    g.fillRoundedRectangle(meterArea.toFloat(), 2.5f);

    // Fill con gradiente basado en peak
    float peakNorm = juce::jlimit(0.0f, 1.0f, (bus.avgPeakDb + 60.0f) / 66.0f);
    if (peakNorm > 0.01f) {
        auto fillBounds = meterArea.withWidth((int)(meterArea.getWidth() * peakNorm)).toFloat();

        // Color: red (> -6), yellow (> -18), green (else)
        juce::Colour fillCol;
        if (bus.avgPeakDb > -6.0f)
            fillCol = MixCoachTheme::meterRed();
        else if (bus.avgPeakDb > -18.0f)
            fillCol = MixCoachTheme::meterYellow();
        else
            fillCol = MixCoachTheme::meterGreen();

        g.setColour(fillCol.withAlpha(0.85f));
        g.fillRoundedRectangle(fillBounds, 2.5f);
    }

    // Border del meter
    g.setColour(MixCoachTheme::border().withAlpha(0.15f));
    g.drawRoundedRectangle(meterArea.toFloat(), 2.5f, 0.5f);

    bounds.removeFromLeft(4);

    // ─── Stats: Peak / RMS / Crest ───────────────────────────────────────
    auto statsArea = bounds.reduced(0, 6);

    // Peak value
    auto peakArea = statsArea.removeFromLeft(50);
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
    juce::Colour pkCol = (bus.avgPeakDb > -6.0f) ? MixCoachTheme::meterRed()
                        : (bus.avgPeakDb > -18.0f) ? MixCoachTheme::meterYellow()
                        : MixCoachTheme::textDim();
    g.setColour(pkCol);
    g.drawText("Pk " + juce::String(bus.avgPeakDb, 1), peakArea, juce::Justification::centredLeft);

    // RMS value
    auto rmsArea = statsArea.removeFromLeft(50);
    g.setColour(MixCoachTheme::accentCyan().withAlpha(0.7f));
    g.drawText("Rms " + juce::String(bus.avgRmsDb, 1), rmsArea, juce::Justification::centredLeft);

    // Crest factor
    auto crestArea = statsArea.removeFromLeft(45);
    juce::Colour crestCol = (bus.crestDb < 8.0f) ? MixCoachTheme::meterGreen()
                           : (bus.crestDb < 15.0f) ? MixCoachTheme::meterYellow()
                           : MixCoachTheme::meterRed();
    g.setColour(crestCol.withAlpha(0.7f));
    g.drawText("CR " + juce::String(bus.crestDb, 1), crestArea, juce::Justification::centredLeft);
}

} // namespace mixcoach
