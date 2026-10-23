#include "ultragravity/input_smoothing.h"

#include <algorithm>
#include <cmath>

#include "magic_formula.h"
#include "ultragravity/units.h"

namespace ultragravity {
namespace {

constexpr double slip_search_limit = 1.0; // rad, the peak of every usable curve lies below this
constexpr double min_speed = 1.0;         // m/s, guards the division by the square

// Moves value toward target at a constant rate (unit per second).
double approach(double value, double target, double rate, double dt) {
    const double step = rate * dt; // unit of value
    return value + std::clamp(target - value, -step, step);
}

// Steering: away from the value toward full lock it builds up slowly; back
// toward the center and across it, it returns faster.
double steer_rate(double value, double target, const SmoothingParams& p) {
    const bool building = std::abs(target) > std::abs(value) && (value == 0.0 || (target > 0.0) == (value > 0.0));
    return 1.0 / (building ? p.steer_rise_time : p.steer_fall_time);
}

double glide(double value, double target, double rise_time, double fall_time, double dt) {
    return approach(value, target, 1.0 / (target > value ? rise_time : fall_time), dt);
}

} // namespace

double steering_limit(const VehicleParams& params, double speed, double peak_fraction) {
    const Tire& tire = params.front.tire;
    const double peak = peak_fraction * peak_slip(tire.lateral, slip_search_limit); // rad
    const double wheelbase = params.front.x - params.rear.x;                       // m
    const double v = std::max(std::abs(speed), min_speed);                         // m/s
    const double kinematic = wheelbase * lateral_acceleration_limit(tire.peak_friction) / (v * v); // rad
    return std::clamp((peak + kinematic) / params.steering.max_steer_angle, 0.0, 1.0);
}

SmoothingResult smooth(const SmoothingState& state, const SmoothingParams& p,
                       const VehicleParams& vehicle, const BodyMotion& body,
                       const RawInput& raw, std::uint32_t tick, double dt) {
    const Vec3 v = rotate_inverse(body.orientation, body.velocity); // m/s, vehicle frame
    const double speed = std::hypot(v.x, v.y);                      // m/s
    const double drift = drift_angle(v, 0.0);                       // rad, positive to the left

    const double raw_steering = std::clamp(raw.steering, -1.0, 1.0); // 1
    double target = raw_steering * steering_limit(vehicle, speed, p.peak_fraction); // 1
    if (std::abs(raw_steering) < p.steer_key_deadzone && speed > p.countersteer_min_speed &&
        std::abs(drift) > p.countersteer_deadzone) {
        // Countersteer in the direction of travel: if the car moves left of its longitudinal axis (vy > 0), the aid steers left.
        const double excess = std::abs(drift) - p.countersteer_deadzone; // rad
        target = std::copysign(std::min(excess * p.countersteer_gain, 1.0), drift);
    }

    SmoothingResult e;
    e.state.steering = approach(state.steering, target, steer_rate(state.steering, target, p), dt);
    e.state.throttle = glide(state.throttle, std::clamp(raw.throttle, 0.0, 1.0), p.throttle_rise_time,
                             p.throttle_fall_time, dt);
    e.state.brake = glide(state.brake, std::clamp(raw.brake, 0.0, 1.0), p.brake_rise_time, p.brake_fall_time, dt);

    e.packet.tick = tick;
    e.packet.steering = e.state.steering;
    e.packet.throttle = e.state.throttle;
    e.packet.brake = e.state.brake;
    e.packet.clutch = raw.clutch;
    e.packet.handbrake = raw.handbrake;
    e.packet.gear_request = raw.gear_request;
    e.packet.driving_aids = raw.driving_aids;
    return e;
}

SmoothingResult pass_through(const RawInput& raw, std::uint32_t tick) {
    SmoothingResult e;
    e.packet.tick = tick;
    e.packet.steering = std::clamp(raw.steering, -1.0, 1.0);
    e.packet.throttle = std::clamp(raw.throttle, 0.0, 1.0);
    e.packet.brake = std::clamp(raw.brake, 0.0, 1.0);
    e.packet.clutch = raw.clutch;
    e.packet.handbrake = raw.handbrake;
    e.packet.gear_request = raw.gear_request;
    e.packet.driving_aids = raw.driving_aids;
    e.state = {e.packet.steering, e.packet.throttle, e.packet.brake};
    return e;
}

} // namespace ultragravity
