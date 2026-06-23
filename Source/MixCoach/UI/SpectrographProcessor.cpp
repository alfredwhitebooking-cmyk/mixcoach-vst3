#include "SpectrographComponent.h"
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  SET SAMPLE RATE
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::setSampleRate(double sampleRate)
    {
        if (sampleRate < 8000.0) return;

        if (std::abs(sampleRate_ - sampleRate) > 1.0) {
            sampleRate_ = sampleRate;
            rebuildBands();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  REBUILD BANDS — Recalcula las 60 bandas logarítmicas
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::rebuildBands()
    {
        bands_.resize((size_t)kNumRtaBands);

        std::vector<float> centers((size_t)kNumRtaBands);
        for (int i = 0; i < kNumRtaBands; ++i) {
            const float t      = (kNumRtaBands <= 1) ? 0.0f : (float)i / (float)(kNumRtaBands - 1);
            centers[(size_t)i] = kMinFreq * std::pow(kMaxFreq / kMinFreq, t);
        }

        for (int i = 0; i < kNumRtaBands; ++i) {
            auto& b              = bands_[(size_t)i];
            b.centerHz           = centers[(size_t)i];
            const float lowEdge  = (i == 0) ? kMinFreq : std::sqrt(centers[(size_t)(i - 1)] * centers[(size_t)i]);
            const float highEdge = (i == kNumRtaBands - 1) ? kMaxFreq
                                                           : std::sqrt(centers[(size_t)i] * centers[(size_t)(i + 1)]);
            b.lowHz              = lowEdge;
            b.highHz             = highEdge;
        }

        peakDecayRates_.resize(kNumRtaBands);
        peakHoldTimers_.resize(kNumRtaBands, 0.0f);
        static const float kLogMin   = std::log2(20.0f);
        static const float kLogMax   = std::log2(20000.0f);
        static const float kLogRange = kLogMax - kLogMin;
        for (int i = 0; i < kNumRtaBands; ++i) {
            const float t              = (std::log2(bands_[(size_t)i].centerHz) - kLogMin) / kLogRange;
            peakDecayRates_[(size_t)i] = 0.350f - 0.200f * t;
        }

        computeDefaultReferenceCurve();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  COORDINATE HELPERS
    // ═══════════════════════════════════════════════════════════════════════════

    float SpectrographComponent::dbToDisplayNorm(float db) const noexcept
    {
        return juce::jlimit(0.0f, 1.0f, juce::jmap(db, kDisplayBottomDb, kDisplayTopDb, 0.0f, 1.0f));
    }

    float SpectrographComponent::freqToX(float freqHz, juce::Rectangle<float> plot) const noexcept
    {
        freqHz           = juce::jlimit(0.0f, kMaxFreq, freqHz);
        const float norm = (freqHz <= kMinFreq) ? 0.0f : std::log2(freqHz / kMinFreq) / std::log2(kMaxFreq / kMinFreq);
        return plot.getX() + norm * plot.getWidth();
    }

    float SpectrographComponent::xToFreq(float x, juce::Rectangle<float> plot) const noexcept
    {
        const float norm = juce::jmap(x, plot.getX(), plot.getRight(), 0.0f, 1.0f);
        if (norm <= 0.0f) return kMinFreq;
        if (norm >= 1.0f) return kMaxFreq;
        return kMinFreq * std::pow(kMaxFreq / kMinFreq, norm);
    }

    float SpectrographComponent::displayNormToDb(float norm) const noexcept
    {
        return juce::jmap(norm, 0.0f, 1.0f, kDisplayBottomDb, kDisplayTopDb);
    }

    void SpectrographComponent::invalidateStaticCache()
    {
        staticCacheValid_ = false;
        staticCache_      = juce::Image();
        layout_.valid     = false;
    }

    SpectrographComponent::PlotLayout SpectrographComponent::computePlotLayout() const
    {
        PlotLayout L;
        if (plotArea_.isEmpty()) return L;

        auto area = plotArea_.toFloat();
        area.setPosition(0, 0);
        auto dbAxis = area.removeFromLeft(20.0f);
        area.removeFromBottom(16.0f);
        area.removeFromTop(8.0f);

        L.plot  = area;
        L.dbCol = dbAxis.withHeight(L.plot.getHeight()).withY(L.plot.getY());
        L.valid = !L.plot.isEmpty();
        return L;
    }

    void SpectrographComponent::rebuildStaticCache()
    {
        invalidateStaticCache();

        if (plotArea_.isEmpty()) return;

        layout_ = computePlotLayout();
        if (!layout_.valid) return;

        const int w = plotArea_.getWidth();
        const int h = plotArea_.getHeight();
        if (w < 8 || h < 8) return;

        staticCache_ = juce::Image(juce::Image::ARGB, w, h, true);
        staticCache_.clear(staticCache_.getBounds());

        juce::Graphics cg(staticCache_);

        drawPlotBackground(cg, layout_.plot);
        drawGrid(cg, layout_.plot);

        cg.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
        cg.setColour(juce::Colour(0xFF44BBFF).withAlpha(0.60f));
        cg.drawText(juce::CharPointer_UTF8("RTA"),
                    juce::Rectangle<float>(layout_.plot.getX() + 4.0f, layout_.plot.getY() + 2.0f, 28.0f, 10.0f),
                    juce::Justification::centredLeft);

        staticCacheValid_ = true;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  UPDATE SPECTRUM — Pipeline principal de análisis espectral
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::updateSpectrum(const float* data, int numBins)
    {
        if (data == nullptr || numBins <= 0) return;

        lastSpectrumUpdateMs_ = juce::Time::getMillisecondCounter();

        if (bands_.empty()) rebuildBands();

        // STAGE 1: Adaptive Fractional Octave Smoothing
        const float binWidth = (float)sampleRate_ / (float)(numBins * 2);
        std::vector<float> smoothedBins(static_cast<size_t>(numBins));
        if (numBins > 0) {
            std::vector<double> prefixSq(static_cast<size_t>(numBins + 1), 0.0);
            for (int i = 0; i < numBins; ++i) {
                const double mag                     = static_cast<double>(juce::jmax(0.0f, data[i]));
                prefixSq[static_cast<size_t>(i + 1)] = prefixSq[static_cast<size_t>(i)] + mag * mag;
            }

            constexpr float kRatioNarrowLo = 0.986f;
            constexpr float kRatioNarrowHi = 1.015f;
            constexpr float kRatioMidLo    = 0.972f;
            constexpr float kRatioMidHi    = 1.029f;

            for (int i = 0; i < numBins; ++i) {
                const float freq = (float)(i)*binWidth + binWidth * 0.5f;

                float lowRatio, highRatio;
                if (freq < 500.0f) {
                    lowRatio  = kRatioNarrowLo;
                    highRatio = kRatioNarrowHi;
                }
                else {
                    lowRatio  = kRatioMidLo;
                    highRatio = kRatioMidHi;
                }

                int lowBin  = (int)(freq * lowRatio / binWidth);
                int highBin = (int)(freq * highRatio / binWidth);
                lowBin      = std::max(0, lowBin);
                highBin     = std::min(numBins - 1, highBin);

                const int count = highBin - lowBin + 1;
                const double meanSq =
                    (prefixSq[static_cast<size_t>(highBin + 1)] - prefixSq[static_cast<size_t>(lowBin)]) / count;
                smoothedBins[static_cast<size_t>(i)] = static_cast<float>(std::sqrt(meanSq));
            }
        }

        // STAGE 2: High-Resolution Spectral Envelope (2048 points)
        highResEnvelope_.resize(kHighResPoints);
        for (int p = 0; p < kHighResPoints; ++p) {
            const float t    = (float)p / (float)(kHighResPoints - 1);
            const float freq = 20.0f * std::pow(1000.0f, t);

            const float binPos = freq / binWidth;
            const int binLow   = juce::jlimit(0, numBins - 1, (int)binPos);
            const int binHigh  = juce::jlimit(0, numBins - 1, binLow + 1);
            const float frac   = binPos - (float)binLow;

            const float& vLow           = smoothedBins[(size_t)binLow];
            const float& vHigh          = smoothedBins[(size_t)binHigh];
            highResEnvelope_[(size_t)p] = vLow + (vHigh - vLow) * frac;
        }

        // STAGE 3: Band Aggregation from High-Res Envelope
        for (int i = 0; i < kNumRtaBands; ++i) {
            const auto& band = bands_[(size_t)i];

            float maxMag = 0.0f;
            double sumSq = 0.0;
            int count    = 0;

            for (int p = 0; p < kHighResPoints; ++p) {
                const float t    = (float)p / (float)(kHighResPoints - 1);
                const float freq = 20.0f * std::pow(1000.0f, t);

                if (freq >= band.lowHz && freq < band.highHz) {
                    const float m = highResEnvelope_[(size_t)p];
                    maxMag        = juce::jmax(maxMag, m);
                    sumSq += static_cast<double>(m) * static_cast<double>(m);
                    count++;
                }
            }

            float avgRms = (count > 0) ? static_cast<float>(std::sqrt(sumSq / static_cast<double>(count))) : 0.0f;

            float peakRaw = (maxMag > 1e-10f) ? 20.0f * std::log10(maxMag) : kDisplayBottomDb;
            float rmsRaw  = (avgRms > 1e-10f) ? 20.0f * std::log10(avgRms) : kDisplayBottomDb;

            constexpr float kRefFreq = 1000.0f;
            float slopeVal           = kSlopePresets[slopePresetIndex_];
            float slopeCorrection    = slopeVal * std::log2(band.centerHz / kRefFreq);

            float tiltCompensation = 0.0f;
            if (pinkNoiseEnabled_) tiltCompensation = 3.0f * std::log2(band.centerHz / 1000.0f);

            float peakDb = peakRaw + slopeCorrection + tiltCompensation;
            float rmsDb  = rmsRaw + slopeCorrection + tiltCompensation;

            peakDb = juce::jlimit(kDisplayBottomDb, kDisplayTopDb, peakDb);
            rmsDb  = juce::jlimit(kDisplayBottomDb, kDisplayTopDb, rmsDb);

            bandLevels_[(size_t)i].setTargetValue(dbToDisplayNorm(peakDb));
            bandRmsLevels_[(size_t)i].setTargetValue(dbToDisplayNorm(rmsDb));
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  RESET SPECTRUM
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::resetSpectrum()
    {
        lastSpectrumUpdateMs_ = juce::Time::getMillisecondCounter();
        highResEnvelope_.assign(kHighResPoints, 0.0f);

        for (int i = 0; i < kNumRtaBands; ++i) {
            bandLevels_[(size_t)i].reset(0.0f);
            bandRmsLevels_[(size_t)i].reset(0.0f);
            bandPeaks_[(size_t)i]      = 0.0f;
            peakHoldTimers_[(size_t)i] = 0.0f;
        }

        for (auto& slice : waterfall_) slice.valid = false;
        waterfallWritePos_  = 0;
        waterfallCount_     = 0;
        waterfallFrameSkip_ = 0;

        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  SMOOTH SPECTRUM — Avanza suavizado de barras + peak hold + waterfall
    // ═══════════════════════════════════════════════════════════════════════════

    bool SpectrographComponent::smoothSpectrum(double sampleRateHz, bool allowRepaint)
    {
        bool needsRepaint = false;
        const float invSr = 1.0f / static_cast<float>(sampleRateHz);

        uint32_t nowMs                       = juce::Time::getMillisecondCounter();
        constexpr uint32_t kStaleThresholdMs = 80;
        bool dataStale                       = (nowMs - lastSpectrumUpdateMs_ > kStaleThresholdMs);

        for (int i = 0; i < kNumRtaBands; ++i) {
            if (dataStale) {
                bandLevels_[(size_t)i].setTargetValue(0.0f);
                bandRmsLevels_[(size_t)i].setTargetValue(0.0f);
                bandPeaks_[(size_t)i]      = 0.0f;
                peakHoldTimers_[(size_t)i] = 0.0f;
                needsRepaint |= bandLevels_[(size_t)i].advance(sampleRateHz);
                needsRepaint |= bandRmsLevels_[(size_t)i].advance(sampleRateHz);
                continue;
            }

            bool peakChanged = bandLevels_[(size_t)i].advance(sampleRateHz);
            needsRepaint |= peakChanged;

            bool rmsChanged = bandRmsLevels_[(size_t)i].advance(sampleRateHz);
            needsRepaint |= rmsChanged;

            float current = bandLevels_[(size_t)i].getCurrent();
            if (current > bandPeaks_[(size_t)i]) {
                bandPeaks_[(size_t)i]      = current;
                peakHoldTimers_[(size_t)i] = 0.0f;
                needsRepaint               = true;
            }
            else {
                float prevPeak                = bandPeaks_[(size_t)i];
                constexpr float kPeakHoldTime = 0.300f;
                float& timer                  = peakHoldTimers_[(size_t)i];
                timer += invSr;

                if (timer >= kPeakHoldTime) {
                    const float tau   = peakDecayRates_[(size_t)i];
                    float decayFactor = std::exp(-invSr / tau);
                    bandPeaks_[(size_t)i] *= decayFactor;
                }

                if (bandPeaks_[(size_t)i] != prevPeak) needsRepaint = true;
            }
        }

        if (waterfallEnabled_) {
            waterfallFrameSkip_++;
            if (waterfallFrameSkip_ >= 4) {
                waterfallFrameSkip_ = 0;
                pushWaterfallSlice();
                needsRepaint = true;
            }
        }

        if (needsRepaint && allowRepaint) repaint();

        return needsRepaint;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  WATERFALL
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::pushWaterfallSlice()
    {
        auto& slice = waterfall_[waterfallWritePos_];
        for (int i = 0; i < kNumRtaBands; ++i) slice.levels[(size_t)i] = bandLevels_[(size_t)i].getCurrent();
        slice.valid = true;

        waterfallWritePos_ = (waterfallWritePos_ + 1) % kWaterfallRows;
        if (waterfallCount_ < kWaterfallRows) waterfallCount_++;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  FREQUENCY MARKERS
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::addFrequencyMarker(
        float freqHz, const juce::String& label, float severity, bool isCritical, bool isPraise, float durationSecs)
    {
        if (freqHz <= 0.0f || freqHz > kMaxFreq) return;

        FrequencyMarker marker;
        marker.frequencyHz  = freqHz;
        marker.label        = label;
        marker.severity     = severity;
        marker.isCritical   = isCritical;
        marker.isPraise     = isPraise;
        marker.timestampMs  = static_cast<int64_t>(juce::Time::getMillisecondCounter());
        marker.durationSecs = durationSecs;

        for (auto& m : frequencyMarkers_) {
            float logRatio = (freqHz > 0.0f && m.frequencyHz > 0.0f) ? std::abs(std::log2(freqHz / m.frequencyHz))
                                                                     : 999.0f;
            if (logRatio < 0.05f) {
                m = marker;
                return;
            }
        }

        if (frequencyMarkers_.size() >= static_cast<size_t>(kMaxFrequencyMarkers))
            frequencyMarkers_.erase(frequencyMarkers_.begin());

        frequencyMarkers_.push_back(marker);
    }

    void SpectrographComponent::clearFrequencyMarkers()
    {
        frequencyMarkers_.clear();
        repaint();
    }

    void SpectrographComponent::pruneFrequencyMarkers()
    {
        int64_t nowMs = static_cast<int64_t>(juce::Time::getMillisecondCounter());

        if (nowMs - lastMarkerPruneMs_ < 500) return;
        lastMarkerPruneMs_ = nowMs;

        auto it = frequencyMarkers_.begin();
        while (it != frequencyMarkers_.end()) {
            if (it->isExpired(nowMs)) it = frequencyMarkers_.erase(it);
            else
                ++it;
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  REFERENCE CURVE
    // ═══════════════════════════════════════════════════════════════════════════

    void SpectrographComponent::computeDefaultReferenceCurve()
    {
        if (bands_.empty()) return;

        referenceCurve_.resize(kNumRtaBands);

        struct RefCurveParams
        {
            float bassFreq      = 100.0f;
            float bassBoost     = 3.0f;
            float bassWidth     = 1.5f;
            float scoopFreq     = 350.0f;
            float scoopDepth    = -2.0f;
            float scoopWidth    = 1.2f;
            float presenceFreq  = 3000.0f;
            float presenceBoost = 1.5f;
            float presenceWidth = 1.5f;
            float highRolloff   = 12000.0f;
            float highRolloffDb = -6.0f;
            float subRolloff    = 40.0f;
            float subRolloffDb  = -3.0f;
            float centerDb      = -14.0f;
        };

        const RefCurveParams p;

        for (int i = 0; i < kNumRtaBands; ++i) {
            const float freq = bands_[(size_t)i].centerHz;

            float db = -3.0f * std::log2(freq / 1000.0f);

            db += p.bassBoost * std::exp(-0.5f * std::pow(std::log2(freq / p.bassFreq) / p.bassWidth, 2.0f));

            db += p.scoopDepth * std::exp(-0.5f * std::pow(std::log2(freq / p.scoopFreq) / p.scoopWidth, 2.0f));

            db +=
                p.presenceBoost * std::exp(-0.5f * std::pow(std::log2(freq / p.presenceFreq) / p.presenceWidth, 2.0f));

            if (freq > p.highRolloff) db += p.highRolloffDb * (freq - p.highRolloff) / (20000.0f - p.highRolloff);

            if (freq < p.subRolloff) db += p.subRolloffDb * (p.subRolloff - freq) / p.subRolloff;

            const float centeredDb     = db + p.centerDb;
            referenceCurve_[(size_t)i] = dbToDisplayNorm(centeredDb);
        }
    }

    void SpectrographComponent::setReferenceCurve(const float* data, int numBands)
    {
        if (data == nullptr || numBands <= 0) return;

        referenceCurve_.resize((size_t)numBands);
        for (int i = 0; i < numBands && i < kNumRtaBands; ++i)
            referenceCurve_[(size_t)i] = juce::jlimit(0.0f, 1.0f, data[i]);

        repaint();
    }

} // namespace mixcoach
