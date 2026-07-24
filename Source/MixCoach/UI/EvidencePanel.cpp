#include "EvidencePanel.h"

namespace mixcoach {

EvidencePanel::EvidencePanel()
{
    // ─── Título ───────────────────────────────────────────────────────
    titleLabel_.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::accent());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel_);

    // ─── Botón cerrar ─────────────────────────────────────────────────
    closeButton_.setButtonText("✕");
    closeButton_.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeButton_.setColour(juce::TextButton::textColourOnId, MixCoachTheme::textDim());
    closeButton_.setColour(juce::TextButton::textColourOffId, MixCoachTheme::textDim().withAlpha(0.5f));
    closeButton_.onClick = [this]() { closeEvidence(); };
    addAndMakeVisible(closeButton_);

    // ─── 4 analizadores — todos ocultos inicialmente ─────────────────
    addAndMakeVisible(levelMeter_);
    addAndMakeVisible(phaseMeter_);
    addAndMakeVisible(spectrumAnalyzer_);
    addAndMakeVisible(targetMarkers_);
    hideAllAnalyzers();

    setVisible(false);
}

EvidencePanel::~EvidencePanel()
{
    if (analyzerManager_ != nullptr)
        analyzerManager_->onChanged = nullptr;
}

void EvidencePanel::setAnalyzerManager(AnalyzerManager* manager)
{
    if (analyzerManager_ != nullptr)
        analyzerManager_->onChanged = nullptr;

    analyzerManager_ = manager;

    if (analyzerManager_ != nullptr) {
        analyzerManager_->onChanged = [this]() {
            onManagerChanged();
        };
    }

    refreshFromManager();
}

void EvidencePanel::refreshFromManager()
{
    if (analyzerManager_ == nullptr || !analyzerManager_->isOpen()) {
        setVisible(false);
        hideAllAnalyzers();
        return;
    }

    showAnalyzer(analyzerManager_->getCurrentType(),
                 analyzerManager_->getCurrentParams());
}

void EvidencePanel::closeEvidence()
{
    if (analyzerManager_ != nullptr)
        analyzerManager_->close();
    else {
        setVisible(false);
        hideAllAnalyzers();
    }
}

void EvidencePanel::onManagerChanged()
{
    refreshFromManager();
}

void EvidencePanel::hideAllAnalyzers()
{
    levelMeter_.setVisible(false);
    phaseMeter_.setVisible(false);
    spectrumAnalyzer_.setVisible(false);
    targetMarkers_.setVisible(false);
    titleLabel_.setText({}, juce::dontSendNotification);
}

void EvidencePanel::showAnalyzer(AnalyzerType type, const AnalyzerParams& params)
{
    hideAllAnalyzers();
    setVisible(true);

    titleLabel_.setText(params.title.isEmpty()
                        ? AnalyzerManager::getDisplayName(type)
                        : params.title,
                        juce::dontSendNotification);

    switch (type) {
        case AnalyzerType::LevelMeter:
            levelMeter_.updateLevel(params.levelDb);
            levelMeter_.setTrackLabel(params.trackName);
            levelMeter_.setVisible(true);
            break;

        case AnalyzerType::PhaseMeter:
            phaseMeter_.updateCorrelation(params.correlation);
            phaseMeter_.setVisible(true);
            break;

        case AnalyzerType::Spectrum:
            spectrumAnalyzer_.setTitle(params.title);
            if (params.highlightFreq > 0.0f)
                spectrumAnalyzer_.setHighlightFreq(params.highlightFreq, params.highlightGain);
            spectrumAnalyzer_.setVisible(true);
            break;

        case AnalyzerType::TargetMarkers:
            targetMarkers_.clearTargets();
            for (const auto& t : params.targets)
                targetMarkers_.setTarget(std::get<0>(t), std::get<1>(t), std::get<2>(t));
            if (!params.summary.isEmpty())
                targetMarkers_.setSummary(params.summary);
            targetMarkers_.setVisible(true);
            break;

        default:
            setVisible(false);
            break;
    }

    resized();
    repaint();
}

void EvidencePanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float cr = 6.0f;

    // ─── Sombra ───────────────────────────────────────────────────────
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.fillRoundedRectangle(bounds.expanded(1.0f, 1.0f), cr + 1.0f);

    // ─── Fondo oscuro semitransparente ────────────────────────────────
    g.setColour(MixCoachTheme::bgPanel().withAlpha(0.95f));
    g.fillRoundedRectangle(bounds, cr);

    // ─── Borde accent ─────────────────────────────────────────────────
    g.setColour(MixCoachTheme::accent().withAlpha(0.3f));
    g.drawRoundedRectangle(bounds, cr, 1.0f);

    // ─── Glass highlight superior ─────────────────────────────────────
    auto glassArea = bounds.withHeight(bounds.getHeight() * 0.3f);
    juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(0.04f),
                                    glassArea.getX(), glassArea.getY(),
                                    juce::Colour(0x00000000),
                                    glassArea.getX(), glassArea.getBottom(),
                                    false);
    g.setGradientFill(glassGrad);
    g.fillRoundedRectangle(glassArea, cr);
}

void EvidencePanel::resized()
{
    auto bounds = getLocalBounds().reduced(6, 4);

    // ─── Header: título + botón cerrar ────────────────────────────────
    auto headerArea = bounds.removeFromTop(22);
    titleLabel_.setBounds(headerArea.removeFromLeft(headerArea.getWidth() - 30).reduced(2, 0));
    closeButton_.setBounds(headerArea.reduced(2, 2));

    // ─── Área del analizador activo ───────────────────────────────────
    bounds.removeFromTop(4);
    auto analyzerArea = bounds.reduced(2, 0);

    levelMeter_.setBounds(analyzerArea);
    phaseMeter_.setBounds(analyzerArea);
    spectrumAnalyzer_.setBounds(analyzerArea);
    targetMarkers_.setBounds(analyzerArea);
}

} // namespace mixcoach
