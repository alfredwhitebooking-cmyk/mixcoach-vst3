#include "ReferencePanelComponent.h"
#include <cmath>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  DropZoneComponent Implementation
// ═══════════════════════════════════════════════════════════════════════════

DropZoneComponent::DropZoneComponent()
{
    browseButton_.setButtonText("EXPLORAR ARCHIVOS");
    browseButton_.setColour(juce::TextButton::buttonColourId, juce::Colour(0x00000000)); // transparent bg
    browseButton_.setColour(juce::TextButton::textColourOffId, MixCoachTheme::accentGlow());
    browseButton_.setColour(juce::TextButton::buttonOnColourId, MixCoachTheme::accent().withAlpha(0.15f));
    browseButton_.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    browseButton_.onClick = [this]() { browseForFiles(); };
    addAndMakeVisible(browseButton_);

    setMouseCursor(juce::MouseCursor::NormalCursor);
}

DropZoneComponent::~DropZoneComponent() = default;

void DropZoneComponent::resized()
{
    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(40);  // icon + text area
    browseButton_.setBounds(area.removeFromTop(22).withSizeKeepingCentre(160, 22));
}

void DropZoneComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    const float radius = 8.0f;

    // ─── Fondo según estado ─────────────────────────────────────────────
    if (isDragging_) {
        g.setColour(MixCoachTheme::accent().withAlpha(0.12f));
        g.fillRoundedRectangle(bounds, radius);
        g.setColour(MixCoachTheme::accent().withAlpha(0.7f));
    } else if (isHovering_) {
        g.setColour(MixCoachTheme::accent().withAlpha(0.06f));
        g.fillRoundedRectangle(bounds, radius);
        g.setColour(MixCoachTheme::accentDim().withAlpha(0.5f));
    } else {
        g.setColour(MixCoachTheme::bgDarker().withAlpha(0.4f));
        g.fillRoundedRectangle(bounds, radius);
        g.setColour(MixCoachTheme::border().withAlpha(0.4f));
    }

    // ─── Borde dashed cuando se arrastra ────────────────────────────────
    if (isDragging_) {
        juce::Path dashRect;
        dashRect.addRoundedRectangle(bounds, radius);
        g.strokePath(dashRect, juce::PathStrokeType(1.5f));
    } else {
        g.drawRoundedRectangle(bounds, radius, 1.0f);
    }

    if (!showHint_) return;

    // ─── Zona de texto e icono (la mitad superior) ──────────────────────
    auto textArea = bounds.reduced(12, 8);
    auto iconArea = textArea.removeFromTop(24);

    // ─── Icono nube-upload (☁) grande ──────────────────────────────────
    g.setFont(juce::Font(juce::FontOptions(18.0f)));
    if (isDragging_) {
        g.setColour(MixCoachTheme::accentGlow());
    } else {
        g.setColour(MixCoachTheme::textDim().withAlpha(0.6f));
    }
    g.drawText(juce::CharPointer_UTF8("\xE2\x98\x81"), iconArea, juce::Justification::centred);

    // ─── Texto principal ────────────────────────────────────────────────
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
    g.setColour(isDragging_ ? MixCoachTheme::accentGlow() : MixCoachTheme::textDim());
    g.drawText(juce::String("Arrastra y suelta archivos aqu\xC3\xAD o"),
               textArea.removeFromTop(16), juce::Justification::centred);

    // ─── Formatos soportados (parte inferior, debajo del boton) ─────────
    auto fmtArea = bounds.removeFromBottom(14).reduced(12, 0);
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.6f));
    g.drawText(juce::String("WAV, AIFF, FLAC - arrastra o explora"),
               fmtArea, juce::Justification::centred);
}

void DropZoneComponent::setShowDropHint(bool showHint)
{
    showHint_ = showHint;
    repaint();
}

void DropZoneComponent::browseForFiles()
{
    // Use JUCE FileChooser to pick audio files
    auto* chooser = new juce::FileChooser(
        "Seleccionar archivos de audio",
        juce::File(),
        "*.wav;*.aiff;*.aif;*.flac;*.mp3;*.ogg");

    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                         juce::FileBrowserComponent::canSelectMultipleItems,
        [this](const juce::FileChooser& fc) {
            auto results = fc.getResults();
            for (auto& f : results) {
                if (onFileDropped)
                    onFileDropped(f.getFullPathName());
            }
        });
}

void DropZoneComponent::mouseEnter(const juce::MouseEvent&) { isHovering_ = true; repaint(); }
void DropZoneComponent::mouseExit(const juce::MouseEvent&)  { isHovering_ = false; repaint(); }

void DropZoneComponent::fileDragEnter(const juce::StringArray&, int, int)
{
    isDragging_ = true;
    repaint();
}

void DropZoneComponent::fileDragExit(const juce::StringArray&)
{
    isDragging_ = false;
    repaint();
}

void DropZoneComponent::filesDropped(const juce::StringArray& files, int, int)
{
    isDragging_ = false;
    for (const auto& f : files) {
        auto file = juce::File(f);
        auto ext = file.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".mp3" || ext == ".flac" ||
            ext == ".aiff" || ext == ".aif" || ext == ".ogg") {
            if (onFileDropped)
                onFileDropped(file.getFullPathName());
        }
    }
    repaint();
}

// ═══════════════════════════════════════════════════════════════════════════
//  AudioFileInfo helper — Lee metadata del archivo usando JUCE
// ═══════════════════════════════════════════════════════════════════════════
AudioFileInfo ReferencePanelComponent::readAudioFileInfo(const juce::File& file)
{
    AudioFileInfo info;

    if (!file.existsAsFile())
        return info;

    // Use AudioFormatManager to read file header
    juce::AudioFormatManager formatMgr;
    formatMgr.registerBasicFormats();

    auto* reader = formatMgr.createReaderFor(file);
    if (reader == nullptr)
        return info;

    info.sampleRate      = (int)reader->sampleRate;
    info.bitDepth        = reader->bitsPerSample;
    info.durationSeconds = reader->lengthInSamples / reader->sampleRate;
    info.valid           = true;

    delete reader;
    return info;
}

// ═══════════════════════════════════════════════════════════════════════════
//  ReferencePanelComponent Implementation
// ═══════════════════════════════════════════════════════════════════════════

ReferencePanelComponent::ReferencePanelComponent()
{
    // ─── Drop zone ─────────────────────────────────────────────────────────
    dropZone_.onFileDropped = [this](const juce::String& path) {
        addFileReference(path);
    };
    addAndMakeVisible(dropZone_);

    // ─── URL input ─────────────────────────────────────────────────────────
    urlInput_.setMultiLine(false);
    urlInput_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)));
    urlInput_.setTextToShowWhenEmpty("Pega un enlace de YouTube, Spotify o audio...", MixCoachTheme::textMuted());
    urlInput_.setColour(juce::TextEditor::backgroundColourId, MixCoachTheme::bgDarker());
    urlInput_.setColour(juce::TextEditor::textColourId, MixCoachTheme::textPrimary());
    urlInput_.setColour(juce::TextEditor::outlineColourId, MixCoachTheme::border());
    urlInput_.setColour(juce::TextEditor::focusedOutlineColourId, MixCoachTheme::accent());
    urlInput_.setIndents(8, 4);
    urlInput_.addListener(this);
    addAndMakeVisible(urlInput_);

    // ─── Add URL button ────────────────────────────────────────────────────
    addUrlButton_.setButtonText(juce::CharPointer_UTF8("\xE2\x9E\x95"));  // ➕
    addUrlButton_.setColour(juce::TextButton::buttonColourId, MixCoachTheme::accent().withAlpha(0.3f));
    addUrlButton_.setColour(juce::TextButton::textColourOffId, MixCoachTheme::accentGlow());
    addUrlButton_.onClick = [this]() {
        auto url = urlInput_.getText().trim();
        if (url.isNotEmpty()) {
            addURLReference(url);
            urlInput_.clear();
        }
    };
    addAndMakeVisible(addUrlButton_);

    // ─── Reference count ───────────────────────────────────────────────────
    referenceCountLabel_.setText("0 referencias", juce::dontSendNotification);
    referenceCountLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    referenceCountLabel_.setJustificationType(juce::Justification::centredRight);
    referenceCountLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
    addAndMakeVisible(referenceCountLabel_);

    // ─── Inicializar visibilidad de componentes según tab activa ────────────
    switchTab(ActiveTab::AudioRefs);

    // ─── Empty state ───────────────────────────────────────────────────────
    emptyLabel_.setText(juce::String(juce::CharPointer_UTF8("\xF0\x9F\x94\x8D Carga referencias para comparar tu mezcla")), juce::dontSendNotification);
    emptyLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
    emptyLabel_.setJustificationType(juce::Justification::centred);
    emptyLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
    addAndMakeVisible(emptyLabel_);
}

// ─── Switch active tab ───────────────────────────────────────────────────────

void ReferencePanelComponent::switchTab(ActiveTab tab)
{
    activeTab_ = tab;
    bool isAudioTab = (tab == ActiveTab::AudioRefs);

    dropZone_.setVisible(isAudioTab);
    urlInput_.setVisible(!isAudioTab);
    addUrlButton_.setVisible(!isAudioTab);
    referenceCountLabel_.setVisible(isAudioTab);
    emptyLabel_.setText(isAudioTab
        ? juce::String(juce::CharPointer_UTF8("\xF0\x9F\x94\x8D Carga referencias para comparar tu mezcla"))
        : juce::String(juce::CharPointer_UTF8("\xF0\x9F\x94\x97 Agrega enlaces de referencia")),
        juce::dontSendNotification);

    resized();
    repaint();
}

// ─── Add file reference ──────────────────────────────────────────────────────

void ReferencePanelComponent::addFileReference(const juce::String& filePath)
{
    auto file = juce::File(filePath);
    auto info = readAudioFileInfo(file);

    MixReference ref;
    ref.type = MixReference::Type::File;
    ref.name = file.getFileName();
    ref.path = filePath;
    // Asignar color según el índice (cíclico entre violeta, cyan, verde)
    static const juce::Colour kRowColours[] = {
        juce::Colour(0xFF8B5CF6),  // violeta
        juce::Colour(0xFF00B4D8),  // cyan
        juce::Colour(0xFF22C55E),  // verde
    };
    int colourIdx = (int)references_.size() % 3;
    ref.colour = kRowColours[colourIdx];
    ref.addedTime = juce::Time::getMillisecondCounter();
    ref.audioInfo = info;
    references_.push_back(ref);
    refreshDisplay();
    if (onReferencesChanged) onReferencesChanged();
}

// ─── Add URL reference ───────────────────────────────────────────────────────

void ReferencePanelComponent::addURLReference(const juce::String& url)
{
    MixReference ref;
    ref.type = MixReference::Type::URL;
    auto name = url;
    if (url.contains("youtube.com") || url.contains("youtu.be"))
        name = "\xF0\x9F\x8E\xAC YouTube";
    else if (url.contains("spotify.com"))
        name = "\xF0\x9F\x8E\xB5 Spotify";
    else if (url.contains("soundcloud.com"))
        name = "\xF0\x9F\x8E\xB5 SoundCloud";
    else
        name = "\xF0\x9F\x94\x97 " + url.substring(0, 36) + "...";

    ref.name = name;
    ref.path = url;
    ref.colour = MixCoachTheme::accent2().withAlpha(0.7f);
    ref.addedTime = juce::Time::getMillisecondCounter();
    references_.push_back(ref);
    refreshDisplay();
    if (onReferencesChanged) onReferencesChanged();
}

// ─── Remove / Clear ──────────────────────────────────────────────────────────

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

// ─── Refresh display ──────────────────────────────────────────────────────────

void ReferencePanelComponent::refreshDisplay()
{
    referenceCountLabel_.setText(
        juce::String(references_.size()) + " referencia" + (references_.size() != 1 ? "s" : ""),
        juce::dontSendNotification);
    emptyLabel_.setVisible(references_.empty());
    repaint();
    resized();
}

// ─── Layout ──────────────────────────────────────────────────────────────────

void ReferencePanelComponent::resized()
{
    auto area = getLocalBounds().reduced(4);

    // ─── Sub-tabs row: REFERENCIAS DE AUDIO | ENLACES ÚTILES ────────────────
    auto tabRow = area.removeFromTop(18);
    audioTabBounds_ = tabRow.removeFromLeft(130).reduced(0, 2);
    linksTabBounds_ = tabRow.removeFromLeft(100).reduced(0, 2);
    referenceCountLabel_.setBounds(tabRow.removeFromRight(80).reduced(0, 2));

    area.removeFromTop(2);

    bool isAudio = (activeTab_ == ActiveTab::AudioRefs);

    if (isAudio) {
        // ─── Drop zone (solo tab audio) ───────────────────────────────────
        auto dropArea = area.removeFromTop(100);
        dropZone_.setBounds(dropArea.reduced(2));
    } else {
        // ─── URL input + button (solo tab links) ──────────────────────────
        auto urlArea = area.removeFromTop(28);
        addUrlButton_.setBounds(urlArea.removeFromRight(28));
        urlInput_.setBounds(urlArea.reduced(0, 2));
    }

    area.removeFromTop(2);

    // ─── Lista de referencias (resto del espacio) ──────────────────────────
    visibleRowBounds_.clear();
    int rowH = 24;
    int maxVisible = area.getHeight() / rowH;
    int totalToDraw = juce::jmin((int)references_.size(), maxVisible);
    int drawn = 0;

    for (int i = (int)references_.size() - 1; i >= 0 && drawn < totalToDraw; --i) {
        auto rowArea = area.removeFromTop(rowH).reduced(2, 1);
        visibleRowBounds_.push_back(rowArea);
        drawn++;
    }

    // ─── Empty state label ─────────────────────────────────────────────────
    emptyLabel_.setBounds(area);
}

// ─── Paint ──────────────────────────────────────────────────────────────────

void ReferencePanelComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // ─── Fondo del panel ──────────────────────────────────────────────────
    MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), 6.0f);

    // ─── Sub-tabs ─────────────────────────────────────────────────────────
    bool isAudioActive = (activeTab_ == ActiveTab::AudioRefs);

    // Tab: REFERENCIAS DE AUDIO
    {
        g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
        g.setColour(isAudioActive ? MixCoachTheme::accentGlow() : MixCoachTheme::textMuted().withAlpha(0.5f));
        g.drawText("REFERENCIAS DE AUDIO", audioTabBounds_, juce::Justification::centred);

        // Underline for active tab
        if (isAudioActive) {
            g.setColour(MixCoachTheme::accent().withAlpha(0.6f));
            g.fillRect(audioTabBounds_.getX(), audioTabBounds_.getBottom() - 1,
                       audioTabBounds_.getWidth(), 2);
        }

        // Reference count next to audio tab label
        g.setFont(juce::Font(juce::FontOptions(6.5f)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
        auto countArea = audioTabBounds_.translated(audioTabBounds_.getWidth() + 2, 0)
                         .withWidth(40);
        int fileCount = 0;
        for (auto& r : references_)
            if (r.type == MixReference::Type::File) fileCount++;
        g.drawText(juce::String(fileCount), countArea, juce::Justification::centredLeft);
    }

    // Tab: ENLACES ÚTILES
    {
        g.setFont(juce::Font(juce::FontOptions(7.5f)).boldened());
        g.setColour(isAudioActive ? MixCoachTheme::textMuted().withAlpha(0.5f) : MixCoachTheme::accentGlow());
    g.drawText(juce::String("ENLACES \xC3\x9ATILES"),
               linksTabBounds_, juce::Justification::centred);

        if (!isAudioActive) {
            g.setColour(MixCoachTheme::accent().withAlpha(0.6f));
            g.fillRect(linksTabBounds_.getX(), linksTabBounds_.getBottom() - 1,
                       linksTabBounds_.getWidth(), 2);
        }

        int linkCount = 0;
        for (auto& r : references_)
            if (r.type == MixReference::Type::URL) linkCount++;
        g.setFont(juce::Font(juce::FontOptions(6.5f)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
        auto countArea = linksTabBounds_.translated(linksTabBounds_.getWidth() + 2, 0)
                         .withWidth(40);
        g.drawText(juce::String(linkCount), countArea, juce::Justification::centredLeft);
    }

    if (references_.empty()) return;

    // ─── Dibujar filas de referencias ─────────────────────────────────────
    int drawn = 0;
    for (int i = (int)references_.size() - 1; i >= 0 && drawn < (int)visibleRowBounds_.size(); ++i) {
        drawReferenceRow(g, visibleRowBounds_[drawn], references_[i], i);
        drawn++;
    }

    // Si hay más de las que caben, indicarlo
    int totalVisible = (int)visibleRowBounds_.size();
    if ((int)references_.size() > totalVisible) {
        g.setColour(MixCoachTheme::textMuted());
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
        g.drawText(juce::String("+ ") + juce::String(references_.size() - totalVisible) + juce::String(" m\xC3\xA1s..."),
                   visibleRowBounds_.back().translated(0, 22).reduced(4, 0),
                   juce::Justification::centredLeft);
    }
}

// ─── Draw reference row (delegate según tipo) ────────────────────────────────

void ReferencePanelComponent::drawReferenceRow(juce::Graphics& g, juce::Rectangle<int> bounds,
                                               const MixReference& ref, int index)
{
    if (ref.type == MixReference::Type::File)
        drawAudioRow(g, bounds, ref, index);
    else
        drawLinkRow(g, bounds, ref, index);
}

// ─── Draw audio reference row ─────────────────────────────────────────────────

void ReferencePanelComponent::drawAudioRow(juce::Graphics& g, juce::Rectangle<int> bounds,
                                           const MixReference& ref, int index)
{
    juce::ignoreUnused(index);
    auto row = bounds.toFloat();
    const float radius = 4.0f;

    // ─── Row background ─────────────────────────────────────────────────
    g.setColour(MixCoachTheme::bgDarker().withAlpha(0.35f));
    g.fillRoundedRectangle(row, radius);

    // ─── Play button ▶ (leftmost) ───────────────────────────────────────
    auto playArea = row.removeFromLeft(20).reduced(4, 6);
    juce::Path playPath;
    playPath.addTriangle(playArea.getX() + 2, playArea.getY(),
                         playArea.getRight(), playArea.getCentreY(),
                         playArea.getX() + 2, playArea.getBottom());
    g.setColour(MixCoachTheme::textDim().withAlpha(0.6f));
    g.fillPath(playPath);

    // ─── Waveform bar (colored) ──────────────────────────────────────────
    auto waveformArea = row.removeFromLeft(40).reduced(2, 4);
    g.setColour(ref.colour.withAlpha(0.25f));
    g.fillRoundedRectangle(waveformArea, 2.0f);

    // Draw waveform bars (simulated from file name hash, not actual audio)
    float barW = 3.0f;
    float gap = 1.5f;
    float totalBars = waveformArea.getWidth() / (barW + gap);
    int numBars = juce::jmin((int)totalBars, 10);
    float peakGuess = 0.3f + (ref.name.hash() & 0x3F) / 128.0f;

    for (int i = 0; i < numBars; ++i) {
        float x = waveformArea.getX() + i * (barW + gap);
        float h = waveformArea.getHeight() * (0.2f + 0.8f *
            ((i + 1) * peakGuess * (1.0f + std::sin((float)i * 0.7f) * 0.3f)));
        h = juce::jlimit(2.0f, waveformArea.getHeight(), h);
        float y = waveformArea.getBottom() - h;
        g.setColour(ref.colour.withAlpha(0.45f + 0.4f * (h / waveformArea.getHeight())));
        g.fillRoundedRectangle(x, y, barW, h, 1.0f);
    }

    // ─── File name ──────────────────────────────────────────────────────
    auto nameArea = row.removeFromLeft(juce::jmin(100.0f, row.getWidth() * 0.4f));
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    g.setColour(MixCoachTheme::textPrimary());
    auto displayName = ref.name;
    if (displayName.length() > 18) displayName = displayName.substring(0, 16) + "...";
    g.drawText(displayName, nameArea.reduced(4, 0), juce::Justification::centredLeft);

    // ─── Audio info: sample rate · bit depth ────────────────────────────
    auto infoArea = row.removeFromLeft(70);
    g.setFont(juce::Font(juce::FontOptions(6.5f)));
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.6f));
    if (ref.audioInfo.valid) {
        juce::String rateStr = juce::String(ref.audioInfo.sampleRate / 1000) + " kHz";
        juce::String bitsStr = juce::String(ref.audioInfo.bitDepth) + " bit";
        g.drawText(rateStr + " " + juce::String("\xC2\xB7") + " " + bitsStr,
                   infoArea.reduced(4, 0), juce::Justification::centredLeft);
    } else {
        g.drawText(juce::String("-- kHz \xC2\xB7 -- bit"),
                   infoArea.reduced(4, 0), juce::Justification::centredLeft);
    }

    // ─── Duration ───────────────────────────────────────────────────────
    auto durArea = row.removeFromLeft(40);
    g.setFont(juce::Font(juce::FontOptions(6.5f)));
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
    if (ref.audioInfo.valid && ref.audioInfo.durationSeconds > 0) {
        int totalSec = (int)ref.audioInfo.durationSeconds;
        int mins = totalSec / 60;
        int secs = totalSec % 60;
        g.drawText(juce::String(mins).paddedLeft('0', 2) + ":" +
                   juce::String(secs).paddedLeft('0', 2),
                   durArea.reduced(2, 0), juce::Justification::centredLeft);
    } else {
        g.drawText("--:--", durArea.reduced(2, 0), juce::Justification::centredLeft);
    }

    // ─── Delete ✕ button ────────────────────────────────────────────────
    auto delArea = row.removeFromRight(18).reduced(3, 5);
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.35f));
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.drawText(juce::CharPointer_UTF8("\xE2\x9C\x95"), delArea, juce::Justification::centred);

    // ─── More options ⋯ button ─────────────────────────────────────────
    auto moreArea = row.removeFromRight(16).reduced(2, 5);
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.25f));
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    g.drawText(juce::CharPointer_UTF8("\xE2\x8B\xAF"), moreArea, juce::Justification::centred);
}

// ─── Draw link reference row ──────────────────────────────────────────────────

void ReferencePanelComponent::drawLinkRow(juce::Graphics& g, juce::Rectangle<int> bounds,
                                          const MixReference& ref, int index)
{
    juce::ignoreUnused(index);
    auto row = bounds.toFloat();

    // ─── Row background ─────────────────────────────────────────────────
    g.setColour(MixCoachTheme::bgDarker().withAlpha(0.35f));
    g.fillRoundedRectangle(row, 4.0f);

    // ─── Link icon ──────────────────────────────────────────────────────
    auto iconArea = row.removeFromLeft(20);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.setColour(ref.colour);
    g.drawText(juce::CharPointer_UTF8("\xF0\x9F\x94\x97"), iconArea, juce::Justification::centred);

    // ─── Link name ──────────────────────────────────────────────────────
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    g.setColour(MixCoachTheme::textPrimary());
    auto displayName = ref.name;
    if (displayName.length() > 30) displayName = displayName.substring(0, 28) + "...";
    g.drawText(displayName, row.reduced(4, 0), juce::Justification::centredLeft);
}

// ─── Mouse down — handle clicks on interactive elements ───────────────────────

void ReferencePanelComponent::mouseDown(const juce::MouseEvent& e)
{
    auto pos = e.getPosition();

    // ─── Check tab clicks ───────────────────────────────────────────────
    if (audioTabBounds_.contains(pos)) {
        switchTab(ActiveTab::AudioRefs);
        return;
    }
    if (linksTabBounds_.contains(pos)) {
        switchTab(ActiveTab::UsefulLinks);
        return;
    }

    // ─── Check row element clicks (play, delete, more) ─────────────────
    int drawn = 0;
    for (int i = (int)references_.size() - 1; i >= 0 && drawn < (int)visibleRowBounds_.size(); ++i, ++drawn) {
        auto& rowBounds = visibleRowBounds_[drawn];
        if (!rowBounds.contains(pos))
            continue;

        auto& ref = references_[i];

        if (ref.type == MixReference::Type::File) {
            // Play button hit test (left 20px of row)
            juce::Rectangle<int> playArea(rowBounds.getX(), rowBounds.getY(), 20, rowBounds.getHeight());
            if (playArea.contains(pos)) {
                // TODO: Play audio reference
                return;
            }

            // Delete button hit test (right 18px of row)
            juce::Rectangle<int> delArea(rowBounds.getRight() - 18, rowBounds.getY(), 18, rowBounds.getHeight());
            if (delArea.contains(pos)) {
                removeReference(i);
                return;
            }
        }

        // For URL type, clicking the row opens the link
        if (ref.type == MixReference::Type::URL) {
            juce::URL(ref.path).launchInDefaultBrowser();
            return;
        }
    }
}

// ─── TextEditor return key — adds URL on enter ────────────────────────────────

void ReferencePanelComponent::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    if (&editor == &urlInput_) {
        auto url = urlInput_.getText().trim();
        if (url.isNotEmpty()) {
            addURLReference(url);
            urlInput_.clear();
        }
    }
}

} // namespace mixcoach
