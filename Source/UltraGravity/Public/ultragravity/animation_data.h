#pragma once

#include <array>

namespace ultragravity {

inline constexpr int wheel_count = 4; // API

// The visible state of a car per tick. Rendering only reads it.
// Angles follow ISO 8855 (ADR-0004): pitch positive nose down,
// roll positive with the left side up, steering positive to the left.
struct AnimationData {
    std::array<double, wheel_count> wheel_angle{};       // rad, positive when rolling forward, in [-π, π)
    std::array<double, wheel_count> steer_angle{};       // rad, per wheel (zero at the rear)
    std::array<double, wheel_count> suspension_travel{}; // m of compression from fully extended
    double pitch = 0.0;                                  // rad
    double roll = 0.0;                                   // rad
    double steering_wheel = 0.0;                         // rad, rotation of the steering wheel
    double engine_speed = 0.0;                           // rad/s of the engine
};

// Wraps an angle into [-π, π).
double wrap_angle(double angle);

// Value between two consecutive ticks for arbitrary frame rates.
// fraction 0 yields exactly a, fraction 1 exactly b; in between it blends linearly.
// The wheel angle takes the short way across the wrap at ±π (a wheel
// turns less than half a revolution per tick).
AnimationData interpolate(const AnimationData& a, const AnimationData& b, double fraction);

} // namespace ultragravity
