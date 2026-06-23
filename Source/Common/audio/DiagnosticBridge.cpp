#include "DiagnosticBridge.h"
#include <juce_graphics/juce_graphics.h>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  BandDiagnostic
// ═══════════════════════════════════════════════════════════════════════════

juce::Colour BandDiagnostic::getDisplayColour() const noexcept
{
    if (isPraise)
        return juce::Colour(0xFF10B981); // Green for praise
    
    if (isCritical)
        return juce::Colour(0xFFEF4444); // Red for critical
    
    // Severity-based: yellow -> orange -> red gradient
    float t = juce::jlimit(0.0f, 1.0f, severity);
    auto yellow = juce::Colour(0xFFF59E0B);
    auto orange = juce::Colour(0xFFF97316);
    auto red    = juce::Colour(0xFFEF4444);
    
    if (t < 0.5f)
        return yellow.interpolatedWith(orange, t * 2.0f);
    else
        return orange.interpolatedWith(red, (t - 0.5f) * 2.0f);
}

// ═══════════════════════════════════════════════════════════════════════════
//  PhaseDiagnostic
// ═══════════════════════════════════════════════════════════════════════════

juce::Colour PhaseDiagnostic::getDisplayColour() const noexcept
{
    if (isPraise)
        return juce::Colour(0xFF10B981); // Green for healthy phase
    if (isWarning)
        return juce::Colour(0xFFEF4444); // Red for phase warning
    
    // Severity-based: yellow -> orange -> red
    float t = juce::jlimit(0.0f, 1.0f, severity);
    auto yellow = juce::Colour(0xFFF59E0B);
    auto red    = juce::Colour(0xFFEF4444);
    return yellow.interpolatedWith(red, t);
}

// ═══════════════════════════════════════════════════════════════════════════
//  DiagnosticBridge
// ═══════════════════════════════════════════════════════════════════════════

void DiagnosticBridge::setDiagnostics(const std::vector<BandDiagnostic>& diagnostics)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        diagnostics_ = diagnostics;
    }
    sendChangeMessage(); // Notify UI listeners
}

std::vector<BandDiagnostic> DiagnosticBridge::getDiagnostics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return diagnostics_;
}

void DiagnosticBridge::clearDiagnostics()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        diagnostics_.clear();
        phaseDiagnostics_.clear();
    }
    sendChangeMessage(); // Notify UI to clear overlay
}

bool DiagnosticBridge::hasDiagnostics() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    return !diagnostics_.empty();
}

// ═══ PhaseDiagnostics ═════════════════════════════════════════════════════

void DiagnosticBridge::setPhaseDiagnostics(const std::vector<PhaseDiagnostic>& diagnostics)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        phaseDiagnostics_ = diagnostics;
    }
    sendChangeMessage();
}

std::vector<PhaseDiagnostic> DiagnosticBridge::getPhaseDiagnostics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return phaseDiagnostics_;
}

void DiagnosticBridge::clearPhaseDiagnostics()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        phaseDiagnostics_.clear();
    }
    sendChangeMessage();
}

bool DiagnosticBridge::hasPhaseDiagnostics() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    return !phaseDiagnostics_.empty();
}

void DiagnosticBridge::setAllDiagnostics(const std::vector<BandDiagnostic>& bandDiags,
                                          const std::vector<PhaseDiagnostic>& phaseDiags)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        diagnostics_ = bandDiags;
        phaseDiagnostics_ = phaseDiags;
    }
    sendChangeMessage();
}

} // namespace mixcoach
