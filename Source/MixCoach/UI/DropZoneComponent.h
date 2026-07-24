#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "MixCoachTheme.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  DropZoneComponent — Zona de arrastre + URL input unificados
    //  Acepta archivos (drag & drop) y URLs (input + tecla Enter)
    // ═══════════════════════════════════════════════════════════════════════════
    class DropZoneComponent :
        public juce::Component,
        public juce::FileDragAndDropTarget,
        public juce::TextEditor::Listener
    {
    public:
        DropZoneComponent();
        ~DropZoneComponent() override;

        std::function<void(const juce::String&)> onFileDropped;
        std::function<void(const juce::String&)> onURLAdded;

        void paint(juce::Graphics& g) override;
        void resized() override;

    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent& e) override;

    // FileDragAndDropTarget
        bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }

        void fileDragEnter(const juce::StringArray&, int, int) override;
        void fileDragExit(const juce::StringArray&) override;
        void filesDropped(const juce::StringArray& files, int x, int y) override;

        void setShowDropHint(bool showHint);

        // TextEditor::Listener
        void textEditorReturnKeyPressed(juce::TextEditor&) override;

    private:
        void browseForFiles();

        juce::TextEditor urlInput_;
        juce::TextButton browseButton_;
        juce::TextButton addUrlButton_;
        bool isHovering_ = false;
        bool isDragging_ = false;
        bool showHint_   = true;

        // Section bounds for paint (Audio left / Link right)
        juce::Rectangle<int> audioSectionBounds_;
        juce::Rectangle<int> linkSectionBounds_;

        // FileChooser con unique_ptr para evitar memory leak
        // Antes: new FileChooser sin delete si el usuario cancela el diálogo.
        // Ahora: unique_ptr se limpia automáticamente al destruir el componente
        // o al asignar un nuevo FileChooser.
        std::unique_ptr<juce::FileChooser> fileChooser_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DropZoneComponent)
    };

} // namespace mixcoach
