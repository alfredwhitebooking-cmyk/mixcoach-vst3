#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  SpaceTrackRow — Datos de una pista para sugerencias de espacio/reverb
// ═══════════════════════════════════════════════════════════════════════════
struct SpaceTrackRow
{
    int slotIndex = -1;
    juce::String trackName;
    juce::String roleName;
    float correlation = 0.95f;
    float stereoWidth = 0.5f;
    float suggestedPreDelayMs = 40.0f;
    float suggestedDecaySec = 1.8f;
    float suggestedMixPct = 18.0f;
    juce::String reverbType; // "Room", "Hall", "Plate", "Spring"
    bool isApplied = false;
};

// ═══════════════════════════════════════════════════════════════════════════
//  SpacePanel — Panel de espacialización con sugerencias de reverb/delay
//
//  Aparece automáticamente al entrar en CoachRoomState::Space.
//  Muestra correlación y ancho estéreo actual vs sugerencias de reverb.
//  Ofrece botones para aplicar configuración de envío a reverb.
// ═══════════════════════════════════════════════════════════════════════════
class SpacePanel : public juce::Component
{
public:
    SpacePanel();
    ~SpacePanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    void setTrackData(const std::vector<SpaceTrackRow>& rows);
    void clear();

    bool hasData() const noexcept { return !rows_.empty(); }

    /** Callback cuando el usuario aplica configuración de reverb a una pista. */
    std::function<void(int slotIndex, float preDelayMs,
                       float decaySec, float mixPct, const juce::String& reverbType)> onApplyReverb;

    std::function<void()> onApplyAll;

private:
    std::vector<SpaceTrackRow> rows_;

    static constexpr int kHeaderHeight = 22;
    static constexpr int kRowHeight = 36;
    static constexpr int kGap = 2;
    static constexpr int kPadding = 8;
    static constexpr int kNameWidth = 85;
    static constexpr int kCorrBarW = 60;
    static constexpr int kWidthBarW = 50;
    static constexpr int kPreDelayW = 40;
    static constexpr int kDecayW = 40;
    static constexpr int kMixW = 35;
    static constexpr int kTypeW = 50;
    static constexpr int kApplyW = 50;

    std::vector<juce::Rectangle<int>> applyBtnBounds_;
    juce::Rectangle<int> applyAllBounds_;
    int hoveredRow_ = -1;
    int hoveredApplyAll_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpacePanel)
};

} // namespace mixcoach
