#include "ultragravity/test_driver.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ultragravity {

TestDriver::TestDriver(const VehicleParams& k)
    : gears_(static_cast<int>(k.drivetrain.gearbox.ratios.size())), shift_pause_(k.drivetrain.gearbox.shift_time + 0.05) {}

InputPacket TestDriver::packet(const TestWorld& world) {
    InputPacket p;
    p.gear_request = static_cast<std::int8_t>(gear_);
    if (time_ < engage) {
        p.clutch = 1.0;
    } else if (time_ < engage + clutch_launch) {
        p.throttle = throttle();
        p.clutch = 1.0 - (time_ - engage) / clutch_launch;
    } else {
        const double since_shift = time_ - shifted_; // s
        if (gear_ < gears_ && since_shift > shift_pause_ + clutch_shift &&
            world.last_step().animation.engine_speed > shift_speed) {
            ++gear_;
            shifted_ = time_;
            p.gear_request = static_cast<std::int8_t>(gear_);
            p.clutch = 1.0;
        } else if (since_shift < shift_pause_) {
            p.clutch = 1.0;
        } else {
            p.throttle = throttle();
            p.clutch = std::max(1.0 - (since_shift - shift_pause_) / clutch_shift, 0.0);
        }
    }
    time_ += TestWorld::time_step;
    return p;
}

namespace {

constexpr double circle_steer_per_meter = 0.01;      // rad of steer angle per m of circle error
constexpr double circle_steer_per_heading = 0.5;     // 1, steer angle per rad of heading error
constexpr double circle_steer_integral = 0.004;      // rad per m·s of circle error
constexpr double circle_integral_limit = 20.0;       // m·s

} // namespace

SteeringController::SteeringController(const VehicleParams& k)
    : wheelbase_(k.front.x - k.rear.x), max_steer_angle_(k.steering.max_steer_angle) {}

double SteeringController::circle_error(const BodyMotion& b, double radius) {
    return std::hypot(b.position.x, b.position.y - radius) - radius;
}

double SteeringController::circle(const BodyMotion& b, double radius) {
    const double error = circle_error(b, radius); // m
    // Tangent of the left-hand circle at the position: perpendicular to the radius, counterclockwise.
    const double target_heading = std::atan2(b.position.x, -(b.position.y - radius)); // rad; helper-ok: tangent of the circle
    const double heading = std::atan2(b.velocity.y, b.velocity.x);                    // rad; helper-ok: course in the world frame
    const double heading_error = wrap_angle(target_heading - heading);                // rad
    integral_ = std::clamp(integral_ + error * TestWorld::time_step, -circle_integral_limit, circle_integral_limit);
    const double angle = std::atan(wheelbase_ / radius) + circle_steer_per_meter * error +
                         circle_steer_per_heading * heading_error + circle_steer_integral * integral_; // rad
    return std::clamp(angle / max_steer_angle_, -1.0, 1.0);
}

double SteeringController::path(const BodyMotion& b, const std::function<double(double)>& target_y,
                                double lookahead) {
    const Vec3 forward = rotate(b.orientation, {1.0, 0.0, 0.0});
    const double heading = std::atan2(forward.y, forward.x); // rad; helper-ok: heading in the world frame
    const double aim_x = b.position.x + lookahead;            // m
    const double dx = aim_x - b.position.x;                   // m
    const double dy = target_y(aim_x) - b.position.y;         // m
    const double lateral = -std::sin(heading) * dx + std::cos(heading) * dy; // m, aim point in the vehicle frame
    const double curvature = 2.0 * lateral / (dx * dx + dy * dy);            // 1/m
    return std::clamp(std::atan(wheelbase_ * curvature) / max_steer_angle_, -1.0, 1.0);
}

namespace {

constexpr double drift_countersteer_p = 0.5;    // 1, steer angle per rad of drift angle deviation
constexpr double drift_yaw_rate_d = 1.0;        // 1, share of the yaw term in the front wheel direction
constexpr double drift_rate_d = 0.3;            // s, steer angle per rad/s of drift angle rate
constexpr double drift_smoothing = 0.3;         // 1, low-pass of the heading rate per tick
constexpr double drift_base_throttle = 0.65;    // 1; the package had 0.55, too little to hold 25 degrees with peak friction 1.25
constexpr double drift_throttle_p = 0.6;        // 1 throttle per rad of deviation
constexpr double drift_throttle_i = 1.0;        // 1/s per rad of deviation
constexpr double drift_integral_limit = 0.5;    // 1, bound of the integral part
constexpr double drift_throttle_min = 0.25;     // 1
constexpr double drift_speed_min = 5.0;         // m/s, guards the division in the yaw term

} // namespace

DriftController::DriftController(const VehicleParams& k)
    : max_steer_angle_(k.steering.max_steer_angle), front_axle_distance_(k.front.x - k.center_of_mass.x) {}

double DriftController::drift_angle(const BodyMotion& b) {
    // Sign: positive when the motion lies right of the longitudinal axis (rear out to the left).
    return -drift_angle_full(rotate_inverse(b.orientation, b.velocity));
}

DriftController::Output DriftController::regulate(const BodyMotion& b, double target) {
    const double angle = drift_angle(b);        // rad
    const double deviation = angle - target;    // rad
    throttle_integral_ = std::clamp(throttle_integral_ - drift_throttle_i * deviation * TestWorld::time_step,
                                    -drift_integral_limit, drift_integral_limit);
    // Counter-steer against the drift angle; more deviation means more counter-steering.
    // Rate of the drift angle: yaw rate minus rotation rate of the direction of travel (smoothed).
    const double heading = std::atan2(b.velocity.y, b.velocity.x); // rad; helper-ok: course in the world frame
    if (heading_known_) {
        const double rate = wrap_angle(heading - heading_) / TestWorld::time_step; // rad/s
        heading_rate_ += (rate - heading_rate_) * drift_smoothing;
    }
    heading_ = heading;
    heading_known_ = true;
    const double angle_rate = b.angular_velocity.z - heading_rate_;                                   // rad/s
    const double speed = std::max(std::hypot(b.velocity.x, b.velocity.y), drift_speed_min);         // m/s
    const double steer_angle = -angle + drift_yaw_rate_d * b.angular_velocity.z * front_axle_distance_ / speed -
                               drift_countersteer_p * deviation - drift_rate_d * angle_rate; // rad
    Output out;
    out.steering = std::clamp(steer_angle / max_steer_angle_, -1.0, 1.0);
    out.throttle = std::clamp(drift_base_throttle - drift_throttle_p * deviation + throttle_integral_, drift_throttle_min, 1.0);
    return out;
}

} // namespace ultragravity
