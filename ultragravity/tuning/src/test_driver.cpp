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
    const double target_heading = std::atan2(b.position.x, -(b.position.y - radius)); // rad
    const double heading = std::atan2(b.velocity.y, b.velocity.x);                    // rad
    const double heading_error = wrap_angle(target_heading - heading);                // rad
    integral_ = std::clamp(integral_ + error * TestWorld::time_step, -circle_integral_limit, circle_integral_limit);
    const double angle = std::atan(wheelbase_ / radius) + circle_steer_per_meter * error +
                         circle_steer_per_heading * heading_error + circle_steer_integral * integral_; // rad
    return std::clamp(angle / max_steer_angle_, -1.0, 1.0);
}

double SteeringController::path(const BodyMotion& b, const std::function<double(double)>& target_y,
                                double lookahead) {
    const Vec3 forward = rotate(b.orientation, {1.0, 0.0, 0.0});
    const double heading = std::atan2(forward.y, forward.x); // rad
    const double aim_x = b.position.x + lookahead;            // m
    const double dx = aim_x - b.position.x;                   // m
    const double dy = target_y(aim_x) - b.position.y;         // m
    const double lateral = -std::sin(heading) * dx + std::cos(heading) * dy; // m, aim point in the vehicle frame
    const double curvature = 2.0 * lateral / (dx * dx + dy * dy);            // 1/m
    return std::clamp(std::atan(wheelbase_ * curvature) / max_steer_angle_, -1.0, 1.0);
}

} // namespace ultragravity
