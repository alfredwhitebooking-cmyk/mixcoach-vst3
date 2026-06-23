#include "AudioAnalyzer.h"
#include "../UI/VectorscopeComponent.h"
#include <algorithm>
#include <cmath>
#include "Common/types/Constants.h"

namespace mixcoach {

    void AudioAnalyzer::prepare(double sampleRate, int samplesPerBlock)
    {
        sampleRate_ = sampleRate;
        masterAnalysis_.prepare(sampleRate, samplesPerBlock);
        leftAnalysis_.prepare(sampleRate, samplesPerBlock);
        rightAnalysis_.prepare(sampleRate, samplesPerBlock);
        loudness_.prepare(sampleRate, samplesPerBlock);
        sampleBufferWrite_.store(0);
        lastReadIndex_   = 0;
        decimateCounter_ = 0;

        // ═══ Preasignar master buffer (evita heap alloc en audio thread) ═══
        // Usamos el samplesPerBlock maximo para que nunca necesite redimensionarse.
        maxBlockSize_ = samplesPerBlock;
        if (masterBuffer_.getNumSamples() < samplesPerBlock) masterBuffer_.setSize(1, samplesPerBlock);
    }

    void AudioAnalyzer::processBlock(const juce::AudioBuffer<float>& buffer)
    {
        auto numSamples = buffer.getNumSamples();

        if (buffer.getNumChannels() >= 2) {
            auto left  = buffer.getReadPointer(0);
            auto right = buffer.getReadPointer(1);

            leftAnalysis_.process(left, numSamples);
            rightAnalysis_.process(right, numSamples);

            // ─── Master = mezcla de L + R ─────────────────────────────────
            // Usamos el buffer preasignado para evitar heap allocation en audio thread.
            // jassert: en debug, verificar que prepare() nos dio suficiente espacio.
            jassert(numSamples <= maxBlockSize_);
            if (numSamples > masterBuffer_.getNumSamples()) masterBuffer_.setSize(1, numSamples, false, false, true);
            auto* masterData = masterBuffer_.getWritePointer(0);
            for (int i = 0; i < numSamples; ++i) masterData[i] = (left[i] + right[i]) * 0.5f;

            masterAnalysis_.process(masterData, numSamples);

            // ─── Stereo Width: mid/side energy ratio ──────────────────────
            // avgStereoWidth = sideEnergy / (midEnergy + sideEnergy)
            // 0.0 = fully mono, 1.0 = fully side/wide
            double midEnergy = 0.0, sideEnergy = 0.0;
            for (int i = 0; i < numSamples; ++i) {
                float m = (left[i] + right[i]) * 0.5f;
                float s = (left[i] - right[i]) * 0.5f;
                midEnergy += static_cast<double>(m) * m;
                sideEnergy += static_cast<double>(s) * s;
            }
            float total = static_cast<float>(midEnergy + sideEnergy);
            float width = (total > 1e-12f) ? static_cast<float>(sideEnergy / total) : 0.0f;
            avgStereoWidth_.store(juce::jlimit(0.0f, 1.0f, width), std::memory_order_relaxed);

            // ─── Phase correlation (L/R) ──────────────────────────────────
            double sumProduct = 0.0;
            double sumLeftSq  = 0.0;
            double sumRightSq = 0.0;
            for (int i = 0; i < numSamples; ++i) {
                sumProduct += static_cast<double>(left[i]) * right[i];
                sumLeftSq += static_cast<double>(left[i]) * left[i];
                sumRightSq += static_cast<double>(right[i]) * right[i];
            }
            float correlation = 1.0f;
            auto denom        = std::sqrt(sumLeftSq * sumRightSq);
            if (denom > 1e-12) correlation = static_cast<float>(sumProduct / denom);
            masterAnalysis_.setCorrelation(correlation);

            // ─── LUFS analysis (siempre, cada bloque) ──────────────────────
            loudness_.processBlock(left, right, numSamples);

            // ─── Vectorscope sample buffer (decimado ~8x) ──────────────────
            // Almacena samples L/R decimados en ring buffer circular.
            // Audio thread escribe, UI thread lee (race aceptable para metering).
            int writeIdx = sampleBufferWrite_.load();
            for (int i = 0; i < numSamples; ++i) {
                if (++decimateCounter_ >= kDecimateFactor) {
                    decimateCounter_                            = 0;
                    sampleBuffer_[writeIdx % kSampleBufferSize] = {left[i], right[i]};
                    ++writeIdx;
                }
            }
            sampleBufferWrite_.store(writeIdx);
        }
        else if (buffer.getNumChannels() == 1) {
            auto mono = buffer.getReadPointer(0);
            masterAnalysis_.process(mono, numSamples);
            leftAnalysis_.process(mono, numSamples);
            rightAnalysis_.process(mono, numSamples);
            masterAnalysis_.setCorrelation(1.0f);

            // LUFS mono: usa mismo buffer para L y R
            loudness_.processBlock(mono, mono, numSamples);

            // Vectorscope sample buffer (mono -> L=R)
            int writeIdx = sampleBufferWrite_.load();
            for (int i = 0; i < numSamples; ++i) {
                if (++decimateCounter_ >= kDecimateFactor) {
                    decimateCounter_                            = 0;
                    sampleBuffer_[writeIdx % kSampleBufferSize] = {mono[i], mono[i]};
                    ++writeIdx;
                }
            }
            sampleBufferWrite_.store(writeIdx);
        }
    }

    void AudioAnalyzer::flushSampleBufferToVectorscope(VectorscopeComponent& vectorscope)
    {
        // Lee todos los samples nuevos desde el ring buffer y los empuja
        // al vectorscope. lastReadIndex_ se actualiza para evitar duplicados.
        // Solo se llama desde UI thread.
        int writePos = sampleBufferWrite_.load(std::memory_order_acquire);
        int readPos  = lastReadIndex_;

        if (writePos <= readPos) return;

        int totalToPush        = writePos - readPos;
        constexpr int kMaxPush = 256; // No saturar el vectorscope en un solo frame
        int toPush             = std::min(totalToPush, kMaxPush);
        int startPos           = writePos - toPush; // tomar los más recientes

        for (int i = 0; i < toPush; ++i) {
            int idx = (startPos + i) % kSampleBufferSize;
            vectorscope.pushSample(sampleBuffer_[idx].l, sampleBuffer_[idx].r);
        }

        lastReadIndex_ = writePos;
    }

    ReferenceMetrics AudioAnalyzer::computeReferenceMetrics() const noexcept
    {
        ReferenceMetrics metrics{};
        // Compute overall RMS and peak from master analysis
        float overallRMS = masterAnalysis_.getRMS();
        float peak       = masterAnalysis_.getPeak();
        // Crest factor (peak / RMS)
        metrics.crestFactor = (overallRMS > 0.0f) ? (peak / overallRMS) : 0.0f;
        // Compute spectral centroid and spread using spectrum
        const float* spectrum = masterAnalysis_.getSpectrum();
        if (spectrum != nullptr) {
            double sumMag    = 0.0;
            double sumFreq   = 0.0;
            double sumSqFreq = 0.0;
            // Frequency per bin (assuming linear up to Nyquist)
            // Use sampleRate_ and kFFTSize to derive bin frequency.
            const double binFreq = sampleRate_ / static_cast<double>(kFFTSize);
            for (int i = 0; i < kNumSpectrumBins; ++i) {
                float mag = spectrum[i];
                if (mag <= 0.0f) continue;
                sumMag += mag;
                double freq = i * binFreq;
                sumFreq += freq * mag;
                sumSqFreq += freq * freq * mag;
            }
            if (sumMag > 0.0) {
                metrics.centroid = static_cast<float>(sumFreq / sumMag);
                double var       = (sumSqFreq / sumMag) - (metrics.centroid * metrics.centroid);
                metrics.spread   = static_cast<float>(std::sqrt(std::max(0.0, var)));
            }
        }
        // RMS per band – using same band definitions as ReferenceAnalyzer
        // Duplicate band bin definitions
        static constexpr int kNumBands               = 7;
        static constexpr int kBandBins[kNumBands][2] = {
            {1, 5},     // Sub:       43-215 Hz
            {5, 12},    // Bass:      215-516 Hz
            {12, 35},   // Low-Mid:   516-1500 Hz
            {35, 70},   // High-Mid:  1500-3000 Hz
            {70, 120},  // Presence:  3000-5160 Hz
            {120, 200}, // High:      5160-8600 Hz
            {200, 300}  // Air:       8600-12900 Hz
        };
        // Compute RMS in each band from spectrum magnitude
        if (spectrum != nullptr) {
            for (int b = 0; b < kNumBands; ++b) {
                double sum   = 0.0;
                int count    = 0;
                int startBin = kBandBins[b][0];
                int endBin   = std::min(kBandBins[b][1], kNumSpectrumBins);
                for (int i = startBin; i < endBin; ++i) {
                    sum += spectrum[i];
                    ++count;
                }
                metrics.rmsByBand[b] = (count > 0) ? static_cast<float>(sum / count) : 0.0f;
            }
        }
        // LUFS per band – placeholder: use overall momentary LUFS for first band, zero for others
        metrics.lufsByBand[0] = getMomentaryLUFS();
        for (int i = 1; i < ReferenceMetrics::kNumBands; ++i) {
            metrics.lufsByBand[i] = 0.0f;
        }
        return metrics;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  flushSamples — Lee samples no leídos del ring buffer en buffers externos.
    //  Solo se llama desde UI thread (no concurrente con flushSampleBufferToVectorscope).
    // ═══════════════════════════════════════════════════════════════════════════
    int AudioAnalyzer::flushSamples(float* leftOut, float* rightOut, int maxCount) const
    {
        int writePos = sampleBufferWrite_.load(std::memory_order_acquire);
        int readPos  = lastReadIndex_;

        if (writePos <= readPos || maxCount <= 0) return 0;

        int totalAvail = writePos - readPos;
        int toRead     = std::min(totalAvail, maxCount);
        int startPos   = writePos - toRead; // tomar los más recientes

        for (int i = 0; i < toRead; ++i) {
            int idx     = (startPos + i) % kSampleBufferSize;
            leftOut[i]  = sampleBuffer_[idx].l;
            rightOut[i] = sampleBuffer_[idx].r;
        }

        lastReadIndex_ = writePos;
        return toRead;
    }

} // namespace mixcoach
