#include "CoachingEvidenceHost.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor
    // ═══════════════════════════════════════════════════════════════════════════
    CoachingEvidenceHost::CoachingEvidenceHost()
    {
        setOpaque(false);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setPanels — Registra los 6 paneles gestionados
    // ═══════════════════════════════════════════════════════════════════════════
    void CoachingEvidenceHost::setPanels(juce::Component& gainStaging,
                                          juce::Component& eq,
                                          juce::Component& compression,
                                          juce::Component& space,
                                          juce::Component& automation,
                                          juce::Component& masterCheck,
                                          juce::Component& evidence)
    {
        gainPanel_     = &gainStaging;
        eqPanel_       = &eq;
        compPanel_     = &compression;
        spacePanel_    = &space;
        autoPanel_     = &automation;
        masterPanel_   = &masterCheck;
        evidencePanel_ = &evidence;

        // Todos los paneles ya deben ser addChildAndVisible por el padre
        resized();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setActivePhase — Muestra el panel correspondiente + título
    // ═══════════════════════════════════════════════════════════════════════════
    void CoachingEvidenceHost::setActivePhase(CoachRoomState phase)
    {
        activePhase_ = phase;

        // Ocultar todos los paneles
        if (gainPanel_)    gainPanel_->setVisible(false);
        if (eqPanel_)      eqPanel_->setVisible(false);
        if (compPanel_)    compPanel_->setVisible(false);
        if (spacePanel_)   spacePanel_->setVisible(false);
        if (autoPanel_)    autoPanel_->setVisible(false);
        if (masterPanel_)  masterPanel_->setVisible(false);
        if (evidencePanel_) evidencePanel_->setVisible(true); // Siempre visible

        // Mostrar solo el panel activo
        auto* active = getActivePanel();
        if (active) active->setVisible(true);

        resized();
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  NOTA: Los dots de progreso están en PhaseProgressBar (NavigationShell top bar).
    //  No se duplican aquí. Esta clase solo muestra header + phase panel + evidence panel.
    // ═══════════════════════════════════════════════════════════════════════════

    // ═══════════════════════════════════════════════════════════════════════════
    //  getActivePanel — Panel correspondiente a la fase activa
    // ═══════════════════════════════════════════════════════════════════════════
    juce::Component* CoachingEvidenceHost::getActivePanel() const noexcept
    {
        switch (activePhase_) {
            case CoachRoomState::GainStaging: return gainPanel_;
            case CoachRoomState::EQ:          return eqPanel_;
            case CoachRoomState::Compression: return compPanel_;
            case CoachRoomState::Space:       return spacePanel_;
            case CoachRoomState::Automation:  return autoPanel_;
            case CoachRoomState::MasterCheck: return masterPanel_;
            default:                          return gainPanel_; // Fallback
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getPhaseTitle — Título de la fase actual
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String CoachingEvidenceHost::getPhaseTitle() const
    {
        switch (activePhase_) {
            case CoachRoomState::GainStaging: return "GAIN STAGING";
            case CoachRoomState::EQ:          return "EQ";
            case CoachRoomState::Compression: return "COMPRESION";
            case CoachRoomState::Space:       return "ESPACIO";
            case CoachRoomState::Automation:  return "AUTOMACION";
            case CoachRoomState::MasterCheck: return "MASTER CHECK";
            default:                          return "COACHING";
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getPhaseSubtitle — Subtítulo descriptivo de la fase
    // ═══════════════════════════════════════════════════════════════════════════
    juce::String CoachingEvidenceHost::getPhaseSubtitle() const
    {
        switch (activePhase_) {
            case CoachRoomState::GainStaging:
                return "Ajusta los niveles de cada pista";
            case CoachRoomState::EQ:
                return "Balance espectral y enmascaramiento";
            case CoachRoomState::Compression:
                return "Control de dinámica y crest factor";
            case CoachRoomState::Space:
                return "Imagen estéreo y profundidad";
            case CoachRoomState::Automation:
                return "Dinámica de secciones y automatización";
            case CoachRoomState::MasterCheck:
                return "Comparación final con referencia";
            default:
                return {};
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  getPhaseSubtitle — Subtítulo descriptivo de la fase
    // ═══════════════════════════════════════════════════════════════════════════


    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Fondo glass + header con título (sin dots — están en PhaseProgressBar)
    // ═══════════════════════════════════════════════════════════════════════════
    void CoachingEvidenceHost::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        float cr = 8.0f;

        // ─── Glass background ──────────────────────────────────────────────
        g.setColour(MixCoachTheme::bgPanel().withAlpha(0.85f));
        g.fillRoundedRectangle(bounds, cr);
        g.setColour(MixCoachTheme::border().withAlpha(0.15f));
        g.drawRoundedRectangle(bounds, cr, 0.5f);

        // ─── Header ────────────────────────────────────────────────────────
        auto headerArea = bounds.removeFromTop((float)kHeaderHeight).reduced(10, 6);
        g.setColour(MixCoachTheme::border().withAlpha(0.10f));
        g.fillRect(juce::Rectangle<float>(headerArea.getX(), (float)kHeaderHeight - 1,
                                            (float)(getWidth() - 20), 1.0f));

        // Phase icon + title
        juce::String icon;
        switch (activePhase_) {
            case CoachRoomState::GainStaging: icon = "\xE2\x96\xB2"; break; // ▲
            case CoachRoomState::EQ:          icon = "\xE2\x97\x89"; break; // ◉
            case CoachRoomState::Compression: icon = "\xE2\x96\xA0"; break; // ■
            case CoachRoomState::Space:       icon = "\xE2\x97\x8B"; break; // ○
            case CoachRoomState::Automation:  icon = "\xE2\x97\x86"; break; // ◆
            case CoachRoomState::MasterCheck: icon = "\xE2\x98\x85"; break; // ★
            default:                          icon = "\xE2\x96\xB8"; break; // ▸
        }

        g.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
        g.setColour(MixCoachTheme::textBright());
        g.drawText(icon + " " + getPhaseTitle(), headerArea.reduced(2, 0),
                   juce::Justification::centredLeft);

        // Subtitle (dimmed, right-aligned)
        auto subArea = headerArea;
        g.setFont(juce::Font(juce::FontOptions(8.0f)));
        g.setColour(MixCoachTheme::textDim().withAlpha(0.6f));
        g.drawText(getPhaseSubtitle(), subArea, juce::Justification::centredRight);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  resized — Header (44px) + body [phase panel 65%, evidence panel 35%]
    //  NOTA: Sin dots footer — los dots de progreso están en PhaseProgressBar (top bar).
    //
    //  IMPORTANTE: Los paneles de fase son hijos de MixCoachPanel (no de este host),
    //  por lo que sus bounds deben transformarse a coordenadas del padre sumando
    //  la posición del host. Sin esta transformación, los paneles se renderizarían
    //  en coordenadas locales del host (x=0, y=44) en vez del padre (x=getX(), y=getY()+44).
    // ═══════════════════════════════════════════════════════════════════════════
    void CoachingEvidenceHost::resized()
    {
        auto area = getLocalBounds();

        // ─── Header ────────────────────────────────────────────────────────
        area.removeFromTop(kHeaderHeight);
        area.removeFromTop(2); // Separator

        // ─── Body: phase panel (65%) + evidence panel (35%) ───────────────
        int phaseW = area.getWidth() * kSplitRatio / 100;
        auto phaseArea = area.removeFromLeft(phaseW);
        area.removeFromLeft(2); // Gap

        // ═══ Transformar coordenadas locales → coordenadas del padre ═══════
        // Los paneles son hijos de MixCoachPanel, NO de este host.
        // Sin esta transformación, setBounds(posición_local) los posicionaría
        // en (0,44) del padre en vez de (getX(), getY()+44).
        auto parentOffset = getPosition();

        auto* active = getActivePanel();
        if (active) {
            active->setBounds(phaseArea.translated(parentOffset.x, parentOffset.y));
        }

        if (evidencePanel_) {
            evidencePanel_->setBounds(area.translated(parentOffset.x, parentOffset.y));
        }
    }

} // namespace mixcoach
