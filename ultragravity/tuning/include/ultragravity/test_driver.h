#pragma once

#include <array>
#include <cstddef>

#include "ultragravity/input.h"
#include "ultragravity/test_world.h"
#include "ultragravity/units.h"
#include "ultragravity/vehicle_file.h"

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

} // namespace ultragravity
