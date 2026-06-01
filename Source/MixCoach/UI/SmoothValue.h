#pragma once
#include <cmath>

namespace mixcoach {

// ═══════════════════════════════════════════════════════════════════════════
//  SmoothValue — Suavizado exponencial con ballistics separados attack/release
// ═══════════════════════════════════════════════════════════════════════════
class SmoothValue {
public:
    SmoothValue(float initial = -80.0f, float attackMs = 20.0f, float releaseMs = 200.0f);
    void setTarget(float newTarget, double sampleRate = 30.0);
    [[nodiscard]] float getCurrent() const noexcept { return current_; }
    [[nodiscard]] float getTarget() const noexcept { return target_; }
    void setBallistics(float attackMs, float releaseMs);
    void reset(float value = -80.0f);
    operator float() const { return current_; }

private:
    float current_ = -80.0f;
    float target_  = -80.0f;
    float attackCoeff_  = 0.8f;
    float releaseCoeff_ = 0.15f;
};

} // namespace mixcoach
