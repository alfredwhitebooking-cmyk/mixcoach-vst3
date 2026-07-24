"""
Wire MixMapDetailPanel.onTogglePinned to persist pinned state.
Adds the callback right after the existing onClose wiring.
"""
path = "Source/MixCoach/UI/CoachChatComponent.cpp"

with open(path, 'r', encoding='utf-8') as f:
    content = f.read()

original = content

# Find the onClose wiring and add onTogglePinned right after
old_text = """                    mixMapDetailPanel_.onClose = [this]() {
                        mixMapDetailPanel_.setVisible(false);
                    };"""

new_text = """                    mixMapDetailPanel_.onClose = [this]() {
                        mixMapDetailPanel_.setVisible(false);
                    };

                    // Wire pin toggle to persist pinned state
                    mixMapDetailPanel_.onTogglePinned = [this](bool pinned) {
                        mixMapDetailPanel_.setPinned(pinned);
                    };"""

if old_text in content:
    content = content.replace(old_text, new_text, 1)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"DONE: {len(content)} chars (was {len(original)})", flush=True)
else:
    print("FAIL: old text not found", flush=True)
