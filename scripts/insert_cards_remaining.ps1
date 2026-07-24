param(
    [string]$filePath = "Source/MixCoach/UI/CoachChatComponent.cpp"
)

$content = [System.IO.File]::ReadAllText($filePath)

# ─── 1. Add setShowModeCards method implementation ─────────────────────────
# Find the setShowSystemMessages method and add setShowModeCards after it
$anchor1 = "void MixCoachPanel::setShowSystemMessages"
$idx1 = $content.IndexOf($anchor1)
if ($idx1 -lt 0) { Write-Host "ERROR: setShowSystemMessages not found"; exit 1 }

# Find the closing of the setShowSystemMessages method (two closing braces with semicolon)
$methodEnd = $content.IndexOf("`n    }`n`n    // ═══════════════════════════════════", $idx1)
if ($methodEnd -lt 0) { 
    # Try alternate pattern
    $methodEnd = $content.IndexOf("`n    }`n`n    //", $idx1)
}
if ($methodEnd -lt 0) { Write-Host "ERROR: Could not find end of setShowSystemMessages"; exit 1 }

$insertMethod = @"
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setShowModeCards — Show/hide ModeSelectionCards for Intention state
    // ═══════════════════════════════════════════════════════════════════════════
    void MixCoachPanel::setShowModeCards(bool show)
    {
        showModeCards_ = show;
        modeMixCard_.setVisible(show);
        modeMasterCard_.setVisible(show);
        if (show) {
            modeMixCard_.restartAnimation();
            modeMasterCard_.restartAnimation();
        }
        resized();
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  updateCoachAdvice
"@

$content = $content.Substring(0, $methodEnd) + $insertMethod + $content.Substring($methodEnd)

# ─── 2. Add ModeSelectionCard positioning in resized() ──────────────────────
# Find the welcome mode section where chat viewport is positioned
# Look for:  "// Chat viewport" in welcome mode section
$anchor2 = "chatViewport_.setBounds(leftArea);`n            {"
$idx2 = $content.IndexOf($anchor2)

if ($idx2 -ge 0) {
    # Find the closing of this block (the }) that ends the welcome mode resized logic
    $blockEnd = $content.IndexOf("`n        } else {`n            // ═══ Modo normal", $idx2)
    if ($blockEnd -ge 0) {
        # Insert ModeSelectionCard positioning before the else block
        $cardsLayout = @"

            // ─── Mode Selection Cards (Intention state) ────────────────────────────
            if (showModeCards_) {
                int cardW = 240;
                int cardH = 330;
                int gap = 16;
                int totalW = cardW * 2 + gap;
                int cardsX = leftArea.getX() + (leftArea.getWidth() - totalW) / 2;
                int cardsY = leftArea.getY() + (leftArea.getHeight() - cardH) / 2;
                modeMixCard_.setBounds(cardsX, cardsY, cardW, cardH);
                modeMasterCard_.setBounds(cardsX + cardW + gap, cardsY, cardW, cardH);
            } else {
                modeMixCard_.setBounds(0, 0, 0, 0);
                modeMasterCard_.setBounds(0, 0, 0, 0);
            }

"@
        $content = $content.Substring(0, $blockEnd) + $cardsLayout + $content.Substring($blockEnd)
        Write-Host "SUCCESS: Card positioning added to resized()"
    } else {
        Write-Host "ERROR: Could not find else block in resized()"
        exit 1
    }
} else {
    Write-Host "ERROR: Could not find chatViewport anchor in resized()"
    exit 1
}

[System.IO.File]::WriteAllText($filePath, $content)
Write-Host "SUCCESS: All insertions complete"
