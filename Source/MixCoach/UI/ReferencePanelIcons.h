#pragma once
#include <juce_graphics/juce_graphics.h>

namespace mixcoach {

// ─── ICONOS VECTORIALES NATIVOS ─────────────────────────────────────────────
inline void drawFileIcon(juce::Graphics& g, float cx, float cy, float size, juce::Colour col)
{
    g.setColour(col);
    float w = size * 0.65f;
    float h = size * 0.85f;
    float x = cx - w * 0.5f;
    float y = cy - h * 0.5f;

    juce::Path p;
    p.startNewSubPath(x, y);
    p.lineTo(x + w * 0.65f, y);
    p.lineTo(x + w, y + h * 0.35f);
    p.lineTo(x + w, y + h);
    p.lineTo(x, y + h);
    p.closeSubPath();

    g.strokePath(p, juce::PathStrokeType(1.2f));

    juce::Path fold;
    fold.startNewSubPath(x + w * 0.65f, y);
    fold.lineTo(x + w * 0.65f, y + h * 0.35f);
    fold.lineTo(x + w, y + h * 0.35f);
    g.strokePath(fold, juce::PathStrokeType(1.0f));
}

inline void drawLinkIcon(juce::Graphics& g, float cx, float cy, float size, juce::Colour col)
{
    g.setColour(col);
    g.saveState();
    g.addTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::pi * 0.25f, cx, cy));

    float w = size * 0.55f;
    float h = size * 0.25f;
    float r = h * 0.5f;

    g.drawRoundedRectangle(cx - w * 0.8f, cy - h * 0.5f, w, h, r, 1.2f);
    g.drawRoundedRectangle(cx - w * 0.2f, cy - h * 0.5f, w, h, r, 1.2f);

    g.restoreState();
}

inline void drawPlayIcon(juce::Graphics& g, float cx, float cy, float size, juce::Colour col)
{
    g.setColour(col);
    float r = size * 0.32f;
    juce::Path p;
    p.addTriangle(cx - r * 0.5f, cy - r * 0.866f,
                  cx + r, cy,
                  cx - r * 0.5f, cy + r * 0.866f);
    g.fillPath(p);
}

inline void drawPauseIcon(juce::Graphics& g, float cx, float cy, float size, juce::Colour col)
{
    g.setColour(col);
    float w = size * 0.18f;
    float h = size * 0.65f;
    float gap = size * 0.12f;
    g.fillRoundedRectangle(cx - gap - w, cy - h * 0.5f, w, h, w * 0.3f);
    g.fillRoundedRectangle(cx + gap,     cy - h * 0.5f, w, h, w * 0.3f);
}

} // namespace mixcoach
