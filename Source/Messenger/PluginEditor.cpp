#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "../Common/SharedData.h"

namespace mixcoach {

// ─── LevelBar ──────────────────────────────────────────────────────────────
class LevelBar : public juce::Component
{
public:
    LevelBar() { setOpaque(false); }

    void setLevel(float levelDb)
    {
        auto newLevel = juce::jlimit(-100.0f, 6.0f, levelDb);
        if (std::abs(newLevel - level_) > 0.1f) {
            level_ = newLevel;
            repaint();
        }
    }

    void setBarColour(juce::Colour col) { barColour_ = col; }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);

        g.setColour(juce::Colour(0xFF1A1A2E));
        g.fillRoundedRectangle(bounds, 3.0f);

        auto normalised = juce::jlimit(0.0f, 1.0f, (level_ + 60.0f) / 60.0f);
        auto barHeight = bounds.getHeight() * normalised;
        auto bar = bounds.withTop(bounds.getBottom() - barHeight);

        juce::Colour fillColour;
        if (level_ > -3.0f)
            fillColour = juce::Colour(0xFFE74C3C);
        else if (level_ > -12.0f)
            fillColour = juce::Colour(0xFFF39C12);
        else
            fillColour = barColour_;

        g.setColour(fillColour.withAlpha(0.85f));
        g.fillRoundedRectangle(bar, 2.0f);

        g.setColour(juce::Colour(0xFF2C2C3E));
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

        // Linea de 0dB
        g.setColour(juce::Colour(0xFFE74C3C).withAlpha(0.3f));
        g.drawHorizontalLine(bounds.getY() + 1, bounds.getX() + 2, bounds.getRight() - 2);
    }

private:
    float level_ = -100.0f;
    juce::Colour barColour_{0xFF3498DB};
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
    setSize(280, 250);
    setResizable(false, false);

    // Componentes personalizados
    colourSwatch_   = std::make_unique<ColourSwatch>();
    levelBarLeft_   = std::make_unique<LevelBar>();
    levelBarRight_  = std::make_unique<LevelBar>();
    levelBarLeft_->setBarColour(juce::Colour(0xFF3498DB));
    levelBarRight_->setBarColour(juce::Colour(0xFF2ECC71));

    // Icono
    iconLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x8E\xB5"), juce::dontSendNotification);
    iconLabel_.setFont(juce::Font(juce::FontOptions(22.0f)));
    iconLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(iconLabel_);

    // Editor de nombre
    nameEditor_.setText(processorRef_.getTrackName());
    nameEditor_.setFont(juce::Font(juce::FontOptions(15.0f)).boldened());
    nameEditor_.setJustification(juce::Justification::centredLeft);
    nameEditor_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF0F0F1A));
    nameEditor_.setColour(juce::TextEditor::textColourId, juce::Colour(0xFFE0E0E0));
    nameEditor_.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF2C2C3E));
    nameEditor_.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xFF3498DB));
    nameEditor_.setIndents(8, 4);
    nameEditor_.setBorder(juce::BorderSize<int>(2));
    nameEditor_.setInputRestrictions(32);
    nameEditor_.addListener(this);
    addAndMakeVisible(nameEditor_);

    // Color swatch
    currentColour_ = processorRef_.getTrackColour();
    colourSwatch_->setColour(currentColour_);
    colourSwatch_->onClick = [this]() { openColourSelector(); };
    addAndMakeVisible(colourSwatch_.get());

    // Listener de cambio de color
    colourListener_.onColourChanged = [this](juce::Colour newColour) {
        currentColour_ = newColour;
        colourSwatch_->setColour(newColour);
        processorRef_.setTrackColour(newColour);
        repaint();
    };

    // Estado
    statusLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x93\xA1 Activo"), juce::dontSendNotification);
    statusLabel_.setFont(juce::Font(juce::FontOptions(12.0f)));
    statusLabel_.setJustificationType(juce::Justification::centredLeft);
    statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF2ECC71));
    addAndMakeVisible(statusLabel_);

    routingLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x94\x97 Enviando a MixCoach"), juce::dontSendNotification);
    routingLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
    routingLabel_.setJustificationType(juce::Justification::centredLeft);
    routingLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF3498DB));
    addAndMakeVisible(routingLabel_);

    // Medidores
    levelLeftLabel_.setText("L: --.- dB", juce::dontSendNotification);
    levelLeftLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
    levelLeftLabel_.setJustificationType(juce::Justification::centredLeft);
    levelLeftLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
    addAndMakeVisible(levelLeftLabel_);

    levelRightLabel_.setText("R: --.- dB", juce::dontSendNotification);
    levelRightLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
    levelRightLabel_.setJustificationType(juce::Justification::centredLeft);
    levelRightLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
    addAndMakeVisible(levelRightLabel_);

    rmsLabel_.setText("RMS: --.- dB", juce::dontSendNotification);
    rmsLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
    rmsLabel_.setJustificationType(juce::Justification::centredLeft);
    rmsLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
    addAndMakeVisible(rmsLabel_);

    correlationLabel_.setText("Corr: --.--", juce::dontSendNotification);
    correlationLabel_.setFont(juce::Font(juce::FontOptions(11.0f)));
    correlationLabel_.setJustificationType(juce::Justification::centredLeft);
    correlationLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
    addAndMakeVisible(correlationLabel_);

    // Barras de nivel
    addAndMakeVisible(levelBarLeft_.get());
    addAndMakeVisible(levelBarRight_.get());

    // Timer ~20 fps (50ms intervalo)
    startTimer(50);
}

MessengerAudioProcessorEditor::~MessengerAudioProcessorEditor()
{
    stopTimer();
}

// ─── Layout ───────────────────────────────────────────────────────────────────

void MessengerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(8);

    // Fila superior: icono + nombre + color
    auto topRow = area.removeFromTop(34);
    iconLabel_.setBounds(topRow.removeFromLeft(30));
    auto colourArea = topRow.removeFromRight(38);
    colourSwatch_->setBounds(colourArea.reduced(4, 5));
    nameEditor_.setBounds(topRow.reduced(4, 2));

    area.removeFromTop(6);

    // Estado y ruteo
    auto statusRow = area.removeFromTop(18);
    statusLabel_.setBounds(statusRow.removeFromLeft(140));
    routingLabel_.setBounds(statusRow);

    area.removeFromTop(4);

    // Labels de medidores
    auto labelsRow = area.removeFromTop(16);
    levelLeftLabel_.setBounds(labelsRow.removeFromLeft(95));
    levelRightLabel_.setBounds(labelsRow.removeFromLeft(95));
    rmsLabel_.setBounds(labelsRow);

    area.removeFromTop(4);

    // Barras de nivel
    auto barsRow = area.removeFromTop(90);
    auto leftBarArea = barsRow.removeFromLeft(65);
    levelBarLeft_->setBounds(leftBarArea.reduced(2, 2));
    auto rightBarArea = barsRow.removeFromLeft(65);
    levelBarRight_->setBounds(rightBarArea.reduced(2, 2));

    // Correlacion
    correlationLabel_.setBounds(barsRow.removeFromTop(18));
}

// ─── Paint ────────────────────────────────────────────────────────────────────

void MessengerAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    juce::ColourGradient bgGrad(
        juce::Colour(0xFF1A1A2E),
        juce::Point<float>(0.0f, 0.0f),
        juce::Colour(0xFF0F0F1A),
        juce::Point<float>(0.0f, (float)bounds.getHeight()),
        false);
    g.setGradientFill(bgGrad);
    g.fillRect(bounds);

    g.setColour(juce::Colour(0xFF2C2C3E));
    g.drawRect(bounds, 1);

    g.setColour(juce::Colour(0xFF2C2C3E).withAlpha(0.5f));
    g.drawHorizontalLine(42, 8.0f, (float)(bounds.getWidth() - 8));

    g.setColour(currentColour_.withAlpha(0.15f));
    g.fillRect(0, 0, 3, bounds.getHeight());
}

// ─── TextEditor callback ──────────────────────────────────────────────────────

void MessengerAudioProcessorEditor::textEditorTextChanged(juce::TextEditor& editor)
{
    if (&editor == &nameEditor_) {
        auto newName = editor.getText().trim();
        if (newName.isNotEmpty())
            processorRef_.setTrackName(newName);
    }
}

// ─── Timer callback ───────────────────────────────────────────────────────────

void MessengerAudioProcessorEditor::timerCallback()
{
    auto slotIndex = processorRef_.getSlotIndex();
    if (slotIndex < 0) return;

    auto& registry = processorRef_.getSharedData().getSlotRegistry();
    auto info = registry.getSlotInfo(slotIndex);

    if (!info.active) {
        statusLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x94\x87 Inactivo"), juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
        routingLabel_.setText("\u26D4 Sin ruteo", juce::dontSendNotification);
        routingLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
        return;
    }

    auto latest = registry.getTelemetry(slotIndex).latest();

    bool hasSignal = (latest.peakLeft > -60.0f || latest.peakRight > -60.0f);
    if (hasSignal) {
        statusLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x93\xA1 Activo"), juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF2ECC71));
    } else {
        statusLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x94\x87 Silencio"), juce::dontSendNotification);
        statusLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFFF39C12));
    }

    if (hasSignal && slotIndex >= 0) {
        routingLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x94\x97 Enviando a MixCoach"), juce::dontSendNotification);
        routingLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF3498DB));
    } else {
        routingLabel_.setText("\u26D4 Sin ruteo", juce::dontSendNotification);
        routingLabel_.setColour(juce::Label::textColourId, juce::Colour(0xFF888888));
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
    correlationLabel_.setText("Corr: " + corrIcon + " " +
                              juce::String(lastCorrelation_, 2),
                              juce::dontSendNotification);
    correlationLabel_.setColour(juce::Label::textColourId, corrColour);

    levelBarLeft_->setLevel(lastPeakLeft_);
    levelBarRight_->setLevel(lastPeakRight_);
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
