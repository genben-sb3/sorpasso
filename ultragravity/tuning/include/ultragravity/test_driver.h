#pragma once

#include <array>
#include <cstddef>
#include <functional>

#include "ultragravity/input.h"
#include "ultragravity/test_world.h"
#include "ultragravity/units.h"
#include "ultragravity/vehicle_file.h"
#include "ultragravity/vehicle_step.h"

namespace ultragravity {

// Pulls away and shifts up just before the limiter: clutch in, throttle off,
// next gear, after the shift time let the clutch out briskly. In the
// lower gears it only gives as much throttle as keeps the rear wheels from spinning much.
// It controls the car only through input packets and only reads the engine speed from
// the animation data, as a driver does on the rev counter.
class TestDriver {
public:
    static constexpr double engage = 0.3;               // s clutch pressed before pulling away
    static constexpr double clutch_launch = 0.5;        // s for letting the clutch out when pulling away
    static constexpr double clutch_shift = 0.15;        // s for letting the clutch out after a shift
    static constexpr double shift_speed = 6800.0 * rpm; // rad/s
    static constexpr std::array<double, 3> throttle_low_gears{0.75, 0.95, 1.0}; // 1

    explicit TestDriver(const VehicleParams& k);

    // The input packet for the next tick, without tick number.
    InputPacket packet(const TestWorld& world);

    int gear() const { return gear_; }
    bool clutch_engaged() const { return time_ - shifted_ > shift_pause_ + clutch_shift; }

private:
    double throttle() const { return gear_ <= 3 ? throttle_low_gears[static_cast<std::size_t>(gear_ - 1)] : 1.0; }

    int gears_;             // 1, forward gears of the car
    double shift_pause_;    // s
    int gear_ = 1;          // 1, gear engaged by the driver
    double time_ = 0.0;     // s since the start of pulling away
    double shifted_ = -1e9; // s, time of the last gear change
};

// The steering controller of the test driver: it only reads the body motion (position, heading),
// as a driver sees the road ahead, and returns the steering (-1 right … 1 left) for the
// input packet. It does not call the input smoothing.
class SteeringController {
public:
    explicit SteeringController(const VehicleParams& k);

    // Holds a left-hand circle about the point (0, radius). Time step: TestWorld::time_step.
    double circle(const BodyMotion& body, double radius); // radius in m

    // Follows the path y = target_y(x) (m) like a driver who aims at a point `lookahead` m ahead.
    double path(const BodyMotion& body, const std::function<double(double)>& target_y,
                double lookahead); // lookahead in m

    // Distance of the body from the circle, positive outside, in m.
    static double circle_error(const BodyMotion& body, double radius);

private:
    double wheelbase_;       // m
    double max_steer_angle_; // rad
    double integral_ = 0.0;  // m·s, circle error over time
};

// The drift controller of the test driver: holds a drift angle through counter-steering and
// throttle. It only reads the body motion (heading and direction of travel, as a driver sees
// them) and returns steering and throttle for the input packet; gear and clutch stay with the
// caller. It does not call the input smoothing.
class DriftController {
public:
    struct Output {
        double steering = 0.0; // -1 right … 1 left
        double throttle = 0.0; // 0 … 1
    };

    explicit DriftController(const VehicleParams& k);

    // Controls toward the drift angle `target` (rad, positive: front left of the direction of
    // travel, i.e. the rear out in a left-hand corner). Counter-steering acts fast, the throttle slowly.
    Output regulate(const BodyMotion& body, double target); // target in rad

    // Drift angle of the body in rad, positive as for `regulate`.
    static double drift_angle(const BodyMotion& body);

private:
    double max_steer_angle_;      // rad
    double front_axle_distance_;  // m, center of mass to front axle
    double heading_ = 0.0;        // rad, direction of travel in the last tick
    double heading_rate_ = 0.0;   // rad/s, smoothed rotation rate of the direction of travel
    bool heading_known_ = false;
    double throttle_integral_ = 0.0; // 1, throttle share from the control deviation
};

} // namespace ultragravity
