#include "AnalyzersPanelComponent.h"
#include "../../Common/types/Constants.h"
#include "../../Common/memory/SharedData.h"
#include <juce_graphics/juce_graphics.h>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  AnalyzersPanelComponent — Panel de metering profesional
//  Layout perfecto 25/25/50:
//
//  ┌──────────────┬───────────────────┬────────────────────────────────┐
//  │              │    METER          │                                │
//  │  PLAYLIST    │   (25% center     │   SPECTROGRAPH (50% W)        │
//  │  (25% W)     │    top 50%)       │   (50% H top)                  │
//  │  FULL HEIGHT ├───────────────────┤                                │
//  │  scroll      │   PHASE SCOPE     ├────────────────────────────────┤
//  │              │   (25% center     │   VU METERS (50% W)            │
//  │              │    bottom 50%)    │   (50% H bottom)               │
//  │              │   Vec+Phase+Crest │   L ██  R ██  M ░░  S ░░      │
//  └──────────────┴───────────────────┴────────────────────────────────┘
// ═══════════════════════════════════════════════════════════════════════════

AnalyzersPanelComponent::AnalyzersPanelComponent(SharedData& sharedData)
    : sharedData_(sharedData)
{
    try
    {
        // ─── Playlist (izquierda, scrollable) ──────────────────────────
        playlist_.onSlotSelected = [this](int slotIndex) {
            selectedSlot_ = slotIndex;
            auto& registry = sharedData_.getSlotRegistry();
            auto info = registry.getSlotInfo(slotIndex);
            selectedColour_ = info.colour;
            playlist_.setSelectedSlot(slotIndex);
            if (onTrackSelected)
                onTrackSelected(slotIndex);
        };
        playlistViewport_.setViewedComponent(&playlist_, false);
        playlistViewport_.setScrollBarsShown(true, false);
        playlistViewport_.setScrollBarThickness(6);
        playlistViewport_.getVerticalScrollBar().setColour(
            juce::ScrollBar::thumbColourId, MixCoachTheme::accent().withAlpha(0.3f));
        playlistViewport_.getVerticalScrollBar().setColour(
            juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(playlistViewport_);

        // ─── Meter ──────────────────────────────────────────────────────
        addAndMakeVisible(meter_);

        // ─── Spectrograph ────────────────────────────────────────────────
        addAndMakeVisible(spectrograph_);

        // ─── Phase Scope (vectorscope + phase corr + crest) ─────────────
        addAndMakeVisible(phaseScope_);

        // ─── VU Meters ──────────────────────────────────────────────────
        addAndMakeVisible(vuMeters_);

        // ─── Footer labels (siempre visibles debajo del playlist) ─────
        footerActiveLabel_.setText("Mensajeros activos: 0", juce::dontSendNotification);
        footerActiveLabel_.setFont(juce::Font(juce::FontOptions(7.0f)));
        footerActiveLabel_.setJustificationType(juce::Justification::centredLeft);
        footerActiveLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
        addAndMakeVisible(footerActiveLabel_);

        footerStatusLabel_.setText(juce::CharPointer_UTF8("\xE2\x97\x8F Conectado"), juce::dontSendNotification);
        footerStatusLabel_.setFont(juce::Font(juce::FontOptions(7.0f)));
        footerStatusLabel_.setJustificationType(juce::Justification::centredRight);
        footerStatusLabel_.setColour(juce::Label::textColourId, MixCoachTheme::success().withAlpha(0.7f));
        addAndMakeVisible(footerStatusLabel_);
    }
    catch (const std::exception& e)
    {
        juce::Logger::outputDebugString("[AnalyzersPanelComponent] Exception: "
                                         + juce::String(e.what()));
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Layout perfecto: 25% | 25% | 50%
// ═══════════════════════════════════════════════════════════════════════════
void AnalyzersPanelComponent::resized()
{
    try
    {
        auto area = getLocalBounds().reduced(2);

        // ─── Columnas: Playlist 25% | Center 25% | Right 50% ────────────
        int playlistW = (int)(area.getWidth() * 0.25f);
        int centerW   = (int)(area.getWidth() * 0.25f);

        auto playlistArea = area.removeFromLeft(playlistW).reduced(1);
        auto centerCol    = area.removeFromLeft(centerW);
        auto rightCol     = area.reduced(1, 0);

        // ─── Columna central partida a la mitad: Meter (top) + PhaseScope (bottom) ──
        auto centerTop  = centerCol.removeFromTop(centerCol.getHeight() / 2).reduced(1);
        auto centerBot  = centerCol.reduced(1);

        // ─── Columna derecha con asimetría: Spectrograph ~55% + VU ~45% ──
        auto rightTop   = rightCol.removeFromTop((int)(rightCol.getHeight() * 0.55f)).reduced(1);
        auto rightBot   = rightCol.reduced(1);

        // ─── Footer fijo (18px) debajo del viewport del playlist ──────
        auto playlistFooterArea = playlistArea.removeFromBottom(18).reduced(4, 2);
        auto footerLeft  = playlistFooterArea.removeFromLeft(playlistFooterArea.getWidth() / 2);
        auto footerRight = playlistFooterArea;
        footerActiveLabel_.setBounds(footerLeft);
        footerStatusLabel_.setBounds(footerRight);

        // ─── Asignar bounds ──────────────────────────────────────────────
        playlistViewport_.setBounds(playlistArea);
        int prefH = playlist_.getPreferredHeight();
        playlist_.setSize(playlistArea.getWidth(), juce::jmax(prefH, playlistArea.getHeight()));
        // ═══ CRÍTICO: Forzar re-evaluación de scroll bars ─══════════════════
        // setBounds() ya llamó a Viewport::resized() internamente, pero en ese
        // momento playlist_ aún tenía su tamaño viejo (antes de setSize). Sin
        // este resized() explícito, los scroll bars no aparecen hasta el próximo
        // resize de la ventana, dejando la lista inaccesible al cambiar de tab.
        playlistViewport_.resized();
        meter_.setBounds(centerTop);
        phaseScope_.setBounds(centerBot);
        spectrograph_.setBounds(rightTop);
        vuMeters_.setBounds(rightBot);
    }
    catch (const std::exception& e)
    {
        juce::Logger::outputDebugString("[AnalyzersPanelComponent::resized] Exception: "
                                         + juce::String(e.what()));
    }
}

void AnalyzersPanelComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // ─── Fondo ─────────────────────────────────────────────────────────
    g.fillAll(MixCoachTheme::bgDark());

    // ─── Grid sutil ──────────────────────────────────────────────────
    g.setColour(MixCoachTheme::border().withAlpha(0.03f));
    for (int x = 0; x < bounds.getWidth(); x += 48)
        g.drawVerticalLine(x, 0.0f, (float)bounds.getHeight());
    for (int y = 0; y < bounds.getHeight(); y += 48)
        g.drawHorizontalLine(y, 0.0f, (float)bounds.getWidth());

    // ─── Panel backgrounds ────────────────────────────────────────────
    auto area = getLocalBounds().reduced(2);

    int plW = (int)(area.getWidth() * 0.25f);
    int ctW = (int)(area.getWidth() * 0.25f);

    // Playlist panel
    auto plPanel = area.removeFromLeft(plW).reduced(1);
    // Center panel bounds
    auto ctPanel = area.removeFromLeft(ctW);
    auto rtPanel = area.reduced(1, 0);

    auto ctTop = ctPanel.removeFromTop(ctPanel.getHeight() / 2).reduced(1);
    auto ctBot = ctPanel.reduced(1);
    auto rtTop = rtPanel.removeFromTop((int)(rtPanel.getHeight() * 0.55f)).reduced(1);
    auto rtBot = rtPanel.reduced(1);

    auto drawPanel = [&](juce::Rectangle<int> r) {
        auto rf = r.toFloat();
        g.setColour(MixCoachTheme::bgPanel().withAlpha(0.35f));
        g.fillRoundedRectangle(rf, 5.0f);
        g.setColour(MixCoachTheme::border().withAlpha(0.12f));
        g.drawRoundedRectangle(rf, 5.0f, 0.5f);
    };

    drawPanel(plPanel);
    drawPanel(ctTop);
    drawPanel(ctBot);
    drawPanel(rtTop);
    drawPanel(rtBot);

    // ─── Section labels (estilo referencia: SESIÓN N – NOMBRE, violeta ALL CAPS) ──
    g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
    g.setColour(MixCoachTheme::accentGlow());
    g.drawText("SESIÓN 1 – PLAYLIST", plPanel.reduced(4, 0).withHeight(12), juce::Justification::centredLeft);
    g.drawText("SESIÓN 2 – METER", ctTop.reduced(4, 0).withHeight(12), juce::Justification::centredLeft);
    g.drawText("SESIÓN 5 – PHASE SCOPE", ctBot.reduced(4, 0).withHeight(12), juce::Justification::centredLeft);
    g.drawText("SESIÓN 3 – SPECTRUM ANALYZER", rtTop.reduced(4, 0).withHeight(12), juce::Justification::centredLeft);
    g.drawText("SESIÓN 4 – VU METERS", rtBot.reduced(4, 0).withHeight(12), juce::Justification::centredLeft);

    // ─── Separadores verticales sutiles ──────────────────────────────
    g.setColour(MixCoachTheme::divider().withAlpha(0.08f));
    int col1X = plPanel.getRight() + 1;
    int col2X = ctPanel.getX() - 1;
    g.drawVerticalLine(col1X, 2.0f, (float)(bounds.getHeight() - 2));
    g.drawVerticalLine(col2X, 2.0f, (float)(bounds.getHeight() - 2));
}

void AnalyzersPanelComponent::updateAnalyzers(SlotRegistry& registry, double sampleRate)
{        spectrograph_.setSampleRate(sampleRate);
    try
    {
        playlist_.updateList(registry);

        // ═══ Actualizar tamaño del playlist + Viewport después de updateList ═══
        // CRÍTICO: resized() establece el tamaño del playlist UNA VEZ al inicio,
        // pero en ese momento activeCount_ puede ser 0 (aún no hay datos).
        // updateList() cambia activeCount_ y por tanto getPreferredHeight(),
        // pero el componente mantiene el tamaño viejo. Sin esta actualización,
        // el Viewport nunca muestra scroll bars aunque haya 20 tracks.
        {
            auto viewBounds = playlistViewport_.getBounds();
            int prefH = playlist_.getPreferredHeight();
            int newH = juce::jmax(prefH, viewBounds.getHeight());
            if (playlist_.getHeight() != newH)
            {
                playlist_.setSize(viewBounds.getWidth(), newH);
                playlistViewport_.resized();
            }
        }

        // ─── Auto-select first active slot ────────────────────────────
        if (selectedSlot_ < 0) {
            registry.forEachActive([&](const SlotInfo& info) {
                if (selectedSlot_ < 0) {
                    selectedSlot_ = info.slotIndex;
                    selectedColour_ = info.colour;
                    playlist_.setSelectedSlot(selectedSlot_);
                }
            });
        }

        if (registry.activeCount() == 0) {
            selectedSlot_ = -1;
            return;
        }

        // ─── Update footer ─────────────────────────────────────────────
        int activeCount = registry.activeCount();
        if (activeCount != lastActiveCount_) {
            lastActiveCount_ = activeCount;
            footerActiveLabel_.setText("Mensajeros activos: " + juce::String(activeCount),
                                        juce::dontSendNotification);
        }

        // ─── Update all analyzers ─────────────────────────────────────
        if (selectedSlot_ >= 0) {
            auto& telem = registry.getTelemetry(selectedSlot_);
            auto latest = telem.latest();

            meter_.updateData(latest);

            if (latest.active) {
                bool hasFFT = false;
                for (int fi = 0; fi < 256 && !hasFFT; ++fi)
                    if (latest.spectrum[fi] > 0.01f) hasFFT = true;
                if (hasFFT)
                    spectrograph_.updateSpectrum(latest.spectrum, 256);
            }

            phaseScope_.setCorrelation(latest.correlation);
            phaseScope_.getVectorscope().setDisplayCorrelation(latest.correlation);
            if (latest.sampleL != 0.0f || latest.sampleR != 0.0f) {
                float vL = juce::jlimit(-1.0f, 1.0f, latest.sampleL * 2.0f);
                float vR = juce::jlimit(-1.0f, 1.0f, latest.sampleR * 2.0f);
                phaseScope_.pushSample(vL, vR);
            }
            if (latest.crestFactor > 0.0f && latest.peakLeft > -60.0f) {
                float rms = (latest.rmsLeft + latest.rmsRight) * 0.5f;
                phaseScope_.pushCrest(latest.peakLeft, rms);
            }

            float mid = (latest.sampleL + latest.sampleR) * 0.5f;
            float side = (latest.sampleL - latest.sampleR) * 0.5f;
            auto toDb = [](float s) -> float {
                return (std::abs(s) < 0.00001f) ? -80.0f : 20.0f * std::log10(std::abs(s));
            };
            vuMeters_.setLevel(0, toDb(latest.sampleL));
            vuMeters_.setLevel(1, toDb(latest.sampleR));
            vuMeters_.setLevel(2, toDb(mid));
            vuMeters_.setLevel(3, toDb(side));
        }
    }
    catch (const std::exception& e)
    {
        juce::Logger::outputDebugString("[AnalyzersPanelComponent] Exception: "
                                         + juce::String(e.what()));
    }
}

void AnalyzersPanelComponent::fastUpdateMeters(SlotRegistry& registry)
{
    try
    {
        if (selectedSlot_ < 0) return;

        auto& telem = registry.getTelemetry(selectedSlot_);
        auto latest = telem.latest();

        meter_.updateData(latest);
        phaseScope_.setCorrelation(latest.correlation);
        phaseScope_.getVectorscope().setDisplayCorrelation(latest.correlation);

        if (latest.sampleL != 0.0f || latest.sampleR != 0.0f) {
            float vL = juce::jlimit(-1.0f, 1.0f, latest.sampleL * 2.0f);
            float vR = juce::jlimit(-1.0f, 1.0f, latest.sampleR * 2.0f);
            phaseScope_.pushSample(vL, vR);
        }
        if (latest.crestFactor > 0.0f && latest.peakLeft > -60.0f) {
            float rms = (latest.rmsLeft + latest.rmsRight) * 0.5f;
            phaseScope_.pushCrest(latest.peakLeft, rms);
        }

        float mid = (latest.sampleL + latest.sampleR) * 0.5f;
        float side = (latest.sampleL - latest.sampleR) * 0.5f;
        auto toDb = [](float s) -> float {
            return (std::abs(s) < 0.00001f) ? -80.0f : 20.0f * std::log10(std::abs(s));
        };
        vuMeters_.setLevel(0, toDb(latest.sampleL));
        vuMeters_.setLevel(1, toDb(latest.sampleR));
        vuMeters_.setLevel(2, toDb(mid));
        vuMeters_.setLevel(3, toDb(side));
    }
    catch (const std::exception& e)
    {
        juce::Logger::outputDebugString("[AnalyzersPanelComponent::fastUpdateMeters] Exception: "
                                         + juce::String(e.what()));
    }
}

void AnalyzersPanelComponent::smoothVisuals(double sampleRateHz)
{
    if (! isShowing())
        return;

    meter_.advanceVisuals(sampleRateHz);
    spectrograph_.smoothSpectrum(sampleRateHz);
    vuMeters_.advanceMeters(sampleRateHz);
    phaseScope_.advanceVisuals(sampleRateHz);
}

// ═══════════════════════════════════════════════════════════════════════════
//  setSelectedSlot — Selección desde fuera (Tab 1 -> Tab 2 sync)
// ═══════════════════════════════════════════════════════════════════════════
void AnalyzersPanelComponent::setSelectedSlot(int slotIndex)
{
    try
    {
        if (slotIndex == selectedSlot_)
            return;

        selectedSlot_ = slotIndex;
        auto& registry = sharedData_.getSlotRegistry();

        if (slotIndex >= 0)
        {
            auto info = registry.getSlotInfo(slotIndex);
            selectedColour_ = info.colour;
        }

        playlist_.setSelectedSlot(slotIndex);

        // Update all analyzers for the new slot
        if (slotIndex >= 0 && slotIndex < SlotRegistry::kMaxSlots)
        {
            auto& telem = registry.getTelemetry(slotIndex);
            auto latest = telem.latest();
            meter_.updateData(latest);
            // Don't force spectrograph update here — fastUpdateSpectrograph handles it
        }
    }
    catch (const std::exception& e)
    {
        juce::Logger::outputDebugString("[AnalyzersPanelComponent::setSelectedSlot] Exception: "
                                         + juce::String(e.what()));
    }
}

} // namespace mixcoach
