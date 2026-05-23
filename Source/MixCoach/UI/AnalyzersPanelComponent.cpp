#include "AnalyzersPanelComponent.h"
#include "../../Common/Constants.h"
#include "../../Common/SharedData.h"
#include <juce_graphics/juce_graphics.h>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  SmoothValue — Suavizado exponencial con ballistics separados attack/release
// ═══════════════════════════════════════════════════════════════════════════

SmoothValue::SmoothValue(float initial, float attackMs, float releaseMs)
    : current_(initial), target_(initial)
{
    setBallistics(attackMs, releaseMs);
}

void SmoothValue::setBallistics(float attackMs, float releaseMs)
{
    // Convertir ms a coeficientes (a 30fps de referencia)
    auto msToCoeff = [](float ms) {
        if (ms <= 0.0f) return 1.0f;
        return 1.0f - std::exp(-1.0f / (ms * 0.03f));
    };
    attackCoeff_  = msToCoeff(attackMs);
    releaseCoeff_ = msToCoeff(releaseMs);
}

void SmoothValue::setTarget(float newTarget, double /*sampleRate*/)
{
    target_ = newTarget;
    if (target_ > current_) {
        // Attack: más rápido
        current_ += (target_ - current_) * attackCoeff_;
    } else {
        // Release: más lento (VU ballistics)
        current_ += (target_ - current_) * releaseCoeff_;
    }
}

void SmoothValue::reset(float value)
{
    current_ = value;
    target_  = value;
}

// ═══════════════════════════════════════════════════════════════════════════
//  LUFSMeter — EBU R128 con target markers
// ═══════════════════════════════════════════════════════════════════════════

LUFSMeter::LUFSMeter()
{
    titleLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x94\x8A Loudness (LUFS)"),
                        juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(titleLabel_);
}

void LUFSMeter::resized()
{
    titleLabel_.setBounds(getLocalBounds().removeFromTop(18));
}

void LUFSMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

    auto area    = getLocalBounds().reduced(4);
    area.removeFromTop(20); // title

    auto content = area.reduced(2, 0);
    constexpr int kNumBars = 5;
    auto barWidth = content.getWidth() / kNumBars;

    // ─── Draw 5 bars ──────────────────────────────────────────────────────
    auto barBounds = content;

    drawBar(g, barBounds.removeFromLeft(barWidth).toFloat().reduced(2, 0),
            integrated_.getCurrent(), "I", "LUFS",
            MixCoachTheme::lufsIntegrated(), kTargetIntegrated);
    drawBar(g, barBounds.removeFromLeft(barWidth).toFloat().reduced(2, 0),
            shortTerm_.getCurrent(), "S", "LUFS",
            MixCoachTheme::lufsShort(), kTargetStreaming);
    drawBar(g, barBounds.removeFromLeft(barWidth).toFloat().reduced(2, 0),
            momentary_.getCurrent(), "M", "LU",
            MixCoachTheme::lufsMomentary(), kTargetBroadcast);
    drawBar(g, barBounds.removeFromLeft(barWidth).toFloat().reduced(2, 0),
            truePeak_.getCurrent(), "TP", "dBTP",
            MixCoachTheme::truePeak());
    drawBar(g, barBounds.removeFromLeft(barWidth).toFloat().reduced(2, 0),
            range_.getCurrent(), "R", "LU",
            MixCoachTheme::accent());
}

void LUFSMeter::drawBar(juce::Graphics& g, juce::Rectangle<float> bounds,
                         float value, const juce::String& label,
                         const juce::String& unit, juce::Colour colour,
                         float targetLine)
{
    // ─── Fondo ────────────────────────────────────────────────────────────
    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(bounds, 4.0f);

    // ─── Label en la parte superior ────────────────────────────────────────
    g.setColour(MixCoachTheme::textDim());
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
    auto labelArea = bounds.removeFromTop(14).reduced(2, 0);
    g.drawText(label, labelArea, juce::Justification::centred);

    // ─── Escala: mapear -40 LUFS → bottom, 0 LUFS → top ───────────────────
    float norm = juce::jlimit(0.0f, 1.0f, (value + 40.0f) / 45.0f);
    auto barFill = bounds.withTop(bounds.getBottom() - bounds.getHeight() * norm);

    // ─── Target line ─────────────────────────────────────────────────────
    if (targetLine > 0.0f) {
        float targetNorm = juce::jlimit(0.0f, 1.0f, (targetLine + 40.0f) / 45.0f);
        float targetY = bounds.getBottom() - bounds.getHeight() * targetNorm;
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        // Dashed line
        for (float x = bounds.getX(); x < bounds.getRight(); x += 6.0f) {
            g.fillRect(x, targetY - 0.5f, 3.0f, 1.0f);
        }
    }

    // ─── Bar fill con gradiente y glow ─────────────────────────────────────
    if (barFill.getHeight() > 1.0f) {
        // Gradient del color al negro
        juce::ColourGradient barGrad(
            colour.withAlpha(0.9f),
            juce::Point<float>(0.0f, barFill.getY()),
            colour.withAlpha(0.2f),
            juce::Point<float>(0.0f, barFill.getBottom()),
            false);
        g.setGradientFill(barGrad);
        g.fillRoundedRectangle(barFill, 3.0f);

        // Glow en la punta
        auto glowRect = barFill.withHeight(juce::jmax(2.0f, barFill.getHeight() * 0.2f));
        juce::ColourGradient glow(
            juce::Colours::white.withAlpha(0.2f),
            juce::Point<float>(0.0f, glowRect.getY()),
            juce::Colour(0x00000000),
            juce::Point<float>(0.0f, glowRect.getBottom()),
            false);
        g.setGradientFill(glow);
        g.fillRoundedRectangle(glowRect, 3.0f);
    }

    // ─── Valor numérico ────────────────────────────────────────────────────
    g.setColour(MixCoachTheme::textBright());
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
    auto valStr = (unit == "LU" || unit == "dBTP")
        ? juce::String(value, 1) + " " + unit
        : juce::String(value, 1);
    auto valArea = bounds.removeFromBottom(16).reduced(1, 0);
    g.drawText(valStr, valArea, juce::Justification::centred);

    // ─── Grid lines ──────────────────────────────────────────────────────
    g.setColour(MixCoachTheme::border().withAlpha(0.2f));
    float gridLines[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f };
    for (float gl : gridLines) {
        float glNorm = juce::jlimit(0.0f, 1.0f, (gl + 40.0f) / 45.0f);
        float glY = bounds.getBottom() - bounds.getHeight() * glNorm;
        g.drawHorizontalLine((int)glY, bounds.getX() + 1, bounds.getRight() - 1);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  StereoVUMeter — Medidor VU estéreo profesional
// ═══════════════════════════════════════════════════════════════════════════

StereoVUMeter::StereoVUMeter()
{
    titleLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x93\x88 Level (RMS + Peak)"),
                        juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(titleLabel_);

    leftLabel_.setText("L: --.- dB", juce::dontSendNotification);
    leftLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    leftLabel_.setJustificationType(juce::Justification::centred);
    leftLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    addAndMakeVisible(leftLabel_);

    rightLabel_.setText("R: --.- dB", juce::dontSendNotification);
    rightLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    rightLabel_.setJustificationType(juce::Justification::centred);
    rightLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    addAndMakeVisible(rightLabel_);
}

void StereoVUMeter::resized()
{
    auto area = getLocalBounds().reduced(2);
    titleLabel_.setBounds(area.removeFromTop(18));

    auto content = area.reduced(2, 0);
    auto halfWidth = content.getWidth() / 2;
    leftLabel_.setBounds(content.removeFromLeft(halfWidth).removeFromBottom(14));
    rightLabel_.setBounds(content.removeFromLeft(halfWidth).removeFromBottom(14));
}

void StereoVUMeter::setLevels(float leftRMS, float rightRMS,
                               float leftPeak, float rightPeak)
{
    leftRMS_.setTarget(leftRMS);
    rightRMS_.setTarget(rightRMS);
    leftPeak_.setTarget(leftPeak);
    rightPeak_.setTarget(rightPeak);


    // Peak hold
    if (leftPeak > leftPeakHold_) {
        leftPeakHold_ = leftPeak;
        leftHoldTimer_ = 30;
    } else if (leftHoldTimer_ > 0) {
        leftHoldTimer_--;
    } else {
        leftPeakHold_ += (-80.0f - leftPeakHold_) * 0.03f;
    }

    if (rightPeak > rightPeakHold_) {
        rightPeakHold_ = rightPeak;
        rightHoldTimer_ = 30;
    } else if (rightHoldTimer_ > 0) {
        rightHoldTimer_--;
    } else {
        rightPeakHold_ += (-80.0f - rightPeakHold_) * 0.03f;
    }

    leftLabel_.setText("L: " + juce::String(leftPeak, 1) + " dB", juce::dontSendNotification);
    rightLabel_.setText("R: " + juce::String(rightPeak, 1) + " dB", juce::dontSendNotification);
    repaint();
}

void StereoVUMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(20); // title
    area.removeFromBottom(14); // labels

    auto content = area.reduced(2, 0);
    auto halfWidth = content.getWidth() / 2;

    drawChannelMeter(g, content.removeFromLeft(halfWidth).toFloat().reduced(2, 0),
                     leftRMS_.getCurrent(), leftPeak_.getCurrent(), leftPeakHold_,
                     "L", leftColour_);
    drawChannelMeter(g, content.removeFromLeft(halfWidth).toFloat().reduced(2, 0),
                     rightRMS_.getCurrent(), rightPeak_.getCurrent(), rightPeakHold_,
                     "R", rightColour_);
}

void StereoVUMeter::drawChannelMeter(juce::Graphics& g, juce::Rectangle<float> bounds,
                                      float rms, float peak, float peakHold,
                                      const juce::String& channelLabel,
                                      juce::Colour colour)
{
    // ─── Fondo ────────────────────────────────────────────────────────────
    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(bounds, 4.0f);

    // ─── Channel label ────────────────────────────────────────────────────
    g.setColour(MixCoachTheme::textDim());
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
    auto labelArea = bounds.removeFromTop(14).reduced(1, 0);
    g.drawText(channelLabel, labelArea, juce::Justification::centred);

    // ─── Grid lines ──────────────────────────────────────────────────────
    g.setColour(MixCoachTheme::border().withAlpha(0.3f));
    float gridLevels[] = { -18.0f, -12.0f, -6.0f, 0.0f };
    for (float gl : gridLevels) {
        float glNorm = juce::jlimit(0.0f, 1.0f, (gl + 60.0f) / 66.0f);
        float glY = bounds.getBottom() - bounds.getHeight() * glNorm;
        g.drawHorizontalLine((int)glY, bounds.getX() + 2, bounds.getRight() - 2);

        // Label
        g.setFont(juce::Font(juce::FontOptions(6.5f)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.4f));
        g.drawText(juce::String((int)gl),
                   juce::Rectangle<float>(bounds.getX() + 2, glY - 5, 14, 8),
                   juce::Justification::centredLeft);
    }

    // ─── RMS Bar fill ─────────────────────────────────────────────────────
    float norm = juce::jlimit(0.0f, 1.0f, (rms + 60.0f) / 66.0f);
    if (norm > 0.01f) {
        auto fillBounds = bounds.withTop(bounds.getBottom() - bounds.getHeight() * norm);

        juce::Colour fillColour;
        if (rms > -6.0f)      fillColour = MixCoachTheme::meterRed();
        else if (rms > -12.0f) fillColour = MixCoachTheme::meterOrange();
        else if (rms > -18.0f) fillColour = MixCoachTheme::meterYellow();
        else                   fillColour = colour;

        juce::ColourGradient barGrad(
            fillColour.withAlpha(0.9f),
            juce::Point<float>(0.0f, fillBounds.getY()),
            fillColour.withAlpha(0.2f),
            juce::Point<float>(0.0f, fillBounds.getBottom()),
            false);
        g.setGradientFill(barGrad);
        g.fillRoundedRectangle(fillBounds, 3.0f);
    }

    // ─── Peak indicator dot ───────────────────────────────────────────────
    if (peak > -60.0f) {
        float peakNorm = juce::jlimit(0.0f, 1.0f, (peak + 60.0f) / 66.0f);
        float peakY = bounds.getBottom() - bounds.getHeight() * peakNorm;
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.fillEllipse(bounds.getCentreX() - 3.0f, peakY - 2.0f, 6.0f, 4.0f);
    }

    // ─── Peak hold line ──────────────────────────────────────────────────
    if (peakHold > -60.0f) {
        float holdNorm = juce::jlimit(0.0f, 1.0f, (peakHold + 60.0f) / 66.0f);
        float holdY = bounds.getBottom() - bounds.getHeight() * holdNorm;
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.fillRect(bounds.getX() + 2, holdY - 0.5f, bounds.getWidth() - 4, 1.5f);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  SpectrographComponent — Espectrograma con colores dinámicos
// ═══════════════════════════════════════════════════════════════════════════

SpectrographComponent::SpectrographComponent()
{
    bins_.resize(kNumBins, 0.0f);
    smoothBins_.resize(kNumBins, 0.0f);
}

void SpectrographComponent::updateSpectrum(const float* data, int numBins)
{
    int n = std::min(numBins, kNumBins);
    for (int i = 0; i < n; ++i) {
        // Smooth: ataque rápido, release medio
        float raw = juce::jlimit(-100.0f, 6.0f, 20.0f * std::log10(data[i] + 1e-6f));
        float rawNorm = juce::jmap(raw, -80.0f, 0.0f, 0.0f, 1.0f);
        rawNorm = juce::jlimit(0.0f, 1.0f, rawNorm);

        if (rawNorm > smoothBins_[i])
            smoothBins_[i] += (rawNorm - smoothBins_[i]) * 0.6f;
        else
            smoothBins_[i] += (rawNorm - smoothBins_[i]) * 0.15f;

        bins_[i] = smoothBins_[i];
    }
    repaint();
}

juce::Colour SpectrographComponent::getBinColour(float magnitude) const
{
    // Gradiente: azul → cyan → verde → amarillo → rojo
    if (magnitude < 0.15f) {
        // Azul → Cyan
        float t = magnitude / 0.15f;
        return juce::Colour::fromFloatRGBA(t * 0.5f, t * 0.8f, 0.5f + t * 0.5f, 1.0f);
    } else if (magnitude < 0.35f) {
        // Cyan → Verde
        float t = (magnitude - 0.15f) / 0.20f;
        return juce::Colour::fromFloatRGBA(0.5f * (1.0f - t), 0.8f + t * 0.2f, 1.0f - t * 0.9f, 1.0f);
    } else if (magnitude < 0.55f) {
        // Verde → Amarillo
        float t = (magnitude - 0.35f) / 0.20f;
        return juce::Colour::fromFloatRGBA(0.5f + t * 0.5f, 1.0f, 0.1f * (1.0f - t), 1.0f);
    } else if (magnitude < 0.75f) {
        // Amarillo → Naranja
        float t = (magnitude - 0.55f) / 0.20f;
        return juce::Colour::fromFloatRGBA(1.0f, 1.0f - t * 0.2f, 0.0f, 1.0f);
    } else {
        // Naranja → Rojo
        float t = (magnitude - 0.75f) / 0.25f;
        return juce::Colour::fromFloatRGBA(1.0f, 0.8f * (1.0f - t), 0.0f, 1.0f);
    }
}

void SpectrographComponent::resized()
{
}

void SpectrographComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto w = bounds.getWidth();
    auto h = bounds.getHeight();

    // ─── Fondo ────────────────────────────────────────────────────────────
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

    // ─── Grid lines (octavas) ────────────────────────────────────────────
    g.setColour(MixCoachTheme::border().withAlpha(0.2f));
    // Octave markers: 31Hz, 63Hz, 125Hz, 250Hz, 500Hz, 1kHz, 2kHz, 4kHz, 8kHz, 16kHz
    float octaveFreqs[] = { 31.0f, 63.0f, 125.0f, 250.0f, 500.0f,
                            1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };
    float maxFreq = 20000.0f;
    for (float freq : octaveFreqs) {
        // Log position on X axis
        float xNorm = std::log2(freq / 20.0f) / std::log2(maxFreq / 20.0f);
        float x = bounds.getX() + xNorm * w;
        g.drawVerticalLine((int)x, bounds.getY() + 1, bounds.getBottom() - 1);

        // Frecuencia label
        g.setFont(juce::Font(juce::FontOptions(7.0f)));
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.4f));
        juce::String freqLabel;
        if (freq >= 1000.0f)
            freqLabel = juce::String((int)(freq / 1000.0f)) + "k";
        else
            freqLabel = juce::String((int)freq);
        g.drawText(freqLabel,
                   juce::Rectangle<float>(x - 8, bounds.getBottom() - 12, 24, 10),
                   juce::Justification::centred);
    }

    // ─── Grid horizontal (dB) ────────────────────────────────────────────
    float dbLevels[] = { -48.0f, -36.0f, -24.0f, -12.0f, -6.0f, 0.0f };
    for (float db : dbLevels) {
        float dbNorm = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 66.0f);
        float y = bounds.getBottom() - dbNorm * h;
        g.setColour(MixCoachTheme::border().withAlpha(0.15f));
        g.drawHorizontalLine((int)y, bounds.getX() + 1, bounds.getRight() - 1);
    }

    if (bins_.empty()) return;

    // ─── Dibujar espectro como path con relleno de gradiente ──────────────
    juce::Path path;
    bool first = true;
    path.preallocateSpace((int)bins_.size() * 4);

    for (size_t i = 0; i < bins_.size(); ++i) {
        // Log spacing: comprimir bins bajos, expandir altos visualmente
        float x = bounds.getX() + (float)i / (float)bins_.size() * w;
        float normalized = juce::jlimit(0.0f, 1.0f, bins_[i]);

        // Colorear el área debajo del path
        float y = bounds.getBottom() - normalized * h;

        if (first) {
            path.startNewSubPath(x, bounds.getBottom());
            path.lineTo(x, y);
            first = false;
        } else {
            path.lineTo(x, y);
        }
    }

    // Cerrar el path
    float lastX = bounds.getX() + w;
    path.lineTo(lastX, bounds.getBottom());
    path.closeSubPath();

    // ─── Filled gradient ──────────────────────────────────────────────────
    juce::ColourGradient spectrumGrad(
        juce::Colour(0xFF00B4D8).withAlpha(0.3f),
        juce::Point<float>(0.0f, bounds.getY()),
        juce::Colour(0xFFFF2D55).withAlpha(0.1f),
        juce::Point<float>(0.0f, bounds.getBottom()),
        false);
    g.setGradientFill(spectrumGrad);
    g.fillPath(path);

    // ─── Línea del espectro con color dinámico por segmentos ──────────────
    // Dibujar segmentos de colores siguiendo bins_
    for (size_t i = 1; i < bins_.size(); ++i) {
        float x1 = bounds.getX() + (float)(i - 1) / (float)bins_.size() * w;
        float x2 = bounds.getX() + (float)i / (float)bins_.size() * w;
        float y1 = bounds.getBottom() - juce::jlimit(0.0f, 1.0f, bins_[i - 1]) * h;
        float y2 = bounds.getBottom() - juce::jlimit(0.0f, 1.0f, bins_[i]) * h;

        float avgMag = (bins_[i - 1] + bins_[i]) * 0.5f;
        g.setColour(getBinColour(avgMag));
        g.drawLine(x1, y1, x2, y2, 1.5f);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  VectorscopeComponent — Vectorscopio circular
// ═══════════════════════════════════════════════════════════════════════════

VectorscopeComponent::VectorscopeComponent()
{
    titleLabel_.setText(juce::CharPointer_UTF8("\xE2\xAD\x90 Vectorscope"),
                        juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(titleLabel_);

    corrLabel_.setText("\xCF\x86: +1.00", juce::dontSendNotification);
    corrLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
    corrLabel_.setJustificationType(juce::Justification::centred);
    corrLabel_.setColour(juce::Label::textColourId, MixCoachTheme::success());
    addAndMakeVisible(corrLabel_);
}

void VectorscopeComponent::resized()
{
    auto area = getLocalBounds().reduced(2);
    titleLabel_.setBounds(area.removeFromTop(16));
    corrLabel_.setBounds(area.removeFromBottom(16));
}

void VectorscopeComponent::pushSample(float left, float right)
{
    auto& pt = trace_[writePos_ % kTraceLen];
    pt.x = juce::jlimit(-1.0f, 1.0f, left);
    pt.y = juce::jlimit(-1.0f, 1.0f, right);
    pt.alpha = 1.0f;
    writePos_ = (writePos_ + 1) % kTraceLen;

    // Actualizar correlación para mostrarla
    float corr = 0.0f;
    float sumL2 = 0.0f, sumR2 = 0.0f, sumLR = 0.0f;
    int count = 0;
    for (int i = 0; i < kTraceLen; ++i) {
        auto& p = trace_[i];
        if (p.alpha > 0.01f) {
            sumLR += p.x * p.y;
            sumL2 += p.x * p.x;
            sumR2 += p.y * p.y;
            count++;
        }
        // Decaimiento fósforo
        p.alpha *= 0.96f;
    }
    if (count > 0 && sumL2 > 0.0f && sumR2 > 0.0f) {
        corr = sumLR / (std::sqrt(sumL2) * std::sqrt(sumR2));
    }

    juce::Colour corrColour;
    if (std::abs(corr) < 0.3f)
        corrColour = MixCoachTheme::error();
    else if (corr < 0.0f)
        corrColour = MixCoachTheme::warning();
    else
        corrColour = MixCoachTheme::success();

    corrLabel_.setText("\xCF\x86: " + juce::String(corr, 2), juce::dontSendNotification);
    corrLabel_.setColour(juce::Label::textColourId, corrColour);

    repaint();
}

void VectorscopeComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(18); // title
    area.removeFromBottom(18); // correlation label

    // ─── Área circular ────────────────────────────────────────────────────
    auto squareSize = juce::jmin(area.getWidth(), area.getHeight()) - 4;
    auto circleArea = juce::Rectangle<float>(0, 0, (float)squareSize, (float)squareSize)
                          .withCentre(area.toFloat().getCentre());
    drawGrid(g, circleArea);

    // ─── Lissajous trace ─────────────────────────────────────────────────
    auto cx = circleArea.getCentreX();
    auto cy = circleArea.getCentreY();
    auto radius = circleArea.getWidth() * 0.5f - 4.0f;

    juce::Path tracePath;
    bool first = true;

    for (int i = 0; i < kTraceLen; ++i) {
        int idx = (writePos_ + i) % kTraceLen;
        auto& pt = trace_[idx];
        if (pt.alpha < 0.01f) continue;

        float sx = cx + pt.x * radius;
        float sy = cy + pt.y * radius;

        if (first) {
            tracePath.startNewSubPath(sx, sy);
            first = false;
        } else {
            tracePath.lineTo(sx, sy);
        }
    }

    // Dibujar trace con efecto fósforo (opacidad basada en alpha)
    for (int i = 0; i < kTraceLen; ++i) {
        int idx = (writePos_ + i) % kTraceLen;
        auto& pt = trace_[idx];
        if (pt.alpha < 0.01f) continue;

        float sx = cx + pt.x * radius;
        float sy = cy + pt.y * radius;

        g.setColour(juce::Colour(0xFF00E676).withAlpha(pt.alpha * 0.6f));
        g.fillEllipse(sx - 1.5f, sy - 1.5f, 3.0f, 3.0f);
    }

    // Línea de contorno del trace
    if (!first) {
        g.setColour(juce::Colour(0xFF00E676).withAlpha(0.4f));
        g.strokePath(tracePath, juce::PathStrokeType(1.0f));
    }
}

void VectorscopeComponent::drawGrid(juce::Graphics& g, juce::Rectangle<float> area)
{
    auto cx = area.getCentreX();
    auto cy = area.getCentreY();
    auto radius = area.getWidth() * 0.5f - 4.0f;

    // ─── Círculo exterior ─────────────────────────────────────────────────
    g.setColour(MixCoachTheme::border().withAlpha(0.4f));
    g.drawEllipse(area.reduced(4.0f), 1.0f);

    // ─── Círculo interior (0.5) ───────────────────────────────────────────
    juce::ColourGradient innerGrad(
        MixCoachTheme::border().withAlpha(0.15f),
        cx, cy,
        MixCoachTheme::border().withAlpha(0.0f),
        cx + radius * 0.5f, cy,
        false);
    g.setGradientFill(innerGrad);
    g.drawEllipse(juce::Rectangle<float>(cx - radius * 0.5f, cy - radius * 0.5f,
                                          radius, radius), 1.0f);

    // ─── Líneas cruzadas ─────────────────────────────────────────────────
    g.setColour(MixCoachTheme::border().withAlpha(0.2f));
    g.drawHorizontalLine((int)cy, area.getX() + 2, area.getRight() - 2);
    g.drawVerticalLine((int)cx, area.getY() + 2, area.getBottom() - 2);

    // ─── Diagonales a 45° ────────────────────────────────────────────────
    float d = radius * 0.707f; // cos(45°) * radius
    g.drawLine(cx - d, cy - d, cx + d, cy + d, 0.5f);
    g.drawLine(cx - d, cy + d, cx + d, cy - d, 0.5f);

    // ─── Center dot ──────────────────────────────────────────────────────
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
    g.fillEllipse(cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);

    // ─── Labels ──────────────────────────────────────────────────────────
    g.setFont(juce::Font(juce::FontOptions(7.0f)));
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
    g.drawText("L", juce::Rectangle<float>(area.getX(), cy - 8, 12, 12),
               juce::Justification::centred);
    g.drawText("R", juce::Rectangle<float>(area.getRight() - 14, cy - 8, 12, 12),
               juce::Justification::centred);
    g.drawText("R", juce::Rectangle<float>(cx - 8, area.getY(), 12, 12),
               juce::Justification::centred);
    g.drawText("L", juce::Rectangle<float>(cx - 8, area.getBottom() - 14, 12, 12),
               juce::Justification::centred);
}

// ═══════════════════════════════════════════════════════════════════════════
//  PhaseCorrelationMeter
// ═══════════════════════════════════════════════════════════════════════════

PhaseCorrelationMeter::PhaseCorrelationMeter()
{
    titleLabel_.setText(juce::CharPointer_UTF8("\xCF\x86 Phase Correlation"),
                        juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(titleLabel_);

    valueLabel_.setText("+1.00", juce::dontSendNotification);
    valueLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)).boldened());
    valueLabel_.setJustificationType(juce::Justification::centred);
    valueLabel_.setColour(juce::Label::textColourId, MixCoachTheme::success());
    addAndMakeVisible(valueLabel_);
}

void PhaseCorrelationMeter::resized()
{
    auto area = getLocalBounds().reduced(2);
    titleLabel_.setBounds(area.removeFromTop(16));
    valueLabel_.setBounds(area.removeFromTop(18));
}

void PhaseCorrelationMeter::setCorrelation(float value)
{
    correlation_.setTarget(value);

    auto corr = correlation_.getCurrent();

    juce::Colour col;
    if (std::abs(corr) < 0.3f)
        col = MixCoachTheme::error();
    else if (corr < 0.0f)
        col = MixCoachTheme::warning();
    else
        col = MixCoachTheme::success();

    valueLabel_.setColour(juce::Label::textColourId, col);
    valueLabel_.setText(juce::String(corr, 2), juce::dontSendNotification);
    repaint();
}

void PhaseCorrelationMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(34); // title + value

    // ─── Barra de correlación ─────────────────────────────────────────────
    auto barBounds = area.reduced(8, 4).toFloat();

    // Fondo
    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(barBounds, 4.0f);

    // Gradiente de fondo: rojo (izq) → gris (centro) → verde (der)
    juce::ColourGradient bgGrad(
        MixCoachTheme::error().withAlpha(0.2f),
        juce::Point<float>(barBounds.getX(), 0.0f),
        MixCoachTheme::warning().withAlpha(0.15f),
        juce::Point<float>(barBounds.getCentreX(), 0.0f),
        false);
    bgGrad.addColour(0.75, MixCoachTheme::success().withAlpha(0.2f));
    g.setGradientFill(bgGrad);
    g.fillRoundedRectangle(barBounds, 4.0f);

    // Indicador
    float corr = juce::jlimit(-1.0f, 1.0f, correlation_.getCurrent());
    float norm = (corr + 1.0f) * 0.5f;
    float markerX = barBounds.getX() + norm * barBounds.getWidth();

    juce::Colour indicatorColour;
    if (std::abs(corr) < 0.3f)
        indicatorColour = MixCoachTheme::error();
    else if (corr < 0.0f)
        indicatorColour = MixCoachTheme::warning();
    else
        indicatorColour = MixCoachTheme::success();

    // Glow del indicador
    juce::ColourGradient glow(
        indicatorColour.withAlpha(0.4f),
        juce::Point<float>(markerX, barBounds.getCentreY()),
        indicatorColour.withAlpha(0.0f),
        juce::Point<float>(markerX + 20.0f, barBounds.getCentreY()),
        false);
    g.setGradientFill(glow);
    g.fillEllipse(markerX - 10.0f, barBounds.getCentreY() - 6.0f, 20.0f, 12.0f);

    // Indicador (triángulo o círculo)
    g.setColour(indicatorColour);
    g.fillEllipse(markerX - 5.0f, barBounds.getCentreY() - 5.0f, 10.0f, 10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.fillEllipse(markerX - 2.0f, barBounds.getCentreY() - 2.0f, 4.0f, 4.0f);

    // Borde
    g.setColour(MixCoachTheme::border().withAlpha(0.5f));
    g.drawRoundedRectangle(barBounds, 4.0f, 1.0f);

    // Labels -1, 0, +1
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    g.drawText("-1", juce::Rectangle<float>(barBounds.getX() - 2, barBounds.getBottom() + 2, 16, 12),
               juce::Justification::centred);
    g.drawText("0", juce::Rectangle<float>(barBounds.getCentreX() - 8, barBounds.getBottom() + 2, 16, 12),
               juce::Justification::centred);
    g.drawText("+1", juce::Rectangle<float>(barBounds.getRight() - 14, barBounds.getBottom() + 2, 16, 12),
               juce::Justification::centred);

    // Label de warning si está fuera de fase
    if (corr < -0.3f) {
        g.setColour(MixCoachTheme::error().withAlpha(0.5f));
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
        g.drawText("\xE2\x9A\xA0 Fuera de fase!", barBounds.withTop(barBounds.getBottom() - 22).toNearestInt(),
                   juce::Justification::centred);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  CrestHistogram — Histograma dinámico de Crest Factor
// ═══════════════════════════════════════════════════════════════════════════

CrestHistogram::CrestHistogram()
{
    titleLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x93\x88 Crest Factor"),
                        juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(titleLabel_);

    avgLabel_.setText("Avg: --.- dB", juce::dontSendNotification);
    avgLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    avgLabel_.setJustificationType(juce::Justification::centredLeft);
    avgLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    addAndMakeVisible(avgLabel_);

    maxLabel_.setText("Max: --.- dB", juce::dontSendNotification);
    maxLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    maxLabel_.setJustificationType(juce::Justification::centredRight);
    maxLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    addAndMakeVisible(maxLabel_);

    histogram_.fill(0);
}

void CrestHistogram::resized()
{
    auto area = getLocalBounds().reduced(2);
    titleLabel_.setBounds(area.removeFromTop(16));
    auto infoRow = area.removeFromBottom(16);
    avgLabel_.setBounds(infoRow.removeFromLeft(infoRow.getWidth() / 2));
    maxLabel_.setBounds(infoRow);
}

void CrestHistogram::pushCrest(float peakDb, float rmsDb)
{
    // Crest factor = peak - RMS (en dB)
    float crest = juce::jlimit(0.0f, 30.0f, peakDb - rmsDb);
    recentCrest_.push_back(crest);

    if (recentCrest_.size() > (size_t)kMaxSamples)
        recentCrest_.pop_front();

    // Actualizar histograma
    totalSamples_++;
    int bin = juce::jlimit(0, kNumBins - 1,
                           (int)(crest / 30.0f * (float)kNumBins));
    histogram_[bin]++;

    // Calcular promedio
    float sum = 0.0f;
    float maxVal = 0.0f;
    for (auto c : recentCrest_) {
        sum += c;
        if (c > maxVal) maxVal = c;
    }
    float avg = recentCrest_.empty() ? 0.0f : sum / (float)recentCrest_.size();

    avgLabel_.setText("Avg: " + juce::String(avg, 1) + " dB", juce::dontSendNotification);
    maxLabel_.setText("Max: " + juce::String(maxVal, 1) + " dB", juce::dontSendNotification);

    repaint();
}

void CrestHistogram::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(18); // title
    area.removeFromBottom(18); // stats labels

    auto histArea = area.toFloat().reduced(2, 4);

    // ─── Fondo ────────────────────────────────────────────────────────────
    g.setColour(MixCoachTheme::bgDarker());
    g.fillRoundedRectangle(histArea, 3.0f);

    if (totalSamples_ == 0) {
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
        g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
        g.drawText("Esperando datos...", histArea.toNearestInt(), juce::Justification::centred);
        return;
    }

    // ─── Encontrar máximo para normalizar ──────────────────────────────────
    int maxBinCount = 1;
    for (int count : histogram_)
        if (count > maxBinCount) maxBinCount = count;

    float barWidth = histArea.getWidth() / (float)kNumBins;
    float maxHeight = histArea.getHeight() - 4.0f;

    // ─── Dibujar barras verticales ────────────────────────────────────────
    for (int i = 0; i < kNumBins; ++i) {
        float height = ((float)histogram_[i] / (float)maxBinCount) * maxHeight;
        if (height < 1.0f) continue;

        float x = histArea.getX() + (float)i * barWidth + 1.0f;
        float w = barWidth - 2.0f;
        float y = histArea.getBottom() - 2.0f - height;

        // Color: azul (bajo crest) → verde → amarillo → rojo (alto crest)
        float t = (float)i / (float)kNumBins;
        juce::Colour barColour;
        if (t < 0.33f)
            barColour = juce::Colour::fromFloatRGBA(t * 3.0f * 0.3f, 0.5f + t, 0.8f, 0.8f);
        else if (t < 0.66f)
            barColour = juce::Colour::fromFloatRGBA((t - 0.33f) * 3.0f, 0.9f, 0.1f, 0.8f);
        else
            barColour = juce::Colour::fromFloatRGBA(1.0f, 0.8f * (1.0f - (t - 0.66f) * 3.0f), 0.0f, 0.8f);

        juce::ColourGradient barGrad(
            barColour,
            juce::Point<float>(x, y),
            barColour.withAlpha(0.2f),
            juce::Point<float>(x, y + height),
            false);
        g.setGradientFill(barGrad);
        g.fillRoundedRectangle(juce::Rectangle<float>(x, y, w, height), 1.5f);
    }

    // ─── Crest factor labels en el eje X ──────────────────────────────────
    g.setFont(juce::Font(juce::FontOptions(7.0f)));
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.4f));
    g.drawText("0", juce::Rectangle<float>(histArea.getX(), histArea.getBottom() - 12, 20, 10),
               juce::Justification::centredLeft);
    g.drawText("15", juce::Rectangle<float>(histArea.getCentreX() - 10, histArea.getBottom() - 12, 20, 10),
               juce::Justification::centred);
    g.drawText("30dB", juce::Rectangle<float>(histArea.getRight() - 28, histArea.getBottom() - 12, 28, 10),
               juce::Justification::centredRight);

    // ─── Línea de crest promedio ──────────────────────────────────────────
    float sum = 0.0f;
    for (auto c : recentCrest_) sum += c;
    if (!recentCrest_.empty()) {
        float avgCrest = sum / (float)recentCrest_.size();
        float avgX = histArea.getX() + (avgCrest / 30.0f) * histArea.getWidth();
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.drawVerticalLine((int)avgX, histArea.getY() + 2, histArea.getBottom() - 2);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  TrackMiniStrip — Mini barra de nivel individual
// ═══════════════════════════════════════════════════════════════════════════

TrackMiniStrip::TrackMiniStrip()
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setOpaque(false);
}

void TrackMiniStrip::setTrackData(const SlotInfo& info, float peakLeft, float peakRight, bool hasSignal)
{
    info_ = info;
    peakLeft_ = peakLeft;
    peakRight_ = peakRight;
    hasSignal_ = hasSignal;
    slotIndex_ = info.slotIndex;
    repaint();
}

void TrackMiniStrip::setSelected(bool selected)
{
    if (selected_ != selected) {
        selected_ = selected;
        repaint();
    }
}

void TrackMiniStrip::mouseDown(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    if (onClick && slotIndex_ >= 0)
        onClick(slotIndex_);
}

void TrackMiniStrip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);

    // ─── Fondo ────────────────────────────────────────────────────────────
    if (selected_) {
        g.setColour(MixCoachTheme::accent().withAlpha(0.15f));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(MixCoachTheme::accent().withAlpha(0.5f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.5f);
    } else {
        g.setColour(MixCoachTheme::bgPanel().withAlpha(0.6f));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(MixCoachTheme::border().withAlpha(0.3f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }

    // ─── Color bar ────────────────────────────────────────────────────────
    auto colourBar = bounds.removeFromLeft(4);
    g.setColour(info_.colour);
    g.fillRoundedRectangle(colourBar, 2.0f);

    bounds.removeFromLeft(2);

    // ─── Status dot ──────────────────────────────────────────────────────
    auto dotArea = bounds.removeFromLeft(6).reduced(2, 7);
    if (hasSignal_) {
        float pulse = 0.6f + 0.4f * std::sin(juce::Time::getMillisecondCounter() * 0.006f);
        g.setColour(MixCoachTheme::success().withAlpha(pulse));
    } else {
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
    }
    g.fillEllipse(dotArea.toFloat());

    // ─── Track name ──────────────────────────────────────────────────────
    auto name = juce::String(info_.trackName);
    if (name.length() > 6) name = name.substring(0, 5) + ".";
    g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
    g.setColour(selected_ ? MixCoachTheme::textBright() : MixCoachTheme::textPrimary());
    g.drawText(name, bounds.reduced(0, 6).toNearestInt(), juce::Justification::centredLeft);

    // ─── Mini level bar ─────────────────────────────────────────────────
    if (hasSignal_) {
        auto barArea = bounds.removeFromBottom(4).reduced(2, 0);
        float norm = juce::jlimit(0.0f, 1.0f, (peakLeft_ + 60.0f) / 66.0f);
        g.setColour(MixCoachTheme::bgDarker());
        g.fillRoundedRectangle(barArea.toFloat(), 1.0f);
        if (norm > 0.01f) {
            g.setColour(peakLeft_ > -6.0f ? MixCoachTheme::error()
                        : peakLeft_ > -12.0f ? MixCoachTheme::warning()
                        : MixCoachTheme::success());
            g.fillRoundedRectangle(barArea.withWidth(barArea.getWidth() * norm).toFloat(), 1.0f);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  TrackSelectorStrip — Barra horizontal de selección de pistas
// ═══════════════════════════════════════════════════════════════════════════

TrackSelectorStrip::TrackSelectorStrip()
{
    for (auto& strip : strips_)
        strip = std::make_unique<TrackMiniStrip>();

    for (int i = 0; i < kMaxVisible; ++i) {
        auto* s = strips_[i].get();
        s->onClick = [this](int slotIndex) {
            setSelectedTrack(slotIndex);
            if (onTrackSelected)
                onTrackSelected(slotIndex);
        };
        addAndMakeVisible(s);
    }

    placeholderLabel_.setText(
        juce::CharPointer_UTF8("\xF0\x9F\x94\x8C No hay Messengers conectados. "
                               "Coloca un plugin Messenger en tus pistas para verlas aqui."),
        juce::dontSendNotification);
    placeholderLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)));
    placeholderLabel_.setJustificationType(juce::Justification::centred);
    placeholderLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
    addAndMakeVisible(placeholderLabel_);
}

void TrackSelectorStrip::resized()
{
    auto bounds = getLocalBounds().reduced(4, 2);
    auto stripWidth = 72;

    if (activeTrackCount_ == 0) {
        placeholderLabel_.setBounds(bounds);
        for (auto& s : strips_) s->setVisible(false);
        return;
    }

    placeholderLabel_.setVisible(false);
    int xOffset = bounds.getX();

    for (int i = 0; i < kMaxVisible && i < activeTrackCount_; ++i) {
        auto* s = strips_[i].get();
        s->setVisible(true);
        s->setBounds(xOffset, bounds.getY(), stripWidth, bounds.getHeight());
        xOffset += stripWidth + 3;
    }

    // Hide remaining strips
    for (int i = activeTrackCount_; i < kMaxVisible; ++i)
        strips_[i]->setVisible(false);
}

void TrackSelectorStrip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);
}

void TrackSelectorStrip::updateTracks(SlotRegistry& registry)
{
    int count = 0;
    registry.forEachActive([&](const SlotInfo& info) {
        if (count >= kMaxVisible) return;
        int idx = info.slotIndex;
        auto& telem = registry.getTelemetry(idx);
        auto latest = telem.latest();
        bool hasSignal = (latest.peakLeft > -60.0f || latest.peakRight > -60.0f);
        strips_[count]->setTrackData(info, latest.peakLeft, latest.peakRight, hasSignal);
        strips_[count]->setSelected(idx == selectedSlot_);
        count++;
    });

    activeTrackCount_ = count;
    resized();
}

void TrackSelectorStrip::setSelectedTrack(int slotIndex)
{
    if (selectedSlot_ == slotIndex) return;
    selectedSlot_ = slotIndex;

    // Actualizar estado de todos los strips
    for (int i = 0; i < activeTrackCount_ && i < kMaxVisible; ++i) {
        strips_[i]->setSelected(strips_[i]->getSlotIndex() == slotIndex);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  AnalyzersPanelComponent — Panel principal de metering
// ═══════════════════════════════════════════════════════════════════════════

AnalyzersPanelComponent::AnalyzersPanelComponent(SharedData& sharedData)
    : sharedData_(sharedData)
{
    // ─── Header ──────────────────────────────────────────────────────────
    headerLabel_.setText(juce::CharPointer_UTF8("\xF0\x9F\x93\x8A Professional Metering"),
                         juce::dontSendNotification);
    headerLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTitle)).boldened());
    headerLabel_.setJustificationType(juce::Justification::centredLeft);
    headerLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textBright());
    addAndMakeVisible(headerLabel_);

    // ─── Track selector ──────────────────────────────────────────────────
    trackSelector_.onTrackSelected = [this](int slotIndex) {
        selectedSlot_ = slotIndex;
        auto& registry = sharedData_.getSlotRegistry();
        auto info = registry.getSlotInfo(slotIndex);
        selectedTrackName_ = juce::String(info.trackName);
        selectedColour_ = info.colour;
        selectedTrackLabel_.setText(
            juce::String("\xF0\x9F\x8E\xB5 ") + "Analizando: " +
            selectedTrackName_,
            juce::dontSendNotification);
        selectedTrackLabel_.setColour(juce::Label::textColourId, selectedColour_);
    };
    addAndMakeVisible(trackSelector_);

    // ─── Selected track label ────────────────────────────────────────────
    selectedTrackLabel_.setText(
        "\xF0\x9F\x90\xBB Selecciona una pista en el selector superior",
        juce::dontSendNotification);
    selectedTrackLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeHeader)).boldened());
    selectedTrackLabel_.setJustificationType(juce::Justification::centredLeft);
    selectedTrackLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    addAndMakeVisible(selectedTrackLabel_);

    // ─── Analyzer components ─────────────────────────────────────────────
    addAndMakeVisible(spectrograph_);
    addAndMakeVisible(lufsMeter_);
    addAndMakeVisible(stereoVUMeter_);
    addAndMakeVisible(vectorscope_);
    addAndMakeVisible(phaseMeter_);
    addAndMakeVisible(crestHistogram_);
}

void AnalyzersPanelComponent::resized()
{
    auto area = getLocalBounds().reduced(6);

    // ─── Header ──────────────────────────────────────────────────────────
    headerLabel_.setBounds(area.removeFromTop(24));

    // ─── Track selector ──────────────────────────────────────────────────
    auto selectorArea = area.removeFromTop(56);
    trackSelector_.setBounds(selectorArea);

    // ─── Selected track info ─────────────────────────────────────────────
    selectedTrackLabel_.setBounds(area.removeFromTop(20));

    // ─── Main content vertical split ─────────────────────────────────────
    // Top: Spectrograph (45%)
    auto topArea = area.removeFromTop((int)(area.getHeight() * 0.45f));
    spectrograph_.setBounds(topArea.reduced(2));

    // Bottom: split into two columns
    auto bottomArea = area.reduced(2);

    // Left column (60%): LUFS + Stereo VU (stacked horizontally)
    auto leftColumn = bottomArea.removeFromLeft((int)(bottomArea.getWidth() * 0.55f));
    auto leftTop = leftColumn.removeFromTop((int)(leftColumn.getHeight() * 0.55f));
    lufsMeter_.setBounds(leftTop.reduced(1));
    stereoVUMeter_.setBounds(leftColumn.reduced(1));

    // Right column (45%): Vectorscope + Phase + Crest Histogram
    auto rightColumn = bottomArea.reduced(1);

    auto vectorscopeArea = rightColumn.removeFromTop((int)(rightColumn.getHeight() * 0.38f));
    vectorscope_.setBounds(vectorscopeArea);

    auto phaseArea = rightColumn.removeFromTop((int)(rightColumn.getHeight() * 0.32f));
    phaseMeter_.setBounds(phaseArea);

    crestHistogram_.setBounds(rightColumn);
}

void AnalyzersPanelComponent::paint(juce::Graphics& g)
{
    // ─── Fondo con gradiente oscuro ──────────────────────────────────────
    juce::ColourGradient bgGrad(
        MixCoachTheme::bgDark(),
        juce::Point<float>(0.0f, 0.0f),
        MixCoachTheme::bgDarker(),
        juce::Point<float>(0.0f, (float)getHeight()),
        false);
    g.setGradientFill(bgGrad);
    g.fillRect(getLocalBounds());

    // Subtle grid pattern
    g.setColour(MixCoachTheme::border().withAlpha(0.05f));
    for (int x = 0; x < getWidth(); x += 50)
        g.drawVerticalLine(x, 0.0f, (float)getHeight());
    for (int y = 0; y < getHeight(); y += 50)
        g.drawHorizontalLine(y, 0.0f, (float)getWidth());
}

void AnalyzersPanelComponent::updateAnalyzers(SlotRegistry& registry)
{
    // ─── 1. Actualizar selector de pistas ─────────────────────────────────
    trackSelector_.updateTracks(registry);

    // ─── 2. Si no hay slot seleccionado, auto-seleccionar el primero ──────
    if (selectedSlot_ < 0) {
        registry.forEachActive([&](const SlotInfo& info) {
            if (selectedSlot_ < 0) {
                selectedSlot_ = info.slotIndex;
                selectedTrackName_ = juce::String(info.trackName);
                selectedColour_ = info.colour;
                selectedTrackLabel_.setText(
                    juce::String("\xF0\x9F\x8E\xB5 ") + "Analizando: " +
                    selectedTrackName_,
                    juce::dontSendNotification);
                selectedTrackLabel_.setColour(juce::Label::textColourId, selectedColour_);
                trackSelector_.setSelectedTrack(selectedSlot_);
            }
        });
    }

    // ─── 3. Si no hay pistas activas, resetear ───────────────────────────
    if (registry.activeCount() == 0) {
        selectedSlot_ = -1;
        selectedTrackName_.clear();
        selectedTrackLabel_.setText(
            "\xF0\x9F\x90\xBB Conecta un Messenger para empezar",
            juce::dontSendNotification);
        selectedTrackLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
        return;
    }

    // ─── 4. Actualizar analizadores con datos REALES del slot seleccionado ──
    //     Ahora los datos fluyen desde el Messenger via:
    //     TelemetryCollector (FFT + crest + LUFS + samples)
    //     → TrackTelemetry
    //     → updateSharedTelemetry() → SharedMemory
    //     → syncFromShared() → TelemetryBuffer local
    //     → registry.getTelemetry().latest()
    //     → AQUI
    if (selectedSlot_ >= 0) {
        auto& telem = registry.getTelemetry(selectedSlot_);
        auto latest = telem.latest();

        // ─── Stereo VU (RMS + Peak) ──────────────────────────────────────
        // Datos reales de TelemetryCollector::computePeak/computeRMS
        stereoVUMeter_.setLevels(latest.rmsLeft, latest.rmsRight,
                                  latest.peakLeft, latest.peakRight);

        // ─── Spectrograph (FFT real via juce::dsp::FFT) ──────────────────
        // Datos reales de TelemetryCollector::computeSpectrum()
        if (latest.active) {
            // Verificar si hay datos FFT reales (no todos ceros)
            bool hasFFT = false;
            for (int fi = 0; fi < 256 && !hasFFT; ++fi) {
                if (latest.spectrum[fi] > 0.01f) hasFFT = true;
            }
            if (hasFFT) {
                spectrograph_.updateSpectrum(latest.spectrum, 256);
            }
        }

        // ─── Phase Correlation meter ─────────────────────────────────────
        // Dato real de TelemetryCollector::computeCorrelation()
        phaseMeter_.setCorrelation(latest.correlation);

        // ─── Vectorscope con muestras de AUDIO REALES ────────────────────
        // Ya NO usa sinteticas! Usa latest.sampleL/sampleR que son las
        // últimas muestras de audio del buffer del Messenger.
        if (latest.sampleL != 0.0f || latest.sampleR != 0.0f) {
            // Normalizar samples del rango [-1, 1] a [-1, 1] para el display
            float vectL = juce::jlimit(-1.0f, 1.0f, latest.sampleL * 2.0f);
            float vectR = juce::jlimit(-1.0f, 1.0f, latest.sampleR * 2.0f);
            vectorscope_.pushSample(vectL, vectR);
        }

        // ─── LUFS (EBU R128 via LoudnessMeter con K-weighting filter) ────
        // Ya NO es simulacion! Usa datos reales del LoudnessMeter que
        // aplica K-weighting filter y mide ventanas 400ms/3s/acumulativo.
        if (latest.lufsIntegrated > -99.0f) {
            lufsMeter_.setIntegrated(latest.lufsIntegrated);
        }
        if (latest.lufsShortTerm > -99.0f) {
            lufsMeter_.setShortTerm(latest.lufsShortTerm);
        }
        if (latest.lufsMomentary > -99.0f) {
            lufsMeter_.setMomentary(latest.lufsMomentary);
        }
        if (latest.peakLeft > -99.0f || latest.peakRight > -99.0f) {
            lufsMeter_.setTruePeak(juce::jmax(latest.peakLeft, latest.peakRight));
        }
        if (latest.loudnessRange > 0.0f) {
            lufsMeter_.setRange(latest.loudnessRange);
        }

        // ─── Crest Factor histogram (dato REAL del TelemetryCollector) ───
        if (latest.crestFactor > 0.0f && latest.peakLeft > -60.0f) {
            // Usar el crest factor calculado directamente en el Messenger
            // crestFactor = peakDb - rmsDb
            float rmsAvg = (latest.rmsLeft + latest.rmsRight) * 0.5f;
            crestHistogram_.pushCrest(latest.peakLeft, rmsAvg);
        }
    }

    // ─── 5. Actualizar track selector con colores de cada pista ──────────
    // Se actualiza vía trackSelector_.updateTracks() al inicio
}

} // namespace mixcoach
