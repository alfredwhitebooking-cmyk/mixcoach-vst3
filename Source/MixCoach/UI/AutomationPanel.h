#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "MixCoachTheme.h"
#include <vector>
#include <functional>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  AutomationSection — Una sección de la canción con su metadata LUFS
// ═══════════════════════════════════════════════════════════════════════════
struct AutomationSection
{
    juce::String name;       // "Verse", "Chorus", "Bridge", etc.
    float startBeat = 0.0f;  // Position in beats
    float durationBeats = 0.0f;
    float integratedLUFS = -18.0f;
    float shortTermLUFS = -16.0f;
    float momentaryLUFS = -14.0f;
    float crestDb = 8.0f;    // Crest factor in dB
    float correlation = 0.5f;
    bool isActive = false;
};

// ═══════════════════════════════════════════════════════════════════════════
//  AutomationTimelineData — Datos completos para el timeline
// ═══════════════════════════════════════════════════════════════════════════
struct AutomationTimelineData
{
    float overallIntegratedLUFS = -16.0f;
    float overallShortTermLUFS = -14.0f;
    float overallMomentaryLUFS = -12.0f;
    float dynamicRange = 12.0f;     // dB entre sección más quieta y más loud
    float crestFactor = 8.0f;       // dB
    float targetIntegratedLUFS = -14.0f; // LUFS target (e.g., -14 LUFS for streaming)
    
    std::vector<AutomationSection> sections;
    
    bool hasData() const noexcept { return !sections.empty(); }
    int getNumSections() const noexcept { return (int)sections.size(); }
};

// ═══════════════════════════════════════════════════════════════════════════
//  AutomationPanel — Panel de automatización con LUFS timeline
//
//  Diseño premium con glass effect, timeline interactivo, y
//  cards de sección con métricas detalladas.
// ═══════════════════════════════════════════════════════════════════════════
class AutomationPanel : public juce::Component,
                        private juce::Timer
{
public:
    AutomationPanel();
    ~AutomationPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    /** Pausa/restaura el timer segun la visibilidad del componente. */
    void visibilityChanged() override;

    /** Actualiza todos los datos del timeline. */
    void setTimelineData(const AutomationTimelineData& data);
    
    /** Actualiza solo los valores LUFS en tiempo real (sin cambiar secciones). */
    void updateLiveLUFS(float momentaryLUFS, float shortTermLUFS, float integratedLUFS);

    void clear();
    bool hasData() const noexcept { return data_.hasData(); }

    std::function<void(int sectionIndex)> onSectionClicked;
    std::function<void()> onRequestReanalyze;

private:
    // ═══ Constants ═══════════════════════════════════════════════════════
    static constexpr float kTimelineHeight = 80.0f;
    static constexpr float kSectionCardH = 52.0f;
    static constexpr float kPadding = 8.0f;
    static constexpr float kMinLUFS = -30.0f;
    static constexpr float kMaxLUFS = -6.0f;
    static constexpr int kAnimFrames = 30;
    static constexpr float kLUFSRange = kMaxLUFS - kMinLUFS;

    // ═══ Data ═══════════════════════════════════════════════════════════
    AutomationTimelineData data_;
    float animProgress_ = 0.0f;
    int hoveredSection_ = -1;
    int64_t lastUpdateMs_ = 0;

    // ═══ Live LUFS values for pulsing meters ═══════════════════════════
    float liveMomentaryLUFS_ = -20.0f;
    float liveShortTermLUFS_ = -18.0f;
    float liveIntegratedLUFS_ = -16.0f;

    // ═══ Drawing helpers ═══════════════════════════════════════════════
    void drawTimeline(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawSectionCard(juce::Graphics& g, juce::Rectangle<float> bounds, 
                         const AutomationSection& section, int index);
    void drawLUFSMeter(juce::Graphics& g, juce::Rectangle<float> bounds,
                       float currentLUFS, float targetLUFS,
                       const char* label, juce::Colour colour);
    void drawStatsRow(juce::Graphics& g, juce::Rectangle<float> bounds);

    /** Convierte LUFS a posición Y en el timeline (0=top, kMaxLUFS, kTimelineHeight=bottom, kMinLUFS). */
    float lufsToY(float lufs, float top, float height) const noexcept
    {
        float norm = (lufs - kMinLUFS) / kLUFSRange;
        return top + height * (1.0f - juce::jlimit(0.0f, 1.0f, norm));
    }

    // Easing
    static float easeOutCubic(float t) noexcept
    {
        return 1.0f - std::pow(1.0f - t, 3.0f);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutomationPanel)
};

} // namespace mixcoach
