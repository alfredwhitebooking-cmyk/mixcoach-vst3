#include "ReferencePanelComponent.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  DropZoneComponent
// ═══════════════════════════════════════════════════════════════════════════

void DropZoneComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    // Fondo
    if (isDragging_) {
        g.setColour(MixCoachTheme::accent().withAlpha(0.1f));
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(MixCoachTheme::accent().withAlpha(0.6f));
    } else if (isHovering_) {
        g.setColour(MixCoachTheme::accent().withAlpha(0.05f));
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(MixCoachTheme::accentDim().withAlpha(0.4f));
    } else {
        g.setColour(MixCoachTheme::bgDarker().withAlpha(0.5f));
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(MixCoachTheme::border().withAlpha(0.5f));
    }

    // Borde dashed cuando se arrastra
    if (isDragging_) {
        auto dashLength = 6.0f;
        auto gapLength = 4.0f;
        float dashPos = 0.0f;
        juce::Path dashPath;
        dashPath.startNewSubPath(bounds.getX(), bounds.getY());
        // Top edge
        while (dashPos < bounds.getWidth()) {
            float endX = juce::jmin(bounds.getX() + dashPos + dashLength, bounds.getRight());
            dashPath.lineTo(endX, bounds.getY());
            dashPos += dashLength + gapLength;
            if (dashPos < bounds.getWidth()) {
                dashPath.startNewSubPath(bounds.getX() + dashPos, bounds.getY());
            }
        }
        // Right edge
        // (simplified: just draw a dashed rectangle outline)
        g.drawRoundedRectangle(bounds, 6.0f, 2.0f);
    } else {
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
    }

    // Icono y texto
    auto textBounds = bounds.reduced(8);
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)));

    if (isDragging_) {
        g.setColour(MixCoachTheme::accentGlow());
        g.drawText(juce::CharPointer_UTF8("\xF0\x9F\x93\x82 Suelta aqui para agregar referencia"),
                   textBounds, juce::Justification::centred);
    } else {
        g.setColour(MixCoachTheme::textDim());
        g.drawText(juce::CharPointer_UTF8("\xF0\x9F\x93\x82 Arrastra archivos de audio aqui"),
                   textBounds.removeFromTop(24), juce::Justification::centred);
        g.setColour(MixCoachTheme::textMuted());
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
        g.drawText("WAV, MP3, FLAC, AIFF — o pega una URL abajo",
                   textBounds, juce::Justification::centred);
    }
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
        if (ext == ".wav" || ext == ".mp3" || ext == ".flac" || ext == ".aiff" || ext == ".aif" || ext == ".ogg") {
            if (onFileDropped)
                onFileDropped(file.getFullPathName());
        }
    }
    repaint();
}

// ═══════════════════════════════════════════════════════════════════════════
//  ReferencePanelComponent
// ═══════════════════════════════════════════════════════════════════════════

ReferencePanelComponent::ReferencePanelComponent()
{
    titleLabel_.setText(juce::String(juce::CharPointer_UTF8("\xF0\x9F\x93\x9A Referencias de Mezcla")), juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeHeader)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(titleLabel_);

    // Drop zone
    dropZone_.onFileDropped = [this](const juce::String& path) {
        addFileReference(path);
    };
    addAndMakeVisible(dropZone_);

    // URL input
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

    // Add URL button
    addUrlButton_.setButtonText(juce::String(juce::CharPointer_UTF8("\xE2\x9E\x95")));
    addUrlButton_.setColour(juce::TextButton::buttonColourId, MixCoachTheme::accent().withAlpha(0.3f));
    // Text colour uses default from LookAndFeel
    addUrlButton_.onClick = [this]() {
        auto url = urlInput_.getText().trim();
        if (url.isNotEmpty()) {
            addURLReference(url);
            urlInput_.clear();
        }
    };
    addAndMakeVisible(addUrlButton_);

    // Contador
    referenceCountLabel_.setText("0 referencias", juce::dontSendNotification);
    referenceCountLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
    referenceCountLabel_.setJustificationType(juce::Justification::centredRight);
    referenceCountLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
    addAndMakeVisible(referenceCountLabel_);

    // Empty state
    emptyLabel_.setText(juce::String(juce::CharPointer_UTF8("\xF0\x9F\x94\x8D Carga referencias para comparar tu mezcla")), juce::dontSendNotification);
    emptyLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
    emptyLabel_.setJustificationType(juce::Justification::centred);
    emptyLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
    addAndMakeVisible(emptyLabel_);
}

void ReferencePanelComponent::addFileReference(const juce::String& filePath)
{
    auto file = juce::File(filePath);
    MixReference ref;
    ref.type = MixReference::Type::File;
    ref.name = file.getFileName();
    ref.path = filePath;
    ref.colour = MixCoachTheme::accent().withAlpha(0.7f);
    ref.addedTime = juce::Time::getMillisecondCounter();
    references_.push_back(ref);
    refreshDisplay();
    if (onReferencesChanged) onReferencesChanged();
}

void ReferencePanelComponent::addURLReference(const juce::String& url)
{
    MixReference ref;
    ref.type = MixReference::Type::URL;
    // Extract a readable name from URL
    auto name = url;
    if (url.contains("youtube.com") || url.contains("youtu.be")) {
        name = "\xF0\x9F\x8E\xAC YouTube Reference";
    } else if (url.contains("spotify.com")) {
        name = "\xF0\x9F\x8E\xB5 Spotify Reference";
    } else {
        name = "\xF0\x9F\x94\x97 " + url.substring(0, 40) + "...";
    }
    ref.name = name;
    ref.path = url;
    ref.colour = MixCoachTheme::accent2().withAlpha(0.7f);
    ref.addedTime = juce::Time::getMillisecondCounter();
    references_.push_back(ref);
    refreshDisplay();
    if (onReferencesChanged) onReferencesChanged();
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

void ReferencePanelComponent::refreshDisplay()
{
    referenceCountLabel_.setText(
        juce::String(references_.size()) + " referencia" + (references_.size() != 1 ? "s" : ""),
        juce::dontSendNotification);
    emptyLabel_.setVisible(references_.empty());
    repaint();
    resized();
}

void ReferencePanelComponent::resized()
{
    auto area = getLocalBounds().reduced(4);

    auto headerArea = area.removeFromTop(20);
    titleLabel_.setBounds(headerArea.removeFromLeft(200));
    referenceCountLabel_.setBounds(headerArea);

    // Drop zone
    auto dropArea = area.removeFromTop(56);
    dropZone_.setBounds(dropArea.reduced(2));

    // URL row
    auto urlArea = area.removeFromTop(28);
    addUrlButton_.setBounds(urlArea.removeFromRight(28));
    urlInput_.setBounds(urlArea.reduced(0, 2));

    area.removeFromTop(2);

    // Empty state
    emptyLabel_.setBounds(area);
}

void ReferencePanelComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Fondo
    MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), 6.0f);

    if (references_.empty()) return;

    // Dibujar referencias después de la URL row
    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(20); // header
    area.removeFromTop(56 + 28 + 4); // dropzone + url + gap

    int maxVisible = 3;
    int drawn = 0;
    for (int i = (int)references_.size() - 1; i >= 0 && drawn < maxVisible; --i) {
        auto rowArea = area.removeFromTop(22).reduced(2, 1);
        drawReferenceRow(g, rowArea, references_[i], i);
        drawn++;
    }

    if ((int)references_.size() > maxVisible) {
        g.setColour(MixCoachTheme::textMuted());
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
        g.drawText("+ " + juce::String(references_.size() - maxVisible) + " mas...",
                   area.removeFromTop(16).reduced(4, 0),
                   juce::Justification::centredLeft);
    }
}

void ReferencePanelComponent::drawReferenceRow(juce::Graphics& g, juce::Rectangle<int> bounds,
                                                 const MixReference& ref, int index)
{
    juce::ignoreUnused(index);

    // Row background
    g.setColour(MixCoachTheme::bgDarker().withAlpha(0.4f));
    g.fillRoundedRectangle(bounds.toFloat(), 3.0f);

    // Type icon
    auto iconArea = bounds.removeFromLeft(20);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.setColour(ref.colour);
    if (ref.type == MixReference::Type::File)
        g.drawText(juce::CharPointer_UTF8("\xF0\x9F\x93\x81"), iconArea, juce::Justification::centred);
    else
        g.drawText(juce::CharPointer_UTF8("\xF0\x9F\x94\x97"), iconArea, juce::Justification::centred);

    // Name
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    g.setColour(MixCoachTheme::textPrimary());
    auto name = ref.name;
    if (name.length() > 28) name = name.substring(0, 26) + "...";
    g.drawText(name, bounds, juce::Justification::centredLeft);
}

} // namespace mixcoach
