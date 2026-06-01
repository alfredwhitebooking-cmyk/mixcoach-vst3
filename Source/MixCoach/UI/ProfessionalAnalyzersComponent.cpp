#include "ProfessionalAnalyzersComponent.h"
#include "../../Common/types/Constants.h"

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  ProfessionalAnalyzersComponent
// ═══════════════════════════════════════════════════════════════════════════

ProfessionalAnalyzersComponent::ProfessionalAnalyzersComponent()
{
    try
    {
        // ─── Header ──────────────────────────────────────────────────────
        headerLabel_.setText(juce::CharPointer_UTF8("\\xF0\\x9F\\x94\\x8A SYSTEM ANALYZER"),
                             juce::dontSendNotification);
        headerLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTitle)).boldened());
        headerLabel_.setJustificationType(juce::Justification::centredLeft);
        headerLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textBright());
        addAndMakeVisible(headerLabel_);

        // ─── Info label (selected track) ─────────────────────────────────
        infoLabel_.setText(juce::CharPointer_UTF8("\\xF0\\x9F\\x90\\xBB Selecciona un Messenger en el panel Mix Coach"),
                           juce::dontSendNotification);
        infoLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeHeader)).boldened());
        infoLabel_.setJustificationType(juce::Justification::centredLeft);
        infoLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
        addAndMakeVisible(infoLabel_);

        // ─── Vectorscope ─────────────────────────────────────────────────
        vectorscope_ = std::make_unique<VectorscopeSystem>();
        addAndMakeVisible(vectorscope_.get());

        // ─── Phase Correlation ───────────────────────────────────────────
        phaseMeter_ = std::make_unique<PhaseCorrelationSystem>();
        addAndMakeVisible(phaseMeter_.get());
    }
    catch (const std::exception& e)
    {
        juce::Logger::outputDebugString("[ProfessionalAnalyzersComponent] Exception: "
                                         + juce::String(e.what()));
    }
    catch (...)
    {
        juce::Logger::outputDebugString("[ProfessionalAnalyzersComponent] Unknown exception");
    }
}

void ProfessionalAnalyzersComponent::resized()
{
    auto area = getLocalBounds().reduced(6);

    // Header
    headerLabel_.setBounds(area.removeFromTop(24));

    // Info row
    infoLabel_.setBounds(area.removeFromTop(20));

    // ─── Split: Vectorscope (left, ~62%) | Phase Correlation (right, ~38%) ──
    auto leftArea  = area.removeFromLeft(static_cast<int>(area.getWidth() * 0.62f));
    auto rightArea = area.reduced(2, 0);

    vectorscope_->setBounds(leftArea.reduced(1));
    phaseMeter_->setBounds(rightArea.reduced(1));
}

void ProfessionalAnalyzersComponent::paint(juce::Graphics& g)
{
    // ─── Fondo oscuro con gradiente (estilo rack profesional) ───
    juce::ColourGradient bgGrad(
        MixCoachTheme::bgDark(),
        juce::Point<float>(0.0f, 0.0f),
        MixCoachTheme::bgDarker(),
        juce::Point<float>(0.0f, static_cast<float>(getHeight())),
        false);
    g.setGradientFill(bgGrad);
    g.fillRect(getLocalBounds());

    // ─── Grid pattern sutil ───
    g.setColour(MixCoachTheme::border().withAlpha(0.03f));
    for (int gy = 0; gy < getHeight(); gy += 24) {
        for (int gx = 0; gx < getWidth(); gx += 24) {
            g.fillRect(gx, gy, 1, 1);
        }
    }

    // ─── Borde superior neón ───
    juce::ColourGradient topLine(
        MixCoachTheme::accent().withAlpha(0.2f),
        juce::Point<float>(0.0f, 0.0f),
        MixCoachTheme::accent().withAlpha(0.0f),
        juce::Point<float>(static_cast<float>(getWidth()), 0.0f),
        false);
    g.setGradientFill(topLine);
    g.fillRect(0, 0, getWidth(), 1);
    g.setColour(MixCoachTheme::border().withAlpha(0.25f));
    g.fillRect(0, 1, getWidth(), 1);
}

void ProfessionalAnalyzersComponent::updateAnalyzers(SlotRegistry& registry)
{
    // ─── Buscar el primer slot activo ────────────────────────────────────
    if (registry.activeCount() == 0) {
        currentSlotIndex_ = -1;
        infoLabel_.setText(juce::CharPointer_UTF8("\\xF0\\x9F\\x90\\xBB Conecta un Messenger para ver el analisis"),
                           juce::dontSendNotification);
        infoLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textMuted());
        return;
    }

    // Auto-select first active slot if none selected
    if (currentSlotIndex_ < 0) {
        registry.forEachActive([&](const SlotInfo& info) {
            if (currentSlotIndex_ < 0) {
                currentSlotIndex_ = info.slotIndex;
                currentTrackName_ = juce::String(info.trackName);
                currentColour_ = info.colour;
            }
        });
    }

    if (currentSlotIndex_ < 0)
        return;

    // ─── Leer telemetría del slot seleccionado ────────────────────────────
    auto& telem = registry.getTelemetry(currentSlotIndex_);
    auto latest = telem.latest();

    // ─── Actualizar info label ────────────────────────────────────────────
    juce::String trackInfo = juce::String(juce::CharPointer_UTF8("\\xF0\\x9F\\x8E\\xB5 "))
                             + currentTrackName_
                             + juce::String(juce::CharPointer_UTF8(" \\xE2\\x80\\xA2 Slot #"))
                             + juce::String(currentSlotIndex_);
    infoLabel_.setText(trackInfo, juce::dontSendNotification);
    infoLabel_.setColour(juce::Label::textColourId, currentColour_);

    // ─── Vectorscope: push sample L/R ───────────────────────────────────
    if (latest.active || latest.peakLeft > -60.0f) {
        float vectL = juce::jlimit(-1.0f, 1.0f, latest.sampleL);
        float vectR = juce::jlimit(-1.0f, 1.0f, latest.sampleR);
        vectorscope_->pushSample(vectL, vectR);
        vectorscope_->setCorrelation(latest.correlation);
        vectorscope_->setSlotInfo(currentSlotIndex_, currentColour_, currentTrackName_);
    }

    // ─── Phase Correlation meter ─────────────────────────────────────────
    phaseMeter_->setCorrelation(latest.correlation);
}

// ═══════════════════════════════════════════════════════════════════════════
//  VectorscopeSystem
// ═══════════════════════════════════════════════════════════════════════════

VectorscopeSystem::VectorscopeSystem()
{
    // ─── Title ────────────────────────────────────────────────────────────
    titleLabel_.setText(juce::CharPointer_UTF8("\\xE2\\x9C\\xA6 Vectorscope"),
                        juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeSmall)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textPrimary());
    addAndMakeVisible(titleLabel_);

    // ─── Correlation value ────────────────────────────────────────────────
    corrValueLabel_.setText("\\xCF\\x86: +1.00", juce::dontSendNotification);
    corrValueLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
    corrValueLabel_.setJustificationType(juce::Justification::centredRight);
    corrValueLabel_.setColour(juce::Label::textColourId, MixCoachTheme::success());
    addAndMakeVisible(corrValueLabel_);

    // ─── Track name ───────────────────────────────────────────────────────
    trackLabel_.setText("--", juce::dontSendNotification);
    trackLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)));
    trackLabel_.setJustificationType(juce::Justification::centredLeft);
    trackLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    addAndMakeVisible(trackLabel_);

    // ═══ Timer VISUAL a 30fps ════════════════════════════════════════
    // startTimerHz aquí + en visibilityChanged() como safety restart
    startTimerHz(30);
}

VectorscopeSystem::~VectorscopeSystem()
{
    stopTimer();
}

void VectorscopeSystem::visibilityChanged()
{
    if (isVisible())
        startTimerHz(30);
    else
        stopTimer();
}

void VectorscopeSystem::timerCallback()
{
    if (!isVisible())
        return;

    // ─── Smooth correlation ──────────────────────────────────────────────
    float diff = correlation_ - smoothCorrelation_;
    if (std::abs(diff) > 0.001f) {
        float coeff = (diff > 0.0f) ? 0.30f : 0.08f;
        smoothCorrelation_ += diff * coeff;
    }

    // ─── Age all points ──────────────────────────────────────────────────
    for (auto& pt : points_)
        if (pt.age < 9999) pt.age++;

    // ─── Age phosphor trail ──────────────────────────────────────────────
    for (auto& pt : phosphorTrail_)
        pt.age++;

    // Remove aged-out phosphor points
    while (!phosphorTrail_.empty() && phosphorTrail_.front().age > kPhosphorSteps)
        phosphorTrail_.pop_front();

    // ─── Update correlation label color ──────────────────────────────────
    juce::Colour corrColour;
    float sc = smoothCorrelation_;
    if (sc < -0.3f)
        corrColour = MixCoachTheme::error();
    else if (sc < 0.0f)
        corrColour = MixCoachTheme::warning();
    else if (sc < 0.5f)
        corrColour = MixCoachTheme::warning().interpolatedWith(MixCoachTheme::success(), (sc + 0.3f) / 0.8f);
    else
        corrColour = MixCoachTheme::success();

    // Animar pulso para advertencia de fase
    corrValueLabel_.setColour(juce::Label::textColourId, corrColour);
    corrValueLabel_.setText("\\xCF\\x86: " + juce::String(smoothCorrelation_, 2),
                            juce::dontSendNotification);

    repaint();
}

void VectorscopeSystem::pushSample(float left, float right)
{
    // Clamp
    left  = juce::jlimit(-1.0f, 1.0f, left);
    right = juce::jlimit(-1.0f, 1.0f, right);

    // Only add if signal present
    if (std::abs(left) < 0.001f && std::abs(right) < 0.001f)
        return;

    // Write to circular buffer
    auto& pt = points_[writePos_ % kTraceLen];
    pt.x = left;
    pt.y = right;
    pt.age = 0;
    writePos_ = (writePos_ + 1) % kTraceLen;
    if (pointCount_ < kTraceLen) pointCount_++;

    // Add to phosphor trail
    TracePoint phosphorPt{ left, right, 0 };
    phosphorTrail_.push_back(phosphorPt);
    if (phosphorTrail_.size() > static_cast<size_t>(kMaxPhosphor))
        phosphorTrail_.pop_front();
}

void VectorscopeSystem::setSlotInfo(int slotIndex, const juce::Colour& colour, const juce::String& trackName)
{
    slotIndex_ = slotIndex;
    slotColour_ = colour;
    trackName_ = trackName;
    trackLabel_.setText(trackName, juce::dontSendNotification);
    trackLabel_.setColour(juce::Label::textColourId, colour);
}

void VectorscopeSystem::resized()
{
    auto area = getLocalBounds().reduced(2);
    auto topBar = area.removeFromTop(16);
    titleLabel_.setBounds(topBar.removeFromLeft(static_cast<int>(topBar.getWidth() * 0.5f)));
    corrValueLabel_.setBounds(topBar);
    trackLabel_.setBounds(area.removeFromBottom(16));
}

void VectorscopeSystem::drawBackground(juce::Graphics& g, juce::Rectangle<float> area)
{
    auto cx = area.getCentreX();
    auto cy = area.getCentreY();
    auto radius = std::min(area.getWidth(), area.getHeight()) * 0.5f - 6.0f;

    // ─── Fondo con gradiente radial (radar style) ────────────────────────
    juce::ColourGradient radialGrad(
        MixCoachTheme::bgElevated().withAlpha(0.6f),
        cx, cy,
        MixCoachTheme::bgDarker(),
        cx + radius, cy,
        false);
    radialGrad.addColour(0.4f, MixCoachTheme::bgElevated().withAlpha(0.3f));
    radialGrad.addColour(0.7f, MixCoachTheme::bgDarker().withAlpha(0.8f));
    g.setGradientFill(radialGrad);
    g.fillEllipse(area.reduced(4.0f));
}

void VectorscopeSystem::drawGrid(juce::Graphics& g, juce::Rectangle<float> area)
{
    auto cx = area.getCentreX();
    auto cy = area.getCentreY();
    auto outerR = std::min(area.getWidth(), area.getHeight()) * 0.5f - 6.0f;

    // ─── Outer circle ────────────────────────────────────────────────────
    g.setColour(MixCoachTheme::border().withAlpha(0.5f));
    g.drawEllipse(area.reduced(4.0f), 1.5f);

    // Glow on outer circle
    g.setColour(MixCoachTheme::accent().withAlpha(0.08f));
    g.drawEllipse(area.reduced(4.0f), 3.0f);

    // ─── Concentric circles ──────────────────────────────────────────────
    float innerRadii[] = { 0.75f, 0.50f, 0.25f };
    for (float r : innerRadii) {
        float ir = outerR * r;
        float alpha = (r == 0.75f) ? 0.3f : (r == 0.50f) ? 0.2f : 0.1f;
        g.setColour(MixCoachTheme::border().withAlpha(alpha));
        g.drawEllipse(cx - ir, cy - ir, ir * 2.0f, ir * 2.0f, 0.5f);
    }

    // ─── Crosshairs ──────────────────────────────────────────────────────
    g.setColour(MixCoachTheme::border().withAlpha(0.3f));
    float innerX1 = area.getX() + 6.0f;
    float innerX2 = area.getRight() - 6.0f;
    float innerY1 = area.getY() + 6.0f;
    float innerY2 = area.getBottom() - 6.0f;
    g.drawHorizontalLine(juce::roundToInt(cy), innerX1, innerX2);
    g.drawVerticalLine(juce::roundToInt(cx), innerY1, innerY2);

    // ─── Diagonal lines at 45° ───────────────────────────────────────────
    float d = outerR * 0.707f;
    g.setColour(MixCoachTheme::border().withAlpha(0.15f));
    g.drawLine(cx - d, cy - d, cx + d, cy + d, 0.5f);
    g.drawLine(cx - d, cy + d, cx + d, cy - d, 0.5f);

    // ─── Center dot ──────────────────────────────────────────────────────
    g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
    g.fillEllipse(cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);

    // Outer glow on center
    g.setColour(MixCoachTheme::accent().withAlpha(0.08f));
    g.fillEllipse(cx - 6.0f, cy - 6.0f, 12.0f, 12.0f);

    // ─── Channel labels ──────────────────────────────────────────────────
    g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
    // L - Left side
    g.setColour(MixCoachTheme::channelLeft().withAlpha(0.6f));
    g.drawText("L", juce::Rectangle<float>(area.getX() + 2, cy - 8, 14, 14),
               juce::Justification::centred);
    // R - Right side
    g.setColour(MixCoachTheme::channelRight().withAlpha(0.6f));
    g.drawText("R", juce::Rectangle<float>(area.getRight() - 16, cy - 8, 14, 14),
               juce::Justification::centred);
    // R - Top
    g.setColour(MixCoachTheme::channelRight().withAlpha(0.6f));
    g.drawText("R", juce::Rectangle<float>(cx - 8, area.getY() + 2, 16, 14),
               juce::Justification::centred);
    // L - Bottom
    g.setColour(MixCoachTheme::channelLeft().withAlpha(0.6f));
    g.drawText("L", juce::Rectangle<float>(cx - 8, area.getBottom() - 16, 16, 14),
               juce::Justification::centred);

    // ─── Axis tick marks ─────────────────────────────────────────────────
    g.setColour(MixCoachTheme::border().withAlpha(0.12f));
    float tickLen = 6.0f;
    for (float angle = 0; angle < 360.0f; angle += 30.0f) {
        if (std::abs(std::fmod(angle, 90.0f)) < 0.01f) continue; // skip cardinal
        float rad = juce::MathConstants<float>::pi * angle / 180.0f;
        float cosA = std::cos(rad);
        float sinA = std::sin(rad);
        float r1 = outerR - 2.0f;
        float r2 = outerR - 2.0f - tickLen;
        g.drawLine(cx + cosA * r1, cy + sinA * r1,
                   cx + cosA * r2, cy + sinA * r2, 0.5f);
    }
}

void VectorscopeSystem::drawPhosphorPoints(juce::Graphics& g, juce::Rectangle<float> area)
{
    auto cx = area.getCentreX();
    auto cy = area.getCentreY();
    auto radius = std::min(area.getWidth(), area.getHeight()) * 0.5f - 8.0f;

    if (radius < 10.0f) return;

    // ─── Color based on correlation ──────────────────────────────────────
    float sc = juce::jlimit(-1.0f, 1.0f, smoothCorrelation_);
    juce::Colour traceColour;
    float mix;
    if (sc > 0.5f) {
        // Green (centered)
        traceColour = MixCoachTheme::success();
    } else if (sc > 0.0f) {
        // Green → Yellow transition
        mix = (0.5f - sc) / 0.5f;
        traceColour = MixCoachTheme::success().interpolatedWith(MixCoachTheme::warning(), mix);
    } else if (sc > -0.3f) {
        // Yellow → Orange transition
        mix = (0.0f - sc) / 0.3f;
        traceColour = MixCoachTheme::warning().interpolatedWith(MixCoachTheme::warning().interpolatedWith(MixCoachTheme::error(), 0.5f), mix);
    } else {
        // Orange → Red (excessive phase issues)
        mix = (-0.3f - sc) / 0.7f;
        traceColour = MixCoachTheme::warning().interpolatedWith(MixCoachTheme::error(), 0.5f).interpolatedWith(MixCoachTheme::error(),
                                                                juce::jmin(1.0f, mix));
    }

    // ─── Draw phosphor trail (oldest = most transparent) ─────────────────
    for (const auto& pt : phosphorTrail_) {
        float ageAlpha = 1.0f - static_cast<float>(pt.age) / static_cast<float>(kPhosphorSteps);
        if (ageAlpha < 0.005f) continue;

        float sx = cx + pt.x * radius;
        float sy = cy - pt.y * radius; // Y inverted for display (positive up)

        // Clamp to area
        if (sx < area.getX() || sx > area.getRight() ||
            sy < area.getY() || sy > area.getBottom())
            continue;

        // Size varies with age (newer = larger)
        float pointSize = 1.5f + ageAlpha * 2.0f;

        // Alpha varies with age
        float alpha = ageAlpha * 0.7f;

        // Draw glow for newer points
        if (ageAlpha > 0.5f) {
            float glowSize = pointSize * 3.0f;
            g.setColour(traceColour.withAlpha(alpha * 0.15f));
            g.fillEllipse(sx - glowSize * 0.5f, sy - glowSize * 0.5f, glowSize, glowSize);
        }

        // Draw point
        g.setColour(traceColour.withAlpha(alpha));
        g.fillEllipse(sx - pointSize * 0.5f, sy - pointSize * 0.5f, pointSize, pointSize);
    }

    // ─── Draw Lissajous path (connecting recent points) ──────────────────
    if (pointCount_ >= 2) {
        juce::Path path;
        int pathStart = -1;
        for (int i = 0; i < kTraceLen; ++i) {
            int idx = (writePos_ + i) % kTraceLen;
            auto& pt = points_[idx];
            if (pt.age > 60) continue; // only very recent points for the line

            float sx = cx + pt.x * radius;
            float sy = cy - pt.y * radius;

            if (pathStart < 0) {
                path.startNewSubPath(sx, sy);
                pathStart = i;
            } else {
                path.lineTo(sx, sy);
            }
        }

        if (pathStart >= 0) {
            // Glow line
            g.setColour(traceColour.withAlpha(0.25f));
            g.strokePath(path, juce::PathStrokeType(3.0f));
            // Core line
            g.setColour(traceColour.withAlpha(0.6f));
            g.strokePath(path, juce::PathStrokeType(1.5f));
        }
    }

    // ─── Green reference ring (mono compatibility) ───────────────────────
    // Inner ring at ~0.5 shows where mono signals sit
    g.setColour(MixCoachTheme::success().withAlpha(0.06f));
    float refR = radius * 0.15f;
    g.drawEllipse(cx - refR, cy - refR, refR * 2.0f, refR * 2.0f, 0.5f);
}

void VectorscopeSystem::drawCorrelationIndicator(juce::Graphics& g, juce::Rectangle<float> area)
{
    // ─── Bottom indicator bar (subtle mini correlation meter) ─────────────
    auto barBounds = area.reduced(8, 0).withHeight(6.0f).translated(0, area.getHeight() + 4.0f);
    if (barBounds.getY() + barBounds.getHeight() > getHeight() - 22.0f)
        return;

    float sc = juce::jlimit(-1.0f, 1.0f, smoothCorrelation_);

    // Background
    g.setColour(MixCoachTheme::bgDarker().withAlpha(0.6f));
    g.fillRoundedRectangle(barBounds, 2.0f);

    // Gradient fill for correlation zones
    auto gradBounds = barBounds;
    juce::ColourGradient corrGrad(
        MixCoachTheme::error().withAlpha(0.2f),
        juce::Point<float>(gradBounds.getX(), 0.0f),
        MixCoachTheme::success().withAlpha(0.2f),
        juce::Point<float>(gradBounds.getRight(), 0.0f),
        false);
    corrGrad.addColour(0.25f, MixCoachTheme::warning().withAlpha(0.15f));
    corrGrad.addColour(0.5f, MixCoachTheme::textMuted().withAlpha(0.1f));
    corrGrad.addColour(0.75f, MixCoachTheme::success().withAlpha(0.15f));
    g.setGradientFill(corrGrad);
    g.fillRoundedRectangle(gradBounds, 2.0f);

    // Marker
    float norm = (sc + 1.0f) * 0.5f;
    float mx = barBounds.getX() + norm * barBounds.getWidth();
    juce::Colour markerColour;
    if (sc < -0.3f)      markerColour = MixCoachTheme::error();
    else if (sc < 0.0f)  markerColour = MixCoachTheme::warning();
    else                 markerColour = MixCoachTheme::success();

    g.setColour(markerColour);
    g.fillRect(mx - 1.5f, barBounds.getY() - 1.0f, 3.0f, barBounds.getHeight() + 2.0f);

    // Border
    g.setColour(MixCoachTheme::border().withAlpha(0.3f));
    g.drawRoundedRectangle(barBounds, 2.0f, 0.5f);
}

void VectorscopeSystem::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // ─── Glass panel background ──────────────────────────────────────────
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

    // ─── Content area (below title, above track label) ───────────────────
    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(18);  // title
    area.removeFromBottom(18); // track label

    auto gfxArea = area.toFloat();

    // ─── Draw layers ─────────────────────────────────────────────────────
    drawBackground(g, gfxArea);
    drawGrid(g, gfxArea);
    drawPhosphorPoints(g, gfxArea);
    drawCorrelationIndicator(g, gfxArea);
}

// ═══════════════════════════════════════════════════════════════════════════
//  PhaseCorrelationSystem
// ═══════════════════════════════════════════════════════════════════════════

PhaseCorrelationSystem::PhaseCorrelationSystem()
{
    // ─── Title ────────────────────────────────────────────────────────────
    titleLabel_.setText(juce::CharPointer_UTF8("\\xCF\\x86 Phase Correlation"),
                        juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeHeader)).boldened());
    titleLabel_.setJustificationType(juce::Justification::centred);
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textBright());
    addAndMakeVisible(titleLabel_);

    // ─── Value ───────────────────────────────────────────────────────────
    valueLabel_.setText("+1.00", juce::dontSendNotification);
    valueLabel_.setFont(juce::Font(juce::FontOptions(24.0f)).boldened());
    valueLabel_.setJustificationType(juce::Justification::centred);
    valueLabel_.setColour(juce::Label::textColourId, MixCoachTheme::success());
    addAndMakeVisible(valueLabel_);

    // ─── Warning labels (added in constructor, toggled via setVisible) ───
    lWarning_.setText(juce::CharPointer_UTF8("\\xE2\\x9A\\xA0 OUT OF PHASE"),
                      juce::dontSendNotification);
    lWarning_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)).boldened());
    lWarning_.setJustificationType(juce::Justification::centred);
    lWarning_.setColour(juce::Label::textColourId, MixCoachTheme::error());
    addAndMakeVisible(lWarning_);
    lWarning_.setVisible(false);

    rWarning_.setText(juce::CharPointer_UTF8("\\xE2\\x9A\\xA0 WIDE / UNUSUAL"),
                      juce::dontSendNotification);
    rWarning_.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeBody)).boldened());
    rWarning_.setJustificationType(juce::Justification::centred);
    rWarning_.setColour(juce::Label::textColourId, MixCoachTheme::warning());
    addAndMakeVisible(rWarning_);
    rWarning_.setVisible(false);

    // ═══ Timer visual a 30fps ════════════════════════════════════════════
    // startTimerHz aquí + en visibilityChanged() como safety restart
}

PhaseCorrelationSystem::~PhaseCorrelationSystem()
{
    stopTimer();
}

void PhaseCorrelationSystem::visibilityChanged()
{
    if (isVisible())
        startTimerHz(30);
    else
        stopTimer();
}

void PhaseCorrelationSystem::setCorrelation(float value)
{
    target_ = juce::jlimit(-1.0f, 1.0f, value);
}

void PhaseCorrelationSystem::timerCallback()
{
    if (!isVisible())
        return;

    // ─── Smooth interpolation ────────────────────────────────────────────
    float diff = target_ - current_;
    if (std::abs(diff) > 0.0005f) {
        float coeff = (diff > 0.0f) ? kAttackCoeff : kReleaseCoeff;
        current_ += diff * coeff;
    } else {
        current_ = target_;
    }

    // ─── Update value label ──────────────────────────────────────────────
    juce::Colour col = getCorrelationColour(current_);
    valueLabel_.setColour(juce::Label::textColourId, col);
    juce::String prefix = (current_ >= 0.0f) ? "+" : "";
    valueLabel_.setText(prefix + juce::String(current_, 2),
                        juce::dontSendNotification);

    // ─── Warning labels visibility (both added in constructor, just toggle) ─
    if (current_ < -0.2f) {
        lWarning_.setVisible(true);
        rWarning_.setVisible(false);
    } else if (current_ < 0.3f) {
        lWarning_.setVisible(false);
        rWarning_.setVisible(true);
    } else {
        lWarning_.setVisible(false);
        rWarning_.setVisible(false);
    }

    repaint();
}

juce::Colour PhaseCorrelationSystem::getCorrelationColour(float corr) const noexcept
{
    // -1.0 → Red, 0.0 → Yellow, +1.0 → Green
    if (corr < -0.3f)
        return MixCoachTheme::error();
    else if (corr < 0.0f)
        return MixCoachTheme::error().interpolatedWith(MixCoachTheme::warning(),
                                                         (corr + 0.3f) / 0.3f);
    else if (corr < 0.5f)
        return MixCoachTheme::warning().interpolatedWith(MixCoachTheme::success(),
                                                         corr / 0.5f);
    else
        return MixCoachTheme::success();
}

void PhaseCorrelationSystem::drawCorrelationBar(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // ─── Background ──────────────────────────────────────────────────────
    juce::ColourGradient bgGrad(
        MixCoachTheme::bgDarker(),
        juce::Point<float>(0.0f, bounds.getY()),
        MixCoachTheme::bgDark().withAlpha(0.5f),
        juce::Point<float>(0.0f, bounds.getBottom()),
        false);
    g.setGradientFill(bgGrad);
    g.fillRoundedRectangle(bounds, 6.0f);

    // ─── Phase gradient background ───────────────────────────────────────
    juce::ColourGradient phaseGrad(
        MixCoachTheme::error().withAlpha(0.15f),
        juce::Point<float>(bounds.getX(), 0.0f),
        MixCoachTheme::success().withAlpha(0.15f),
        juce::Point<float>(bounds.getRight(), 0.0f),
        false);
    phaseGrad.addColour(0.25f, MixCoachTheme::warning().withAlpha(0.12f));
    phaseGrad.addColour(0.5f,  MixCoachTheme::border().withAlpha(0.06f));
    phaseGrad.addColour(0.75f, MixCoachTheme::success().withAlpha(0.12f));
    g.setGradientFill(phaseGrad);
    g.fillRoundedRectangle(bounds, 6.0f);

    // ─── Zone markers ────────────────────────────────────────────────────
    struct ZoneMarker { float val; const char* label; };
    ZoneMarker markers[] = {
        { -1.0f, "-1" },
        { -0.5f, "-0.5" },
        {  0.0f, "0" },
        {  0.5f, "+0.5" },
        {  1.0f, "+1" }
    };

    g.setFont(juce::Font(juce::FontOptions(MixCoachTheme::fontSizeTiny)).boldened());
    for (auto& m : markers) {
        float norm = (m.val + 1.0f) * 0.5f;
        float mx = bounds.getX() + norm * bounds.getWidth();
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.3f));
        g.drawVerticalLine(juce::roundToInt(mx),
                           bounds.getY() + 4, bounds.getBottom() - 4);
        // Label
        g.setColour(MixCoachTheme::textMuted().withAlpha(0.5f));
        g.drawText(juce::String(m.label),
                   juce::Rectangle<float>(mx - 12, bounds.getBottom() + 2, 24, 12),
                   juce::Justification::centred);
    }

    // ─── Zone labels ─────────────────────────────────────────────────────
    g.setFont(juce::Font(juce::FontOptions(7.0f)));
    g.setColour(MixCoachTheme::error().withAlpha(0.4f));
    g.drawText("OUT OF PHASE",
               juce::Rectangle<float>(bounds.getX() + 4, bounds.getY() + 4,
                                      bounds.getWidth() * 0.3f, 14),
               juce::Justification::centredLeft);
    g.setColour(MixCoachTheme::success().withAlpha(0.4f));
    g.drawText("IN PHASE",
               juce::Rectangle<float>(bounds.getRight() - bounds.getWidth() * 0.25f,
                                      bounds.getY() + 4,
                                      bounds.getWidth() * 0.25f, 14),
               juce::Justification::centredLeft);

    // ─── Fill bar (active fill from center to marker) ────────────────────
    float norm = juce::jlimit(0.0f, 1.0f, (current_ + 1.0f) * 0.5f);
    if (norm > 0.0f) {
        float mx = bounds.getX() + norm * bounds.getWidth();
        juce::Rectangle<float> fillBounds;

        // Fill from left for negative, from center for positive
        if (current_ < 0.0f) {
            float centerX = bounds.getX() + 0.5f * bounds.getWidth();
            fillBounds = juce::Rectangle<float>(mx, bounds.getY() + 2,
                                                centerX - mx, bounds.getHeight() - 4);
        } else {
            float centerX = bounds.getX() + 0.5f * bounds.getWidth();
            fillBounds = juce::Rectangle<float>(centerX, bounds.getY() + 2,
                                                mx - centerX, bounds.getHeight() - 4);
        }

        if (fillBounds.getWidth() > 1.0f) {
            g.setColour(getCorrelationColour(current_).withAlpha(0.35f));
            g.fillRoundedRectangle(fillBounds, 4.0f);
        }
    }

    // ─── Border ──────────────────────────────────────────────────────────
    g.setColour(MixCoachTheme::border().withAlpha(0.5f));
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
}

void PhaseCorrelationSystem::drawValueIndicator(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    float norm = juce::jlimit(0.0f, 1.0f, (current_ + 1.0f) * 0.5f);
    float mx = bounds.getX() + norm * bounds.getWidth();
    float my = bounds.getCentreY();

    juce::Colour indicatorColour = getCorrelationColour(current_);

    // ─── Outer glow ──────────────────────────────────────────────────────
    for (int i = 3; i >= 0; --i) {
        float glowR = 16.0f + static_cast<float>(i) * 6.0f;
        g.setColour(indicatorColour.withAlpha(0.08f / static_cast<float>(i + 1)));
        g.fillEllipse(mx - glowR * 0.5f, my - glowR * 0.5f, glowR, glowR);
    }

    // ─── Diamond indicator ───────────────────────────────────────────────
    float sz = 10.0f;
    juce::Path diamond;
    diamond.startNewSubPath(mx, my - sz);
    diamond.lineTo(mx + sz * 0.7f, my);
    diamond.lineTo(mx, my + sz);
    diamond.lineTo(mx - sz * 0.7f, my);
    diamond.closeSubPath();

    // Fill
    g.setColour(indicatorColour);
    g.fillPath(diamond);

    // Inner highlight
    juce::Path innerDiamond;
    float isz = sz * 0.4f;
    innerDiamond.startNewSubPath(mx, my - isz);
    innerDiamond.lineTo(mx + isz * 0.7f, my);
    innerDiamond.lineTo(mx, my + isz);
    innerDiamond.lineTo(mx - isz * 0.7f, my);
    innerDiamond.closeSubPath();
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.fillPath(innerDiamond);

    // Stroke
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.strokePath(diamond, juce::PathStrokeType(1.0f));
}

void PhaseCorrelationSystem::resized()
{
    auto area = getLocalBounds().reduced(4);

    // Title at top
    titleLabel_.setBounds(area.removeFromTop(20));

    // Warning labels (always reserve 44px for layout consistency)
    lWarning_.setBounds(area.removeFromTop(22));
    rWarning_.setBounds(area.removeFromTop(22));

    // Value in center
    valueLabel_.setBounds(area.removeFromTop(40));

    // Correlation bar takes remaining space
    auto barArea = area.reduced(8, 0).toFloat();
    float barHeight = juce::jmin(60.0f, barArea.getHeight() * 0.5f);
    barArea = barArea.withHeight(barHeight).withCentre(barArea.getCentre());
}

void PhaseCorrelationSystem::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // ─── Glass panel background ──────────────────────────────────────────
    MixCoachTheme::fillGlassPanel(g, bounds, 6.0f);

    // ─── Main bar area (mismo offset que resized(): 20+22+22+40=104px) ──
    auto area = getLocalBounds().reduced(4);
    area.removeFromTop(20);   // title
    area.removeFromTop(22);   // lWarning
    area.removeFromTop(22);   // rWarning
    area.removeFromTop(40);   // value label

    auto barBounds = area.reduced(8, 4).toFloat();
    float barH = juce::jmin(56.0f, barBounds.getHeight() * 0.55f);
    barBounds = barBounds.withHeight(barH).withCentre({ barBounds.getCentreX(), barBounds.getCentreY() });

    // ─── Draw bar ────────────────────────────────────────────────────────
    drawCorrelationBar(g, barBounds);

    // ─── Draw indicator ──────────────────────────────────────────────────
    drawValueIndicator(g, barBounds);
}

} // namespace mixcoach