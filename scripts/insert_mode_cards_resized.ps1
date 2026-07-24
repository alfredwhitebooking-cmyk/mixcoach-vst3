# Insert ModeSelectionCard positioning in resized() after the chat viewport code
$filePath = "Source/MixCoach/UI/CoachChatComponent.cpp"
$content = Get-Content $filePath -Raw

$oldCode = "    // Chat viewport`r`n            chatViewport_.setBounds(leftArea);`r`n            {`r`n                int ch = chatMessages_.getTotalHeight();`r`n                chatMessages_.setSize(leftArea.getWidth(), ch);`r`n                writeChatLog(`"[Resize] welcome mode chatViewport h=`" + juce::String(leftArea.getHeight()));`r`n            }"

$newCode = "    // Chat viewport`r`n            chatViewport_.setBounds(leftArea);`r`n            {`r`n                int ch = chatMessages_.getTotalHeight();`r`n                chatMessages_.setSize(leftArea.getWidth(), ch);`r`n                writeChatLog(`"[Resize] welcome mode chatViewport h=`" + juce::String(leftArea.getHeight()));`r`n            }`r`n`r`n            // ─── ModeSelectionCard positioning (overlay in welcome mode) ───`r`n            if (showModeCards_ && getWidth() > 0) {`r`n                int cardW  = juce::jmin(240, (leftArea.getWidth() - 16) / 2);`r`n                int cardH  = 330;`r`n                int gap    = 16;`r`n                int totalW = cardW * 2 + gap;`r`n                int cardsX = leftArea.getX() + (leftArea.getWidth() - totalW) / 2;`r`n                int cardsY = (getHeight() - cardH) / 2;`r`n                modeMixCard_.setBounds(cardsX, cardsY, cardW, cardH);`r`n                modeMasterCard_.setBounds(cardsX + cardW + gap, cardsY, cardW, cardH);`r`n            }"

Write-Host "Replacing in $filePath..."
if ($content.Contains($oldCode)) {
    $content = $content.Replace($oldCode, $newCode)
    Set-Content $filePath $content
    Write-Host "✅ ModeSelectionCard positioning inserted successfully in resized()"
    exit 0
} else {
    Write-Host "❌ Could not find the exact anchor text. Searching for partial match..."
    if ($content.Contains("writeChatLog(`"[Resize] welcome mode chatViewport h=`"")) {
        Write-Host "Found partial match - anchor text exists"
    }
    exit 1
}
