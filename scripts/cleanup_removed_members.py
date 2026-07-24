# -*- coding: utf-8 -*-
"""
Find and fix remaining references to removed members in CoachChatComponent.cpp.
The intention screen refactor removed several member variables and methods,
but the cpp file still references them in:
1. Constructor initialization
2. Timer callback
3. Various methods

This script finds and removes/fixes these references.
"""
path = 'Source/MixCoach/UI/CoachChatComponent.cpp'
with open(path, 'rb') as f:
    content = f.read()

changed = False

# List of patterns to search for (members that were removed)
removed_members = [
    b'intentionGreeting_',
    b'intentionLabel_',
    b'intentionSubtitle_',
    b'intentionFooter_',
    b'showIntentionScreen_',
    b'avatarAnimActive_',
    b'avatarAnimProgress_',
    b'avatarAnimStartSize_',
    b'avatarAnimStartMs_',
    b'avatarFloatOffset_',
    b'kAvatarAnimDurationMs',
    b'typewriterActive_',
    b'typewriterFullText_',
    b'typewriterCharIndex_',
    b'typewriterComplete_',
    b'typewriterStartMs_',
    b'kTypewriterCharIntervalMs',
]

found_any = False
for member in removed_members:
    count = content.count(member)
    if count > 0:
        print(f"  STILL EXISTS: {member.decode('ascii', errors='replace')} ({count} occurrences)")
        found_any = True

if not found_any:
    print("  No more references to removed members found!")

# Check for the timer callback typewriter block
# The timer callback at ~line 2396 contains typewriter animation code
# We need to remove the entire typewriter block
typewriter_marker = b'// Typewriter effect: reveal label char by char'
idx = content.find(typewriter_marker)
if idx >= 0:
    # Find the end of the typewriter block (look for the next section comment or closing brace)
    end_marker = b'// Avatar floating animation'
    end_idx = content.find(end_marker, idx)
    if end_idx >= 0:
        # Remove from typewriter start to just before floating animation
        removed = content[idx:end_idx]
        content = content[:idx] + content[end_idx:]
        print(f"  Removed typewriter block ({len(removed)} bytes)")
        changed = True
    else:
        print("  Found typewriter marker but could not find end")

# Also check for the avatar animation completion code
# When animation completes, it references removed members
completion_marker = b'avatarAnimActive_ = false;'
idx = content.find(completion_marker)
if idx >= 0:
    print(f"  Found avatarAnimActive_ = false at byte {idx}")

# Check if the setWelcomeMode simplified version still references showIntentionScreen_
idx = content.find(b'showIntentionScreen_')
if idx >= 0:
    print(f"  Found showIntentionScreen_ at byte {idx}")
    # Show context
    start = max(0, idx - 50)
    end = min(len(content), idx + 100)
    print(f"  Context: {repr(content[start:end])}")
else:
    print("  showIntentionScreen_ not found - good!")

print("\nAnalysis complete.")
