#include "SmoothValue.h"
#include <algorithm>

namespace mixcoach {

    namespace {
        constexpr double kMinHz = 1.0;
    } // namespace

    SmoothValue::SmoothValue(float initial, float attackMs, float releaseMs) :
        current_(initial),
        target_(initial),
        attackMs_(attackMs),
        releaseMs_(releaseMs)
    {}

    void SmoothValue::setBallistics(float attackMs, float releaseMs)
    {
        attackMs_  = attackMs;
        releaseMs_ = releaseMs;
    }

    float SmoothValue::stepCoeff(double sampleRateHz, bool attacking) const noexcept
    {
        const float hz    = (float)std::max(kMinHz, sampleRateHz);
        const float dtMs  = 1000.0f / hz;
        const float tauMs = attacking ? attackMs_ : releaseMs_;
        return 1.0f - std::exp(-dtMs / std::max(0.05f, tauMs));
    }

    void SmoothValue::setTarget(float newTarget, double sampleRateHz)
    {
        setTargetValue(newTarget);
        advance(sampleRateHz);
    }

    bool SmoothValue::advance(double sampleRateHz)
    {
        const float prev     = current_;
        const bool attacking = target_ > current_;
        const float coeff    = stepCoeff(sampleRateHz, attacking);
        current_ += (target_ - current_) * coeff;
        return std::abs(current_ - prev) > 0.0002f;
    }

    void SmoothValue::reset(float value)
    {
        current_ = value;
        target_  = value;
    }

} // namespace mixcoach
