#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"

namespace mixcoach {

// ─── Divider bar con pintado personalizado ──────────────────────────────────
class DividerBar : public juce::Component {
public:
    void paint(juce::Graphics& g) override {
        g.fillAll(MixCoachTheme::border());
    }
};

} // namespace mixcoach
