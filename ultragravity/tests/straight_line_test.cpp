#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>

#include "drive.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

InputPacket full_brake() {
    InputPacket packet;
    packet.brake = 1.0;
    return packet;
}

// Brakes fully until the car practically stands and returns the distance from the start of braking.
double braking_distance(TestWorld& world, Drive& drive) {
    const Vec3 start = world.body().position;
    for (int tick = 0; forward_speed(world) > 0.01; ++tick) {
        REQUIRE(tick < 20 * 120);
        drive.one_tick(full_brake());
    }
    return length(world.body().position - start);
}

// Braking distance from μ·g: s = v²/(2·μ·g). Locked wheels slide below the
// peak friction, so the distance may be somewhat longer, but never shorter.
void check_braking_distance(double distance, double v, double friction) {
    const double ideal = v * v / (2.0 * friction * TestWorld::gravity);
    CAPTURE(ideal);
    CHECK(distance > 0.98 * ideal); // measured: 1.08·ideal (dry), 1.06·ideal (wet), band: > 0.98·ideal
    CHECK(distance < 1.15 * ideal); // measured: 1.08·ideal (dry), 1.06·ideal (wet), band: < 1.15·ideal
}

} // namespace

TEST_CASE("The braking distance from 100 km/h on a dry surface follows from μ·g") {
    const VehicleParams car = sports_coupe();
    TestWorld world(car, 0.02, TestGround{}, kmh_100);
    Drive drive(world);
    drive.run(1.5); // let the suspension settle

    // While settling, aerodynamic drag and rolling resistance cost about 2 %.
    const double v = forward_speed(world);
    REQUIRE(v == doctest::Approx(kmh_100).epsilon(0.03)); // measured: -1.9 %, band: ±3 %
    const double distance = braking_distance(world, drive);

    CAPTURE(distance);
    check_braking_distance(distance, v, car.front.tire.peak_friction * TestGround{}.friction);
}

TEST_CASE("On its sports tires the coupe stops from 100 km/h within 34 m") {
    TestWorld world(sports_coupe(), 0.02, TestGround{}, kmh_100);
    Drive drive(world);
    drive.run(1.5);
    const double distance = braking_distance(world, drive);
    CAPTURE(distance);
    // With μ = 1.05 (everyday tire) it was 38.6 m.
    CHECK(distance < 34.0); // measured: 32.7 m, band: < 34 m
    CHECK(distance > 31.0); // measured: 32.7 m, band: > 31 m
}

TEST_CASE("On a wet surface zone the braking distance is correspondingly longer") {
    const VehicleParams car = sports_coupe();
    const double dry = TestGround{}.friction;
    const double wet = 0.7;
    // The car rolls about 40 m while settling and then brakes entirely on the zone.
    TestGround ground;
    ground.surface_zones.push_back({.x_from = 20.0, .x_to = 500.0, .y_from = -10.0, .y_to = 10.0, .friction = wet});

    TestWorld world_dry(car, 0.02, TestGround{}, kmh_100);
    TestWorld world_wet(car, 0.02, ground, kmh_100);
    Drive drive_dry(world_dry);
    Drive drive_wet(world_wet);
    drive_dry.run(1.5);
    drive_wet.run(1.5);
    REQUIRE(world_wet.body().position.x - 2.0 > 20.0); // measured: 39.3 m, band: > 20 m

    const double v = forward_speed(world_wet);
    const double distance_dry = braking_distance(world_dry, drive_dry);
    const double distance_wet = braking_distance(world_wet, drive_wet);

    CAPTURE(distance_dry);
    CAPTURE(distance_wet);
    check_braking_distance(distance_wet, v, car.front.tire.peak_friction * wet);
    // s = v²/(2·μ·g): at the same initial speed the distances are inversely proportional to the frictions.
    CHECK(distance_wet / distance_dry == doctest::Approx(dry / wet).epsilon(0.03)); // measured: -1.6 %, band: ±3 %
}

TEST_CASE("When braking the body pitches forward and the front axle carries more load") {
    const VehicleParams car = sports_coupe();
    TestWorld world(car, 0.02, TestGround{}, kmh_100);
    Drive drive(world);
    drive.run(1.5);

    // After one second of full braking the pitch has settled.
    drive.run(1.0, full_brake());
    const double v_before = forward_speed(world);
    drive.one_tick(full_brake());
    const double deceleration = (v_before - forward_speed(world)) / TestWorld::time_step;
    REQUIRE(deceleration > 5.0); // measured: 11.2 m/s², band: > 5.0 m/s²

    const Vec3 nose = rotate(world.body().orientation, {1.0, 0.0, 0.0});
    CHECK(nose.z < 0.0);

    // Beckman: Lf = G·b/L + m·a·h/L with h as the height of the center of mass above the ground.
    const double weight = car.mass * TestWorld::gravity;
    const double wheelbase = car.front.x - car.rear.x;
    const double b = car.center_of_mass.x - car.rear.x;
    const double h = world.body().position.z;
    const double expected = weight * b / wheelbase + car.mass * deceleration * h / wheelbase;

    const auto& wheel_load = world.last_step().telemetry.wheel_load;
    const double front_axle = wheel_load[front_left] + wheel_load[front_right];
    CHECK(front_axle > 1.2 * weight * b / wheelbase); // measured: 1.41·static, band: > 1.2·static
    CHECK(front_axle == doctest::Approx(expected).epsilon(0.03)); // measured: +0.45 %, band: ±3 %
}

TEST_CASE("Under full braking the wheels lock without the wheel speed oscillating around zero") {
    TestWorld world(sports_coupe(), 0.02, TestGround{}, kmh_100);
    Drive drive(world);
    drive.run(1.5);

    // Until standstill and one second beyond.
    std::array<bool, wheel_count> locked{};
    double lowest_speed = 0.0;
    int trailing_ticks = 0;
    for (int tick = 0; tick < 10 * 120 && trailing_ticks < 120; ++tick) {
        drive.one_tick(full_brake());
        if (forward_speed(world) < 0.01) ++trailing_ticks;
        for (int i = 0; i < wheel_count; ++i) {
            const double omega = world.last_step().state.wheel_speed[i];
            lowest_speed = std::min(lowest_speed, omega);
            // Once locked, the wheel stays exactly still.
            if (locked[i]) CHECK(omega == 0.0);
            locked[i] = locked[i] || omega == 0.0;
        }
    }

    REQUIRE(trailing_ticks == 120);
    CHECK(lowest_speed == 0.0);
    for (bool b : locked) CHECK(b);
    // The stopped car no longer moves.
    CHECK(length(world.body().velocity) < 1e-3); // measured: 1.9e-4 m/s, band: < 1e-3 m/s
}
