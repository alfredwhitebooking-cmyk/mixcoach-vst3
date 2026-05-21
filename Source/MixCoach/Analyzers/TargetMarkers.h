#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace mixcoach {

// ─── Target Markers (referencias visuales) ──────────────────────────────────
class TargetMarkers : public juce::Component
{
public:
    TargetMarkers();
    ~TargetMarkers() override = default;

    void paint(juce::Graphics& g) override;
    void setTargets(const juce::StringArray& labels, const std::vector<float>& values);

private:
    juce::StringArray labels_;
    std::vector<float> values_;
    juce::Label titleLabel_;
};

} // namespace mixcoach
