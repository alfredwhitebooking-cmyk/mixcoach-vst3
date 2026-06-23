#include "AnalyzersPanelComponent.h"
#include "AnalyzersPanelDrawing.h"
#include "../../Common/types/Constants.h"
#include "../../Common/memory/SharedData.h"
#include "../../Common/memory/SlotRegistry.h"
#include "../audio/AudioAnalyzer.h"
#include <juce_graphics/juce_graphics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

namespace mixcoach {

    namespace {
        constexpr int kPanelGap = MixCoachTheme::spacingSM + 1; // 7
        constexpr int kOuterPad = MixCoachTheme::spacingXS;     // 4
    } // namespace

    // ═══════════════════════════════════════════════════════════════════════════
    //  PHASESCOPEPANEL
    // ═══════════════════════════════════════════════════════════════════════════
    PhaseScopePanel::PhaseScopePanel()
    {
        addAndMakeVisible(vectorscope_);
    }

    PhaseScopePanel::~PhaseScopePanel()
    {
        if (diagnosticBridge_ != nullptr) diagnosticBridge_->removeChangeListener(this);
    }

    void PhaseScopePanel::resized()
    {
        auto area       = getLocalBounds().reduced(MixCoachTheme::spacingSM / 2, MixCoachTheme::spacingXXS);
        auto headerArea = area.removeFromTop(14);
        juce::ignoreUnused(headerArea);
        area.removeFromTop(22);    // correlation area
        area.removeFromBottom(36); // metrics area
        vectorscope_.setBounds(area.reduced(MixCoachTheme::spacingXXS, MixCoachTheme::spacingXXS));
    }

    void PhaseScopePanel::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds();
        MixCoachTheme::fillGlassPanel(g, bounds.toFloat(), 5.0f);

        auto area       = bounds.reduced(3, 2);
        auto headerArea = area.removeFromTop(14);
        drawAnalyzerHeader(g, headerArea, "PHASE SCOPE");

        auto corrArea = area.removeFromTop(22).reduced(0, 2);
        float corr    = juce::jlimit(-1.0f, 1.0f, correlation_.getCurrent());

        auto corrBg = corrArea.toFloat();
        g.setColour(MixCoachTheme::bgInput());
        g.fillRoundedRectangle(corrBg, 2.0f);

        juce::ColourGradient corrGrad(MixCoachTheme::error().withAlpha(0.12f),
                                      corrBg.getX(),
                                      corrBg.getCentreY(),
                                      MixCoachTheme::success().withAlpha(0.12f),
                                      corrBg.getRight(),
                                      corrBg.getCentreY(),
                                      false);
        corrGrad.addColour(0.35f, MixCoachTheme::warning().withAlpha(0.08f));
        corrGrad.addColour(0.65f, MixCoachTheme::success().withAlpha(0.12f));
        g.setGradientFill(corrGrad);
        g.fillRoundedRectangle(corrBg, 2.0f);

        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
        g.setColour(MixCoachTheme::textDim().withAlpha(0.6f));
        auto labelArea = corrArea.toFloat();
        g.drawText("-1", labelArea.removeFromLeft(16), juce::Justification::centredLeft);
        g.drawText("0", labelArea.removeFromLeft(12), juce::Justification::centred);
        g.drawText("+1", labelArea.removeFromRight(16), juce::Justification::centredRight);

        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawVerticalLine(corrBg.getCentreX(), corrBg.getY() + 2, corrBg.getBottom() - 2);

        float cNorm          = (corr + 1.0f) * 0.5f;
        float mx             = corrBg.getX() + cNorm * corrBg.getWidth();
        float my             = corrBg.getCentreY();
        juce::Colour corrCol = (corr < -0.3f)  ? MixCoachTheme::error()
                               : (corr < 0.3f) ? MixCoachTheme::warning()
                                               : MixCoachTheme::success();

        g.setColour(corrCol.withAlpha(0.15f));
        g.fillEllipse(mx - 7, my - 7, 14, 14);

        juce::Path diamond;
        diamond.addTriangle(mx, my - 6, mx - 4, my, mx, my + 6);
        diamond.addTriangle(mx, my - 6, mx + 4, my, mx, my + 6);
        g.setColour(corrCol);
        g.fillPath(diamond);
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.strokePath(diamond, juce::PathStrokeType(0.5f));

        auto vArea = corrArea.removeFromRight(65);
        g.setFont(juce::Font(juce::FontOptions(13.0f)).boldened());
        g.setColour(corrCol);
        juce::String corrStr = (corr >= 0.0f ? "+" : "") + juce::String(corr, 2);
        g.drawText(corrStr, vArea, juce::Justification::centredRight);

        auto metricsArea = area.removeFromBottom(36).reduced(2, 0);

        struct Metric
        {
            const char* label;
            float value;
            juce::Colour col;
        };

        Metric metrics[] = {
            {"PEAK", currentPeak_, MixCoachTheme::meterYellow()},
            {"RMS", currentRms_, MixCoachTheme::accentCyan()},
        };
        int mW = metricsArea.getWidth() / 2;
        for (int i = 0; i < 2; ++i) {
            auto mArea = metricsArea.removeFromLeft(mW).reduced(1, 0);
            if (i > 0) {
                g.setColour(MixCoachTheme::divider().withAlpha(0.2f));
                g.drawVerticalLine(mArea.getX(), (float)mArea.getY(), (float)mArea.getBottom());
            }
            g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeExtraTiny)).boldened());
            g.setColour(metrics[i].col.withAlpha(0.75f));
            g.drawText(metrics[i].label, mArea.removeFromTop(10), juce::Justification::centred);
            g.setFont(juce::Font(juce::FontOptions(12.0f)).boldened());
            g.setColour(MixCoachTheme::textBright());
            juce::String vStr = metrics[i].value > -60.0f ? juce::String(metrics[i].value, 2) : "--.-";
            g.drawText(vStr, mArea, juce::Justification::centred);
        }
    }

    void PhaseScopePanel::pushCrest(float peak, float rms)
    {
        currentPeak_ = peak;
        currentRms_  = rms;
    }

    void PhaseScopePanel::setDiagnosticBridge(DiagnosticBridge* bridge)
    {
        if (diagnosticBridge_ != nullptr) diagnosticBridge_->removeChangeListener(this);

        diagnosticBridge_ = bridge;

        if (bridge != nullptr) bridge->addChangeListener(this);
    }

    void PhaseScopePanel::setPhaseDiagnostics(const std::vector<PhaseDiagnostic>& diagnostics)
    {
        if (diagnostics.empty()) {
            vectorscope_.setPhaseDiagnostic(nullptr);
            return;
        }
        const PhaseDiagnostic* worst = &diagnostics[0];
        for (const auto& d : diagnostics) {
            if (d.severity > worst->severity) worst = &d;
        }
        vectorscope_.setPhaseDiagnostic(worst);
    }

    void PhaseScopePanel::changeListenerCallback(juce::ChangeBroadcaster* source)
    {
        auto* bridge = dynamic_cast<DiagnosticBridge*>(source);
        if (bridge == nullptr) return;

        auto phaseDiags = bridge->getPhaseDiagnostics();
        setPhaseDiagnostics(phaseDiags);
    }

    bool PhaseScopePanel::advanceVisuals(double sr, bool allowRepaint)
    {
        bool dirty = correlation_.advance(sr);
        dirty |= vectorscope_.advanceFrame(sr, false);
        if (dirty && allowRepaint) repaint();
        return dirty;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  ANALYZERSPANELCOMPONENT — Tab 2 main layout container
    // ═══════════════════════════════════════════════════════════════════════════
    AnalyzersPanelComponent::AnalyzersPanelComponent(AudioAnalyzer& audioAnalyzer) :
        audioAnalyzer_(audioAnalyzer)
    {
        addAndMakeVisible(meterPanel_);
        addAndMakeVisible(spectrograph_);
        addAndMakeVisible(phaseScope_);
        addAndMakeVisible(vuMeters_);
        addAndMakeVisible(crestPanel_);
        addAndMakeVisible(stereoWidthMeter_);
        addAndMakeVisible(audioDNA_);

        refToggle_ = std::make_unique<RefToggle>();
        addAndMakeVisible(refToggle_.get());
        refToggle_->onClick = [this]() {
            auto& spec = spectrograph_;
            spec.setReferenceEnabled(!spec.isReferenceEnabled());
            refToggle_->toggled = spec.isReferenceEnabled();
            refToggle_->repaint();
        };
    }

    void AnalyzersPanelComponent::setDiagnosticBridge(DiagnosticBridge* bridge)
    {
        spectrograph_.setDiagnosticBridge(bridge);
        phaseScope_.setDiagnosticBridge(bridge);
        audioDNA_.setDiagnosticBridge(bridge);
    }

    void AnalyzersPanelComponent::resized()
    {
        try {
            auto area = getLocalBounds().reduced(kOuterPad);

            int topH    = area.getHeight() * 50 / 100;
            auto topRow = area.removeFromTop(topH);
            area.removeFromTop(kPanelGap);

            int meterW = topRow.getWidth() * 28 / 100;
            meterPanel_.setBounds(topRow.removeFromLeft(meterW));
            topRow.removeFromLeft(kPanelGap);
            spectrograph_.setBounds(topRow);

            // Reference toggle: flota sobre la esquina superior derecha del spectrograph
            {
                auto specBounds = spectrograph_.getBounds();
                auto toggleArea = specBounds.removeFromRight(60).removeFromTop(18);
                refToggle_->setBounds(toggleArea.reduced(MixCoachTheme::spacingXXS, MixCoachTheme::spacingXXS / 2));
            }

            // Toggle bar: paneles opcionales (DNA, WIDTH, CREST)
            toggleBtns_.clear();
            {
                auto toggleBar = area.removeFromTop(18).reduced(MixCoachTheme::spacingXS, MixCoachTheme::spacingXXS);
                int tx         = toggleBar.getX();
                int ty         = toggleBar.getY();
                int th         = toggleBar.getHeight();

                toggleBtns_.push_back({{tx, ty, 36, th}, "DNA", &showDna_});
                toggleBtns_.push_back({{tx + 38, ty, 42, th}, "WIDTH", &showWidth_});
                toggleBtns_.push_back({{tx + 82, ty, 40, th}, "CREST", &showCrest_});
                toggleBtns_.push_back({{tx + 124, ty, 28, th}, "AI", &showAi_});
            }
            area.removeFromTop(kPanelGap);

            // Bottom row: layout condicional
            auto botRow   = area;
            const int gap = kPanelGap;

            // Phase Scope (siempre visible)
            int phasePct = (showDna_ || showWidth_ || showCrest_) ? 18 : 28;
            int phaseW   = botRow.getWidth() * phasePct / 100;
            phaseScope_.setBounds(botRow.removeFromLeft(phaseW));
            botRow.removeFromLeft(gap);

            // Stereo Width (opcional)
            if (showWidth_) {
                int w = botRow.getWidth() * 15 / 100;
                stereoWidthMeter_.setBounds(botRow.removeFromLeft(w));
                botRow.removeFromLeft(gap);
                stereoWidthMeter_.setVisible(true);
            }
            else {
                stereoWidthMeter_.setVisible(false);
            }

            // Audio DNA (opcional)
            if (showDna_) {
                int w = botRow.getWidth() * 30 / 100;
                audioDNA_.setBounds(botRow.removeFromLeft(w));
                botRow.removeFromLeft(gap);
                audioDNA_.setVisible(true);
            }
            else {
                audioDNA_.setVisible(false);
            }

            // Crest (opcional)
            if (showCrest_) {
                int w = botRow.getWidth() * 15 / 100;
                crestPanel_.setBounds(botRow.removeFromLeft(w));
                botRow.removeFromLeft(gap);
                crestPanel_.setVisible(true);
            }
            else {
                crestPanel_.setVisible(false);
            }

            // VU Meters (siempre visible, toma el resto del espacio)
            vuMeters_.setBounds(botRow);

            bgCacheValid_ = false;
        }
        catch (const std::exception& e) {
            juce::Logger::outputDebugString("[AnalyzersPanel] Exception in resized: " + juce::String(e.what()));
        }
    }

    void AnalyzersPanelComponent::mouseDown(const juce::MouseEvent& e)
    {
        for (auto& btn : toggleBtns_) {
            if (btn.bounds.contains(e.getPosition())) {
                *btn.active = !(*btn.active);

                if (btn.label == "AI" && coachEngine_) coachEngine_->setProactiveAnalysisEnabled(*btn.active);

                resized();
                repaint();
                return;
            }
        }
    }

    void AnalyzersPanelComponent::rebuildBgCache()
    {
        const int w = getWidth();
        const int h = getHeight();
        if (w < 8 || h < 8) return;

        bgCache_ = juce::Image(juce::Image::ARGB, w, h, true);
        bgCache_.clear(bgCache_.getBounds());

        juce::Graphics cg(bgCache_);

        {
            juce::ColourGradient bgGrad(MixCoachTheme::gradientDark(),
                                        (float)w * 0.5f,
                                        0.0f,
                                        MixCoachTheme::gradientMid(),
                                        (float)w * 0.5f,
                                        (float)h,
                                        false);
            cg.setGradientFill(bgGrad);
            cg.fillAll(MixCoachTheme::bgCanvas());
            cg.fillRect(bgCache_.getBounds().toFloat());
        }

        cg.setColour(juce::Colour(0x03FFFFFF));
        for (int gy = 0; gy < h; gy += 32)
            for (int gx = 0; gx < w; gx += 32) cg.fillRect(gx, gy, 1, 1);

        int topH = h * 50 / 100;
        cg.setColour(MixCoachTheme::accent().withAlpha(0.035f));
        cg.drawHorizontalLine(topH, (float)kOuterPad, (float)w - kOuterPad);

        bgCacheValid_ = true;
    }

    void AnalyzersPanelComponent::paint(juce::Graphics& g)
    {
        if (!bgCacheValid_) rebuildBgCache();

        if (bgCacheValid_) g.drawImageAt(bgCache_, 0, 0);

        // Draw toggle pills
        for (const auto& btn : toggleBtns_) {
            bool active = *btn.active;
            auto b      = btn.bounds.toFloat();
            auto bgCol  = active ? juce::Colour(0x44A855F7) : juce::Colour(0x1A888888);
            auto fgCol  = active ? juce::Colour(0xCCA855F7) : juce::Colour(0x55999999);
            g.setColour(bgCol);
            g.fillRoundedRectangle(b, 4.0f);
            g.setColour(fgCol);
            g.drawRoundedRectangle(b, 4.0f, 0.8f);
            g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
            g.drawText(btn.label, b, juce::Justification::centred);
        }
    }

    void AnalyzersPanelComponent::feedFromAudioAnalyzer(bool includeSpectrograph)
    {
        if (includeSpectrograph) refreshSpectrograph();

        const auto& master   = audioAnalyzer_.getMasterAnalysis();
        const auto& loudness = audioAnalyzer_.getLoudness();
        juce::ignoreUnused(loudness);

        meterPanel_.updateData(audioAnalyzer_);

        float correlation = master.getCorrelation();
        phaseScope_.setCorrelation(correlation);
        phaseScope_.getVectorscope().setDisplayCorrelation(correlation);
        audioAnalyzer_.flushSampleBufferToVectorscope(phaseScope_.getVectorscope());
        phaseScope_.pushCrest(master.getPeak(), master.getRMS());

        float leftRms  = audioAnalyzer_.getLeftAnalysis().getRMS();
        float rightRms = audioAnalyzer_.getRightAnalysis().getRMS();
        float leftLin  = juce::Decibels::decibelsToGain(leftRms);
        float rightLin = juce::Decibels::decibelsToGain(rightRms);
        float midLin   = (leftLin + rightLin) * 0.5f;
        float sideLin  = (leftLin - rightLin) * 0.5f;
        float midRms   = juce::Decibels::gainToDecibels(midLin);
        float sideRms  = juce::Decibels::gainToDecibels(sideLin);
        vuMeters_.setLevels(leftRms, rightRms, midRms, sideRms);

        crestPanel_.setValues(master.getPeak(), master.getRMS());

        // Stereo Width Meter (solo si visible)
        float avgWidth = audioAnalyzer_.getAvgStereoWidth();
        if (showWidth_ || showDna_) {
            stereoWidthMeter_.setAvgWidth(avgWidth);
            phaseScope_.getVectorscope().setStereoWidth(avgWidth);
        }
    }

    void AnalyzersPanelComponent::refreshSpectrograph()
    {
        uint32_t now = juce::Time::getMillisecondCounter();
        if (now - lastSpectrumUpdateMs_ < 15) return;
        lastSpectrumUpdateMs_ = now;

        const auto& master = audioAnalyzer_.getMasterAnalysis();

        float masterPeak = master.getPeak();
        if (masterPeak <= -60.0f) {
            spectrograph_.resetSpectrum();
            return;
        }

        int64_t lastUpdateUs = master.getLastUpdateTime();
        if (lastUpdateUs > 0) {
            uint32_t lastUpdateMs = static_cast<uint32_t>(lastUpdateUs / 1000);
            if (now - lastUpdateMs > 500) {
                spectrograph_.resetSpectrum();
                return;
            }
        }

        std::array<float, 8192> hiResSnapshot{};
        const int copiedBins = master.copyHiResSpectrum(hiResSnapshot.data(), static_cast<int>(hiResSnapshot.size()));
        if (copiedBins > 0) {
            spectrograph_.updateSpectrum(hiResSnapshot.data(), copiedBins);
        }
        else {
            const float* spectrum = master.getSpectrum();
            if (spectrum != nullptr) spectrograph_.updateSpectrum(spectrum, kNumSpectrumBins);
        }
    }

    void AnalyzersPanelComponent::updateAnalyzers(double sampleRate)
    {
        try {
            spectrograph_.setSampleRate(sampleRate);
            feedFromAudioAnalyzer(true);

            if (coachEngine_ != nullptr && coachEngine_->hasReferenceAudio()) {
                updateReferenceCurve(coachEngine_->getReferenceAnalyzer());
            }
        }
        catch (const std::exception& e) {
            juce::Logger::outputDebugString("[AnalyzersPanel] updateAnalyzers exception: " + juce::String(e.what()));
        }
    }

    void AnalyzersPanelComponent::fastUpdateMeters()
    {
        try {
            feedFromAudioAnalyzer(true);
        }
        catch (const std::exception& e) {
            juce::Logger::outputDebugString("[AnalyzersPanel] fastUpdateMeters exception: " + juce::String(e.what()));
        }
    }

    void AnalyzersPanelComponent::updateAudioDNA(SlotRegistry& registry, SharedData& sharedData)
    {
        audioDNA_.update(registry, sharedData);

        // Computar stereo width per-band desde tracks activos
        float perBandSum[StereoWidthMeter::kNumBands] = {0.0f};
        int perBandCount[StereoWidthMeter::kNumBands] = {0};

        registry.forEachActive([&](const SlotInfo& info) {
            auto result = sharedData.getTrackAudioResult(info.slotIndex);
            for (int b = 0; b < StereoWidthMeter::kNumBands; ++b) {
                if (result.stereoWidthPerBand[b] > 0.01f) {
                    perBandSum[b] += result.stereoWidthPerBand[b];
                    perBandCount[b]++;
                }
            }
        });

        float perBandAvg[StereoWidthMeter::kNumBands] = {0.0f};
        bool hasPerBand                               = false;
        for (int b = 0; b < StereoWidthMeter::kNumBands; ++b) {
            if (perBandCount[b] > 0) {
                perBandAvg[b] = perBandSum[b] / (float)perBandCount[b];
                hasPerBand    = true;
            }
        }

        if (hasPerBand) stereoWidthMeter_.setPerBandWidth(perBandAvg);
    }

    void AnalyzersPanelComponent::updateReferenceCurve(const ReferenceAnalyzer& refAnalyzer)
    {
        if (!refAnalyzer.hasReference()) {
            spectrograph_.computeDefaultReferenceCurve();
            return;
        }

        const auto& analysis  = refAnalyzer.getAnalysis();
        const float* spectrum = analysis.getSpectrum();
        if (spectrum == nullptr) {
            spectrograph_.computeDefaultReferenceCurve();
            return;
        }

        constexpr int kNumRtaBands  = 60;
        constexpr float kRefMinFreq = 20.0f;
        constexpr float kRefMaxFreq = 20000.0f;
        constexpr int kNumSpecBins  = kNumSpectrumBins;

        float rtaCurve[kNumRtaBands] = {0.0f};
        const double sampleRate      = (double)refAnalyzer.getSampleRate();
        const double binFreqStep     = (sampleRate > 0.0) ? sampleRate / (2.0 * kNumSpecBins) : 44100.0 / 1024.0;

        for (int b = 0; b < kNumRtaBands; ++b) {
            const float t        = (kNumRtaBands <= 1) ? 0.0f : (float)b / (float)(kNumRtaBands - 1);
            const float centerHz = kRefMinFreq * std::pow(kRefMaxFreq / kRefMinFreq, t);

            const float lowHz  = (b == 0) ? kRefMinFreq : centerHz * 0.89f;
            const float highHz = (b == kNumRtaBands - 1) ? kRefMaxFreq : centerHz * 1.12f;

            int lowBin  = juce::jmax(1, (int)(lowHz / binFreqStep));
            int highBin = juce::jmin(kNumSpecBins - 1, (int)(highHz / binFreqStep));

            double sumMag = 0.0;
            int count     = 0;
            for (int bin = lowBin; bin <= highBin; ++bin) {
                float mag = spectrum[bin];
                if (mag > 1e-12f) {
                    float db = 20.0f * std::log10(mag);
                    sumMag += juce::jmax(-60.0f, db);
                    ++count;
                }
            }

            if (count > 0) {
                float avgDb = (float)(sumMag / count);
                rtaCurve[b] = juce::jlimit(0.0f, 1.0f, (avgDb + 40.0f) / 40.0f);
            }
            else {
                rtaCurve[b] = 0.0f;
            }
        }

        float avgLevel = 0.0f;
        for (int b = 0; b < kNumRtaBands; ++b) avgLevel += rtaCurve[b];
        avgLevel /= (float)kNumRtaBands;

        if (avgLevel > 0.01f) {
            float targetCenter = 0.35f;
            float offset       = targetCenter - avgLevel;
            for (int b = 0; b < kNumRtaBands; ++b) rtaCurve[b] = juce::jlimit(0.0f, 1.0f, rtaCurve[b] + offset);
        }

        spectrograph_.setReferenceCurve(rtaCurve, kNumRtaBands);
        spectrograph_.setReferenceEnabled(true);
    }

    void AnalyzersPanelComponent::smoothVisuals(double sr)
    {
        if (!isShowing()) return;
        meterPanel_.advanceVisuals(sr);
        phaseScope_.advanceVisuals(sr, true);
        vuMeters_.advanceVisuals(sr, true);
        if (showWidth_ || showDna_) stereoWidthMeter_.advanceVisuals(sr, true);
        if (showCrest_) crestPanel_.advanceVisuals(sr, true);

        if (refToggle_->toggled != spectrograph_.isReferenceEnabled()) {
            refToggle_->toggled = spectrograph_.isReferenceEnabled();
            refToggle_->repaint();
        }

        spectrograph_.smoothSpectrum(sr, true);
    }

} // namespace mixcoach
