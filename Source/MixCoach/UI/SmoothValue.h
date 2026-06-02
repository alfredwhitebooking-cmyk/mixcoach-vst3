#pragma once
#include <cmath>

namespace mixcoach {

// Suavizado exponencial con attack/release; avance dependiente del Hz del timer UI.
class SmoothValue
{
public:
    SmoothValue(float initial = -80.0f, float attackMs = 20.0f, float releaseMs = 200.0f);

    void setBallistics(float attackMs, float releaseMs);

    /** Solo fija el objetivo (sin avanzar). Usar con advance() en el timer 60 Hz. */
    void setTargetValue(float newTarget) noexcept { target_ = newTarget; }

    /** Fija objetivo y avanza un paso (compatibilidad con llamadas existentes). */
    void setTarget(float newTarget, double sampleRateHz = 60.0);

    /** Avanza hacia target_; devuelve true si el valor visible cambió. */
    bool advance(double sampleRateHz = 60.0);

    [[nodiscard]] float getCurrent() const noexcept { return current_; }
    [[nodiscard]] float getTarget() const noexcept { return target_; }
    void reset(float value = -80.0f);
    operator float() const { return current_; }

private:
    float current_ = -80.0f;
    float target_  = -80.0f;
    float attackMs_  = 20.0f;
    float releaseMs_ = 200.0f;

    float stepCoeff(double sampleRateHz, bool attacking) const noexcept;
};

} // namespace mixcoach
