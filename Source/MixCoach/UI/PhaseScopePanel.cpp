#include "PhaseScopePanel.h"

namespace mixcoach {

PhaseScopePanel::PhaseScopePanel()
{
    headerLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x94\x84 Phase Scope"),
                         juce::dontSendNotification);
    headerLabel_.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
    headerLabel_.setJustificationType(juce::Justification::centredLeft);
    headerLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(headerLabel_);

    addAndMakeVisible(vectorscope_);
    addAndMakeVisible(phaseMeter_);
    addAndMakeVisible(crestHistogram_);
}

void PhaseScopePanel::setPhaseDiagnostics(const std::vector<PhaseDiagnostic>& diagnostics)
{
    if (diagnostics.empty()) {
        vectorscope_.setPhaseDiagnostic(nullptr);
        phaseMeter_.setPhaseDiagnostic(nullptr);
        return;
    }
    
    // Encontrar el más severo y pasarlo a ambos sub-componentes
    const PhaseDiagnostic* worst = &diagnostics[0];
    for (const auto& d : diagnostics) {
        if (d.severity > worst->severity)
            worst = &d;
    }
    
    vectorscope_.setPhaseDiagnostic(worst);
    phaseMeter_.setPhaseDiagnostic(worst);
}

void PhaseScopePanel::resized()
{
    auto area = getLocalBounds().reduced(4, 2);
    headerLabel_.setBounds(area.removeFromTop(14));

    int pcH = area.getHeight();
    int vecH    = pcH * 45 / 100;  // 45% vectorscope
    int phaseH  = pcH * 25 / 100;  // 25% phase correlation
    int crestH  = pcH - vecH - phaseH;  // 30% crest

    // Separadores verticales entre secciones (2px gap)
    vectorscope_.setBounds(area.removeFromTop(vecH).reduced(0, 1));
    phaseMeter_.setBounds(area.removeFromTop(phaseH).reduced(0, 1));
    crestHistogram_.setBounds(area.reduced(0, 1));
}

void PhaseScopePanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

    // ─── Separadores horizontales entre secciones ───────────────────────
    auto area = getLocalBounds().reduced(4, 2);
    area.removeFromTop(14);

    int pcH = area.getHeight();
    int vecH   = pcH * 45 / 100;
    int phaseH = pcH * 25 / 100;

    int y1 = area.getY() + vecH;
    int y2 = y1 + phaseH;

    g.setColour(MixCoachTheme::divider().withAlpha(0.18f));
    g.fillRect(area.getX() + 4, y1, area.getWidth() - 8, 1);
    g.fillRect(area.getX() + 4, y2, area.getWidth() - 8, 1);
}

bool PhaseScopePanel::advanceVisuals(double sampleRateHz, bool allowRepaint)
{
    bool dirty = false;
    dirty |= vectorscope_.advanceFrame(sampleRateHz, allowRepaint);
    dirty |= phaseMeter_.advanceFrame(sampleRateHz, allowRepaint);
    dirty |= crestHistogram_.advanceFrame(sampleRateHz, allowRepaint);
    return dirty;
}

} // namespace mixcoach
