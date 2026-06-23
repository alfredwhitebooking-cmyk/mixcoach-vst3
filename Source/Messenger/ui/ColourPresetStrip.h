#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  ColourPresetStrip — 8 colores predefinidos para asignación rápida
    // ═══════════════════════════════════════════════════════════════════════════
    class ColourPresetStrip : public juce::Component
    {
    public:
        std::function<void(juce::Colour)> onColourChosen;

        ColourPresetStrip();

        void setActiveColour(juce::Colour col);

        void mouseDown(const juce::MouseEvent& e) override;
        void paint(juce::Graphics& g) override;

    private:
        static const juce::Colour presetColours_[8];
        juce::Colour activeColour_{0xFF808080};

        int getNumPresets() const { return 8; }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ColourPresetStrip)
    };

} // namespace mixcoach
