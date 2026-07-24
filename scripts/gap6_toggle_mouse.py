import os

os.chdir('C:\\Proyectos\\MixCoach')

with open('Source/MixCoach/UI/EndOfSessionComponent.cpp', 'r', encoding='utf-8') as f:
    content = f.read()

# ─── Fix mouseDown ───
old_down = '''    void EndOfSessionComponent::mouseDown(const juce::MouseEvent& e)
    {
        auto pos = e.getPosition();

        // Adjust for scroll
        pos.setY(pos.getY() + scrollOffset_);

        if (exportButtonBounds_.contains(pos)) {
            exportReport();'''

new_down = '''    void EndOfSessionComponent::mouseDown(const juce::MouseEvent& e)
    {
        auto pos = e.getPosition();

        // Adjust for scroll
        pos.setY(pos.getY() + scrollOffset_);

        // Toggle technical details
        if (toggleBounds_.contains(pos)) {
            showTechnicalDetails_ = !showTechnicalDetails_;
            resized();
            repaint();
            return;
        }

        if (exportButtonBounds_.contains(pos)) {
            exportReport();'''

if old_down in content:
    content = content.replace(old_down, new_down, 1)
    print('mouseDown: Updated')
else:
    print('mouseDown: Pattern not found')
    # Show hex around mouseDown
    idx = content.find('void EndOfSessionComponent::mouseDown')
    if idx >= 0:
        # Show exact characters
        snippet = content[idx:idx+300]
        print(repr(snippet[:150]))

# ─── Fix mouseMove ───
old_move = '''    void EndOfSessionComponent::mouseMove(const juce::MouseEvent& e)
    {
        auto pos = e.getPosition();
        pos.setY(pos.getY() + scrollOffset_);

        int oldHovered = hoveredCard_;'''

new_move = '''    void EndOfSessionComponent::mouseMove(const juce::MouseEvent& e)
    {
        auto pos = e.getPosition();
        pos.setY(pos.getY() + scrollOffset_);

        // Toggle hover
        toggleHovered_ = toggleBounds_.contains(pos);

        int oldHovered = hoveredCard_;'''

if old_move in content:
    content = content.replace(old_move, new_move, 1)
    print('mouseMove: Updated')
else:
    print('mouseMove: Pattern not found')
    idx = content.find('void EndOfSessionComponent::mouseMove')
    if idx >= 0:
        print(repr(content[idx:idx+200]))

# ─── Check mouseExit ───
if 'mouseExit' not in content:
    print('mouseExit: Not found in file')
else:
    print('mouseExit: Found')

with open('Source/MixCoach/UI/EndOfSessionComponent.cpp', 'w', encoding='utf-8') as f:
    f.write(content)

print('Done')
