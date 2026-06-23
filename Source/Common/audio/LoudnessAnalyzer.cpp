#include "LoudnessAnalyzer.h"
#include <algorithm>
#include <cmath>

namespace mixcoach {

LoudnessAnalyzer::LoudnessAnalyzer()
{
    initFilters();
}

void LoudnessAnalyzer::initFilters()
{
    // ─── EBU R128 / ITU-R BS.1770-4 K-weighting filter ────────────────
    //
    // Stage 1: 2nd-order Butterworth high-pass @ 48.1 Hz
    //   H₁(s) = s² / (s² + √2·s + 1), with s normalized to 48.1 Hz
    //
    // Stage 2: High-shelving filter +4 dB @ 1.5 kHz, Q=0.5
    //   H₂(s) = G₀ · (1 + j·f/f_s) / (1 + j·f/(G₀^{1/2}·f_s))
    //   where G₀ = 10^(4/20), f_s = 1.5 kHz
    //
    // Both filters are chained via IIR::Filter<float> instances.

    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(
        sampleRate_, 48.1f);
    hpFilter_.coefficients = hpCoeffs;

    // High-shelf: +4 dB gain at 1.5 kHz, shelf Q = 0.5
    auto shelfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        sampleRate_, 1500.0f, 0.5f, 4.0f);
    shelfFilter_.coefficients = shelfCoeffs;

    // Reset filter states
    hpFilter_.reset();
    shelfFilter_.reset();
}

void LoudnessAnalyzer::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    sampleRate_ = sampleRate;

    // Block size for 400ms momentary window
    // We use ~100ms blocks (75% overlap → 4 blocks for 400ms)
    samplesPerBlock_ = static_cast<int>(sampleRate * 0.1);  // 100ms
    if (samplesPerBlock_ < 64)
        samplesPerBlock_ = 64;

    // Ring buffers sized for ~3s of data
    int totalMomentarySlots = 2 + static_cast<int>(3.0 / 0.1);  // ~32 slots
    int totalShortTermSlots = 2 + static_cast<int>(3.0 / 0.1);  // ~32 slots

    momentaryBlocks_.assign(totalMomentarySlots, 0.0f);
    shortTermBlocks_.assign(totalShortTermSlots, 0.0f);
    momentaryWrite_ = 0;
    shortTermWrite_ = 0;
    blockCounter_ = 0;
    blockSumSq_ = 0.0;
    blockSampleCount_ = 0;

    // Reset integrated values
    resetAccumulators();

    // Re-init filters with actual sample rate and reset states
    initFilters();

    // Histogram for LRA
    stHistogram_.reserve(kMaxHistogram);
}

void LoudnessAnalyzer::resetAccumulators()
{
    gatedSumSq_ = 0.0;
    gatedCount_ = 0;
    prelimSumSq_ = 0.0;
    prelimCount_ = 0;
    prelimLUFS_ = -100.0f;
    gatingDone_ = false;

    truePeak_ = 0.0f;
    upsamplerDelay_ = { 0.0f, 0.0f, 0.0f };

    truePeakDBTP_.store(-100.0f, std::memory_order_release);
    momentaryLUFS_.store(-100.0f, std::memory_order_release);
    shortTermLUFS_.store(-100.0f, std::memory_order_release);
    integratedLUFS_.store(-100.0f, std::memory_order_release);
    loudnessRange_.store(0.0f, std::memory_order_release);

    stHistogram_.clear();
}

void LoudnessAnalyzer::processBlock(const float* left, const float* right,
                                    int numSamples)
{
    if (numSamples <= 0)
        return;

    // ─── Step 1: Sum L+R to mono, apply K-weighting ────────────────────
    // (EBU R128 measures the sum signal)
    for (int i = 0; i < numSamples; ++i)
    {
        float mono = (left[i] + right[i]) * 0.5f;

        // Apply K-weighting (HPF → Shelf, EBU R128 / BS.1770-4)
        float filtered = shelfFilter_.processSample(
                             hpFilter_.processSample(mono));

        // Accumulate RMS² for current block
        blockSumSq_ += static_cast<double>(filtered) * filtered;
        ++blockSampleCount_;
    }

    // ─── Step 2: Flush completed blocks ────────────────────────────────
    while (blockSampleCount_ >= samplesPerBlock_)
        flushBlock();

    // ─── Step 3: Update True Peak (on raw sum, before filtering) ───────
    // True peak measures the PEAK of the reconstructed analog waveform.
    // We use linear interpolation / oversampling approximation.
    updateTruePeak(left, right, numSamples);

    // ─── Step 4: Update computed values ────────────────────────────────
    // Momentary: average of last 4 blocks (400ms with 100ms overlaps)
    {
        int nMomentary = 4;
        double sum = 0.0;
        int count = 0;
        int sz = static_cast<int>(momentaryBlocks_.size());
        for (int i = 0; i < nMomentary && i < sz; ++i)
        {
            int idx = (momentaryWrite_ - 1 - i + sz) % sz;
            sum += momentaryBlocks_[idx];
            ++count;
        }
        if (count > 0)
            momentaryLUFS_.store(computeLUFS(sum, count), std::memory_order_release);
    }

    // Short-term: average of last 30 blocks (3s)
    {
        int nShortTerm = 30;
        double sum = 0.0;
        int count = 0;
        int sz = static_cast<int>(shortTermBlocks_.size());
        for (int i = 0; i < nShortTerm && i < sz; ++i)
        {
            int idx = (shortTermWrite_ - 1 - i + sz) % sz;
            float val = shortTermBlocks_[idx];
            if (val > 1e-15f)
            {
                sum += val;
                ++count;
            }
        }
        if (count > 0)
            shortTermLUFS_.store(computeLUFS(sum, count), std::memory_order_release);
    }

    // Integrated (gated): update once per block flush
    updateIntegrated();

    // Loudness Range: from short-term histogram
    {
        if (!stHistogram_.empty())
        {
            auto sorted = stHistogram_;
            std::sort(sorted.begin(), sorted.end());
            int lowIdx  = static_cast<int>(sorted.size() * 0.1f);
            int highIdx = static_cast<int>(sorted.size() * 0.95f);
            lowIdx  = juce::jlimit(0, static_cast<int>(sorted.size()) - 1, lowIdx);
            highIdx = juce::jlimit(0, static_cast<int>(sorted.size()) - 1, highIdx);
            loudnessRange_.store(sorted[highIdx] - sorted[lowIdx], std::memory_order_release);
        }
    }
}

void LoudnessAnalyzer::flushBlock()
{
    if (blockSampleCount_ <= 0)
    {
        blockSumSq_ = 0.0;
        blockSampleCount_ = 0;
        return;
    }

    // Compute RMS² for this block
    double rmsSq = blockSumSq_ / blockSampleCount_;
    float blockVal = static_cast<float>(rmsSq);

    // Store in momentary ring buffer
    int mIdx = momentaryWrite_ % momentaryBlocks_.size();
    momentaryBlocks_[mIdx] = blockVal;
    ++momentaryWrite_;

    // For short-term: store every 3rd block (300ms intervals → 10 per 3s)
    // This matches the 75% overlap pattern of momentary
    if (blockCounter_ % 3 == 0)
    {
        int sIdx = shortTermWrite_ % shortTermBlocks_.size();
        shortTermBlocks_[sIdx] = blockVal;
        ++shortTermWrite_;

        // Store for LRA histogram (short-term LUFS values)
        float stLUFS = computeLUFS(rmsSq, 1);
        if (stLUFS > -70.0f && static_cast<int>(stHistogram_.size()) < kMaxHistogram)
            stHistogram_.push_back(stLUFS);
    }

    // Gate 1 (preliminary): absolute threshold -70 LUFS
    float blockLUFS = computeLUFS(rmsSq, 1);
    if (blockLUFS > -70.0f)
    {
        prelimSumSq_ += rmsSq;
        ++prelimCount_;
    }

    ++blockCounter_;
    blockSumSq_ = 0.0;
    blockSampleCount_ = 0;
    gatingDone_ = false;  // Recompute on next read
}

void LoudnessAnalyzer::updateIntegrated()
{
    if (prelimCount_ <= 0)
        return;

    // Gate 1 result: measure without relative gating
    float gate1LUFS = computeLUFS(prelimSumSq_, prelimCount_);

    // Gate 2: relative threshold = Gate1 - 10 LU
    float relThresholdLUFS = gate1LUFS - 10.0f;

    // Re-scan all stored momentary blocks for Gate 2
    // We approximate by using the current stored buffer
    double gate2SumSq = 0.0;
    int gate2Count = 0;

    int sz = static_cast<int>(momentaryBlocks_.size());
    for (int i = 0; i < sz; ++i)
    {
        float val = momentaryBlocks_[i];
        if (val > 1e-15f)
        {
            float blkLUFS = computeLUFS(val, 1);
            if (blkLUFS > relThresholdLUFS)
            {
                gate2SumSq += val;
                ++gate2Count;
            }
        }
    }

    if (gate2Count > 0)
    {
        integratedLUFS_.store(computeLUFS(gate2SumSq, gate2Count), std::memory_order_release);
        gatingDone_ = true;
    }
    else
    {
        integratedLUFS_.store(gate1LUFS, std::memory_order_release);
    }

    gatedSumSq_ = gate2SumSq;
    gatedCount_ = gate2Count;
}

void LoudnessAnalyzer::updateTruePeak(const float* left, const float* right,
                                      int numSamples)
{
    // True peak estimation via linear interpolation (4x oversampling)
    // For each pair of consecutive samples, we linearly interpolate
    // 3 points between them and find the maximum.
    const float* channels[2] = { left, right };
    float maxPeak = truePeak_;

    for (int ch = 0; ch < 2; ++ch)
    {
        const float* data = channels[ch];
        if (data == nullptr) continue;

        // Process first sample with delay line
        float prev = upsamplerDelay_[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            float curr = data[i];

            // Interpolate 3 points between prev and curr
            // Linear interpolation coefficients
            float a0 = prev;
            float a1 = curr;

            // Check 3 intermediate points
            for (int k = 1; k <= 3; ++k)
            {
                float t = k / 4.0f;
                float interp = a0 + (a1 - a0) * t;
                maxPeak = juce::jmax(maxPeak, std::abs(interp));
            }

            // Also check the original sample
            maxPeak = juce::jmax(maxPeak, std::abs(curr));

            prev = curr;
        }

        upsamplerDelay_[ch] = prev;
    }

    // Apply decay: true peak releases over ~3s
    // Decay factor: 0.9995 per sample at 48kHz → ~0.5dB/s release
    float decayed = truePeak_ * 0.99995f;  // ~0.05dB/s release at 48kHz
    truePeak_ = juce::jmax(maxPeak, decayed);
    truePeakDBTP_.store((truePeak_ > 1e-12f)
        ? juce::Decibels::gainToDecibels(truePeak_)
        : -100.0f, std::memory_order_release);
}

} // namespace mixcoach
