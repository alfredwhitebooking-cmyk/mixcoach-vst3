#include "PlaylistComponent.h"

namespace mixcoach {

    namespace {

        /** Convierte BusType a índice de bus groups. None/UNASSIGNED → kNumBuses. */
        int busToIndex(BusType bus) noexcept
        {
            int idx = static_cast<int>(bus);
            return (idx >= 0 && idx < kNumBuses) ? idx : kNumBuses;
        }

    } // namespace

    PlaylistComponent::PlaylistComponent()
    {
        headerLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x93\x8B Playlist"), juce::dontSendNotification);
        headerLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
        headerLabel_.setJustificationType(juce::Justification::centredLeft);
        headerLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
        addAndMakeVisible(headerLabel_);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateList — Agrupa los slots activos por bus y actualiza métricas
    // ═══════════════════════════════════════════════════════════════════════════
    void PlaylistComponent::updateList(SlotRegistry& registry)
    {
        // ─── Resetear grupos ──────────────────────────────────────────────────
        for (auto& group : busGroups_) group.count = 0;

        int count     = 0;
        float maxPeak = -100.0f;

        registry.forEachActive([&](const SlotInfo& info) {
            if (count >= kMaxEntries) return;
            int idx = info.slotIndex;

            auto& entry     = entries_[count];
            entry.slotIndex = idx;
            entry.info      = info;
            entry.selected  = (idx == selectedSlot_);

            // V3: sin telemetría per-slot. Peak/RMS desde identidad.
            entry.peakLeft  = -100.0f;
            entry.peakRight = -100.0f;
            entry.hasSignal = false;

            // Track peak para LED strip
            float entryPeak = -100.0f;
            if (entryPeak > maxPeak) maxPeak = entryPeak;

            // ─── Agrupar por bus ─────────────────────────────────────────────
            int busIdx  = busToIndex(info.bus);
            auto& group = busGroups_[busIdx];
            if (group.count < SlotRegistry::kMaxSlots) group.slotIndices[group.count++] = count;

            count++;
        });

        activeCount_ = count;

        // ─── Alimentar SmoothValue del LED strip ──────────────────────────
        float targetLevel = (maxPeak > -80.0f && activeCount_ > 0)
                                ? juce::jmap(juce::jlimit(-60.0f, 0.0f, maxPeak), -60.0f, 0.0f, 0.0f, 1.0f)
                                : 0.0f;
        ledSmooth_.setTarget(targetLevel, 30.0);
        ledLevel_ = ledSmooth_.getCurrent();

        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getPreferredHeight — Altura total calculada para scroll si es necesario
    // ═══════════════════════════════════════════════════════════════════════════
    int PlaylistComponent::getPreferredHeight() const
    {
        if (activeCount_ == 0) return getHeight();

        int h = kPlLEDStripH + kPlLabelH + 16 + 8; // +16 for sub-header

        for (int busIdx = 0; busIdx <= kNumBuses; ++busIdx) {
            auto& group = busGroups_[busIdx];
            if (group.count == 0) continue;
            h += kPlBusHeaderH + 2;
            h += (kPlCardHeight + kPlCardGap) * group.count;
            h += 4;
        }

        return h + 4;
    }

    void PlaylistComponent::setSelectedSlot(int slotIndex)
    {
        if (selectedSlot_ == slotIndex) return;

        // ─── Validación: -1 (deseleccionar) o rango válido ────────────────
        if (slotIndex < -1 || slotIndex >= SlotRegistry::kMaxSlots) return;

        selectedSlot_ = slotIndex;

        for (int i = 0; i < activeCount_; ++i) {
            entries_[i].selected = (entries_[i].slotIndex == slotIndex);
        }

        // ─── Auto-scroll: hacer visible el track seleccionado ──────────────
        if (playlistViewport_ != nullptr && slotIndex >= 0) {
            int rowY = getSlotY(slotIndex);
            if (rowY >= 0) {
                auto viewPos = playlistViewport_->getViewPosition();
                int viewH    = playlistViewport_->getMaximumVisibleHeight();
                // Si está debajo del área visible → scroll down
                if (rowY + kPlCardHeight > viewPos.getY() + viewH)
                    playlistViewport_->setViewPosition(viewPos.getX(), rowY + kPlCardHeight - viewH);
                // Si está arriba del área visible → scroll up
                else if (rowY < viewPos.getY())
                    playlistViewport_->setViewPosition(viewPos.getX(), rowY);
            }
        }

        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getSlotY — Calcula la coordenada Y absoluta de un slot en la lista
    //  Sigue la misma lógica de layout que paint() y mouseDown()
    // ═══════════════════════════════════════════════════════════════════════════
    int PlaylistComponent::getSlotY(int slotIndex) const noexcept
    {
        if (slotIndex < 0 || activeCount_ == 0) return -1;

        int y = 2 + kPlLEDStripH + kPlLabelH + 16; // +16 for sub-header

        for (int busIdx = 0; busIdx <= kNumBuses; ++busIdx) {
            auto& group = busGroups_[busIdx];
            if (group.count == 0) continue;

            y += kPlBusHeaderH + 2;

            for (int r = 0; r < group.count; ++r) {
                int entryIdx = group.slotIndices[r];
                if (entries_[entryIdx].slotIndex == slotIndex) return y;
                y += kPlCardHeight + kPlCardGap;
            }

            y += 4;
        }

        return -1;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  mouseDown — Hit detection sobre las track cards agrupadas por bus
    // ═══════════════════════════════════════════════════════════════════════════
    void PlaylistComponent::mouseDown(const juce::MouseEvent& e)
    {
        auto pos = e.getPosition();

        // ─── + GRUPO click ───────────────────────────────────────────────────
        if (subHeaderGrupoBounds_.contains(pos)) {
            if (onAddGroupRequested) onAddGroupRequested();
            return;
        }

        // ─── Track card / bus header hit detection ───────────────────────────
        auto area = getLocalBounds();
        int y     = 2 + kPlLEDStripH + kPlLabelH + 16; // +16 for sub-header

        for (int busIdx = 0; busIdx <= kNumBuses; ++busIdx) {
            auto& group = busGroups_[busIdx];
            if (group.count == 0) continue;

            y += kPlBusHeaderH + 2; // skip header

            for (int r = 0; r < group.count; ++r) {
                int entryIdx     = group.slotIndices[r];
                auto entryBounds = juce::Rectangle<int>(area.getX(), y, area.getWidth(), kPlCardHeight);
                if (entryBounds.contains(pos)) {
                    if (onSlotSelected) onSlotSelected(entries_[entryIdx].slotIndex);
                    return;
                }
                y += kPlCardHeight + kPlCardGap;
            }

            y += 4; // gap between bus groups
        }
    }

    void PlaylistComponent::resized()
    {
        auto area    = getLocalBounds().reduced(4, 2);
        auto ledArea = area.removeFromTop(kPlLEDStripH);
        juce::ignoreUnused(ledArea);
        headerLabel_.setBounds(area.removeFromTop(kPlLabelH));
        // sub-header area (16px) handled in paint()
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  PAINT — Layout con bus headers + track cards
    // ═══════════════════════════════════════════════════════════════════════════
    void PlaylistComponent::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

        auto area = getLocalBounds().reduced(4, 2);

        // ─── LED strip ────────────────────────────────────────────────────────
        auto ledStripArea = area.removeFromTop(kPlLEDStripH);
        animateLEDStrip(g, ledStripArea);

        // ─── Header label (pintado por headerLabel_ label) ──────────────────
        auto headerArea = area.removeFromTop(kPlLabelH);
        juce::ignoreUnused(headerArea);

        // ═══ Sub-header: MESSENGERS & GRUPOS + + GRUPO ─────────────────────
        auto subHeaderArea = area.removeFromTop(16).reduced(0, 1);
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
        g.drawText("TRACKLIST", subHeaderArea, juce::Justification::centredLeft);

        // + GRUPO clickable
        auto grupoArea        = subHeaderArea.removeFromRight(52);
        subHeaderGrupoBounds_ = grupoArea;
        g.setColour(MixCoachTheme::accentGlow().withAlpha(0.6f));
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened());
        g.drawText("+ GRUPO", grupoArea, juce::Justification::centred);

        // Grid icon
        auto gridIconArea = grupoArea.removeFromRight(4);
        juce::ignoreUnused(gridIconArea);

        if (activeCount_ == 0) {
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
            g.drawText("\xF0\x9F\x94\x8C No hay Messengers conectados", area, juce::Justification::centred);
            return;
        }

        int drawY  = area.getY();
        int availW = area.getWidth();

        for (int busIdx = 0; busIdx <= kNumBuses; ++busIdx) {
            auto& group = busGroups_[busIdx];
            if (group.count == 0) continue;

            // ─── Bus header ─────────────────────────────────────────────────
            auto headerRect = juce::Rectangle<int>(area.getX(), drawY, availW, kPlBusHeaderH);
            drawBusHeader(g, headerRect, busIdx, group.count);
            drawY += kPlBusHeaderH + 2;

            // ─── Track cards ────────────────────────────────────────────────
            for (int r = 0; r < group.count; ++r) {
                int entryIdx  = group.slotIndices[r];
                auto cardRect = juce::Rectangle<int>(area.getX(), drawY, availW, kPlCardHeight);
                drawTrackCard(g, cardRect, entries_[entryIdx]);
                drawY += kPlCardHeight + kPlCardGap;
            }

            drawY += 4; // gap between bus groups
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawBusHeader — Cabecera de bus compacta
    // ═══════════════════════════════════════════════════════════════════════════
    void PlaylistComponent::drawBusHeader(juce::Graphics& g, const juce::Rectangle<int>& bounds, int busIdx, int count)
    {
        auto headerArea = bounds.reduced(0, 1);

        // ─── Accent bar (2px) ────────────────────────────────────────────────
        juce::Colour busColour;
        juce::String busName;

        if (busIdx == kNumBuses) {
            busColour = MixCoachTheme::textMuted();
            busName   = "UNASSIGNED";
        }
        else {
            busColour = getBusColour(busIdx);
            busName   = juce::String(busNames[busIdx]).toUpperCase();
        }

        auto accentBar = headerArea.removeFromLeft(2);
        g.setColour(busColour.withAlpha(0.5f));
        g.fillRect(accentBar.reduced(0, 1).toFloat());
        headerArea.removeFromLeft(3);

        // ─── Bus name ────────────────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
        g.setColour(busColour);
        g.drawText(busName, headerArea.removeFromLeft(60), juce::Justification::centredLeft);

        // ─── Count badge ────────────────────────────────────────────────────
        auto badgeArea = headerArea.removeFromLeft(20).reduced(0, 2);
        g.setColour(busColour.withAlpha(0.12f));
        g.fillRoundedRectangle(badgeArea.toFloat(), 4.0f);
        g.setColour(busColour);
        g.drawText(juce::String(count), badgeArea, juce::Justification::centred);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawTrackCard — Card compacta con indicadores de nivel y bus coloreado
    //  ┌──────────────────────────────────────────┐
    //  │ ▌ Kick  █▓░░░░░  PK -6.2  [DRUMS]  ▐    │
    //  └──────────────────────────────────────────┘
    //  • Barra izquierda (3px): color del BUS (agrupación visual)
    //  • Bus badge derecho: abreviatura sobre fondo sólido + borde glow
    //  • Mini VU + PK stats
    // ═══════════════════════════════════════════════════════════════════════════
    void PlaylistComponent::drawTrackCard(juce::Graphics& g, juce::Rectangle<int> bounds, const TrackEntry& entry)
    {
        auto b       = bounds.toFloat();
        bool isStale = entry.info.stale;

        int busIdx          = busToIndex(entry.info.bus);
        juce::Colour busCol = (busIdx < kNumBuses) ? getBusColour(busIdx) : MixCoachTheme::textMuted();

        // ─── Background ─────────────────────────────────────────────────────
        if (isStale) {
            // Stale: fondo gris con alpha bajo
            g.setColour(MixCoachTheme::border().withAlpha(0.08f));
            g.fillRoundedRectangle(b, 4.0f);
        }
        else if (entry.selected && selectedSlot_ == entry.slotIndex) {
            g.setColour(MixCoachTheme::accent().withAlpha(0.10f));
            g.fillRoundedRectangle(b, 4.0f);
            g.setColour(MixCoachTheme::accent().withAlpha(0.35f));
            g.drawRoundedRectangle(b, 4.0f, 1.0f);
        }
        else {
            g.setColour(MixCoachTheme::bgPanel().withAlpha(0.4f));
            g.fillRoundedRectangle(b, 4.0f);
        }

        // ═══ Bus color bar ──────────────────────────────────────────────────
        auto busBar = b.removeFromLeft(4);
        g.setColour(busCol.withAlpha(isStale ? 0.2f : 0.8f));
        g.fillRoundedRectangle(busBar.reduced(0, 5), 2.0f);
        b.removeFromLeft(2);

        // ─── Type icon ──────────────────────────────────────────────────────
        auto typeArea = b.removeFromLeft(12).reduced(0, 7);
        juce::String typeIcon;
        switch (entry.info.bus) {
            case BusType::Drums:
                typeIcon = juce::CharPointer_UTF8("\xF0\x9F\xA5\x81");
                break;
            case BusType::Bass:
                typeIcon = juce::CharPointer_UTF8("\xF0\x9F\x8E\xB8");
                break;
            case BusType::Guitars:
                typeIcon = juce::CharPointer_UTF8("\xF0\x9F\x8E\xB8");
                break;
            case BusType::Keys:
                typeIcon = juce::CharPointer_UTF8("\xF0\x9F\x8E\xB9");
                break;
            case BusType::Vocals:
                typeIcon = juce::CharPointer_UTF8("\xF0\x9F\x8E\xA4");
                break;
            default:
                typeIcon.clear();
                break;
        }
        if (typeIcon.isNotEmpty()) {
            g.setFont(juce::Font(juce::FontOptions(9.0f)));
            g.setColour(isStale ? MixCoachTheme::textMuted().withAlpha(0.2f)
                                : MixCoachTheme::textMuted().withAlpha(0.5f));
            g.drawText(typeIcon, typeArea.toNearestInt(), juce::Justification::centred);
        }
        b.removeFromLeft(1);

        // ─── Signal indicator ───────────────────────────────────────────────
        auto sigArea = b.removeFromLeft(4).reduced(0, 11);
        if (isStale) {
            g.setColour(MixCoachTheme::error().withAlpha(0.35f)); // Rojo fijo para stale
        }
        else if (entry.hasSignal) {
            float pulse = 0.6f + 0.4f * std::sin(juce::Time::getMillisecondCounter() * 0.005f);
            g.setColour(MixCoachTheme::success().withAlpha(pulse));
        }
        else {
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.2f));
        }
        g.fillEllipse(sigArea.toFloat());
        b.removeFromLeft(2);

        // ─── Nombre de pista ────────────────────────────────────────────────
        auto name = juce::String(entry.info.trackName);
        if (name.isEmpty()) name = "Track " + juce::String(entry.slotIndex);

        auto nameArea = b.removeFromLeft(64);
        g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
        if (isStale) {
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.4f)); // Atenuado
        }
        else {
            g.setColour(entry.selected ? MixCoachTheme::textBright() : MixCoachTheme::textPrimary());
        }
        g.drawText(name, nameArea.toNearestInt(), juce::Justification::centredLeft);
        b.removeFromLeft(2);

        // ═══ ··· button + SIN SEÑAL badge + stats ═══════════════════════════
        // ··· button (rightmost)
        auto dotsArea = b.removeFromRight(14).reduced(0, 6);
        g.setColour(MixCoachTheme::textMuted().withAlpha(isStale ? 0.12f : 0.35f));
        g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
        g.drawText("\xC2\xB7\xC2\xB7\xC2\xB7", dotsArea.toNearestInt(), juce::Justification::centred);
        b.removeFromRight(1);

        // ═══ SIN SEÑAL badge (para stale) ══════════════════════════════════
        if (isStale) {
            auto staleBadge = b.removeFromRight(54).reduced(0, 4);
            g.setColour(MixCoachTheme::error().withAlpha(0.20f));
            g.fillRoundedRectangle(staleBadge.toFloat(), 4.0f);
            g.setColour(MixCoachTheme::error().withAlpha(0.6f));
            g.drawRoundedRectangle(staleBadge.toFloat(), 4.0f, 1.0f);
            g.setColour(MixCoachTheme::error().withAlpha(0.8f));
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)).boldened());
            g.drawFittedText(juce::CharPointer_UTF8("SIN SE\xC3\x91"
                                                    "AL"),
                             staleBadge.toNearestInt(),
                             juce::Justification::centred,
                             1);
            b.removeFromRight(2);
        }
        else {
            // ═══ Bus badge (solo si NO es stale) ═════════════════════════════
            juce::String busAbbr;
            switch (entry.info.bus) {
                case BusType::Drums:
                    busAbbr = "DRM";
                    break;
                case BusType::Bass:
                    busAbbr = "BAS";
                    break;
                case BusType::Guitars:
                    busAbbr = "GTR";
                    break;
                case BusType::Keys:
                    busAbbr = "KEY";
                    break;
                case BusType::Vocals:
                    busAbbr = "VOX";
                    break;
                case BusType::FX:
                    busAbbr = "FX";
                    break;
                default:
                    busAbbr = (busIdx >= 0) ? juce::String(busNames[busIdx]).substring(0, 3) : "";
                    break;
            }

            if (busIdx >= 0 && busAbbr.isNotEmpty()) {
                auto badgeArea = b.removeFromRight(42).reduced(0, 4);
                auto glowArea  = badgeArea.expanded(2.0f, 2.0f).toFloat();
                g.setColour(busCol.withAlpha(0.12f));
                g.fillRoundedRectangle(glowArea, 5.0f);
                g.setColour(busCol.withAlpha(0.45f));
                g.fillRoundedRectangle(badgeArea.toFloat(), 4.0f);
                auto innerGlow = badgeArea.toFloat().withHeight(badgeArea.getHeight() * 0.4f);
                g.setColour(juce::Colours::white.withAlpha(0.10f));
                g.fillRoundedRectangle(innerGlow, 4.0f);
                g.setColour(busCol.withAlpha(0.75f));
                g.drawRoundedRectangle(badgeArea.toFloat(), 4.0f, 1.0f);
                g.setColour(juce::Colours::white);
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened());
                g.drawFittedText(busAbbr, badgeArea.toNearestInt(), juce::Justification::centred, 1);
                b.removeFromRight(2);
            }

            // ─── Stats: PK + mini VU ─────────────────────────────────────────
            auto statsArea = b.removeFromRight(48).reduced(0, 4);
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.6f));
            g.drawText(
                "PK " + juce::String(entry.peakLeft, 1), statsArea.toNearestInt(), juce::Justification::centredLeft);
            b.removeFromRight(2);

            // ─── Mini VU horizontal ───────────────────────────────────────────
            if (entry.hasSignal && b.getWidth() > 20) {
                auto vuArea = b.reduced(0, 8).toFloat();
                g.setColour(MixCoachTheme::bgDarker());
                g.fillRoundedRectangle(vuArea, 1.5f);

                float norm = juce::jlimit(0.0f, 1.0f, (entry.peakLeft + 60.0f) / 66.0f);
                if (norm > 0.01f) {
                    auto fillW    = juce::jmax(2.0f, vuArea.getWidth() * norm);
                    auto fillRect = vuArea.withWidth(fillW);

                    juce::Colour fillCol = entry.peakLeft > -6.0f    ? MixCoachTheme::error()
                                           : entry.peakLeft > -12.0f ? MixCoachTheme::warning()
                                                                     : MixCoachTheme::success();
                    g.setColour(fillCol);
                    g.fillRoundedRectangle(fillRect, 1.5f);
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  LED Strip Animado — Estilo consola SSL / UA Apollo
    // ═══════════════════════════════════════════════════════════════════════════
    void PlaylistComponent::animateLEDStrip(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        if (bounds.getWidth() < 10) return;

        auto rf      = bounds.toFloat();
        uint32_t now = juce::Time::getMillisecondCounter();
        float level  = juce::jlimit(0.0f, 1.0f, ledLevel_);

        // ─── Fondo oscuro del strip ───────────────────────────────────────
        g.setColour(MixCoachTheme::bgDarker().withAlpha(0.9f));
        g.fillRoundedRectangle(rf, 2.0f);
        g.setColour(MixCoachTheme::divider().withAlpha(0.2f));
        g.drawRoundedRectangle(rf, 2.0f, 0.5f);

        if (level < 0.005f) {
            float ledW = rf.getWidth() / (float)kLEDCount;
            float ledH = rf.getHeight() - 4.0f;
            float ly   = rf.getY() + 2.0f;
            for (int i = 0; i < kLEDCount; ++i) {
                float lx = rf.getX() + 2.0f + (float)i * ledW;
                g.setColour(MixCoachTheme::divider().withAlpha(0.08f));
                g.fillRoundedRectangle({lx, ly, ledW - 1.5f, ledH}, 1.0f);
            }
            return;
        }

        float speed  = 0.006f;
        float phase  = (float)(now % 2000) * speed;
        float ledW   = rf.getWidth() / (float)kLEDCount;
        float ledH   = rf.getHeight() - 4.0f;
        float ly     = rf.getY() + 2.0f;
        int litCount = juce::jmax(1, (int)(level * (float)kLEDCount * 1.2f));

        for (int i = 0; i < kLEDCount; ++i) {
            float lx     = rf.getX() + 2.0f + (float)i * ledW;
            auto ledRect = juce::Rectangle<float>(lx, ly, ledW - 1.5f, ledH);

            float posFactor   = (i < litCount) ? 1.0f : 0.0f;
            float chaseOffset = (float)i / (float)kLEDCount;
            float chase       = 0.5f
                                + 0.5f
                                      * std::sin(phase * juce::MathConstants<float>::twoPi
                                                 - chaseOffset * juce::MathConstants<float>::twoPi * 3.0f);

            float brightness = juce::jlimit(0.0f, 1.0f, posFactor * (0.3f + 0.7f * chase));

            if (brightness < 0.01f) {
                g.setColour(MixCoachTheme::divider().withAlpha(0.04f));
                g.fillRoundedRectangle(ledRect, 1.0f);
                continue;
            }

            float normPos = (float)i / (float)kLEDCount;
            juce::Colour ledColour;
            if (normPos < 0.6f) ledColour = MixCoachTheme::success();
            else if (normPos < 0.82f)
                ledColour = MixCoachTheme::warning();
            else
                ledColour = MixCoachTheme::error();

            g.setColour(ledColour.withAlpha(brightness * 0.15f));
            g.fillRoundedRectangle(ledRect.expanded(1.0f), 1.5f);
            g.setColour(ledColour.withAlpha(brightness * 0.9f));
            g.fillRoundedRectangle(ledRect, 1.0f);
            g.setColour(juce::Colours::white.withAlpha(brightness * 0.25f));
            g.fillRoundedRectangle(ledRect.withHeight(ledH * 0.4f), 0.6f);
        }
    }

} // namespace mixcoach
