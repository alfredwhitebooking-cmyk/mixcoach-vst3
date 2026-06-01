#include "AnalyzersPanelComponent.h"
#include "../../Common/types/Constants.h"
#include "../../Common/memory/SharedData.h"
#include <juce_graphics/juce_graphics.h>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  AnalyzersPanelComponent — Panel principal con layout 5 sesiones
// ═══════════════════════════════════════════════════════════════════════════

AnalyzersPanelComponent::AnalyzersPanelComponent(SharedData& sharedData)
    : sharedData_(sharedData)
{
    try
    {
        // ─── Playlist (25% izquierda) ──────────────────────────────────────
        playlist_.onSlotSelected = [this](int slotIndex) {
            selectedSlot_ = slotIndex;
            auto& registry = sharedData_.getSlotRegistry();
            auto info = registry.getSlotInfo(slotIndex);
            selectedTrackName_ = juce::String(info.trackName);
            selectedColour_ = info.colour;
            playlist_.setSelectedSlot(slotIndex);
        };
        addAndMakeVisible(playlist_);

        // ─── Meter (25% centro-arriba) ────────────────────────────────────
        addAndMakeVisible(meter_);

        // ─── Spectrum Analyzer (50% arriba-derecha) ───────────────────────
        addAndMakeVisible(spectrograph_);

        // ─── Phase Scope (abajo-centro-izquierda) ─────────────────────────
        addAndMakeVisible(phaseScope_);

        // ─── VU Meters (50% abajo-derecha) ────────────────────────────────
        addAndMakeVisible(vuMeters_);
    }
    catch (const std::exception& e)
    {
        juce::Logger::outputDebugString("[AnalyzersPanelComponent] Exception: "
                                         + juce::String(e.what()));
    }
    catch (...)
    {
        juce::Logger::outputDebugString("[AnalyzersPanelComponent] Unknown exception");
    }
}

void AnalyzersPanelComponent::resized()
{
    try
    {
        auto area = getLocalBounds().reduced(4);

        // ══════════════════════════════════════════════════════════════════
        //  LAYOUT 5 SESIONES
        // ══════════════════════════════════════════════════════════════════
        //
        //  |  Playlist   |  Meter      |  Spectrum Analyzer    |
        //  |  (25% L)    |  (25% C-T)  |  (50% R-Top)          |
        //  |  full height|             |                       |
        //  |             |-------------|-----------------------|
        //  |             | Phase Scope |  VU Meters            |
        //  |             | (C-Bottom)  |  (50% R-Bottom)       |
        //  |             | Vec+Phase   |  L R  → 2 arriba     |
        //  |             | +Crest      |  M S  → 2 abajo      |
        //
        //  Split vertical: 50% top / 50% bottom
        //  Split horizontal: 25% | 25% | 50%
        //
        // ══════════════════════════════════════════════════════════════════

        // ─── Vertical split ───────────────────────────────────────────────
        auto topHalf = area.removeFromTop(area.getHeight() / 2);
        auto bottomHalf = area;

        // ─── Horizontal splits ────────────────────────────────────────────
        int totalW = topHalf.getWidth();

        // Left column = 25% (full height playlist)
        int leftW = (int)(totalW * 0.25f);

        // Center column = 25%
        int centerW = (int)(totalW * 0.25f);

        // Right column = 50% (will be split further for top vs bottom)
        juce::ignoreUnused(totalW - leftW - centerW);

        // ─── TOP ROW ──────────────────────────────────────────────────────
        // Playlist
        auto topLeft = topHalf.removeFromLeft(leftW);
        playlist_.setBounds(topLeft.reduced(2));

        // Meter (center-top)
        auto topCenter = topHalf.removeFromLeft(centerW);
        meter_.setBounds(topCenter.reduced(2));

        // Spectrum Analyzer (right-top)
        auto topRight = topHalf.reduced(2);
        spectrograph_.setBounds(topRight);

        // ─── BOTTOM ROW ──────────────────────────────────────────────────
        // Phase Scope area (bottom-center, same width as Meter)
        auto bottomLeft = bottomHalf.removeFromLeft(leftW);
        juce::ignoreUnused(bottomLeft); // space under Playlist (empty or reserved)

        auto bottomCenter = bottomHalf.removeFromLeft(centerW);
        auto bottomRight = bottomHalf.reduced(2);

        // ─── Phase Scope (abajo-centro) ──────────────────────────────────
        phaseScope_.setBounds(bottomCenter.reduced(2));

        // ─── VU Meters (bottom-right) ─────────────────────────────────────
        vuMeters_.setBounds(bottomRight);
    }
    catch (const std::exception& e)
    {
        juce::Logger::outputDebugString("[AnalyzersPanelComponent::resized] Exception: "
                                         + juce::String(e.what()));
    }
}

void AnalyzersPanelComponent::paint(juce::Graphics& g)
{
    // ─── Fondo oscuro con gradiente sutil ────────────────────────────────
    juce::ColourGradient bgGrad(
        MixCoachTheme::bgDark(),
        juce::Point<float>(0.0f, 0.0f),
        MixCoachTheme::bgDarker(),
        juce::Point<float>(0.0f, (float)getHeight()),
        false);
    g.setGradientFill(bgGrad);
    g.fillRect(getLocalBounds());

    // ─── Grid pattern sutil ──────────────────────────────────────────────
    g.setColour(MixCoachTheme::border().withAlpha(0.04f));
    for (int x = 0; x < getWidth(); x += 50)
        g.drawVerticalLine(x, 0.0f, (float)getHeight());
    for (int y = 0; y < getHeight(); y += 50)
        g.drawHorizontalLine(y, 0.0f, (float)getWidth());
}

void AnalyzersPanelComponent::updateAnalyzers(SlotRegistry& registry)
{
    try
    {
        // ─── 1. Actualizar playlist ───────────────────────────────────────
        playlist_.updateList(registry);

        // ─── 2. Auto-seleccionar primer slot si no hay selección ──────────
        if (selectedSlot_ < 0) {
            registry.forEachActive([&](const SlotInfo& info) {
                if (selectedSlot_ < 0) {
                    selectedSlot_ = info.slotIndex;
                    selectedTrackName_ = juce::String(info.trackName);
                    selectedColour_ = info.colour;
                    playlist_.setSelectedSlot(selectedSlot_);
                }
            });
        }

        // ─── 3. Si no hay pistas activas, resetear ────────────────────────
        if (registry.activeCount() == 0) {
            selectedSlot_ = -1;
            selectedTrackName_.clear();
            return;
        }

        // ─── 4. Actualizar analizadores con datos del slot seleccionado ───
        if (selectedSlot_ >= 0) {
            auto& telem = registry.getTelemetry(selectedSlot_);
            auto latest = telem.latest();

            // ─── Meter panel ────────────────────────────────────────────
            meter_.updateData(latest);

            // ─── Spectrograph (FFT) ─────────────────────────────────────
            if (latest.active) {
                bool hasFFT = false;
                for (int fi = 0; fi < 256 && !hasFFT; ++fi) {
                    if (latest.spectrum[fi] > 0.01f) hasFFT = true;
                }
                if (hasFFT) {
                    spectrograph_.updateSpectrum(latest.spectrum, 256);
                }
            }

            // ─── Phase Scope panel ─────────────────────────────────────
            phaseScope_.setCorrelation(latest.correlation);

            if (latest.sampleL != 0.0f || latest.sampleR != 0.0f) {
                float vectL = juce::jlimit(-1.0f, 1.0f, latest.sampleL * 2.0f);
                float vectR = juce::jlimit(-1.0f, 1.0f, latest.sampleR * 2.0f);
                phaseScope_.pushSample(vectL, vectR);
            }

            if (latest.crestFactor > 0.0f && latest.peakLeft > -60.0f) {
                float rmsAvg = (latest.rmsLeft + latest.rmsRight) * 0.5f;
                phaseScope_.pushCrest(latest.peakLeft, rmsAvg);
            }

            // ─── VU Meters (L, R, M, S) ────────────────────────────────
            float mid = (latest.sampleL + latest.sampleR) * 0.5f;
            float side = (latest.sampleL - latest.sampleR) * 0.5f;

            auto toDb = [](float sample) -> float {
                if (std::abs(sample) < 0.00001f) return -80.0f;
                return 20.0f * std::log10(std::abs(sample));
            };

            vuMeters_.setLevel(0, toDb(latest.sampleL));
            vuMeters_.setLevel(1, toDb(latest.sampleR));
            vuMeters_.setLevel(2, toDb(mid));
            vuMeters_.setLevel(3, toDb(side));
        }
    }
    catch (const std::exception& e)
    {
        juce::Logger::outputDebugString("[AnalyzersPanelComponent::updateAnalyzers] Exception: "
                                         + juce::String(e.what()));
    }
}

void AnalyzersPanelComponent::fastUpdateMeters(SlotRegistry& registry)
{
    try
    {
        if (selectedSlot_ < 0)
            return;

        auto& telem = registry.getTelemetry(selectedSlot_);
        auto latest = telem.latest();

        // ─── Meter panel (actualización rápida de niveles) ───────────────
        meter_.updateData(latest);

        // ─── Phase Scope (correlación + vectorscope + crest) ─────────────
        phaseScope_.setCorrelation(latest.correlation);

        if (latest.sampleL != 0.0f || latest.sampleR != 0.0f) {
            float vectL = juce::jlimit(-1.0f, 1.0f, latest.sampleL * 2.0f);
            float vectR = juce::jlimit(-1.0f, 1.0f, latest.sampleR * 2.0f);
            phaseScope_.pushSample(vectL, vectR);
        }

        if (latest.crestFactor > 0.0f && latest.peakLeft > -60.0f) {
            float rmsAvg = (latest.rmsLeft + latest.rmsRight) * 0.5f;
            phaseScope_.pushCrest(latest.peakLeft, rmsAvg);
        }

        // ─── VU Meters (L, R, M, S) ────────────────────────────────────
        float mid = (latest.sampleL + latest.sampleR) * 0.5f;
        float side = (latest.sampleL - latest.sampleR) * 0.5f;

        auto toDb = [](float sample) -> float {
            if (std::abs(sample) < 0.00001f) return -80.0f;
            return 20.0f * std::log10(std::abs(sample));
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

} // namespace mixcoach
