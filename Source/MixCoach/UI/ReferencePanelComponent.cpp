#include "ReferencePanelComponent.h"
#include "ReferencePanelIcons.h"
#include <cmath>
#include <thread>
#include "../../Common/types/Constants.h"
#include "../../Common/types/LogHelper.h"
#include "../engine/CoachEngine.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  AudioFileInfo helper
// ═══════════════════════════════════════════════════════════════════════════
AudioFileInfo ReferencePanelComponent::readAudioFileInfo(const juce::File& file)
{
    AudioFileInfo info;
    if (!file.existsAsFile())
        return info;

    try
    {
        juce::AudioFormatManager formatMgr;
        formatMgr.registerBasicFormats();

        auto* reader = formatMgr.createReaderFor(file);
        if (reader == nullptr)
            return info;

        info.sampleRate = (int)reader->sampleRate;
        info.bitDepth   = reader->bitsPerSample;
        if (reader->sampleRate > 0.0)
            info.durationSeconds = reader->lengthInSamples / reader->sampleRate;
        else
            info.durationSeconds = 0.0;
        info.valid = true;

        delete reader;
    }
    catch (const std::exception& e)
    {
        LogHelper::writeToLog("[ReferencePanel] Excepcion al leer audio: "
                              + juce::String(e.what()));
        info = AudioFileInfo{};
    }
    catch (...)
    {
        LogHelper::writeToLog("[ReferencePanel] Excepcion desconocida al leer audio");
        info = AudioFileInfo{};
    }

    return info;
}

// ═══════════════════════════════════════════════════════════════════════════
//  ReferencePanelComponent Implementation
// ═══════════════════════════════════════════════════════════════════════════

ReferencePanelComponent::ReferencePanelComponent()
{
    // ─── Drop zone unificada ───────────────────────────────────────────────
    dropZone_.onFileDropped = [this](const juce::String& path) {
        addFileReference(path);
    };
    dropZone_.onURLAdded = [this](const juce::String& url) {
        addURLReference(url);
    };
    addAndMakeVisible(dropZone_);

    // ─── Notes editor ──────────────────────────────────────────────────────
    notesEditor_.setMultiLine(true);
    notesEditor_.setFont(juce::Font(juce::FontOptions(11.0f)));
    notesEditor_.setTextToShowWhenEmpty("Escribe tus notas de referencia aqu\xC3\xAD...",
                                        MixCoachTheme::textMuted());
    notesEditor_.setColour(juce::TextEditor::backgroundColourId, MixCoachTheme::bgInput());
    notesEditor_.setColour(juce::TextEditor::textColourId, MixCoachTheme::textPrimary());
    notesEditor_.setColour(juce::TextEditor::outlineColourId, MixCoachTheme::border());
    notesEditor_.setColour(juce::TextEditor::focusedOutlineColourId, MixCoachTheme::accent());
    notesEditor_.setScrollBarThickness(6);
    addAndMakeVisible(notesEditor_);

    // ─── Reference count badge ─────────────────────────────────────────────
    referenceCountLabel_.setText("0", juce::dontSendNotification);
    referenceCountLabel_.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
    referenceCountLabel_.setJustificationType(juce::Justification::centred);
    referenceCountLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
    addAndMakeVisible(referenceCountLabel_);

    // ─── Empty state ───────────────────────────────────────────────────────
    emptyLabel_.setText("Carga archivos de audio o pega enlaces de canciones",
                        juce::dontSendNotification);
    emptyLabel_.setFont(juce::Font(juce::FontOptions(9.0f)));
    emptyLabel_.setJustificationType(juce::Justification::centred);
    emptyLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted().withAlpha(0.6f));
    addAndMakeVisible(emptyLabel_);

    // --- Match panel ---
    addAndMakeVisible(matchPanel_);

    // --- Seek slider (playback bar) ---
    seekSlider_.setRange(0.0, 1.0, 0.001);
    seekSlider_.setValue(0.0, juce::dontSendNotification);
    seekSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    seekSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    seekSlider_.setColour(juce::Slider::trackColourId, MixCoachTheme::accent().withAlpha(0.3f));
    seekSlider_.setColour(juce::Slider::thumbColourId, MixCoachTheme::accentGlow());
    seekSlider_.setColour(juce::Slider::backgroundColourId, MixCoachTheme::bgDarker().withAlpha(0.3f));
    seekSlider_.onValueChange = [this]() {
        if (isDraggingSeek_ && onSeekReference)
        {
            double pos = seekSlider_.getValue() * playbackTotal_;
            onSeekReference(pos);
        }
    };
    seekSlider_.onDragStart = [this]() { isDraggingSeek_ = true; };
    seekSlider_.onDragEnd = [this]() { isDraggingSeek_ = false; };
    addAndMakeVisible(seekSlider_);

    positionLabel_.setText("--:-- / --:--", juce::dontSendNotification);
    positionLabel_.setFont(juce::Font(juce::FontOptions(8.0f)));
    positionLabel_.setJustificationType(juce::Justification::centredRight);
    positionLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
    addAndMakeVisible(positionLabel_);

    // ─── Smooth animation timer (60 fps para progreso) ──────────────
    startTimerHz(60);

    switchTab(ActiveTab::References);
}

void ReferencePanelComponent::switchTab(ActiveTab tab)
{
    activeTab_ = tab;
    dropZone_.setVisible(tab == ActiveTab::References);
    notesEditor_.setVisible(tab == ActiveTab::Notes);

    if (tab == ActiveTab::Notes) {
        emptyLabel_.setText("", juce::dontSendNotification);
    } else {
        emptyLabel_.setVisible(references_.empty());
        emptyLabel_.setText(references_.empty()
            ? "Carga archivos de audio o pega enlaces de canciones — la IA usa ambos"
            : "", juce::dontSendNotification);
    }

    resized();
    repaint();
}

// ═══════════════════════════════════════════════════════════════════════════
//  URL Title Extraction - Helpers
// ═══════════════════════════════════════════════════════════════════════════

juce::String ReferencePanelComponent::parseHTMLTitle(const juce::String& html,
                                                      const juce::String& url)
{
    juce::ignoreUnused(url);

    int titleStart = html.indexOfIgnoreCase("<title>");
    int titleEnd   = html.indexOfIgnoreCase("</title>");
    juce::String title;
    if (titleStart >= 0 && titleEnd > titleStart)
    {
        title = html.substring(titleStart + 7, titleEnd).trim();
        // Limpiar sufijos comunes
        title = title.replace(" - YouTube", "")
                     .replace(" | Spotify", "")
                     .replace(" - Spotify", "");
    }

    // Fallback: og:title content attribute
    if (title.isEmpty())
    {
        int ogIdx = html.indexOf(juce::StringRef("og:title"));
        if (ogIdx >= 0)
        {
            juce::String afterOg = html.substring(ogIdx);
            int localPos = afterOg.indexOf(juce::StringRef("content="));
            if (localPos >= 0)
            {
                int contentStart = ogIdx + localPos + 8;
                while (contentStart < html.length()
                       && html[contentStart] != '\"'
                       && html[contentStart] != '\''
                       && html[contentStart] != '>')
                    ++contentStart;
                if (contentStart < html.length() && html[contentStart] != '>')
                {
                    juce::juce_wchar quoteChar = html[contentStart];
                    ++contentStart;
                    int contentEnd = html.indexOfChar(contentStart, quoteChar);
                    if (contentEnd > contentStart)
                    {
                        title = html.substring(contentStart, contentEnd);
                        title = title.replace(" - YouTube", "");
                    }
                }
            }
        }
    }

    if (title.length() > 80)
        title = title.substring(0, 77) + "...";
    if (title.contains("  "))
        title = title.replace("  ", " ");

    return title.trim();
}

juce::String ReferencePanelComponent::extractSoundCloudName(const juce::String& url)
{
    int idx = url.indexOfIgnoreCase("soundcloud.com/");
    if (idx < 0)
        return {};

    auto afterDomain = url.substring(idx + 15);
    int qIdx = afterDomain.indexOfChar('?');
    if (qIdx >= 0) afterDomain = afterDomain.substring(0, qIdx);
    if (afterDomain.endsWithChar('/'))
        afterDomain = afterDomain.dropLastCharacters(1);

    auto parts = juce::StringArray::fromTokens(afterDomain, "/", "");
    if (parts.size() >= 2)
    {
        juce::String trackName;
        if (parts[0].equalsIgnoreCase("sets") && parts.size() >= 2)
            trackName = parts[parts.size() - 1];
        else
            trackName = parts[parts.size() - 1];

        trackName = trackName.replace("-", " ").replace("_", " ").trim();
        juce::String result;
        bool capNext = true;
        for (int i = 0; i < trackName.length(); ++i)
        {
            auto c = trackName[i];
            if (c == ' ')
            {
                result += ' ';
                capNext = true;
            }
            else if (capNext && c >= 'a' && c <= 'z')
            {
                result += (juce::juce_wchar)(c - 32);
                capNext = false;
            }
            else
            {
                result += c;
                capNext = false;
            }
        }

        if (result.isNotEmpty() && result.length() > 3)
            return result;
    }

    return {};
}

juce::String ReferencePanelComponent::fetchURLTitle(const juce::String& url, int timeoutMs)
{
    try
    {
        juce::URL pageUrl(url);
        int statusCode = 0;
        juce::StringPairArray responseHeaders;

        auto stream = pageUrl.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs(timeoutMs)
                .withResponseHeaders(&responseHeaders)
                .withStatusCode(&statusCode));

        if (stream != nullptr && statusCode == 200)
        {
            juce::String pageContent = stream->readEntireStreamAsString();
            auto title = parseHTMLTitle(pageContent, url);

            if (title.isNotEmpty())
            {
                juce::String emoji;
                if (url.contains("youtube.com") || url.contains("youtu.be"))
                    emoji = "\xF0\x9F\x8E\xAC ";
                else if (url.contains("spotify.com"))
                    emoji = "\xF0\x9F\x8E\xB5 ";
                else if (url.contains("soundcloud.com"))
                    emoji = "\xF0\x9F\x8E\xB5 ";
                else
                    emoji = "\xF0\x9F\x94\x97 ";

                return emoji + title;
            }
        }
    }
    catch (...)
    {
    }

    return {};
}

juce::String ReferencePanelComponent::extractURLName(const juce::String& url)
{
    auto scName = extractSoundCloudName(url);
    if (scName.isNotEmpty())
        return "\xF0\x9F\x8E\xB5 " + scName + " (SoundCloud)";

    if (url.contains("youtube.com") || url.contains("youtu.be")
        || url.contains("spotify.com"))
    {
        return {};
    }

    return {};
}

// ═══════════════════════════════════════════════════════════════════════════
//  Add / Remove References
// ═══════════════════════════════════════════════════════════════════════════

void ReferencePanelComponent::addFileReference(const juce::String& filePath)
{
    auto file = juce::File(filePath);
    auto info = readAudioFileInfo(file);

    MixReference ref;
    ref.type = MixReference::Type::File;
    ref.name = file.getFileName();
    ref.path = filePath;
    static const juce::Colour kRowColours[] = {
        MixCoachTheme::accent(),
        MixCoachTheme::accentCyan(),
        MixCoachTheme::success(),
    };
    int colourIdx = (int)references_.size() % 3;
    ref.colour = kRowColours[colourIdx];
    ref.addedTime = juce::Time::getMillisecondCounter();
    ref.audioInfo = info;
    ref.analysisStatus = AnalysisStatus::Pending;
    references_.push_back(ref);
    matchRefIndex_ = static_cast<int>(references_.size()) - 1;
    refreshDisplay();
    if (onReferencesChanged) onReferencesChanged();

    if (onFileReferenceAdded)
        onFileReferenceAdded(filePath);
}

void ReferencePanelComponent::addURLReference(const juce::String& url)
{
    MixReference ref;
    ref.type = MixReference::Type::URL;

    auto extractedName = extractURLName(url);
    juce::String name;
    juce::String tempName;

    if (extractedName.isNotEmpty())
    {
        name = extractedName;
    }
    else
    {
        if (url.contains("youtube.com") || url.contains("youtu.be"))
        {
            tempName = "\xF0\x9F\x8E\xAC YouTube Reference";
            name = tempName;
        }
        else if (url.contains("spotify.com"))
        {
            tempName = "\xF0\x9F\x8E\xB5 Spotify Reference";
            name = tempName;
        }
        else if (url.contains("soundcloud.com"))
        {
            tempName = "\xF0\x9F\x8E\xB5 SoundCloud Reference";
            name = tempName;
        }
        else
        {
            name = "\xF0\x9F\x94\x97 " + url.substring(0, 30) + "...";
        }

        if (tempName.isNotEmpty())
        {
            auto safeThis = juce::Component::SafePointer<ReferencePanelComponent>(this);
            int refIdx = static_cast<int>(references_.size());

            std::thread([safeThis, url, refIdx, tempName]()
            {
                auto title = ReferencePanelComponent::fetchURLTitle(url);
                if (title.isNotEmpty())
                {
                    juce::MessageManager::callAsync([safeThis, title, refIdx, tempName]()
                    {
                        if (safeThis != nullptr && refIdx < (int)safeThis->references_.size()
                            && safeThis->references_[refIdx].name == tempName)
                        {
                            safeThis->references_[refIdx].name = title;
                            safeThis->references_[refIdx].isFetchingTitle = false;
                            safeThis->refreshDisplay();

                            if (safeThis->onURLReferenceAdded)
                                safeThis->onURLReferenceAdded(title, safeThis->references_[refIdx].path);
                        }
                    });
                }
                else
                {
                    juce::MessageManager::callAsync([safeThis, refIdx, tempName]()
                    {
                        if (safeThis != nullptr && refIdx < (int)safeThis->references_.size()
                            && safeThis->references_[refIdx].name == tempName)
                        {
                            safeThis->references_[refIdx].isFetchingTitle = false;
                            safeThis->refreshDisplay();
                        }
                    });
                }
            }).detach();
        }
    }

    ref.name = name;
    ref.path = url;
    ref.isFetchingTitle = tempName.isNotEmpty();
    ref.colour = MixCoachTheme::accentCyan().withAlpha(0.7f);
    ref.addedTime = juce::Time::getMillisecondCounter();
    ref.analysisStatus = AnalysisStatus::Pending;
    references_.push_back(ref);
    refreshDisplay();
    if (onReferencesChanged) onReferencesChanged();
    if (onURLReferenceAdded) onURLReferenceAdded(name, url);
}

void ReferencePanelComponent::updateReferenceName(int refIndex, const juce::String& newName)
{
    if (refIndex >= 0 && refIndex < (int)references_.size())
    {
        references_[refIndex].name = newName;
        refreshDisplay();
        if (onReferencesChanged) onReferencesChanged();
        if (onURLReferenceAdded)
            onURLReferenceAdded(newName, references_[refIndex].path);
    }
}

void ReferencePanelComponent::removeReference(int index)
{
    if (index >= 0 && index < (int)references_.size()) {
        references_.erase(references_.begin() + index);
        refreshDisplay();
        if (onReferencesChanged) onReferencesChanged();
    }
}

void ReferencePanelComponent::clearReferences()
{
    references_.clear();
    refreshDisplay();
    if (onReferencesChanged) onReferencesChanged();
}

void ReferencePanelComponent::setReferenceAnalysisStatus(int index, AnalysisStatus status)
{
    if (index >= 0 && index < (int)references_.size()) {
        references_[index].analysisStatus = status;
        repaint();
    }
}

void ReferencePanelComponent::setReferenceDrivenMode(bool enabled)
{
    referenceDrivenMode_ = enabled;
    repaint();
}

void ReferencePanelComponent::setReferenceProgress(float currentMatch, float delta)
{
    referenceProgressRaw_ = currentMatch;
    referenceDeltaRaw_ = delta;
    progressFillSmooth_.setTargetValue(currentMatch);
    deltaSmooth_.setTargetValue(delta);
    // No repaint() needed — timerCallback() will advance and repaint
}

// ═══════════════════════════════════════════════════════════════════════════
//  Timer callback — SmoothValue animation at ~60 fps
// ═══════════════════════════════════════════════════════════════════════════

void ReferencePanelComponent::timerCallback()
{
    if (!referenceDrivenMode_ && progressFillSmooth_.getCurrent() < 0.001f)
        return;  // No need to animate when mode is off and fill is already zero

    bool changed = progressFillSmooth_.advance(60.0);
    changed |= deltaSmooth_.advance(60.0);
    if (changed)
        repaint();
}

void ReferencePanelComponent::refreshDisplay()
{
    referenceCountLabel_.setText(juce::String(references_.size()), juce::dontSendNotification);
    emptyLabel_.setVisible(references_.empty() && activeTab_ == ActiveTab::References);
    repaint();
    resized();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Layout (resized)
// ═══════════════════════════════════════════════════════════════════════════

void ReferencePanelComponent::resized()
{
    auto area = getLocalBounds().reduced(4);

    // ─── Tab row: REFERENCE | NOTES | REF MODE toggle ────────────────────
    auto tabRow = area.removeFromTop(20);
    int tabW = juce::jmin(100, tabRow.getWidth() / 2 - 4);
    refsTabBounds_  = tabRow.removeFromLeft(tabW).reduced(0, 2);
    notesTabBounds_ = tabRow.removeFromLeft(tabW).reduced(0, 2);

    // ─── Reference-Driven Mode toggle (right side of tab row) ───────────
    {
        int toggleW = 80;
        int toggleH = 16;
        int toggleX = tabRow.getRight() - toggleW - 4;
        int toggleY = tabRow.getY() + (tabRow.getHeight() - toggleH) / 2;
        refModeToggleBounds_ = { toggleX, toggleY, toggleW, toggleH };
    }

    referenceCountLabel_.setBounds(tabRow.getRight() - 20 - 80 - 4, tabRow.getY() + 2, 20, 16);

    area.removeFromTop(3);

    // ─── Reference-Driven progress bar (thin, below header) ──────────────
    if (referenceDrivenMode_)
    {
        auto progArea = area.removeFromTop(12).reduced(4, 1);
        refProgressBounds_ = progArea;
        area.removeFromTop(2);
    }
    else
    {
        refProgressBounds_ = {};
    }

    // ─── Content ─────────────────────────────────────────────────────────
    visibleRowBounds_.clear();
    playBtnBounds_.clear();
    deleteBtnBounds_.clear();

    if (activeTab_ == ActiveTab::Notes) {
        notesEditor_.setBounds(area.reduced(2));
        return;
    }

    // ─── REFERENCES TAB ──────────────────────────────────────────────────
    if (references_.empty()) {
        addRefHeaderBounds_ = area.removeFromTop(8);

        int dropH = juce::jmin(area.getHeight() - 28, juce::jmax(80, (int)(area.getHeight() * 0.65f)));
        auto dropArea = area.removeFromTop(dropH);
        dropZone_.setBounds(dropArea.reduced(0, 4));
        dropZone_.setShowDropHint(true);
        dropZone_.setVisible(true);

        area.removeFromTop(4);
        emptyLabel_.setBounds(area.reduced(4, 0));
        return;
    }

    // ─── Has references: compact drop zone + rows ────────────────────────
    listDividerY_ = area.getY();

    int dropZoneH = 44;
    auto dropArea = area.removeFromTop(dropZoneH);
    dropZone_.setBounds(dropArea.reduced(0, 2));
    dropZone_.setShowDropHint(false);
    dropZone_.setVisible(true);

    area.removeFromTop(4);

    // ─── Reference rows: altura adaptativa ──────────────────────────────
    int numRefs = (int)references_.size();
    int availH  = area.getHeight();

    int idealRowH = juce::jmin(48, juce::jmax(30, availH / juce::jmax(1, numRefs)));
    int rowH = idealRowH;
    int maxRows = availH / rowH;
    int totalToDraw = juce::jmin(numRefs, maxRows);

    int totalRowsH = totalToDraw * rowH;
    int topPad = (availH - totalRowsH) / 2;
    if (topPad > 0 && totalToDraw == numRefs)
        area.removeFromTop(topPad);

    int drawn = 0;
    for (int i = (int)references_.size() - 1; i >= 0 && drawn < totalToDraw; --i) {
        auto rowArea = area.removeFromTop(rowH).reduced(4, 2);
        visibleRowBounds_.push_back(rowArea);
        playBtnBounds_.push_back({ rowArea.getX(), rowArea.getY(), 26, rowArea.getHeight() });
        deleteBtnBounds_.push_back({ rowArea.getRight() - 20, rowArea.getY(), 20, rowArea.getHeight() });
        drawn++;
    }

    // --- Section selector bar (only when sections exist) ---
    if (!sections_.empty())
    {
        auto sectionArea = area.removeFromBottom(22).reduced(4, 1);
        sectionBtnBounds_.clear();
    }

    // --- Playback bar (always visible when playing) ---
    {
        int barH = 14;
        auto barArea = area.removeFromBottom(barH + 4).reduced(4, 0);
        positionLabel_.setBounds(barArea.removeFromRight(70));
        seekSlider_.setBounds(barArea);
    }

    // --- Label: "SECTIONS" above the section selector ---
    if (!sections_.empty())
    {
        auto labelArea = area.removeFromBottom(10).reduced(4, 0);
        juce::ignoreUnused(labelArea);
    }

    // --- Match panel (takes remaining space) ---
    matchPanel_.setBounds(area.reduced(0, 2));

    emptyLabel_.setVisible(false);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Paint
// ═══════════════════════════════════════════════════════════════════════════

void ReferencePanelComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), 6.0f);

    // ─── Tab buttons ──────────────────────────────────────────────────────
    auto drawTab = [&](juce::Rectangle<int> tabBounds, bool isActive,
                       const juce::String& label, int count) {
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
        g.setColour(isActive ? MixCoachTheme::accentGlow() : MixCoachTheme::textMuted().withAlpha(0.5f));
        g.drawText(label, tabBounds, juce::Justification::centred);

        if (isActive) {
            int cx = tabBounds.getCentreX();
            int bw = tabBounds.getWidth();
            float glowW = juce::jmin(60.0f, bw * 0.8f);
            auto glowArea = juce::Rectangle<float>((float)(cx - glowW * 0.5f),
                                                      (float)(tabBounds.getBottom() - 2),
                                                      glowW, 3.0f);
            juce::ColourGradient glow(
                MixCoachTheme::accent().withAlpha(0.35f), (float)cx, glowArea.getY(),
                MixCoachTheme::accent().withAlpha(0.0f),  glowArea.getRight(), glowArea.getY(), false);
            glow.addColour(0.5f, MixCoachTheme::accent().withAlpha(0.12f));
            g.setGradientFill(glow);
            g.fillRect(glowArea);
            g.setColour(MixCoachTheme::accent().withAlpha(0.5f));
            g.drawHorizontalLine(tabBounds.getBottom() - 1,
                                 (float)(cx - bw * 0.25f), (float)(cx + bw * 0.25f));
        }

        if (!isActive && count > 0) {
            auto badgeBounds = tabBounds.translated(tabBounds.getWidth() + 2, 0)
                                .withWidth(14).reduced(0, 5);
            g.setColour(MixCoachTheme::bgPanel().withAlpha(0.6f));
            g.fillRoundedRectangle(badgeBounds.toFloat(), 3.0f);
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)).boldened());
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.6f));
            g.drawText(juce::String(count), badgeBounds, juce::Justification::centred);
        }
    };

    drawTab(refsTabBounds_, activeTab_ == ActiveTab::References, "REFERENCE", (int)references_.size());
    drawTab(notesTabBounds_, activeTab_ == ActiveTab::Notes, "NOTAS", 0);

    // ─── Reference-Driven Mode toggle ─────────────────────────────────────
    {
        auto toggle = refModeToggleBounds_.toFloat();
        bool hovered = refModeToggleBounds_.contains(getMouseXYRelative());

        // Background pill
        g.setColour(referenceDrivenMode_
            ? MixCoachTheme::accentCyan().withAlpha(0.15f)
            : MixCoachTheme::bgDarker().withAlpha(0.30f));
        g.fillRoundedRectangle(toggle, 8.0f);

        // Border
        g.setColour(referenceDrivenMode_
            ? MixCoachTheme::accentCyan().withAlpha(0.50f)
            : MixCoachTheme::border().withAlpha(0.25f));
        g.drawRoundedRectangle(toggle, 8.0f, 0.8f);

        if (hovered && !referenceDrivenMode_) {
            g.setColour(MixCoachTheme::accentCyan().withAlpha(0.05f));
            g.fillRoundedRectangle(toggle, 8.0f);
        }

        // Glow when active
        if (referenceDrivenMode_) {
            auto glowArea = toggle.expanded(4.0f, 2.0f);
            juce::ColourGradient glow(
                MixCoachTheme::accentCyan().withAlpha(0.0f),  glowArea.getCentreX(), glowArea.getY(),
                MixCoachTheme::accentCyan().withAlpha(0.0f),  glowArea.getCentreX(), glowArea.getBottom(), false);
            glow.addColour(0.5f, MixCoachTheme::accentCyan().withAlpha(0.08f));
            g.setGradientFill(glow);
            g.fillRoundedRectangle(glowArea, 10.0f);
        }

        // Label
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)).boldened());
        g.setColour(referenceDrivenMode_
            ? MixCoachTheme::accentCyan()
            : MixCoachTheme::textMuted().withAlpha(0.5f));
        g.drawText("REF MODE", toggle.reduced(2, 0), juce::Justification::centredLeft);

        // Toggle knob (circle)
        float knobRadius = 5.0f;
        float knobX = referenceDrivenMode_
            ? toggle.getRight() - knobRadius - 4.0f
            : toggle.getX() + knobRadius + 4.0f;
        float knobY = toggle.getCentreY();

        juce::Colour knobCol = referenceDrivenMode_
            ? MixCoachTheme::accentCyan()
            : MixCoachTheme::textMuted().withAlpha(0.40f);

        g.setColour(knobCol);
        g.fillEllipse(knobX - knobRadius, knobY - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

        if (referenceDrivenMode_) {
            // Inner glow on knob
            g.setColour(juce::Colours::white.withAlpha(0.25f));
            g.fillEllipse(knobX - knobRadius * 0.5f, knobY - knobRadius * 0.5f, knobRadius, knobRadius);
        }
    }

    // ─── Reference-Driven progress bar ────────────────────────────────────
    if (referenceDrivenMode_ && refProgressBounds_.getWidth() > 0)
    {
        auto prog = refProgressBounds_.toFloat();

        // Background track
        g.setColour(MixCoachTheme::bgDarker().withAlpha(0.35f));
        g.fillRoundedRectangle(prog, 3.0f);

        // Filled portion (smoothly animated)
        float fill = juce::jlimit(0.0f, 1.0f, progressFillSmooth_.getCurrent());
        if (fill > 0.01f)
        {
            auto fillRect = prog.withWidth(prog.getWidth() * fill);

            juce::Colour progressCol;
            if (fill >= 0.80f)
                progressCol = MixCoachTheme::success();
            else if (fill >= 0.50f)
                progressCol = MixCoachTheme::warning();
            else
                progressCol = MixCoachTheme::error();

            juce::ColourGradient fillGrad(
                progressCol.brighter(0.2f).withAlpha(0.7f),
                fillRect.getX(), fillRect.getY(),
                progressCol.withAlpha(0.5f),
                fillRect.getRight(), fillRect.getY(),
                false);
            g.setGradientFill(fillGrad);
            g.fillRoundedRectangle(fillRect, 3.0f);

            // Shine
            auto shineRect = fillRect.withHeight(juce::jmin(3.0f, fillRect.getHeight() * 0.4f));
            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.fillRoundedRectangle(shineRect, 2.0f);
        }

        // Percentage text (uses RAW values for accuracy, not smoothed)
        juce::String pctText = juce::String((int)(referenceProgressRaw_ * 100.0f)) + "%";

        // Delta arrow with smooth fade-in/fade-out alpha
        float smoothDeltaMagnitude = std::abs(deltaSmooth_.getCurrent());
        if (std::abs(referenceDeltaRaw_) > 0.01f && smoothDeltaMagnitude > 0.005f)
        {
            float arrowAlpha = juce::jlimit(0.15f, 1.0f, smoothDeltaMagnitude * 6.0f);
            juce::String arrow = (referenceDeltaRaw_ > 0.0f) ? "\xE2\x96\xB2" : "\xE2\x96\xBC"; // ▲ or ▼
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)).boldened());
            g.setColour(MixCoachTheme::textPrimary().withAlpha(0.55f));
            g.drawText(pctText, prog.reduced(3, 0), juce::Justification::centredLeft);

            juce::String deltaStr = " " + arrow + juce::String(std::abs(referenceDeltaRaw_) * 100.0f, 0) + "%";
            juce::Colour arrowCol = (referenceDeltaRaw_ > 0.0f)
                ? MixCoachTheme::success().withAlpha(arrowAlpha * 0.8f)
                : MixCoachTheme::error().withAlpha(arrowAlpha * 0.8f);

            g.setColour(arrowCol);
            auto textW = juce::GlyphArrangement::getStringWidthInt(
                juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)).boldened(), pctText);
            auto deltaBounds = prog.translated((float)textW + 5.0f, 0.0f);
            g.drawText(deltaStr, deltaBounds.reduced(3, 0), juce::Justification::centredLeft);
        }
        else
        {
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)).boldened());
            g.setColour(MixCoachTheme::textPrimary().withAlpha(0.55f));
            g.drawText(pctText, prog.reduced(3, 0), juce::Justification::centredLeft);
        }

        // End marker "REF" label
        g.setFont(juce::Font(juce::FontOptions(5.5f)));
        g.setColour(MixCoachTheme::textDim().withAlpha(0.35f));
        g.drawText("MATCH", prog.reduced(3, 0), juce::Justification::centredRight);
    }

    // ─── Reference count badge (top-right pill) ───────────────────────────
    if (!references_.empty()) {
        auto badge = referenceCountLabel_.getBounds().toFloat();
        g.setColour(MixCoachTheme::accent().withAlpha(0.15f));
        g.fillRoundedRectangle(badge, 9.0f);
        g.setColour(MixCoachTheme::accent().withAlpha(0.4f));
        g.drawRoundedRectangle(badge, 9.0f, 0.5f);
    }

    if (activeTab_ == ActiveTab::Notes)
        return;

    if (references_.empty())
        return;

    // ─── Subtle separator line between drop zone and rows ────────────────
    {
        int dy = dropZone_.getBottom() + 1;
        if (dy > 0 && dropZone_.isVisible()) {
            auto area = getLocalBounds().reduced(8);
            float lx = (float)area.getX() + 20.0f;
            float rx = (float)area.getRight() - 20.0f;
            float fy = (float)dy;
            g.setColour(MixCoachTheme::border().withAlpha(0.12f));
            g.drawHorizontalLine(dy, lx, rx);
            g.setColour(MixCoachTheme::border().withAlpha(0.2f));
            g.fillEllipse((lx + rx) * 0.5f - 1.0f, fy - 1.0f, 2.0f, 2.0f);
        }
    }

    // ─── Draw reference rows ──────────────────────────────────────────────
    int drawn = 0;
    for (int i = (int)references_.size() - 1; i >= 0 && drawn < (int)visibleRowBounds_.size(); --i) {
        bool isGhost = isDraggingRow_ && (i == dragSourceArrayIdx_);
        drawReferenceRow(g, visibleRowBounds_[drawn], references_[i], i, drawn);

        if (isGhost) {
            auto row = visibleRowBounds_[drawn].toFloat();
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            g.fillRoundedRectangle(row, 4.0f);
            g.setColour(juce::Colours::white.withAlpha(0.04f));
            g.drawRoundedRectangle(row, 4.0f, 1.0f);
        }

        drawn++;
    }

    // ─── Drag & drop drop-indicator ────────────────────────────────────
    if (isDraggingRow_ && dropTargetVisualIdx_ >= 0) {
        int totalV = (int)visibleRowBounds_.size();
        if (dropTargetVisualIdx_ <= totalV) {
            if (dropTargetVisualIdx_ == 0)
                drawDropIndicator(g, visibleRowBounds_[0], true);
            else if (dropTargetVisualIdx_ >= totalV)
                drawDropIndicator(g, visibleRowBounds_.back(), false);
            else
                drawDropIndicator(g, visibleRowBounds_[dropTargetVisualIdx_ - 1], false);
        }
    }

    // More indicator
    int totalVisible = (int)visibleRowBounds_.size();
    if ((int)references_.size() > totalVisible) {
        g.setColour(MixCoachTheme::textMuted());
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)));
        g.drawText("+ " + juce::String(references_.size() - totalVisible) + " m\xC3\xA1" "s...",
                   visibleRowBounds_.back().translated(0, 24).reduced(4, 0),
                   juce::Justification::centredLeft);
    }

    // ─── Section selector ──────────────────────────────────────────────────
    if (!sections_.empty() && !isDraggingRow_)
    {
        auto bounds = getLocalBounds().reduced(4);
        auto selArea = bounds.removeFromBottom(22).reduced(4, 1);
        selArea.removeFromLeft(20);
        selArea.removeFromRight(20);
        if (selArea.getWidth() > 40)
            drawSectionSelector(g, selArea);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawReferenceRow
// ═══════════════════════════════════════════════════════════════════════════

void ReferencePanelComponent::drawReferenceRow(juce::Graphics& g,
                                               juce::Rectangle<int> bounds,
                                               const MixReference& ref,
                                               int index,
                                               int visualIndex)
{
    juce::ignoreUnused(index);
    auto row = bounds.toFloat();
    const float radius = 4.0f;

    bool isAudio = (ref.type == MixReference::Type::File);
    bool isPlaying = (playingRefIndex_ == index);
    bool isMatchRef = (matchRefIndex_ == index);

    if (visualIndex % 2 == 1 && !isPlaying && !isMatchRef) {
        g.setColour(MixCoachTheme::bgDarker().withAlpha(0.08f));
        g.fillRoundedRectangle(row, radius);
    }

    if (isPlaying) {
        g.setColour(MixCoachTheme::accentCyan().withAlpha(0.06f));
        g.fillRoundedRectangle(row, radius);
    }

    if (isMatchRef && !isPlaying) {
        g.setColour(MixCoachTheme::accent().withAlpha(0.08f));
        g.fillRoundedRectangle(row, radius);
        g.setColour(MixCoachTheme::accent().withAlpha(0.5f));
        g.fillRect(row.getX(), row.getY() + 2, 3.0f, row.getHeight() - 4);
    }

    float barAlpha = isPlaying ? 0.6f : (visualIndex % 2 == 0 ? 0.30f : 0.20f);
    g.setColour(ref.colour.withAlpha(barAlpha));
    g.fillRect(row.getX(), row.getY() + 3, 2.0f, row.getHeight() - 6);

    // ─── Delete button ──────────────────────────────────────────────────
    auto delArea = row.removeFromRight(18).withSizeKeepingCentre(12.0f, 12.0f);
    bool delHovered = (visualIndex < (int)deleteBtnBounds_.size()
                       && deleteBtnBounds_[visualIndex].contains(getMouseXYRelative()));
    g.setColour(delHovered ? MixCoachTheme::error() : MixCoachTheme::textMuted().withAlpha(0.35f));

    float dx = delArea.getCentreX();
    float dy = delArea.getCentreY();
    float ds = delArea.getWidth() * 0.35f;
    g.drawLine(dx - ds, dy - ds, dx + ds, dy + ds, 1.2f);
    g.drawLine(dx + ds, dy - ds, dx - ds, dy + ds, 1.2f);

    row.removeFromRight(2);

    // ─── Status dot ──────────────────────────────────────────────────────
    auto dotArea = row.removeFromRight(14).withSizeKeepingCentre(6.0f, 6.0f);
    {
        juce::Colour dotCol;
        switch (ref.analysisStatus) {
            case AnalysisStatus::Pending:   dotCol = MixCoachTheme::textMuted().withAlpha(0.4f); break;
            case AnalysisStatus::Analyzing: dotCol = MixCoachTheme::warning(); break;
            case AnalysisStatus::Ready:     dotCol = MixCoachTheme::success(); break;
            case AnalysisStatus::Error:     dotCol = MixCoachTheme::error(); break;
        }
        g.setColour(dotCol.withAlpha(0.15f));
        g.fillEllipse(dotArea.expanded(2.0f, 2.0f));
        g.setColour(dotCol);
        g.fillEllipse(dotArea);
    }

    // ─── Play/Pause/Link icon ───────────────────────────────────────────
    auto iconArea = row.removeFromLeft(24).withSizeKeepingCentre(16.0f, 16.0f);
    if (isAudio) {
        bool isHovered = (visualIndex < (int)playBtnBounds_.size()
                          && playBtnBounds_[visualIndex].contains(getMouseXYRelative()));

        if (isHovered || isPlaying) {
            g.setColour((isPlaying ? MixCoachTheme::accentCyan() : MixCoachTheme::accent()).withAlpha(0.12f));
            g.fillEllipse(iconArea.expanded(2.0f, 2.0f));
        }

        juce::Colour btnColour;
        if (isPlaying)            btnColour = MixCoachTheme::accentCyan();
        else if (isHovered)       btnColour = MixCoachTheme::accentGlow();
        else                      btnColour = MixCoachTheme::textDim().withAlpha(0.45f);

        if (isPlaying)
            drawPauseIcon(g, iconArea.getCentreX(), iconArea.getCentreY(), iconArea.getWidth(), btnColour);
        else
            drawPlayIcon(g, iconArea.getCentreX(), iconArea.getCentreY(), iconArea.getWidth(), btnColour);
    } else {
        drawLinkIcon(g, iconArea.getCentreX(), iconArea.getCentreY(), iconArea.getWidth(), ref.colour.withAlpha(isPlaying ? 0.9f : 0.5f));
    }

    // ─── Metadata Text ──────────────────────────────────────────────────
    juce::String metaText;
    if (isAudio) {
        if (ref.audioInfo.valid) {
            int mins = (int)(ref.audioInfo.durationSeconds) / 60;
            int secs = (int)(ref.audioInfo.durationSeconds) % 60;
            juce::String durationStr = juce::String(mins) + ":" + juce::String(secs).paddedLeft('0', 2);

            float srKhz = ref.audioInfo.sampleRate / 1000.0f;
            juce::String srStr = (srKhz == (int)srKhz) ? juce::String((int)srKhz) : juce::String(srKhz, 1);

            metaText = srStr + " kHz \xE2\x80\xA2 " + juce::String(ref.audioInfo.bitDepth) + "-bit \xE2\x80\xA2 " + durationStr;
        } else {
            metaText = "Archivo de Audio";
        }
    } else {
        if (ref.path.contains("youtube.com") || ref.path.contains("youtu.be"))
            metaText = "Enlace de YouTube";
        else if (ref.path.contains("spotify.com"))
            metaText = "Enlace de Spotify";
        else if (ref.path.contains("soundcloud.com"))
            metaText = "Enlace de SoundCloud";
        else
            metaText = "Enlace Web";
    }

    row.removeFromLeft(2);

    // ─── Name & Metadata Stack ────────────────────────────────────────────
    auto textBounds = row.reduced(2, 0);
    const float nameH      = 13.0f;
    const float metaH      = 11.0f;
    const float textGap    =  2.0f;
    const float totalTextH = nameH + metaH + textGap;
    const float textTopY   = textBounds.getCentreY() - totalTextH * 0.5f;
    auto nameBounds = juce::Rectangle<float>(
        textBounds.getX(), textTopY, textBounds.getWidth(), nameH).toNearestInt();
    auto metaBounds = juce::Rectangle<float>(
        textBounds.getX(), textTopY + nameH + textGap, textBounds.getWidth(), metaH).toNearestInt();

    if (ref.isFetchingTitle)
    {
        g.setColour(MixCoachTheme::accentCyan().withAlpha(0.7f));
        g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
        g.drawText("Obteniendo enlace...", nameBounds, juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
        g.drawText(ref.path, metaBounds, juce::Justification::centredLeft);
    }
    else
    {
        g.setColour(MixCoachTheme::textPrimary());
        g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());

        auto displayName = ref.name;
        g.drawText(displayName, nameBounds, juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)));
        g.setColour(MixCoachTheme::textMuted());
        g.drawText(metaText, metaBounds, juce::Justification::centredLeft);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  Mouse events — Click & Drag & Drop reorder
// ═══════════════════════════════════════════════════════════════════════════

void ReferencePanelComponent::mouseDown(const juce::MouseEvent& e)
{
    auto pos = e.getPosition();

    if (refsTabBounds_.contains(pos))  { switchTab(ActiveTab::References); return; }
    if (notesTabBounds_.contains(pos)) { switchTab(ActiveTab::Notes); return; }

    // ─── Reference-Driven Mode toggle ─────────────────────────────────────
    if (refModeToggleBounds_.contains(pos))
    {
        // Right-click: show context menu
        if (e.mods.isRightButtonDown())
        {
            juce::PopupMenu menu;
            menu.addItem(1, "Configurar gaps", true, false);
            menu.addItem(2, "Ver historial de match", true, false);
            menu.addItem(3, "Resetear progreso", true, false);

            menu.showMenuAsync(juce::PopupMenu::Options()
                .withTargetScreenArea(refModeToggleBounds_),
                [this](int result)
                {
                    if (onRefModeMenuAction)
                    {
                        if (result == 1) onRefModeMenuAction(RefModeMenuAction::ConfigureGaps);
                        else if (result == 2) onRefModeMenuAction(RefModeMenuAction::ViewHistory);
                        else if (result == 3) onRefModeMenuAction(RefModeMenuAction::ResetProgress);
                    }
                });
            return;
        }

        // Left-click: toggle mode
        referenceDrivenMode_ = !referenceDrivenMode_;
        resized();  // resized() also calls repaint() and reclaims/allocates progress bar space
        if (onReferenceDrivenModeToggled)
            onReferenceDrivenModeToggled(referenceDrivenMode_);
        return;
    }

    if (activeTab_ == ActiveTab::Notes) return;

    isDraggingRow_ = false;
    dragSourceVisualIdx_ = -1;
    dragSourceArrayIdx_ = -1;
    dropTargetVisualIdx_ = -1;
    mouseDownPos_ = pos;

    if (!sections_.empty())
    {
        for (size_t si = 0; si < sectionBtnBounds_.size(); ++si)
        {
            if (sectionBtnBounds_[si].contains(pos))
            {
                if (onSectionSelected)
                    onSectionSelected(static_cast<int>(si) - 1);
                if (onSectionSeekTo && si < sections_.size())
                    onSectionSeekTo(sections_[si].startSeconds);
                return;
            }
        }
    }

    int drawn = 0;
    for (int i = (int)references_.size() - 1; i >= 0 && drawn < (int)visibleRowBounds_.size(); --i, ++drawn) {
        auto& rowBounds = visibleRowBounds_[drawn];
        if (!rowBounds.contains(pos))
            continue;

        auto& ref = references_[i];

        if (ref.type == MixReference::Type::File
            && drawn < (int)playBtnBounds_.size()
            && playBtnBounds_[drawn].contains(pos)) {
            dragSourceVisualIdx_ = drawn;
            dragSourceArrayIdx_ = i;
            return;
        }

        if (drawn < (int)deleteBtnBounds_.size()
            && deleteBtnBounds_[drawn].contains(pos)) {
            removeReference(i);
            return;
        }

        dragSourceVisualIdx_ = drawn;
        dragSourceArrayIdx_ = i;
        return;
    }
}

void ReferencePanelComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (activeTab_ == ActiveTab::Notes)
        return;

    if (dragSourceVisualIdx_ < 0 || dragSourceArrayIdx_ < 0)
        return;

    auto pos = e.getPosition();

    if (!isDraggingRow_) {
        int dx = std::abs(pos.x - mouseDownPos_.x);
        int dy = std::abs(pos.y - mouseDownPos_.y);
        if (dx + dy < 8)
            return;
        isDraggingRow_ = true;
    }

    int totalVisible = (int)visibleRowBounds_.size();
    int newDropTarget = totalVisible;

    for (int v = 0; v < totalVisible; ++v) {
        auto& row = visibleRowBounds_[v];
        if (pos.y < row.getY()) {
            newDropTarget = v;
            break;
        }
        if (pos.y >= row.getY() && pos.y <= row.getBottom()) {
            int midY = row.getY() + row.getHeight() / 2;
            newDropTarget = (pos.y < midY) ? v : (v + 1);
            break;
        }
    }

    if (newDropTarget != dropTargetVisualIdx_) {
        dropTargetVisualIdx_ = newDropTarget;
        repaint();
    }

    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
}

void ReferencePanelComponent::mouseUp(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);

    if (activeTab_ == ActiveTab::Notes)
        return;

    if (isDraggingRow_) {
        if (dropTargetVisualIdx_ >= 0
            && dragSourceVisualIdx_ >= 0
            && dragSourceArrayIdx_ >= 0
            && dropTargetVisualIdx_ != dragSourceVisualIdx_)
        {
            int arraySrc = dragSourceArrayIdx_;

            auto ref = references_[arraySrc];
            references_.erase(references_.begin() + arraySrc);

            int insertAt = (int)references_.size() - dropTargetVisualIdx_;
            insertAt = juce::jlimit(0, (int)references_.size(), insertAt);
            references_.insert(references_.begin() + insertAt, ref);

            refreshDisplay();
            if (onReferencesChanged) onReferencesChanged();
        }

        isDraggingRow_ = false;
        dragSourceVisualIdx_ = -1;
        dragSourceArrayIdx_ = -1;
        dropTargetVisualIdx_ = -1;
        setMouseCursor(juce::MouseCursor::NormalCursor);
        repaint();
        return;
    }

    auto pos = e.getPosition();

    if (dragSourceVisualIdx_ < 0)
        return;

    int drawn = dragSourceVisualIdx_;
    int i = dragSourceArrayIdx_;
    auto& ref = references_[i];

    if (ref.type == MixReference::Type::File
        && drawn < (int)playBtnBounds_.size()
        && playBtnBounds_[drawn].contains(pos)) {
        if (onPlayReference) onPlayReference(i);
    }
    else if (ref.type == MixReference::Type::File)
    {
        if (matchRefIndex_ == i) {
            matchRefIndex_ = -1;
            if (onReferenceSelected)
                onReferenceSelected(-1);
        } else {
            matchRefIndex_ = i;
            if (onReferenceSelected)
                onReferenceSelected(i);
        }
        repaint();
    }
    else if (ref.type == MixReference::Type::URL) {
        juce::URL(ref.path).launchInDefaultBrowser();
    }

    if (ref.type != MixReference::Type::File)
    {
        dragSourceVisualIdx_ = -1;
        dragSourceArrayIdx_ = -1;
    }
}

void ReferencePanelComponent::mouseMove(const juce::MouseEvent& e)
{
    auto pos = e.getPosition();

    bool overInteractive = refsTabBounds_.contains(pos)
                        || notesTabBounds_.contains(pos);

    if (!overInteractive) {
        for (size_t i = 0; i < playBtnBounds_.size(); ++i) {
            if (playBtnBounds_[i].contains(pos)) { overInteractive = true; break; }
        }
    }
    if (!overInteractive) {
        for (size_t i = 0; i < deleteBtnBounds_.size(); ++i) {
            if (deleteBtnBounds_[i].contains(pos)) { overInteractive = true; break; }
        }
    }
    if (!overInteractive && activeTab_ == ActiveTab::References) {
        for (size_t i = 0; i < visibleRowBounds_.size(); ++i) {
            if (visibleRowBounds_[i].contains(pos)) { overInteractive = true; break; }
        }
    }

    if (!overInteractive && activeTab_ == ActiveTab::References)
    {
        for (size_t i = 0; i < sectionBtnBounds_.size(); ++i) {
            if (sectionBtnBounds_[i].contains(pos)) { overInteractive = true; break; }
        }
    }

    bool nowHovering = overInteractive;
    if (nowHovering != wasHovering_ || (!sections_.empty() && activeTab_ == ActiveTab::References)) {
        bool hoverChanged = (nowHovering != wasHovering_);
        if (!hoverChanged && !sectionBtnBounds_.empty()) {
            bool wasInSection = false, nowInSection = false;
            for (size_t i = 0; i < sectionBtnBounds_.size(); ++i) {
                if (sectionBtnBounds_[i].contains(mouseDownPos_)) { wasInSection = true; }
                if (sectionBtnBounds_[i].contains(pos)) { nowInSection = true; }
            }
            hoverChanged = (wasInSection != nowInSection);
        }
        if (hoverChanged)
            repaint();
    }

    wasHovering_ = nowHovering;

    setMouseCursor(overInteractive
        ? juce::MouseCursor::PointingHandCursor
        : juce::MouseCursor::NormalCursor);
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawDropIndicator
// ═══════════════════════════════════════════════════════════════════════════

void ReferencePanelComponent::drawDropIndicator(juce::Graphics& g,
                                                 juce::Rectangle<int> rowBounds,
                                                 bool above)
{
    float y = above ? (float)(rowBounds.getY() - 1)
                    : (float)(rowBounds.getBottom() + 1);
    float left  = (float)rowBounds.getX() + 4.0f;
    float right = (float)rowBounds.getRight() - 4.0f;
    float cx = (left + right) * 0.5f;

    auto glowBounds = juce::Rectangle<float>(cx - 40.0f, y - 4.0f, 80.0f, 8.0f);
    juce::ColourGradient glow(
        MixCoachTheme::accent().withAlpha(0.0f),  left, y,
        MixCoachTheme::accent().withAlpha(0.0f),  right, y, false);
    glow.addColour(0.35f, MixCoachTheme::accent().withAlpha(0.20f));
    glow.addColour(0.50f, MixCoachTheme::accent().withAlpha(0.35f));
    glow.addColour(0.65f, MixCoachTheme::accent().withAlpha(0.20f));
    g.setGradientFill(glow);
    g.fillRoundedRectangle(glowBounds, 4.0f);

    g.setColour(MixCoachTheme::accent().withAlpha(0.7f));
    g.drawHorizontalLine((int)y, left + 8.0f, right - 8.0f);

    float dotR = 2.0f;
    g.fillEllipse(left + 8.0f - dotR, y - dotR, dotR * 2.0f, dotR * 2.0f);
    g.fillEllipse(right - 8.0f - dotR, y - dotR, dotR * 2.0f, dotR * 2.0f);

    g.setColour(MixCoachTheme::accent().withAlpha(0.4f));
    juce::Path arrow;
    float aY = above ? (y + 4.0f) : (y - 4.0f);
    float dir = above ? 1.0f : -1.0f;
    arrow.addTriangle(cx - 4.0f, aY - dir * 3.0f,
                      cx,        aY + dir * 4.0f,
                      cx + 4.0f, aY - dir * 3.0f);
    g.fillPath(arrow);
}

// ═══════════════════════════════════════════════════════════════════════════
//  updatePlaybackPosition
// ═══════════════════════════════════════════════════════════════════════════

void ReferencePanelComponent::updatePlaybackPosition(double positionSeconds, double totalSeconds)
{
    playbackPosition_ = positionSeconds;
    playbackTotal_ = totalSeconds;

    if (!isDraggingSeek_)
    {
        double progress = (totalSeconds > 0.0) ? (positionSeconds / totalSeconds) : 0.0;
        seekSlider_.setValue(progress, juce::dontSendNotification);
    }

    auto fmt = [](double secs) -> juce::String {
        int m = (int)(secs) / 60;
        int s = (int)(secs) % 60;
        return juce::String(m) + ":" + juce::String(s).paddedLeft('0', 2);
    };
    positionLabel_.setText(fmt(positionSeconds) + " / " + fmt(totalSeconds), juce::dontSendNotification);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Section data
// ═══════════════════════════════════════════════════════════════════════════

void ReferencePanelComponent::setSectionData(const std::vector<SectionInfo>& sections, int activeIndex)
{
    sections_ = sections;
    activeSection_ = activeIndex;
    sectionBtnBounds_.clear();
    repaint();
}

void ReferencePanelComponent::drawSectionSelector(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (sections_.empty())
        return;

    int numSections = (int)sections_.size();
    int gap = 3;
    int totalGap = gap * (numSections - 1);
    int btnW = (bounds.getWidth() - totalGap) / numSections;
    if (btnW < 12) return;

    auto sectionColour = [](const juce::String& label) -> juce::Colour {
        juce::String lower = label.toLowerCase();
        if (lower == "full")     return MixCoachTheme::accent().withAlpha(0.5f);
        if (lower == "intro")    return MixCoachTheme::textMuted().withAlpha(0.5f);
        if (lower == "chorus")   return MixCoachTheme::accentGlow();
        if (lower == "verse")    return MixCoachTheme::accentCyan();
        if (lower == "bridge")   return MixCoachTheme::warning();
        if (lower == "outro")    return MixCoachTheme::textMuted().withAlpha(0.5f);
        if (lower == "build")    return MixCoachTheme::success();
        if (lower == "drop")     return MixCoachTheme::error();
        if (lower == "solo")     return MixCoachTheme::accent();
        return MixCoachTheme::accent().withAlpha(0.35f);
    };

    sectionBtnBounds_.clear();

    auto mousePos = getMouseXYRelative();

    for (int i = 0; i < numSections; ++i)
    {
        int x = bounds.getX() + i * (btnW + gap);
        auto btnBounds = juce::Rectangle<int>(x, bounds.getY(), btnW, bounds.getHeight());
        sectionBtnBounds_.push_back(btnBounds);

        bool isActive = (i == activeSection_);
        bool isGlobal = (i == 0 && sections_[i].label == "Full");
        bool isHovered = btnBounds.contains(mousePos);

        auto btnFloat = btnBounds.toFloat();
        juce::Colour accent = sectionColour(sections_[i].label);

        if (isActive)
        {
            g.setColour(accent.withAlpha(0.20f));
            g.fillRoundedRectangle(btnFloat, 4.0f);
            g.setColour(accent.withAlpha(0.55f));
            g.drawRoundedRectangle(btnFloat, 4.0f, 1.0f);

            auto underline = juce::Rectangle<float>(
                btnFloat.getX() + 4.0f,
                btnFloat.getBottom() - 2.0f,
                btnFloat.getWidth() - 8.0f,
                2.0f);
            g.setColour(accent.withAlpha(0.8f));
            g.fillRoundedRectangle(underline, 1.0f);
            g.setColour(accent.withAlpha(0.2f));
            g.fillRoundedRectangle(underline.expanded(4.0f, 0.0f), 1.0f);
        }
        else if (isHovered)
        {
            g.setColour(accent.withAlpha(0.12f));
            g.fillRoundedRectangle(btnFloat, 4.0f);
            g.setColour(accent.withAlpha(0.35f));
            g.drawRoundedRectangle(btnFloat, 4.0f, 0.8f);
        }
        else
        {
            g.setColour(MixCoachTheme::bgDarker().withAlpha(0.15f));
            g.fillRoundedRectangle(btnFloat, 4.0f);
            g.setColour(MixCoachTheme::border().withAlpha(0.12f));
            g.drawRoundedRectangle(btnFloat, 4.0f, 0.5f);
        }

        juce::String displayLabel;
        if (isGlobal)
            displayLabel = "All";
        else
            displayLabel = sections_[i].label;

        int maxChars = juce::jmax(2, (btnW - 8) / 6);
        if (displayLabel.length() > maxChars)
            displayLabel = displayLabel.substring(0, juce::jmax(1, maxChars - 2)) + "..";

        g.setColour(isActive ? accent.brighter(0.3f)
                   : isHovered ? accent.withAlpha(0.7f)
                   : MixCoachTheme::textMuted().withAlpha(0.55f));

        auto labelArea = btnBounds.withTrimmedBottom(btnBounds.getHeight() / 3);
        g.drawText(displayLabel, labelArea, juce::Justification::centred);

        if (!isGlobal && btnW >= 28)
        {
            float duration = sections_[i].endSeconds - sections_[i].startSeconds;
            juce::String durStr = juce::String((int)duration) + "s";
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.35f));
            auto durArea = btnBounds.removeFromBottom(btnBounds.getHeight() * 2 / 5).reduced(0, 1);
            g.drawText(durStr, durArea, juce::Justification::centred);
        }
    }
}

void ReferencePanelComponent::updateMatchData(const DifferenceProfile& data)
{
    matchPanel_.updateMatchData(data);
    repaint();
}

// ═══════════════════════════════════════════════════════════════════════════
//  Persistencia
// ═══════════════════════════════════════════════════════════════════════════

std::vector<juce::String> ReferencePanelComponent::getFilePaths() const
{
    std::vector<juce::String> paths;
    for (const auto& ref : references_)
        if (ref.type == MixReference::Type::File)
            paths.push_back(ref.path);
    return paths;
}

std::vector<juce::String> ReferencePanelComponent::getURLs() const
{
    std::vector<juce::String> urls;
    for (const auto& ref : references_)
        if (ref.type == MixReference::Type::URL)
            urls.push_back(ref.path);
    return urls;
}

void ReferencePanelComponent::restoreFromPaths(const std::vector<juce::String>& filePaths,
                                                const std::vector<juce::String>& urls)
{
    bool hadChanges = false;
    for (const auto& path : filePaths) {
        auto f = juce::File(path);
        if (f.existsAsFile()) {
            addFileReference(path);
            hadChanges = true;
        }
    }
    for (const auto& url : urls) {
        addURLReference(url);
        hadChanges = true;
    }
    if (hadChanges) {
        if (onReferencesChanged) onReferencesChanged();
    }
}

} // namespace mixcoach
