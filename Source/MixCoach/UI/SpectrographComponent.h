#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>
#include "SmoothValue.h"
#include "MixCoachTheme.h"

namespace mixcoach {

// SESIÓN 3 – RTA (barras log 20 Hz–20 kHz, estilo UI_REFERENCES Tab2)
class SpectrographComponent : public juce::Component
{
public:
    SpectrographComponent();
    ~SpectrographComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setSampleRate(double sampleRate);
    void updateSpectrum(const float* data, int numBins);
    /** Avanza suavizado de barras RTA a sampleRateHz (p. ej. 60). */
    bool smoothSpectrum(double sampleRateHz = 60.0, bool allowRepaint = true);

private:
    static constexpr int   kNumRtaBands   = 40;
    static constexpr int   kMaxFFTBins    = 512;
    static constexpr int   kMessengerFftSize = 1024;
    static constexpr float kMinFreq       = 20.0f;
    static constexpr float kMaxFreq       = 20000.0f;
    static constexpr float kDisplayTopDb  = 0.0f;
    static constexpr float kDisplayBottomDb = -45.0f;

    struct RtaBand {
        float centerHz = 0.0f;
        float lowHz    = 0.0f;
        float highHz   = 0.0f;
    };

    /** Geometría del gráfico, en coordenadas locales de staticCache_ (origen plotArea_). */
    struct PlotLayout {
        juce::Rectangle<float> plot;
        juce::Rectangle<float> dbCol;
        bool valid = false;
    };

    double sampleRate_ = 48000.0;
    std::vector<RtaBand> bands_;
    std::vector<SmoothValue> bandLevels_;

    juce::Rectangle<int> plotArea_;
    juce::Rectangle<int> settingsButton_;

    juce::Image staticCache_;
    bool staticCacheValid_ = false;
    PlotLayout layout_;

    void rebuildBands();
    void invalidateStaticCache();
    void rebuildStaticCache();

    float freqToX(float freqHz, juce::Rectangle<float> plot) const noexcept;
    float normToDb(float norm01) const noexcept;
    float dbToDisplayNorm(float db) const noexcept;
    void aggregateBand(const float* data, int numBins, int bandIndex, float& outDb) const;

    PlotLayout computePlotLayout() const;

    void drawPlotBackground(juce::Graphics& g, juce::Rectangle<float> plot) const;
    void drawGrid(juce::Graphics& g, juce::Rectangle<float> plot) const;
    void drawRtaBars(juce::Graphics& g, juce::Rectangle<float> plot) const;
    void drawDbAxis(juce::Graphics& g, juce::Rectangle<float> labelCol) const;
    void drawFreqAxis(juce::Graphics& g, juce::Rectangle<float> plot) const;
    void drawSettingsChrome(juce::Graphics& g) const;
};

} // namespace mixcoach
