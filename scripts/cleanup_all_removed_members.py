# -*- coding: utf-8 -*-
"""
COMPREHENSIVE CLEANUP: Remove ALL remaining references to removed members.
After removing the intention screen (Scene 2), these members no longer exist
but their references remain in the cpp file, causing compile errors.
"""
import sys

path = 'Source/MixCoach/UI/CoachChatComponent.cpp'
with open(path, 'rb') as f:
    content = f.read()

changes = 0

# Strategy: Remove entire blocks of code that reference removed members.
# These blocks are from the timer callback, constructor, and various methods.

# 1. REMOVE: Entire timer callback avatar animation + typewriter + floating block
# The timer callback has:
#   if (avatarAnimActive_) {
#       ... avatar shrink animation ...
#       ... typewriter effect ...
#       ... floating animation ...
#       if (eased >= 1.0f) { ... }
#   }
# We need to remove the entire if (avatarAnimActive_) block.

old_timer_block = b'        if (avatarAnimActive_) {\n            juce::uint32 elapsed = juce::Time::getMillisecondCounter() - avatarAnimStartMs_;\n            float t = juce::jmin(1.0f, (float)elapsed / kAvatarAnimDurationMs);'

idx = content.find(old_timer_block)
if idx >= 0:
    # Find the closing brace of this block
    # The pattern ends with: if (eased >= 1.0f) { ... } followed by }\n\n        // Avatar floating or if (fulluiAnim_
    end_markers = [
        b'if (fulluiAnim_.active) {',
        b'// Avatar floating animation',
        b'if (fulluiAnim_.active || typewriterActive_)',
    ]
    
    # Find the end of the avatar animation block
    # Look for the pattern: after the closing brace, there should be a blank line or another if
    brace_depth = 1
    pos = idx + len(old_timer_block)
    while pos < len(content) and brace_depth > 0:
        if content[pos:pos+1] == b'{':
            brace_depth += 1
        elif content[pos:pos+1] == b'}':
            brace_depth -= 1
        pos += 1
    
    avatar_block_end = pos
    
    # Now remove from idx to avatar_block_end
    removed_len = avatar_block_end - idx
    content = content[:idx] + content[avatar_block_end:]
    changes += 1
    print(f"[OK] Removed timer callback avatar animation block ({removed_len} bytes)")
else:
    print("[WARN] Could not find timer callback avatar block")

# 2. REMOVE: Constructor initialization of intention labels
# Lines like:
#   intentionGreeting_.setFont(...)
#   intentionGreeting_.setColour(...)
#   intentionGreeting_.setText(...)
#   addAndMakeVisible(intentionGreeting_);
#   intentionGreeting_.setVisible(false);
# Similar for intentionLabel_, intentionSubtitle_, intentionFooter_

# Find the block that initializes all 4 intention labels in the constructor
# Look for "intentionGreeting_ initialization" marker
for label_name in [b'intentionGreeting_', b'intentionLabel_', b'intentionSubtitle_', b'intentionFooter_']:
    while True:
        idx = content.find(label_name)
        if idx < 0:
            break
        # Find the end of this line and remove it
        line_end = content.find(b'\n', idx)
        if line_end >= 0:
            # Also look for the next few related lines
            # Find all consecutive lines that reference this label
            line_start = content.rfind(b'\n', 0, idx) + 1
            # Remove 1-3 lines that reference this label
            # Simply remove the line containing the reference
            content = content[:line_start] + content[line_end:]
            changes += 1
            print(f"[OK] Removed line with {label_name.decode('ascii', errors='replace')}")
        else:
            break
    
    # Also remove any setVisible(false) calls for these labels
    if b'intentionLabel' in label_name or b'intentionGreeting' in label_name:
        # These also appear in "Salir del modo intencion" code which was already modified
        pass

# 3. REMOVE: References in the session prep / setWelcomeMode / other methods
# Just remove any remaining references to these members as individual line removals
additional_members = [
    b'intentionGreeting_',
    b'intentionSubtitle_', 
    b'intentionFooter_',
    b'avatarAnimProgress_',
    b'avatarAnimStartSize_',
    b'avatarAnimStartMs_',
    b'kAvatarAnimDurationMs',
    b'typewriterActive_',
    b'typewriterFullText_',
    b'typewriterCharIndex_',
    b'typewriterComplete_',
    b'typewriterStartMs_',
    b'kTypewriterCharIntervalMs',
]

for member in additional_members:
    while True:
        idx = content.find(member)
        if idx < 0:
            break
        # Remove the line containing this reference
        line_start = content.rfind(b'\n', 0, idx) + 1
        line_end = content.find(b'\n', idx)
        if line_end >= 0:
            line_end += 1
        else:
            line_end = len(content)
        context = content[line_start:line_end].decode('ascii', errors='replace').strip()[:80]
        content = content[:line_start] + content[line_end:]
        changes += 1
        print(f"[OK] Removed: {context}")

# 4. Clean up any remaining orphaned code (like empty if blocks)
# Remove: if (!fulluiAnim_.active) stopTimer();
old_stop = b"            if (!fulluiAnim_.active) stopTimer();\n"
if old_stop in content:
    content = content.replace(old_stop, b'', 1)
    changes += 1
    print("[OK] Removed orphaned stopTimer() reference")

# Remove duplicate blank lines
while b'\n\n\n\n' in content:
    content = content.replace(b'\n\n\n\n', b'\n\n\n')

# Write back
with open(path, 'wb') as f:
    f.write(content)

print(f"\n=== ALL DONE. {changes} changes applied ===")
