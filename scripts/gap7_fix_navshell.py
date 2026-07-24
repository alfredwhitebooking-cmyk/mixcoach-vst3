import sys
import os

os.chdir('C:\\Proyectos\\MixCoach')

# ── Read NavigationShell.cpp ──
with open('Source/MixCoach/UI/NavigationShell.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# ── 1. Rename void NavigationShell::refreshReport to include crossfade methods ──
# First check if the script already added crossfade code
if 'startCrossfade' in content:
    print('Crossfade code already present, skipping method addition')
else:
    print('Adding crossfade methods...')
    # This shouldn't happen but handle it
    old_end = '''void NavigationShell::refreshReport()
{
    wireReportPanel();
    reportPanel_->refresh();
}

} // namespace mixcoach'''
    # Write simpler - just add the crossfade
    new_end = '''void NavigationShell::refreshReport()
{
    wireReportPanel();
    reportPanel_->refresh();
}

void NavigationShell::timerCallback()
{
    advanceCrossfade();
}

void NavigationShell::startCrossfade(juce::Component* from, juce::Component* to)
{
    if (from == to) {
        to->setAlpha(1.0f);
        to->setVisible(true);
        to->repaint();
        crossfade_.active = false;
        return;
    }

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

    if (from != nullptr) {
        from->setAlpha(1.0f);
        from->setVisible(true);
    }
    if (to != nullptr) {
        to->setAlpha(0.0f);
        to->setVisible(true);
        to->toFront(false);
    }

    startTimerHz(60);
}

bool NavigationShell::advanceCrossfade()
{
    if (!crossfade_.active) {
        stopTimer();
        return false;
    }

    const float kStep = 1.0f / 9.0f; // ~150ms at 60fps
    crossfade_.progress += kStep;

    if (crossfade_.progress >= 1.0f) {
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
    if old_end in content:
        content = content.replace(old_end, new_end)
        print('Added crossfade methods successfully')
    else:
        print('Could not find end pattern!')
        idx = content.rfind('} // namespace mixcoach')
        print('Last 1000 chars:', repr(content[idx-1000:idx]))

# ── 2. Find and replace switchContent method ──
idx_start = content.find('void NavigationShell::switchContent')
if idx_start >= 0:
    # Find the end of the method (next void or end of file)
    rest = content[idx_start:]
    # Find the next method or the end of switchContent
    idx_next_method = rest.find('\nvoid ', 10)
    if idx_next_method < 0:
        idx_next_method = rest.find('\nbool ', 10)
    if idx_next_method < 0:
        idx_next_method = rest.find('\n} // namespace', 10)
    
    if idx_next_method >= 0:
        old_method = rest[:idx_next_method]
    else:
        # Fallback: find by counting braces
        brace_count = 0
        method_end = 0
        for i, ch in enumerate(rest):
            if ch == '{': brace_count += 1
            elif ch == '}': brace_count -= 1
            if brace_count == 0 and i > 0:
                method_end = i + 1
                break
        old_method = rest[:method_end]
    
    print(f'Found switchContent method ({len(old_method)} chars)')
    
    new_method = '''void NavigationShell::switchContent(SidebarComponent::Section section)
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

    // Determinar el panel visible actualmente (outgoing)
    juce::Component* outgoing = nullptr;
    SidebarComponent::Section activeSect = sidebar_.getActiveSection();
    switch (activeSect) {
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
    auto contentArea = getLocalBounds();
    contentArea.removeFromLeft(SidebarComponent::kWidth);
    if (outgoing != nullptr) outgoing->setBounds(contentArea);
    if (incoming != nullptr) incoming->setBounds(contentArea);

    // Iniciar crossfade transition
    startCrossfade(outgoing, incoming);
}

'''
    content = content.replace(old_method, new_method, 1)
    print('switchContent: Updated successfully')
else:
    print('switchContent method not found!')

with open('Source/MixCoach/UI/NavigationShell.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print('NavigationShell.cpp: Done')
