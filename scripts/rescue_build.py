#!/usr/bin/env python3
"""
Fix two build issues:
1. ModeSelectionCard.cpp: drawWaveform defined 5 times (duplicate)
2. CoachChatComponent.cpp: setWelcomeMode has broken syntax (else without matching if)

Strategy:
- For both files, extract the correct versions from git HEAD and re-apply only our
  intended changes to each function.
"""

import subprocess
import re

# ===========================================================
# Fix 1: ModeSelectionCard.cpp - drawWaveform duplicates
# ===========================================================
CPP1 = "Source/MixCoach/UI/ModeSelectionCard.cpp"
print(f"=== Fix 1: {CPP1} ===")

# Get HEAD version
result = subprocess.run(
    ["git", "show", "HEAD:Source/MixCoach/UI/ModeSelectionCard.cpp"],
    capture_output=True,
    cwd=r"C:\Proyectos\MixCoach"
)
head1 = result.stdout.decode('utf-8', errors='replace')

# Check how many drawWaveform are in HEAD
head_count = head1.count("drawWaveform")
print(f"drawWaveform in HEAD: {head_count}")

# Read current version
with open(CPP1, 'rb') as f:
    current1 = f.read().decode('utf-8', errors='replace')

current_count = current1.count("drawWaveform")
print(f"drawWaveform in CURRENT: {current_count}")

if current_count > head_count:
    print(f"FOUND {current_count - head_count} extra drawWaveform definitions")
    # Replace current with HEAD to fix the duplication
    with open(CPP1, 'wb') as f:
        f.write(head1.encode('utf-8'))
    print("Reverted ModeSelectionCard.cpp to HEAD")
else:
    print("No extra definitions - checking other issues")

# ===========================================================
# Fix 2: CoachChatComponent.cpp - setWelcomeMode
# ===========================================================
CPP2 = "Source/MixCoach/UI/CoachChatComponent.cpp"
print(f"\n=== Fix 2: {CPP2} ===")

# Get HEAD version
result2 = subprocess.run(
    ["git", "show", "HEAD:Source/MixCoach/UI/CoachChatComponent.cpp"],
    capture_output=True,
    cwd=r"C:\Proyectos\MixCoach"
)
head2 = result2.stdout.decode('utf-8', errors='replace')

# Extract the correct setWelcomeMode from HEAD
head_marker = "void MixCoachPanel::setWelcomeMode(bool welcome)"
head_idx = head2.find(head_marker)
if head_idx < 0:
    print("ERROR: Could not find setWelcomeMode in HEAD")
    exit(1)

# Find the function body in HEAD
brace_start = head2.find('{', head_idx)
brace_count = 0
head_func_end = brace_start
for i in range(brace_start, len(head2)):
    if head2[i] == '{': brace_count += 1
    elif head2[i] == '}': 
        brace_count -= 1
        if brace_count == 0:
            head_func_end = i + 1
            break

head_setWelcomeMode = head2[head_idx:head_func_end]
print(f"HEAD setWelcomeMode: {len(head_setWelcomeMode)} chars")
print(f"First 100: {repr(head_setWelcomeMode[:100])}")

# Now find the corrupted setWelcomeMode in CURRENT
current_marker = "MixCoachPanel::setWelcomeMode(bool welcome)"
curr_idx = current1.find(current_marker)
if curr_idx < 0:
    print("ERROR: Could not find setWelcomeMode in CURRENT")
    exit(1)

# Find it more precisely
curr_full_marker = head_marker
curr_idx2 = current1.find(curr_full_marker)
if curr_idx2 < 0:
    print("ERROR: Could not find setWelcomeMode in CURRENT (trying shorter marker)")
    curr_idx2 = curr_idx

# Find the function body end in CURRENT
curr_brace_start = current1.find('{', curr_idx2)
brace_count = 0
curr_func_end = curr_brace_start
for i in range(curr_brace_start, min(curr_brace_start + 300, len(current1))):
    if current1[i] == '{': brace_count += 1
    elif current1[i] == '}': 
        brace_count -= 1
        if brace_count == 0:
            curr_func_end = i + 1
            break

curr_setWelcomeMode = current1[curr_idx2:curr_func_end]
print(f"CURRENT setWelcomeMode: {len(curr_setWelcomeMode)} chars")
print(f"First 100: {repr(curr_setWelcomeMode[:100])}")

# Replace the corrupted function with the HEAD version
current1 = current1[:curr_idx2] + head_setWelcomeMode + current1[curr_func_end:]
print(f"Replaced corrupted setWelcomeMode with HEAD version")
print(f"New file length: {len(current1)} chars")

# Save
with open(CPP2, 'wb') as f:
    f.write(current1.encode('utf-8'))

print(f"\nSaved {CPP2}")
print("Done! Now run the build.")
