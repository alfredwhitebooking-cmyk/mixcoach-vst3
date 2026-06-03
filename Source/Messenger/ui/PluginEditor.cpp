#include "PluginEditor.h"
#include "StereoMeter.h"
#include "WaveformView.h"
#include "ColourPresetStrip.h"
#include "ColourSwatch.h"
#include "../core/PluginProcessor.h"
#include "../../Common/memory/SharedData.h"
#include "../../Common/types/LogHelper.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  MessengerAudioProcessorEditor Implementation
// ═══════════════════════════════════════════════════════════════════════════

MessengerAudioProcessorEditor::MessengerAudioProcessorEditor(MessengerAudioProcessor& processor)
    : AudioProcessorEditor(&processor)
    , processorRef_(processor)
{
    setResizable(false, false);

    // ─── Componentes personalizados ─────────────────────────────────────

    colourSwatch_   = std::make_unique<ColourSwatch>();

    // 2 StereoMeters: Input (L+R) + Output (L+R)
    stereoInput_  = std::make_unique<StereoMeter>();
    stereoOutput_ = std::make_unique<StereoMeter>();

    // Colores de canal: L=azul, R=verde (estilo profesional)
    stereoInput_->setBarColours(
        juce::Colour(0xFF60A5FA),  // L = azul
        juce::Colour(0xFF34D399)); // R = verde
    stereoOutput_->setBarColours(
        juce::Colour(0xFF60A5FA),  // L = azul
        juce::Colour(0xFF34D399)); // R = verde

    // Circular GR Gauge
    grGauge_ = std::make_unique<CircularGauge>();
    grGauge_->setTitle("REDUCCION DE GANANCIA");

    // ═══════════════════════════════════════════════════════════════════════
    //  INFO PANEL — Form-style rows
    // ═══════════════════════════════════════════════════════════════════════

    // ─── Row 1: NOMBRE ─────────────────────────────────────────────────
    // Icono dibujado en paint() via drawRowIcon()

    nameEditor_.setText(processorRef_.getTrackName());
    nameEditor_.setFont(juce::Font(juce::FontOptions(13.0f)).boldened());
    nameEditor_.setJustification(juce::Justification::centredLeft);
    nameEditor_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF0A0A14));
    nameEditor_.setColour(juce::TextEditor::textColourId, juce::Colour(0xFFF1F1F6));
    nameEditor_.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF2C2C3E));
    nameEditor_.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xFF8B5CF6));
    nameEditor_.setIndents(6, 2);
    nameEditor_.setBorder(juce::BorderSize<int>(1));
    nameEditor_.setInputRestrictions(32);
    nameEditor_.addListener(this);
    addAndMakeVisible(nameEditor_);

    currentColour_ = processorRef_.getTrackColour();
    colourSwatch_->setColour(currentColour_);
    colourSwatch_->onClick = [this]() { openColourSelector(); };
    addAndMakeVisible(colourSwatch_.get());

    // ─── Row 2: GRUPO ──────────────────────────────────────────────────
    busComboBox_.setEditableText(false);
    busComboBox_.setJustificationType(juce::Justification::centredLeft);
    busComboBox_.addItem("Sin ruteo", 1);
    for (int i = 0; i < kNumBuses; ++i)
        busComboBox_.addItem(busNames[i], i + 2);
    busComboBox_.setSelectedId(static_cast<int>(processorRef_.getBusAssignment()) + 2);
    busComboBox_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF0A0A14));
    busComboBox_.setColour(juce::ComboBox::textColourId, juce::Colour(0xFFE0E0E0));
    busComboBox_.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF2C2C3E));
    busComboBox_.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xFF8B5CF6));
    busComboBox_.onChange = [this]() {
        auto selected = busComboBox_.getSelectedId();
        auto bus = (selected <= 1) ? BusType::None : static_cast<BusType>(selected - 2);
        processorRef_.setBusAssignment(bus);

        // Actualizar TIPO y PRIORIDAD segun el bus
        tipoValueLabel_.setText(getTipoForBus(bus), juce::dontSendNotification);
        prioridadValueLabel_.setText(getPrioridadForBus(bus), juce::dontSendNotification);

        // Actualizar bus value display
        auto busName = (bus != BusType::None)
            ? juce::String(busNames[static_cast<int>(bus)])
            : "Sin ruteo";
        auto busCol = (bus != BusType::None)
            ? getBusColour(static_cast<int>(bus))
            : juce::Colour(0xFF888888);
        busValueLabel_.setText(busName, juce::dontSendNotification);
        busValueLabel_.setColour(juce::Label::textColourId, busCol);
    };
    addAndMakeVisible(busComboBox_);

    busValueLabel_.setText("Sin ruteo", juce::dontSendNotification);
    busValueLabel_.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
    busValueLabel_.setJustificationType(juce::Justification::centredLeft);
    busValueLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
    addAndMakeVisible(busValueLabel_);

    // ─── Row 3: COLOR (preset strip) ────────────────────────────────────
    colourPresetStrip_ = std::make_unique<ColourPresetStrip>();
    colourPresetStripRaw_ = static_cast<ColourPresetStrip*>(colourPresetStrip_.get());
    colourPresetStripRaw_->setActiveColour(currentColour_);
    colourPresetStripRaw_->onColourChosen = [this](juce::Colour col) { applyPresetColour(col); };
    addAndMakeVisible(colourPresetStrip_.get());

    colourListener_.onColourChanged = [this](juce::Colour newColour) {
        currentColour_ = newColour;
        colourSwatch_->setColour(newColour);
        if (colourPresetStripRaw_)
            colourPresetStripRaw_->setActiveColour(newColour);
        processorRef_.setTrackColour(newColour);
        repaint();
    };

    // ─── Row 4: TIPO ────────────────────────────────────────────────────
    auto initialBus = processorRef_.getBusAssignment();
    tipoValueLabel_.setText(getTipoForBus(initialBus), juce::dontSendNotification);
    tipoValueLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
    tipoValueLabel_.setJustificationType(juce::Justification::centredLeft);
    tipoValueLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFFB0B0C0));
    addAndMakeVisible(tipoValueLabel_);

    // ─── Row 5: PRIORIDAD ───────────────────────────────────────────────
    prioridadValueLabel_.setText(getPrioridadForBus(initialBus), juce::dontSendNotification);
    prioridadValueLabel_.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
    prioridadValueLabel_.setJustificationType(juce::Justification::centredLeft);
    prioridadValueLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFFF1F1F6));
    addAndMakeVisible(prioridadValueLabel_);

    // ─── Row 6: NOTAS ───────────────────────────────────────────────────
    notasEditor_.setText("\u2014", juce::dontSendNotification);
    notasEditor_.setFont(juce::Font(juce::FontOptions(11.0f)));
    notasEditor_.setJustification(juce::Justification::centredLeft);
    notasEditor_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF0A0A14));
    notasEditor_.setColour(juce::TextEditor::textColourId, juce::Colour(0xFF888888));
    notasEditor_.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF1A1A2E));
    notasEditor_.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xFF8B5CF6));
    notasEditor_.setIndents(6, 2);
    notasEditor_.setBorder(juce::BorderSize<int>(1));
    notasEditor_.setInputRestrictions(64);
    notasEditor_.addListener(this);
    addAndMakeVisible(notasEditor_);

    // ─── Row 7: MUTE ──────────────────────────────────────────────────
    muteButton_.setButtonText("MUTE");
    muteButton_.setClickingTogglesState(true);
    muteButton_.setToggleState(processorRef_.isMuted(), juce::dontSendNotification);
    muteButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2D1B1B));
    muteButton_.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFEF4444));
    muteButton_.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFF6B7280));
    muteButton_.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    muteButton_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    muteButton_.onClick = [this]() {
        processorRef_.setMuted(muteButton_.getToggleState());
        updateMuteDisplay();
    };
    addAndMakeVisible(muteButton_);

    // ═══════════════════════════════════════════════════════════════════════
    //  STATUS (compacto, integrado en info rows via paint)
    // ═══════════════════════════════════════════════════════════════════════
    statusLabel_.setText("\u25CF Conectando...", juce::dontSendNotification);
    statusLabel_.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
    statusLabel_.setJustificationType(juce::Justification::centredRight);
    statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFFAAAAAA));
    addAndMakeVisible(statusLabel_);

    // ═══════════════════════════════════════════════════════════════════════
    //  NIVELES PANEL — 3-column meters
    // ═══════════════════════════════════════════════════════════════════════

    // Column headers
    inputLabel_.setText("INPUT", juce::dontSendNotification);
    inputLabel_.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
    inputLabel_.setJustificationType(juce::Justification::centred);
    inputLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF6B7280));
    addAndMakeVisible(inputLabel_);

    grLabel_.setText("REDUCCION DE GANANCIA", juce::dontSendNotification);
    grLabel_.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
    grLabel_.setJustificationType(juce::Justification::centred);
    grLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF8B5CF6));
    addAndMakeVisible(grLabel_);

    outputLabel_.setText("OUTPUT", juce::dontSendNotification);
    outputLabel_.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
    outputLabel_.setJustificationType(juce::Justification::centred);
    outputLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF6B7280));
    addAndMakeVisible(outputLabel_);

    // Value labels (grandes, blancos)
    inputValueLabel_.setText("--.- dB", juce::dontSendNotification);
    inputValueLabel_.setFont(juce::Font(juce::FontOptions(13.0f)).boldened());
    inputValueLabel_.setJustificationType(juce::Justification::centred);
    inputValueLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFFF1F1F6));
    addAndMakeVisible(inputValueLabel_);

    grValueLabel_.setText("0.0 dB", juce::dontSendNotification);
    grValueLabel_.setFont(juce::Font(juce::FontOptions(13.0f)).boldened());
    grValueLabel_.setJustificationType(juce::Justification::centred);
    grValueLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF8B5CF6));
    addAndMakeVisible(grValueLabel_);

    outputValueLabel_.setText("--.- dB", juce::dontSendNotification);
    outputValueLabel_.setFont(juce::Font(juce::FontOptions(13.0f)).boldened());
    outputValueLabel_.setJustificationType(juce::Justification::centred);
    outputValueLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFFF1F1F6));
    addAndMakeVisible(outputValueLabel_);

    // StereoMeters (Input + Output)
    addAndMakeVisible(stereoInput_.get());
    addAndMakeVisible(stereoOutput_.get());

    // Circular GR Gauge
    addAndMakeVisible(grGauge_.get());

    // Stereo labels below each meter
    stereoInputLabel_.setText("L: --.-   R: --.-", juce::dontSendNotification);
    stereoInputLabel_.setFont(juce::Font(juce::FontOptions(7.0f)));
    stereoInputLabel_.setJustificationType(juce::Justification::centred);
    stereoInputLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF6B7280));
    addAndMakeVisible(stereoInputLabel_);

    stereoOutputLabel_.setText("L: --.-   R: --.-", juce::dontSendNotification);
    stereoOutputLabel_.setFont(juce::Font(juce::FontOptions(7.0f)));
    stereoOutputLabel_.setJustificationType(juce::Justification::centred);
    stereoOutputLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF6B7280));
    addAndMakeVisible(stereoOutputLabel_);

    // Bottom info: RMS y correlacion
    rmsLabel_.setText("RMS --.- dB", juce::dontSendNotification);
    rmsLabel_.setFont(juce::Font(juce::FontOptions(7.5f)));
    rmsLabel_.setJustificationType(juce::Justification::centred);
    rmsLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF6B7280));
    addAndMakeVisible(rmsLabel_);

    correlationLabel_.setText("\u03C6 --.--", juce::dontSendNotification);
    correlationLabel_.setFont(juce::Font(juce::FontOptions(7.5f)));
    correlationLabel_.setJustificationType(juce::Justification::centred);
    correlationLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF22C55E));
    addAndMakeVisible(correlationLabel_);

    // Window size: 400x640 (proporcion que cabe en FL Studio)
    setSize(400, 640);

    // Timer ~30 fps
    startTimer(33);
}

MessengerAudioProcessorEditor::~MessengerAudioProcessorEditor()
{
    stopTimer();
}

// ─── Apply Preset Colour ──────────────────────────────────────────────────

void MessengerAudioProcessorEditor::applyPresetColour(juce::Colour colour)
{
    currentColour_ = colour;
    colourSwatch_->setColour(colour);
    if (colourPresetStripRaw_)
        colourPresetStripRaw_->setActiveColour(colour);
    processorRef_.setTrackColour(colour);
    repaint();
}

// ─── Iconos vectoriales profesionales para las filas INFO ────────────────────

void MessengerAudioProcessorEditor::drawRowIcon(juce::Graphics& g, int rowIndex, juce::Rectangle<float> bounds)
{
    auto cx = bounds.getCentreX();
    auto cy = bounds.getCentreY();
    auto s = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.45f;  // half-size

    juce::Path p;
    g.setColour(juce::Colour(0xFF6B7280));

    switch (rowIndex)
    {
        case 0:  // NOMBRE - Shield/Badge
        {
            p.startNewSubPath(cx - s * 0.45f, cy - s * 0.2f);
            p.lineTo(cx - s * 0.45f, cy + s * 0.2f);
            p.lineTo(cx, cy + s * 0.55f);
            p.lineTo(cx + s * 0.45f, cy + s * 0.2f);
            p.lineTo(cx + s * 0.45f, cy - s * 0.2f);
            p.lineTo(cx + s * 0.3f, cy - s * 0.45f);
            p.lineTo(cx - s * 0.3f, cy - s * 0.45f);
            p.closeSubPath();
            // Small circle inside (identifier dot)
            p.addEllipse(cx - s * 0.07f, cy - s * 0.2f, s * 0.14f, s * 0.14f);
            break;
        }
        case 1:  // GRUPO - Three stacked bars (hierarchy)
        {
            float bh = s * 0.14f;
            float gap = s * 0.09f;
            float widths[] = { s * 0.55f, s * 0.75f, s };
            float totalH = bh * 3.0f + gap * 2.0f;
            float top = cy - totalH * 0.5f;
            for (int i = 0; i < 3; ++i) {
                float x = cx - widths[i] * 0.5f;
                p.addRoundedRectangle(x, top + i * (bh + gap), widths[i], bh, bh * 0.4f);
            }
            break;
        }
        case 2:  // COLOR - Droplet
        {
            p.startNewSubPath(cx, cy - s * 0.55f);
            p.quadraticTo(cx + s * 0.5f, cy + s * 0.05f, cx + s * 0.5f, cy + s * 0.25f);
            p.quadraticTo(cx + s * 0.5f, cy + s * 0.55f, cx, cy + s * 0.55f);
            p.quadraticTo(cx - s * 0.5f, cy + s * 0.55f, cx - s * 0.5f, cy + s * 0.25f);
            p.quadraticTo(cx - s * 0.5f, cy + s * 0.05f, cx, cy - s * 0.55f);
            p.closeSubPath();
            // Inner dot
            p.addEllipse(cx - s * 0.07f, cy + s * 0.12f, s * 0.14f, s * 0.14f);
            break;
        }
        case 3:  // TIPO - Sine wave
        {
            // Draw waveform as stroked path
            juce::Path wave;
            wave.startNewSubPath(cx - s * 0.45f, cy);
            wave.quadraticTo(cx - s * 0.22f, cy - s * 0.4f, cx, cy);
            wave.quadraticTo(cx + s * 0.22f, cy + s * 0.4f, cx + s * 0.45f, cy);
            juce::PathStrokeType stroke(1.5f, juce::PathStrokeType::curved);
            g.strokePath(wave, stroke);
            return;  // Already drawn, skip fillPath
        }
        case 4:  // PRIORIDAD - Flag on pole
        {
            float px = cx - s * 0.25f;
            // Pole
            p.addRoundedRectangle(px - s * 0.04f, cy - s * 0.55f, s * 0.08f, s * 1.1f, s * 0.04f);
            // Flag
            juce::Path flag;
            flag.startNewSubPath(px + s * 0.04f, cy - s * 0.5f);
            flag.lineTo(px + s * 0.55f, cy - s * 0.3f);
            flag.lineTo(px + s * 0.04f, cy - s * 0.1f);
            flag.closeSubPath();
            p.addPath(flag);
            break;
        }
        case 5:  // NOTAS - Speech bubble
        {
            float bw = s * 1.0f;
            float bh = s * 0.75f;
            // Bubble body
            p.addRoundedRectangle(cx - bw * 0.5f, cy - bh * 0.45f, bw, bh, s * 0.12f);
            // Triangle pointer at bottom
            p.addTriangle(cx - s * 0.08f, cy + bh * 0.3f,
                          cx + s * 0.08f, cy + bh * 0.3f,
                          cx, cy + s * 0.6f);
            // Text cursor line inside
            p.addRoundedRectangle(cx - s * 0.2f, cy - s * 0.08f, s * 0.3f, s * 0.04f, s * 0.02f);
            break;
        }
        case 6:  // MUTE - Microphone (tachado si muteado)
        {
            // Mic body (capsule)
            float micW = s * 0.35f;
            float micH = s * 0.5f;
            p.addRoundedRectangle(cx - micW * 0.5f, cy - micH * 0.6f, micW, micH, s * 0.1f);
            // Mic stand
            p.addRoundedRectangle(cx - s * 0.04f, cy + micH * 0.3f, s * 0.08f, s * 0.35f, s * 0.04f);
            // Mic base
            p.addRoundedRectangle(cx - s * 0.2f, cy + s * 0.5f, s * 0.4f, s * 0.08f, s * 0.04f);

            if (processorRef_.isMuted()) {
                // Línea diagonal roja (tachado)
                g.setColour(juce::Colour(0xFFEF4444));
                juce::Path xLine;
                xLine.startNewSubPath(cx - s * 0.6f, cy - s * 0.65f);
                xLine.lineTo(cx + s * 0.6f, cy + s * 0.65f);
                g.strokePath(xLine, juce::PathStrokeType(2.5f));
            }
            break;
        }
    }

    g.fillPath(p);
}

// ─── Layout — Form-style info rows + 3-column meters ─────────────────────────

void MessengerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(8);

    // ====================================================================
    //  INFO PANEL ROWS (form-style, 6 rows with separators)
    // ====================================================================
    // Each row: icon(24) + label(60) + value(rest) | rowHeight = 26
    // Header takes 16px, each row takes 26px, separator 1px

    const int infoHeaderH = 14;      // "INFORMACION DE PISTA"
    const int rowH = 28;              // altura por fila (+2px mas espacioso)
    const int sepH = 1;               // separador entre filas
    const int iconW = 22;             // ancho del icono
    const int labelW = 58;            // ancho del label
    const int statusW = 100;          // ancho del status label (right-aligned)

    area.removeFromTop(infoHeaderH + 4);  // header + mas spacer

    // ─── Row 1: NOMBRE ────────────────────────────────────────────────
    {
        auto row = area.removeFromTop(rowH);
        row.removeFromLeft(iconW + 4);  // icon drawn in paint()

        auto valueArea = row.removeFromLeft(row.getWidth() - 50);
        nameEditor_.setBounds(valueArea.reduced(0, 2));

        auto swatchArea = row.removeFromLeft(50);
        colourSwatch_->setBounds(swatchArea.reduced(8, 3));
    }
    area.removeFromTop(sepH);

    // ─── Row 2: GRUPO ─────────────────────────────────────────────────
    {
        auto row = area.removeFromTop(rowH);
        row.removeFromLeft(iconW + 4);  // skip icon (painted)

        auto labelArea = row.removeFromLeft(labelW);
        juce::ignoreUnused(labelArea);  // label painted in paint()

        auto valueArea = row.removeFromLeft(row.getWidth() - 80);
        busValueLabel_.setBounds(valueArea.reduced(2, 2));
        auto comboArea = row.removeFromLeft(80);
        busComboBox_.setBounds(comboArea.reduced(1, 2));
    }
    area.removeFromTop(sepH);

    // ─── Row 3: COLOR ─────────────────────────────────────────────────
    {
        auto row = area.removeFromTop(rowH);
        row.removeFromLeft(iconW + 4);

        auto labelArea = row.removeFromLeft(labelW);
        juce::ignoreUnused(labelArea);

        colourPresetStrip_->setBounds(row.reduced(2, 2));
    }
    area.removeFromTop(sepH);

    // ─── Row 4: TIPO ──────────────────────────────────────────────────
    {
        auto row = area.removeFromTop(rowH);
        row.removeFromLeft(iconW + 4);

        auto labelArea = row.removeFromLeft(labelW);
        juce::ignoreUnused(labelArea);

        tipoValueLabel_.setBounds(row.reduced(2, 2));
    }
    area.removeFromTop(sepH);

    // ─── Row 5: PRIORIDAD ─────────────────────────────────────────────
    {
        auto row = area.removeFromTop(rowH);
        row.removeFromLeft(iconW + 4);

        auto labelArea = row.removeFromLeft(labelW);
        juce::ignoreUnused(labelArea);

        prioridadValueLabel_.setBounds(row.reduced(2, 2));
    }
    area.removeFromTop(sepH);

    // ─── Row 6: NOTAS ─────────────────────────────────────────────────
    {
        auto row = area.removeFromTop(rowH);
        row.removeFromLeft(iconW + 4);

        auto labelArea = row.removeFromLeft(labelW);
        juce::ignoreUnused(labelArea);

        notasEditor_.setBounds(row.reduced(2, 2));
    }

    // ─── Row 7: MUTE ──────────────────────────────────────────────────
    {
        auto row = area.removeFromTop(rowH);
        row.removeFromLeft(iconW + 4);

        auto labelArea = row.removeFromLeft(labelW);
        juce::ignoreUnused(labelArea);

        muteButton_.setBounds(row.reduced(2, 3));
    }

    // ─── Status label (esquina superior derecha) ───────────────────────
    statusLabel_.setBounds(getWidth() - statusW - 10, 4, statusW, 14);

    area.removeFromTop(12);  // spacer entre INFO y NIVELES

    // ====================================================================
    //  NIVELES PANEL — 3-column meters
    // ====================================================================

    auto nivelesArea = area.reduced(0, 2);

    // ─── Proporciones de columnas: INPUT 35% | GR 30% | OUTPUT 35% ────
    auto totalW = nivelesArea.getWidth();
    int colInW  = static_cast<int>(totalW * 0.35f);
    int colGrW  = static_cast<int>(totalW * 0.30f);
    int colOutW = totalW - colInW - colGrW;

    // ─── Column headers (16px, mas espaciado) ────────────────────────────
    auto colHeaderRow = nivelesArea.removeFromTop(16);
    inputLabel_.setBounds(colHeaderRow.removeFromLeft(colInW));
    grLabel_.setBounds(colHeaderRow.removeFromLeft(colGrW));
    outputLabel_.setBounds(colHeaderRow);

    // ─── Value dB labels (22px, mas espacio) ────────────────────────────
    auto valueRow = nivelesArea.removeFromTop(22);
    inputValueLabel_.setBounds(valueRow.removeFromLeft(colInW));
    grValueLabel_.setBounds(valueRow.removeFromLeft(colGrW));
    outputValueLabel_.setBounds(valueRow);

    nivelesArea.removeFromTop(4);

    // ─── Meters: 3 columnas con padding consistente ─────────────────────
    auto metersRow = nivelesArea.removeFromTop(320);

    // INPUT column: StereoMeter (L+R, escala compartida)
    auto inputCol = metersRow.removeFromLeft(colInW).reduced(3, 2);
    stereoInput_->setBounds(inputCol);

    // GR column: CircularGauge centered
    auto grCol = metersRow.removeFromLeft(colGrW).reduced(3, 2);
    grGauge_->setBounds(grCol);

    // OUTPUT column: StereoMeter (L+R, escala compartida)
    auto outputCol = metersRow.reduced(3, 2);
    stereoOutput_->setBounds(outputCol);

    nivelesArea.removeFromTop(6);

    // ─── Bottom info row: L/R labels + RMS + correlation ────────────────
    auto chRow = nivelesArea.removeFromTop(16);
    stereoInputLabel_.setBounds(chRow.removeFromLeft(colInW));
    chRow.removeFromLeft(colGrW);  // skip GR column
    stereoOutputLabel_.setBounds(chRow);

    // RMS + φ correlation below the channel labels
    auto infoRow = nivelesArea.removeFromTop(16);
    infoRow.removeFromLeft(colInW);  // skip input
    rmsLabel_.setBounds(infoRow.removeFromLeft(colGrW));
    correlationLabel_.setBounds(infoRow);

    // ─── Calcular bounds del panel NIVELES (alineado con contenido) ────
    nivelesPanelBounds_ = getLocalBounds().withTop(inputLabel_.getY() - 6)
                                          .withBottom(correlationLabel_.getBottom() + 8)
                                          .withLeft(6)
                                          .withRight(getWidth() - 6);
}

// ─── Paint ───────────────────────────────────────────────────────────────────

void MessengerAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // ─── Fondo profundo (#08080F como la referencia) ───────────────────
    juce::ColourGradient bgGrad(
        juce::Colour(0xFF12121E),
        juce::Point<float>(0.0f, 0.0f),
        juce::Colour(0xFF08080F),
        juce::Point<float>(0.0f, (float)bounds.getHeight()),
        false);
    g.setGradientFill(bgGrad);
    g.fillRect(bounds);

    // ─── Borde exterior sutil ──────────────────────────────────────────
    g.setColour(juce::Colour(0xFF1A1A2E));
    g.drawRect(bounds, 1);

    // ─── Barra de color lateral (3px, color del track) ─────────────────
    g.setColour(currentColour_.withAlpha(0.2f));
    g.fillRect(0, 0, 3, bounds.getHeight());

    // ─── SECTION HEADERS ────────────────────────────────────────────────

    // "INFORMACION DE PISTA"
    g.setFont(juce::Font(juce::FontOptions(9.5f)).boldened());
    g.setColour(juce::Colour(0xFFA78BFA));
    g.drawText("INFORMACION DE PISTA",
               juce::Rectangle<int>(12, 4, 160, 14),
               juce::Justification::centredLeft);

    // ─── INFO ROW LABELS + ICONS ───────────────────────────────────────
    const int infoHeaderH = 14;
    const int rowH = 28;  // sincronizado con resized()
    const int sepH = 1;
    const int iconW = 22;
    const int labelW = 58;

    const char* rowLabels[] = {
        "NOMBRE", "GRUPO", "COLOR", "TIPO", "PRIORIDAD", "NOTAS", "MUTE"
    };
    juce::Colour labelColour = juce::Colour(0xFF6B7280);
    juce::Colour sepColour = juce::Colour(0xFF1A1A2E);

    int y = 8 + infoHeaderH + 4;

    for (int i = 0; i < 7; ++i)
    {
        // ─── Icono vectorial ────────────────────────────────────────────
        drawRowIcon(g, i, juce::Rectangle<float>(12.0f, (float)y, (float)iconW, (float)rowH));

        // ─── Label ────────────────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
        g.setColour(labelColour);
        g.drawText(rowLabels[i],
                   juce::Rectangle<int>(12 + iconW + 4, y, labelW, rowH),
                   juce::Justification::centredLeft);

        y += rowH;

        // ─── Separator ────────────────────────────────────────────────
        if (i < 6) {
            g.setColour(sepColour);
            g.drawHorizontalLine(y, 12.0f, (float)(getWidth() - 12));
            y += sepH;
        }
    }

    // ─── Separador sutil entre INFO y NIVELES ───────────────────────────
    y += 5;
    g.setColour(juce::Colour(0xFF1A1A2E).withAlpha(0.5f));
    g.drawHorizontalLine(y, 12.0f, (float)(getWidth() - 12));
    y += 7;

    // ─── Fondo del panel NIVELES con esquinas redondeadas ─────────────
    if (!nivelesPanelBounds_.isEmpty()) {
        auto panel = nivelesPanelBounds_.toFloat();
        g.setColour(juce::Colour(0xFF0C0C18));
        g.fillRoundedRectangle(panel, 12.0f);
        g.setColour(juce::Colour(0xFF1A1A2E).withAlpha(0.35f));
        g.drawRoundedRectangle(panel, 12.0f, 1.0f);
    }

    // ─── "NIVELES" header ──────────────────────────────────────────────
    g.setFont(juce::Font(juce::FontOptions(9.5f)).boldened());
    g.setColour(juce::Colour(0xFFA78BFA));
    g.drawText("NIVELES",
               juce::Rectangle<int>(12, y, 80, 14),
               juce::Justification::centredLeft);

    y += 14;

    // ─── Separador sutil antes de columnas ─────────────────────────────
    g.setColour(juce::Colour(0xFF1A1A2E).withAlpha(0.5f));
    g.drawHorizontalLine(y, 12.0f, (float)(getWidth() - 12));
    y += 4;

    // ─── Separadores verticales entre las 3 columnas (35% | 30% | 35%) ──
    const int nivelesTop = y;
    const int nivelesBottom = stereoOutput_->getBounds().getBottom() + 2;

    if (nivelesBottom > nivelesTop) {
        int totalNetW = getWidth() - 16;
        int sep1 = 8 + static_cast<int>(totalNetW * 0.35f);
        int sep2 = 8 + static_cast<int>(totalNetW * 0.65f);
        g.setColour(juce::Colour(0xFF1A1A2E).withAlpha(0.6f));
        g.drawVerticalLine(sep1, (float)nivelesTop, (float)nivelesBottom);
        g.drawVerticalLine(sep2, (float)nivelesTop, (float)nivelesBottom);

        // ─── Separador horizontal despues de los meters ──────────────
        g.setColour(juce::Colour(0xFF1A1A2E).withAlpha(0.5f));
        g.drawHorizontalLine(nivelesBottom, 12.0f, (float)(getWidth() - 12));
    }
}

// ─── TextEditor callback ──────────────────────────────────────────────────────

void MessengerAudioProcessorEditor::textEditorTextChanged(juce::TextEditor& editor)
{
    if (&editor == &nameEditor_) {
        auto newName = editor.getText().trim();
        if (newName.isNotEmpty()) {
            processorRef_.setTrackName(newName);
        }
    }
}

// ─── Timer callback (30fps) ───────────────────────────────────────────────────

void MessengerAudioProcessorEditor::timerCallback()
{
    auto slotIndex = processorRef_.getSlotIndex();

    if (slotIndex < 0) {
        processorRef_.ensureSlotRegistered();
        slotIndex = processorRef_.getSlotIndex();

        if (slotIndex >= 0) {
            statusLabel_.setText("\u25CF Conectado a Brain",
                juce::dontSendNotification);
            statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF22C55E));
        } else {
            statusLabel_.setText("\u25CB Conectando...",
                juce::dontSendNotification);
            statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
            return;
        }
    }

    auto* sharedData = processorRef_.getSharedData();
    if (!sharedData) return;
    auto& registry = sharedData->getSlotRegistry();
    auto info = registry.getSlotInfo(slotIndex);

    if (!info.active) {
        statusLabel_.setText("\u25CB Sin conexion",
            juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
        busValueLabel_.setText("Sin ruteo", juce::dontSendNotification);
        busValueLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
        grGauge_->setValue(0.0f);
        return;
    }

    auto latest = registry.getTelemetry(slotIndex).latest();

    bool hasSignal = (latest.peakLeft > -60.0f || latest.peakRight > -60.0f);
    auto bus = processorRef_.getBusAssignment();

    // ─── Status ────────────────────────────────────────────────────────
    if (hasSignal) {
        statusLabel_.setText("\u25CF Transmitiendo",
                             juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF22C55E));
    } else {
        statusLabel_.setText("\u25CF Conectado",
                             juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF22C55E).withAlpha(0.6f));
    }

    // ─── Bus value display ─────────────────────────────────────────────
    {
        auto busName = (bus != BusType::None)
            ? juce::String(busNames[static_cast<int>(bus)])
            : "Sin ruteo";
        auto busCol = (bus != BusType::None)
            ? getBusColour(static_cast<int>(bus))
            : juce::Colour(0xFF888888);
        busValueLabel_.setText(busName, juce::dontSendNotification);
        busValueLabel_.setColour(juce::Label::textColourId, busCol);
    }

    // ─── Telemetry data ────────────────────────────────────────────────
    lastPeakLeft_    = latest.peakLeft;
    lastPeakRight_   = latest.peakRight;
    lastCorrelation_ = latest.correlation;
    lastRMS_ = (latest.rmsLeft + latest.rmsRight) * 0.5f;

    // ─── Gain Reduction (simulada desde peak) ───────────────────────
    //  Mapeo: -20 dB peak -> 0.0 dB GR, +6 dB peak -> 6.0 dB GR
    //  Cuando la senal es mas fuerte, hay mas compresion
    {
        float avgPeak = (lastPeakLeft_ + lastPeakRight_) * 0.5f;
        float gr = 0.0f;
        if (avgPeak > -20.0f)
            gr = (avgPeak + 20.0f) / 26.0f * 6.0f;
        lastGR_ = juce::jlimit(0.0f, 8.0f, gr);
    }

    // ─── Column value labels ──────────────────────────────────────────
    float avgInput = (lastPeakLeft_ + lastPeakRight_) * 0.5f;
    float avgOutput = avgInput - lastGR_;  // output = input - GR

    inputValueLabel_.setText(juce::String(avgInput, 1) + " dB", juce::dontSendNotification);
    outputValueLabel_.setText(juce::String(avgOutput, 1) + " dB", juce::dontSendNotification);
    grValueLabel_.setText(juce::String(lastGR_, 1) + " dB", juce::dontSendNotification);
    grGauge_->setValue(lastGR_);

    // ─── StereoMeters: input con peaks reales, output con GR aplicada ─
    stereoInput_->setLevels(lastPeakLeft_, lastPeakRight_);
    {
        float outL = lastPeakLeft_ - lastGR_;
        float outR = lastPeakRight_ - lastGR_;
        stereoOutput_->setLevels(outL, outR);
    }

    // ─── Channel labels ───────────────────────────────────────────────
    stereoInputLabel_.setText(
        "L: " + juce::String(lastPeakLeft_, 1) +
        "   R: " + juce::String(lastPeakRight_, 1),
        juce::dontSendNotification);
    stereoOutputLabel_.setText(
        "L: " + juce::String(lastPeakLeft_ - lastGR_, 1) +
        "   R: " + juce::String(lastPeakRight_ - lastGR_, 1),
        juce::dontSendNotification);

    // ─── RMS ──────────────────────────────────────────────────────────
    rmsLabel_.setText(
        "RMS " + juce::String(lastRMS_, 1) + " dB",
        juce::dontSendNotification);

    // ─── Correlacion (phi) ────────────────────────────────────────────
    {
        auto corrCol = (lastCorrelation_ > 0.3f)
            ? juce::Colour(0xFF22C55E)
            : (lastCorrelation_ > -0.3f)
                ? juce::Colour(0xFFEAB308)
                : juce::Colour(0xFFEF4444);
        correlationLabel_.setColour(juce::Label::textColourId, corrCol);
        correlationLabel_.setText(
            "\u03C6 " + juce::String(lastCorrelation_, 2),
            juce::dontSendNotification);
    }

    // ─── Sincronizar estado del botón MUTE ───────────────────────────
    // El estado puede cambiar externamente (e.g. al cargar preset)
    if (muteButton_.getToggleState() != processorRef_.isMuted()) {
        muteButton_.setToggleState(processorRef_.isMuted(), juce::dontSendNotification);
        updateMuteDisplay();
    }

    // ─── TIPO y PRIORIDAD (actualizados al cambiar bus) ───────────────
    // Ya se actualizan en busComboBox_.onChange
}

void MessengerAudioProcessorEditor::updateMuteDisplay()
{
    bool muted = processorRef_.isMuted();
    muteButton_.setButtonText(muted ? "MUTED" : "MUTE");
    repaint();  // Repaint icon overlay
}

// ─── Selector de color ────────────────────────────────────────────────────────

void MessengerAudioProcessorEditor::openColourSelector()
{
    auto* selector = new juce::ColourSelector(
        juce::ColourSelector::showColourAtTop |
        juce::ColourSelector::showSliders |
        juce::ColourSelector::showColourspace);

    selector->setName("Color de Pista");
    selector->setCurrentColour(currentColour_);
    selector->addChangeListener(&colourListener_);

    juce::CallOutBox::launchAsynchronously(
        std::unique_ptr<juce::Component>(selector),
        colourSwatch_->getScreenBounds(),
        nullptr);
}

} // namespace mixcoach
