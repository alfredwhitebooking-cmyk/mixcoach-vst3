#include "TargetMarkers.h"

namespace mixcoach {

TargetMarkers::TargetMarkers()
{
    titleLabel_.setText("Referencia", juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions(9.0f)));
    titleLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim());
    titleLabel_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel_);

    summaryLabel_.setFont(juce::Font(juce::FontOptions(9.0f)));
    summaryLabel_.setColour(juce::Label::textColourId, MixCoachTheme::textDim().withAlpha(0.7f));
    summaryLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(summaryLabel_);
}

void TargetMarkers::setTarget(const juce::String& label, float targetValue, float actualValue)
{
    // Buscar si ya existe, actualizar
    for (auto& t : targets_) {
        if (t.label == label) {
            t.targetValue = targetValue;
            t.actualValue = actualValue;
            // Normalizar: qué tan cerca está del target (1.0 = perfecto, 0.0 = muy lejos)
            float diff = std::abs(actualValue - targetValue);
            float maxDiff = std::abs(targetValue) * 2.0f;
            if (maxDiff < 0.01f) maxDiff = 1.0f;
            t.normalised = juce::jlimit(0.0f, 1.0f, 1.0f - diff / maxDiff);
            repaint();
            return;
        }
    }

    // Nuevo target
    TargetItem item;
    item.label = label;
    item.targetValue = targetValue;
    item.actualValue = actualValue;
    float diff = std::abs(actualValue - targetValue);
    float maxDiff = std::abs(targetValue) * 2.0f;
    if (maxDiff < 0.01f) maxDiff = 1.0f;
    item.normalised = juce::jlimit(0.0f, 1.0f, 1.0f - diff / maxDiff);
    targets_.push_back(item);
    repaint();
}

void TargetMarkers::clearTargets()
{
    targets_.clear();
    repaint();
}

void TargetMarkers::setSummary(const juce::String& summary)
{
    summary_ = summary;
    summaryLabel_.setText(summary_, juce::dontSendNotification);
}

juce::Colour TargetMarkers::getDeviationColour(float actual, float target) const noexcept
{
    float diff = std::abs(actual - target);
    float maxDiff = std::abs(target);
    if (maxDiff < 0.01f) return MixCoachTheme::success();

    float ratio = diff / maxDiff;
    if (ratio < 0.1f)  return MixCoachTheme::success();    // Verde — muy cerca
    if (ratio < 0.3f)  return MixCoachTheme::warning();    // Amarillo — algo lejos
    return MixCoachTheme::error();                          // Rojo — muy lejos
}

void TargetMarkers::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float cr = 4.0f;

    // ─── Fondo oscuro ──────────────────────────────────────────────────
    g.setColour(MixCoachTheme::bgPanel());
    g.fillRoundedRectangle(bounds, cr);

    if (targets_.empty()) {
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.setColour(MixCoachTheme::textDim().withAlpha(0.5f));
        g.drawText("Sin referencias cargadas", bounds.toNearestInt(),
                   juce::Justification::centred);
        return;
    }

    auto area = bounds.reduced(6, 4);
    area.removeFromTop(14.0f); // título

    float rowHeight = juce::jmin(22.0f, area.getHeight() / (float)targets_.size());
    float totalRowsH = rowHeight * (float)targets_.size();
    float startY = area.getY() + (area.getHeight() - totalRowsH) * 0.5f;

    for (size_t i = 0; i < targets_.size(); ++i) {
        const auto& t = targets_[i];
        auto row = area.withY(startY + (float)i * rowHeight).withHeight(rowHeight);

        // ─── Label ─────────────────────────────────────────────────────
        auto labelArea = row.removeFromLeft(row.getWidth() * 0.35f).reduced(2, 0);
        g.setFont(juce::Font(juce::FontOptions(9.0f)));
        g.setColour(MixCoachTheme::textDim());
        g.drawText(t.label, labelArea.toNearestInt(), juce::Justification::centredLeft);

        // ─── Barra de fondo ────────────────────────────────────────────
        auto barArea = row.reduced(2, 2);
        g.setColour(MixCoachTheme::bgDarker());
        g.fillRoundedRectangle(barArea, 2.0f);

        // ─── Barra de valor actual ────────────────────────────────────
        float barW = barArea.getWidth() * t.normalised;
        if (barW > 2.0f) {
            juce::Colour barColour = getDeviationColour(t.actualValue, t.targetValue);
            g.setColour(barColour.withAlpha(0.7f));
            g.fillRoundedRectangle(barArea.withWidth(barW), 2.0f);
        }

        // ─── Marca del target ──────────────────────────────────────────
        float targetX = barArea.getX() + barArea.getWidth() * 0.5f; // target en el centro
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.drawVerticalLine((int)targetX, barArea.getY(), barArea.getBottom());

        // ─── Marcador del target (triángulo) ───────────────────────────
        float triSize = 4.0f;
        juce::Path triPath;
        triPath.startNewSubPath(targetX, barArea.getY() - triSize);
        triPath.lineTo(targetX - triSize, barArea.getY());
        triPath.lineTo(targetX + triSize, barArea.getY());
        triPath.closeSubPath();
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.fillPath(triPath);

        // ─── Valor numérico ───────────────────────────────────────────
        g.setFont(juce::Font(juce::FontOptions(8.0f)).boldened());
        g.setColour(MixCoachTheme::accentCyan());
        g.drawText(juce::String(t.actualValue, 1),
                   barArea.getRight() - 32.0f, barArea.getY(), 30.0f, barArea.getHeight(),
                   juce::Justification::centredRight);
    }
}

void TargetMarkers::resized()
{
    auto bounds = getLocalBounds();
    titleLabel_.setBounds(bounds.removeFromTop(14).reduced(4, 0));

    if (!summary_.isEmpty()) {
        summaryLabel_.setVisible(true);
        summaryLabel_.setBounds(bounds.removeFromBottom(16).reduced(4, 0));
    } else {
        summaryLabel_.setVisible(false);
    }
}

} // namespace mixcoach
