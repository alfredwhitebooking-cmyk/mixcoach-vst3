#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "MixCoachTheme.h"

namespace mixcoach {

// ─── Referencia de mezcla (archivo o enlace) ───────────────────────────────
struct MixReference {
    enum class Type { File, URL };
    Type        type;
    juce::String name;
    juce::String path;       // Ruta de archivo o URL
    juce::Colour colour;
    int64_t     addedTime;
};

// ─── Drop Zone interna para arrastrar archivos ─────────────────────────────
class DropZoneComponent : public juce::Component,
                          public juce::FileDragAndDropTarget
{
public:
    std::function<void(const juce::String&)> onFileDropped;

    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    bool isHovering_ = false;
    bool isDragging_ = false;
};

// ─── Panel de Referencias de Mezcla ────────────────────────────────────────
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

    [[nodiscard]] int getNumReferences() const { return references_.size(); }
    [[nodiscard]] const MixReference& getReference(int index) const { return references_[index]; }

    std::function<void()> onReferencesChanged;

private:
    juce::Label      titleLabel_;
    DropZoneComponent dropZone_;
    juce::TextEditor  urlInput_;
    juce::TextButton  addUrlButton_;
    juce::Label       referenceCountLabel_;
    juce::Label       emptyLabel_;

    std::vector<MixReference> references_;

    void refreshDisplay();
    void drawReferenceRow(juce::Graphics& g, juce::Rectangle<int> bounds, const MixReference& ref, int index);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReferencePanelComponent)
};

} // namespace mixcoach
