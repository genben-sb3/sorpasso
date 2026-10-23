#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>

#include "drive.h"
#include "ultragravity/test_maneuver.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

double drift_angle(const TestWorld& world) {
    const BodyMotion& b = world.body();
    return drift_angle_magnitude(rotate_inverse(b.orientation, b.velocity)); // rad
}

struct Circle {
    double largest_drift = 0.0; // rad, after the first second
    BodyMotion end;
};

// Left-hand circle of 100 m radius with the steering controller of the test driver and a fixed throttle.
Circle drive_circle(bool esc, double speed_kmh, double throttle, double seconds, double esc_from = 0.0) {
    const VehicleParams car = sports_coupe();
    TestWorld world(car, 0.02, TestGround{}, speed_kmh * kmh);
    Drive drive(world);
    SteeringController steer(car);
    Circle c;
    const int ticks = Drive::ticks(seconds);
    for (int t = 0; t < ticks; ++t) {
        InputPacket p;
        p.gear_request = 3;
        p.throttle = throttle;
        p.clutch = throttle > 0.0 ? 0.0 : 1.0;
        p.steering = steer.circle(world.body(), 100.0);
        p.driving_aids.stability_control = esc && t >= Drive::ticks(esc_from);
        drive.one_tick(p);
        if (t > Drive::ticks(1.0)) c.largest_drift = std::max(c.largest_drift, drift_angle(world));
    }
    c.end = world.body();
    return c;
}

// Lane change at a fixed speed, then rolling out straight ahead.
struct Settling {
    OscillationDetector yaw_rate{0.002};
    double yaw_rate_at_end = 0.0; // rad/s
    double speed_at_end = 0.0;    // m/s
};

Settling lane_change_and_settle(bool esc, double speed_kmh) {
    const VehicleParams car = sports_coupe();
    TestWorld world(car, 0.02, TestGround{}, speed_kmh * kmh);
    Drive drive(world);
    SteeringController steer(car);
    const auto path = [](double x) {
        const auto smooth = [](double t) {
            t = std::clamp(t, 0.0, 1.0);
            return t * t * (3.0 - 2.0 * t);
        };
        return x < 80.0 ? smooth((x - 30.0) / 13.5) : 1.0 - smooth((x - 80.0) / 12.5);
    };
    Settling s;
    InputPacket p;
    p.clutch = 1.0;
    p.driving_aids.stability_control = esc;
    const double end_x = world.body().position.x + 200.0; // m
    while (world.body().position.x < end_x) {
        const double x = world.body().position.x; // m
        p.steering = steer.path(world.body(), path, std::max(3.0, 0.2 * speed_kmh * kmh));
        drive.one_tick(p);
        if (x > 120.0) s.yaw_rate.add(world.body().angular_velocity.z);
    }
    s.yaw_rate_at_end = world.body().angular_velocity.z;
    const Vec3 v = world.body().velocity;
    s.speed_at_end = std::hypot(v.x, v.y);
    return s;
}

} // namespace

TEST_CASE("In the lane change the car enters faster with stability control than without") {
    const VehicleParams car = sports_coupe();
    const Measurement without = run_test_maneuver(Maneuver::lane_change, car);
    const Measurement with = run_test_maneuver(Maneuver::lane_change_esc, car);

    REQUIRE_FALSE(without.failed());
    REQUIRE_FALSE(with.failed());
    CAPTURE(*without.result);
    CAPTURE(*with.result);
    CHECK(*with.result > *without.result + 3.0); // km/h; measured: 105.0 against 115.0 km/h (+10), band: at least +3
}

TEST_CASE("On a calm straight drive and a steady circle far from the limit it does not intervene") {
    // An intervention brakes or takes throttle away and would change the path: the same drive without it is bit-identical.
    // The circle runs in for 4 s until the driver has found the path (overshoot when turning in).
    for (double throttle : {0.0, 0.3}) {
        const Circle without = drive_circle(false, 70.0, throttle, 10.0);
        const Circle with = drive_circle(true, 70.0, throttle, 10.0, 4.0);
        CAPTURE(throttle);
        CHECK(with.end == without.end);
    }
    // Straight ahead with throttle.
    const VehicleParams car = sports_coupe();
    BodyMotion ends[2];
    for (int esc = 0; esc < 2; ++esc) {
        TestWorld world(car, 0.02, TestGround{}, 100.0 * kmh);
        Drive drive(world);
        InputPacket p = drive_input(0.5);
        p.driving_aids.stability_control = esc == 1;
        drive.run(6.0, p);
        ends[esc] = world.body();
    }
    CHECK(ends[1] == ends[0]);
}

TEST_CASE("Switched off the car behaves exactly as without stability control") {
    // At the limit, where it would intervene, off is bit-identical to the default packet; on is not.
    const Circle off1 = drive_circle(false, 50.0, 1.0, 12.0);
    const Circle off2 = drive_circle(false, 50.0, 1.0, 12.0);
    const Circle on = drive_circle(true, 50.0, 1.0, 12.0);
    CHECK(off1.end == off2.end);
    CHECK_FALSE(on.end == off1.end);

    InputPacket packet;
    CHECK_FALSE(packet.driving_aids.stability_control);
}

TEST_CASE("With stability control no circle drift can be held") {
    const Circle without = drive_circle(false, 50.0, 1.0, 12.0);
    const Circle with = drive_circle(true, 50.0, 1.0, 12.0);

    CAPTURE(without.largest_drift);
    CAPTURE(with.largest_drift);
    CHECK(without.largest_drift > 0.5); // rad; counter-check, measured: 1.565 (spin), band: ×0.32
    CHECK(with.largest_drift < 0.15);   // rad; measured: 0.060, band: ×2.5
}

TEST_CASE("Its intervention creates no new oscillation; after catching the car it calms down") {
    for (double speed : {90.0, 105.0}) {
        const Settling with = lane_change_and_settle(true, speed);
        CAPTURE(speed);
        CHECK(with.yaw_rate.longest_run() < 3); // measured: 0, band: < 3
        CHECK(std::abs(with.yaw_rate_at_end) < 0.02); // rad/s; measured: below 1e-8 rad/s, band: < 0.02 rad/s
        CHECK(with.speed_at_end > 5.0); // m/s; measured: 20.2 and 23.9 m/s, band: > 5 m/s
    }
}
