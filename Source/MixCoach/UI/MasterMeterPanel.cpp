#include "MasterMeterPanel.h"
#include <cmath>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  Presets data
// ═══════════════════════════════════════════════════════════════════════════
constexpr MasterMeterPanel::PresetInfo MasterMeterPanel::kPresets_[6];

// ═══════════════════════════════════════════════════════════════════════════
//  Constructor
// ═══════════════════════════════════════════════════════════════════════════
MasterMeterPanel::MasterMeterPanel()
{
    setOpaque(false);
    applyPreset(activePreset_);
}

// ═══════════════════════════════════════════════════════════════════════════
//  applyPreset — Aplica los targets del preset seleccionado
// ═══════════════════════════════════════════════════════════════════════════
void MasterMeterPanel::applyPreset(Preset p)
{
    activePreset_ = p;
    int idx = static_cast<int>(p);
    if (idx >= 0 && idx < 6) {
        loudnessTarget_ = kPresets_[idx].targetLUFS;
        peakTarget_     = kPresets_[idx].targetPeak;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  updateMeters — Lee valores del AudioAnalyzer maestro
// ═══════════════════════════════════════════════════════════════════════════
void MasterMeterPanel::updateMeters(const AudioAnalyzer& analyzer)
{
    rawMomentary_  = analyzer.getMomentaryLUFS();
    rawShortTerm_  = analyzer.getShortTermLUFS();
    rawIntegrated_ = analyzer.getIntegratedLUFS();
    rawTruePeak_   = analyzer.getTruePeakDBTP();
    rawLRA_        = analyzer.getLoudnessRange();

    momentaryLUFS_.setTargetValue(rawMomentary_);
    shortTermLUFS_.setTargetValue(rawShortTerm_);
    integratedLUFS_.setTargetValue(rawIntegrated_);
    truePeakDBTP_.setTargetValue(rawTruePeak_);
    loudnessRange_.setTargetValue(rawLRA_);
}

// ═══════════════════════════════════════════════════════════════════════════
//  advanceVisuals — Interpola valores suavizados (60fps)
// ═══════════════════════════════════════════════════════════════════════════
void MasterMeterPanel::advanceVisuals(double sampleRateHz)
{
    bool dirty = false;
    dirty |= momentaryLUFS_.advance(sampleRateHz);
    dirty |= shortTermLUFS_.advance(sampleRateHz);
    dirty |= integratedLUFS_.advance(sampleRateHz);
    dirty |= truePeakDBTP_.advance(sampleRateHz);
    dirty |= loudnessRange_.advance(sampleRateHz);
    if (dirty)
        repaint();
}

// ═══════════════════════════════════════════════════════════════════════════
//  resized
// ═══════════════════════════════════════════════════════════════════════════
void MasterMeterPanel::resized()
{
    // Layout tracking for click areas is handled in paint()
    // by storing the relevant bounds during draw calls
}

// ═══════════════════════════════════════════════════════════════════════════
//  paint — Panel completo estilo Youlean / Insight 2
// ═══════════════════════════════════════════════════════════════════════════
void MasterMeterPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    drawBackground(g, bounds);

    auto area = bounds.reduced(4, 3);

    // ─── TITLE row ───────────────────────────────────────────────────────
    area.removeFromTop(3);
    auto titleArea = area.removeFromTop(20);
    drawTitle(g, titleArea);

    // ─── 3-COLUMN METRICS (Short-Term | Integrated | Momentary) ─────────
    auto metricsArea = area.removeFromTop(92);
    drawMetricsRow(g, metricsArea);

    // ─── SECONDARY ROW (LRA | True Peak) ────────────────────────────────
    auto secondaryArea = area.removeFromTop(18);
    drawSecondaryRow(g, secondaryArea);

    // ─── SEPARATOR line ──────────────────────────────────────────────────
    {
        juce::Rectangle<int> sepArea(
            area.getX(), area.getY(),
            area.getWidth(), 1);
        g.setColour(MixCoachTheme::divider().withAlpha(0.25f));
        g.fillRect(sepArea);
        area.removeFromTop(3);
    }

    // ─── CONTROL BAR (presets, targets, gate) ───────────────────────────
    auto controlArea = area.removeFromTop(34);
    drawControlBar(g, controlArea);
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawBackground — Fondo glass premium con sombra, usando MixCoachTheme
// ═══════════════════════════════════════════════════════════════════════════
void MasterMeterPanel::drawBackground(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    auto b = bounds.toFloat();
    const float radius = 6.0f;

    // ─── Shadow exterior (2 capas de profundidad) ────────────────────────
    g.setColour(juce::Colours::black.withAlpha(0.15f));
    g.fillRoundedRectangle(b.expanded(2.0f, 2.5f), radius + 1.0f);
    g.setColour(juce::Colours::black.withAlpha(0.08f));
    g.fillRoundedRectangle(b.expanded(4.0f, 4.5f), radius + 2.0f);

    // ─── Base fill ──────────────────────────────────────────────────────
    g.setColour(MixCoachTheme::bgPanel());
    g.fillRoundedRectangle(b, radius);

    // ─── Glass highlight superior ──────────────────────────────────────
    auto glassH = b.withHeight(b.getHeight() * 0.35f);
    juce::ColourGradient glassGrad(
        juce::Colours::white.withAlpha(0.06f), glassH.getX(), glassH.getY(),
        juce::Colour(0x00000000),              glassH.getX(), glassH.getBottom(), false);
    g.setGradientFill(glassGrad);
    g.fillRoundedRectangle(glassH, radius);

    // ─── Border sutil (usando MixCoachTheme) ────────────────────────────
    g.setColour(MixCoachTheme::border());
    g.drawRoundedRectangle(b, radius, 0.8f);
    g.setColour(MixCoachTheme::glassEdge().withAlpha(0.3f));
    g.drawHorizontalLine((int)b.getY() + 1,
                         (int)b.getX() + 4,
                         (int)b.getRight() - 4);
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawTitle — "MASTER LOUDNESS" centrado
// ═══════════════════════════════════════════════════════════════════════════
void MasterMeterPanel::drawTitle(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setFont(MixCoachTheme::sectionHeaderFont());
    g.setColour(MixCoachTheme::accentGlow());
    g.drawText("MASTER LOUDNESS", bounds, juce::Justification::centred);
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawMetricsRow — 3 columnas: Short-Term / Integrated / Momentary
// ═══════════════════════════════════════════════════════════════════════════
void MasterMeterPanel::drawMetricsRow(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    int w = bounds.getWidth();
    int colW = w / 3;
    int x0 = bounds.getX();
    int y0 = bounds.getY();

    float shortVal = shortTermLUFS_.getCurrent();
    float intVal   = integratedLUFS_.getCurrent();
    float momVal   = momentaryLUFS_.getCurrent();

    // Columna 1: Short-Term (yellow #FFD93D)
    {
        auto cell = juce::Rectangle<float>((float)x0, (float)y0, (float)colW, (float)bounds.getHeight());
        drawBigValue(g, cell, "SHORT-TERM", shortVal, "LUFS",
                     juce::Colour(0xFFFFD93D), false);
    }

    // Columna 2: Integrated — DOMINANTE (red/pink #FF5C74, 70px font)
    {
        auto cell = juce::Rectangle<float>((float)(x0 + colW), (float)y0, (float)colW, (float)bounds.getHeight());
        drawBigValue(g, cell, "INTEGRATED", intVal, "LUFS",
                     juce::Colour(0xFFFF5C74), true);
    }

    // Columna 3: Momentary (green #4ADE80)
    {
        auto cell = juce::Rectangle<float>((float)(x0 + colW * 2), (float)y0, (float)colW, (float)bounds.getHeight());
        drawBigValue(g, cell, "MOMENTARY", momVal, "LUFS",
                     juce::Colour(0xFF4ADE80), false);
    }

    // ─── Vertical separators between columns ──────────────────────────
    int midY1 = y0 + 12;
    int midY2 = y0 + bounds.getHeight() - 4;
    g.setColour(MixCoachTheme::divider().withAlpha(0.4f));
    g.drawVerticalLine(x0 + colW, (float)midY1, (float)midY2);
    g.drawVerticalLine(x0 + colW * 2, (float)midY1, (float)midY2);
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawBigValue — Un valor grande con label arriba y unidad abajo
//
//  Layout:
//    [label]       ← 10px, gray (#AAAAAA)
//    [−15.0]       ← 50-70px, coloured (tabular figures)
//    [LUFS]        ← 10px, gray
// ═══════════════════════════════════════════════════════════════════════════
void MasterMeterPanel::drawBigValue(juce::Graphics& g, juce::Rectangle<float> cell,
                                     const juce::String& label, float value,
                                     const juce::String& unit, juce::Colour colour,
                                     bool dominant)
{
    float padding = 4.0f;
    auto inner = cell.reduced(padding, 2.0f);

    // ─── Label (top) ──────────────────────────────────────────────────
    auto labelArea = inner.removeFromTop(12.0f);
    g.setFont(interFont(MixCoachTheme::fontSizeTiny).boldened());
    g.setColour(MixCoachTheme::textDim());
    g.drawText(label, labelArea, juce::Justification::centred);

    // ─── Big value (center) ───────────────────────────────────────────
    float fontSize = dominant ? 62.0f : 48.0f;
    // Adjust font size to fit width
    float maxW = inner.getWidth() - 4.0f;
    while (fontSize > 12.0f) {
        auto testFont = interFont(fontSize).boldened();
        juce::GlyphArrangement ga;
        ga.addLineOfText(testFont, juce::String(value, 1), 0.0f, 0.0f);
        auto textW = ga.getBoundingBox(0, ga.getNumGlyphs(), true).getWidth();
        if (textW <= maxW)
            break;
        fontSize -= 2.0f;
    }

    // Value text
    juce::String valStr;
    if (value > -60.0f)
        valStr = juce::String(value, 1);
    else
        valStr = "--.-";

    // Add + sign for positive values
    if (value > 0.0f && value < 60.0f)
        valStr = "+" + valStr;

    g.setFont(interFont(fontSize).boldened());
    g.setColour(colour);
    g.drawText(valStr, inner, juce::Justification::centred);

    // ─── Glow behind the dominant number (Integrated) ─────────────────
    if (dominant && value > -60.0f) {
        auto glowBounds = inner.expanded(8.0f, 2.0f);
        g.setColour(colour.withAlpha(0.06f));
        g.fillRoundedRectangle(glowBounds, 4.0f);
    }

    // ─── Unit (bottom) ────────────────────────────────────────────────
    auto unitArea = inner.removeFromBottom(12.0f);
    g.setFont(interFont(MixCoachTheme::fontSizeTiny));
    g.setColour(MixCoachTheme::textMuted());
    g.drawText(unit, unitArea, juce::Justification::centred);
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawSecondaryRow — LRA (left) | True Peak (right)
// ═══════════════════════════════════════════════════════════════════════════
void MasterMeterPanel::drawSecondaryRow(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    int w = bounds.getWidth();
    int x0 = bounds.getX();
    int y0 = bounds.getY();

    float lra = loudnessRange_.getCurrent();
    float tp  = truePeakDBTP_.getCurrent();
    float intLUFS = integratedLUFS_.getCurrent();

    // ─── LRA (left-aligned) ─────────────────────────────────────────────
    g.setFont(interFont(9.0f).boldened());
    g.setColour(MixCoachTheme::textDim());
    auto lraArea = bounds.removeFromLeft(w / 2);
    juce::String lraStr = (lra > 0.1f)
        ? "LRA " + juce::String((int)lra) + " LU"
        : "LRA -- LU";
    g.drawText(lraStr, lraArea.reduced(6, 0), juce::Justification::centredLeft);

    // ─── True Peak (right-aligned) ──────────────────────────────────────
    auto tpArea = bounds.reduced(6, 0);
    // Colour según nivel: red > -1dBTP, yellow > -3dBTP, else gray
    juce::Colour tpColour = MixCoachTheme::textDim();
    if (tp > -1.0f)
        tpColour = juce::Colour(0xFFFF5C74);  // Red
    else if (tp > -3.0f)
        tpColour = juce::Colour(0xFFFFD93D);  // Yellow

    g.setFont(interFont(9.0f).boldened());
    g.setColour(tpColour);
    juce::String tpStr = (tp > -60.0f)
        ? "TP " + juce::String(tp, 1) + " dBTP"
        : "TP --.- dBTP";
    g.drawText(tpStr, tpArea, juce::Justification::centredRight);

    // ─── Integrated vs Target delta (center, between LRA and TP) ───────
    float delta = intLUFS - loudnessTarget_;
    if (intLUFS > -60.0f && std::abs(delta) < 15.0f) {
        auto deltaArea = bounds.reduced(50, 0);
        juce::String deltaStr;
        juce::Colour deltaColour;

        if (delta > 1.0f) {
            deltaStr = "+" + juce::String(delta, 1);
            deltaColour = juce::Colour(0xFFFF5C74);
        } else if (delta < -1.0f) {
            deltaStr = juce::String(delta, 1);
            deltaColour = juce::Colour(0xFF4ADE80);
        } else {
            deltaStr = "\u0394" + juce::String(delta, 1);
            deltaColour = juce::Colour(0xFFFFD93D);
        }

        g.setFont(interFont(MixCoachTheme::fontSizeExtraTiny));
        g.setColour(deltaColour.withAlpha(0.7f));
        g.drawText(deltaStr + " LU", deltaArea, juce::Justification::centred);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  drawControlBar — Presets | Peak target | LUFS target | Gate toggle
//
//  Layout:
//  ┌──────────────────────────────────────────────────────┐
//  │ [Streaming ▼]  Peak: -1.0  Loud: -14 LUFS  [ Gate ]│
//  └──────────────────────────────────────────────────────┘
// ═══════════════════════════════════════════════════════════════════════════
void MasterMeterPanel::drawControlBar(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    int x0 = bounds.getX();
    int y0 = bounds.getY();
    int h = bounds.getHeight();
    int w = bounds.getWidth();

    // ─── Background sutil ──────────────────────────────────────────────
    g.setColour(MixCoachTheme::bgInput());
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

    // ─── 1. PRESET selector (left) ────────────────────────────────────
    int presetW = juce::jmin(100, w / 4);
    presetHitArea_ = juce::Rectangle<int>(x0 + 4, y0 + 3, presetW, h - 6);

    juce::String presetName = kPresets_[static_cast<int>(activePreset_)].name;

    // Background
    g.setColour(MixCoachTheme::bgSurface().withAlpha(0.6f));
    g.fillRoundedRectangle(presetHitArea_.toFloat(), 3.0f);

    // Hover highlight
    juce::Point<int> mousePos = getMouseXYRelative();
    if (presetHitArea_.contains(mousePos)) {
        g.setColour(MixCoachTheme::accentBg().withAlpha(0.3f));
        g.fillRoundedRectangle(presetHitArea_.toFloat(), 3.0f);
    }

    // Border
    g.setColour(MixCoachTheme::divider().withAlpha(0.5f));
    g.drawRoundedRectangle(presetHitArea_.toFloat(), 3.0f, 0.7f);

    // Text + dropdown arrow
    g.setFont(interFont(MixCoachTheme::fontSizeTiny).boldened());
    g.setColour(MixCoachTheme::textDim());
    g.drawText(presetName + " \u25BC",
               presetHitArea_, juce::Justification::centred);

    // ─── 2. PEAK target ───────────────────────────────────────────────
    int peakW = juce::jmin(72, w / 5);
    peakTargetArea_ = juce::Rectangle<int>(
        presetHitArea_.getRight() + 6, y0 + 3, peakW, h - 6);

    g.setFont(interFont(MixCoachTheme::fontSizeExtraTiny).boldened());
    g.setColour(MixCoachTheme::textMuted());
    g.drawText("Peak:", peakTargetArea_.reduced(4, 0),
               juce::Justification::centredLeft);

    g.setFont(interFont(MixCoachTheme::fontSizeTiny).boldened());
    g.setColour(MixCoachTheme::textDim());
    g.drawText(juce::String(peakTarget_, 1) + " dB",
               peakTargetArea_, juce::Justification::centredRight);

    // ─── 3. LOUDNESS target ───────────────────────────────────────────
    int loudW = juce::jmin(82, w / 5);
    lufsTargetArea_ = juce::Rectangle<int>(
        peakTargetArea_.getRight() + 6, y0 + 3, loudW, h - 6);

    g.setFont(interFont(MixCoachTheme::fontSizeExtraTiny).boldened());
    g.setColour(MixCoachTheme::textMuted());
    g.drawText("Loud:", lufsTargetArea_.reduced(4, 0),
               juce::Justification::centredLeft);

    g.setFont(interFont(MixCoachTheme::fontSizeTiny).boldened());
    auto loudTargetColour = juce::Colour(0xFF4ADE80);
    g.setColour(loudTargetColour);
    g.drawText(juce::String((int)loudnessTarget_) + " LUFS",
               lufsTargetArea_, juce::Justification::centredRight);

    // ─── 4. GATE toggle (right) ──────────────────────────────────────
    int gateW = 48;
    gateHitArea_ = juce::Rectangle<int>(
        x0 + w - gateW - 4, y0 + 3, gateW, h - 6);

    if (gateEnabled_) {
        g.setColour(MixCoachTheme::success().withAlpha(0.15f));
        g.fillRoundedRectangle(gateHitArea_.toFloat(), 3.0f);
        g.setColour(MixCoachTheme::success().withAlpha(0.6f));
        g.drawRoundedRectangle(gateHitArea_.toFloat(), 3.0f, 0.8f);
        g.setColour(MixCoachTheme::success());
    } else {
        g.setColour(MixCoachTheme::bgSurface().withAlpha(0.6f));
        g.fillRoundedRectangle(gateHitArea_.toFloat(), 3.0f);
        g.setColour(MixCoachTheme::divider().withAlpha(0.5f));
        g.drawRoundedRectangle(gateHitArea_.toFloat(), 3.0f, 0.7f);
        g.setColour(MixCoachTheme::textMuted());
    }

    g.setColour(MixCoachTheme::textMuted());
    g.drawText("Gate", gateHitArea_, juce::Justification::centred);

    // ─── Separator lines between control sections ─────────────────────
    g.setColour(MixCoachTheme::divider().withAlpha(0.4f));
    g.drawVerticalLine(presetHitArea_.getRight() + 3, (float)(y0 + 6), (float)(y0 + h - 6));
    g.drawVerticalLine(peakTargetArea_.getRight() + 3, (float)(y0 + 6), (float)(y0 + h - 6));
    g.drawVerticalLine(lufsTargetArea_.getRight() + 3, (float)(y0 + 6), (float)(y0 + h - 6));
}

// ═══════════════════════════════════════════════════════════════════════════
//  showPresetPopup — PopupMenu oscuro con los 6 presets (flota sobre todo)
// ═══════════════════════════════════════════════════════════════════════════
void MasterMeterPanel::showPresetPopup()
{
    juce::PopupMenu menu;

    // ─── Items ──────────────────────────────────────────────────────────
    for (int i = 0; i < 6; ++i) {
        bool isActive = (i == static_cast<int>(activePreset_));
        juce::String label = juce::String(kPresets_[i].name);
        juce::String suffix = isActive ? " [x]" : "";
        juce::String targetStr = " (" + juce::String((int)kPresets_[i].targetLUFS) + " LUFS)";

        menu.addItem(i + 1, label + suffix + targetStr, true, isActive);
    }

    // ─── Dark theme colours via LookAndFeel ─────────────────────────────
    auto& laf = juce::LookAndFeel::getDefaultLookAndFeel();
    laf.setColour(juce::PopupMenu::backgroundColourId,            juce::Colour(0xFF1A1A2A));
    laf.setColour(juce::PopupMenu::textColourId,                  juce::Colour(0xFFCCCCCC));
    laf.setColour(juce::PopupMenu::highlightedBackgroundColourId,  juce::Colour(0xFF2A2A4A));
    laf.setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colour(0xFFFFD93D));

    // ─── Show at preset button position (JUCE 8 async API) ─────────────
    auto screenRect = localAreaToGlobal(presetHitArea_);
    menu.showMenuAsync(
        juce::PopupMenu::Options()
            .withTargetScreenArea(screenRect)
            .withMinimumWidth(presetHitArea_.getWidth() + 60)
            .withStandardItemHeight(22),
        [this](int result) {
            if (result > 0) {
                applyPreset(static_cast<Preset>(result - 1));
                repaint();
            }
        });
}

// ═══════════════════════════════════════════════════════════════════════════
//  mouseDown — Maneja clics en preset, gate, etc.
// ═══════════════════════════════════════════════════════════════════════════
void MasterMeterPanel::mouseDown(const juce::MouseEvent& e)
{
    auto pos = e.getPosition();

    // ─── Preset dropdown click ─────────────────────────────────────────
    if (presetHitArea_.contains(pos)) {
        showPresetPopup();
        return;
    }

    // ─── Gate toggle ──────────────────────────────────────────────────
    if (gateHitArea_.contains(pos)) {
        gateEnabled_ = !gateEnabled_;
        repaint();
        return;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  mouseMove — Cursor changes, repaint on preset hover state change
// ═══════════════════════════════════════════════════════════════════════════
void MasterMeterPanel::mouseMove(const juce::MouseEvent& e)
{
    auto pos = e.getPosition();

    bool overPreset = presetHitArea_.contains(pos);
    bool overGate   = gateHitArea_.contains(pos);

    // Only repaint if preset hover state actually changed
    if (overPreset != presetHovered_) {
        presetHovered_ = overPreset;
        repaint();
    }

    setMouseCursor((overPreset || overGate)
        ? juce::MouseCursor::PointingHandCursor
        : juce::MouseCursor::NormalCursor);
}

} // namespace mixcoach
