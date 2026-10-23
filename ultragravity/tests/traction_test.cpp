#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>

#include "drive.h"
#include "ultragravity/checksum.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

struct FullThrottleRun {
    double peak_slip = 0.0;       // 1, largest slip of the rear wheels after pulling away
    double mean_slip = 0.0;       // 1
    double speed = 0.0;           // m/s at the end
    int direction_changes = 0;    // of the throttle factor, above noise
    std::uint64_t checksum = 0;
};

// Full throttle from standstill with the automatic, with traction control at the level.
FullThrottleRun full_throttle(int level, double seconds = 4.0) {
    const VehicleParams car = with_automatic();
    TestWorld world(car, 0.02);
    Drive drive(world);
    drive.run(1.5);
    InputPacket packet = drive_input(1.0);
    packet.driving_aids.traction_control = level;

    FullThrottleRun f;
    int n = 0;
    double factor_before = 1.0;
    int direction = 0;
    const int ticks = Drive::ticks(seconds);
    for (int tick = 0; tick < ticks; ++tick) {
        drive.one_tick(packet);
        const auto& s = world.last_step().state;
        const double v = forward_speed(world);
        const double slip = std::max((s.wheel_speed[rear_left] - v / car.rear.wheel_radius),
                                     (s.wheel_speed[rear_right] - v / car.rear.wheel_radius)) *
                            car.rear.wheel_radius / std::sqrt(v * v + 1.0);
        // For the first 0.5 s the automatic is still engaging the clutch.
        if (tick > Drive::ticks(0.5)) {
            f.peak_slip = std::max(f.peak_slip, slip);
            f.mean_slip += slip;
            ++n;
        }
        const double d = s.traction_factor - factor_before;
        if (std::abs(d) > 1e-6) {
            const int now = d > 0.0 ? 1 : -1;
            if (direction != 0 && now != direction) ++f.direction_changes;
            direction = now;
        }
        factor_before = s.traction_factor;
    }
    f.mean_slip /= std::max(n, 1);
    f.speed = forward_speed(world);
    f.checksum = checksum(world.last_step().state, world.body());
    return f;
}

} // namespace

TEST_CASE("At full throttle from standstill the slip is the smaller the higher the level") {
    double before = 1e9;
    for (int level = 0; level <= 5; ++level) {
        const FullThrottleRun f = full_throttle(level);
        CAPTURE(level);
        CAPTURE(f.mean_slip);
        CHECK(f.mean_slip < before);
        before = f.mean_slip;
    }
}

TEST_CASE("Level 0 behaves exactly like no traction control") {
    const VehicleParams car = with_automatic();
    TestWorld off(car, 0.02);
    TestWorld level0(car, 0.02);
    Drive drive_off(off);
    Drive drive_level0(level0);
    InputPacket with_level0 = drive_input(1.0);
    with_level0.driving_aids.traction_control = 0;
    drive_off.run(1.0, drive_input(1.0));
    drive_level0.run(1.0, with_level0);
    CHECK(checksum(off.last_step().state, off.body()) ==
          checksum(level0.last_step().state, level0.body()));
    // And without the aid it spins noticeably, otherwise the comparison would prove nothing.
    CHECK(full_throttle(0).peak_slip > 1.0); // measured: 4.09, band: > 1.0
}

TEST_CASE("Traction control does not oscillate") {
    for (int level = 1; level <= 5; ++level) {
        const FullThrottleRun f = full_throttle(level);
        CAPTURE(level);
        CAPTURE(f.direction_changes);
        // Measured: at most 3 changes in 4 s (cut, hold, restore); switching
        // on and off every tick would be over 100.
        CHECK(f.direction_changes <= 6); // measured: at most 3, band: <= 6
        // It does not brake into nothing: the car makes progress with it.
        CHECK(f.speed > 6.0); // measured: at least 8.30 m/s, band: > 6.0 m/s
    }
}
