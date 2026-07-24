# -*- coding: utf-8 -*-
"""
Revert the avatar floating animation fix - the original code was correct.
The easing recalculates Y from scratch each frame, so there's no accumulation.
Remove avatarAnimBaseY_ from header and restore original floating code.
"""
import sys

# === FIX 1: Restore original floating animation code in cpp ===
cpp_path = 'Source/MixCoach/UI/CoachChatComponent.cpp'
with open(cpp_path, 'rb') as f:
    cpp_content = f.read()

# The current (broken) code:
#   // Avatar floating animation (fixed: no accumulation)
#   float floatOffset = std::sin((float)elapsed * 0.003f) * 3.0f;
#   auto ab = avatar_.getBounds();
#   // Restore base Y before adding offset (undo previous frame offset)
#   int baseY = avatarAnimBaseY_;
#   avatar_.setBounds(ab.getX(), baseY + (int)floatOffset, ab.getWidth(), ab.getHeight());
#
# Restore to original:
#   // Avatar floating animation
#   avatarFloatOffset_ = std::sin((float)elapsed * 0.003f) * 3.0f;
#   auto ab = avatar_.getBounds();
#   avatar_.setBounds(ab.getX(), ab.getY() + (int)avatarFloatOffset_, ab.getWidth(), ab.getHeight());

old_float = b'            // Avatar floating animation (fixed: no accumulation)\n            float floatOffset = std::sin((float)elapsed * 0.003f) * 3.0f;\n            auto ab = avatar_.getBounds();\n            // Restore base Y before adding offset (undo previous frame offset)\n            int baseY = avatarAnimBaseY_;\n            avatar_.setBounds(ab.getX(), baseY + (int)floatOffset, ab.getWidth(), ab.getHeight());'

new_float = b'            // Avatar floating animation\n            avatarFloatOffset_ = std::sin((float)elapsed * 0.003f) * 3.0f;\n            auto ab = avatar_.getBounds();\n            avatar_.setBounds(ab.getX(), ab.getY() + (int)avatarFloatOffset_, ab.getWidth(), ab.getHeight());'

if old_float in cpp_content:
    cpp_content = cpp_content.replace(old_float, new_float, 1)
    print("[OK] Reverted avatar floating animation to original code")
else:
    print("[WARN] Could not find the broken floating code in cpp")
    # Search for the pattern
    idx = cpp_content.find(b'avatarAnimBaseY_')
    if idx >= 0:
        snippet = cpp_content[max(0,idx-50):idx+100]
        print(f"  Found avatarAnimBaseY_ at byte {idx}: {repr(snippet)}")
    else:
        print("  avatarAnimBaseY_ not found in cpp - may have been compiled successfully?")

# Also revert the avatarAnimBaseY_ initialization in startIntentionAnimation
# Change: avatarAnimBaseY_ = (int)sy; -> (remove this line)
old_init = b'avatar_.setBounds((int)sx, (int)sy, (int)avatarAnimStartSize_, (int)avatarAnimStartSize_);\n        avatarAnimBaseY_ = (int)sy;\n\n        // Clear text'
new_init = b'avatar_.setBounds((int)sx, (int)sy, (int)avatarAnimStartSize_, (int)avatarAnimStartSize_);\n\n        // Clear text'

if old_init in cpp_content:
    cpp_content = cpp_content.replace(old_init, new_init, 1)
    print("[OK] Reverted avatarAnimBaseY_ initialization in startIntentionAnimation")
else:
    print("[WARN] Could not find avatarAnimBaseY_ initialization")

with open(cpp_path, 'wb') as f:
    f.write(cpp_content)

# === FIX 2: Remove avatarAnimBaseY_ from header ===
h_path = 'Source/MixCoach/UI/CoachChatComponent.h'
with open(h_path, 'rb') as f:
    h_content = f.read()

old_h = b'        float avatarFloatOffset_     = 0.0f;\n        int avatarAnimBaseY_          = 0;\n        static constexpr float kAvatarAnimDurationMs = 600.0f;'
new_h = b'        float avatarFloatOffset_     = 0.0f;\n        static constexpr float kAvatarAnimDurationMs = 600.0f;'

if old_h in h_content:
    h_content = h_content.replace(old_h, new_h, 1)
    print("[OK] Removed avatarAnimBaseY_ from header")
else:
    print("[WARN] Could not find avatarAnimBaseY_ in header")

with open(h_path, 'wb') as f:
    f.write(h_content)

print("\nDone.")
