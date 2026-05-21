#include "PluginEditor.h"

namespace mixcoach {

MixCoachAudioProcessorEditor::MixCoachAudioProcessorEditor(MixCoachAudioProcessor& processor, SharedData& sharedData)
    : AudioProcessorEditor(&processor)
    , processorRef_(processor)
    , sharedData_(sharedData)
    , tabbedComponent_(processor, sharedData)
{
    setSize(900, 600);
    setResizable(true, true);
    setResizeLimits(700, 450, 1600, 1200);

    addAndMakeVisible(tabbedComponent_);

    // Timer para actualizar UI periódicamente (30fps)
    startTimerHz(30);
}

MixCoachAudioProcessorEditor::~MixCoachAudioProcessorEditor()
{
    stopTimer();
}

void MixCoachAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Header
    auto headerBounds = area.removeFromTop(48);
    auto statusBounds = area.removeFromBottom(24);

    tabbedComponent_.setBounds(area);
}

void MixCoachAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(MixCoachTheme::bgDarker());

    auto area = getLocalBounds();
    auto headerBounds = area.removeFromTop(48);

    // Header con degradado
    juce::ColourGradient grad(
        MixCoachTheme::accent().withAlpha(0.15f),
        juce::Point<float>(0.0f, 0.0f),
        MixCoachTheme::accent().withAlpha(0.0f),
        juce::Point<float>(0.0f, (float)headerBounds.getHeight()),
        false);
    g.setGradientFill(grad);
    g.fillRect(headerBounds);

    // Logo / título
    g.setColour(MixCoachTheme::textPrimary());
    g.setFont(juce::Font(juce::FontOptions(22.0f)).boldened());
    g.drawText("🎛  MixCoach", headerBounds.removeFromLeft(200), juce::Justification::centredLeft);

    g.setColour(MixCoachTheme::textDim());
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText("Sistema Inteligente de Mentoría para Mezcla",
               headerBounds.removeFromLeft(300), juce::Justification::centredLeft);

    // Línea separadora
    auto statusBounds = area.removeFromBottom(24);
    g.setColour(MixCoachTheme::border());
    g.drawHorizontalLine(statusBounds.getY(), 0.0f, (float)getWidth());
}

void MixCoachAudioProcessorEditor::timerCallback()
{
    // Actualizar datos de telemetría de los slots activos
    auto& registry = sharedData_.getSlotRegistry();
    auto& analyzer = processorRef_.getAudioAnalyzer();

    registry.forEachActive([&](const SlotInfo& info) {
        auto& telem = registry.getTelemetry(info.slotIndex);
        auto latest = telem.latest();
        if (latest.active) {
            // Actualizar dashboard y analizadores con los datos más recientes
            auto& dashboard = tabbedComponent_.getDashboard();
            auto& analyzers = tabbedComponent_.getAnalyzersPanel();

            analyzers.updateLevels(latest.peakLeft, latest.peakRight);
            analyzers.updatePhase(latest.correlation);
            analyzers.updateSpectrum(latest.spectrum, 256);
        }
    });

    // Refrescar UI
    repaint();
}

} // namespace mixcoach
