#pragma once

#include <juce_core/juce_core.h>
#include "SpectralData.h"

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  ReferenceSummary — Perceptual attributes derived from ReferenceFingerprint
    //
    //  Transforms raw technical data (30-band energies, LUFS, crest, correlation)
    //  into high-level musical descriptors: energy, brightness, depth, dynamics,
    //  stereo width, and an inferred genre.
    //
    //  Created immediately after ReferenceFingerprint analysis completes,
    //  giving the user instant high-level insight into the reference track.
    // ═══════════════════════════════════════════════════════════════════════════
    struct ReferenceSummary
    {
        bool valid = false;

        // ─── Perceptual attributes (0.0-1.0, mapped to 0-100%) ──────────────────
        float energy      = 0.0f;
        float brightness  = 0.0f;
        float depth       = 0.0f;
        float dynamics    = 0.0f;
        float stereoWidth = 0.0f;

        // ─── Inferred genre (heuristic based on spectral signature) ─────────────
        juce::String inferredGenre;

        // ─── Qualitative labels for display ─────────────────────────────────────
        juce::String energyLabel;
        juce::String brightnessLabel;
        juce::String depthLabel;
        juce::String dynamicsLabel;
        juce::String widthLabel;

        // ─── Raw values for tooltip / detail ────────────────────────────────────
        float integratedLUFS     = -100.0f;
        float crestFactor        = 0.0f;
        float correlation        = 0.0f;
        float spectralCentroidHz = 0.0f;
        float lufsMomentary      = -100.0f;
        float truePeakDBTP       = -100.0f;

        static ReferenceSummary compute(const ReferenceFingerprint& fp);

        [[nodiscard]] juce::String toShortDisplay() const;
        [[nodiscard]] const char* genreEmoji() const noexcept;

    private:
        static float energyFromLUFS(float l) noexcept;
        static float brightnessFromCentroid(float hz) noexcept;
        static float widthFromCorrelation(float c) noexcept;
        static float dynamicsFromCrest(float db) noexcept;
        static float depthFromProfile(const float be[30], float c, float hz) noexcept;
        static juce::String inferGenre(const float be[30], float l, float cr, float c, float hz) noexcept;
        static juce::String energyLabelFor(float s) noexcept;
        static juce::String brightnessLabelFor(float s) noexcept;
        static juce::String depthLabelFor(float s) noexcept;
        static juce::String dynamicsLabelFor(float s) noexcept;
        static juce::String widthLabelFor(float s) noexcept;
        static void regionEnergies(const float be[30], float re[6]) noexcept;
    };

} // namespace mixcoach
