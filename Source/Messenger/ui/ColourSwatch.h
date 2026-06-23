#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  ColourSwatch — Muestra de color clickeable que abre selector
    // ═══════════════════════════════════════════════════════════════════════════
    class ColourSwatch : public juce::Component
    {
    public:
        std::function<void()> onClick;

        ColourSwatch();

        void setColour(juce::Colour col);

        [[nodiscard]] juce::Colour getColour() const { return colour_; }

        void mouseDown(const juce::MouseEvent&) override;
        void mouseEnter(const juce::MouseEvent&) override;
        void mouseExit(const juce::MouseEvent&) override;

        void paint(juce::Graphics& g) override;

    private:
        juce::Colour colour_{0xFF808080};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ColourSwatch)
    };

} // namespace mixcoach
