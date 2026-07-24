#include "CompressionPanel.h"
#include <cmath>

namespace mixcoach {

    CompressionPanel::CompressionPanel()
    {
        setSize(300, 200); // default size evita 0x0 si se agrega sin bounds
        setOpaque(false);
        setVisible(false);
        addAndMakeVisible(crestGauge_);
        crestGauge_.setVisible(false);
    }

    void CompressionPanel::setTrackData(const std::vector<TrackCompressionRow>& rows)
    {
        rows_ = rows;
        applyBtnBounds_.resize(rows.size());

        // Resize crestAnims_ — mantener animaciones existentes, crear nuevas si es necesario
        size_t oldSize = crestAnims_.size();
        crestAnims_.resize(rows.size());

        // Para filas nuevas, inicializar con el valor actual
        for (size_t i = oldSize; i < rows.size(); ++i) {
            crestAnims_[i] = SmoothValue(rows[i].currentCrestDb, 30.0f, 120.0f);
        }

        // Actualizar targets de animación para TODAS las filas
        for (size_t i = 0; i < rows.size() && i < crestAnims_.size(); ++i) {
            crestAnims_[i].setBallistics(30.0f, 120.0f);
            crestAnims_[i].setTargetValue(rows[i].currentCrestDb);
        }

        // Reset gauge si los datos cambiaron
        hideGauge();

        resized();
        repaint();
    }

    bool CompressionPanel::advanceVisuals(double sampleRateHz)
    {
        bool dirty = false;
        for (auto& anim : crestAnims_) {
            dirty |= anim.advance(sampleRateHz);
        }
        // Avanzar animación del CrestPanel gauge
        if (crestGaugeVisible_) {
            // allowRepaint=false — dejamos que el padre repinte
            dirty |= crestGauge_.advanceVisuals(sampleRateHz, false);
        }
        if (dirty) repaint();
        return dirty;
    }

    void CompressionPanel::clear()
    {
        rows_.clear();
        crestAnims_.clear();
        applyBtnBounds_.clear();
        repaint();
    }

    void CompressionPanel::resized()
    {
        auto bounds = getLocalBounds().reduced(kPadding, kPadding);

        // Si el CrestPanel gauge está visible, restar espacio a la derecha
        juce::Rectangle<int> tableArea = bounds;
        if (crestGaugeVisible_ && crestGaugeRowIndex_ >= 0) {
            int gaugeW = juce::jmin(200, bounds.getWidth() / 3);
            tableArea = bounds.withWidth(bounds.getWidth() - gaugeW - kGap);
            auto gaugeBounds = juce::Rectangle<int>(
                tableArea.getRight() + kGap, bounds.getY(),
                gaugeW, bounds.getHeight() - kPadding);
            crestGauge_.setBounds(gaugeBounds);
        }

        int totalContentH = kHeaderHeight + static_cast<int>(rows_.size()) * (kRowHeight + kGap);
        int applyAllY = juce::jmax(tableArea.getY() + totalContentH + 8, tableArea.getBottom() - 30);
        applyAllBounds_ = juce::Rectangle<int>(tableArea.getCentreX() - 60, applyAllY, 120, 24);

        for (int i = 0; i < (int)rows_.size(); ++i) {
            int rowY = tableArea.getY() + kHeaderHeight + i * (kRowHeight + kGap);
            int btnX = tableArea.getRight() - kApplyW - 4;
            int btnY = rowY + (kRowHeight - 20) / 2;
            applyBtnBounds_[i] = juce::Rectangle<int>(btnX, btnY, kApplyW, 20);
        }
    }

    void CompressionPanel::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().reduced(kPadding, kPadding);
        if (bounds.isEmpty()) return;

        // Panel background
        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), MixCoachTheme::cornerRadius_medium);

        // Header
        {
            auto header = bounds.removeFromTop(kHeaderHeight);
            g.setColour(MixCoachTheme::accentGlow());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSectionHeader)).boldened());
            g.drawText("COMPRESIÓN — Control de dinámica", header.reduced(4, 0),
                       juce::Justification::centredLeft);

            auto labelArea = header.withTop(header.getBottom() - 14).withHeight(14);
            int x = labelArea.getX() + 4;
            g.setColour(MixCoachTheme::textDim());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
            g.drawText("Pista",    juce::Rectangle<int>(x, labelArea.getY(), kNameWidth, 14), juce::Justification::centredLeft); x += kNameWidth;
            g.drawText("Crest",    juce::Rectangle<int>(x, labelArea.getY(), kCrestBarW, 14), juce::Justification::centredLeft); x += kCrestBarW;
            g.drawText("Obj",      juce::Rectangle<int>(x, labelArea.getY(), kTargetW, 14), juce::Justification::centredLeft); x += kTargetW;
            g.drawText("Ratio",    juce::Rectangle<int>(x, labelArea.getY(), kRatioW, 14), juce::Justification::centredLeft); x += kRatioW;
            g.drawText("Atk",      juce::Rectangle<int>(x, labelArea.getY(), kAttackW, 14), juce::Justification::centredLeft); x += kAttackW;
            g.drawText("Rel",      juce::Rectangle<int>(x, labelArea.getY(), kReleaseW, 14), juce::Justification::centredLeft);
        }

        if (rows_.empty()) {
            g.setColour(MixCoachTheme::textMuted());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)));
            g.drawText("Esperando datos de dinámica... Dale Play a tu sesión.",
                       bounds.reduced(8), juce::Justification::centred);
            return;
        }

        int y = bounds.getY() + kHeaderHeight;
        for (int i = 0; i < (int)rows_.size(); ++i) {
            const auto& row = rows_[i];
            auto rowBounds = juce::Rectangle<int>(bounds.getX(), y, bounds.getWidth(), kRowHeight);

            // Row bg
            if (i == hoveredRow_) {
                g.setColour(MixCoachTheme::accentBg().withAlpha(0.08f));
                g.fillRoundedRectangle(rowBounds.toFloat(), 2.0f);
            } else if (i % 2 == 0) {
                g.setColour(juce::Colours::white.withAlpha(0.02f));
                g.fillRoundedRectangle(rowBounds.toFloat(), 2.0f);
            }

            int x = rowBounds.getX() + 4;

            // Track name
            g.setColour(MixCoachTheme::textBright());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
            g.drawText(row.trackName.substring(0, 10),
                       juce::Rectangle<int>(x, rowBounds.getY(), kNameWidth, kRowHeight),
                       juce::Justification::centredLeft); x += kNameWidth;

            // Animated crest bar with reduction indicator
            auto barBounds = juce::Rectangle<int>(x, rowBounds.getY() + 6, kCrestBarW, kRowHeight - 12);
            drawAnimatedCrestBar(g, barBounds, row, i);
            x += kCrestBarW;

            // Target value
            g.setColour(MixCoachTheme::textMuted());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
            g.drawText(juce::String((int)row.targetCrestDb) + "dB",
                       juce::Rectangle<int>(x, rowBounds.getY(), kTargetW, kRowHeight),
                       juce::Justification::centredLeft); x += kTargetW;

            // Ratio
            g.setColour(MixCoachTheme::textSecondary());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
            g.drawText(juce::String(row.suggestedRatio, 1) + ":1",
                       juce::Rectangle<int>(x, rowBounds.getY(), kRatioW, kRowHeight),
                       juce::Justification::centredLeft); x += kRatioW;

            // Attack
            g.setColour(MixCoachTheme::textDim());
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
            g.drawText(juce::String((int)row.suggestedAttackMs) + "ms",
                       juce::Rectangle<int>(x, rowBounds.getY(), kAttackW, kRowHeight),
                       juce::Justification::centredLeft); x += kAttackW;

            // Release
            g.drawText(juce::String((int)row.suggestedReleaseMs) + "ms",
                       juce::Rectangle<int>(x, rowBounds.getY(), kReleaseW, kRowHeight),
                       juce::Justification::centredLeft); x += kReleaseW;

            // Apply button
            if (i < (int)applyBtnBounds_.size() && !row.isApplied) {
                auto btn = applyBtnBounds_[i];
                bool hovered = (hoveredRow_ == i && btn.contains(getMouseXYRelative()));
                g.setColour(hovered ? MixCoachTheme::accent() : MixCoachTheme::accentDim());
                g.fillRoundedRectangle(btn.toFloat(), 3.0f);
                g.setColour(MixCoachTheme::textBright());
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
                g.drawText("Aplicar", btn, juce::Justification::centred);
            }

            y += kRowHeight + kGap;
        }

        // Apply All button
        if (rows_.size() > 1) {
            bool allApplied = true;
            for (const auto& r : rows_) if (!r.isApplied) { allApplied = false; break; }
            if (!allApplied) {
                bool hovered = hoveredApplyAll_ >= 0;
                g.setColour(hovered ? MixCoachTheme::accent() : MixCoachTheme::accentDim());
                g.fillRoundedRectangle(applyAllBounds_.toFloat(), 4.0f);
                g.setColour(MixCoachTheme::textBright());
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
                g.drawText("Aplicar todas", applyAllBounds_, juce::Justification::centred);
            }
        }
    }

    void CompressionPanel::mouseMove(const juce::MouseEvent& e)
    {
        int prevHover = hoveredRow_;
        int prevApplyAll = hoveredApplyAll_;
        hoveredRow_ = -1;
        hoveredApplyAll_ = -1;

        if (crestGauge_.isVisible() && crestGauge_.getBounds().contains(e.getPosition())) {
            // Mouse está sobre el gauge — mantener estado actual
            hoveredRow_ = crestGaugeRowIndex_;
            return;
        }

        if (applyAllBounds_.contains(e.getPosition())) {
            hoveredApplyAll_ = 0;
        } else {
            for (int i = 0; i < (int)rows_.size(); ++i) {
                int y = kHeaderHeight + kPadding + i * (kRowHeight + kGap);
                auto rowBounds = juce::Rectangle<int>(kPadding, y, getWidth() - kPadding * 2, kRowHeight);
                if (rowBounds.contains(e.getPosition())) { hoveredRow_ = i; break; }
            }
        }

        // Mostrar CrestPanel gauge al hacer hover sobre una fila
        if (hoveredRow_ >= 0 && hoveredRow_ < (int)rows_.size()) {
            feedGauge(hoveredRow_);
        } else {
            hideGauge();
        }

        if (prevHover != hoveredRow_ || prevApplyAll != hoveredApplyAll_) repaint();
    }

    void CompressionPanel::mouseExit(const juce::MouseEvent& e)
    {
        juce::ignoreUnused(e);
        hideGauge();
        hoveredRow_ = -1;
        hoveredApplyAll_ = -1;
        repaint();
    }

    void CompressionPanel::mouseDown(const juce::MouseEvent& e)
    {
        if (applyAllBounds_.contains(e.getPosition())) {
            if (onApplyAll) onApplyAll();
            return;
        }
        for (int i = 0; i < (int)rows_.size() && i < (int)applyBtnBounds_.size(); ++i) {
            if (applyBtnBounds_[i].contains(e.getPosition()) && !rows_[i].isApplied) {
                if (onApplyCompression)
                    onApplyCompression(rows_[i].slotIndex, rows_[i].suggestedRatio,
                                       rows_[i].suggestedAttackMs, rows_[i].suggestedReleaseMs,
                                       rows_[i].suggestedThresholdDb);
                rows_[i].isApplied = true;
                repaint();
                return;
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  feedGauge — Alimenta el CrestPanel gauge con datos de la fila hovered
    // ═══════════════════════════════════════════════════════════════════════════
    void CompressionPanel::feedGauge(int rowIndex)
    {
        if (rowIndex < 0 || rowIndex >= (int)rows_.size()) {
            hideGauge();
            return;
        }

        const auto& row = rows_[rowIndex];
        // Usar peak de referencia fijo y derivar RMS para que crest = peak - RMS = currentCrestDb
        float peakRef = -6.0f;
        float rms = peakRef - row.currentCrestDb;

        crestGauge_.setValues(peakRef, rms);
        // Marcar target en el gauge
        crestGauge_.setTargetCrest(row.targetCrestDb,
                                   "Obj: " + juce::String((int)row.targetCrestDb) + "dB");
        crestGaugeVisible_ = true;
        crestGaugeRowIndex_ = rowIndex;

        if (!crestGauge_.isVisible()) {
            crestGauge_.setVisible(true);
            resized();
        }
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  hideGauge — Oculta el CrestPanel gauge
    // ═══════════════════════════════════════════════════════════════════════════
    void CompressionPanel::hideGauge()
    {
        if (!crestGaugeVisible_ && !crestGauge_.isVisible()) return;

        crestGauge_.clearTargetCrest();
        crestGaugeVisible_ = false;
        crestGaugeRowIndex_ = -1;
        crestGauge_.setVisible(false);
        resized();
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawAnimatedCrestBar — Crest meter animado con SmoothValue
    //
    //  Dibuja una barra que se anima suavemente del valor actual al target.
    //  Incluye:
    //    - Barra de fondo gris (crest máximo)
    //    - Barra animada del valor actual con color dinámico (verde→amarillo→rojo)
    //    - Línea target con glow
    //    - Zona de reducción (diferencia entre crest actual y target)
    //    - Flecha de reducción indicando la compresión necesaria
    // ═══════════════════════════════════════════════════════════════════════════
    void CompressionPanel::drawAnimatedCrestBar(juce::Graphics& g, juce::Rectangle<int> barBounds,
                                                  const TrackCompressionRow& row, int animIndex)
    {
        auto b = barBounds.toFloat();
        if (b.isEmpty()) return;

        // ─── Obtener el valor animado (suavizado) ────────────────────────────
        float animatedCrest = row.currentCrestDb; // fallback
        if (animIndex >= 0 && animIndex < (int)crestAnims_.size()) {
            animatedCrest = crestAnims_[animIndex].getCurrent();
        }

        constexpr float kMaxCrest = 24.0f;
        float animNorm  = juce::jmap(juce::jlimit(0.0f, kMaxCrest, animatedCrest), 0.0f, kMaxCrest, 0.0f, 1.0f);
        float targetNorm = juce::jmap(juce::jlimit(0.0f, kMaxCrest, row.targetCrestDb), 0.0f, kMaxCrest, 0.0f, 1.0f);

        // ─── 1. Barra de fondo (track completo 0-24dB) ───────────────────────
        {
            juce::Colour trackBg = MixCoachTheme::bgSurface().withAlpha(0.20f);
            g.setColour(trackBg);
            g.fillRoundedRectangle(b, 2.0f);

            // Subtle inner shadow
            g.setColour(MixCoachTheme::bgDarker().withAlpha(0.10f));
            g.fillRoundedRectangle(b.reduced(1.0f), 1.5f);
        }

        // ─── 2. Zona de reducción (si crest > target) ───────────────────────
        //    Muestra visualmente cuánto hay que comprimir con un glow rojizo
        if (animatedCrest > row.targetCrestDb) {
            float fillW = b.getWidth() * animNorm;
            float targetX = b.getX() + b.getWidth() * targetNorm;
            float reductionW = fillW - (targetX - b.getX());

            if (reductionW > 2.0f) {
                auto reductionBounds = juce::Rectangle<float>(targetX, b.getY() + 2, reductionW, b.getHeight() - 4);

                // Glow externo
                g.setColour(MixCoachTheme::meterOrange().withAlpha(0.12f));
                g.fillRoundedRectangle(reductionBounds.expanded(2.0f), 3.0f);

                // Relleno semitransparente
                g.setColour(MixCoachTheme::error().withAlpha(0.18f));
                g.fillRoundedRectangle(reductionBounds, 1.5f);

                // Borde izquierdo marcado
                g.setColour(MixCoachTheme::meterOrange().withAlpha(0.40f));
                g.drawVerticalLine((int)targetX, b.getY() + 2, b.getBottom() - 2);
            }
        }

        // ─── 3. Barra animada del valor actual ──────────────────────────────
        if (animNorm > 0.01f) {
            auto fillBounds = b.withWidth(b.getWidth() * animNorm);

            // Color dinámico según nivel de crest
            juce::Colour crestCol;
            if (animNorm < 0.30f)        crestCol = MixCoachTheme::success();
            else if (animNorm < 0.55f)   crestCol = MixCoachTheme::meterYellow();
            else if (animNorm < 0.75f)   crestCol = MixCoachTheme::meterOrange();
            else                         crestCol = MixCoachTheme::error();

            // Glow exterior
            g.setColour(crestCol.withAlpha(0.15f));
            g.fillRoundedRectangle(fillBounds.expanded(1.5f), 3.5f);

            // Relleno principal con gradiente
            juce::ColourGradient fillGrad(crestCol.withAlpha(0.85f), fillBounds.getX(), fillBounds.getY(),
                                          crestCol.withAlpha(0.70f), fillBounds.getRight(), fillBounds.getY(), false);
            fillGrad.addColour(0.5f, crestCol.withAlpha(0.80f));
            g.setGradientFill(fillGrad);
            g.fillRoundedRectangle(fillBounds, 2.0f);

            // Highlight superior (efecto 3D)
            auto highlight = fillBounds.withHeight(fillBounds.getHeight() * 0.4f);
            g.setColour(juce::Colours::white.withAlpha(0.08f));
            g.fillRoundedRectangle(highlight, 2.0f);
        }

        // ─── 4. Línea target con glow ───────────────────────────────────────
        {
            float markerX = b.getX() + b.getWidth() * targetNorm;

            // Glow
            g.setColour(MixCoachTheme::accentGlow().withAlpha(0.08f));
            g.drawVerticalLine((int)markerX + 1, b.getY() - 4, b.getBottom() + 4);
            g.drawVerticalLine((int)markerX - 1, b.getY() - 4, b.getBottom() + 4);

            // Línea principal
            g.setColour(MixCoachTheme::accentGlow().withAlpha(0.65f));
            g.drawVerticalLine((int)markerX, b.getY() - 3, b.getBottom() + 3);

            // Diamante en el tope
            g.setColour(MixCoachTheme::accent());
            auto diamond = juce::Rectangle<float>(markerX - 3, b.getY() - 6, 6, 6);
            g.fillRect(diamond);
        }

        // ─── 5. Lectura digital animada sobre la barra ──────────────────────
        {
            juce::String crestStr = juce::String(animatedCrest, 1) + "dB";
            g.setFont(juce::Font(juce::FontOptions(8.5f)).boldened());
            g.setColour(MixCoachTheme::textBright().withAlpha(0.85f));
            g.drawText(crestStr, b.reduced(3, 0), juce::Justification::centredRight);
        }

        // ─── 6. Flecha de reducción (si crest > target) ─────────────────────
        if (animatedCrest > row.targetCrestDb + 0.5f) {
            float diff = animatedCrest - row.targetCrestDb;
            juce::String redStr = "\u2193 " + juce::String((int)diff) + "dB";
            g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
            g.setColour(MixCoachTheme::meterOrange().withAlpha(0.55f));
            g.drawText(redStr, b.reduced(2, 0), juce::Justification::centredLeft);
        }
    }

} // namespace mixcoach
