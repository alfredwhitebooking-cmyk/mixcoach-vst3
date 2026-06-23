#pragma once
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  MeterCard — Tarjeta métrica individual (PEAK, RMS, LUFS, DR)
    //  Fondo oscuro con borde glow, número grande, unidad pequeña
    // ═══════════════════════════════════════════════════════════════════════════
    class MeterCard : public juce::Component
    {
    public:
        MeterCard(const juce::String& label,
                  const juce::String& unit,
                  juce::Colour accentColour,
                  const juce::String& formatStr = "{:.1f}") :
            label_(label),
            unit_(unit),
            accent_(accentColour),
            format_(formatStr)
        {}

        void setValue(float v)
        {
            value_ = v;
            repaint();
        }

        void paint(juce::Graphics& g) override;

        void resized() override {}

    private:
        juce::String label_, unit_, format_;
        juce::Colour accent_;
        float value_ = -80.0f;
    };

} // namespace mixcoach
