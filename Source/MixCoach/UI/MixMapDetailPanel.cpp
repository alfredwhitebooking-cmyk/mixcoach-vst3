#include "MixMapDetailPanel.h"
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  MixMapDetailPanel — Panel lateral con detalles de pista seleccionada
    // ═══════════════════════════════════════════════════════════════════════════

    MixMapDetailPanel::MixMapDetailPanel()
    {
        setOpaque(false);
        setSize(kPanelWidth, 10); // Altura mínima, resized() la ajusta

        rmsSmooth_.setBallistics(50.0f, 400.0f);
        rmsSmooth_.reset(-80.0f);

        // Timer NO se inicia aquí — se inicia cuando setTrackData() es llamado
    }


    // ═══════════════════════════════════════════════════════════════════════════
    //  visibilityChanged — Pausa timer cuando el plugin no es visible
    // ═══════════════════════════════════════════════════════════════════════════

    void MixMapDetailPanel::visibilityChanged()
    {
        if (isShowing() && !isTimerRunning()) {
            startTimerHz(10);
        }
        // Nunca detener el timer — las actualizaciones del panel de
        // detalle (pin toggle, track data) deben seguir activas.
    }

    MixMapDetailPanel::~MixMapDetailPanel()
    {
        stopTimer();
    }

    void MixMapDetailPanel::setTrackData(int slotIndex,
                                          const juce::String& trackName,
                                          const juce::String& roleName,
                                          float peakDb,
                                          float rmsDb,
                                          float correlation,
                                          float stereoWidth,
                                          const float* bandEnergies,
                                          float targetLevelDb,
                                          float roleConfidence)
    {
        slotIndex_ = slotIndex;
        trackName_ = trackName;
        roleName_  = roleName;
        peakDb_    = peakDb;
        rmsDb_     = rmsDb;
        correlation_   = correlation;
        stereoWidth_   = stereoWidth;
        targetLevelDb_ = targetLevelDb;
        roleConfidence_ = roleConfidence;

        // Store all 30 bands directly for the EQ curve display
        if (bandEnergies != nullptr) {
            for (int b = 0; b < kNumBands; ++b)
                bandEnergies_[b] = (bandEnergies[b] > -90.0f) ? bandEnergies[b] : -100.0f;
        } else {
            for (int b = 0; b < kNumBands; ++b) bandEnergies_[b] = -100.0f;
        }

        rmsSmooth_.setTargetValue(juce::jlimit(-80.0f, 0.0f, rmsDb));

        repaint();
    }

    void MixMapDetailPanel::setVisible(bool show)
    {
        juce::Component::setVisible(show);
        if (show) {
            startTimerHz(10); // ~10 fps refresh from live data
        } else {
            stopTimer();
            clear();
        }
    }

    void MixMapDetailPanel::clear()
    {
        slotIndex_ = -1;
        trackName_.clear();
        roleName_.clear();
        peakDb_ = -100.0f;
        rmsDb_  = -100.0f;
        correlation_ = 0.0f;
        stereoWidth_ = 0.0f;
        for (int b = 0; b < kNumBands; ++b) bandEnergies_[b] = -100.0f;
        hasReferenceData_ = false;
        for (int b = 0; b < kNumBands; ++b) refBandEnergies_[b] = -100.0f;
        targetLevelDb_ = -18.0f;
        roleConfidence_ = 0.0f;
        rmsSmooth_.reset(-80.0f);
        repaint();
    }

    void MixMapDetailPanel::setReferenceData(const float* refBandEnergies, bool hasReference)
    {
        hasReferenceData_ = hasReference && (refBandEnergies != nullptr);
        if (hasReferenceData_) {
            for (int b = 0; b < kNumBands; ++b)
                refBandEnergies_[b] = (refBandEnergies[b] > -90.0f) ? refBandEnergies[b] : -100.0f;
        }
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  resized — Calcula layout del panel (altura dinámica según datos)
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapDetailPanel::resized()
    {
        const int contentH = kHeaderH
                             + kSectionLabelH + kMeterH + kPadding      // NIVEL section
                             + kSectionLabelH + kPanH + kPadding        // PAN section
                             + kSectionLabelH + kEqH + kPadding         // EQ section
                             + kSectionLabelH + kRouteH + kPadding      // ROUTE section
                             + kSoloH + kPadding * 2;                   // Solo button + padding

        setSize(kPanelWidth, contentH);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Renderiza todo el contenido del panel
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapDetailPanel::paint(juce::Graphics& g)
    {
        if (slotIndex_ < 0) return;

        auto bounds = getLocalBounds();
        rmsSmooth_.advance(60.0);

        const float cr = 6.0f;

        // ─── Shadow ────────────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.fillRoundedRectangle(bounds.toFloat().expanded(1.0f, 2.0f), cr + 1.0f);

        // ─── Background glass gradient ──────────────────────────────────────
        juce::ColourGradient bgGrad(juce::Colour(0xEE1A0A3E),
                                    (float)bounds.getX(), (float)bounds.getY(),
                                    juce::Colour(0xEE081020),
                                    (float)bounds.getX(), (float)bounds.getBottom(),
                                    false);
        bgGrad.addColour(0.5f, juce::Colour(0xEE141838));
        g.setGradientFill(bgGrad);
        g.fillRoundedRectangle(bounds.toFloat(), cr);

        // ─── Border ────────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::accent().withAlpha(0.20f));
        g.drawRoundedRectangle(bounds.toFloat(), cr, 0.8f);

        // ─── Accent bar (left edge) ─────────────────────────────────────────
        g.setColour(MixCoachTheme::accent().withAlpha(0.40f));
        g.fillRoundedRectangle(juce::Rectangle<float>((float)bounds.getX() + 1.0f,
                                                       (float)bounds.getY() + 3.0f,
                                                       2.5f,
                                                       (float)bounds.getHeight() - 6.0f),
                               1.5f);

        auto area = bounds.reduced(kPadding, kPadding);

        // ═══════════════════════════════════════════════════════════════════
        // ════════════════════════════════════
        // 1. HEADER — icono + nombre + rol + pin + × close
        // ═══        // 2. NIVEL section — RMS bar + peak + target markers
        // ═══════════════════════════════════════════════════════════════════
        {
            auto labelArea = area.removeFromTop(kSectionLabelH);
            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.setColour(MixCoachTheme::accentGlow().withAlpha(0.7f));
            g.drawText("LEVEL", labelArea.reduced(2, 0), juce::Justification::centredLeft);

            auto meterArea = area.removeFromTop(kMeterH);
            drawLevelSection(g, meterArea);
        }

        area.removeFromTop(kPadding);

        // ═══════════════════════════════════════════════════════════════════
        // 3. PAN section — slider L-C-R visual
        // ═══════════════════════════════════════════════════════════════════
        {
            auto labelArea = area.removeFromTop(kSectionLabelH);
            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.setColour(MixCoachTheme::accentGlow().withAlpha(0.7f));
            g.drawText("STEREO", labelArea.reduced(2, 0), juce::Justification::centredLeft);

            auto panArea = area.removeFromTop(kPanH);
            drawPanSection(g, panArea);
        }

        area.removeFromTop(kPadding);

        // ═══════════════════════════════════════════════════════════════════
        // 4. EQ section — 6-region spectral curve
        // ═══════════════════════════════════════════════════════════════════
        {
            auto labelArea = area.removeFromTop(kSectionLabelH);
            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.setColour(MixCoachTheme::accentGlow().withAlpha(0.7f));
            g.drawText("SPECTRUM", labelArea.reduced(2, 0), juce::Justification::centredLeft);

            auto eqArea = area.removeFromTop(kEqH);
            drawEqSection(g, eqArea);
        }

        area.removeFromTop(kPadding);

        // ═══════════════════════════════════════════════════════════════════
        // 5. ROUTE section — Track → Bus → Master
        // ═══════════════════════════════════════════════════════════════════
        {
            auto labelArea = area.removeFromTop(kSectionLabelH);
            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.setColour(MixCoachTheme::accentGlow().withAlpha(0.7f));
            g.drawText("ROUTING", labelArea.reduced(2, 0), juce::Justification::centredLeft);

            auto routeArea = area.removeFromTop(kRouteH);
            drawRouteSection(g, routeArea);
        }

        area.removeFromTop(kPadding);

        // ═══════════════════════════════════════════════════════════════════
        // 6. SOLO button
        // ═══════════════════════════════════════════════════════════════════
        {
            auto soloArea = area.removeFromTop(kSoloH);
            float btnW = 160.0f;
            float btnX = soloArea.getCentreX() - btnW * 0.5f;
            soloBounds_.setBounds(btnX, (float)soloArea.getY(), btnW, (float)soloArea.getHeight());

            // Button background
            g.setColour(MixCoachTheme::warning().withAlpha(0.12f));
            g.fillRoundedRectangle(soloBounds_, 5.0f);
            g.setColour(MixCoachTheme::warning().withAlpha(0.35f));
            g.drawRoundedRectangle(soloBounds_, 5.0f, 0.6f);

            // Button text
            g.setFont(juce::Font(juce::FontOptions(8.5f)).boldened());
            g.setColour(MixCoachTheme::warning());
            g.drawText("Solo this track", soloBounds_.toNearestInt(), juce::Justification::centred);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawLevelSection — Barra RMS animada con target marker + valores dB
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapDetailPanel::drawLevelSection(juce::Graphics& g, juce::Rectangle<int> area)
    {
        auto rect = area.toFloat();

        // ─── Background ──────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::bgDarker().withAlpha(0.5f));
        g.fillRoundedRectangle(rect, 3.0f);

        // ─── RMS bar (animada) ──────────────────────────────────────────────
        float rmsSmoothed = rmsSmooth_.getCurrent();
        float rmsNorm     = juce::jlimit(0.0f, 1.0f, (rmsSmoothed + 80.0f) / 80.0f);
        float barW        = rect.getWidth() * rmsNorm;
        if (barW > 1.0f) {
            auto barRect = rect.withWidth(barW);

            juce::Colour barColour;
            if (rmsSmoothed > -6.0f) barColour = MixCoachTheme::error();
            else if (rmsSmoothed > -18.0f) barColour = MixCoachTheme::warning();
            else barColour = MixCoachTheme::success();

            juce::ColourGradient barGrad(barColour.withAlpha(0.8f),
                                         barRect.getX(), barRect.getY(),
                                         barColour.withAlpha(0.3f),
                                         barRect.getX(), barRect.getBottom(),
                                         false);
            g.setGradientFill(barGrad);
            g.fillRoundedRectangle(barRect, 3.0f);
        }

        // ─── Target marker (triángulo invertido) ────────────────────────────
        float targetNorm = juce::jlimit(0.0f, 1.0f, (targetLevelDb_ + 80.0f) / 80.0f);
        float targetX    = rect.getX() + rect.getWidth() * targetNorm;
        juce::Path targetArrow;
        targetArrow.addTriangle(targetX, rect.getY() + 2.0f,
                                 targetX - 4.0f, rect.getY() + 8.0f,
                                 targetX + 4.0f, rect.getY() + 8.0f);
        g.setColour(MixCoachTheme::accentGlow().withAlpha(0.6f));
        g.fillPath(targetArrow);

        // ─── Labels: RMS + Peak values (right side) ─────────────────────────
        auto labelArea = rect.reduced(4, 0);
        g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());

        // RMS label
        g.setColour(MixCoachTheme::textPrimary().withAlpha(0.8f));
        g.drawText("RMS: " + juce::String(rmsDb_, 1) + " dB", labelArea.removeFromLeft(80),
                   juce::Justification::centredLeft);

        // Peak label
        juce::Colour peakColour = (peakDb_ > -0.5f) ? MixCoachTheme::error()
                                  : (peakDb_ > -6.0f) ? MixCoachTheme::warning()
                                  : MixCoachTheme::textMuted();
        g.setColour(peakColour);
        g.drawText("PK: " + juce::String(peakDb_, 1) + " dB", labelArea.removeFromLeft(80),
                   juce::Justification::centredLeft);

        // Target label
        g.setColour(MixCoachTheme::accentGlow().withAlpha(0.5f));
        g.drawText("T: " + juce::String(targetLevelDb_, 1) + " dB", labelArea,
                   juce::Justification::centredLeft);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawPanSection — Indicador visual estéreo L-C-R con barra + badges
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapDetailPanel::drawPanSection(juce::Graphics& g, juce::Rectangle<int> area)
    {
        auto rect = area.toFloat();

        // ─── Background ──────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::bgDarker().withAlpha(0.5f));
        g.fillRoundedRectangle(rect, 3.0f);

        // ─── Correlation meter bar ──────────────────────────────────────────
        float corrW = rect.getWidth() - 80.0f;
        float corrX = rect.getX() + 4.0f;
        float corrY = rect.getY() + 6.0f;
        float corrH = rect.getHeight() - 12.0f;

        // Background track
        g.setColour(MixCoachTheme::bgInput().withAlpha(0.4f));
        g.fillRoundedRectangle(corrX, corrY, corrW, corrH, 2.0f);

        // Center line
        float centerX = corrX + corrW * 0.5f;
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.drawVerticalLine((int)centerX, corrY + 1.0f, corrY + corrH - 1.0f);

        // Fill from center to correlation value
        float fillEnd = centerX + (corrW * 0.5f * correlation_);
        float fillX   = juce::jmin(centerX, fillEnd);
        float fillW   = std::abs(fillEnd - centerX);
        if (fillW > 1.0f) {
            auto fillRect = juce::Rectangle<float>(fillX, corrY, fillW, corrH);
            juce::Colour corrColour = (correlation_ < 0.0f) ? MixCoachTheme::error()
                                       : (correlation_ < 0.3f) ? MixCoachTheme::warning()
                                       : MixCoachTheme::success();
            g.setColour(corrColour.withAlpha(0.5f));
            g.fillRoundedRectangle(fillRect, 2.0f);
        }

        // ─── Labels (right side) ────────────────────────────────────────────
        auto labelArea = rect.reduced(4, 0).withLeft(corrX + corrW + 6.0f);

        g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
        g.setColour(MixCoachTheme::textPrimary().withAlpha(0.8f));

        // Correlation value
        juce::Colour corrTextColour = (correlation_ < 0.0f) ? MixCoachTheme::error()
                                       : (correlation_ < 0.3f) ? MixCoachTheme::warning()
                                       : MixCoachTheme::success();
        g.setColour(corrTextColour);
        g.drawText(juce::String(correlation_, 2), labelArea.removeFromTop(labelArea.getHeight() / 2),
                   juce::Justification::centredLeft);

        // Stereo width
        g.setColour(MixCoachTheme::textMuted());
        g.drawText("W: " + juce::String(stereoWidth_, 2), labelArea,
                   juce::Justification::centredLeft);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // ═══════════════════════════════════════════════════════════════════════════
    //  drawEqSection — Curva EQ realista con 30 bandas de frecuencia
    //  Muestra línea de energía conectando todas las bandas + relleno degradado
    //  + etiquetas de frecuencia en puntos clave + eje de ganancia
    //  + overlay punteado del perfil de referencia (si disponible)
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapDetailPanel::drawEqSection(juce::Graphics& g, juce::Rectangle<int> area)
    {
        auto rect = area.toFloat();
        const float w  = rect.getWidth();
        const float h  = rect.getHeight();
        const float x0 = rect.getX();
        const float y0 = rect.getY();

        // Margen izquierdo para labels de ganancia (dB)
        constexpr float kGainAxisW = 28.0f;
        const float curveX0 = x0 + kGainAxisW;
        const float curveW  = w - kGainAxisW;

        // ─── Background ──────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::bgDarker().withAlpha(0.5f));
        g.fillRoundedRectangle(rect, 3.0f);

        // ─── Grid horizontal (ganancia: -60, -40, -20, -6, 0 dB) ───────────
        constexpr float kGridDb[5] = {-60.0f, -40.0f, -20.0f, -6.0f, 0.0f};
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        for (int gi = 0; gi < 5; ++gi) {
            float norm = juce::jlimit(0.0f, 1.0f, (kGridDb[gi] + 60.0f) / 60.0f);
            float gy   = y0 + h * (1.0f - norm);
            g.drawHorizontalLine((int)gy, curveX0, x0 + w - 2.0f);
        }

        // ─── Grid vertical (frecuencias clave) ─────────────────────────────
        // Key frequency indices: 0(43Hz), 3(215Hz), 9(1kHz), 14(3.5kHz), 21(8kHz), 29(16kHz)
        constexpr int kFreqMarkers[6]    = {0, 3, 9, 14, 21, 29};
        constexpr const char* kFreqLabels[6] = {"43Hz", "215Hz", "1kHz", "3.5k", "8k", "16k"};
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        for (int fi = 0; fi < 6; ++fi) {
            float fx = curveX0 + curveW * ((float)kFreqMarkers[fi] / (float)(kNumBands - 1));
            g.drawVerticalLine((int)fx, y0 + 2.0f, y0 + h - 2.0f);
        }

        // ─── Find max energy for normalization ───────────────────────────────
        float maxEnergy = -100.0f;
        for (int b = 0; b < kNumBands; ++b) {
            if (bandEnergies_[b] > maxEnergy) maxEnergy = bandEnergies_[b];
        }
        if (maxEnergy < -90.0f) maxEnergy = -30.0f; // fallback si no hay datos

        // ─── Helper lambda: energy dB → normalized Y position (0-1) ─────────
        auto energyToNorm = [](float energyDb) -> float {
            return juce::jlimit(0.05f, 1.0f, (energyDb + 60.0f) / 54.0f);
        };

        // ─── Build current EQ path (filled area + line) ─────────────────────
        juce::Path eqPath;
        juce::Path eqFillPath;
        bool first = true;

        for (int b = 0; b < kNumBands; ++b) {
            float energy   = bandEnergies_[b];
            float fraction = (energy > -90.0f && maxEnergy > -90.0f)
                             ? energyToNorm(energy)
                             : 0.05f;

            float px = curveX0 + curveW * ((float)b / (float)(kNumBands - 1));
            float py = y0 + h * (1.0f - fraction);

            if (first) {
                eqPath.startNewSubPath(px, py);
                eqFillPath.startNewSubPath(px, y0 + h);
                eqFillPath.lineTo(px, py);
                first = false;
            } else {
                eqPath.lineTo(px, py);
                eqFillPath.lineTo(px, py);
            }
        }

        // Close fill path at bottom edge
        if (!first) {
            float lastPx = curveX0 + curveW;
            eqFillPath.lineTo(lastPx, y0 + h);
            eqFillPath.closeSubPath();
        }

        // ─── Draw filled area under current curve with gradient ─────────────
        if (!first) {
            juce::ColourGradient fillGrad(MixCoachTheme::accent().withAlpha(0.25f),
                                          curveX0, y0,
                                          MixCoachTheme::accent().withAlpha(0.04f),
                                          curveX0, y0 + h,
                                          false);
            g.setGradientFill(fillGrad);
            g.fillPath(eqFillPath);
        }

        // ─── Draw current EQ curve line ─────────────────────────────────────
        if (!first) {
            g.setColour(MixCoachTheme::accentGlow().withAlpha(0.8f));
            g.strokePath(eqPath, juce::PathStrokeType(1.5f));
        }

        // ─── Draw small dots at each current band point ────────────────────
        for (int b = 0; b < kNumBands; ++b) {
            float energy = bandEnergies_[b];
            if (energy <= -90.0f) continue;
            float fraction = energyToNorm(energy);
            float px       = curveX0 + curveW * ((float)b / (float)(kNumBands - 1));
            float py       = y0 + h * (1.0f - fraction);
            g.setColour(MixCoachTheme::accentGlow().withAlpha(0.5f));
            g.fillEllipse(px - 1.5f, py - 1.5f, 3.0f, 3.0f);
        }

        // ─── ═══════════════════════════════════════════════════════════════
        //     REFERENCE OVERLAY — Dotted line showing target EQ profile
        //     Solo se dibuja si hay datos de referencia válidos
        // ═══════════════════════════════════════════════════════════════════
        if (hasReferenceData_) {
            // Build reference path
            juce::Path refPath;
            bool refFirst = true;

            for (int b = 0; b < kNumBands; ++b) {
                float energy = refBandEnergies_[b];
                if (energy <= -90.0f) continue;
                float fraction = energyToNorm(energy);
                float px = curveX0 + curveW * ((float)b / (float)(kNumBands - 1));
                float py = y0 + h * (1.0f - fraction);

                if (refFirst) {
                    refPath.startNewSubPath(px, py);
                    refFirst = false;
                } else {
                    refPath.lineTo(px, py);
                }
            }

            if (!refFirst) {
                // Create dashed stroke from reference path
                juce::Path dashedPath;
                float dashLengths[] = {4.0f, 3.0f}; // 4px on, 3px off
                juce::PathStrokeType strokeType(1.0f);
                strokeType.createDashedStroke(dashedPath, refPath,
                                              dashLengths, 2,
                                              juce::AffineTransform());

                // Draw dashed reference line with warm gold/amber color
                g.setColour(MixCoachTheme::warning().withAlpha(0.85f));
                g.fillPath(dashedPath);

                // Small hollow circles at reference key markers
                for (int fi = 0; fi < 6; ++fi) {
                    int b = kFreqMarkers[fi];
                    float energy = refBandEnergies_[b];
                    if (energy <= -90.0f) continue;
                    float fraction = energyToNorm(energy);
                    float px = curveX0 + curveW * ((float)b / (float)(kNumBands - 1));
                    float py = y0 + h * (1.0f - fraction);
                    g.setColour(MixCoachTheme::warning().withAlpha(0.60f));
                    g.drawEllipse(px - 2.5f, py - 2.5f, 5.0f, 5.0f, 1.0f);
                }

                // "REF" label in top-right of EQ area
                auto refLabelArea = juce::Rectangle<float>(curveX0 + curveW - 34.0f, y0 + 2.0f, 32.0f, 12.0f);
                g.setColour(MixCoachTheme::warning().withAlpha(0.60f));
                g.setFont(juce::Font(juce::FontOptions(5.5f)).boldened());
                g.drawText("REF", refLabelArea, juce::Justification::centredRight);
            }
        }

        // ─── Frequency labels at bottom ────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(5.5f)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.4f));
        for (int fi = 0; fi < 6; ++fi) {
            float fx = curveX0 + curveW * ((float)kFreqMarkers[fi] / (float)(kNumBands - 1));
            auto labelBounds = juce::Rectangle<float>(fx - 14.0f, y0 + h - 10.0f, 28.0f, 10.0f);
            g.drawText(kFreqLabels[fi], labelBounds, juce::Justification::centred);
        }

        // ─── Gain axis labels (left side) ──────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(5.5f)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
        for (int gi = 0; gi < 5; ++gi) {
            float norm = juce::jlimit(0.0f, 1.0f, (kGridDb[gi] + 60.0f) / 60.0f);
            float gy   = y0 + h * (1.0f - norm);
            auto labelBounds = juce::Rectangle<float>(x0 + 1.0f, gy - 4.0f, kGainAxisW - 3.0f, 8.0f);
            g.drawText(juce::String((int)kGridDb[gi]), labelBounds, juce::Justification::centredRight);
        }

        // ─── Peak highlight dot at max current energy band ─────────────────
        int peakBand = 0;
        for (int b = 1; b < kNumBands; ++b) {
            if (bandEnergies_[b] > bandEnergies_[peakBand]) peakBand = b;
        }
        if (bandEnergies_[peakBand] > -90.0f) {
            float fraction = energyToNorm(bandEnergies_[peakBand]);
            float px       = curveX0 + curveW * ((float)peakBand / (float)(kNumBands - 1));
            float py       = y0 + h * (1.0f - fraction);

            // Glow ring
            g.setColour(MixCoachTheme::accentGlow().withAlpha(0.20f));
            g.fillEllipse(px - 5.0f, py - 5.0f, 10.0f, 10.0f);
            g.setColour(MixCoachTheme::accentGlow().withAlpha(0.40f));
            g.fillEllipse(px - 3.0f, py - 3.0f, 6.0f, 6.0f);
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.fillEllipse(px - 1.5f, py - 1.5f, 3.0f, 3.0f);

            // Label above peak
            auto peakLabel = juce::Rectangle<float>(px - 20.0f, py - 14.0f, 40.0f, 10.0f);
            g.setFont(juce::Font(juce::FontOptions(5.5f)).boldened());
            g.setColour(MixCoachTheme::accentGlow());
            g.drawText(juce::String(bandEnergies_[peakBand], 1) + "dB", peakLabel, juce::Justification::centred);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  timerCallback — Refresh live data from parent at ~10fps
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapDetailPanel::timerCallback()
    {
        if (slotIndex_ >= 0 && onRefreshData) {
            onRefreshData();
            repaint();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawRouteSection — Track → Bus → Master routing chain
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapDetailPanel::drawRouteSection(juce::Graphics& g, juce::Rectangle<int> area)
    {
        auto rect = area.toFloat();

        // ─── Background ──────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::bgDarker().withAlpha(0.5f));
        g.fillRoundedRectangle(rect, 3.0f);

        float cx = rect.getCentreX();
        float cy = rect.getCentreY();

        // ─── \"Track\" node (left) ──────────────────────────────────────────
        auto trackNode = juce::Rectangle<float>(cx - 130.0f, cy - 10.0f, 60.0f, 20.0f);
        g.setColour(MixCoachTheme::bgInput().withAlpha(0.6f));
        g.fillRoundedRectangle(trackNode, 4.0f);
        g.setColour(MixCoachTheme::accent().withAlpha(0.30f));
        g.drawRoundedRectangle(trackNode, 4.0f, 0.5f);
        g.setFont(juce::Font(juce::FontOptions(7.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText("TRACK", trackNode, juce::Justification::centred);

        // ─── Arrow → ───────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
        g.drawHorizontalLine((int)cy, trackNode.getRight() + 4.0f, cx + 30.0f);

        // ─── \"Bus\" node (center) ──────────────────────────────────────────
        auto busNode = juce::Rectangle<float>(cx - 30.0f, cy - 10.0f, 60.0f, 20.0f);
        g.setColour(MixCoachTheme::info().withAlpha(0.10f));
        g.fillRoundedRectangle(busNode, 4.0f);
        g.setColour(MixCoachTheme::info().withAlpha(0.25f));
        g.drawRoundedRectangle(busNode, 4.0f, 0.5f);
        g.setColour(MixCoachTheme::info());
        g.drawText("BUS", busNode, juce::Justification::centred);

        // ─── Arrow → ───────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
        g.drawHorizontalLine((int)cy, busNode.getRight() + 4.0f, cx + 70.0f);

        // ─── \"Master\" node (right) ────────────────────────────────────────
        auto masterNode = juce::Rectangle<float>(cx + 70.0f, cy - 10.0f, 70.0f, 20.0f);
        g.setColour(MixCoachTheme::accentGlow().withAlpha(0.10f));
        g.fillRoundedRectangle(masterNode, 4.0f);
        g.setColour(MixCoachTheme::accentGlow().withAlpha(0.25f));
        g.drawRoundedRectangle(masterNode, 4.0f, 0.5f);
        g.setColour(MixCoachTheme::accentGlow());
        g.drawText("MASTER", masterNode, juce::Justification::centred);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  mouseDown — Handle close and solo button clicks
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapDetailPanel::mouseDown(const juce::MouseEvent& e)
    {
        auto pos = e.getPosition().toFloat();

        // Pin toggle
        if (pinBounds_.contains(pos)) {
            isPinned_ = !isPinned_;
            if (onTogglePinned) onTogglePinned(isPinned_);
            repaint();
            return;
        }

        // Close button
        if (closeBounds_.contains(pos)) {
            if (onClose) onClose();
            return;
        }

        // Solo button
        if (soloBounds_.contains(pos)) {
            if (onRequestSolo) onRequestSolo(slotIndex_);
            return;
        }
    }

} // namespace mixcoach
