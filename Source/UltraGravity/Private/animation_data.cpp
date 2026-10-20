#include "ultragravity/animation_data.h"

#include <cmath>
#include <numbers>

namespace ultragravity {
namespace {

double mix(double a, double b, double fraction) { return fraction == 1.0 ? b : a + (b - a) * fraction; }

} // namespace

double wrap_angle(double angle) {
    constexpr double pi = std::numbers::pi; // rad
    if (angle >= -pi && angle < pi) return angle;
    const double wrapped = angle - 2.0 * pi * std::floor((angle + pi) / (2.0 * pi)); // rad
    // Rounding can yield exactly π.
    return wrapped >= pi ? -pi : wrapped;
}

AnimationData interpolate(const AnimationData& a, const AnimationData& b, double fraction) {
    AnimationData r;
    for (int i = 0; i < wheel_count; ++i) {
        // Shortest way from a to b across the wrap.
        const double step = wrap_angle(b.wheel_angle[i] - a.wheel_angle[i]); // rad
        r.wheel_angle[i] = fraction == 1.0 ? b.wheel_angle[i] : wrap_angle(a.wheel_angle[i] + step * fraction);
        r.steer_angle[i] = mix(a.steer_angle[i], b.steer_angle[i], fraction);
        r.suspension_travel[i] = mix(a.suspension_travel[i], b.suspension_travel[i], fraction);
    }
    r.pitch = mix(a.pitch, b.pitch, fraction);
    r.roll = mix(a.roll, b.roll, fraction);
    r.steering_wheel = mix(a.steering_wheel, b.steering_wheel, fraction);
    r.engine_speed = mix(a.engine_speed, b.engine_speed, fraction);
    return r;
}

} // namespace ultragravity
