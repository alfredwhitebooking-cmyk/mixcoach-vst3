#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "SmoothValue.h"
#include "MixCoachTheme.h"
#include "../audio/AudioAnalyzer.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  MasterMeterPanel — Master LUFS Meter (Youlean / iZotope Insight 2 style)
    //
    //  📊 Layout:
    //    ┌──────────────────────────────────────────┐
    //    │           MASTER LOUDNESS                │  ← Title
    //    ├──────────┬──────────────┬────────────────┤
    //    │Short-Term│  Integrated  │   Momentary    │  ← 3-column metrics
    //    │  -15.0   │    -19.0     │     -8.5       │  ← Big numbers (50-70px)
    //    │   LUFS   │     LUFS     │     LUFS       │  ← Units
    //    ├──────────┴──────────────┴────────────────┤
    //    │ LRA 11 LU              TP -1.5 dBTP     │  ← Secondary metrics
    //    ├──────────────────────────────────────────┤  ← 1px separator
    //    │ [Streaming ▼]  Peak:-1.0  Loud:-14  [Gate]│  ← Control bar
    //    └──────────────────────────────────────────┘
    //
    //  Data source: AudioAnalyzer (getMomentaryLUFS, getShortTermLUFS, etc.)
    // ═══════════════════════════════════════════════════════════════════════════
    class MasterMeterPanel : public juce::Component
    {
    public:
        MasterMeterPanel();
        ~MasterMeterPanel() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;

        /** Lee todos los valores del AudioAnalyzer maestro y actualiza targets. */
        void updateMeters(const AudioAnalyzer& analyzer);

        /** Suaviza todos los valores animados (llamar a 60fps). */
        void advanceVisuals(double sampleRateHz = 60.0);

        // ─── Target management ─────────────────────────────────────────────────
        void setLoudnessTarget(float lufs) { loudnessTarget_ = lufs; }

        void setPeakTarget(float dbtp) { peakTarget_ = dbtp; }

        [[nodiscard]] float getLoudnessTarget() const noexcept { return loudnessTarget_; }

        [[nodiscard]] float getPeakTarget() const noexcept { return peakTarget_; }

    private:
        // ═══ Smoothed values ══════════════════════════════════════════════════
        SmoothValue momentaryLUFS_{-40.0f, 3.0f, 200.0f};
        SmoothValue shortTermLUFS_{-40.0f, 3.0f, 200.0f};
        SmoothValue integratedLUFS_{-40.0f, 5.0f, 250.0f};
        SmoothValue truePeakDBTP_{-40.0f, 1.0f, 100.0f};
        SmoothValue loudnessRange_{0.0f, 8.0f, 300.0f};

        // ─── Raw cached values (for text display without smoothing lag) ─────
        float rawMomentary_  = -40.0f;
        float rawShortTerm_  = -40.0f;
        float rawIntegrated_ = -40.0f;
        float rawTruePeak_   = -40.0f;
        float rawLRA_        = 0.0f;

        // ─── Target presets ────────────────────────────────────────────────────
        float loudnessTarget_ = -14.0f; // Default: -14 LUFS
        float peakTarget_     = -1.0f;  // Default: -1 dBTP

        // ─── Gate ──────────────────────────────────────────────────────────────
        bool gateEnabled_    = false;
        float gateThreshold_ = -40.0f;

        // ─── Preset system ─────────────────────────────────────────────────────
        enum Preset : uint8_t
        {
            Streaming,
            Spotify,
            YouTube,
            AppleMusic,
            Broadcast,
            Custom
        };

        Preset activePreset_ = Spotify;

        struct PresetInfo
        {
            const char* name;
            float targetLUFS;
            float targetPeak;
        };

        static constexpr PresetInfo kPresets_[6] = {
            {"Streaming", -14.0f, -1.0f},
            {"Spotify", -14.0f, -1.0f},
            {"YouTube", -14.0f, -1.0f},
            {"Apple Music", -16.0f, -1.0f},
            {"Broadcast", -23.0f, -2.0f},
            {"Custom", -14.0f, -1.0f},
        };

        void applyPreset(Preset p);

        // ═══ Drawing helpers ══════════════════════════════════════════════════
        void drawBackground(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawTitle(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawMetricsRow(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawBigValue(juce::Graphics& g,
                          juce::Rectangle<float> cell,
                          const juce::String& label,
                          float value,
                          const juce::String& unit,
                          juce::Colour colour,
                          bool dominant = false);
        void drawSecondaryRow(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawControlBar(juce::Graphics& g, juce::Rectangle<int> bounds);
        void showPresetPopup();

        // ─── Hit areas for interaction ─────────────────────────────────────────
        juce::Rectangle<int> presetHitArea_;
        juce::Rectangle<int> peakTargetArea_;
        juce::Rectangle<int> lufsTargetArea_;
        juce::Rectangle<int> gateHitArea_;
        bool presetHovered_ = false;

        void mouseDown(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;

        // ─── Stats ─────────────────────────────────────────────────────────────
        float getTargetDelta(float current, float target) const noexcept { return current - target; }

        juce::Colour getLUFScolour(float value, float target) const noexcept
        {
            float delta = getTargetDelta(value, target);
            if (delta > 1.0f) return MixCoachTheme::error();  // Red/pink (too loud)
            if (delta > -1.0f) return MixCoachTheme::warning(); // Yellow (near target)
            if (delta > -4.0f) return MixCoachTheme::success(); // Green (in range)
            return MixCoachTheme::textMuted();                    // Gray (too quiet)
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterMeterPanel)
    };

} // namespace mixcoach
