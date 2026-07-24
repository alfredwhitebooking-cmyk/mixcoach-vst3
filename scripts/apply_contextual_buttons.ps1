# Apply contextual question button modifications to ChatMessagesComponent.cpp
$file = "Source/MixCoach/UI/ChatMessagesComponent.cpp"
$content = Get-Content $file -Raw

# ==============================================================
# 1. Modify addMessage() to detect [!contextual:trackName] marker
# ==============================================================

$oldAddMsg = @'
    void ChatMessagesComponent::addMessage(const juce::String& text, const juce::String& tag)
    {
        // ═══ Auto-transition from welcome animation when first coach message arrives ═══
        // FadingOut: fundido suave de la welcome card (~400ms en lugar de desaparecer instantáneamente)
        if (welcomePhase_ != WelcomePhase::Inactive && welcomePhase_ != WelcomePhase::Complete) {
            if (welcomePhase_ != WelcomePhase::FadingOut) {
                welcomePhase_ = WelcomePhase::FadingOut;
                fadeOutStartMs_ = juce::Time::getMillisecondCounter();
                welcomeFadeAlpha_ = 1.0f;
                isTyping_ = false;
            }
        }

        ChatBubble bubble;
        bubble.text      = text;
        bubble.tag       = tag;
        bubble.isUser    = false;
        bubble.isSystem  = false;
        bubble.timestamp = formatTimestamp();
        messages_.push_back(bubble);
        {
            juce::String preview = text.substring(0, 80);
            preview              = preview.replace("\n", " ");
            writeChatLog("[ChatBubble] addMessage (coach): \"" + preview + "\" (len=" + juce::String(text.length())
                         + ")");
        }

        // ═══ Detectar pregunta de modo (Mix/Master) → mostrar botones ═══
'@

$newAddMsg = @'
    void ChatMessagesComponent::addMessage(const juce::String& text, const juce::String& tag)
    {
        // ═══ Auto-transition from welcome animation when first coach message arrives ═══
        // FadingOut: fundido suave de la welcome card (~400ms en lugar de desaparecer instantáneamente)
        if (welcomePhase_ != WelcomePhase::Inactive && welcomePhase_ != WelcomePhase::Complete) {
            if (welcomePhase_ != WelcomePhase::FadingOut) {
                welcomePhase_ = WelcomePhase::FadingOut;
                fadeOutStartMs_ = juce::Time::getMillisecondCounter();
                welcomeFadeAlpha_ = 1.0f;
                isTyping_ = false;
            }
        }

        // ═══ Detectar marcador [!contextual:trackName] ═══
        juce::String cleanText = text;
        juce::String contextualTrack;
        int markerStart = cleanText.indexOf("[!contextual:");
        if (markerStart >= 0) {
            int markerEnd = cleanText.indexOf(markerStart, "]");
            if (markerEnd > markerStart) {
                contextualTrack = cleanText.substring(markerStart + 14, markerEnd);
                cleanText = cleanText.substring(0, markerStart).trimEnd();
            }
        }

        ChatBubble bubble;
        bubble.text      = cleanText;
        bubble.tag       = tag;
        bubble.isUser    = false;
        bubble.isSystem  = false;
        bubble.timestamp = formatTimestamp();
        messages_.push_back(bubble);
        {
            juce::String preview = cleanText.substring(0, 80);
            preview              = preview.replace("\n", " ");
            writeChatLog("[ChatBubble] addMessage (coach): \"" + preview + "\" (len=" + juce::String(cleanText.length())
                         + ")");
        }

        // ═══ Activar botones contextuales si se detectó marcador ═══
        if (contextualTrack.isNotEmpty()) {
            contextualQuestionBubbleIndex_ = (int)messages_.size() - 1;
            contextualTrackName_ = contextualTrack;
            writeChatLog("[ChatBubble] Contextual question activated for track: " + contextualTrack);
        }

        // ═══ Detectar pregunta de modo (Mix/Master) → mostrar botones ═══
'@

Write-Output "1. Replacing addMessage..."
if ($content.Contains($oldAddMsg)) {
    $content = $content.Replace($oldAddMsg, $newAddMsg)
    Write-Output "   [OK] addMessage updated"
} else {
    Write-Output "   [WARN] Pattern not found - attempting smaller replacement..."
}

# ==============================================================
# 2. Modify drawActionButtons() to delegate to contextual question
# ==============================================================

$oldDrawAction = @'
    }float ChatMessagesComponent::drawActionButtons(juce::Graphics& g,
                                                    juce::Rectangle<float> bounds,
                                                    const juce::String& bubbleText,
                                                    int bubbleIndex)
    {
        if (bubbleIndex < 0 || bubbleIndex >= (int)messages_.size()) return 0.0f;
        auto& msg = messages_[bubbleIndex];
        if (msg.isUser || msg.isSystem || msg.isRefinementCard) return 0.0f;
        if (msg.resolved) return 0.0f;

        float btnY = bounds.getBottom() - kActionBtnHeight - 4.0f;
        float innerPad = 6.0f;
        auto btnArea = juce::Rectangle<float>(
            bounds.getX() + innerPad, btnY, bounds.getWidth() - innerPad * 2.0f, kActionBtnHeight
        );

        const float btnGap = 4.0f;
        const float btnFontSize = 9.5f;

        struct BtnDef { const char* label; juce::Colour colour; int type; };
        BtnDef btns[3] = {
            { "[+] Lo hice",    MixCoachTheme::success(),     0 },
            { "\xE2\x96\xB6 Explicame", MixCoachTheme::accentGlow(), 1 },
            { "? Pista",       MixCoachTheme::warning(),    2 },
        };
'@

$newDrawAction = @'
    }float ChatMessagesComponent::drawActionButtons(juce::Graphics& g,
                                                    juce::Rectangle<float> bounds,
                                                    const juce::String& bubbleText,
                                                    int bubbleIndex)
    {
        if (bubbleIndex < 0 || bubbleIndex >= (int)messages_.size()) return 0.0f;
        auto& msg = messages_[bubbleIndex];
        if (msg.isUser || msg.isSystem || msg.isRefinementCard) return 0.0f;
        if (msg.resolved) return 0.0f;

        // ═══ Burbuja con pregunta contextual → botones diferentes ═══
        if (contextualQuestionBubbleIndex_ == bubbleIndex) {
            return drawContextualQuestionButtons(g, bounds, bubbleText, bubbleIndex);
        }

        float btnY = bounds.getBottom() - kActionBtnHeight - 4.0f;
        float innerPad = 6.0f;
        auto btnArea = juce::Rectangle<float>(
            bounds.getX() + innerPad, btnY, bounds.getWidth() - innerPad * 2.0f, kActionBtnHeight
        );

        const float btnGap = 4.0f;
        const float btnFontSize = 9.5f;

        struct BtnDef { const char* label; juce::Colour colour; int type; };
        BtnDef btns[3] = {
            { "[+] Lo hice",    MixCoachTheme::success(),     0 },
            { "\xE2\x96\xB6 Explicame", MixCoachTheme::accentGlow(), 1 },
            { "? Pista",       MixCoachTheme::warning(),    2 },
        };
'@

Write-Output "2. Replacing drawActionButtons..."
if ($content.Contains($oldDrawAction)) {
    $content = $content.Replace($oldDrawAction, $newDrawAction)
    Write-Output "   [OK] drawActionButtons updated"
} else {
    Write-Output "   [WARN] drawActionButtons pattern not found"
}

# ==============================================================
# 3. Add drawContextualQuestionButtons() implementation
# Insert after the drawActionButtons function
# ==============================================================

$afterDrawAction = @'
        return kActionBtnHeight + 4.0f;
    }
'@

$contextualButtonsImpl = @'
        return kActionBtnHeight + 4.0f;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawContextualQuestionButtons — Botones [✅ Sí, intencional] [↩️ No, accidental]
    //  Se dibujan debajo de la burbuja del coach cuando hay una pregunta contextual
    //  (cambio Critical). Son más grandes que los botones estándar.
    // ═══════════════════════════════════════════════════════════════════════════
    float ChatMessagesComponent::drawContextualQuestionButtons(juce::Graphics& g,
                                                                juce::Rectangle<float> bounds,
                                                                const juce::String& bubbleText,
                                                                int bubbleIndex)
    {
        float btnY = bounds.getBottom() - kContextualBtnHeight - 4.0f;
        float innerPad = 6.0f;
        auto btnArea = juce::Rectangle<float>(
            bounds.getX() + innerPad, btnY, bounds.getWidth() - innerPad * 2.0f, kContextualBtnHeight
        );

        const float btnGap = 8.0f;
        const float btnFontSize = 11.0f;

        struct BtnDef { const char* label; juce::Colour colour; int type; };
        BtnDef btns[2] = {
            { "\u2705 S\u00ED, fue intencional", juce::Colour(0xFF4ADE80), 3 },  // Green
            { "\u21A9\uFE0F No, fue accidental", juce::Colour(0xFFF87171), 4 },  // Red
        };

        juce::Font btnFont = juce::Font(juce::FontOptions(btnFontSize)).boldened();
        float bx = btnArea.getX();

        for (int bi = 0; bi < 2; ++bi) {
            float btnW = getTextWidth(btnFont, juce::String(btns[bi].label)) + 14.0f;
            btnW = juce::jmin(btnW, btnArea.getWidth() * 0.55f);
            auto btnRect = juce::Rectangle<float>(bx, btnArea.getY(), btnW, btnArea.getHeight());
            bool isHovered = (hoveredButtonType_ >= 0 && hoveredBubbleText_ == bubbleText
                              && hoveredButtonType_ == btns[bi].type);
            bool isClicked = (clickedButton_.isActive() && clickedButton_.bubbleText == bubbleText
                              && clickedButton_.buttonType == btns[bi].type);
            float clickElapsed = isClicked ? clickedButton_.getElapsedMs() : 0.0f;
            bool inFlashPhase = isClicked && (clickElapsed < kClickFlashDurationMs);
            const float btnCr = 6.0f;

            // Shadow
            g.setColour(juce::Colours::black.withAlpha(0.20f));
            g.fillRoundedRectangle(btnRect.translated(0.0f, 1.5f), btnCr);

            // Hover glow
            if (isHovered && !inFlashPhase) {
                g.setColour(btns[bi].colour.withAlpha(0.12f));
                g.fillRoundedRectangle(btnRect.expanded(3.0f, 3.0f), btnCr + 1.0f);
            }

            // Click flash animation
            if (inFlashPhase) {
                float flashProgress = clickElapsed / kClickFlashDurationMs;
                float pulseAlpha = 1.0f - flashProgress * 0.5f;
                float glowSize = 2.0f + flashProgress * 6.0f;
                g.setColour(btns[bi].colour.withAlpha(0.20f * (1.0f - flashProgress * 0.5f)));
                g.fillRoundedRectangle(btnRect.expanded(glowSize, glowSize), btnCr + glowSize * 0.5f);
                g.setColour(btns[bi].colour.withAlpha(pulseAlpha * 0.35f));
                g.fillRoundedRectangle(btnRect, btnCr);
                g.setColour(btns[bi].colour.withAlpha(pulseAlpha * 0.75f));
                g.drawRoundedRectangle(btnRect, btnCr, 0.8f);
            } else {
                // Normal state: gradient background
                juce::ColourGradient bgGrad(btns[bi].colour.withAlpha(isHovered ? 0.20f : 0.10f),
                    btnRect.getX(), btnRect.getY(),
                    btns[bi].colour.withAlpha(isHovered ? 0.15f : 0.06f),
                    btnRect.getX(), btnRect.getBottom(), false);
                g.setGradientFill(bgGrad);
                g.fillRoundedRectangle(btnRect, btnCr);

                // Border
                g.setColour(btns[bi].colour.withAlpha(isHovered ? 0.50f : 0.25f));
                g.drawRoundedRectangle(btnRect, btnCr, isHovered ? 0.8f : 0.5f);

                // Glass highlight
                auto glassH = btnRect.withHeight(btnRect.getHeight() * 0.45f);
                juce::ColourGradient glassGrad(juce::Colours::white.withAlpha(isHovered ? 0.10f : 0.06f),
                    glassH.getX(), glassH.getY(),
                    juce::Colour(0x00000000),
                    glassH.getX(), glassH.getBottom(), false);
                g.setGradientFill(glassGrad);
                g.fillRoundedRectangle(glassH, btnCr);
            }

            // Button text
            g.setFont(btnFont);
            g.setColour(btns[bi].colour.withAlpha(isHovered ? 1.0f : 0.85f));
            g.drawText(juce::String(btns[bi].label), btnRect, juce::Justification::centred);

            actionButtonHits_.push_back({bubbleText, btns[bi].type, btnRect, bubbleIndex});
            bx += btnW + btnGap;
        }
        return kContextualBtnHeight + 4.0f;
    }
'@

Write-Output "3. Adding drawContextualQuestionButtons..."
$lastReturn = $content.LastIndexOf("return kActionBtnHeight + 4.0f;")
if ($lastReturn -ge 0) {
    $content = $content.Remove($lastReturn, "return kActionBtnHeight + 4.0f;".Length)
    $content = $content.Insert($lastReturn, $contextualButtonsImpl)
    Write-Output "   [OK] drawContextualQuestionButtons added"
} else {
    Write-Output "   [WARN] Could not find insertion point"
}

# ==============================================================
# 4. Update getBubbleHeight() for contextual button height
# ==============================================================

$oldHeightCheck = @'
        if (!bubble.isUser && !bubble.isSystem && !bubble.isRefinementCard && !bubble.resolved) {
            resultH += kActionBtnHeight + 6.0f;
        }
'@

$newHeightCheck = @'
        if (!bubble.isUser && !bubble.isSystem && !bubble.isRefinementCard && !bubble.resolved) {
            // ═══ Burbuja con pregunta contextual → botones más grandes ═══
            int bubbleIdx = (int)(&bubble - messages_.data()); // approximate
            if (contextualQuestionBubbleIndex_ >= 0) {
                // Find the actual index by scanning messages
                for (int si = 0; si < (int)messages_.size(); ++si) {
                    if (&messages_[si] == &bubble) {
                        if (si == contextualQuestionBubbleIndex_) {
                            resultH += kContextualBtnHeight + 6.0f;
                        } else {
                            resultH += kActionBtnHeight + 6.0f;
                        }
                        break;
                    }
                }
            } else {
                resultH += kActionBtnHeight + 6.0f;
            }
        }
'@

Write-Output "4. Updating getBubbleHeight..."
if ($content.Contains($oldHeightCheck)) {
    $content = $content.Replace($oldHeightCheck, $newHeightCheck)
    Write-Output "   [OK] getBubbleHeight updated"
} else {
    Write-Output "   [WARN] getBubbleHeight pattern not found"
}

# ==============================================================
# 5. Update clear() to reset contextual question state
# ==============================================================

$oldClear = @'
        taskBlockExpandBounds_.clear();
        hoveredTaskBlockBtn_ = -1;
        repaint();
    }
'@

$newClear = @'
        taskBlockExpandBounds_.clear();
        hoveredTaskBlockBtn_ = -1;
        contextualQuestionBubbleIndex_ = -1;
        contextualTrackName_.clear();
        repaint();
    }
'@

Write-Output "5. Updating clear()..."
if ($content.Contains($oldClear)) {
    $content = $content.Replace($oldClear, $newClear)
    Write-Output "   [OK] clear() updated"
} else {
    Write-Output "   [WARN] clear() pattern not found"
}

# ==============================================================
# 6. Update mouseDown to handle button types 3 and 4
# ==============================================================

$oldMouseDown = @'
        // ─── Action buttons (Lo hice / Explícame / Pista) ──────────────────
        for (int i = 0; i < (int)actionButtonHits_.size(); ++i) {
            auto& hit = actionButtonHits_[i];
            if (hit.bounds.contains(pos)) {
                clickedButton_.activate(hit.bubbleText, hit.buttonType, hit.bubbleIndex);
                if (onActionButton) onActionButton(hit.bubbleText, hit.buttonType);
'@

$newMouseDown = @'
        // ─── Action buttons (Lo hice / Explícame / Pista / Intencional / Accidental) ──
        for (int i = 0; i < (int)actionButtonHits_.size(); ++i) {
            auto& hit = actionButtonHits_[i];
            if (hit.bounds.contains(pos)) {
                clickedButton_.activate(hit.bubbleText, hit.buttonType, hit.bubbleIndex);
                // ═══ Botones contextuales: desactivar estado después del click ═══
                if (hit.buttonType == 3 || hit.buttonType == 4) {
                    contextualQuestionBubbleIndex_ = -1;
                    contextualTrackName_.clear();
                }
                if (onActionButton) onActionButton(hit.bubbleText, hit.buttonType);
'@

Write-Output "6. Updating mouseDown..."
if ($content.Contains($oldMouseDown)) {
    $content = $content.Replace($oldMouseDown, $newMouseDown)
    Write-Output "   [OK] mouseDown updated"
} else {
    Write-Output "   [WARN] mouseDown pattern not found"
}

# ==============================================================
# 7. Write the final file
# ==============================================================
Write-Output "7. Writing file..."
$content | Set-Content $file -Encoding utf8 -NoNewline
Write-Output "   [OK] File written successfully"
Write-Output "Done!"
