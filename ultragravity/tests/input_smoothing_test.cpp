#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>

#include "drive.h"
#include "ultragravity/input_smoothing.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

constexpr double dt = TestWorld::time_step; // s

BodyMotion straight_ahead(double speed) {
    BodyMotion b;
    b.velocity = {speed, 0.0, 0.0};
    return b;
}

// Runs n seconds with the same raw signal and returns the last state.
SmoothingResult run(const VehicleParams& car, const BodyMotion& b, const RawInput& raw, double seconds,
                    SmoothingState start = {}) {
    SmoothingResult e;
    e.state = start;
    for (int i = 0; i < Drive::ticks(seconds); ++i) e = smooth(e.state, SmoothingParams{}, car, b, raw, 0, dt);
    return e;
}

} // namespace

TEST_CASE("A full key press builds up the steering over the set time") {
    const VehicleParams car = sports_coupe();
    const SmoothingParams p;
    RawInput raw;
    raw.steering = 1.0;
    const BodyMotion slow = straight_ahead(2.0); // the limit is 1 there

    CHECK(run(car, slow, raw, p.steer_rise_time / 2.0).packet.steering == doctest::Approx(0.5).epsilon(0.02));
    CHECK(run(car, slow, raw, p.steer_rise_time * 1.05).packet.steering == doctest::Approx(1.0));
}

TEST_CASE("Releasing returns the steering faster than it was built up") {
    const VehicleParams car = sports_coupe();
    const SmoothingParams p;
    const BodyMotion slow = straight_ahead(2.0);
    RawInput released;

    const SmoothingResult e = run(car, slow, released, p.steer_fall_time / 2.0, {1.0, 0.0, 0.0});
    CHECK(e.packet.steering == doctest::Approx(0.5).epsilon(0.02));
    CHECK(p.steer_fall_time < p.steer_rise_time);
    CHECK(run(car, slow, released, p.steer_fall_time * 1.05, {1.0, 0.0, 0.0}).packet.steering == doctest::Approx(0.0));
}

TEST_CASE("Throttle and brake rise smoothly with digital input") {
    const VehicleParams car = sports_coupe();
    const SmoothingParams p;
    RawInput raw;
    raw.throttle = 1.0;
    raw.brake = 1.0;

    const SmoothingResult first = smooth({}, p, car, straight_ahead(10.0), raw, 0, dt);
    CHECK(first.packet.throttle > 0.0);
    CHECK(first.packet.throttle < 0.1);
    CHECK(first.packet.brake > 0.0);
    CHECK(first.packet.brake < 0.1);

    const SmoothingResult half = run(car, straight_ahead(10.0), raw, p.throttle_rise_time / 2.0);
    CHECK(half.packet.throttle == doctest::Approx(0.5).epsilon(0.02));
    CHECK(run(car, straight_ahead(10.0), raw, 1.0).packet.throttle == doctest::Approx(1.0));

    RawInput released;
    CHECK(run(car, straight_ahead(10.0), released, p.throttle_fall_time * 1.05, {0.0, 1.0, 1.0}).packet.throttle ==
          doctest::Approx(0.0));
}

TEST_CASE("The steering limit drops with speed down to the peak slip angle") {
    const VehicleParams car = sports_coupe();
    CHECK(steering_limit(car, 2.0, 1.0) == doctest::Approx(1.0));
    CHECK(steering_limit(car, 15.0, 1.0) < steering_limit(car, 8.0, 1.0));
    CHECK(steering_limit(car, 40.0, 1.0) < steering_limit(car, 15.0, 1.0));
    // Lateral curve of the coupe: B = 14, C = 1.5, E = 0.3, peak at about 0.12 rad.
    const double angle = steering_limit(car, 100.0, 1.0) * car.steering.max_steer_angle; // rad
    CHECK(angle > 0.10);
    CHECK(angle < 0.17);
}

TEST_CASE("At high speed a full key press does not steer beyond the grip") {
    const VehicleParams car = sports_coupe();
    const double speed = 150.0 / 3.6; // m/s
    TestWorld world(car, 0.02, TestGround{}, speed);
    Drive drive(world);
    RawInput raw;
    raw.steering = 1.0;
    raw.gear_request = 5;

    SmoothingState state;
    double highest_lateral = 0.0; // m/s²
    double highest_drift = 0.0;   // rad
    for (int i = 0; i < Drive::ticks(4.0); ++i) {
        const BodyMotion& b = world.body();
        raw.throttle = 0.12; // about rolling on the straight, so no throttle drives the rear
        const double limit = steering_limit(car, std::hypot(b.velocity.x, b.velocity.y), SmoothingParams{}.peak_fraction);
        const SmoothingResult e = smooth(state, SmoothingParams{}, car, b, raw, 0, dt);
        state = e.state;
        drive.one_tick(e.packet);
        const Vec3 v = rotate_inverse(world.body().orientation, world.body().velocity);
        const Vec3 force = rotate_inverse(world.body().orientation, world.last_step().forces.force);
        highest_lateral = std::max(highest_lateral, std::abs(force.y) / car.mass);
        highest_drift = std::max(highest_drift, std::abs(std::atan2(v.y, v.x)));
        CHECK(std::abs(e.packet.steering) <= limit + 1e-3);
    }
    CAPTURE(highest_lateral);
    CAPTURE(highest_drift);
    CHECK(steering_limit(car, speed, SmoothingParams{}.peak_fraction) < 0.3);
    // Friction circle: a little above μ·g is possible thanks to downforce and load sensitivity.
    // The turn-in jolt overshoots briefly (inertia, relaxation), hence 1.3 instead of 1.05.
    CHECK(highest_lateral < 1.3 * 1.05 * TestWorld::gravity);
    // A spin goes toward 90° and more; here the car catches itself.
    CHECK(highest_drift < 0.5);
}

TEST_CASE("If the rear slides and the steering key is released, the countersteer aid steers in the direction of travel") {
    const VehicleParams car = sports_coupe();
    RawInput released;

    // Moves left of the longitudinal axis (vy > 0): the rear has stepped out to the right.
    BodyMotion left = straight_ahead(20.0);
    left.velocity = {20.0, 5.0, 0.0};
    CHECK(run(car, left, released, 0.3).packet.steering > 0.1);

    BodyMotion right = straight_ahead(20.0);
    right.velocity = {20.0, -5.0, 0.0};
    CHECK(run(car, right, released, 0.3).packet.steering < -0.1);

    // If the driver holds the key, their wish applies, not the aid.
    RawInput right_key;
    right_key.steering = -1.0;
    CHECK(run(car, left, right_key, 0.3).packet.steering < 0.0);

    // Without a drift angle the aid does not intervene.
    CHECK(run(car, straight_ahead(20.0), released, 0.3).packet.steering == doctest::Approx(0.0));
}

TEST_CASE("Equal raw signals and equal state yield the same input packet") {
    const VehicleParams car = sports_coupe();
    RawInput raw;
    raw.steering = 0.7;
    raw.throttle = 0.6;
    raw.gear_request = 3;
    BodyMotion b = straight_ahead(30.0);
    b.velocity.y = 2.0;
    const SmoothingState s{0.2, 0.3, 0.1};

    const SmoothingResult a = smooth(s, SmoothingParams{}, car, b, raw, 7, dt);
    const SmoothingResult c = smooth(s, SmoothingParams{}, car, b, raw, 7, dt);
    CHECK(a.packet.steering == c.packet.steering);
    CHECK(a.packet.throttle == c.packet.throttle);
    CHECK(a.packet.brake == c.packet.brake);
    CHECK(a.packet.tick == 7);
    CHECK(a.packet.gear_request == 3);
}
