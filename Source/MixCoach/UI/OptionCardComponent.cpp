#include "OptionCardComponent.h"
#include <cmath>

namespace mixcoach {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor
    // ═══════════════════════════════════════════════════════════════════════════
    OptionCardComponent::OptionCardComponent()
    {
        setSize((int)kCardWidth, (int)kCardHeight);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        startTimerHz(60);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  setCardData / setSelected
    // ═══════════════════════════════════════════════════════════════════════════
    void OptionCardComponent::setCardData(const OptionCardData& data)
    {
        data_ = data;
        repaint();
    }

    void OptionCardComponent::setSelected(bool selected)
    {
        if (selected_ == selected) return;
        selected_ = selected;
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  timerCallback — Anima la elevación del hover (SmoothValue-style)
    // ═══════════════════════════════════════════════════════════════════════════
    void OptionCardComponent::timerCallback()
    {
        float target = hovered_ ? 1.0f : 0.0f;
        float diff = target - liftAmount_;

        if (std::abs(diff) < 0.001f) {
            if (liftAmount_ != target) {
                liftAmount_ = target;
                liftSmooth_ = target * kHoverLift;
                repaint();
            }
            return;
        }

        // Smooth approach: 25% towards target per frame (~400ms to settle)
        liftAmount_ += diff * 0.25f;
        liftSmooth_ = liftAmount_ * kHoverLift;
        repaint();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Mouse events
    // ═══════════════════════════════════════════════════════════════════════════
    void OptionCardComponent::mouseEnter(const juce::MouseEvent&)
    {
        hovered_ = true;
    }

    void OptionCardComponent::mouseExit(const juce::MouseEvent&)
    {
        hovered_ = false;
    }

    void OptionCardComponent::mouseDown(const juce::MouseEvent&)
    {
        // Quick press feedback: slight depress
        liftSmooth_ = -1.0f;
        repaint();
    }

    void OptionCardComponent::mouseUp(const juce::MouseEvent&)
    {
        if (getLocalBounds().contains(getMouseXYRelative())) {
            if (onClick)
                onClick();
        }
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  resized
    // ═══════════════════════════════════════════════════════════════════════════
    void OptionCardComponent::resized()
    {
        // All drawing is relative to bounds — no child layout needed
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  paint — Dibuja la tarjeta con hover 3D effect
    // ═══════════════════════════════════════════════════════════════════════════
    void OptionCardComponent::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // Apply lift offset for hover effect
        float liftY = liftSmooth_;
        auto cardBounds = bounds.withTrimmedTop(liftY > 0.0f ? 0.0f : -liftY)
                                .withHeight(bounds.getHeight() - std::abs(liftY));

        if (liftY > 0.0f)
            cardBounds = cardBounds.translated(0.0f, -liftY);

        drawOptionCard(g, cardBounds, data_, hovered_, liftAmount_);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  drawOptionCard — Static drawing helper
    // ═══════════════════════════════════════════════════════════════════════════
    void OptionCardComponent::drawOptionCard(juce::Graphics& g,
                                              juce::Rectangle<float> bounds,
                                              const OptionCardData& data,
                                              bool hovered,
                                              float animLift)
    {
        if (bounds.isEmpty() || !data.isValid()) return;

        const float cr = kCornerRadius;
        juce::Colour cardColour = data.getColour();
        float alphaMult = hovered ? 1.0f : 0.85f;

        // ═══ Shadow (solo si hovered) ═══════════════════════════════════════
        if (hovered || animLift > 0.01f) {
            float shadowAlpha = 0.08f + 0.12f * animLift;
            float shadowOffset = 1.0f + 3.0f * animLift;
            g.setColour(juce::Colours::black.withAlpha(shadowAlpha));
            g.fillRoundedRectangle(bounds.translated(0.0f, shadowOffset)
                                         .expanded(1.0f, 1.0f), cr);
        }

        // ═══ Background ═════════════════════════════════════════════════════
        g.setColour(cardColour.withAlpha(0.12f * alphaMult));
        g.fillRoundedRectangle(bounds, cr);

        // ═══ Border ═════════════════════════════════════════════════════════
        float borderAlpha = hovered ? 0.45f : 0.20f;
        g.setColour(cardColour.withAlpha(borderAlpha * alphaMult));
        g.drawRoundedRectangle(bounds, cr, 0.8f);

        // ═══ Top glow line ═══════════════════════════════════════════════════
        {
            auto glowLine = bounds.withHeight(2.0f).reduced(4, 0);
            g.setGradientFill(juce::ColourGradient(
                cardColour.withAlpha(0.30f * alphaMult),
                glowLine.getCentreX(), glowLine.getY(),
                cardColour.withAlpha(0.0f),
                glowLine.getCentreX(), glowLine.getBottom(),
                false));
            g.fillRoundedRectangle(glowLine, 1.0f);
        }

        auto area = bounds.reduced(5, 4);

        // ═══ TIER BADGE (top-left) ═══════════════════════════════════════════
        auto badgeArea = area.removeFromTop(14.0f);

        g.setFont(juce::Font(juce::FontOptions(7.0f)).boldened());
        g.setColour(cardColour.withAlpha(0.80f * alphaMult));
        juce::String badgeText = juce::String(OptionCardData::tierIcon(data.tier))
                                + " " + OptionCardData::tierLabel(data.tier);
        g.drawText(badgeText, badgeArea, juce::Justification::centredLeft);

        // ═══ SELECTED INDICATOR (top-right) ═══════════════════════════════════
        // (Solo si alguna lógica externa marca selected)

        // ═══ PLUGIN NAME (middle) ═════════════════════════════════════════════
        area.removeFromTop(2);
        auto nameArea = area.removeFromTop(24.0f);

        // Plugin name — bold, wrap si es necesario
        g.setFont(juce::Font(juce::FontOptions(9.0f)).boldened());
        g.setColour(MixCoachTheme::textBright().withAlpha(alphaMult));
        g.drawText(data.pluginName, nameArea,
                   juce::Justification::centredLeft);

        // ═══ ACTION TEXT / PARAMS (bottom) ════════════════════════════════════
        if (data.actionText.isNotEmpty()) {
            auto paramsArea = area.removeFromTop(16.0f);

            // Parameter pills (parse actionText to extract key=value pairs)
            g.setFont(juce::Font(juce::FontOptions(6.5f)).boldened());
            g.setColour(cardColour.withAlpha(0.85f * alphaMult));

            // Draw actionText inline
            g.drawText(data.actionText, paramsArea.reduced(0, 1),
                       juce::Justification::centredLeft);
        }

        // ═══ EXTRA INFO (bottom if space) ════════════════════════════════════
        if (data.extraInfo.isNotEmpty() && area.getHeight() >= 10) {
            g.setFont(juce::Font(juce::FontOptions(5.5f)));
            g.setColour(MixCoachTheme::textMuted().withAlpha(0.45f * alphaMult));
            g.drawText(data.extraInfo, area, juce::Justification::bottomLeft);
        }
    }

} // namespace mixcoach
