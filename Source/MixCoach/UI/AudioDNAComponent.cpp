#include "AudioDNAComponent.h"
#include <cmath>
#include <algorithm>

namespace mixcoach {

    namespace {

        constexpr int kPad        = 4;
        constexpr int kHeaderH    = 16;
        constexpr int kLabelH     = 13;
        constexpr int kHealthBarH = 18;
        constexpr int kValueRowH  = 14;
        constexpr int kDiagRowH   = 13;
        constexpr int kSummaryH   = 14;
        constexpr int kColGap     = 3;

        // ─── Health colors ── using MixCoachTheme::success/warning/error ────────────
        // (Locally defined aliases removed — now using canonical theme colors)

        // ─── Health levels ─────────────────────────────────────────────────────────
        enum HealthLevel : uint8_t
        {
            kGood    = 0,
            kCaution = 1,
            kProblem = 2
        };

        struct BandHealth
        {
            HealthLevel crestHealth  = kGood;
            HealthLevel stereoHealth = kGood;
            HealthLevel composite    = kGood; // worst of all
            float crestScore         = 0.5f;  // 0-1 for display
            float stereoScore        = 0.5f;
            float compositeScore     = 0.5f;
            const char* diagnosis    = "—";
            float crestVal           = 0.0f;
            float stereoVal          = 0.0f;
            int trackCount           = 0;
        };

        // ─── Thresholds per band [6] ───────────────────────────────────────────────
        // [goodMin, goodMax, cautionMin, cautionMax] for crest (dB)
        static constexpr float kCrestThresh[6][4] = {
            {8.0f, 16.0f, 5.0f, 20.0f}, // Sub
            {8.0f, 14.0f, 5.0f, 18.0f}, // Bass
            {8.0f, 14.0f, 5.0f, 18.0f}, // LoMid
            {8.0f, 14.0f, 5.0f, 18.0f}, // HiMid
            {8.0f, 14.0f, 5.0f, 18.0f}, // Pres
            {8.0f, 14.0f, 5.0f, 18.0f}, // Air
        };
        // [goodMax, cautionMax, problemMax] for stereo width
        static constexpr float kWidthThresh[6][3] = {
            {0.25f, 0.40f, 1.00f}, // Sub  — sub should be mostly mono
            {0.35f, 0.55f, 1.00f}, // Bass — bass should be centered
            {0.50f, 0.70f, 1.00f}, // LoMid
            {0.50f, 0.70f, 1.00f}, // HiMid
            {0.60f, 0.80f, 1.00f}, // Pres
            {0.60f, 0.80f, 1.00f}, // Air
        };

        static constexpr const char* kDiagnosisLabels[6][3] = {
            // Good              Caution             Problem
            {"Good", "Controlled", "Over"},   // Sub
            {"Good", "Controlled", "Over"},   // Bass
            {"Good", "Lively", "Compressed"}, // LoMid
            {"Good", "Lively", "Compressed"}, // HiMid
            {"Good", "Bright", "Harsh"},      // Pres
            {"Good", "Airy", "Sibilant"},     // Air
        };

        // ─── Helpers ───────────────────────────────────────────────────────────────
        HealthLevel crestHealth(float crestDb, int band)
        {
            if (crestDb <= 0.0f) return kProblem;
            const auto& t = kCrestThresh[band];
            if (crestDb >= t[0] && crestDb <= t[1]) return kGood;
            if (crestDb >= t[2] && crestDb <= t[3]) return kCaution;
            return kProblem;
        }

        HealthLevel stereoHealth(float width, int band)
        {
            if (width <= 0.02f) return kProblem; // essentially mono
            const auto& t = kWidthThresh[band];
            if (width <= t[0]) return kGood;
            if (width <= t[1]) return kCaution;
            return kProblem;
        }

        float scoreFromHealth(HealthLevel h)
        {
            switch (h) {
                case kGood:
                    return 0.85f;
                case kCaution:
                    return 0.50f;
                case kProblem:
                    return 0.20f;
                default:
                    return 0.50f;
            }
        }

        juce::Colour healthColour(HealthLevel h, float alpha = 1.0f)
        {
            switch (h) {
                case kGood:
                    return MixCoachTheme::success().withAlpha(alpha);
                case kCaution:
                    return MixCoachTheme::warning().withAlpha(alpha);
                case kProblem:
                    return MixCoachTheme::error().withAlpha(alpha);
                default:
                    return MixCoachTheme::textMuted().withAlpha(alpha);
            }
        }

        juce::Colour healthBg(HealthLevel h)
        {
            switch (h) {
                case kGood:
                    return MixCoachTheme::success().withAlpha(0.15f);
                case kCaution:
                    return MixCoachTheme::warning().withAlpha(0.12f);
                case kProblem:
                    return MixCoachTheme::error().withAlpha(0.15f);
                default:
                    return MixCoachTheme::bgInput().withAlpha(0.5f);
            }
        }

        // ─── Compute per-band health from aggregated data ──────────────────────────
        void computeBandHealth(const AudioDNAData& data, BandHealth bands[6])
        {
            for (int b = 0; b < 6; ++b) {
                auto& bh      = bands[b];
                bh.trackCount = data.crestBandCounts[b];

                if (data.crestBandCounts[b] == 0 && data.stereoWidthCounts[b] == 0) {
                    bh.composite      = kCaution;
                    bh.compositeScore = 0.3f;
                    bh.diagnosis      = "N/A";
                    continue;
                }

                // Crest
                bh.crestVal    = data.avgCrestPerBand[b];
                bh.crestHealth = crestHealth(bh.crestVal, b);
                bh.crestScore  = scoreFromHealth(bh.crestHealth);
                if (bh.crestVal <= 0.0f) {
                    bh.crestHealth = kCaution;
                    bh.crestScore  = 0.3f;
                }

                // Stereo width
                bh.stereoVal    = data.avgStereoWidthPerBand[b];
                bh.stereoHealth = stereoHealth(bh.stereoVal, b);
                bh.stereoScore  = scoreFromHealth(bh.stereoHealth);

                // Composite = worst of the two
                bh.composite = static_cast<HealthLevel>(
                    (std::max)(static_cast<int>(bh.crestHealth), static_cast<int>(bh.stereoHealth)));
                bh.compositeScore = (bh.crestScore + bh.stereoScore) * 0.5f;

                // Diagnosis label: choose based on which metric is worse
                if (bh.composite == kGood) {
                    bh.diagnosis = kDiagnosisLabels[b][0];
                }
                else if (bh.composite == kCaution) {
                    bh.diagnosis = kDiagnosisLabels[b][1];
                }
                else {
                    bh.diagnosis = kDiagnosisLabels[b][2];
                }

                // Refine diagnosis based on specific issue
                // ═══ Order: check narrow/mono FIRST before "Wide" ═══════════════
                if (bh.stereoHealth == kProblem && bh.stereoVal < 0.05f) bh.diagnosis = "Mono";
                else if (bh.stereoHealth == kProblem && bh.crestHealth != kProblem)
                    bh.diagnosis = "Wide";
                else if (bh.crestHealth == kProblem && bh.crestVal < 5.0f)
                    bh.diagnosis = "Over";
                else if (bh.crestHealth == kProblem && bh.crestVal > 18.0f)
                    bh.diagnosis = "Wild";
            }
        }


    } // namespace

    // ═══════════════════════════════════════════════════════════════════════════
    // --- Mapa de frecuencias: 6 bandas --> [lowHz, highHz] ---
    static constexpr float kBandFreqRanges[6][2] = {
        {20.0f, 86.0f},     // Sub
        {86.0f, 301.0f},    // Bass
        {301.0f, 1076.0f},  // LoMid
        {1076.0f, 3532.0f}, // HiMid
        {3532.0f, 8355.0f}, // Pres
        {8355.0f, 16458.0f} // Air
    };

    void AudioDNAComponent::pushHealthDiagnostics()
    {
        if (diagnosticBridge_ == nullptr) return;

        // Throttle: push diagnostics max every 2 seconds
        int64_t nowMs = (int64_t)juce::Time::getMillisecondCounter();
        if (nowMs - lastDiagnosticPushMs_ < 2000) return;
        lastDiagnosticPushMs_ = nowMs;

        // Compute per-band health from current data
        BandHealth bands[6];
        computeBandHealth(data_, bands);

        std::vector<BandDiagnostic> diags;

        for (int b = 0; b < 6; ++b) {
            const auto& bh = bands[b];
            if (bh.trackCount == 0) continue;

            if (bh.composite == kGood) {
                BandDiagnostic d;
                d.lowFreqHz   = kBandFreqRanges[b][0];
                d.highFreqHz  = kBandFreqRanges[b][1];
                d.severity    = 0.1f;
                d.isCritical  = false;
                d.isPraise    = true;
                d.description = juce::String(kBandLabels[b]) + ": " + juce::String(bh.diagnosis) + " ("
                                + juce::String(bh.crestVal, 1) + "dB crest, " + juce::String(bh.stereoVal, 2)
                                + " width)";
                d.trackName   = "Health Grid";
                diags.push_back(d);
                continue;
            }

            bool needMarker = false;
            float severity  = 0.3f;
            bool isCritical = false;
            juce::String diagnosis;

            juce::String diagLabel = juce::String(bh.diagnosis).toLowerCase();

            if (diagLabel == "compressed" || diagLabel == "over" || diagLabel == "wild") {
                needMarker = true;
                severity   = (bh.crestHealth == kProblem) ? 0.7f : 0.4f;
                isCritical = (bh.crestHealth == kProblem && bh.crestVal < 5.0f);
                if (diagLabel == "wild") diagnosis = "Excessive dynamics in " + juce::String(kBandLabels[b]);
                else
                    diagnosis = "Crest issue in " + juce::String(kBandLabels[b]) + ": " + juce::String(bh.diagnosis);
            }
            else if (diagLabel == "wide") {
                needMarker = true;
                severity   = 0.6f;
                isCritical = (b < 2);
                diagnosis  = juce::String(kBandLabels[b]) + " too wide (" + juce::String(bh.stereoVal, 2) + ")";
            }
            else if (diagLabel == "mono") {
                needMarker = true;
                severity   = 0.5f;
                isCritical = (b >= 3);
                diagnosis  = juce::String(kBandLabels[b]) + " nearly mono (" + juce::String(bh.stereoVal, 2) + ")";
            }
            else if (diagLabel == "harsh") {
                needMarker = true;
                severity   = 0.8f;
                isCritical = true;
                diagnosis  = "Harsh presence in " + juce::String(kBandLabels[b]);
            }
            else if (diagLabel == "sibilant") {
                needMarker = true;
                severity   = 0.7f;
                isCritical = true;
                diagnosis  = "Sibilant air region in " + juce::String(kBandLabels[b]);
            }
            else if (diagLabel == "lively" || diagLabel == "airy") {
                needMarker = true;
                severity   = 0.25f;
                isCritical = false;
                diagnosis  = juce::String(kBandLabels[b]) + ": " + juce::String(bh.diagnosis);
            }
            else if (diagLabel == "bright") {
                needMarker = true;
                severity   = 0.4f;
                isCritical = false;
                diagnosis  = juce::String(kBandLabels[b]) + " bright";
            }
            else if (diagLabel == "controlled") {
                if (bh.composite == kProblem) {
                    needMarker = true;
                    severity   = 0.5f;
                    diagnosis  = juce::String(kBandLabels[b]) + " needs attention";
                }
            }

            if (!needMarker) continue;

            BandDiagnostic d;
            d.lowFreqHz   = kBandFreqRanges[b][0];
            d.highFreqHz  = kBandFreqRanges[b][1];
            d.severity    = severity;
            d.isCritical  = isCritical;
            d.isPraise    = false;
            d.description = diagnosis;
            d.trackName   = "Health Grid";
            diags.push_back(d);
        }

        // Merge with existing diagnostics to preserve coach markers
        // Remove old health grid entries, keep everything else
        auto existing = diagnosticBridge_->getDiagnostics();
        existing.erase(std::remove_if(existing.begin(),
                                      existing.end(),
                                      [](const BandDiagnostic& d) { return d.trackName == "Health Grid"; }),
                       existing.end());
        existing.insert(existing.end(), diags.begin(), diags.end());
        diagnosticBridge_->setDiagnostics(existing);
    }

    //  AudioDNAComponent — Health Grid
    // ═══════════════════════════════════════════════════════════════════════════
    AudioDNAComponent::AudioDNAComponent() {}

    void AudioDNAComponent::update(SlotRegistry& registry, SharedData& sharedData)
    {
        AudioDNAData newData;

        registry.forEachActive([&](const SlotInfo& info) {
            auto telem = sharedData.getTrackAudioResult(info.slotIndex);
            if (telem.timestampUs == 0) return;

            juce::String name = juce::String(info.trackName).trim();
            if (name.isEmpty()) name = "Track " + juce::String(info.slotIndex + 1);

            newData.activeTrackCount++;
            newData.avgTransientRatio += telem.transientRatio;
            if (telem.transientRatio > 2.0f) newData.highTransientCount++;

            // Per-track data (first kMaxDisplayTracks)
            if (newData.tracks.size() < AudioDNAData::kMaxDisplayTracks) {
                AudioDNAData::TrackDNA td;
                td.name           = name;
                td.transientRatio = telem.transientRatio;
                for (int b = 0; b < 6; ++b) {
                    td.crestPerBand[b]       = telem.crestPerBand[b];
                    td.stereoWidthPerBand[b] = telem.stereoWidthPerBand[b];
                }
                newData.tracks.push_back(td);
            }

            // Aggregate crest per band
            for (int b = 0; b < 6; ++b) {
                if (telem.crestPerBand[b] > 0.5f) {
                    newData.avgCrestPerBand[b] += telem.crestPerBand[b];
                    newData.crestBandCounts[b]++;
                }
                if (telem.stereoWidthPerBand[b] > 0.01f) {
                    newData.avgStereoWidthPerBand[b] += telem.stereoWidthPerBand[b];
                    newData.stereoWidthCounts[b]++;
                }
            }

            // Wide stereo flags
            if (telem.stereoWidthPerBand[0] > 0.5f) newData.wideSubCount++;
            if (telem.stereoWidthPerBand[1] > 0.6f) newData.wideBassCount++;
            if (telem.stereoWidthPerBand[4] > 0.8f || telem.stereoWidthPerBand[5] > 0.8f) newData.widePresAirCount++;
        });

        // Compute averages
        if (newData.activeTrackCount > 0) newData.avgTransientRatio /= newData.activeTrackCount;

        for (int b = 0; b < 6; ++b) {
            if (newData.crestBandCounts[b] > 0) newData.avgCrestPerBand[b] /= newData.crestBandCounts[b];
            if (newData.stereoWidthCounts[b] > 0) newData.avgStereoWidthPerBand[b] /= newData.stereoWidthCounts[b];
        }

        data_         = newData;
        bgCacheValid_ = false;
        repaint();
    }

    void AudioDNAComponent::resized()
    {
        bgCacheValid_ = false;
    }

    void AudioDNAComponent::rebuildBgCache()
    {
        const int w = getWidth();
        const int h = getHeight();
        if (w < 8 || h < 8) return;

        bgCache_ = juce::Image(juce::Image::ARGB, w, h, true);
        bgCache_.clear(bgCache_.getBounds());
        juce::Graphics cg(bgCache_);

        cg.setColour(MixCoachTheme::bgDarker().withAlpha(0.3f));
        cg.fillRoundedRectangle(juce::Rectangle<float>(0, 0, (float)w, (float)h), 5.0f);

        bgCacheValid_ = true;
    }

    void AudioDNAComponent::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds();

        if (!bgCacheValid_) rebuildBgCache();
        if (bgCacheValid_) g.drawImageAt(bgCache_, 0, 0);

        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), 5.0f);

        auto area = bounds.reduced(kPad, kPad);

        // ─── Header ──────────────────────────────────────────────────────────
        auto headerArea = area.removeFromTop(kHeaderH);
        drawHeader(g, headerArea);

        if (!data_.hasData()) {
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
            g.drawText("Waiting for track data...", area, juce::Justification::centred);
            return;
        }

        area.removeFromTop(2);

        // ─── Compute per-band health ════════════════════════════════════════
        BandHealth bands[6];
        computeBandHealth(data_, bands);
        pushHealthDiagnostics();

        // ─── Health Grid Layout ─────────────────────────────────────────────
        auto gridArea = area;

        // Dimensions
        int totalColGaps = kColGap * 5;
        int colW         = (gridArea.getWidth() - totalColGaps) / 6;
        if (colW < 30) colW = 30; // minimum

        int rowY = gridArea.getY();

        // ─── Row 1: Band Labels ──────────────────────────────────────────
        {
            int y = rowY;
            rowY += kLabelH;
            for (int b = 0; b < 6; ++b) {
                auto cell = juce::Rectangle<int>(gridArea.getX() + b * (colW + kColGap), y, colW, kLabelH);
                g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
                g.setColour(MixCoachTheme::accentGlow().withAlpha(0.9f));
                g.drawText(juce::String(kBandLabels[b]), cell, juce::Justification::centred);
            }
        }

        // ─── Row 2: Health Bar (colored pill with glow) ──────────────────
        {
            int y = rowY;
            rowY += kHealthBarH + 1;
            for (int b = 0; b < 6; ++b) {
                auto cell = juce::Rectangle<int>(gridArea.getX() + b * (colW + kColGap), y, colW, kHealthBarH);

                auto cellF     = cell.toFloat().reduced(1.0f, 2.0f);
                const auto& bh = bands[b];

                // Background pill
                g.setColour(healthBg(bh.composite));
                g.fillRoundedRectangle(cellF, 4.0f);

                // Fill (proportional to compositeScore)
                float fillPct = bh.compositeScore;
                if (fillPct > 0.01f) {
                    auto fill = cellF.withWidth(juce::jmax(4.0f, cellF.getWidth() * fillPct));
                    g.setColour(healthColour(bh.composite, 0.7f));
                    g.fillRoundedRectangle(fill, 4.0f);
                }

                // Glow border
                g.setColour(healthColour(bh.composite, 0.3f));
                g.drawRoundedRectangle(cellF, 4.0f, 0.8f);

                // Score text overlay
                g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
                g.setColour(healthColour(bh.composite, 0.9f));
                juce::String scoreStr = juce::String((int)(bh.compositeScore * 100.0f)) + "%";
                g.drawText(scoreStr, cell, juce::Justification::centred);
            }
        }

        // ─── Row 3: Crest Value ──────────────────────────────────────────
        {
            int y = rowY;
            rowY += kValueRowH + 1;
            for (int b = 0; b < 6; ++b) {
                auto cell      = juce::Rectangle<int>(gridArea.getX() + b * (colW + kColGap), y, colW, kValueRowH);
                const auto& bh = bands[b];

                // Mini icon
                auto iconArea = cell.removeFromLeft(12);
                auto dotR     = 3.5f;
                g.setColour(healthColour(bh.crestHealth, 0.9f));
                g.fillEllipse(iconArea.getCentreX() - dotR, iconArea.getCentreY() - dotR, dotR * 2.0f, dotR * 2.0f);

                // Crest value
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
                g.setColour(MixCoachTheme::textDim().withAlpha(0.85f));
                juce::String crestStr = bh.crestVal > 0.0f ? juce::String(bh.crestVal, 1) + "dB" : "--";
                g.drawText(crestStr, cell, juce::Justification::centredLeft);
            }
        }

        // ─── Row 4: Stereo Width bar ────────────────────────────────────
        {
            int y = rowY;
            rowY += kValueRowH + 1;
            for (int b = 0; b < 6; ++b) {
                auto cell      = juce::Rectangle<int>(gridArea.getX() + b * (colW + kColGap), y, colW, kValueRowH);
                const auto& bh = bands[b];

                // Small width bar
                auto barArea = cell.reduced(2, 3);
                g.setColour(MixCoachTheme::bgInput().withAlpha(0.5f));
                g.fillRoundedRectangle(barArea.toFloat(), 2.0f);

                float wNorm = juce::jlimit(0.0f, 1.0f, bh.stereoVal);
                if (wNorm > 0.01f) {
                    auto fill = barArea.withWidth(juce::jmax(2.0f, barArea.getWidth() * wNorm));
                    g.setColour(healthColour(bh.stereoHealth, 0.75f));
                    g.fillRoundedRectangle(fill.toFloat(), 2.0f);
                }

                // Value label
                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeNano)));
                g.setColour(MixCoachTheme::textMuted().withAlpha(0.6f));
                juce::String wStr = bh.stereoVal > 0.01f ? juce::String(bh.stereoVal, 2) : "--";
                g.drawText(wStr, cell, juce::Justification::centredRight);
            }
        }

        // ─── Row 5: Diagnosis label ─────────────────────────────────────
        {
            int y = rowY;
            rowY += kDiagRowH;
            for (int b = 0; b < 6; ++b) {
                auto cell      = juce::Rectangle<int>(gridArea.getX() + b * (colW + kColGap), y, colW, kDiagRowH);
                const auto& bh = bands[b];

                auto cellF = cell.toFloat().reduced(1.0f, 1.0f);
                g.setColour(healthBg(bh.composite));
                g.fillRoundedRectangle(cellF, 3.0f);

                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)).boldened());
                g.setColour(healthColour(bh.composite, 0.9f));
                g.drawText(juce::String(bh.diagnosis), cell, juce::Justification::centred);
            }
        }

        // ═══ Layout overflow guard ═══════════════════════════════════════
        if (rowY <= gridArea.getY()) return;

        // ─── Summary Row ────────────────────────────────────────────────
        {
            int remainingH = gridArea.getBottom() - rowY;
            if (remainingH > kSummaryH) {
                int y            = gridArea.getBottom() - kSummaryH;
                auto summaryArea = juce::Rectangle<int>(gridArea.getX(), y, gridArea.getWidth(), kSummaryH);

                g.setColour(MixCoachTheme::divider().withAlpha(0.15f));
                g.drawHorizontalLine(y, (float)gridArea.getX(), (float)gridArea.getRight());

                g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeMicro)));
                g.setColour(MixCoachTheme::textDim().withAlpha(0.7f));

                juce::String summary = juce::String(data_.activeTrackCount) + " tracks";
                if (data_.highTransientCount > 0)
                    summary += "  |  " + juce::String(data_.highTransientCount) + " strong transients";

                // Count problem bands
                int problemBands = 0;
                for (int b = 0; b < 6; ++b)
                    if (bands[b].composite == kProblem) problemBands++;
                if (problemBands > 0) summary += "  |  " + juce::String(problemBands) + " bands need attention";

                // Wide stereo summary
                juce::String wideSummary;
                if (data_.wideSubCount > 0) wideSummary += "Sub ";
                if (data_.wideBassCount > 0) wideSummary += "Bass ";
                if (data_.widePresAirCount > 0) wideSummary += "Top ";

                if (wideSummary.isNotEmpty()) summary += "  |  Wide: " + wideSummary.trim();

                g.drawText(summary, summaryArea, juce::Justification::centredLeft);
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  HEADER
    // ═══════════════════════════════════════════════════════════════════════════
    void AudioDNAComponent::drawHeader(juce::Graphics& g, juce::Rectangle<int> area)
    {
        g.setFont(juce::Font(juce::FontOptions(10.0f)).boldened());
        g.setColour(MixCoachTheme::accentGlow());
        g.drawText(juce::CharPointer_UTF8("AUDIO DNA — HEALTH GRID"), area, juce::Justification::centredLeft);

        auto rightArea = area.removeFromRight(area.getWidth() / 2);
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)));
        g.setColour(MixCoachTheme::textDim().withAlpha(0.7f));
        g.drawText(juce::String(data_.activeTrackCount) + " tracks", rightArea, juce::Justification::centredRight);

        auto b = area.toFloat();
        g.setColour(MixCoachTheme::divider().withAlpha(0.3f));
        g.drawHorizontalLine((int)(b.getBottom()), b.getX(), b.getRight());

        juce::ColourGradient glow(MixCoachTheme::accent().withAlpha(0.12f),
                                  b.getX() + b.getWidth() * 0.3f,
                                  0.0f,
                                  MixCoachTheme::accent().withAlpha(0.0f),
                                  b.getX() + b.getWidth(),
                                  0.0f,
                                  false);
        g.setGradientFill(glow);
        g.fillRect(b.getX(), b.getBottom(), b.getWidth(), 1.0f);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  No longer used — kept for API compatibility
    // ═══════════════════════════════════════════════════════════════════════════
    juce::Colour AudioDNAComponent::bandColour(int bandIndex, float value, float maxVal)
    {
        juce::ignoreUnused(bandIndex);
        float t = maxVal > 0.0f ? juce::jlimit(0.0f, 1.0f, value / maxVal) : 0.0f;
        return healthColour(t < 0.35f ? kProblem : (t < 0.65f ? kCaution : kGood), 1.0f);
    }

} // namespace mixcoach
