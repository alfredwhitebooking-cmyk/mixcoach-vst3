# -*- coding: utf-8 -*-
"""
Add a guard at the start of resized() to prevent FullUI from being shown
when welcomeMode_ is true (before setShowIntentionScreen is called).

Also fix the intention mode block to use setFullUIPanelsVisible(false)
instead of setFullUIPanelsAlpha(0.0f).
"""
path = 'Source/MixCoach/UI/CoachChatComponent.cpp'
with open(path, 'rb') as f:
    content = f.read()

count = 0

# 1. Add welcome mode guard after "auto area = getLocalBounds().reduced(6);"
# The current code has:
#   auto area = getLocalBounds().reduced(6);
#
#   // ===========================================================
#   // ===========================================================
#   //  MODO INTENCION ...
#
# We need to insert the guard BEFORE the MODO INTENCION comment.

old_after_area = b'        auto area = getLocalBounds().reduced(6);\n\n        // ===========================================================\n        // ===========================================================\n        //  MODO INTENCION'

new_after_area = b'        auto area = getLocalBounds().reduced(6);\n\n        // ===========================================================\n        //  GUARD: No mostrar FullUI si estamos en welcome mode\n        //  (NavigationShell llama setWelcomeMode(true) antes de\n        //  setShowIntentionScreen(true), y entre medio NO queremos\n        //  que resized() muestre el FullUI)\n        // ===========================================================\n        if (welcomeMode_ && !showIntentionScreen_ && !showSessionPrepCard_) {\n            setFullUIPanelsVisible(false);\n            tracksSectionLabel_.setVisible(false);\n            footerModeLabel_.setVisible(false);\n            footerPhaseLabel_.setVisible(false);\n            footerGenreLabel_.setVisible(false);\n            footerTargetLabel_.setVisible(false);\n            footerSampleRateLabel_.setVisible(false);\n            footerExpLevelLabel_.setVisible(false);\n            return;\n        }\n\n        // ===========================================================\n        // ===========================================================\n        //  MODO INTENCION'

if old_after_area in content:
    content = content.replace(old_after_area, new_after_area, 1)
    count += 1
    print("[OK] Added welcome mode guard")
else:
    print("[WARN] Could not find anchor for welcome guard")

# 2. Fix intention mode: replace setFullUIPanelsAlpha(0.0f) with setFullUIPanelsVisible(false)
old_intention_alpha = b'            // Ocultar paneles FullUI\n            setFullUIPanelsAlpha(0.0f);\n            tracksSectionLabel_.setVisible(false);\n            footerModeLabel_.setVisible(false);\n            footerPhaseLabel_.setVisible(false);\n            footerGenreLabel_.setVisible(false);\n            footerTargetLabel_.setVisible(false);\n            footerSampleRateLabel_.setVisible(false);\n            footerExpLevelLabel_.setVisible(false);'

new_intention_visible = b'            // Ocultar paneles FullUI\n            setFullUIPanelsVisible(false);\n            tracksSectionLabel_.setVisible(false);\n            footerModeLabel_.setVisible(false);\n            footerPhaseLabel_.setVisible(false);\n            footerGenreLabel_.setVisible(false);\n            footerTargetLabel_.setVisible(false);\n            footerSampleRateLabel_.setVisible(false);\n            footerExpLevelLabel_.setVisible(false);'

if old_intention_alpha in content:
    content = content.replace(old_intention_alpha, new_intention_visible, 1)
    count += 1
    print("[OK] Fixed intention mode: setFullUIPanelsVisible instead of setAlpha")
else:
    print("[WARN] Could not find intention mode alpha pattern")

with open(path, 'wb') as f:
    f.write(content)

print(f"\nDone. {count} replacements applied.")
