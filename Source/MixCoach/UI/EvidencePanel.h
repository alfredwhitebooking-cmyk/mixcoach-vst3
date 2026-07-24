#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/AnalyzerManager.h"
#include "../Analyzers/LevelMeter.h"
#include "../Analyzers/PhaseMeter.h"
#include "../Analyzers/SpectrumAnalyzer.h"
#include "../Analyzers/TargetMarkers.h"
#include "MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  EvidencePanel — Panel que muestra el analizador activo.
//
//  El Coach abre un analizador (vía AnalyzerManager) y este panel lo
//  renderiza. Solo un analizador visible a la vez. El panel se posiciona
//  como overlay en la UI del Coach.
//
//  Uso:
//    evidencePanel.setAnalyzerManager(&manager);
//    // manager.open(AnalyzerType::Spectrum, params); → panel reacciona
//    addAndMakeVisible(evidencePanel);
// ═══════════════════════════════════════════════════════════════════════════
class EvidencePanel : public juce::Component
{
public:
    EvidencePanel();
    ~EvidencePanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Conecta el panel al AnalyzerManager (puede ser nullptr). */
    void setAnalyzerManager(AnalyzerManager* manager);

    /** Fuerza actualización del analizador visible según el manager. */
    void refreshFromManager();

    /** Cierra el analizador y se oculta. */
    void closeEvidence();

    /** ¿Hay evidencia visible ahora? */
    bool isShowingEvidence() const noexcept { return isVisible() && analyzerManager_ != nullptr && analyzerManager_->isOpen(); }

private:
    AnalyzerManager* analyzerManager_ = nullptr;

    // Los 4 analizadores — solo uno visible a la vez
    LevelMeter levelMeter_;
    PhaseMeter phaseMeter_;
    SpectrumAnalyzer spectrumAnalyzer_;
    TargetMarkers targetMarkers_;

    juce::Label titleLabel_;
    juce::TextButton closeButton_;

    void showAnalyzer(AnalyzerType type, const AnalyzerParams& params);
    void hideAllAnalyzers();
    void onManagerChanged();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EvidencePanel)
};

} // namespace mixcoach
