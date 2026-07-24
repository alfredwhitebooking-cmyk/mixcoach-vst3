import sys

# ── Read NavigationShell.cpp ──
with open('Source/MixCoach/UI/NavigationShell.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# Add crossfade implementation before the closing namespace
old_end = '''void NavigationShell::refreshReport()
{
    wireReportPanel();
    reportPanel_->refresh();
}

} // namespace mixcoach'''

new_end = '''void NavigationShell::refreshReport()
{
    wireReportPanel();
    reportPanel_->refresh();
}

// ═══════════════════════════════════════════════════════════════════════════
//  startCrossfade \u2014 Inicia transici\u00f3n crossfade de 150ms
//  El panel outgoing fade out (alpha 1\u21920), incoming fade in (alpha 0\u21921).
//  El timer de 60fps avanza la animaci\u00f3n hasta completar.
// ═══════════════════════════════════════════════════════════════════════════
void NavigationShell::startCrossfade(juce::Component* from, juce::Component* to)
{
    if (from == to) {
        to->setAlpha(1.0f);
        to->setVisible(true);
        to->repaint();
        crossfade_.active = false;
        return;
    }

    // Si ya hay una animaci\u00f3n activa, finalizarla inmediatamente
    if (crossfade_.active) {
        if (crossfade_.outgoing != nullptr) {
            crossfade_.outgoing->setAlpha(1.0f);
            crossfade_.outgoing->setVisible(false);
        }
        if (crossfade_.incoming != nullptr) {
            crossfade_.incoming->setAlpha(1.0f);
        }
    }

    crossfade_.outgoing = from;
    crossfade_.incoming = to;
    crossfade_.progress = 0.0f;
    crossfade_.active   = true;

    // Configurar alphas iniciales
    if (from != nullptr) {
        from->setAlpha(1.0f);
        from->setVisible(true);
    }
    if (to != nullptr) {
        to->setAlpha(0.0f);
        to->setVisible(true);
        to->toFront(false);
    }

    // Iniciar timer 60fps para animaci\u00f3n
    startTimerHz(60);
}

// ═══════════════════════════════════════════════════════════════════════════
//  advanceCrossfade \u2014 Avanza la animaci\u00f3n crossfade 1 frame
//  Retorna true si la animaci\u00f3n a\u00fan est\u00e1 activa.
// ═══════════════════════════════════════════════════════════════════════════
bool NavigationShell::advanceCrossfade()
{
    if (!crossfade_.active) {
        stopTimer();
        return false;
    }

    const float kStep = 1.0f / (150.0f / 16.667f); // 150ms a 60fps \u2248 9 frames, step \u2248 0.111
    crossfade_.progress += kStep;

    if (crossfade_.progress >= 1.0f) {
        // Finalizar animaci\u00f3n
        crossfade_.progress = 1.0f;
        if (crossfade_.outgoing != nullptr) {
            crossfade_.outgoing->setAlpha(0.0f);
            crossfade_.outgoing->setVisible(false);
        }
        if (crossfade_.incoming != nullptr) {
            crossfade_.incoming->setAlpha(1.0f);
        }
        crossfade_.active = false;
        stopTimer();
        repaint();
        return false;
    }

    // Aplicar alphas
    float outAlpha = 1.0f - crossfade_.progress;
    float inAlpha  = crossfade_.progress;

    if (crossfade_.outgoing != nullptr)
        crossfade_.outgoing->setAlpha(outAlpha);
    if (crossfade_.incoming != nullptr)
        crossfade_.incoming->setAlpha(inAlpha);

    repaint();
    return true;
}

} // namespace mixcoach'''

content = content.replace(old_end, new_end)

# Now modify switchContent to use crossfade
old_switch_start = '''void NavigationShell::switchContent(SidebarComponent::Section section)
{
    // Hide all content panels
    dashboardScreen_->setVisible(false);
    mixMap_->setVisible(false);
    analyzersPanel_->setVisible(false);
    coachPanel_->setVisible(false);
    refVisualPanel_->setVisible(false);
    progressScreen_->setVisible(false);
    reportPanel_->setVisible(false);

    // Update sidebar active state
    sidebar_.setActiveSection(section);

    // Show the selected panel
    switch (section) {
        case SidebarComponent::Dashboard:
            dashboardScreen_->setVisible(true);
            break;
        case SidebarComponent::MixMap:
            mixMap_->setVisible(true);
            break;
        case SidebarComponent::Analysis:
            analyzersPanel_->setVisible(true);
            break;
        case SidebarComponent::Reference:
            refVisualPanel_->setVisible(true);
            break;
        case SidebarComponent::Coach:
            coachPanel_->setVisible(true);
            break;
        case SidebarComponent::Progress:
            progressScreen_->setVisible(true);
            break;
        case SidebarComponent::Report:
            reportPanel_->setVisible(true);
            refreshReport();
            break;
        default:
            dashboardScreen_->setVisible(true);
            break;
    }

    resized();
    repaint();
}'''

new_switch = '''void NavigationShell::switchContent(SidebarComponent::Section section)
{
    // Si hay crossfade activo, finalizarlo inmediatamente
    if (crossfade_.active) {
        if (crossfade_.outgoing != nullptr) {
            crossfade_.outgoing->setAlpha(1.0f);
            crossfade_.outgoing->setVisible(false);
        }
        if (crossfade_.incoming != nullptr) {
            crossfade_.incoming->setAlpha(1.0f);
        }
        crossfade_.active = false;
        stopTimer();
    }

    // Determinar el panel que est\u00e1 visible actualmente (outgoing)
    juce::Component* outgoing = nullptr;
    SidebarComponent::Section activeSection = sidebar_.getActiveSection();
    switch (activeSection) {
        case SidebarComponent::Dashboard: outgoing = dashboardScreen_.get(); break;
        case SidebarComponent::MixMap:    outgoing = mixMap_.get(); break;
        case SidebarComponent::Analysis:  outgoing = analyzersPanel_.get(); break;
        case SidebarComponent::Reference: outgoing = refVisualPanel_.get(); break;
        case SidebarComponent::Coach:     outgoing = coachPanel_.get(); break;
        case SidebarComponent::Progress:  outgoing = progressScreen_.get(); break;
        case SidebarComponent::Report:    outgoing = reportPanel_.get(); break;
        default: break;
    }

    // Determinar el panel de destino (incoming)
    juce::Component* incoming = nullptr;
    switch (section) {
        case SidebarComponent::Dashboard: incoming = dashboardScreen_.get(); break;
        case SidebarComponent::MixMap:    incoming = mixMap_.get(); break;
        case SidebarComponent::Analysis:  incoming = analyzersPanel_.get(); break;
        case SidebarComponent::Reference: incoming = refVisualPanel_.get(); break;
        case SidebarComponent::Coach:     incoming = coachPanel_.get(); break;
        case SidebarComponent::Progress:  incoming = progressScreen_.get(); break;
        case SidebarComponent::Report:
            incoming = reportPanel_.get();
            refreshReport();
            break;
        default:
            incoming = dashboardScreen_.get();
            break;
    }

    // Update sidebar active state
    sidebar_.setActiveSection(section);

    // Asegurar bounds correctos para ambos paneles
    auto area = getLocalBounds().removeFromLeft(SidebarComponent::kWidth);
    area = getLocalBounds().removeFromLeft(getWidth() - SidebarComponent::kWidth);
    if (outgoing != nullptr) outgoing->setBounds(area);
    if (incoming != nullptr) incoming->setBounds(area);

    // Iniciar crossfade transition
    startCrossfade(outgoing, incoming);
}'''

if old_switch_start in content:
    content = content.replace(old_switch_start, new_switch)
    print('switchContent: Updated successfully')
else:
    print('switchContent: Pattern not found!')
    idx = content.find('void NavigationShell::switchContent')
    if idx >= 0:
        print('Current switchContent at idx', idx)
        print(repr(content[idx:idx+150]))

with open('Source/MixCoach/UI/NavigationShell.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print('NavigationShell.cpp: Done')
