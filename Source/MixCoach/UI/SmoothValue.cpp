#include "SmoothValue.h"

namespace mixcoach {

SmoothValue::SmoothValue(float initial, float attackMs, float releaseMs)
    : current_(initial), target_(initial)
{
    setBallistics(attackMs, releaseMs);
}

void SmoothValue::setBallistics(float attackMs, float releaseMs)
{
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
        current_ += (target_ - current_) * attackCoeff_;
    } else {
        current_ += (target_ - current_) * releaseCoeff_;
    }
}

void SmoothValue::reset(float value)
{
    current_ = value;
    target_  = value;
}

} // namespace mixcoach
