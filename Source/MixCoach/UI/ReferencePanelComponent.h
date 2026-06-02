#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "MixCoachTheme.h"

namespace mixcoach {

// ─── Información de archivo de audio extraída ──────────────────────────────
struct AudioFileInfo {
    double  durationSeconds = 0.0;
    int     sampleRate      = 0;
    int     bitDepth        = 0;
    bool    valid           = false;
};

// ─── Referencia de mezcla (archivo o enlace) ───────────────────────────────
struct MixReference {
    enum class Type { File, URL };
    Type        type;
    juce::String name;
    juce::String path;       // Ruta de archivo o URL
    juce::Colour colour;
    int64_t     addedTime;
    AudioFileInfo audioInfo; // Metadata del archivo (solo File)
};

// ═══════════════════════════════════════════════════════════════════════════
//  DropZoneComponent — Zona de arrastre con estilo visual rediseñado
//  Icono nube-upload, texto, botón EXPLORAR ARCHIVOS, formatos soportados
// ═══════════════════════════════════════════════════════════════════════════
class DropZoneComponent : public juce::Component,
                          public juce::FileDragAndDropTarget
{
public:
    DropZoneComponent();
    ~DropZoneComponent() override;

    std::function<void(const juce::String&)> onFileDropped;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void setShowDropHint(bool showHint);

private:
    void browseForFiles();

    juce::TextButton browseButton_;
    bool isHovering_ = false;
    bool isDragging_ = false;
    bool showHint_   = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DropZoneComponent)
};

// ═══════════════════════════════════════════════════════════════════════════
//  ReferencePanelComponent — Panel de referencias con sub-tabs
//  Tab 1: REFERENCIAS DE AUDIO — drop zone + file list
//  Tab 2: ENLACES ÚTILES — URL input + links list
// ═══════════════════════════════════════════════════════════════════════════
class ReferencePanelComponent : public juce::Component,
                               public juce::TextEditor::Listener
{
public:
    ReferencePanelComponent();
    ~ReferencePanelComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

    void addFileReference(const juce::String& filePath);
    void addURLReference(const juce::String& url);
    void removeReference(int index);
    void clearReferences();

    [[nodiscard]] int getNumReferences() const { return static_cast<int>(references_.size()); }
    [[nodiscard]] const MixReference& getReference(int index) const { return references_[index]; }

    std::function<void()> onReferencesChanged;

private:
    // TextEditor::Listener
    void textEditorTextChanged(juce::TextEditor&) override {}
    void textEditorReturnKeyPressed(juce::TextEditor&) override;

    // ─── Sub-tabs ──────────────────────────────────────────────────────────
    enum class ActiveTab { AudioRefs, UsefulLinks };
    ActiveTab activeTab_ = ActiveTab::AudioRefs;

    juce::Rectangle<int> audioTabBounds_;
    juce::Rectangle<int> linksTabBounds_;

    void switchTab(ActiveTab tab);

    // ─── Components ────────────────────────────────────────────────────────
    DropZoneComponent dropZone_;
    juce::TextEditor  urlInput_;
    juce::TextButton  addUrlButton_;
    juce::Label       referenceCountLabel_;
    juce::Label       emptyLabel_;

    std::vector<MixReference> references_;

    // ─── Audio metadata helper ─────────────────────────────────────────────
    static AudioFileInfo readAudioFileInfo(const juce::File& file);

    // ─── Display ──────────────────────────────────────────────────────────
    void refreshDisplay();
    void drawReferenceRow(juce::Graphics& g, juce::Rectangle<int> bounds,
                          const MixReference& ref, int index);
    void drawAudioRow(juce::Graphics& g, juce::Rectangle<int> bounds,
                      const MixReference& ref, int index);
    void drawLinkRow(juce::Graphics& g, juce::Rectangle<int> bounds,
                     const MixReference& ref, int index);

    // Click handling for interactive elements within rows
    void mouseDown(const juce::MouseEvent& e) override;

    // Store row bounds for hit testing
    std::vector<juce::Rectangle<int>> visibleRowBounds_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReferencePanelComponent)
};

} // namespace mixcoach
