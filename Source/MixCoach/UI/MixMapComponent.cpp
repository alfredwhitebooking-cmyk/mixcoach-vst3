#include "MixMapComponent.h"
#include <cstdlib> // For std::abs
#include <cmath>   // For std::atan2, std::cos, std::sin

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  MixMapComponent — Mapa de Mezcla Visual
    // ═══════════════════════════════════════════════════════════════════════════

    MixMapComponent::MixMapComponent()
    {
        setOpaque(true);

        // Initialize smooth values for all slots with -80dB default
        for (auto& sv : levelSmooth_) {
            sv.reset(-80.0f);
            sv.setBallistics(25.0f, 250.0f); // fast attack, slow release
        }

        startTimerHz(60); // 60fps animation loop
    }

    MixMapComponent::~MixMapComponent()
    {
        stopTimer();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  timerCallback — 60fps animation: advance level meters and repaint
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapComponent::timerCallback()
    {
        bool changed = false;

        // Avanzar SmoothValues solo para slots activos
        for (const auto& group : busGroups_) {
            for (const auto& node : group.tracks) {
                int idx = node.slotIndex;
                if (idx >= 0 && idx < SlotRegistry::kMaxSlots) {
                    if (levelSmooth_[idx].advance(60.0)) changed = true;
                }
            }
        }

        if (changed) repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateData — Reconstruye los grupos de buses y tracks desde los datos vivos
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapComponent::updateData(SlotRegistry& registry,
                                     SharedData& sharedData,
                                     const std::array<TrackRole, SlotRegistry::kMaxSlots>& trackRoles)
    {
        busGroups_.clear();

        // ─── Mapa temporal: BusType → índice en busGroups_ ────────────────
        // Orden estándar de buses: Drums, Bass, Guitars, Keys, Vocals, FX, Melody, None
        static constexpr BusType kBusOrder[] = {BusType::Drums,
                                                BusType::Bass,
                                                BusType::Guitars,
                                                BusType::Keys,
                                                BusType::Vocals,
                                                BusType::FX,
                                                BusType::Melody,
                                                BusType::None};

        for (auto bus : kBusOrder) {
            BusGroup group;
            group.bus    = bus;
            group.colour = getBusColour(static_cast<int>(bus));
            group.name   = juce::String(busNames[static_cast<int>(bus)]);

            // Iterar slots activos y recoger los que pertenecen a este bus
            registry.forEachActive([&](const SlotInfo& info) {
                if (info.bus != bus) return;

                TrackNode node;
                node.slotIndex = info.slotIndex;
                node.name      = juce::String(info.trackName);
                node.colour    = info.colour;
                node.bus       = info.bus;
                node.role      = trackRoles[info.slotIndex];
                node.active    = true;

                // Audio data from SharedData cache
                auto audio        = sharedData.getTrackAudioResult(info.slotIndex);
                node.peakCombined = audio.getPeakCombined();
                node.rmsCombined  = audio.getRmsCombined();
                node.correlation  = audio.correlation;
                node.hasSignal    = node.peakCombined > -60.0f && audio.timestampUs > 0;

                // Role confidence: known role → 85%, unknown → 0%
                node.roleConfidence = (node.role != TrackRole::Unknown && node.role != TrackRole::Master) ? 0.85f
                                                                                                          : 0.0f;

                // Set smooth level target (clamp between -60 and 0 for meter range)
                if (node.hasSignal) {
                    levelSmooth_[info.slotIndex].setTargetValue(juce::jlimit(-60.0f, 0.0f, node.peakCombined));
                }
                else {
                    levelSmooth_[info.slotIndex].setTargetValue(-60.0f);
                }

                // Compute avg stereo width from 6 regions
                float widthSum = 0.0f;
                int widthCount = 0;
                for (int b = 0; b < 6; ++b) {
                    if (audio.stereoWidthPerBand[b] >= 0.0f) {
                        widthSum += audio.stereoWidthPerBand[b];
                        ++widthCount;
                    }
                }
                node.avgStereoWidth = (widthCount > 0) ? (widthSum / widthCount) : 0.0f;

                // Compute 6-region energy from 30-band data
                for (int r = 0; r < 6; ++r) {
                    float sum = 0.0f;
                    int count = 0;
                    for (int b = kRegionBands[r][0]; b < kRegionBands[r][1]; ++b) {
                        if (b < 30 && audio.bandEnergies[b] > -90.0f) {
                            sum += audio.bandEnergies[b];
                            ++count;
                        }
                    }
                    node.regionEnergy[r] = (count > 0) ? (sum / count) : -100.0f;
                }

                group.tracks.push_back(std::move(node));
            });

            if (!group.tracks.empty()) {
                // Ordenar tracks por rol (percussivo primero) y luego por nombre
                std::sort(group.tracks.begin(), group.tracks.end(), [](const TrackNode& a, const TrackNode& b) {
                    auto catA = getRoleCategory(a.role);
                    auto catB = getRoleCategory(b.role);
                    if (catA != catB) return static_cast<int>(catA) < static_cast<int>(catB);
                    return a.name.compareIgnoreCase(b.name) < 0;
                });
                busGroups_.push_back(std::move(group));
            }
        }

        // Total tracks
        totalTracks_ = 0;
        for (const auto& g : busGroups_) totalTracks_ += (int)g.tracks.size();

        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getPreferredHeight — Calcula altura total del mapa
    // ═══════════════════════════════════════════════════════════════════════════
    int MixMapComponent::getPreferredHeight() const
    {
        int h = 4; // top padding
        for (const auto& g : busGroups_) {
            h += kHeaderH + 2;                 // bus header row
            h += (int)g.tracks.size() * kRowH; // track rows
            h += 4;                            // gap between bus groups
        }
        h += 4; // bottom padding

        if (busGroups_.empty()) h += 60; // Empty state height

        return h;
    }

    void MixMapComponent::resized()
    {
        int h = getPreferredHeight();
        setSize(getWidth(), h);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Renderiza el árbol de mezcla completo
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapComponent::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds();

        // ─── Rebuild row bounds cache ────────────────────────────────────────
        trackRowBounds_.clear();

        // ─── Fondo ──────────────────────────────────────────────────────────
        g.fillAll(MixCoachTheme::bgDark());

        if (busGroups_.empty()) {
            // ─── Empty state ──────────────────────────────────────────────
            g.setFont(juce::Font(juce::FontOptions(12.0f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            auto emptyArea = bounds.reduced(20);
            g.drawText("No hay pistas activas.\nAgrega Messengers a tus pistas\npara ver el Mapa de Mezcla.",
                       emptyArea,
                       juce::Justification::centred);
            return;
        }

        int y = 4;
        int w = bounds.getWidth();

        // ═══ Bus routing connection data (for drawing lines to Master) ═══════
        struct BusConnection
        {
            int busIdx;
            int startY;
            int endY;
            juce::Colour colour;
        };

        std::vector<BusConnection> busConnections;

        for (int gIdx = 0; gIdx < (int)busGroups_.size(); ++gIdx) {
            const auto& group = busGroups_[gIdx];

            // ─── Bus Header ───────────────────────────────────────────────
            {
                auto headerArea = juce::Rectangle<int>(2, y, w - 4, kHeaderH);
                drawBusHeader(g, group, headerArea, gIdx);
                y += kHeaderH + 2;
            }

            int rowStartY = y;

            // ─── Track rows ───────────────────────────────────────────────
            for (int tIdx = 0; tIdx < (int)group.tracks.size(); ++tIdx) {
                auto rowArea = juce::Rectangle<int>(2, y, w - 4, kRowH);

                // Cache row bounds for hit testing
                trackRowBounds_.emplace_back(group.tracks[tIdx].slotIndex, rowArea);

                drawTrackRow(g, group.tracks[tIdx], rowArea, (tIdx % 2) == 0);
                y += kRowH;
            }

            // Store connection info for this bus group
            busConnections.push_back({gIdx, rowStartY, y - 2, group.colour});

            y += 4; // gap between bus groups
        }

        // ─── Draw routing connection lines from each bus group toward Master ──
        if (!busConnections.empty() && y > 0) {
            int masterSectionTop = y;       // drawMasterSection starts at current y
            int routingColX      = w - 120; // Right-side column for routing connections

            for (const auto& conn : busConnections) {
                // Draw vertical collection line (groups tracks within bus)
                g.setColour(conn.colour.withAlpha(0.12f));
                g.drawVerticalLine(routingColX, (float)conn.startY, (float)conn.endY);

                // Small dot at each track connection point
                int trackCount = (int)busGroups_[conn.busIdx].tracks.size();
                for (int t = 0; t < trackCount; ++t) {
                    float dotY = (float)(conn.startY + t * kRowH + kRowH / 2);
                    g.setColour(conn.colour.withAlpha(0.18f));
                    g.fillEllipse((float)routingColX - 1.5f, dotY - 1.5f, 3.0f, 3.0f);
                }

                // Horizontal line from bus header center to routing column
                float busCenterY = (float)(conn.startY - kHeaderH + kHeaderH / 2);
                float fromX      = (float)(w - 50); // Right edge of header arrow area
                g.setColour(conn.colour.withAlpha(0.18f));
                g.drawHorizontalLine((int)busCenterY, fromX, (float)routingColX);

                // Small diamond at intersection
                g.setColour(conn.colour.withAlpha(0.25f));
                g.fillEllipse((float)routingColX - 2.0f, busCenterY - 2.0f, 4.0f, 4.0f);

                // Angled line from routing column to master section
                float targetX = (float)(w / 2);
                drawRoutingArrow(g,
                                 juce::Point<float>((float)routingColX, busCenterY),
                                 juce::Point<float>(targetX, (float)masterSectionTop + 6.0f),
                                 conn.colour,
                                 0.15f);
            }
        }

        // ─── Draw Master Bus section at the bottom ───────────────────────
        {
            auto masterArea = juce::Rectangle<int>(2, y, w - 4, kHeaderH + 8);
            drawMasterSection(g, masterArea);
            y = masterArea.getBottom() + 4;
        }

        // ─── Draw confirm map button at the bottom ────────────────────────
        {
            int btnY             = y + 8;
            int btnH             = 28;
            auto btnArea         = juce::Rectangle<int>(bounds.getCentreX() - 110, btnY, 220, btnH);
            confirmButtonBounds_ = btnArea;

            bool complete = isMapComplete();
            juce::Colour btnColour;
            juce::String btnLabel;

            if (mapConfirmed_) {
                btnColour = MixCoachTheme::success();
                btnLabel  = "\xE2\x9C\x93 MAPA CONFIRMADO";
            }
            else if (complete) {
                btnColour = MixCoachTheme::success();
                btnLabel  = "\xE2\x9C\x85 CONFIRMAR MAPA";
            }
            else {
                btnColour = MixCoachTheme::warning();
                btnLabel  = "\xE2\x9A\xA0 ASIGNAR BUSES";
            }

            // ─── Shadow ───────────────────────────────────────────────────
            g.setColour(juce::Colours::black.withAlpha(0.20f));
            g.fillRoundedRectangle(btnArea.toFloat().expanded(1.0f, 1.5f), 8.0f);

            // ─── Background gradient ──────────────────────────────────────
            juce::ColourGradient btnGrad(btnColour.withAlpha(mapConfirmed_ ? 0.10f : 0.14f),
                                         (float)btnArea.getCentreX(),
                                         (float)btnArea.getY(),
                                         btnColour.withAlpha(mapConfirmed_ ? 0.05f : 0.07f),
                                         (float)btnArea.getCentreX(),
                                         (float)btnArea.getBottom(),
                                         false);
            if (!mapConfirmed_) btnGrad.addColour(0.5f, btnColour.withAlpha(0.12f));
            g.setGradientFill(btnGrad);
            g.fillRoundedRectangle(btnArea.toFloat(), 8.0f);

            // ─── Glass highlight ─────────────────────────────────────────
            if (!mapConfirmed_) {
                auto glassH = btnArea.withHeight(btnArea.getHeight() / 2).toFloat();
                juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.06f),
                                               glassH.getX(),
                                               glassH.getY(),
                                               juce::Colour(0x00000000),
                                               glassH.getX(),
                                               glassH.getBottom(),
                                               false);
                g.setGradientFill(glassGrad);
                g.fillRoundedRectangle(glassH, 8.0f);
            }

            // ─── Border ──────────────────────────────────────────────────
            float borderAlpha = mapConfirmed_ ? 0.25f : 0.40f;
            g.setColour(btnColour.withAlpha(borderAlpha));
            g.drawRoundedRectangle(btnArea.toFloat(), 8.0f, 0.7f);

            // ─── Text ────────────────────────────────────────────────────
            g.setFont(juce::Font(juce::FontOptions(9.5f)).boldened());
            if (mapConfirmed_) g.setColour(MixCoachTheme::success());
            else if (complete)
                g.setColour(juce::Colours::white);
            else
                g.setColour(MixCoachTheme::warning());
            g.drawText(btnLabel, btnArea, juce::Justification::centred);

            // Update y for overall height (used by getPreferredHeight + resized)
            y = btnArea.getBottom() + 4;
        }

        // ─── Draw hover tooltip on top of everything ───────────────────────
        if (hoveredSlotIndex_ >= 0) {
            auto mousePos = getMouseXYRelative();
            drawTrackTooltip(g, mousePos);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  visibilityChanged — Pausa el timer cuando el componente no está visible
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapComponent::visibilityChanged()
    {
        if (isShowing()) startTimerHz(60);
        else
            stopTimer();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  mouseDown — Detecta clics en filas de pistas
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapComponent::mouseDown(const juce::MouseEvent& e)
    {
        // Buscar la fila donde ocurrió el clic
        for (const auto& [slotIndex, bounds] : trackRowBounds_) {
            if (bounds.contains(e.getPosition())) {
                if (slotIndex != selectedSlotIndex_) {
                    selectedSlotIndex_ = slotIndex;
                    repaint();
                    if (onTrackSelected) onTrackSelected(slotIndex);
                }
                return;
            }
        }

        // ─── Check confirm map button click (solo cuando el mapa está completo) ─
        if (!mapConfirmed_ && isMapComplete() && confirmButtonBounds_.contains(e.getPosition())) {
            if (onConfirmMap) onConfirmMap();
            return;
        }

        // Clic fuera de cualquier pista → deseleccionar
        if (selectedSlotIndex_ >= 0) {
            selectedSlotIndex_ = -1;
            repaint();
            if (onTrackSelected) onTrackSelected(-1);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  mouseMove — Track hover for tooltip display
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapComponent::mouseMove(const juce::MouseEvent& e)
    {
        int newHover = -1;
        auto pos     = e.getPosition();

        for (const auto& [slotIndex, bounds] : trackRowBounds_) {
            if (bounds.contains(pos)) {
                newHover = slotIndex;
                break;
            }
        }

        if (newHover != hoveredSlotIndex_) {
            hoveredSlotIndex_ = newHover;
            repaint();
        }

        setMouseCursor(newHover >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  mouseExit — Clear hover state when mouse leaves the component
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapComponent::mouseExit(const juce::MouseEvent&)
    {
        if (hoveredSlotIndex_ >= 0) {
            hoveredSlotIndex_ = -1;
            repaint();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawBusHeader — Barra de encabezado del bus con color y contador
    // ═══════════════════════════════════════════════════════════════════════════
    void
    MixMapComponent::drawBusHeader(juce::Graphics& g, const BusGroup& group, juce::Rectangle<int> area, int /*index*/)
    {
        const float cr = kCr;

        // ─── Shadow sutil ───────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.20f));
        g.fillRoundedRectangle(area.expanded(0, 1).toFloat(), cr + 1.0f);

        // ─── Fondo con gradiente del bus ────────────────────────────────────
        juce::ColourGradient bgGrad(group.colour.withAlpha(0.20f),
                                    (float)area.getX(),
                                    (float)area.getY(),
                                    group.colour.withAlpha(0.08f),
                                    (float)area.getX(),
                                    (float)area.getBottom(),
                                    false);
        bgGrad.addColour(0.5f, group.colour.withAlpha(0.14f));
        g.setGradientFill(bgGrad);
        g.fillRoundedRectangle(area.toFloat(), cr);

        // ─── Glass highlight ────────────────────────────────────────────────
        auto glassH = area.withHeight(area.getHeight() / 2).toFloat();
        juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.06f),
                                       glassH.getX(),
                                       glassH.getY(),
                                       juce::Colour(0x00000000),
                                       glassH.getX(),
                                       glassH.getBottom(),
                                       false);
        g.setGradientFill(glassGrad);
        g.fillRoundedRectangle(glassH, cr);

        // ─── Borde izquierdo de color (accent bar) ─────────────────────────
        g.setColour(group.colour.withAlpha(0.6f));
        g.fillRoundedRectangle(
            juce::Rectangle<float>(
                (float)area.getX() + 1.0f, (float)area.getY() + 2.0f, 3.0f, (float)area.getHeight() - 4.0f),
            1.5f);

        // ─── Borde exterior ────────────────────────────────────────────────
        g.setColour(group.colour.withAlpha(0.25f));
        g.drawRoundedRectangle(area.toFloat(), cr, 0.5f);

        // ─── Texto del header ──────────────────────────────────────────────
        auto textArea = area.reduced(10, 0);

        // Bus icon + name (left)
        juce::String headerText = juce::String::fromUTF8("\xE2\x96\xA0 ") // ■
                                  + group.name.toUpperCase() + " (" + juce::String((int)group.tracks.size()) + ")";

        g.setFont(juce::Font(juce::FontOptions(9.5f)).boldened());
        g.setColour(group.colour.brighter(0.6f));
        g.drawText(headerText, textArea, juce::Justification::centredLeft);

        // ─── Routing arrow: bus name ───▶ Master ──────────────────────────
        auto rightArea = area.reduced(10, 0);
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());

        // "MASTER" label on far right
        auto masterLabelArea = rightArea.removeFromRight(60);
        g.setColour(MixCoachTheme::textBright().withAlpha(0.6f));
        g.drawText("MASTER", masterLabelArea, juce::Justification::centredRight);

        // Arrow line with bus colour
        int arrowX1 = masterLabelArea.getX() - 40;
        int arrowX2 = masterLabelArea.getX() - 4;
        int arrowY  = rightArea.getCentreY();

        // Arrow line
        g.setColour(group.colour.withAlpha(0.40f));
        g.drawHorizontalLine(arrowY, (float)arrowX1, (float)arrowX2);

        // Arrow head (filled triangle)
        juce::Path arrowHead;
        arrowHead.addTriangle((float)arrowX2,
                              (float)arrowY,
                              (float)arrowX2 - 6.0f,
                              (float)arrowY - 4.0f,
                              (float)arrowX2 - 6.0f,
                              (float)arrowY + 4.0f);
        g.fillPath(arrowHead);

        // Bus name label above the arrow
        auto busLabelArea = juce::Rectangle<int>(arrowX1 - 50, arrowY - 8, 48, 16);
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)));
        g.setColour(group.colour.withAlpha(0.55f));
        g.drawText(group.name.toUpperCase(), busLabelArea, juce::Justification::centredRight);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawTrackRow — Fila de pista individual con rol, estéreo, frecuencia
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapComponent::drawTrackRow(juce::Graphics& g, const TrackNode& node, juce::Rectangle<int> area, bool even)
    {
        // ─── Selection highlight ────────────────────────────────────────────
        bool isSelected = (node.slotIndex == selectedSlotIndex_);
        if (isSelected) {
            // Fondo de selección con gradiente violeta sutil
            juce::ColourGradient selGrad(MixCoachTheme::accent().withAlpha(0.12f),
                                         (float)area.getX(),
                                         (float)area.getY(),
                                         MixCoachTheme::accent().withAlpha(0.04f),
                                         (float)area.getX(),
                                         (float)area.getBottom(),
                                         false);
            g.setGradientFill(selGrad);
            g.fillRect(area);

            // Borde izquierdo de selección (accent bar)
            g.setColour(MixCoachTheme::accent().withAlpha(0.5f));
            g.fillRect(area.getX(), area.getY() + 2, 3, area.getHeight() - 4);
        }
        else {
            // ─── Fondo alternado sutil ────────────────────────────────────
            if (even) {
                g.setColour(MixCoachTheme::bgDarker().withAlpha(0.3f));
                g.fillRect(area);
            }
        }

        auto inner = area.reduced(6, 0);
        int cx     = inner.getX();

        // ═══ 1. Color dot (rol) ═════════════════════════════════════════════
        auto dotArea = juce::Rectangle<int>(cx, area.getCentreY() - kDotSize / 2, kDotSize, kDotSize);
        g.setColour(node.colour.withAlpha(0.5f));
        g.fillEllipse(dotArea.toFloat().expanded(1.0f, 1.0f));
        g.setColour(node.colour);
        g.fillEllipse(dotArea.toFloat());
        cx += kDotSize + 6;

        // ═══ 2. Level bar (animated) ═════════════════════════════════════════
        float smoothDb = -80.0f;
        if (node.slotIndex >= 0 && node.slotIndex < (int)levelSmooth_.size())
            smoothDb = levelSmooth_[node.slotIndex].getCurrent();

        auto levelBarArea = juce::Rectangle<int>(cx, area.getCentreY() - 5, kLevelBarW, 10);
        drawLevelBar(g, levelBarArea, smoothDb);
        cx = levelBarArea.getRight() + 6;

        // ═══ 2b. Issue badge (severity dot + short type) ════════════════════
        const auto& badge = (node.slotIndex >= 0 && node.slotIndex < (int)trackIssueBadges_.size())
                                ? trackIssueBadges_[node.slotIndex]
                                : TrackIssueBadge{};

        if (badge.hasIssues || badge.isOptimal) {
            juce::Colour badgeColour;
            juce::String badgeIcon;

            if (badge.isCritical) {
                badgeColour = MixCoachTheme::error();
                badgeIcon   = juce::String::fromUTF8("\xE2\x9C\x97"); // ✗
            }
            else if (badge.maxSeverity > 0.3f) {
                badgeColour = MixCoachTheme::warning();
                badgeIcon   = juce::String::fromUTF8("\xE2\x9A\xA0"); // ⚠
            }
            else if (badge.hasIssues) {
                badgeColour = MixCoachTheme::accentCyan();
                badgeIcon   = "\xE2\x84\xB9"; // info
            }
            else { // isOptimal
                badgeColour = MixCoachTheme::success();
                badgeIcon   = juce::String::fromUTF8("\xE2\x9C\x93"); // ✓
            }

            int badgeW     = 24;
            auto badgeArea = juce::Rectangle<int>(cx, area.getCentreY() - 7, badgeW, 14);

            // Badge background
            g.setColour(badgeColour.withAlpha(0.12f));
            g.fillRoundedRectangle(badgeArea.toFloat(), 3.0f);
            g.setColour(badgeColour.withAlpha(badge.isCritical ? 0.50f : 0.30f));
            g.drawRoundedRectangle(badgeArea.toFloat(), 3.0f, 0.5f);

            // Badge icon + short type
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened());
            g.setColour(badgeColour);
            juce::String badgeText = badgeIcon;
            if (badge.shortType.isNotEmpty() && (badge.isCritical || badge.maxSeverity > 0.3f))
                badgeText += " " + badge.shortType;
            g.drawText(badgeText, badgeArea, juce::Justification::centred);

            cx = badgeArea.getRight() + 4;
        }

        // ═══ 3. Track name + role + confidence indicator ════════════════════
        juce::String displayName = node.name;
        const char* roleName     = roleDisplayName(node.role);
        juce::String roleStr     = juce::String(roleName);
        if (roleStr.isNotEmpty() && roleStr != "Unknown" && !displayName.containsIgnoreCase(roleStr)) {
            displayName += " (" + roleStr;

            // Add confidence emoji
            if (node.roleConfidence >= 0.75f) displayName += " \xE2\x9C\x93"; // ✅
            else if (node.roleConfidence >= 0.4f)
                displayName += " \xE2\x9A\xA0"; // ⚠️
            else if (node.roleConfidence > 0.0f)
                displayName += " \xE2\x9D\x8C"; // ❌

            displayName += ")";
        }
        else if (roleStr.isEmpty() || roleStr == "Unknown") {
            // Unknown role — show ? icon
            displayName += " (?)";
        }

        int remainingW = inner.getRight() - cx - kFreqBarW - kBadgeW - 60; // 60 = routing + gaps
        remainingW     = juce::jmax(60, remainingW);

        auto nameArea = juce::Rectangle<int>(cx, area.getY(), remainingW, area.getHeight());
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.setColour(MixCoachTheme::textPrimary());
        g.drawText(displayName, nameArea, juce::Justification::centredLeft);
        cx = nameArea.getRight() + 4;

        // ═══ 3. Stereo badge ════════════════════════════════════════════════
        StereoPos stereo = classifyStereo(node.correlation, node.avgStereoWidth);
        auto badgeArea   = juce::Rectangle<int>(cx, area.getCentreY() - 9, kBadgeW, 18);
        drawStereoBadge(g, badgeArea, stereo);
        cx = badgeArea.getRight() + 4;

        // ═══ 4. Frequency bar ═══════════════════════════════════════════════
        auto freqArea = juce::Rectangle<int>(cx, area.getCentreY() - kFreqBarH / 2, kFreqBarW, kFreqBarH);
        drawFreqBar(g, freqArea, node.regionEnergy);
        cx = freqArea.getRight() + 4;

        // ═══ 5. Subtle routing dot indicator ═══════════════════════════════
        // Small colored dot showing bus routing, keeps the UI clean
        auto dotIndArea     = juce::Rectangle<int>(cx + 4, area.getCentreY() - 3, 6, 6);
        juce::Colour dotCol = (node.bus != BusType::None) ? getBusColour(static_cast<int>(node.bus)).withAlpha(0.40f)
                                                          : MixCoachTheme::textMuted().withAlpha(0.15f);
        g.setColour(dotCol);
        g.fillEllipse(dotIndArea.toFloat());
        // Tiny outward line indicating routing direction
        g.setColour(dotCol.withAlpha(0.30f));
        g.drawHorizontalLine(area.getCentreY(), (float)dotIndArea.getRight() + 2.0f, (float)(inner.getRight() - 4));
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawLevelBar — Barra de nivel animada (Peak meter) con gradiente verde→amarillo→rojo
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapComponent::drawLevelBar(juce::Graphics& g, juce::Rectangle<int> area, float levelDb) const
    {
        auto rect      = area.toFloat();
        const float cr = 2.0f;

        // ─── Background (glass) ─────────────────────────────────────────────
        g.setColour(MixCoachTheme::bgDarker().withAlpha(0.6f));
        g.fillRoundedRectangle(rect, cr);
        g.setColour(MixCoachTheme::bgSurface().withAlpha(0.35f));
        g.drawRoundedRectangle(rect, cr, 0.5f);

        // ─── Normalize level: -60dB → 0.0, 0dB → 1.0 ───────────────────────
        float norm = juce::jlimit(0.0f, 1.0f, (levelDb + 60.0f) / 60.0f);

        if (norm > 0.005f) {
            float fillW     = rect.getWidth() * norm;
            auto fillBounds = rect.withWidth(fillW);

            // ─── Color por zona ───────────────────────────────────────────
            // 0-50%: verde, 50-80%: amarillo, 80-100%: rojo
            juce::Colour barColour;
            if (norm < 0.50f)
                barColour = MixCoachTheme::success().interpolatedWith(MixCoachTheme::warning(), norm / 0.50f);
            else if (norm < 0.80f)
                barColour = MixCoachTheme::warning().interpolatedWith(MixCoachTheme::error(), (norm - 0.50f) / 0.30f);
            else
                barColour = MixCoachTheme::error();

            // ─── Gradiente vertical (glow en la parte superior) ───────────
            juce::ColourGradient barGrad(barColour.withAlpha(0.85f),
                                         fillBounds.getX(),
                                         fillBounds.getY(),
                                         barColour.withAlpha(0.50f),
                                         fillBounds.getX(),
                                         fillBounds.getBottom(),
                                         false);
            g.setGradientFill(barGrad);
            g.fillRoundedRectangle(fillBounds, cr);

            // ─── Shine highlight en la parte superior ────────────────────
            if (fillW > 6.0f) {
                auto shine = fillBounds.withHeight(juce::jmax(1.5f, fillBounds.getHeight() * 0.35f));
                juce::ColourGradient shineGrad(juce::Colours::white.withAlpha(0.18f),
                                               shine.getX(),
                                               shine.getY(),
                                               juce::Colour(0x00000000),
                                               shine.getX(),
                                               shine.getBottom(),
                                               false);
                g.setGradientFill(shineGrad);
                g.fillRoundedRectangle(shine, cr);
            }

            // ─── Peak hold dot at the right edge ─────────────────────────
            if (fillW > 6.0f) {
                float dotX = fillBounds.getRight() - 1.0f;
                float dotY = fillBounds.getCentreY();
                g.setColour(barColour.withAlpha(0.60f));
                g.fillEllipse(dotX - 1.5f, dotY - 1.5f, 3.0f, 3.0f);
                g.setColour(MixCoachTheme::textPrimary().withAlpha(0.70f));
                g.fillEllipse(dotX - 0.8f, dotY - 0.8f, 1.6f, 1.6f);
            }
        }

        // ─── Grid lines sutiles (puntos de referencia: -20, -10, -3 dB) ─
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        auto drawGridLine = [&](float thresholdDb) {
            float t  = (thresholdDb + 60.0f) / 60.0f;
            float lx = rect.getX() + rect.getWidth() * t;
            g.drawVerticalLine((int)lx, (int)rect.getY() + 1, (int)rect.getBottom() - 1);
        };
        drawGridLine(-20.0f); // -20dB
        drawGridLine(-10.0f); // -10dB
        drawGridLine(-3.0f);  // -3dB
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawFreqBar — Barra de frecuencia de 6 regiones con colores por energía
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapComponent::drawFreqBar(juce::Graphics& g, juce::Rectangle<int> area, const float regionEnergy[6]) const
    {
        int numBands = 6;
        float barW   = (float)area.getWidth() / (float)numBands;
        float barH   = (float)area.getHeight();
        float x      = (float)area.getX();
        float y      = (float)area.getY();

        // Encontrar región con más energía para normalizar
        float maxEnergy = -100.0f;
        for (int r = 0; r < 6; ++r) maxEnergy = juce::jmax(maxEnergy, regionEnergy[r]);

        // Colores de región: Sub=Bass, Bass=Bass, LoMid=Mid, HiMid=Mid, Pres=High, Air=High
        static const juce::Colour kRegionColors[6] = {MixCoachTheme::specSub(),
                                                      MixCoachTheme::specBass(),
                                                      MixCoachTheme::specLoMid(),
                                                      MixCoachTheme::specHiMid(),
                                                      MixCoachTheme::specPres(),
                                                      MixCoachTheme::specAir()};

        // ─── Background bar (glass) ───────────────────────────────────────
        g.setColour(MixCoachTheme::bgInput().withAlpha(0.4f));
        g.fillRoundedRectangle(area.toFloat(), 2.0f);

        // ─── Region bars ───────────────────────────────────────────────────
        for (int r = 0; r < numBands; ++r) {
            float energy   = regionEnergy[r];
            float fraction = 0.0f;

            if (maxEnergy > -90.0f && energy > -90.0f) {
                // Mapear: -60dB → 0.0, -6dB → 1.0 (con clamp)
                float normalized = (energy + 60.0f) / 54.0f; // -60 a -6 → 0 a 1
                fraction         = juce::jlimit(0.05f, 1.0f, normalized);
            }
            else {
                fraction = 0.05f; // Sin señal: barra mínima
            }

            float barX       = x + (float)r * barW;
            float barY       = y + barH * (1.0f - fraction);
            float barHActual = barH * fraction;

            auto barRect = juce::Rectangle<float>(barX + 1.0f, barY, barW - 2.0f, barHActual);

            // Gradiente vertical para efecto glow
            juce::ColourGradient barGrad(kRegionColors[r].withAlpha(0.7f * fraction + 0.1f),
                                         barRect.getX(),
                                         barRect.getY(),
                                         kRegionColors[r].withAlpha(0.3f * fraction),
                                         barRect.getX(),
                                         barRect.getBottom(),
                                         false);
            g.setGradientFill(barGrad);
            g.fillRoundedRectangle(barRect, 1.5f);

            // Línea sutil entre regiones
            g.setColour(MixCoachTheme::bgDarker().withAlpha(0.3f));
            g.drawVerticalLine((int)(barX + barW), (int)y, (int)(y + barH));
        }

        // ─── Labels pequeños en la parte inferior ──────────────────────────
        static const char* kShortLabels[6] = {"SUB", "BAS", "LOM", "HIM", "PRE", "AIR"};
        g.setFont(juce::Font(juce::FontOptions(5.5f)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
        for (int r = 0; r < numBands; ++r) {
            float labelX = x + (float)r * barW;
            g.drawText(kShortLabels[r],
                       juce::Rectangle<float>(labelX, y + barH - 9.0f, barW, 8.0f),
                       juce::Justification::centred);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawStereoBadge — Badge coloreado con indicación de posición estéreo
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapComponent::drawStereoBadge(juce::Graphics& g, juce::Rectangle<int> area, StereoPos pos) const
    {
        auto rect                = area.toFloat();
        juce::Colour badgeColour = stereoColor(pos);
        juce::String label       = stereoLabel(pos);

        // ─── Fondo del badge ────────────────────────────────────────────────
        g.setColour(badgeColour.withAlpha(0.12f));
        g.fillRoundedRectangle(rect, 3.0f);
        g.setColour(badgeColour.withAlpha(0.35f));
        g.drawRoundedRectangle(rect, 3.0f, 0.5f);

        // ─── Texto ─────────────────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened());
        g.setColour(badgeColour);
        g.drawText(label, rect, juce::Justification::centred);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Clasificaciones semánticas
    // ═══════════════════════════════════════════════════════════════════════════

    MixMapComponent::StereoPos MixMapComponent::classifyStereo(float correlation, float avgWidth) noexcept
    {
        if (correlation < 0.0f) return StereoPos::PhaseIssue;
        if (avgWidth < 0.1f) return StereoPos::Mono;
        if (correlation > 0.85f && avgWidth < 0.2f) return StereoPos::Center;
        if (correlation > 0.6f && avgWidth < 0.3f) return StereoPos::Narrow;
        if (avgWidth < 0.5f) return StereoPos::Wide;
        return StereoPos::Spread;
    }

    MixMapComponent::FreqRange MixMapComponent::classifyFrequency(const float regionEnergy[6]) noexcept
    {
        // Encontrar la región con energía máxima y la segunda
        int maxIdx      = 0;
        int secondIdx   = 0;
        float maxVal    = regionEnergy[0];
        float secondVal = -100.0f;

        for (int r = 1; r < 6; ++r) {
            if (regionEnergy[r] > maxVal) {
                secondIdx = maxIdx;
                secondVal = maxVal;
                maxIdx    = r;
                maxVal    = regionEnergy[r];
            }
            else if (regionEnergy[r] > secondVal) {
                secondIdx = r;
                secondVal = regionEnergy[r];
            }
        }

        if (maxVal < -55.0f) return FreqRange::Silent;

        // Clasificar según las 2 regiones dominantes
        int avgRegion = (maxIdx + secondIdx) / 2;

        if (avgRegion <= 0) return FreqRange::SubBass;
        if (avgRegion <= 1) return FreqRange::BassMid;
        if (avgRegion <= 2) return FreqRange::Mid;
        if (avgRegion <= 3) return FreqRange::MidHigh;
        if (avgRegion <= 4) return FreqRange::High;

        // Si ambas regiones están muy separadas → Full
        if (std::abs(maxIdx - secondIdx) >= 3) return FreqRange::Full;

        return FreqRange::High;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Helpers de texto y color
    // ═══════════════════════════════════════════════════════════════════════════

    juce::String MixMapComponent::stereoLabel(StereoPos pos) noexcept
    {
        switch (pos) {
            case StereoPos::Mono:
                return "MONO";
            case StereoPos::Center:
                return "CENTER";
            case StereoPos::Narrow:
                return "NARROW";
            case StereoPos::Wide:
                return "WIDE";
            case StereoPos::Spread:
                return "SPREAD";
            case StereoPos::PhaseIssue:
                return juce::String::fromUTF8("\xE2\x9A\xA0PHASE"); // ⚠PHASE
        }
        return "";
    }

    juce::Colour MixMapComponent::stereoColor(StereoPos pos) noexcept
    {
        switch (pos) {
            case StereoPos::Mono:
                return MixCoachTheme::textDim(); // gray
            case StereoPos::Center:
                return MixCoachTheme::success(); // green
            case StereoPos::Narrow:
                return MixCoachTheme::info(); // blue
            case StereoPos::Wide:
                return MixCoachTheme::warning(); // amber
            case StereoPos::Spread:
                return MixCoachTheme::roleDrums(); // purple
            case StereoPos::PhaseIssue:
                return MixCoachTheme::error(); // red
        }
        return juce::Colours::grey;
    }

    juce::String MixMapComponent::freqLabel(FreqRange range) noexcept
    {
        switch (range) {
            case FreqRange::SubBass:
                return "SUB-BASS";
            case FreqRange::BassMid:
                return "BASS-MID";
            case FreqRange::Mid:
                return "MID";
            case FreqRange::MidHigh:
                return "MID-HIGH";
            case FreqRange::High:
                return "HIGH";
            case FreqRange::Full:
                return "FULL";
            case FreqRange::Silent:
                return "—";
        }
        return "";
    }

    juce::Colour MixMapComponent::freqColor(FreqRange range) noexcept
    {
        switch (range) {
            case FreqRange::SubBass:
                return MixCoachTheme::roleDrums(); // violet
            case FreqRange::BassMid:
                return MixCoachTheme::info(); // blue
            case FreqRange::Mid:
                return MixCoachTheme::success(); // green
            case FreqRange::MidHigh:
                return MixCoachTheme::warning(); // amber
            case FreqRange::High:
                return MixCoachTheme::roleGuitars(); // orange
            case FreqRange::Full:
                return MixCoachTheme::roleVocals(); // pink
            case FreqRange::Silent:
                return MixCoachTheme::textDim(); // gray
        }
        return juce::Colours::grey;
    }

    const char* MixMapComponent::roleDisplayName(TrackRole role) noexcept
    {
        return getRoleName(role);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawTrackTooltip — Popup con info detallada de la pista al hacer hover
    //  Muestra: nombre, rol, confianza, peak, RMS, stereo, issues, routing
    //  V2: Añadida guía de asignación de bus (sugerencia según rol, warning si falta)
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapComponent::drawTrackTooltip(juce::Graphics& g, juce::Point<int> mousePos) const
    {
        // ─── Find the TrackNode for the hovered slot ────────────────────────
        const TrackNode* hoveredNode = nullptr;
        for (const auto& group : busGroups_) {
            for (const auto& t : group.tracks) {
                if (t.slotIndex == hoveredSlotIndex_) {
                    hoveredNode = &t;
                    break;
                }
            }
            if (hoveredNode) break;
        }

        if (hoveredNode == nullptr) return;

        const auto& node  = *hoveredNode;
        const int slotIdx = node.slotIndex;

        // ─── Precompute bus assignment guidance ─────────────────────────────
        bool needsBus        = (node.bus == BusType::None);
        BusType suggestedBus = getBusForRole(node.role);
        bool hasSuggestedBus = (suggestedBus != BusType::None);
        // (suggestedBus != BusType::None in first check already covers this, but be explicit)

        // ─── Build tooltip text ────────────────────────────────────────────
        juce::StringArray lines;

        // Header: track name + role
        juce::String header  = node.name;
        const char* roleName = roleDisplayName(node.role);
        juce::String roleStr(roleName);
        if (roleStr.isNotEmpty() && roleStr != "Unknown") header += " — " + roleStr;
        lines.add(header);
        lines.add(""); // spacer

        // Levels
        if (node.hasSignal) {
            lines.add("Peak: " + juce::String(node.peakCombined, 1) + " dBFS");
            lines.add("RMS:  " + juce::String(node.rmsCombined, 1) + " dBFS");

            // Crest factor (peak - rms)
            float crest = node.peakCombined - node.rmsCombined;
            if (crest > 0.0f && crest < 60.0f) lines.add("Crest: " + juce::String(crest, 1) + " dB");
        }
        else {
            lines.add("Sin señal");
        }

        lines.add(""); // spacer

        // Stereo
        StereoPos stereo = classifyStereo(node.correlation, node.avgStereoWidth);
        lines.add("Est\xC3\xA9reo: " + stereoLabel(stereo) + " (\xCF\x81=" + juce::String(node.correlation, 2) + ")");

        // Role confidence
        if (node.roleConfidence > 0.0f) {
            juce::String confidenceLabel;
            if (node.roleConfidence >= 0.9f) confidenceLabel = "\xE2\x9C\x85 Alta";
            else if (node.roleConfidence >= 0.6f)
                confidenceLabel = "\xE2\x9A\xA0\xEF\xB8\x8F Media";
            else
                confidenceLabel = "\xE2\x9D\x8C Baja";
            lines.add("Confianza rol: " + confidenceLabel + " (" + juce::String(node.roleConfidence * 100.0f, 0)
                      + "%)");
        }

        // Issues
        if (slotIdx >= 0 && slotIdx < (int)trackIssueBadges_.size()) {
            const auto& badge = trackIssueBadges_[slotIdx];
            if (badge.hasIssues && badge.issueCount > 0) {
                juce::String issueLine = "Issues: ";
                if (badge.isCritical)
                    issueLine += "\xF0\x9F\x94\xB4 " + juce::String(badge.issueCount) + " cr\xC3\xADtico(s)";
                else if (badge.maxSeverity > 0.3f)
                    issueLine += "\xF0\x9F\x9F\xA1 " + juce::String(badge.issueCount) + " warning(s)";
                else
                    issueLine += "\xE2\x84\xB9\xEF\xB8\x8F " + juce::String(badge.issueCount) + " info";
                lines.add(issueLine);

                if (badge.shortType.isNotEmpty()) lines.add("Tipo: " + badge.shortType);
            }
            else if (badge.isOptimal) {
                lines.add("\xE2\x9C\x85 Sin issues — \xC3\xB3ptimo");
            }
        }

        // ═══ Routing section with visual separator ═══════════════════════════
        lines.add(""); // spacer

        // Bus routing line — always shown
        juce::String busLine;
        if (needsBus) {
            // Track needs bus assignment — show warning + suggestion
            busLine = "\xE2\x9A\xA0\xEF\xB8\x8F Sin bus asignado";
            lines.add(busLine);

            if (hasSuggestedBus) {
                // Role is known — suggest the matching bus
                lines.add("   \xE2\x86\xA8 Sugerido: " + juce::String(busNames[static_cast<int>(suggestedBus)]));
            }
            else {
                // Role is unknown — can't suggest a specific bus
                lines.add("   \xE2\x86\xA8 Asigna rol para sugerencia de bus");
            }
        }
        else {
            // Bus assigned — show routing info
            busLine = "\xF0\x9F\x94\x80 Enrutamiento: " + juce::String(busNames[static_cast<int>(node.bus)])
                      + " \xE2\x86\x92 Master";
            lines.add(busLine);

            // If the track's role suggests a different bus, mention it
            if (hasSuggestedBus && suggestedBus != node.bus) {
                lines.add("   \xE2\x86\xA8 Rol sugiere: " + juce::String(busNames[static_cast<int>(suggestedBus)]));
            }
        }

        // ─── Layout: calculate tooltip dimensions ───────────────────────────
        int totalH      = 0;
        const int lineH = 12;
        const int padX  = 10;
        const int padY  = 8;

        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        for (const auto& line : lines) {
            totalH += (line.isEmpty() ? 4 : lineH);
        }

        // Use a slightly wider tooltip for routing guidance
        int tipW = 280;
        int tipH = totalH + padY * 2;

        // ─── Position: prefer right of cursor, flip left if near edge ───────
        auto parentSize = getLocalBounds();
        int tipX        = mousePos.x + 14;
        if (tipX + tipW > parentSize.getRight() - 4) tipX = mousePos.x - tipW - 6;
        if (tipX < 4) tipX = 4;

        int tipY = mousePos.y - 10;
        if (tipY + tipH > parentSize.getBottom() - 4) tipY = parentSize.getBottom() - tipH - 4;
        if (tipY < 4) tipY = 4;

        auto tipBounds = juce::Rectangle<int>(tipX, tipY, tipW, tipH).toFloat();

        // ─── Draw tooltip background ────────────────────────────────────────
        // Shadow
        g.setColour(juce::Colours::black.withAlpha(0.40f));
        g.fillRoundedRectangle(tipBounds.translated(2.0f, 2.0f), 6.0f);

        // Dark glass background
        g.setColour(MixCoachTheme::bgDark().withAlpha(0.95f));
        g.fillRoundedRectangle(tipBounds, 6.0f);

        // Border
        g.setColour(MixCoachTheme::accent().withAlpha(0.25f));
        g.drawRoundedRectangle(tipBounds, 6.0f, 1.0f);

        // Accent bar at left edge — use bus colour when assigned, warning when missing
        juce::Colour accentCol = needsBus ? MixCoachTheme::warning() : node.colour;
        g.setColour(accentCol.withAlpha(0.5f));
        g.fillRoundedRectangle(
            juce::Rectangle<float>(tipBounds.getX() + 1.0f, tipBounds.getY() + 4.0f, 2.5f, tipBounds.getHeight() - 8.0f),
            1.5f);

        // ─── Draw text lines ────────────────────────────────────────────────
        int textY = tipBounds.getY() + padY;
        g.setFont(juce::Font(juce::FontOptions(9.0f)));

        for (int i = 0; i < lines.size(); ++i) {
            const auto& line = lines[i];
            if (line.isEmpty()) {
                textY += 4; // spacer
                continue;
            }

            auto textBounds = juce::Rectangle<float>(
                tipBounds.getX() + padX + 4.0f, (float)textY, tipBounds.getWidth() - padX * 2 - 4.0f, (float)lineH);

            if (i == 0) {
                // Header line: bold + larger
                g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
                g.setColour(MixCoachTheme::textBright());
                g.drawText(line, textBounds.toNearestInt(), juce::Justification::centredLeft);
            }
            else if (line.startsWith("Peak:")) {
                g.setFont(juce::Font(juce::FontOptions(9.0f)));
                // Peak color coding
                float peakVal       = node.peakCombined;
                juce::Colour valCol = (peakVal > -0.5f)   ? MixCoachTheme::error()
                                      : (peakVal > -6.0f) ? MixCoachTheme::warning()
                                                          : MixCoachTheme::success();
                g.setColour(valCol);
                g.drawText(line, textBounds.toNearestInt(), juce::Justification::centredLeft);
            }
            else if (line.startsWith("RMS:")) {
                g.setFont(juce::Font(juce::FontOptions(9.0f)));
                g.setColour(MixCoachTheme::accentCyan());
                g.drawText(line, textBounds.toNearestInt(), juce::Justification::centredLeft);
            }
            else if (line.startsWith("Issues:") || line.startsWith("\xE2\x9C\x85 Sin issues")) {
                g.setFont(juce::Font(juce::FontOptions(9.0f)));
                g.setColour(MixCoachTheme::textPrimary());
                g.drawText(line, textBounds.toNearestInt(), juce::Justification::centredLeft);
            }
            else if (line.startsWith("Confianza")) {
                g.setFont(juce::Font(juce::FontOptions(8.5f)));
                g.setColour(MixCoachTheme::textMuted());
                g.drawText(line, textBounds.toNearestInt(), juce::Justification::centredLeft);
            }
            else if (line.startsWith("\xE2\x9A\xA0") && line.contains("Sin bus")) {
                // Warning: sin bus asignado — highlight in warning colour
                g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
                g.setColour(MixCoachTheme::warning());
                g.drawText(line, textBounds.toNearestInt(), juce::Justification::centredLeft);
            }
            else if (line.startsWith("\xF0\x9F\x94\x80 Enrutamiento")) {
                // Routing header — draw with accent cyan
                g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
                g.setColour(MixCoachTheme::accentCyan());
                g.drawText(line, textBounds.toNearestInt(), juce::Justification::centredLeft);
            }
            else if (line.startsWith("   \xE2\x86\xA8")) {
                // Suggestion sub-line (indented with →) — draw muted
                g.setFont(juce::Font(juce::FontOptions(8.5f)));
                g.setColour(MixCoachTheme::textMuted());
                g.drawText(line, textBounds.toNearestInt(), juce::Justification::centredLeft);
            }
            else {
                g.setFont(juce::Font(juce::FontOptions(9.0f)));
                g.setColour(MixCoachTheme::textPrimary());
                g.drawText(line, textBounds.toNearestInt(), juce::Justification::centredLeft);
            }

            textY += lineH;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawMasterSection — Master Bus summary showing all buses → Master
    //  Draws a final section that collects all bus groups into the Master bus.
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapComponent::drawMasterSection(juce::Graphics& g, juce::Rectangle<int> area)
    {
        const float cr = kCr;

        // ─── Shadow ─────────────────────────────────────────────────────────
        g.setColour(juce::Colours::black.withAlpha(0.20f));
        g.fillRoundedRectangle(area.expanded(0, 1).toFloat(), cr + 1.0f);

        // ─── Background with red-tinted gradient (Master colour) ────────────
        juce::Colour masterCol = MixCoachTheme::accentGlow().withAlpha(0.35f);
        juce::ColourGradient bgGrad(masterCol.withAlpha(0.18f),
                                    (float)area.getX(),
                                    (float)area.getY(),
                                    masterCol.withAlpha(0.06f),
                                    (float)area.getX(),
                                    (float)area.getBottom(),
                                    false);
        bgGrad.addColour(0.5f, masterCol.withAlpha(0.12f));
        g.setGradientFill(bgGrad);
        g.fillRoundedRectangle(area.toFloat(), cr);

        // ─── Glass highlight ───────────────────────────────────────────────
        auto glassH = area.withHeight(area.getHeight() / 2).toFloat();
        juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.07f),
                                       glassH.getX(),
                                       glassH.getY(),
                                       juce::Colour(0x00000000),
                                       glassH.getX(),
                                       glassH.getBottom(),
                                       false);
        g.setGradientFill(glassGrad);
        g.fillRoundedRectangle(glassH, cr);

        // ─── Accent bar (left edge) ─────────────────────────────────────────
        g.setColour(MixCoachTheme::accentGlow().withAlpha(0.5f));
        g.fillRoundedRectangle(
            juce::Rectangle<float>(
                (float)area.getX() + 1.0f, (float)area.getY() + 2.0f, 3.0f, (float)area.getHeight() - 4.0f),
            1.5f);

        // ─── Border ─────────────────────────────────────────────────────────
        g.setColour(MixCoachTheme::accentGlow().withAlpha(0.20f));
        g.drawRoundedRectangle(area.toFloat(), cr, 0.6f);

        auto textArea = area.reduced(10, 0);

        // ─── Left: Master icon + label ─────────────────────────────────────
        juce::String masterLabel = juce::String::fromUTF8("\xE2\x96\xA0 ") // ■
                                   + "MASTER BUS (" + juce::String(totalTracks_) + " tracks)";
        g.setFont(juce::Font(juce::FontOptions(9.5f)).boldened());
        g.setColour(MixCoachTheme::textBright().withAlpha(0.85f));
        g.drawText(masterLabel, textArea, juce::Justification::centredLeft);

        // ─── Right: bus count summary ─────────────────────────────────────
        auto rightArea = area.reduced(10, 0);
        juce::String busSummary;
        for (int gIdx = 0; gIdx < (int)busGroups_.size(); ++gIdx) {
            const auto& group = busGroups_[gIdx];
            if (gIdx > 0) busSummary += "  \xE2\x80\xA2  "; // bullet separator
            busSummary += group.name.toUpperCase().substring(0, 4);
        }
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
        g.drawText(busSummary, rightArea, juce::Justification::centredRight);

        // ─── Subtle glow dot on right side (routing terminus) ──────────────
        float glowCx = (float)rightArea.getRight() - 8.0f;
        float glowCy = (float)area.getCentreY();
        g.setColour(MixCoachTheme::accentGlow().withAlpha(0.10f));
        g.fillEllipse(glowCx - 6.0f, glowCy - 6.0f, 12.0f, 12.0f);
        g.setColour(MixCoachTheme::accentGlow().withAlpha(0.25f));
        g.fillEllipse(glowCx - 3.0f, glowCy - 3.0f, 6.0f, 6.0f);
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.fillEllipse(glowCx - 1.5f, glowCy - 1.5f, 3.0f, 3.0f);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawRoutingArrow — Dibuja una flecha de conexión entre dos puntos
    //  Útil para conectar buses → Master con líneas + punta de flecha
    // ═══════════════════════════════════════════════════════════════════════════
    void MixMapComponent::drawRoutingArrow(
        juce::Graphics& g, juce::Point<float> from, juce::Point<float> to, juce::Colour colour, float alpha) const
    {
        // ─── Main line ──────────────────────────────────────────────────────
        g.setColour(colour.withAlpha(alpha));
        g.drawLine(from.x, from.y, to.x, to.y, 1.2f);

        // ─── Arrow head at destination ─────────────────────────────────────
        float angle      = std::atan2(to.y - from.y, to.x - from.x);
        float arrowLen   = 8.0f;
        float arrowAngle = 0.45f; // ~25 degrees

        juce::Path arrowHead;
        arrowHead.addTriangle(to.x,
                              to.y,
                              to.x - arrowLen * std::cos(angle - arrowAngle),
                              to.y - arrowLen * std::sin(angle - arrowAngle),
                              to.x - arrowLen * std::cos(angle + arrowAngle),
                              to.y - arrowLen * std::sin(angle + arrowAngle));
        g.setColour(colour.withAlpha(alpha * 1.2f));
        g.fillPath(arrowHead);

        // ─── Small glow dot at origin ───────────────────────────────────────
        g.setColour(colour.withAlpha(alpha * 0.6f));
        g.fillEllipse(from.x - 1.5f, from.y - 1.5f, 3.0f, 3.0f);
    }

} // namespace mixcoach
