# Insert ModeSelectionCard code into MixCoachPanel constructor
$filePath = "Source/MixCoach/UI/CoachChatComponent.cpp"
$content = [System.IO.File]::ReadAllText($filePath)

# The code to insert - will add after the constructor opening brace
$insertCode = @'
        // ─── Mode Selection Cards (Intention state) ─────────────────────────────
        modeMixCard_ = ModeSelectionCard(ModeSelectionCard::ModeType::Mix);
        modeMixCard_.onCardSelected = [this]() {
            if (onSuggestionClicked) onSuggestionClicked("Mezclar");
            setShowModeCards(false);
        };
        addAndMakeVisible(modeMixCard_);
        modeMixCard_.setVisible(false);

        modeMasterCard_ = ModeSelectionCard(ModeSelectionCard::ModeType::Master);
        modeMasterCard_.onCardSelected = [this]() {
            if (onSuggestionClicked) onSuggestionClicked("Masterizar");
            setShowModeCards(false);
        };
        addAndMakeVisible(modeMasterCard_);
        modeMasterCard_.setVisible(false);

'@

# Find the line with MixCoachPanel constructor + opening brace + Section PISTAS
# Replace the pattern: opening brace followed by Section PISTAS comment
$anchor = 'MixCoachPanel::MixCoachPanel()'
$idx = $content.IndexOf($anchor)
if ($idx -ge 0) {
    # Find the opening brace after the constructor name
    $braceIdx = $content.IndexOf('{', $idx)
    if ($braceIdx -ge 0) {
        # Insert after the opening brace (and its newline)
        $afterBrace = $braceIdx + 1
        # Skip any whitespace/newlines after the brace
        $newContent = $content.Substring(0, $afterBrace) + "`n" + $insertCode + $content.Substring($afterBrace)
        [System.IO.File]::WriteAllText($filePath, $newContent)
        Write-Host "SUCCESS: ModeSelectionCard code inserted after constructor opening brace"
        exit 0
    }
}
Write-Host "ERROR: Could not find constructor anchor"
exit 1
