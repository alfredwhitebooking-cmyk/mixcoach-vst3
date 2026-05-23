#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "../Common/SharedData.h"
#include "../Common/LogHelper.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  VUMeter — Medidor VU estilo profesional con peak hold
// ═══════════════════════════════════════════════════════════════════════════
class VUMeter : public juce::Component
{
public:
    VUMeter() { setOpaque(false); }

    void setLevel(float levelDb)
    {
        // Smooth ballistics: attack rápido, release lento (VU style)
        auto newLevel = juce::jlimit(-80.0f, 6.0f, levelDb);
        
        // Attack: subida instantánea
        if (newLevel > currentLevel_) {
            currentLevel_ = newLevel;
        } 
        // Release: caída suave (decaimiento como VU meter)
        else {
            currentLevel_ += (newLevel - currentLevel_) * 0.15f;
        }
        
        // Peak hold con decay
        if (newLevel > peakLevel_) {
            peakLevel_ = newLevel;
            peakHoldTimer_ = 30; // frames que se mantiene el peak
        } else if (peakHoldTimer_ > 0) {
            peakHoldTimer_--;
        } else {
            peakLevel_ += (-80.0f - peakLevel_) * 0.03f; // decay lento
        }

        if (std::abs(newLevel - lastDrawnLevel_) > 0.05f ||
            std::abs(peakLevel_ - lastDrawnPeak_) > 0.05f) {
            repaint();
            lastDrawnLevel_ = currentLevel_;
            lastDrawnPeak_ = peakLevel_;
        }
    }

    void setBarColour(juce::Colour col) { barColour_ = col; }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        auto w = bounds.getWidth();
        auto h = bounds.getHeight();

        // ─── Fondo oscuro ─────────────────────────────────────────────────
        g.setColour(juce::Colour(0xFF0A0A15));
        g.fillRoundedRectangle(bounds, 3.0f);

        // ─── Grid de referencia ────────────────────────────────────────────
        g.setColour(juce::Colour(0xFF1A1A2E).withAlpha(0.5f));
        float gridLevels[] = { -18.0f, -12.0f, -6.0f, 0.0f };
        for (float gridDb : gridLevels) {
            float gridY = levelToY(gridDb, bounds);
            g.drawHorizontalLine((int)gridY, bounds.getX() + 2, bounds.getRight() - 2);
        }

        // ─── Labels de referencia ──────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(7.0f)));
        g.setColour(juce::Colour(0xFF555566));
        for (float gridDb : gridLevels) {
            float gridY = levelToY(gridDb, bounds);
            g.drawText(juce::String((int)gridDb),
                       juce::Rectangle<float>(bounds.getX() + 2, gridY - 6, 16, 8),
                       juce::Justification::centredLeft);
        }

        // ─── Barra principal (gradiente verde → amarillo → rojo) ───────────
        float barTop = levelToY(currentLevel_, bounds);
        auto barRect = bounds.withTop(barTop);

        if (barRect.getHeight() > 1.0f) {
            juce::ColourGradient barGrad(
                getLevelColour(currentLevel_, 1.0f),
                juce::Point<float>(0.0f, barRect.getY()),
                getLevelColour(currentLevel_, 1.0f).withAlpha(0.3f),
                juce::Point<float>(0.0f, barRect.getBottom()),
                false);
            g.setGradientFill(barGrad);
            g.fillRoundedRectangle(barRect, 2.0f);

            // Brillito en la parte superior de la barra
            g.setColour(juce::Colours::white.withAlpha(0.15f));
            auto shineRect = barRect.withHeight(juce::jmax(2.0f, barRect.getHeight() * 0.1f));
            g.fillRoundedRectangle(shineRect, 1.0f);
        }

        // ─── Peak hold line ────────────────────────────────────────────────
        float peakY = levelToY(peakLevel_, bounds);
        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.fillRoundedRectangle(
            juce::Rectangle<float>(bounds.getX() + 2, peakY - 1.0f, bounds.getWidth() - 4, 2.5f),
            1.0f);

        // ─── Borde ─────────────────────────────────────────────────────────
        g.setColour(juce::Colour(0xFF2C2C3E));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

        // ─── Clip indicator (si > -0.5dB) ──────────────────────────────────
        if (currentLevel_ > -0.5f) {
            g.setColour(juce::Colour(0xFFE74C3C).withAlpha(0.6f));
            g.fillRoundedRectangle(bounds, 3.0f);
        }
    }

private:
    float currentLevel_ = -80.0f;
    float peakLevel_ = -80.0f;
    int peakHoldTimer_ = 0;
    float lastDrawnLevel_ = -80.0f;
    float lastDrawnPeak_ = -80.0f;
    juce::Colour barColour_{0xFF3498DB};

    float levelToY(float levelDb, juce::Rectangle<float> bounds) const
    {
        // Normalizar: -60dB → bottom, +6dB → top
        float norm = juce::jlimit(0.0f, 1.0f, (levelDb + 60.0f) / 66.0f);
        return bounds.getBottom() - norm * bounds.getHeight();
    }

    juce::Colour getLevelColour(float levelDb, float alpha) const
    {
        if (levelDb > -6.0f)
            return juce::Colour(0xFFE74C3C).withAlpha(alpha);    // rojo
        else if (levelDb > -12.0f)
            return juce::Colour(0xFFF39C12).withAlpha(alpha);    // amarillo
        else if (levelDb > -18.0f)
            return juce::Colour(0xFF2ECC71).withAlpha(alpha);    // verde claro
        else
            return barColour_.withAlpha(alpha * 0.5f);           // color tenue
    }
};

// ─── WaveformView — Mini waveform animado ─────────────────────────────────
class WaveformView : public juce::Component
{
public:
    WaveformView()
    {
        setOpaque(false);
        std::fill(waveData_.begin(), waveData_.end(), 0.0f);
    }

    void pushSample(float amplitude)
    {
        waveData_[writePos_] = juce::jlimit(-1.0f, 1.0f, amplitude);
        writePos_ = (writePos_ + 1) % kNumSamples;
        if (++samplesSinceUpdate_ > kDecimation) {
            repaint();
            samplesSinceUpdate_ = 0;
        }
    }

    void setWaveColour(juce::Colour col) { waveColour_ = col; }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        auto w = bounds.getWidth();
        auto h = bounds.getHeight();
        float centerY = bounds.getCentreY();

        // Fondo
        g.setColour(juce::Colour(0xFF0A0A15));
        g.fillRoundedRectangle(bounds, 3.0f);

        // Dibujar waveform
        if (hasSignal_) {
            juce::Path wavePath;
            bool first = true;

            for (int i = 0; i < kNumSamples; ++i) {
                int idx = (writePos_ + i) % kNumSamples;
                float x = bounds.getX() + (float)i / (float)kNumSamples * w;
                float sampleVal = waveData_[idx];

                if (first) {
                    wavePath.startNewSubPath(x, centerY - sampleVal * (h * 0.4f));
                    first = false;
                } else {
                    wavePath.lineTo(x, centerY - sampleVal * (h * 0.4f));
                }
            }

            g.setColour(waveColour_.withAlpha(0.8f));
            g.strokePath(wavePath, juce::PathStrokeType(1.2f));

            // Relleno suave debajo de la forma de onda
            juce::Path fillPath = wavePath;
            fillPath.lineTo(bounds.getRight(), centerY);
            fillPath.lineTo(bounds.getX(), centerY);
            fillPath.closeSubPath();
            g.setColour(waveColour_.withAlpha(0.1f));
            g.fillPath(fillPath);

            // Brillo cuando hay señal
            if (juce::jmax(currentRMS_, 0.0f) > 0.01f) {
                g.setColour(waveColour_.withAlpha(0.05f));
                g.fillRoundedRectangle(bounds, 3.0f);
            }
        } else {
            // Sin señal — línea plana con opacidad baja
            g.setColour(waveColour_.withAlpha(0.2f));
            g.drawHorizontalLine((int)centerY, bounds.getX(), bounds.getRight());
        }

        // Borde
        g.setColour(juce::Colour(0xFF2C2C3E));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
    }

    void setSignalPresent(bool hasSignal) { hasSignal_ = hasSignal; }
    void setRMS(float rms) { currentRMS_ = rms; }

private:
    static constexpr int kNumSamples = 128;
    static constexpr int kDecimation = 4;
    std::array<float, kNumSamples> waveData_{};
    int writePos_ = 0;
    int samplesSinceUpdate_ = 0;
    bool hasSignal_ = false;
    float currentRMS_ = 0.0f;
    juce::Colour waveColour_{0xFF3498DB};
};

// ─── ColourPresetStrip ──────────────────────────────────────────────────────
// 8 colores predefinidos de alta calidad para asignación rápida
class ColourPresetStrip : public juce::Component
{
public:
    std::function<void(juce::Colour)> onColourChosen;

    ColourPresetStrip()
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setOpaque(false);
    }

    void setActiveColour(juce::Colour col) { activeColour_ = col; repaint(); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        auto bounds = getLocalBounds().reduced(2);
        int numPresets = getNumPresets();
        float dotSize = (float)(bounds.getHeight() - 4);
        float spacing = (float)(bounds.getWidth() - 2) / (float)numPresets;
        float dotSpacing = juce::jmin(spacing, dotSize + 2.0f);
        float startX = bounds.getX() + ((float)bounds.getWidth() - dotSpacing * (float)(numPresets - 1) - dotSize) * 0.5f;

        for (int i = 0; i < numPresets; ++i) {
            float cx = startX + (float)i * dotSpacing + dotSize * 0.5f;
            float cy = bounds.getCentreY();
            float dx = e.x - cx;
            float dy = e.y - cy;
            if (dx * dx + dy * dy <= dotSize * dotSize * 0.25f) {
                if (onColourChosen)
                    onColourChosen(presetColours_[i]);
                break;
            }
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced(2);
        int numPresets = getNumPresets();
        float dotSize = (float)(bounds.getHeight() - 4);
        float spacing = (float)(bounds.getWidth() - 2) / (float)numPresets;
        float dotSpacing = juce::jmin(spacing, dotSize + 2.0f);
        float startX = bounds.getX() + ((float)bounds.getWidth() - dotSpacing * (float)(numPresets - 1) - dotSize) * 0.5f;

        for (int i = 0; i < numPresets; ++i) {
            float cx = startX + (float)i * dotSpacing;
            auto dotBounds = juce::Rectangle<float>(cx, (float)bounds.getCentreY() - dotSize * 0.5f,
                                                     dotSize, dotSize);

            bool isActive = (presetColours_[i].getARGB() == activeColour_.getARGB());

            // Sombra para el activo
            if (isActive) {
                g.setColour(juce::Colours::white.withAlpha(0.3f));
                g.fillEllipse(dotBounds.reduced(-2.0f));
            }

            // Círculo de color
            g.setColour(presetColours_[i]);
            g.fillEllipse(dotBounds.reduced(isActive ? 1.0f : 2.0f));

            // Brillo interno
            auto shineBounds = dotBounds.reduced(isActive ? 3.0f : 4.0f);
            shineBounds = shineBounds.withHeight(shineBounds.getHeight() * 0.4f);
            juce::ColourGradient shine(
                juce::Colours::white.withAlpha(0.3f),
                shineBounds.getCentreX(), shineBounds.getY(),
                juce::Colour(0x00000000),
                shineBounds.getCentreX(), shineBounds.getBottom(),
                false);
            g.setGradientFill(shine);
            g.fillEllipse(shineBounds);

            // Borde
            g.setColour(juce::Colour(0xFF2C2C3E).withAlpha(isActive ? 0.8f : 0.4f));
            g.drawEllipse(dotBounds.reduced(isActive ? 1.0f : 2.0f), 1.0f);
        }
    }

private:
    static const juce::Colour presetColours_[8];
    juce::Colour activeColour_{0xFF808080};

    int getNumPresets() const { return 8; }
};

const juce::Colour ColourPresetStrip::presetColours_[8] = {
    juce::Colour(0xFFE74C3C),  // Rojo     — Drums
    juce::Colour(0xFFE67E22),  // Naranja  — Percusión
    juce::Colour(0xFFF1C40F),  // Amarillo — Teclados
    juce::Colour(0xFF2ECC71),  // Verde    — Guitarras
    juce::Colour(0xFF1ABC9C),  // Turquesa — FX
    juce::Colour(0xFF3498DB),  // Azul     — Bass
    juce::Colour(0xFF9B59B6),  // Púrpura  — Voces
    juce::Colour(0xFFE91E63),  // Rosa     — Coros
};

// ─── ColourSwatch ──────────────────────────────────────────────────────────
class ColourSwatch : public juce::Component
{
public:
    std::function<void()> onClick;

    ColourSwatch()
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setOpaque(false);
    }

    void setColour(juce::Colour col) { colour_ = col; repaint(); }
    juce::Colour getColour() const { return colour_; }

    void mouseDown(const juce::MouseEvent&) override
    {
        if (onClick) onClick();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);

        g.setColour(colour_);
        g.fillRoundedRectangle(bounds, 4.0f);

        if (colour_.getBrightness() < 0.3f) {
            g.setColour(juce::Colours::white.withAlpha(0.15f));
            g.fillRoundedRectangle(bounds.reduced(2.0f), 2.0f);
        }

        g.setColour(juce::Colour(0xFF2C2C3E));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        if (isMouseOver()) {
            g.setColour(juce::Colours::white.withAlpha(0.3f));
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.drawText("\u25BC", bounds, juce::Justification::bottomRight);
        }
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override  { repaint(); }

private:
    juce::Colour colour_{0xFF808080};
};

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
    setSize(280, 340);

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
    auto area = getLocalBounds().reduced(6);

    // ─── Fila superior: icono + nombre + color ──────────────────────────────
    auto topRow = area.removeFromTop(30);
    iconLabel_.setBounds(topRow.removeFromLeft(28));
    auto colourArea = topRow.removeFromRight(34);
    colourSwatch_->setBounds(colourArea.reduced(3, 4));
    nameEditor_.setBounds(topRow.reduced(3, 1));

    // ─── Colour Preset Strip (justo debajo del nombre) ───────────────────────
    area.removeFromTop(2);
    auto presetRow = area.removeFromTop(18);
    presetRow.removeFromLeft(28); // alinear con el nombre
    colourPresetStrip_->setBounds(presetRow.reduced(2, 1));

    area.removeFromTop(2);

    // ─── Fila de estado ─────────────────────────────────────────────────────
    auto statusRow = area.removeFromTop(16);
    statusLabel_.setBounds(statusRow.removeFromLeft(120));
    area.removeFromTop(2);

    // ─── Fila de ruteo (bus selector) ───────────────────────────────────────
    auto routingRow = area.removeFromTop(20);
    auto bus = processorRef_.getBusAssignment();
    routingLabel_.setText("\xF0\x9F\x94\x97 Enviando a MixCoach" +
        (bus != BusType::None ? (juce::String(" [") + busNames[static_cast<int>(bus)] + "]") : juce::String()),
        juce::dontSendNotification);
    auto routingLabelArea = routingRow.removeFromLeft(140);
    routingLabel_.setBounds(routingLabelArea);
    busComboBox_.setBounds(routingRow.reduced(2, 1));

    area.removeFromTop(2);

    // ─── Labels de medidores (más compactos) ────────────────────────────────
    auto labelsRow = area.removeFromTop(14);
    levelLeftLabel_.setBounds(labelsRow.removeFromLeft(80));
    levelRightLabel_.setBounds(labelsRow.removeFromLeft(80));
    rmsLabel_.setBounds(labelsRow.removeFromLeft(80));
    correlationLabel_.setBounds(labelsRow);

    area.removeFromTop(2);

    // ─── VU Meters (más grandes, más protagonismo) ──────────────────────────
    auto metersRow = area.removeFromTop(110);
    auto leftMeterArea = metersRow.removeFromLeft(80);
    vuMeterLeft_->setBounds(leftMeterArea.reduced(1, 2));
    auto rightMeterArea = metersRow.removeFromLeft(80);
    vuMeterRight_->setBounds(rightMeterArea.reduced(1, 2));

    // ─── Waveform (ocupa el resto del espacio restante) ─────────────────────
    waveformView_->setBounds(metersRow.reduced(1, 2));

    area.removeFromTop(2);

    // ─── Barra de actividad/nivel general compacta ──────────────────────────
    // El waveform ocupa el ancho restante y altura completa
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
    if (slotIndex < 0) return;

    auto* sharedData = processorRef_.getSharedData();
    if (!sharedData) return;
    auto& registry = sharedData->getSlotRegistry();
    auto info = registry.getSlotInfo(slotIndex);

    if (!info.active) {            statusLabel_.setText("\xF0\x9F\x94\x87 Sin conexion al Brain", juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
        routingLabel_.setText("\xE2\x9D\x8C Sin ruteo", juce::dontSendNotification);
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
