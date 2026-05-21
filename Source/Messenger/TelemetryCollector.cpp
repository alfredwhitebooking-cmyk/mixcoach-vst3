#include "TelemetryCollector.h"

namespace mixcoach {

TrackTelemetry TelemetryCollector::collect(const juce::AudioBuffer<float>& buffer)
{
    TrackTelemetry telemetry;
    telemetry.timestamp = juce::Time::getMillisecondCounter() * 1000;
    telemetry.slotIndex = -1;
    telemetry.active    = true;

    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    if (numSamples > 0) {
        if (numChannels >= 2) {
            auto left  = buffer.getReadPointer(0);
            auto right = buffer.getReadPointer(1);

            telemetry.peakLeft     = computePeak(left, numSamples);
            telemetry.peakRight    = computePeak(right, numSamples);
            telemetry.rmsLeft      = computeRMS(left, numSamples);
            telemetry.rmsRight     = computeRMS(right, numSamples);
            telemetry.correlation  = computeCorrelation(left, right, numSamples);
        } else if (numChannels == 1) {
            auto mono = buffer.getReadPointer(0);
            telemetry.peakLeft     = computePeak(mono, numSamples);
            telemetry.peakRight    = telemetry.peakLeft;
            telemetry.rmsLeft      = computeRMS(mono, numSamples);
            telemetry.rmsRight     = telemetry.rmsLeft;
            telemetry.correlation  = 1.0f;
        }
    }

    return telemetry;
}

float TelemetryCollector::computePeak(const float* data, int numSamples) const
{
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        peak = std::max(peak, std::abs(data[i]));

    return (peak > 0.0f)
        ? juce::Decibels::gainToDecibels(peak)
        : -100.0f;
}

float TelemetryCollector::computeRMS(const float* data, int numSamples) const
{
    double sumSq = 0.0;
    for (int i = 0; i < numSamples; ++i)
        sumSq += static_cast<double>(data[i]) * data[i];

    return (sumSq > 0.0)
        ? static_cast<float>(juce::Decibels::gainToDecibels(
              static_cast<float>(std::sqrt(sumSq / numSamples))))
        : -100.0f;
}

float TelemetryCollector::computeCorrelation(const float* left, const float* right, int numSamples) const
{
    double sumProduct = 0.0;
    double sumLeftSq  = 0.0;
    double sumRightSq = 0.0;

    for (int i = 0; i < numSamples; ++i) {
        sumProduct += static_cast<double>(left[i]) * right[i];
        sumLeftSq  += static_cast<double>(left[i]) * left[i];
        sumRightSq += static_cast<double>(right[i]) * right[i];
    }

    auto denom = std::sqrt(sumLeftSq * sumRightSq);
    return (denom > 1e-12)
        ? static_cast<float>(sumProduct / denom)
        : 1.0f;
}

} // namespace mixcoach
