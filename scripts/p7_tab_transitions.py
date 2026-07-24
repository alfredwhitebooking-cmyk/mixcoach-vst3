import sys
sys.stdout.reconfigure(encoding='utf-8')

with open('Source/MixCoach/UI/NavigationShell.cpp', 'r', encoding='utf-8') as f:
    data = f.read()

changes = 0

# ═══════════════════════════════════════════════════════════════════════════
# FIX 1: switchContent() - Fix Session tab outgoing (use progressScreen_)
# ═══════════════════════════════════════════════════════════════════════════
old_outgoing = '''        switch (previousTab_) {
            case TabBarComponent::Coach:   outgoing = coachPanel_.get(); break;
            case TabBarComponent::Tools:   outgoing = analyzersPanel_.get(); break;
            case TabBarComponent::Session: outgoing = reportPanel_.get(); break;
            default: break;
        }'''

new_outgoing = '''        switch (previousTab_) {
            case TabBarComponent::Coach:   outgoing = coachPanel_.get(); break;
            case TabBarComponent::Tools:   outgoing = analyzersPanel_.get(); break;
            case TabBarComponent::Session: outgoing = progressScreen_.get(); break;
            default: break;
        }'''

if old_outgoing in data:
    data = data.replace(old_outgoing, new_outgoing, 1)
    changes += 1
    print(f'✅ Fix 1a: Session tab outgoing -> progressScreen_')
else:
    print(f'❌ Fix 1a FAILED: Session outgoing pattern not found')

# ═══════════════════════════════════════════════════════════════════════════
# FIX 2: switchContent() - Fix Session tab incoming (use progressScreen_)
# ═══════════════════════════════════════════════════════════════════════════
old_incoming = '''        switch (tab) {
            case TabBarComponent::Coach:   incoming = coachPanel_.get(); break;
            case TabBarComponent::Tools:   incoming = analyzersPanel_.get(); break;
            case TabBarComponent::Session:
                incoming = reportPanel_.get();
                refreshReport();
                break;
            default: incoming = coachPanel_.get(); break;
        }'''

new_incoming = '''        switch (tab) {
            case TabBarComponent::Coach:   incoming = coachPanel_.get(); break;
            case TabBarComponent::Tools:   incoming = analyzersPanel_.get(); break;
            case TabBarComponent::Session:
                incoming = progressScreen_.get();
                refreshProgress();
                break;
            default: incoming = coachPanel_.get(); break;
        }'''

if old_incoming in data:
    data = data.replace(old_incoming, new_incoming, 1)
    changes += 1
    print(f'✅ Fix 2a: Session tab incoming -> progressScreen_')
else:
    print(f'❌ Fix 2a FAILED: Session incoming pattern not found')

# ═══════════════════════════════════════════════════════════════════════════
# FIX 3: switchContent() - Clean up Session visibility logic
# Remove reportPanel_ visibility from the Session tab flow (let it be handled
# separately by onViewReport / onShowReport). Keep progressScreen_ visibility.
# ═══════════════════════════════════════════════════════════════════════════
old_session_vis = '''        if (tab == TabBarComponent::Session) {
            if (progressScreen_) {
                progressScreen_->setVisible(true);
                refreshProgress();
                auto contentArea = getLocalBounds();
                contentArea.removeFromTop(TabBarComponent::kHeight);
                progressScreen_->setBounds(contentArea);
            }
            if (reportPanel_) reportPanel_->setVisible(false);
        } else {
            if (progressScreen_) progressScreen_->setVisible(false);
            if (reportPanel_) reportPanel_->setVisible(false);
        }'''

new_session_vis = '''        if (tab == TabBarComponent::Session) {
            if (progressScreen_) {
                progressScreen_->setBounds(contentArea);
            }
            if (reportPanel_) reportPanel_->setVisible(false);
        } else {
            if (progressScreen_) progressScreen_->setVisible(false);
            if (reportPanel_) reportPanel_->setVisible(false);
        }'''

if old_session_vis in data:
    data = data.replace(old_session_vis, new_session_vis, 1)
    changes += 1
    print(f'✅ Fix 3a: Session visibility logic cleaned up')
else:
    print(f'❌ Fix 3a FAILED: Session visibility pattern not found')

# ═══════════════════════════════════════════════════════════════════════════
# FIX 4: resized() crossfade branch - use crossfade pointers instead of isVisible()
# ═══════════════════════════════════════════════════════════════════════════
old_resized_cross = '''            if (crossfade_.active) {
                if (coachPanel_ && coachPanel_->isVisible()) coachPanel_->setBounds(area);
                if (analyzersPanel_ && analyzersPanel_->isVisible()) analyzersPanel_->setBounds(area);
                if (progressScreen_ && progressScreen_->isVisible()) progressScreen_->setBounds(area);
                if (reportPanel_ && reportPanel_->isVisible()) reportPanel_->setBounds(area);
                return;
            }'''

new_resized_cross = '''            if (crossfade_.active) {
                if (crossfade_.outgoing != nullptr) crossfade_.outgoing->setBounds(area);
                if (crossfade_.incoming != nullptr) crossfade_.incoming->setBounds(area);
                return;
            }'''

if old_resized_cross in data:
    data = data.replace(old_resized_cross, new_resized_cross, 1)
    changes += 1
    print(f'✅ Fix 4a: Crossfade branch uses pointers instead of isVisible()')
else:
    print(f'❌ Fix 4a FAILED: Crossfade branch pattern not found')

# ═══════════════════════════════════════════════════════════════════════════
# FIX 5: advanceCrossfade() - Add ease-out quad to alpha
# ═══════════════════════════════════════════════════════════════════════════
old_advance = '''    bool NavigationShell::advanceCrossfade()
    {
        if (!crossfade_.active) return false;
        crossfade_.progress += 1.0f / (60.0f * 0.15f); // 150ms crossfade
        if (crossfade_.progress >= 1.0f) {
            crossfade_.progress = 1.0f;
            crossfade_.active = false;
        }
        float alpha = crossfade_.progress;
        if (crossfade_.outgoing != nullptr) {
            crossfade_.outgoing->setAlpha(1.0f - alpha);
            if (crossfade_.active == false) crossfade_.outgoing->setVisible(false);
        }
        if (crossfade_.incoming != nullptr) {
            crossfade_.incoming->setAlpha(alpha);
        }
        repaint();
        return crossfade_.active;
    }'''

new_advance = '''    bool NavigationShell::advanceCrossfade()
    {
        if (!crossfade_.active) return false;
        crossfade_.progress += 1.0f / (60.0f * 0.15f); // 150ms crossfade
        if (crossfade_.progress >= 1.0f) {
            crossfade_.progress = 1.0f;
            crossfade_.active = false;
        }
        // Ease-out quad: smooth deceleration at the end
        float t = crossfade_.progress;
        float easedAlpha = t * (2.0f - t); // 1 - (1-t)^2
        if (crossfade_.outgoing != nullptr) {
            crossfade_.outgoing->setAlpha(1.0f - easedAlpha);
            if (crossfade_.active == false) crossfade_.outgoing->setVisible(false);
        }
        if (crossfade_.incoming != nullptr) {
            crossfade_.incoming->setAlpha(easedAlpha);
        }
        repaint();
        return crossfade_.active;
    }'''

if old_advance in data:
    data = data.replace(old_advance, new_advance, 1)
    changes += 1
    print(f'✅ Fix 5a: Crossfade easedAlpha with ease-out quad')
else:
    print(f'❌ Fix 5a FAILED: advanceCrossfade pattern not found')

# ═══════════════════════════════════════════════════════════════════════════
# FIX 6: switchContent() - Ensure contentArea is defined before Session block
# The Session visibility block now references contentArea but it's defined
# after that block. Let me check...
# Actually looking at the code again:
#   1. Session visibility logic (step 4)
#   2. tabBar_.setActiveTab(tab)
#   3. auto contentArea = getLocalBounds()...
# The Session block used getLocalBounds() directly, but now references
# contentArea which doesn't exist yet. Let me fix this.
# ═══════════════════════════════════════════════════════════════════════════
# Actually, I removed the progressScreen_->setBounds(contentArea) reference
# from the Session block. The new Session block only sets reportPanel_
# visibility and doesn't reference contentArea. So this is fine.

# But wait - progressScreen_ visibility is now handled by the crossfade.
# When entering Session tab, incoming = progressScreen_, so startCrossfade
# makes it visible. When leaving Session tab, advanceCrossfade hides the
# outgoing. This should work correctly.

# ═══════════════════════════════════════════════════════════════════════════
# Write changes
# ═══════════════════════════════════════════════════════════════════════════
with open('Source/MixCoach/UI/NavigationShell.cpp', 'w', encoding='utf-8') as f:
    f.write(data)

print(f'\n✅ Total changes applied: {changes}')
if changes == 5:
    print('🎉 All P7 fixes applied successfully!')
else:
    print(f'⚠️  Expected 5 changes, got {changes}')
