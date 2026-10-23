#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "drive.h"
#include "ultragravity/test_maneuver.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

constexpr double abs_min_speed = 15.0 * kmh; // m/s, below this the ABS does not intervene

InputPacket roll_out() {
    InputPacket p;
    p.clutch = 1.0;
    return p;
}

InputPacket full_braking(bool with_abs, double steering) {
    InputPacket p = roll_out();
    p.brake = 1.0;
    p.steering = steering;
    p.driving_aids.abs = with_abs;
    return p;
}

struct Braking {
    double distance = 0.0;            // m to a stop
    double lowest_wheel_speed = 1e9;  // rad/s, lowest wheel speed while the car runs above the minimum speed
    double heading_change = 0.0;      // rad, course of the velocity 1.5 s after the start of braking against the initial direction
};

// Full braking from 100 km/h; from 0.25 s after the start of braking with the steering input.
Braking brake(bool with_abs, double steering) {
    const VehicleParams car = sports_coupe();
    TestWorld world(car, 0.02, TestGround{}, kmh_100);
    Drive drive(world);
    drive.run(1.5, roll_out());
    const Vec3 start = world.body().position;
    Braking b;
    for (int tick = 0; forward_speed(world) > 0.05; ++tick) {
        REQUIRE(tick < 20 * 120);
        drive.one_tick(full_braking(with_abs, tick >= 30 ? steering : 0.0));
        if (forward_speed(world) > abs_min_speed + 0.5) {
            for (double w : world.last_step().state.wheel_speed) b.lowest_wheel_speed = std::min(b.lowest_wheel_speed, w);
        }
        if (tick == 90) {
            const Vec3 v = world.body().velocity;
            b.heading_change = std::abs(std::atan2(v.y, v.x));
        }
    }
    b.distance = length(world.body().position - start);
    return b;
}

} // namespace

TEST_CASE("With ABS no wheel locks under full braking above the minimum speed") {
    const Braking with = brake(true, 0.0);
    const Braking without = brake(false, 0.0);

    CAPTURE(with.lowest_wheel_speed);
    CAPTURE(without.lowest_wheel_speed);
    CHECK(with.lowest_wheel_speed > 8.0); // rad/s; measured: 11.7 rad/s, band: ×0.68
    CHECK(without.lowest_wheel_speed < 0.5); // counter-check: without ABS a wheel locks; measured: 0 rad/s, band: absolute 0.5 rad/s
}

TEST_CASE("With ABS the car can be steered under full braking, without ABS not") {
    const Braking with = brake(true, 0.15);
    const Braking without = brake(false, 0.15);

    CAPTURE(with.heading_change);
    CAPTURE(without.heading_change);
    CHECK(with.heading_change > 0.05); // rad: the steering input changes the direction; measured: 0.0929 rad, band: ×0.54
    CHECK(with.heading_change > 8.0 * without.heading_change); // measured: 0.0929 against 0.0052 rad, factor 18, band: ×0.44
}

TEST_CASE("The braking distance with ABS on a dry surface is not longer than without") {
    const Braking with = brake(true, 0.0);
    const Braking without = brake(false, 0.0);

    CAPTURE(with.distance);
    CAPTURE(without.distance);
    CHECK(with.distance <= without.distance * 1.05); // measured: 30.29 m against 32.66 m (factor 0.927), band: ×1.05
}

TEST_CASE("The table shows both braking test maneuvers") {
    const VehicleParams car = sports_coupe();
    const Measurement without = run_test_maneuver(Maneuver::braking_100_0, car);
    const Measurement with = run_test_maneuver(Maneuver::braking_100_0_abs, car);
    REQUIRE_FALSE(with.failed());
    CHECK(with.unit == "m");
    CHECK(with.result.value() <= without.result.value() * 1.05); // measured: 31.05 m against 33.73 m (factor 0.921), band: ×1.05

    const std::string table = format_table(car.name, measure(car));
    CHECK(table.find("Braking 100–0 km/h without ABS") != std::string::npos);
    CHECK(table.find("Braking 100–0 km/h with ABS") != std::string::npos);
}
