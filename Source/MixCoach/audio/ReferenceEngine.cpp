#include "ReferenceEngine.h"
#include "ReferenceMetrics.h"
#include <algorithm>
#include <cmath>

namespace mixcoach {

static juce::String dbToString (float valueDb)
{
    return juce::String (valueDb, 1) + " dB";
}

static juce::String percentString (float percent)
{
    return juce::String (percent * 100.0f, 1) + "%";
}

std::vector<Suggestion> ReferenceEngine::generateSuggestions (const ReferenceMetrics& mixMetrics,
                                                          const ReferenceMetrics& refMetrics,
                                                          const std::map<juce::String,float>& thresholds)
{
    std::vector<Suggestion> suggestions;
    // Helper lambda to add suggestion
    auto addSuggestion = [&](const juce::String& name, float delta, const juce::String& desc)
    {
        Suggestion s; s.bandName = name; s.deltaDb = delta; s.description = desc; suggestions.push_back (std::move (s));
    };

    // Centroid (Hz) – use percentage threshold
    if (auto it = thresholds.find ("centroid"); it != thresholds.end())
    {
        float refCent = refMetrics.centroid > 0.0f ? refMetrics.centroid : 1.0f;
        float deltaHz = mixMetrics.centroid - refCent;
        float deltaPct = std::abs (deltaHz) / refCent;
        if (deltaPct > it->second) // threshold expressed as fraction (e.g., 0.10 for 10%)
        {
            juce::String desc = juce::String ("Centroid: ") + juce::String (mixMetrics.centroid, 1)
                                + " Hz (ref " + juce::String (refMetrics.centroid, 1) + " Hz) – "+
                                (deltaHz > 0 ? "Increase" : "Decrease") + " by " + percentString (deltaPct) + ".";
            addSuggestion ("Centroid", deltaHz, desc);
        }
    }

    // RMS per band – thresholds in dB
    for (int i = 0; i < ReferenceMetrics::kNumBands; ++i)
    {
        juce::String key = juce::String ("rmsBand") + juce::String (i);
        if (auto it = thresholds.find (key); it != thresholds.end())
        {
            // Convert RMS (linear) to dB
            float mixRmsDb  = 20.0f * std::log10 (std::max (mixMetrics.rmsByBand[i], 1e-6f));
            float refRmsDb  = 20.0f * std::log10 (std::max (refMetrics.rmsByBand[i], 1e-6f));
            float deltaDb   = mixRmsDb - refRmsDb;
            if (std::abs (deltaDb) > it->second)
            {
                // Band names as defined in implementation plan
                static const char* bandNames[ReferenceMetrics::kNumBands] = {"Sub","Bass","Low‑Mid","High‑Mid","Presence","High","Air"};
                juce::String desc = juce::String (bandNames[i]) + ": " + dbToString (mixRmsDb) +
                                    " (ref " + dbToString (refRmsDb) + ") – "+
                                    (deltaDb > 0 ? "Increase" : "Decrease") + " by " + dbToString (std::abs (deltaDb)) + ".";
                addSuggestion (bandNames[i], deltaDb, desc);
            }
        }
    }

    // Crest factor – threshold in dB (difference) 
    if (auto it = thresholds.find ("crestFactor"); it != thresholds.end())
    {
        float mixCrest = mixMetrics.crestFactor > 0.0f ? 20.0f * std::log10 (mixMetrics.crestFactor) : 0.0f;
        float refCrest = refMetrics.crestFactor > 0.0f ? 20.0f * std::log10 (refMetrics.crestFactor) : 0.0f;
        float delta = mixCrest - refCrest;
        if (std::abs (delta) > it->second)
        {
            juce::String desc = juce::String ("Crest factor: ") + dbToString (mixCrest) +
                                " (ref " + dbToString (refCrest) + ") – "+
                                (delta > 0 ? "Increase" : "Decrease") + " by " + dbToString (std::abs (delta)) + ".";
            addSuggestion ("CrestFactor", delta, desc);
        }
    }

    // LUFS per band – thresholds in dB
    for (int i = 0; i < ReferenceMetrics::kNumBands; ++i)
    {
        juce::String key = juce::String ("lufsBand") + juce::String (i);
        if (auto it = thresholds.find (key); it != thresholds.end())
        {
            float delta = mixMetrics.lufsByBand[i] - refMetrics.lufsByBand[i];
            if (std::abs (delta) > it->second)
            {
                static const char* bandNames[ReferenceMetrics::kNumBands] = {"Sub","Bass","Low‑Mid","High‑Mid","Presence","High","Air"};
                juce::String desc = juce::String (bandNames[i]) + " LUFS: " + juce::String (mixMetrics.lufsByBand[i], 1) +
                                    " (ref " + juce::String (refMetrics.lufsByBand[i], 1) + ") – "+
                                    (delta > 0 ? "Increase" : "Decrease") + " by " + dbToString (std::abs (delta)) + ".";
                addSuggestion (juce::String (bandNames[i]) + " LUFS", delta, desc);
            }
        }
    }

    return suggestions;
}

} // namespace mixcoach
