#pragma once
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  RefToggle — Mini toggle button para la curva de referencia
    //  Mismo estilo que el reference button dentro de SpectrographComponent
    // ═══════════════════════════════════════════════════════════════════════════
    class RefToggle : public juce::Component
    {
    public:
        RefToggle();
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseEnter(const juce::MouseEvent&) override;
        void mouseExit(const juce::MouseEvent&) override;
        void mouseMove(const juce::MouseEvent&) override;
        std::function<void()> onClick;
        bool toggled = false;
        bool hovered = false;
    };

} // namespace mixcoach
