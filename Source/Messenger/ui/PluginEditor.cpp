#include "PluginEditor.h"
#include "VUMeter.h"
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
    // NOTA: setSize() se llama al FINAL del constructor para evitar que resized()
    // acceda a unique_ptrs que aún no han sido creados.
    setResizable(false, false);

    // Componentes personalizados
    colourSwatch_   = std::make_unique<ColourSwatch>();
    vuMeterLeft_    = std::make_unique<VUMeter>();
    vuMeterRight_   = std::make_unique<VUMeter>();
    vuMeterLeft_->setBarColour(juce::Colour(0xFF3498DB));   // azul
    vuMeterRight_->setBarColour(juce::Colour(0xFF2ECC71));  // verde

    waveformView_   = std::make_unique<WaveformView>();

    // Icono
    iconLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x8E\xB5"), juce::dontSendNotification);
    iconLabel_.setFont(juce::Font(juce::FontOptions(20.0f)));
    iconLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(iconLabel_);

    // Editor de nombre
    nameEditor_.setText(processorRef_.getTrackName());
    nameEditor_.setFont(juce::Font(juce::FontOptions(14.0f)).boldened());
    nameEditor_.setJustification(juce::Justification::centredLeft);
    nameEditor_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF0F0F1A));
    nameEditor_.setColour(juce::TextEditor::textColourId, juce::Colour(0xFFE0E0E0));
    nameEditor_.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF2C2C3E));
    nameEditor_.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xFF3498DB));
    nameEditor_.setIndents(6, 3);
    nameEditor_.setBorder(juce::BorderSize<int>(2));
    nameEditor_.setInputRestrictions(32);
    nameEditor_.addListener(this);
    addAndMakeVisible(nameEditor_);

    // Color swatch
    currentColour_ = processorRef_.getTrackColour();
    colourSwatch_->setColour(currentColour_);
    colourSwatch_->onClick = [this]() { openColourSelector(); };
    addAndMakeVisible(colourSwatch_.get());

    // ─── Colour Preset Strip ────────────────────────────────────────────────
    colourPresetStrip_ = std::make_unique<ColourPresetStrip>();
    colourPresetStripRaw_ = static_cast<ColourPresetStrip*>(colourPresetStrip_.get());
    colourPresetStripRaw_->setActiveColour(currentColour_);
    colourPresetStripRaw_->onColourChosen = [this](juce::Colour col) { applyPresetColour(col); };
    addAndMakeVisible(colourPresetStrip_.get());

    // Listener de cambio de color (desde ColourSelector)
    colourListener_.onColourChanged = [this](juce::Colour newColour) {
        LogHelper::writeToLog("[Messenger] ColourSelector cambio color a " + newColour.toDisplayString(false));
        currentColour_ = newColour;
        colourSwatch_->setColour(newColour);
        if (colourPresetStripRaw_)
            colourPresetStripRaw_->setActiveColour(newColour);
        processorRef_.setTrackColour(newColour);
        waveformView_->setWaveColour(newColour);
        repaint();
    };

    // Estado        statusLabel_.setText("\xF0\x9F\x93\xA1 Activo", juce::dontSendNotification);
    statusLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
    statusLabel_.setJustificationType(juce::Justification::centredLeft);
    statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF2ECC71));
    addAndMakeVisible(statusLabel_);

    routingLabel_.setText("\xF0\x9F\x94\x97 Enviando a MixCoach", juce::dontSendNotification);
    routingLabel_.setFont(juce::Font(juce::FontOptions(10.0f)));
    routingLabel_.setJustificationType(juce::Justification::centredLeft);
    routingLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF3498DB));
    addAndMakeVisible(routingLabel_);

    // ─── Bus routing ComboBox ──────────────────────────────────────────────
    busComboBox_.setEditableText(false);
    busComboBox_.setJustificationType(juce::Justification::centredLeft);
    busComboBox_.addItem("Sin ruteo", 1);
    for (int i = 0; i < kNumBuses; ++i)
        busComboBox_.addItem(busNames[i], i + 2);
    busComboBox_.setSelectedId(static_cast<int>(processorRef_.getBusAssignment()) + 2);
    busComboBox_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF0F0F1A));
    busComboBox_.setColour(juce::ComboBox::textColourId, juce::Colour(0xFFE0E0E0));
    busComboBox_.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF2C2C3E));
    busComboBox_.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xFF3498DB));
    busComboBox_.onChange = [this]() {
        auto selected = busComboBox_.getSelectedId();
        auto bus = (selected <= 1) ? BusType::None : static_cast<BusType>(selected - 2);
        LogHelper::writeToLog("[Messenger] Cambio de bus a " +
            juce::String(static_cast<int>(bus)) + " (" +
            (bus != BusType::None ? juce::String(busNames[static_cast<int>(bus)]) : "Sin ruteo") + ")");
        processorRef_.setBusAssignment(bus);
        routingLabel_.setText("\xF0\x9F\x94\x97 Enviando a MixCoach" +
            (bus != BusType::None ? (juce::String(" [") + busNames[static_cast<int>(bus)] + "]") : juce::String()), 
            juce::dontSendNotification);
    };
    addAndMakeVisible(busComboBox_);

    // Labels de nivel (más compactos)
    levelLeftLabel_.setText("L: --.- dB", juce::dontSendNotification);
    levelLeftLabel_.setFont(juce::Font(juce::FontOptions(10.0f)));
    levelLeftLabel_.setJustificationType(juce::Justification::centredLeft);
    levelLeftLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
    addAndMakeVisible(levelLeftLabel_);

    levelRightLabel_.setText("R: --.- dB", juce::dontSendNotification);
    levelRightLabel_.setFont(juce::Font(juce::FontOptions(10.0f)));
    levelRightLabel_.setJustificationType(juce::Justification::centredLeft);
    levelRightLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
    addAndMakeVisible(levelRightLabel_);

    rmsLabel_.setText("RMS: --.- dB", juce::dontSendNotification);
    rmsLabel_.setFont(juce::Font(juce::FontOptions(10.0f)));
    rmsLabel_.setJustificationType(juce::Justification::centredLeft);
    rmsLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
    addAndMakeVisible(rmsLabel_);

    correlationLabel_.setText("\xCF\x86: --.--", juce::dontSendNotification);
    correlationLabel_.setFont(juce::Font(juce::FontOptions(10.0f)));
    correlationLabel_.setJustificationType(juce::Justification::centredLeft);
    correlationLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
    addAndMakeVisible(correlationLabel_);

    // VU Meters (antes LevelBar)
    addAndMakeVisible(vuMeterLeft_.get());
    addAndMakeVisible(vuMeterRight_.get());

    // Waveform View
    waveformView_->setWaveColour(currentColour_);
    addAndMakeVisible(waveformView_.get());

    // Ahora todos los componentes están creados, podemos llamar setSize()
    // que dispara resized() sin peligro.
    setSize(300, 360);

    // Timer ~30 fps (33ms intervalo)
    startTimer(33);
}

MessengerAudioProcessorEditor::~MessengerAudioProcessorEditor()
{
    stopTimer();
}

// ─── Apply Preset Colour ───────────────────────────────────────────────────

void MessengerAudioProcessorEditor::applyPresetColour(juce::Colour colour)
{
    LogHelper::writeToLog("[Messenger] Preset color aplicado: " + colour.toDisplayString(false));
    currentColour_ = colour;
    colourSwatch_->setColour(colour);
    if (colourPresetStripRaw_)
        colourPresetStripRaw_->setActiveColour(colour);
    processorRef_.setTrackColour(colour);
    waveformView_->setWaveColour(colour);
    repaint();
}

// ─── Layout ───────────────────────────────────────────────────────────────────

void MessengerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(8);

    // ─── Fila superior: icono + nombre + color ──────────────────────────────
    auto topRow = area.removeFromTop(34);
    iconLabel_.setBounds(topRow.removeFromLeft(30));
    // Colour swatch más grande (42px vs anteriores 34px)
    auto colourArea = topRow.removeFromRight(42);
    colourSwatch_->setBounds(colourArea.reduced(2, 2));
    nameEditor_.setBounds(topRow.reduced(4, 2));

    // ─── Colour Preset Strip (más alto para dots más grandes) ───────────────
    area.removeFromTop(3);
    auto presetRow = area.removeFromTop(24);
    presetRow.removeFromLeft(30); // alinear con el nombre
    colourPresetStrip_->setBounds(presetRow.reduced(3, 2));

    area.removeFromTop(3);

    // ─── Fila de estado ─────────────────────────────────────────────────────
    auto statusRow = area.removeFromTop(18);
    statusLabel_.setBounds(statusRow.removeFromLeft(130));
    area.removeFromTop(2);

    // ─── Fila de ruteo (bus selector) ───────────────────────────────────────
    auto routingRow = area.removeFromTop(22);
    auto bus = processorRef_.getBusAssignment();
    routingLabel_.setText("\xF0\x9F\x94\x97 Enviando a MixCoach" +
        (bus != BusType::None ? (juce::String(" [") + busNames[static_cast<int>(bus)] + "]") : juce::String()),
        juce::dontSendNotification);
    auto routingLabelArea = routingRow.removeFromLeft(145);
    routingLabel_.setBounds(routingLabelArea);
    busComboBox_.setBounds(routingRow.reduced(2, 1));

    area.removeFromTop(2);

    // ─── Labels de medidores (alineados con los meters) ──────────────────────
    auto labelsRow = area.removeFromTop(16);
    levelLeftLabel_.setBounds(labelsRow.removeFromLeft(85));
    levelRightLabel_.setBounds(labelsRow.removeFromLeft(85));
    rmsLabel_.setBounds(labelsRow.removeFromLeft(85));
    correlationLabel_.setBounds(labelsRow);

    area.removeFromTop(3);

    // ─── VU Meters (extendidos para llenar el espacio vertical) ────────────
    auto metersRow = area.removeFromTop(210);
    auto leftMeterArea = metersRow.removeFromLeft(85);
    vuMeterLeft_->setBounds(leftMeterArea.reduced(1, 2));
    auto rightMeterArea = metersRow.removeFromLeft(85);
    vuMeterRight_->setBounds(rightMeterArea.reduced(1, 2));

    // ─── Waveform (más compacto, ocupa espacio restante) ────────────────────
    waveformView_->setBounds(metersRow.reduced(1, 2));

    area.removeFromTop(2);

    // ─── Espacio restante: actividad/nivel general ──────────────────────────
}

// ─── Paint ────────────────────────────────────────────────────────────────────

void MessengerAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Fondo con gradiente oscuro
    juce::ColourGradient bgGrad(
        juce::Colour(0xFF1A1A2E),
        juce::Point<float>(0.0f, 0.0f),
        juce::Colour(0xFF0F0F1A),
        juce::Point<float>(0.0f, (float)bounds.getHeight()),
        false);
    g.setGradientFill(bgGrad);
    g.fillRect(bounds);

    // Borde exterior
    g.setColour(juce::Colour(0xFF2C2C3E));
    g.drawRect(bounds, 1);

    // Línea separadora después del header
    g.setColour(juce::Colour(0xFF2C2C3E).withAlpha(0.5f));
    g.drawHorizontalLine(38, 6.0f, (float)(bounds.getWidth() - 6));

    // Barra de color en el lateral izquierdo
    g.setColour(currentColour_.withAlpha(0.15f));
    g.fillRect(0, 0, 3, bounds.getHeight());

    // Gradiente sutíl en el waveform area
    auto waveArea = getLocalBounds().reduced(6);
    waveArea.removeFromTop(180); // header + labels + meters
    if (waveArea.getHeight() > 0) {
        juce::ColourGradient waveBg(
            currentColour_.withAlpha(0.03f),
            juce::Point<float>((float)waveArea.getX(), (float)waveArea.getY()),
            currentColour_.withAlpha(0.0f),
            juce::Point<float>((float)waveArea.getX(), (float)waveArea.getBottom()),
            false);
        g.setGradientFill(waveBg);
        g.fillRect(waveArea);
    }
}

// ─── TextEditor callback ──────────────────────────────────────────────────────

void MessengerAudioProcessorEditor::textEditorTextChanged(juce::TextEditor& editor)
{
    if (&editor == &nameEditor_) {
        auto newName = editor.getText().trim();
        if (newName.isNotEmpty()) {
            LogHelper::writeToLog("[Messenger] Cambio de nombre: \"" + processorRef_.getTrackName() + "\" -> \"" + newName + "\"");
            processorRef_.setTrackName(newName);
        }
    }
}

// ─── Timer callback ───────────────────────────────────────────────────────────

void MessengerAudioProcessorEditor::timerCallback()
{
    auto slotIndex = processorRef_.getSlotIndex();

    // ─── Si aún no está registrado, intentarlo ahora ─────────────────────────
    // En FL Studio, prepareToPlay puede tardar mucho en ser llamado.
    // El editor se inicializa antes, así que forzamos el registro desde aquí.
    if (slotIndex < 0) {
        processorRef_.ensureSlotRegistered();
        slotIndex = processorRef_.getSlotIndex();

        // Actualizar la UI del nombre (puede haber cambiado desde el estado inicial)
        if (slotIndex >= 0) {
            // Slot recién registrado — mostrar estado OK
            statusLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x94\x8A Conectado a Brain"),
                juce::dontSendNotification);
            statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF2ECC71));
        } else {
            // Aún sin registrar
            statusLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x94\x84 Conectando..."),
                juce::dontSendNotification);
            statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFFAAAAAA));
            return;
        }
    }

    auto* sharedData = processorRef_.getSharedData();
    if (!sharedData) return;
    auto& registry = sharedData->getSlotRegistry();
    auto info = registry.getSlotInfo(slotIndex);

    if (!info.active) {
        statusLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x94\x87 Sin conexion al Brain"), juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
        routingLabel_.setText(juce::CharPointer_UTF8("\xE2\x9D\x8C Sin ruteo"), juce::dontSendNotification);
        routingLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
        waveformView_->setSignalPresent(false);
        waveformView_->pushSample(0.0f);
        return;
    }


    auto latest = registry.getTelemetry(slotIndex).latest();

    bool hasSignal = (latest.peakLeft > -60.0f || latest.peakRight > -60.0f);
    auto bus = processorRef_.getBusAssignment();
    juce::String busSuffix = (bus != BusType::None)
        ? juce::String(" [") + busNames[static_cast<int>(bus)] + "]"
        : juce::String();        if (hasSignal) {
        // Con audio → mostramos nivel y ruteo activo
        statusLabel_.setText(juce::String("\xF0\x9F\x8E\xB5 Transmitiendo a Brain") + busSuffix,
                            juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF2ECC71));
        routingLabel_.setText("\xF0\x9F\x94\x97 Enviando a MixCoach" + busSuffix, juce::dontSendNotification);
        routingLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF3498DB));
    } else {
        // Conectado pero sin audio
        statusLabel_.setText(juce::String("\xF0\x9F\x94\x8A Conectado a Brain") + busSuffix,
                            juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF2ECC71).withAlpha(0.7f));
        routingLabel_.setText("\xF0\x9F\x94\x97 Enviando a MixCoach" + busSuffix, juce::dontSendNotification);
        routingLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF3498DB));
    }

    lastPeakLeft_    = latest.peakLeft;
    lastPeakRight_   = latest.peakRight;
    lastCorrelation_ = latest.correlation;
    lastRMS_ = (latest.rmsLeft + latest.rmsRight) * 0.5f;

    levelLeftLabel_.setText("L: " + juce::String(lastPeakLeft_, 1) + " dB",
                            juce::dontSendNotification);
    levelRightLabel_.setText("R: " + juce::String(lastPeakRight_, 1) + " dB",
                             juce::dontSendNotification);
    rmsLabel_.setText("RMS: " + juce::String(lastRMS_, 1) + " dB",
                      juce::dontSendNotification);

    // Color de correlacion
    juce::Colour corrColour;
    juce::String corrIcon;
    if (std::abs(lastCorrelation_) < 0.3f) {
        corrColour = juce::Colour(0xFFE74C3C);
        corrIcon = "!";
    } else if (lastCorrelation_ < 0.0f) {
        corrColour = juce::Colour(0xFFF39C12);
        corrIcon = "!";
    } else {
        corrColour = juce::Colour(0xFF2ECC71);
        corrIcon = "+";
    }
    correlationLabel_.setText("\xCF\x86: " + corrIcon + " " +
                              juce::String(lastCorrelation_, 2),
                              juce::dontSendNotification);
    correlationLabel_.setColour(juce::Label::textColourId, corrColour);

    // Actualizar VU Meters
    vuMeterLeft_->setLevel(lastPeakLeft_);
    vuMeterRight_->setLevel(lastPeakRight_);

    // Actualizar waveform
    waveformView_->setSignalPresent(hasSignal);
    waveformView_->setRMS(lastRMS_);
    if (hasSignal) {
        // Generar waveform orgánico usando peak + RMS + variación armónica
        float peakNorm = juce::jmap(juce::jlimit(-60.0f, 0.0f, lastPeakLeft_),
                                    -60.0f, 0.0f, 0.0f, 1.0f);
        float rmsNorm  = juce::jmap(juce::jlimit(-60.0f, 0.0f, lastRMS_),
                                    -60.0f, 0.0f, 0.0f, 1.0f);
        // Crest factor: diferencia entre peak y RMS da la "forma"
        // (crest usado implícitamente en la relación peakNorm/rmsNorm)
        float waveShape = peakNorm * 0.6f + rmsNorm * 0.4f;
        // Oscilación armónica para simular contenido de frecuencia
        float t = juce::Time::getMillisecondCounter() * 0.003f;
        float harmonic = std::sin(t * 2.0f) * 0.3f + std::sin(t * 5.0f) * 0.15f + std::sin(t * 11.0f) * 0.05f;
        float finalAmp = waveShape * (0.6f + 0.4f * harmonic) * (1.0f + juce::jmax(0.0f, lastCorrelation_) * 0.2f);
        waveformView_->pushSample(finalAmp);
    } else {
        waveformView_->pushSample(0.0f);
    }
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
