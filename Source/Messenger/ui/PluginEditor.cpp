#include "../core/PluginProcessor.h"
#include "PluginEditor.h"
#include "../../Common/types/LogHelper.h"

namespace mixcoach {

    // ─── Colores predefinidos para el color picker ───────────────────────────
    static const juce::Colour kPresetColours[8] = {
        juce::Colour(0xFFEF4444), // Rojo    - Bateria
        juce::Colour(0xFFF97316), // Naranja - Percusion
        juce::Colour(0xFFEAB308), // Amarillo - Voz
        juce::Colour(0xFF22C55E), // Verde   - Melodia
        juce::Colour(0xFF3B82F6), // Azul    - Bajo
        juce::Colour(0xFFA78BFA), // Morado  - FX
        juce::Colour(0xFFEC4899), // Rosa    - Vocals
        juce::Colour(0xFF14B8A6), // Teal    - Ambientes
    };

    static const char* kColourNames[8] = {
        "Bateria", "Percusion", "Voz", "Melodia", "Bajo", "FX", "Vocals", "Ambientes"};

    // ═══════════════════════════════════════════════════════════════════════════
    //  ICON DRAWING HELPERS — Line icons minimalistas
    // ═══════════════════════════════════════════════════════════════════════════

    void MessengerAudioProcessorEditor::drawIconCircle(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        // Círculo translúcido con borde tenue
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.fillEllipse(bounds);
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawEllipse(bounds, 0.8f);

        // Inner glow sutil
        auto innerGlow = bounds.reduced(1.0f);
        juce::ColourGradient ig(juce::Colours::white.withAlpha(0.04f),
                                innerGlow.getCentreX(),
                                innerGlow.getCentreY(),
                                juce::Colours::transparentWhite,
                                bounds.getX(),
                                bounds.getY(),
                                true);
        g.setGradientFill(ig);
        g.fillEllipse(innerGlow);
    }

    void
    MessengerAudioProcessorEditor::drawPencilIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour)
    {
        auto b   = bounds.reduced(3.0f);
        float cx = b.getCentreX();
        float cy = b.getCentreY();
        float s  = juce::jmin(b.getWidth(), b.getHeight()) * 0.35f;

        g.setColour(colour.withAlpha(0.8f));
        juce::Path p;
        // Diagonal line (body of pencil)
        p.startNewSubPath(cx - s * 0.6f, cy + s * 0.7f);
        p.lineTo(cx + s * 0.6f, cy - s * 0.7f);
        p.lineTo(cx + s * 0.8f, cy - s * 0.5f);
        p.lineTo(cx - s * 0.4f, cy + s * 0.9f);
        p.closeSubPath();
        g.fillPath(p);

        // Line through the middle
        g.setColour(colour.withAlpha(0.5f));
        g.drawLine(cx - s * 0.4f, cy + s * 0.6f, cx + s * 0.4f, cy - s * 0.6f, 0.8f);
    }

    void MessengerAudioProcessorEditor::drawPaletteIcon(juce::Graphics& g,
                                                        juce::Rectangle<float> bounds,
                                                        juce::Colour colour)
    {
        auto b   = bounds.reduced(3.5f);
        float cx = b.getCentreX();
        float cy = b.getCentreY();
        float r  = juce::jmin(b.getWidth(), b.getHeight()) * 0.4f;

        // Circle body
        g.setColour(colour.withAlpha(0.8f));
        juce::Path p;
        p.addEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.strokePath(p, juce::PathStrokeType(1.2f));

        // Three small dots inside (paint blobs)
        float dotR              = r * 0.18f;
        float dotPositions[][2] = {
            {cx - r * 0.25f, cy - r * 0.3f}, {cx + r * 0.3f, cy - r * 0.15f}, {cx - r * 0.1f, cy + r * 0.35f}};
        for (auto& dp : dotPositions) {
            g.setColour(colour.withAlpha(0.6f));
            g.fillEllipse(dp[0] - dotR, dp[1] - dotR, dotR * 2.0f, dotR * 2.0f);
        }
    }

    void
    MessengerAudioProcessorEditor::drawBoxIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour)
    {
        auto b   = bounds.reduced(3.5f);
        float cx = b.getCentreX();
        float cy = b.getCentreY();
        float s  = juce::jmin(b.getWidth(), b.getHeight()) * 0.35f;

        g.setColour(colour.withAlpha(0.8f));
        // Rounded rect
        juce::Path p;
        p.addRoundedRectangle(cx - s, cy - s * 0.6f, s * 2.0f, s * 1.2f, 1.5f);
        g.strokePath(p, juce::PathStrokeType(1.2f));

        // Horizontal divider line
        g.drawLine(cx - s, cy, cx + s, cy, 0.8f);
        // Vertical divider line
        g.drawLine(cx, cy - s * 0.6f, cx, cy + s * 0.6f, 0.8f);
    }

    void
    MessengerAudioProcessorEditor::drawArrowIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour)
    {
        auto b   = bounds.reduced(3.5f);
        float cx = b.getCentreX();
        float cy = b.getCentreY();
        float s  = juce::jmin(b.getWidth(), b.getHeight()) * 0.35f;

        g.setColour(colour.withAlpha(0.8f));
        // Horizontal line
        g.drawLine(cx - s, cy, cx + s * 0.5f, cy, 1.0f);
        // Arrowhead
        juce::Path arrow;
        arrow.addTriangle(cx + s * 0.8f, cy, cx + s * 0.2f, cy - s * 0.5f, cx + s * 0.2f, cy + s * 0.5f);
        g.fillPath(arrow);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  SPEAKER ICON — Para mute indicator (🔇 when muted, 🔊 when unmuted)
    // ═══════════════════════════════════════════════════════════════════════════
    void MessengerAudioProcessorEditor::drawSpeakerIcon(juce::Graphics& g,
                                                        juce::Rectangle<float> bounds,
                                                        juce::Colour colour,
                                                        bool muted)
    {
        auto b   = bounds.reduced(2.0f);
        float cx = b.getCentreX();
        float cy = b.getCentreY();
        float s  = juce::jmin(b.getWidth(), b.getHeight()) * 0.4f;

        g.setColour(colour);

        // Speaker body (trapezoid)
        juce::Path speaker;
        speaker.startNewSubPath(cx - s * 0.4f, cy - s * 0.5f);
        speaker.lineTo(cx, cy - s * 0.8f);
        speaker.lineTo(cx, cy + s * 0.8f);
        speaker.lineTo(cx - s * 0.4f, cy + s * 0.5f);
        speaker.closeSubPath();
        g.fillPath(speaker);

        // Sound waves (only if not muted)
        if (!muted) {
            float alpha = colour.getAlpha() / 255.0f;
            g.setColour(colour.withAlpha(alpha * 0.6f));
            // First arc
            juce::Path wave1;
            wave1.addArc(cx + s * 0.1f, cy - s * 0.6f, s * 0.5f, s * 1.2f, -0.5f, 0.5f, true);
            g.strokePath(wave1, juce::PathStrokeType(1.0f));
            // Second arc (farther)
            juce::Path wave2;
            wave2.addArc(cx + s * 0.3f, cy - s * 0.8f, s * 0.7f, s * 1.6f, -0.5f, 0.5f, true);
            g.strokePath(wave2, juce::PathStrokeType(0.8f));
        }
        else {
            // X mark over speaker when muted
            g.setColour(colour);
            float xOff = s * 0.25f;
            float yOff = s * 0.25f;
            g.drawLine(cx + xOff - s * 0.3f, cy - yOff - s * 0.3f, cx + xOff + s * 0.3f, cy - yOff + s * 0.3f, 1.5f);
            g.drawLine(cx + xOff + s * 0.3f, cy - yOff - s * 0.3f, cx + xOff - s * 0.3f, cy - yOff + s * 0.3f, 1.5f);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  HEADPHONE ICON — Para solo indicator (🎧 style)
    // ═══════════════════════════════════════════════════════════════════════════
    void MessengerAudioProcessorEditor::drawHeadphoneIcon(juce::Graphics& g,
                                                          juce::Rectangle<float> bounds,
                                                          juce::Colour colour,
                                                          bool soloed)
    {
        auto b   = bounds.reduced(2.5f);
        float cx = b.getCentreX();
        float cy = b.getCentreY();
        float s  = juce::jmin(b.getWidth(), b.getHeight()) * 0.4f;

        g.setColour(colour.withAlpha(0.85f));

        // Headband (arc over top)
        juce::Path headband;
        headband.addArc(cx - s * 0.7f,
                        cy - s * 1.0f,
                        s * 1.4f,
                        s * 1.4f,
                        juce::MathConstants<float>::pi + 0.3f,
                        juce::MathConstants<float>::twoPi - 0.3f,
                        true);
        g.strokePath(headband, juce::PathStrokeType(1.2f));

        // Left earcup
        juce::Path leftEar;
        leftEar.addRoundedRectangle(cx - s * 0.9f, cy - s * 0.3f, s * 0.4f, s * 0.6f, 1.5f);
        g.fillPath(leftEar);

        // Right earcup
        juce::Path rightEar;
        rightEar.addRoundedRectangle(cx + s * 0.5f, cy - s * 0.3f, s * 0.4f, s * 0.6f, 1.5f);
        g.fillPath(rightEar);

        // When soloed: add a small glow dot
        if (soloed) {
            g.setColour(colour.withAlpha(0.4f));
            g.fillEllipse(cx - s * 0.08f, cy - s * 0.45f, s * 0.16f, s * 0.16f);
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  CONSTRUCTOR — Diseño premium con glass panel, título y dropdowns
    // ═══════════════════════════════════════════════════════════════════════════
    MessengerAudioProcessorEditor::MessengerAudioProcessorEditor(MessengerAudioProcessor& processor) :
        AudioProcessorEditor(&processor),
        processorRef_(processor)
    {
        // Copiar colores predefinidos
        for (int i = 0; i < kNumColours; ++i) presetColours_[i] = kPresetColours[i];

        // ─── TÍTULO: "INFORMACIÓN TRACK" (purple neon) ────────────────
        titleLabel_.setText("INFORMACIÓN TRACK", juce::dontSendNotification);
        titleLabel_.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
        titleLabel_.setJustificationType(juce::Justification::centredLeft);
        titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accent()); // #D100FF
        addAndMakeVisible(titleLabel_);

        // ─── NOMBRE ──────────────────────────────────────────────────────
        nameEditor_.setText(processorRef_.getTrackName());
        nameEditor_.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
        nameEditor_.setJustification(juce::Justification::centred);
        nameEditor_.setColour(juce::TextEditor::backgroundColourId, MixCoachTheme::bgInput());
        nameEditor_.setColour(juce::TextEditor::textColourId, MixCoachTheme::textPrimary());
        nameEditor_.setColour(juce::TextEditor::outlineColourId, MixCoachTheme::borderCard().withAlpha(0.6f));
        nameEditor_.setColour(juce::TextEditor::focusedOutlineColourId, MixCoachTheme::accent());
        nameEditor_.setIndents(8, 2);
        nameEditor_.setBorder(juce::BorderSize<int>(1));
        nameEditor_.setInputRestrictions(24);
        nameEditor_.addListener(this);
        addAndMakeVisible(nameEditor_);

        // ─── TIPO (ComboBox) ─────────────────────────────────────────────
        typeComboBox_.setEditableText(false);
        typeComboBox_.setJustificationType(juce::Justification::centredLeft);
        typeComboBox_.addItem("Sin tipo", 1);
        for (int i = 0; i < kNumTrackTypes; ++i) {
            typeComboBox_.addItem(juce::String(kTrackTypeTable[i].name), i + 2);
        }
        typeComboBox_.setSelectedId(trackTypeToComboIndex(processorRef_.getTrackType()) + 1);
        typeComboBox_.setColour(juce::ComboBox::backgroundColourId, MixCoachTheme::bgInput());
        typeComboBox_.setColour(juce::ComboBox::textColourId, MixCoachTheme::textSecondary());
        typeComboBox_.setColour(juce::ComboBox::outlineColourId, MixCoachTheme::borderCard().withAlpha(0.6f));
        typeComboBox_.setColour(juce::ComboBox::arrowColourId, MixCoachTheme::accent());
        typeComboBox_.onChange = [this]() {
            auto selected = typeComboBox_.getSelectedId();
            auto type     = comboIndexToTrackType(selected - 1);
            applyType(type);
        };
        addAndMakeVisible(typeComboBox_);

        // ─── RUTEO (ComboBox) ───────────────────────────────────────────
        busComboBox_.setEditableText(false);
        busComboBox_.setJustificationType(juce::Justification::centredLeft);
        busComboBox_.addItem("Sin ruteo", 1);
        for (int i = 0; i < kNumBuses; ++i) busComboBox_.addItem(juce::String(busNames[i]), i + 2);
        busComboBox_.setSelectedId(static_cast<int>(processorRef_.getBusAssignment()) + 2);
        busComboBox_.setColour(juce::ComboBox::backgroundColourId, MixCoachTheme::bgInput());
        busComboBox_.setColour(juce::ComboBox::textColourId, MixCoachTheme::accent()); // purple for routing value
        busComboBox_.setColour(juce::ComboBox::outlineColourId, MixCoachTheme::borderCard().withAlpha(0.6f));
        busComboBox_.setColour(juce::ComboBox::arrowColourId, MixCoachTheme::accent());
        busComboBox_.onChange = [this]() {
            auto selected = busComboBox_.getSelectedId();
            auto bus      = (selected <= 1) ? BusType::None : static_cast<BusType>(selected - 2);
            applyBus(bus);
        };
        addAndMakeVisible(busComboBox_);

        // ─── Tamaño de ventana premium ──────────────────────────────────
        setSize(310, 220);

        // ─── SmoothValues para animaciones (5 rows ahora: 0-3 + mute/solo)
        for (auto& sv : iconHoverAlpha_) {
            sv.setBallistics(100.0f, 400.0f);
            sv.reset(0.0f);
        }
        colourHoverGlow_.setBallistics(80.0f, 250.0f);
        colourHoverGlow_.reset(0.0f);
        typeHoverGlow_.setBallistics(80.0f, 200.0f);
        typeHoverGlow_.reset(0.0f);
        busHoverGlow_.setBallistics(80.0f, 200.0f);
        busHoverGlow_.reset(0.0f);
        nameFocusGlow_.setBallistics(200.0f, 300.0f);
        nameFocusGlow_.reset(0.0f);
        ledGlow_.setBallistics(60.0f, 120.0f);
        ledGlow_.reset(0.15f);

        // ─── Timer 60 fps para animaciones suaves ────────────────────────
        lastTimerMs_ = juce::Time::getMillisecondCounter();
        startTimerHz(60);

        // ─── Capturar mouse events de hijos para hover tracking ─────────
        addMouseListener(this, true);
    }

    MessengerAudioProcessorEditor::~MessengerAudioProcessorEditor()
    {
        stopTimer();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  LAYOUT — Premium con espaciado amplio y alineación perfecta
    // ═══════════════════════════════════════════════════════════════════════════
    void MessengerAudioProcessorEditor::resized()
    {
        auto area = getLocalBounds().reduced(6, 5);

        // ─── TITLE ROW: "INFORMACION TRACK" (20px, consistente con MixCoach) ────
        auto titleArea = area.removeFromTop(20);
        titleLabel_.setBounds(titleArea.reduced(2, 0));
        titleDividerBounds_ = area.removeFromTop(1);

        // ─── ROW 1: NOMBRE (32px) ───────────────────────────────────────
        area.removeFromTop(3); // spacing (consistente con MixCoach: 3px)
        auto nameRow = area.removeFromTop(28);
        iconRowY_[0] = nameRow.getY();
        // Icon circle (left 22px), label (next 50px), control (rest)
        auto nameIcon = nameRow.removeFromLeft(22).reduced(2, 2);
        juce::ignoreUnused(nameIcon);
        nameRow.removeFromLeft(4);
        auto nameLabel = nameRow.removeFromLeft(48);
        juce::ignoreUnused(nameLabel);
        nameRow.removeFromLeft(4);
        nameEditor_.setBounds(nameRow.reduced(0, 2));
        nameDividerBounds_ = area.removeFromTop(1);

        // ─── ROW 2: COLOR (32px) ────────────────────────────────────────
        area.removeFromTop(3); // spacing
        auto colourRow  = area.removeFromTop(28);
        iconRowY_[1]    = colourRow.getY();
        auto colourIcon = colourRow.removeFromLeft(22).reduced(2, 2);
        juce::ignoreUnused(colourIcon);
        colourRow.removeFromLeft(4);
        auto colourLabel = colourRow.removeFromLeft(48);
        juce::ignoreUnused(colourLabel);
        colourRow.removeFromLeft(4);
        // Color dropdown: circle + arrow
        colourDropdownBounds_ = colourRow.reduced(0, 2).toFloat();
        colourCircleBounds_   = juce::Rectangle<float>(colourDropdownBounds_.getX() + 3.0f,
                                                       colourDropdownBounds_.getY() + 2.0f,
                                                       colourDropdownBounds_.getHeight() - 4.0f,
                                                       colourDropdownBounds_.getHeight() - 4.0f);
        colourDividerBounds_  = area.removeFromTop(1);

        // ─── ROW 3: TIPO (32px) ─────────────────────────────────────────
        area.removeFromTop(3); // spacing
        auto tipoRow  = area.removeFromTop(28);
        iconRowY_[2]  = tipoRow.getY();
        auto tipoIcon = tipoRow.removeFromLeft(22).reduced(2, 2);
        juce::ignoreUnused(tipoIcon);
        tipoRow.removeFromLeft(4);
        auto tipoLabel = tipoRow.removeFromLeft(48);
        juce::ignoreUnused(tipoLabel);
        tipoRow.removeFromLeft(4);
        typeComboBox_.setBounds(tipoRow.reduced(0, 2));
        typeDividerBounds_ = area.removeFromTop(1);

        // ─── ROW 4: RUTEO (32px) ────────────────────────────────────────
        area.removeFromTop(3); // spacing
        auto ruteoRow  = area.removeFromTop(28);
        iconRowY_[3]   = ruteoRow.getY();
        auto ruteoIcon = ruteoRow.removeFromLeft(22).reduced(2, 2);
        juce::ignoreUnused(ruteoIcon);
        ruteoRow.removeFromLeft(4);
        ruteoRow.removeFromLeft(48);
        ruteoRow.removeFromLeft(4);
        busComboBox_.setBounds(ruteoRow.reduced(0, 2));

        // ─── ROW 5: MUTE / SOLO (32px) ──────────────────────────────────
        area.removeFromTop(3);
        estadoDividerBounds_ = area.removeFromTop(1);
        area.removeFromTop(3);
        auto estadoRow  = area.removeFromTop(28);
        iconRowY_[4]    = estadoRow.getY();
        auto estadoIcon = estadoRow.removeFromLeft(22).reduced(2, 2);
        juce::ignoreUnused(estadoIcon);
        estadoRow.removeFromLeft(4);
        estadoRow.removeFromLeft(48); // label space (unused, drawn in paint)
        estadoRow.removeFromLeft(4);

        // Split remaining width: mute (left half) + solo (right half)
        int halfW          = estadoRow.getWidth() / 2;
        muteButtonBounds_  = estadoRow.removeFromLeft(halfW).reduced(2, 2);
        muteDividerBounds_ = estadoRow.removeFromLeft(12); // vertical divider gap
        soloButtonBounds_  = estadoRow.reduced(2, 2);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  PAINT — Glass panel + título neon + iconos + divisores + color dropdown
    // ═══════════════════════════════════════════════════════════════════════════
    void MessengerAudioProcessorEditor::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds();
        auto area   = bounds.reduced(6, 5);

        // ─── Fondo: glass panel premium (consistente con MixCoachTheme) ──
        g.fillAll(MixCoachTheme::bgCanvas());

        // Glass panel using theme
        auto glassBounds = bounds.toFloat().reduced(1.0f);
        MixCoachTheme::fillGlassPanel(g, glassBounds, 6.0f);

        // Accent cyan outer glow (Messenger identity)
        g.setColour(MixCoachTheme::accentCyan().withAlpha(0.04f));
        g.drawRoundedRectangle(glassBounds.expanded(2.0f), 8.0f, 1.5f);

        // ─── Borde lateral de color (3px, color de pista) ───────────────
        auto trackCol = processorRef_.getTrackColour();
        g.setColour(trackCol.withAlpha(0.5f));
        g.fillRoundedRectangle(0.0f, 4.0f, 3.0f, (float)bounds.getHeight() - 8, 1.5f);

        // ─── LED de heartbeat (smooth pulse via SmoothValue) ────────────
        auto titleArea = area.removeFromTop(20);
        auto ledArea   = titleArea.removeFromRight(14).reduced(4, 6);
        float ledA     = ledGlow_.getCurrent();
        g.setColour(MixCoachTheme::success().withAlpha(ledA));
        g.fillEllipse(ledArea.toFloat());
        g.setColour(juce::Colours::white.withAlpha(0.06f + 0.12f * ledA));
        g.drawEllipse(ledArea.toFloat(), 0.5f);

        // ─── TÍTULO ya lo dibuja el Label, solo añadimos glow ────────────
        // El título "INFORMACION TRACK" es manejado por titleLabel_

        // ─── DIVISORES entre filas ───────────────────────────────────────
        auto drawDivider = [&](juce::Rectangle<int> divBounds) {
            g.setColour(MixCoachTheme::divider().withAlpha(0.25f));
            g.drawHorizontalLine(divBounds.getY(), (float)divBounds.getX() + 4, (float)divBounds.getRight() - 4);
        };
        drawDivider(titleDividerBounds_);
        drawDivider(nameDividerBounds_);
        drawDivider(colourDividerBounds_);
        drawDivider(typeDividerBounds_);
        drawDivider(estadoDividerBounds_);

        // ─── ICONOS + LABELS para cada fila ─────────────────────────────
        // NOMBRE (row 1)
        {
            int y                = titleDividerBounds_.getBottom() + 4;
            auto iconBounds      = juce::Rectangle<float>(12.0f, (float)y + 5.0f, 18.0f, 18.0f);
            float hA             = iconHoverAlpha_[0].getCurrent();
            juce::Colour iconCol = juce::Colour(0xFFAAB4C0).interpolatedWith(MixCoachTheme::accent(), hA);
            drawIconCircle(g, iconBounds);
            drawPencilIcon(g, iconBounds, iconCol.withAlpha(0.65f + 0.35f * hA));

            g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
            g.setColour(MixCoachTheme::textMuted().interpolatedWith(MixCoachTheme::accent(), hA * 0.5f));
            g.drawText("NOMBRE",
                       juce::Rectangle<float>(34.0f, (float)y + 2.0f, 46.0f, 24.0f),
                       juce::Justification::centredLeft);
        }

        // COLOR (row 2)
        {
            int y                = nameDividerBounds_.getBottom() + 4;
            auto iconBounds      = juce::Rectangle<float>(12.0f, (float)y + 5.0f, 18.0f, 18.0f);
            float hA             = iconHoverAlpha_[1].getCurrent();
            juce::Colour iconCol = juce::Colour(0xFFAAB4C0).interpolatedWith(MixCoachTheme::accent(), hA);
            drawIconCircle(g, iconBounds);
            drawPaletteIcon(g, iconBounds, iconCol.withAlpha(0.65f + 0.35f * hA));

            g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
            g.setColour(MixCoachTheme::textMuted().interpolatedWith(MixCoachTheme::accent(), hA * 0.5f));
            g.drawText("COLOR",
                       juce::Rectangle<float>(34.0f, (float)y + 2.0f, 46.0f, 24.0f),
                       juce::Justification::centredLeft);

            // Color dropdown: preview circle + arrow ▾ with hover animation
            auto trackColour = processorRef_.getTrackColour();
            float cHover     = colourHoverGlow_.getCurrent();

            // Animated glow behind circle (expands and brightens on hover)
            auto glowBounds = colourCircleBounds_.expanded(3.0f + 6.0f * cHover);
            g.setColour(trackColour.withAlpha(0.06f + 0.18f * cHover));
            g.fillEllipse(glowBounds);

            // Preview circle
            g.setColour(trackColour);
            g.fillEllipse(colourCircleBounds_);
            g.setColour(juce::Colours::white.withAlpha(0.2f + 0.3f * cHover));
            g.drawEllipse(colourCircleBounds_, 1.0f);

            // Dropdown arrow ▾
            auto arrowArea = juce::Rectangle<float>(colourDropdownBounds_.getRight() - 20.0f,
                                                    colourDropdownBounds_.getY(),
                                                    20.0f,
                                                    colourDropdownBounds_.getHeight());
            g.setColour(juce::Colours::white.withAlpha(0.25f + 0.25f * cHover));
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.drawText(juce::CharPointer_UTF8("\xE2\x96\xBE"), arrowArea, juce::Justification::centred);

            // Border around dropdown area (animated on hover)
            g.setColour(juce::Colour(0xFF25263A)
                            .interpolatedWith(MixCoachTheme::accentCyan(), cHover * 0.4f)
                            .withAlpha(0.5f + 0.3f * cHover));
            g.drawRoundedRectangle(colourDropdownBounds_, 4.0f, 1.0f);
        }

        // TIPO (row 3)
        {
            int y                = colourDividerBounds_.getBottom() + 4;
            auto iconBounds      = juce::Rectangle<float>(12.0f, (float)y + 5.0f, 18.0f, 18.0f);
            float hA             = iconHoverAlpha_[2].getCurrent();
            juce::Colour iconCol = juce::Colour(0xFFAAB4C0).interpolatedWith(MixCoachTheme::accent(), hA);
            drawIconCircle(g, iconBounds);
            drawBoxIcon(g, iconBounds, iconCol.withAlpha(0.65f + 0.35f * hA));

            g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
            g.setColour(MixCoachTheme::textMuted().interpolatedWith(MixCoachTheme::accent(), hA * 0.5f));
            g.drawText(
                "TIPO", juce::Rectangle<float>(34.0f, (float)y + 2.0f, 46.0f, 24.0f), juce::Justification::centredLeft);

            // ═══ Auto-inferred badge: "AI" badge when type was set by MixCoach ═══
            // Visible solo cuando el TrackType fue inferido automaticamente
            // (NO seleccionado manualmente por el usuario via ComboBox).
            // isTrackTypePinned() = true -> manual, false -> auto-inferido.
            bool isTypeAuto = !processorRef_.isTrackTypePinned() && processorRef_.getTrackType() != TrackType::None;
            if (isTypeAuto) {
                auto badgeBounds = juce::Rectangle<float>(62.0f, (float)y + 4.0f, 18.0f, 16.0f);
                g.setColour(MixCoachTheme::accentCyan().withAlpha(0.15f));
                g.fillRoundedRectangle(badgeBounds, 3.0f);
                g.setColour(MixCoachTheme::accentCyan().withAlpha(0.85f));
                g.setFont(juce::Font(juce::FontOptions(6.5f)).boldened());
                g.drawText("AI", badgeBounds, juce::Justification::centred);
            }

            // Combo hover glow
            float tGlow = typeHoverGlow_.getCurrent();
            if (tGlow > 0.01f) {
                auto comboGlow = typeComboBox_.getBounds().toFloat().expanded(3.0f * tGlow);
                g.setColour(MixCoachTheme::accent().withAlpha(0.05f * tGlow));
                g.fillRoundedRectangle(comboGlow, 5.0f);
            }
        }

        // RUTEO (row 4)
        {
            int y                = typeDividerBounds_.getBottom() + 4;
            auto iconBounds      = juce::Rectangle<float>(12.0f, (float)y + 5.0f, 18.0f, 18.0f);
            float hA             = iconHoverAlpha_[3].getCurrent();
            juce::Colour iconCol = MixCoachTheme::accent().brighter(hA * 0.3f);
            drawIconCircle(g, iconBounds);
            drawArrowIcon(g, iconBounds, iconCol.withAlpha(0.75f + 0.25f * hA));

            g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
            g.setColour(MixCoachTheme::textMuted().interpolatedWith(MixCoachTheme::accent(), hA * 0.6f));
            g.drawText("RUTEO",
                       juce::Rectangle<float>(34.0f, (float)y + 2.0f, 46.0f, 24.0f),
                       juce::Justification::centredLeft);

            // Combo hover glow
            float bGlow = busHoverGlow_.getCurrent();
            if (bGlow > 0.01f) {
                auto comboGlow = busComboBox_.getBounds().toFloat().expanded(3.0f * bGlow);
                g.setColour(MixCoachTheme::accent().withAlpha(0.05f * bGlow));
                g.fillRoundedRectangle(comboGlow, 5.0f);
            }
        }

        // ─── MUTE / SOLO (row 5) ────────────────────────────────────────
        {
            int y                = estadoDividerBounds_.getBottom() + 4;
            auto iconBounds      = juce::Rectangle<float>(12.0f, (float)y + 5.0f, 18.0f, 18.0f);
            float hA             = iconHoverAlpha_[4].getCurrent();
            juce::Colour iconCol = juce::Colour(0xFFAAB4C0).interpolatedWith(MixCoachTheme::accent(), hA);
            drawIconCircle(g, iconBounds);

            g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
            g.setColour(MixCoachTheme::textMuted().interpolatedWith(MixCoachTheme::accent(), hA * 0.5f));
            g.drawText("ESTADO",
                       juce::Rectangle<float>(34.0f, (float)y + 2.0f, 46.0f, 24.0f),
                       juce::Justification::centredLeft);

            bool muted  = processorRef_.isMuted();
            bool soloed = processorRef_.isSoloed();

            // ─── MUTE button ─────────────────────────────────────────────
            {
                auto mb    = muteButtonBounds_.toFloat();
                bool hover = (hoveredEstado_ == 0);

                // Background: red when muted, dark gray when not
                juce::Colour bgCol = muted ? juce::Colour(0xFFDC2626).withAlpha(0.25f)
                                           : MixCoachTheme::bgInput().brighter(0.05f);
                if (hover) bgCol = bgCol.brighter(0.15f);
                g.setColour(bgCol);
                g.fillRoundedRectangle(mb, 4.0f);

                // Border
                juce::Colour borderCol = muted ? juce::Colour(0xFFDC2626).withAlpha(0.6f)
                                               : MixCoachTheme::borderCard().withAlpha(0.3f);
                if (hover) borderCol = borderCol.brighter(0.3f);
                g.setColour(borderCol);
                g.drawRoundedRectangle(mb, 4.0f, 1.0f);

                // Speaker icon
                float iconSize = mb.getHeight() * 0.5f;
                auto iconRect =
                    juce::Rectangle<float>(mb.getX() + 8.0f, mb.getCentreY() - iconSize / 2.0f, iconSize, iconSize);
                juce::Colour speakerCol = muted ? juce::Colour(0xFFFF6B6B) : juce::Colour(0xFFAAB4C0);
                drawSpeakerIcon(g, iconRect, speakerCol, muted);

                // Text label
                g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
                g.setColour(muted ? juce::Colour(0xFFFF6B6B) : MixCoachTheme::textMuted());
                g.drawText(muted ? "SILENCIADO" : "MUTE", mb.translated(0, 0), juce::Justification::centred);
            }

            // ─── Vertical divider ────────────────────────────────────────
            {
                g.setColour(MixCoachTheme::divider().withAlpha(0.15f));
                g.drawVerticalLine((float)muteDividerBounds_.getCentreX(),
                                   (float)estadoDividerBounds_.getBottom() + 6.0f,
                                   (float)(iconRowY_[4] + 26));
            }

            // ─── SOLO button ─────────────────────────────────────────────
            {
                auto sb    = soloButtonBounds_.toFloat();
                bool hover = (hoveredEstado_ == 1);

                // Background: amber when soloed, dark gray when not
                juce::Colour bgCol = soloed ? juce::Colour(0xFFF59E0B).withAlpha(0.25f)
                                            : MixCoachTheme::bgInput().brighter(0.05f);
                if (hover) bgCol = bgCol.brighter(0.15f);
                g.setColour(bgCol);
                g.fillRoundedRectangle(sb, 4.0f);

                // Border
                juce::Colour borderCol = soloed ? juce::Colour(0xFFF59E0B).withAlpha(0.6f)
                                                : MixCoachTheme::borderCard().withAlpha(0.3f);
                if (hover) borderCol = borderCol.brighter(0.3f);
                g.setColour(borderCol);
                g.drawRoundedRectangle(sb, 4.0f, 1.0f);

                // Headphone icon
                float iconSize = sb.getHeight() * 0.5f;
                auto iconRect =
                    juce::Rectangle<float>(sb.getX() + 8.0f, sb.getCentreY() - iconSize / 2.0f, iconSize, iconSize);
                juce::Colour hpCol = soloed ? juce::Colour(0xFFFCD34D) : juce::Colour(0xFFAAB4C0);
                drawHeadphoneIcon(g, iconRect, hpCol, soloed);

                // Text label
                g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
                g.setColour(soloed ? juce::Colour(0xFFFCD34D) : MixCoachTheme::textMuted());
                g.drawText(soloed ? "EN SOLO" : "SOLO", sb.translated(0, 0), juce::Justification::centred);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  COLOR DROPDOWN — PopupMenu con opciones de color
    // ═══════════════════════════════════════════════════════════════════════════
    void MessengerAudioProcessorEditor::showColourPopup()
    {
        juce::PopupMenu menu;

        for (int i = 0; i < kNumColours; ++i) {
            juce::Colour c    = presetColours_[i];
            juce::String name = juce::String(kColourNames[i]);

            // Añadir indicador visual para el color seleccionado
            bool isSelected     = (i == selectedColourIndex_);
            juce::String prefix = isSelected ? juce::CharPointer_UTF8("\xE2\x97\x89 ")  // ◉
                                             : juce::CharPointer_UTF8("\xE2\x97\x8B "); // ○
            menu.addColouredItem(i + 1, prefix + name, c, true, isSelected);
        }

        auto options = juce::PopupMenu::Options()
                           .withTargetComponent(this)
                           .withMinimumWidth((int)colourDropdownBounds_.getWidth())
                           .withMaximumNumColumns(1);

        menu.showMenuAsync(options, [this](int result) {
            if (result > 0) applyColour(result - 1);
        });
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  TIMER — 60 fps: animaciones suaves + LED heartbeat pulsing
    // ═══════════════════════════════════════════════════════════════════════════
    void MessengerAudioProcessorEditor::timerCallback()
    {
        auto nowMs = juce::Time::getMillisecondCounter();
        float dt   = (float)(nowMs - lastTimerMs_);
        if (dt > 100.0f) dt = 16.0f; // clamp after pause/resume
        lastTimerMs_ = nowMs;

        bool needsRepaint = false; // ─── Icon hover animations (5 rows)
        for (int i = 0; i < 5; ++i) {
            if (iconHoverAlpha_[i].advance()) needsRepaint = true;
        }

        // ═══ Mute/Solo state sync — repaint when state changes
        {
            bool currentMuted  = processorRef_.isMuted();
            bool currentSoloed = processorRef_.isSoloed();
            if (currentMuted != lastMutedState_ || currentSoloed != lastSoloedState_) {
                lastMutedState_  = currentMuted;
                lastSoloedState_ = currentSoloed;
                needsRepaint     = true;
            }
        }

        // ─── Colour dropdown hover glow ──────────────────────────────────
        if (colourHoverGlow_.advance()) needsRepaint = true;

        // ─── TIPO ComboBox hover glow ────────────────────────────────────
        if (typeHoverGlow_.advance()) {
            float glow      = typeHoverGlow_.getCurrent();
            auto outlineCol = MixCoachTheme::borderCard().interpolatedWith(MixCoachTheme::accent(), glow * 0.5f);
            typeComboBox_.setColour(juce::ComboBox::outlineColourId, outlineCol.withAlpha(0.6f + 0.4f * glow));
            typeComboBox_.setColour(juce::ComboBox::backgroundColourId,
                                    MixCoachTheme::bgInput().brighter(glow * 0.05f));
            needsRepaint = true;
        }

        // ─── RUTEO ComboBox hover glow ───────────────────────────────────
        if (busHoverGlow_.advance()) {
            float glow      = busHoverGlow_.getCurrent();
            auto outlineCol = MixCoachTheme::borderCard().interpolatedWith(MixCoachTheme::accent(), glow * 0.5f);
            busComboBox_.setColour(juce::ComboBox::outlineColourId, outlineCol.withAlpha(0.6f + 0.4f * glow));
            busComboBox_.setColour(juce::ComboBox::backgroundColourId, MixCoachTheme::bgInput().brighter(glow * 0.05f));
            needsRepaint = true;
        }

        // ─── Name editor focus glow (check keyboard focus) ───────────────
        float targetFocus = nameEditor_.hasKeyboardFocus(true) ? 1.0f : 0.0f;
        nameFocusGlow_.setTargetValue(targetFocus);
        if (nameFocusGlow_.advance()) {
            // Apply smooth focus color to the editor
            float focusAmt = nameFocusGlow_.getCurrent();
            auto focusCol  = MixCoachTheme::accent().withAlpha(0.8f * focusAmt + 0.2f);
            nameEditor_.setColour(juce::TextEditor::focusedOutlineColourId, focusCol);
            needsRepaint = true;
        }

        // ─── LED smooth pulse (~2Hz sine wave) ───────────────────────────
        auto lastHb    = processorRef_.getLastHeartbeatMs();
        bool connected = (lastHb > 0 && (nowMs - lastHb < 2000));
        if (connected) {
            ledPhase_ += dt * 0.002f; // ~2Hz
            if (ledPhase_ > 1.0f) ledPhase_ -= 1.0f;
            float sine        = std::sin(ledPhase_ * juce::MathConstants<float>::twoPi);
            float targetAlpha = 0.25f + 0.55f * (sine * 0.5f + 0.5f); // 0.25 ~ 0.80
            ledGlow_.setTargetValue(targetAlpha);
        }
        else {
            ledGlow_.setTargetValue(0.08f); // dim gray when disconnected
        }
        if (ledGlow_.advance()) needsRepaint = true;

        // ═══ Feedback Loop V9 + Name Auto-Fill V11 ─────────────────────
        // Throttle: ~1s para no saturar shared memory.
        // MixCoach escribe TrackType inferido -> Messenger lee y:
        //   1. Actualiza TrackType + ComboBox (V9)
        //   2. Auto-llena nombre si es "Pista X" (V11)
        if (nowMs - lastTrackTypeSyncMs_ > 1000) {
            lastTrackTypeSyncMs_ = nowMs;
            if (processorRef_.syncTrackTypeFromSharedMemory()) {
                // Actualizar ComboBox al nuevo tipo inferido por MixCoach
                auto savedOnChange     = typeComboBox_.onChange;
                typeComboBox_.onChange = nullptr;
                typeComboBox_.setSelectedId(trackTypeToComboIndex(processorRef_.getTrackType()) + 1);
                typeComboBox_.onChange = savedOnChange;

                // ═══ V11: Actualizar TextEditor si el nombre fue auto-llenado ─
                // syncTrackTypeFromSharedMemory() llama a autoFillNameFromTrackType()
                // que actualiza trackName_ internamente.
                // Aqui sincronizamos el TextEditor para que refleje el cambio.
                auto currentName = processorRef_.getTrackName();
                if (nameEditor_.getText() != currentName) {
                    nameEditor_.setText(currentName, juce::dontSendNotification);
                }

                needsRepaint = true;
            }
        }

        if (needsRepaint) repaint();
    }

    // ─── TextEditor callback (nombre) ─────────────────────────────────────────
    void MessengerAudioProcessorEditor::textEditorTextChanged(juce::TextEditor& editor)
    {
        if (&editor == &nameEditor_) {
            auto newName = editor.getText().trim();
            if (newName.isNotEmpty()) {
                processorRef_.setTrackName(newName);

                // ═══ Name Auto-Suggestion V10: sugerir tipo segun el nombre ───
                // Solo si el usuario NO ha seleccionado manualmente un tipo.
                // Usa palabras clave en el nombre (Kick, Snare, Voz, etc.)
                // para auto-seleccionar el TrackType en el ComboBox.
                if (!processorRef_.isTrackTypePinned()) {
                    TrackType suggested = processorRef_.suggestTrackTypeFromName(newName);
                    if (suggested != TrackType::None) {
                        processorRef_.setTrackTypeAutoSuggested(suggested);
                        // Suprimir onChange para evitar que setSelectedId() dispare
                        // applyType() -> setTrackType() -> trackTypePinned_=true
                        auto savedOnChange     = typeComboBox_.onChange;
                        typeComboBox_.onChange = nullptr;
                        typeComboBox_.setSelectedId(trackTypeToComboIndex(suggested) + 1);
                        typeComboBox_.onChange = savedOnChange;
                    }
                }
            }
        }
    }

    // ─── Acciones ──────────────────────────────────────────────────────────────

    void MessengerAudioProcessorEditor::applyType(TrackType type)
    {
        processorRef_.setTrackType(type);

        if (type != TrackType::None) {
            auto suggestedBus = getTrackTypeBus(type);
            if (suggestedBus != BusType::None) {
                busComboBox_.setSelectedId(static_cast<int>(suggestedBus) + 2);
            }
        }
        repaint();
    }

    void MessengerAudioProcessorEditor::applyColour(int colourIndex)
    {
        if (colourIndex >= 0 && colourIndex < kNumColours) {
            processorRef_.setTrackColour(presetColours_[colourIndex]);
            selectedColourIndex_ = colourIndex;
            repaint();
        }
    }

    void MessengerAudioProcessorEditor::applyBus(BusType bus)
    {
        processorRef_.setBusAssignment(bus);

        if (processorRef_.getTrackType() == TrackType::None) {
            for (int i = 0; i < kNumTrackTypes; ++i) {
                if (kTrackTypeTable[i].suggestedBus == bus) {
                    TrackType suggestedType = kTrackTypeTable[i].type;
                    typeComboBox_.setSelectedId(trackTypeToComboIndex(suggestedType) + 1);
                    processorRef_.setTrackType(suggestedType);
                    break;
                }
            }
        }
        repaint();
    }

    // ─── Mouse down: detectar clicks en color dropdown y mute/solo ──────────
    void MessengerAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
    {
        auto pos = e.getPosition();

        // ─── Color dropdown hit test ─────────────────────────────────────
        if (colourDropdownBounds_.contains(pos.toFloat())) {
            showColourPopup();
            return;
        }

        // ─── MUTE button click ───────────────────────────────────────────
        if (muteButtonBounds_.contains(pos)) {
            processorRef_.setMuted(!processorRef_.isMuted());
            repaint();
            return;
        }

        // ─── SOLO button click ───────────────────────────────────────────
        if (soloButtonBounds_.contains(pos)) {
            processorRef_.setSoloed(!processorRef_.isSoloed());
            repaint();
            return;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  MOUSE TRACKING — Hover detection para animaciones
    // ═══════════════════════════════════════════════════════════════════════════
    void MessengerAudioProcessorEditor::mouseMove(const juce::MouseEvent& e)
    {
        // Transform coordinates to be relative to THIS component (not child)
        auto pos     = e.getEventRelativeTo(this).getPosition();
        int newHover = -1;
        int my       = pos.getY();

        // ─── Detectar qué fila está hovereada (icon area: left ~74px) ────
        for (int i = 0; i < 5; ++i) {
            if (iconRowY_[i] > 0 && my >= iconRowY_[i] - 2 && my < iconRowY_[i] + 28) {
                newHover = i;
                break;
            }
        }

        // ─── Detectar hover sobre mute/solo buttons ─────────────────────
        int estadoHover = -1;
        if (muteButtonBounds_.contains(pos)) {
            estadoHover = 0;
            newHover    = 4;
        }
        else if (soloButtonBounds_.contains(pos)) {
            estadoHover = 1;
            newHover    = 4;
        }
        if (estadoHover != hoveredEstado_) {
            hoveredEstado_ = estadoHover;
            repaint();
        }

        // ─── Detectar hover sobre colour dropdown area ───────────────────
        if (colourDropdownBounds_.contains(pos.toFloat())) {
            newHover = 1; // Colour dropdown is part of row 1
            colourHoverGlow_.setTargetValue(1.0f);
        }
        else {
            colourHoverGlow_.setTargetValue(0.0f);
        }

        if (newHover != hoveredRow_) {
            hoveredRow_ = newHover;
            for (int i = 0; i < 5; ++i) iconHoverAlpha_[i].setTargetValue(i == hoveredRow_ ? 1.0f : 0.0f);
        }

        // ─── Detectar hover sobre ComboBoxes (TIPO y RUTEO) ────────────
        bool overType     = typeComboBox_.getBounds().contains(pos);
        bool overBus      = busComboBox_.getBounds().contains(pos);
        int newComboHover = overType ? 0 : (overBus ? 1 : -1);
        if (newComboHover != hoveredCombo_) {
            hoveredCombo_ = newComboHover;
            typeHoverGlow_.setTargetValue(overType ? 1.0f : 0.0f);
            busHoverGlow_.setTargetValue(overBus ? 1.0f : 0.0f);
        }
    }

    void MessengerAudioProcessorEditor::mouseExit(const juce::MouseEvent& e)
    {
        juce::ignoreUnused(e);
        hoveredRow_ = -1;
        for (int i = 0; i < 5; ++i) iconHoverAlpha_[i].setTargetValue(0.0f);
        colourHoverGlow_.setTargetValue(0.0f);
        hoveredCombo_  = -1;
        hoveredEstado_ = -1;
        typeHoverGlow_.setTargetValue(0.0f);
        busHoverGlow_.setTargetValue(0.0f);
    }

} // namespace mixcoach
