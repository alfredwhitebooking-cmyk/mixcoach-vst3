#include "SpectrographComponent.h"
#include <cmath>

namespace mixcoach {

    SpectrographComponent::SpectrographComponent()
    {
        setOpaque(true);
        bandLevels_.reserve(kNumRtaBands);
        bandRmsLevels_.reserve(kNumRtaBands);
        bandPeaks_.resize(kNumRtaBands, 0.0f);
        peakHoldTimers_.resize(kNumRtaBands, 0.0f);

        for (int i = 0; i < kNumRtaBands; ++i) {
            bandLevels_.emplace_back(0.0f, 20.0f, 120.0f);
            bandRmsLevels_.emplace_back(0.0f, 25.0f, 200.0f);
        }

        slopePresetIndex_ = 0;
        rebuildBands();

        for (int i = 0; i < kNumRtaBands; ++i) {
            bandLevels_[(size_t)i].setBallistics(3.0f, 120.0f);
            bandRmsLevels_[(size_t)i].setBallistics(35.0f, 180.0f);
        }

        computeDefaultReferenceCurve();
    }

    SpectrographComponent::~SpectrographComponent()
    {
        if (diagnosticBridge_ != nullptr) diagnosticBridge_->removeChangeListener(this);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Diagnostic Bridge Integration
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::setDiagnosticBridge(DiagnosticBridge* bridge)
    {
        if (diagnosticBridge_ != nullptr) diagnosticBridge_->removeChangeListener(this);

        diagnosticBridge_ = bridge;
        activeDiagnostics_.clear();

        if (diagnosticBridge_ != nullptr) {
            diagnosticBridge_->addChangeListener(this);
            activeDiagnostics_ = diagnosticBridge_->getDiagnostics();
        }

        repaint();
    }

    void SpectrographComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
    {
        if (source != diagnosticBridge_) return;

        if (diagnosticBridge_ != nullptr) activeDiagnostics_ = diagnosticBridge_->getDiagnostics();
        else
            activeDiagnostics_.clear();

        int64_t nowMs = juce::Time::getMillisecondCounter();
        for (const auto& diag : activeDiagnostics_) {
            float bandWidth =
                (diag.highFreqHz > 0.0f && diag.lowFreqHz > 0.0f) ? std::log2(diag.highFreqHz / diag.lowFreqHz) : 0.0f;
            if (bandWidth > 2.0f || bandWidth <= 0.0f) continue;

            float centerHz = std::sqrt(diag.lowFreqHz * diag.highFreqHz);
            juce::String label;
            if (centerHz >= 1000.0f) label = juce::String(centerHz / 1000.0f, 1) + "k";
            else
                label = juce::String(static_cast<int>(centerHz));

            bool alreadyExists = false;
            for (const auto& m : frequencyMarkers_) {
                float logRatio =
                    (centerHz > 0.0f && m.frequencyHz > 0.0f) ? std::abs(std::log2(centerHz / m.frequencyHz)) : 999.0f;
                if (std::abs(m.frequencyHz - centerHz) < 10.0f || logRatio < 0.1f) {
                    alreadyExists = true;
                    break;
                }
            }
            if (alreadyExists) continue;

            juce::String desc = diag.description.substring(0, 60);
            if (diag.trackName.isNotEmpty()) desc = diag.trackName + ": " + desc;

            addFrequencyMarker(centerHz, label, diag.severity, diag.isCritical, diag.isPraise, 10.0f);
        }

        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  resized
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::resized()
    {
        auto bounds      = getLocalBounds().reduced(8, 6);
        auto topRight    = bounds.removeFromRight(28);
        settingsButton_  = topRight.removeFromTop(18);
        referenceButton_ = topRight.removeFromTop(16).reduced(2, 1);
        bounds.removeFromTop(22);
        bounds.removeFromBottom(20);
        plotArea_ = bounds.reduced(0, 1);
        rebuildStaticCache();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint() — main render
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::paint(juce::Graphics& g)
    {
        // ─── Advance pulse animation phase (~1 cycle per 3 seconds) ────────
        highlightPulsePhase_ += 0.035f;  // ~0.035 rad per frame at 60fps ≈ 3s period
        if (highlightPulsePhase_ > juce::MathConstants<float>::twoPi)
            highlightPulsePhase_ -= juce::MathConstants<float>::twoPi;

        pruneFrequencyMarkers();

        g.fillAll(juce::Colours::transparentBlack);

        MixCoachTheme::fillGlassPanel(g, getLocalBounds().toFloat(), 6.0f);

        g.setColour(MixCoachTheme::bgSurface().withAlpha(0.3f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f, 1.0f);

        auto headerArea = getLocalBounds().reduced(8, 6).removeFromTop(18);

        g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
        g.setColour(MixCoachTheme::accentGlow());
        g.drawText("SPECTRUM ANALYZER", headerArea, juce::Justification::centred);

        auto underline = headerArea.withTop(headerArea.getBottom() - 1).toFloat();
        float uw       = (float)headerArea.getWidth();
        float ux       = (float)headerArea.getX();
        float uy       = underline.getY();

        g.setColour(MixCoachTheme::divider().withAlpha(0.3f));
        g.drawHorizontalLine((int)uy + 1, ux, ux + uw);

        float glowCx  = ux + uw * 0.5f;
        float glowW   = juce::jmin(100.0f, uw * 0.6f);
        auto glowRect = juce::Rectangle<float>(glowCx - glowW * 0.5f, uy - 1.0f, glowW, 3.0f);
        juce::ColourGradient glow(MixCoachTheme::accent().withAlpha(0.25f),
                                  glowCx,
                                  uy,
                                  MixCoachTheme::accent().withAlpha(0.0f),
                                  glowCx + glowW * 0.5f,
                                  uy,
                                  false);
        glow.addColour(0.5f, MixCoachTheme::accent().withAlpha(0.10f));
        g.setGradientFill(glow);
        g.fillRect(glowRect);

        if (plotArea_.isEmpty()) return;

        if (!staticCacheValid_) rebuildStaticCache();

        if (staticCacheValid_) g.drawImageAt(staticCache_, plotArea_.getX(), plotArea_.getY());

        if (layout_.valid) {
            auto plotInComponent = layout_.plot.translated((float)plotArea_.getX(), (float)plotArea_.getY());
            drawWaterfall(g, plotInComponent);
            drawRtaBars(g, plotInComponent);
            drawDiagnosticOverlay(g, plotInComponent);
            drawFrequencyMarkers(g, plotInComponent);
            drawFrequencyHighlight(g, plotInComponent);
            drawCentroidMarker(g, plotInComponent);
            drawReferenceOverlay(g, plotInComponent);

            {
                auto dbColInComp = layout_.dbCol.translated((float)plotArea_.getX(), (float)plotArea_.getY());
                drawDbAxis(g, dbColInComp);
            }

            drawFreqAxis(g, plotInComponent);
        }
        else {
            drawFreqAxis(g, getLocalBounds().toFloat().reduced(8, 6));
        }

        drawSettingsChrome(g);

        if (mouseOverPlot_ && layout_.valid) {
            auto plotInComponent = layout_.plot.translated((float)plotArea_.getX(), (float)plotArea_.getY());
            drawHoverCursor(g, plotInComponent);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Mouse Events
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::mouseMove(const juce::MouseEvent& e)
    {
        auto pos = e.position;

        if (referenceButton_.contains(pos.toInt())) {
            if (!mouseOverRefButton_) {
                mouseOverRefButton_ = true;
                repaint();
            }
            return;
        }
        else if (mouseOverRefButton_) {
            mouseOverRefButton_ = false;
            repaint();
        }

        if (!layout_.valid || plotArea_.isEmpty()) return;

        auto plotInComp = layout_.plot.translated((float)plotArea_.getX(), (float)plotArea_.getY());

        if (!plotInComp.contains(pos)) {
            if (mouseOverPlot_) {
                mouseOverPlot_ = false;
                repaint();
            }
            return;
        }

        mouseOverPlot_ = true;
        mousePos_      = pos;

        hoverFreqHz_ = xToFreq(pos.x, plotInComp);

        float closestBandLevel = 0.0f;
        float closestDist      = 1e10f;
        for (int i = 0; i < kNumRtaBands; ++i) {
            float dist = std::abs(bands_[(size_t)i].centerHz - hoverFreqHz_);
            if (dist < closestDist) {
                closestDist      = dist;
                closestBandLevel = bandLevels_[(size_t)i].getCurrent();
            }
        }
        hoverBandLevel_ = closestBandLevel;
        hoverDbLevel_   = displayNormToDb(closestBandLevel);

        repaint();
    }

    void SpectrographComponent::mouseExit(const juce::MouseEvent&)
    {
        bool needsRepaint = false;
        if (mouseOverPlot_) {
            mouseOverPlot_ = false;
            needsRepaint   = true;
        }
        if (mouseOverRefButton_) {
            mouseOverRefButton_ = false;
            needsRepaint        = true;
        }
        if (needsRepaint) repaint();
    }

    void SpectrographComponent::mouseDown(const juce::MouseEvent& e)
    {
        if (referenceButton_.contains(e.getPosition())) {
            referenceEnabled_ = !referenceEnabled_;
            repaint();
            return;
        }

        if (settingsButton_.contains(e.getPosition())) {
            if (e.mods.isCtrlDown() && e.mods.isAltDown()) {
                displayMode_ = static_cast<DisplayMode>((displayMode_ + 1) % 3);
            }
            else if (e.mods.isAltDown() && !e.mods.isCtrlDown()) {
                peakHoldEnabled_ = !peakHoldEnabled_;
            }
            else if (e.mods.isCtrlDown() && e.mods.isShiftDown()) {
                pinkNoiseEnabled_ = !pinkNoiseEnabled_;
                if (pinkNoiseEnabled_) slopePresetIndex_ = 0;
            }
            else if (e.mods.isCtrlDown()) {
                slopePresetIndex_ = (slopePresetIndex_ + 1) % 4;
            }
            else if (e.mods.isShiftDown()) {
                referenceEnabled_ = !referenceEnabled_;
            }
            else {
                waterfallEnabled_ = !waterfallEnabled_;
                if (!waterfallEnabled_) {
                    for (auto& slice : waterfall_) slice.valid = false;
                    waterfallCount_     = 0;
                    waterfallWritePos_  = 0;
                    waterfallFrameSkip_ = 0;
                }
            }
            repaint();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Frequency Highlight API
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::setHighlightFrequency(float frequencyHz,
                                                       float bandwidthHz,
                                                       const juce::String& label,
                                                       juce::Colour colour)
    {
        if (frequencyHz <= 0.0f) { clearHighlight(); return; }

        highlightFreqHz_ = frequencyHz;

        if (bandwidthHz > 0.0f) {
            // Explicit bandwidth
            float halfBand = bandwidthHz * 0.5f;
            highlightLowHz_  = juce::jmax(kMinFreq, frequencyHz - halfBand);
            highlightHighHz_ = frequencyHz + halfBand;
        } else {
            // Auto: 1/3 octave around center freq
            // low = freq / 2^(1/6),  high = freq * 2^(1/6)
            float sixthOct = std::pow(2.0f, 1.0f / 6.0f);
            highlightLowHz_  = juce::jmax(kMinFreq, frequencyHz / sixthOct);
            highlightHighHz_ = juce::jmin(kMaxFreq, frequencyHz * sixthOct);
        }

        highlightLabel_ = label.isNotEmpty() ? label : (frequencyHz >= 1000.0f
                                                         ? juce::String(frequencyHz / 1000.0f, 1) + " kHz"
                                                         : juce::String(static_cast<int>(frequencyHz)) + " Hz");
        highlightColour_ = colour;
        highlightPulsePhase_ = 0.0f;
        highlightActive_ = true;

        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setZoomHighlight — Highlight + zoom in on the frequency region
    //
    //  La frecuencia problema se centra en el espectro y el rango visible
    //  se reduce a ±zoomOctaves octavas alrededor de ella.
    //  El resto del espectro se oscurece y el label se muestra con glow.
    // ═══════════════════════════════════════════════════════════════════════════
    void SpectrographComponent::setZoomHighlight(float frequencyHz,
                                                  const juce::String& label,
                                                  float zoomOctaves)
    {
        if (frequencyHz <= 0.0f) { clearHighlight(); return; }

        // ─── Poner el highlight visual ───────────────────────────────────
        setHighlightFrequency(frequencyHz, 0.0f, label, MixCoachTheme::accentCyanBright());

        // ─── Zoom: centrar la frecuencia y mostrar ±zoomOctaves octavas ──
        float halfRangeOct = zoomOctaves * 0.5f;
        float lowOct  = std::log2(frequencyHz / kMinFreq);
        float highOct = std::log2(kMaxFreq / frequencyHz);

        // Limitar para no salirse del rango 20Hz-20kHz
        float zoomLowOct  = juce::jmax(lowOct - halfRangeOct, 0.0f);
        float zoomHighOct = juce::jmin(highOct + halfRangeOct, std::log2(kMaxFreq / kMinFreq));

        // Si el rango es muy pequeÃ±o (frecuencia cerca de los bordes), expandir
        float totalOct = zoomLowOct + zoomHighOct;
        if (totalOct < zoomOctaves) {
            float deficit = zoomOctaves - totalOct;
            float expandLow  = deficit * (zoomLowOct / (zoomLowOct + zoomHighOct + 0.001f));
            float expandHigh = deficit * (zoomHighOct / (zoomLowOct + zoomHighOct + 0.001f));
            zoomLowOct  = juce::jmin(zoomLowOct + expandLow, std::log2(kMaxFreq / kMinFreq));
            zoomHighOct = juce::jmin(zoomHighOct + expandHigh, std::log2(kMaxFreq / kMinFreq));
        }

        float newMinFreq = frequencyHz / std::pow(2.0f, zoomLowOct);
        float newMaxFreq = frequencyHz * std::pow(2.0f, zoomHighOct);

        displayMinFreq_ = juce::jmax(kMinFreq, newMinFreq);
        displayMaxFreq_ = juce::jmin(kMaxFreq, newMaxFreq);

        // Invalidar cache estÃ¡tico para redibujar todo con el nuevo rango
        invalidateStaticCache();

        repaint();
    }

    void SpectrographComponent::clearZoom()
    {
        displayMinFreq_ = kMinFreq;
        displayMaxFreq_ = kMaxFreq;
        invalidateStaticCache();
        repaint();
    }

    void SpectrographComponent::clearHighlight()
    {
        highlightActive_ = false;
        highlightFreqHz_ = 0.0f;
        highlightLowHz_  = 0.0f;
        highlightHighHz_ = 0.0f;
        highlightLabel_.clear();
        highlightPulsePhase_ = 0.0f;
        // Clear zoom too — return to full frequency range
        displayMinFreq_ = kMinFreq;
        displayMaxFreq_ = kMaxFreq;
        invalidateStaticCache();
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawFrequencyHighlight — Renderiza banda glow en la región resaltada
    //
    //  Dibuja:
    //    1. Fill semitransparente de la región (con alpha pulsante)
    //    2. Borde glow a los lados (2 líneas verticales con gradiente)
    //    3. Label descriptivo centrado arriba de la región
    //    4. Líneas de guía horizontales desde el borde hacia los extremos
    // ═══════════════════════════════════════════════════════════════════════════
    void SpectrographComponent::drawFrequencyHighlight(juce::Graphics& g,
                                                        juce::Rectangle<float> plot) const
    {
        if (!highlightActive_) return;

        // ─── Calcular posición X ──────────────────────────────────────────
        float xLow  = freqToX(highlightLowHz_, plot);
        float xHigh = freqToX(highlightHighHz_, plot);
        xLow  = juce::jmax(plot.getX(), xLow);
        xHigh = juce::jmin(plot.getRight(), xHigh);
        if (xHigh - xLow < 2.0f) return;

        auto regionRect = juce::Rectangle<float>(xLow, plot.getY(), xHigh - xLow, plot.getHeight());

        // ─── PASO 1: DIMMING (Apagar las luces del resto) ─────────────────
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        // Área izquierda
        g.fillRect(plot.withWidth(xLow - plot.getX()));
        // Área derecha
        g.fillRect(plot.withLeft(xHigh));

        // ─── PASO 2: SPOTLIGHT (Iluminar el problema) ─────────────────────
        juce::Colour baseCol = (highlightColour_ == juce::Colour()) ? MixCoachTheme::accentCyanBright() : highlightColour_;
        
        // Glow sutil en el fondo de la zona
        juce::ColourGradient spotlight(baseCol.withAlpha(0.12f),
                                       juce::Point<float>(regionRect.getCentreX(), regionRect.getY()),
                                       baseCol.withAlpha(0.0f),
                                       juce::Point<float>(regionRect.getCentreX(), regionRect.getBottom()),
                                       false);
        g.setGradientFill(spotlight);
        g.fillRect(regionRect);

        // Bordes del spotlight
        g.setColour(baseCol.withAlpha(0.50f));
        g.drawVerticalLine(juce::roundToInt(xLow), plot.getY(), plot.getBottom());
        g.drawVerticalLine(juce::roundToInt(xHigh), plot.getY(), plot.getBottom());

        // ─── PASO 3: LABEL SENIOR ──────────────────────────────────────────
        if (highlightLabel_.isNotEmpty()) {
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
            auto labelRect = regionRect.withHeight(20.0f).translated(0, 10.0f);
            
            // Sombra para el texto para legibilidad máxima
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.drawText(highlightLabel_, labelRect.translated(1, 1), juce::Justification::centred);
            
            g.setColour(juce::Colours::white);
            g.drawText(highlightLabel_, labelRect, juce::Justification::centred);
        }
    }

} // namespace mixcoach
