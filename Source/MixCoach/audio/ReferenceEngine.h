#pragma once

#include <juce_core/juce_core.h>

namespace mixcoach {

/** Simple struct that describes a suggestion for a specific band or metric. */
struct Suggestion {
    juce::String bandName;   // e.g. "Sub", "Low‑Mid", "Centroid"
    float       deltaDb;     // positive = increase, negative = decrease
    juce::String description; // full human‑readable text
};

/** Engine that compares the metrics of the current mix with a reference
    and produces plain‑text correction suggestions. */
class ReferenceEngine {
public:
    /**
        thresholds: map where the key is the metric name ("centroid", "rmsBand0", ...)
        and the value is the allowed deviation (in dB for RMS/LUFS/crest, in % for centroid).
        If a metric exceeds the threshold, a Suggestion is generated.
    */
    static std::vector<Suggestion> generateSuggestions (const ReferenceMetrics& mixMetrics,
                                                        const ReferenceMetrics& refMetrics,
                                                        const std::map<juce::String,float>& thresholds);
};

} // namespace mixcoach
