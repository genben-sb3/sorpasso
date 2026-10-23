#pragma once

#include <array>

#include "ultragravity/animation_data.h"
#include "ultragravity/export.h"
#include "ultragravity/input.h"
#include "ultragravity/vector_math.h"
#include "ultragravity/vehicle_file.h"

namespace ultragravity {

// Reverse only engages below this forward speed, never out of forward travel. A rule of the
// gearbox for every car, not a tuning value of one, so it is not in the vehicle file.
inline constexpr double reverse_engage_speed = 0.5; // API, m/s

// Order of the wheels in all per-wheel fields.
enum Wheel { front_left = 0, front_right = 1, rear_left = 2, rear_right = 3 };

// Motion of the body in the world frame, as the test world or later
// Chaos provides it. The position is that of the center of mass.
struct BodyMotion {
    Vec3 position;
    Quat orientation;
    Vec3 velocity;
    Vec3 angular_velocity;

    bool operator==(const BodyMotion&) const = default;
};

// The ray with which the environment probes the ground under a wheel:
// from the strut mount downward, as long as suspension travel plus wheel radius.
struct ProbeRay {
    Vec3 start;
    Vec3 direction; // unit vector
    double length = 0.0; // m
};

struct WheelContact {
    bool contact = false;
    Vec3 point;
    Vec3 normal;
    double friction = 0.0; // 1, of the surface zone
};

using WheelContacts = std::array<WheelContact, wheel_count>;

// Force and torque about the center of mass in the world frame, averaged over the step.
struct Forces {
    Vec3 force;
    Vec3 torque;
};

// Complete, copyable state of the vehicle model.
struct State {
    std::array<double, wheel_count> wheel_speed{}; // rad/s, positive when rolling forward
    std::array<double, wheel_count> wheel_angle{}; // rad, in [-π, π)
    std::array<double, wheel_count> slip_angle{};  // rad, lagged by the tire over the relaxation length
    double engine_speed = 0.0;                     // rad/s
    int gear = 0;                                  // -1 reverse, 0 neutral, 1 … n forward gears
    double shift_remaining = 0.0;                  // s until the gear engages
    double gear_age = 1000.0;                      // s since the last gear change
    double traction_factor = 1.0;                  // 1, share of the throttle that traction control lets through
};


// Measurements of a tick for analysis. They are pure output and do not act
// back on the drive. Slip and tire forces are averaged over the substeps
// of the tick; the forces act along the rolling and lateral direction of the wheel.
struct Telemetry {
    std::array<double, wheel_count> wheel_load{};         // N
    std::array<double, wheel_count> slip{};               // 1, slip ratio
    std::array<double, wheel_count> slip_angle{};         // rad, at the end of the tick
    std::array<double, wheel_count> force_longitudinal{}; // N, tire force in rolling direction
    std::array<double, wheel_count> force_lateral{};      // N, tire force across the rolling direction, positive to the left
    double speed = 0.0;                                   // m/s, body along the vehicle's longitudinal axis
};

struct StepResult {
    Forces forces;
    State state;
    AnimationData animation;
    Telemetry telemetry;
};

UG_API std::array<ProbeRay, wheel_count> probe_rays(const VehicleParams& params, const BodyMotion& body);

// State of a car rolling straight ahead in neutral at the given speed.
UG_API State rolling_state(const VehicleParams& params, double speed);

// The vehicle step: a pure function without global state.
UG_API StepResult vehicle_step(const State& state, const VehicleParams& params,
                        const InputPacket& input, const WheelContacts& contacts,
                        const BodyMotion& body, double dt);

} // namespace ultragravity
