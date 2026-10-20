#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "drive.h"
#include "ultragravity/animation_data.h"
#include "ultragravity/checksum.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

constexpr double pi = std::numbers::pi;

WheelContacts compressed(const VehicleParams& params, const BodyMotion& body, double compression = 0.04) {
    WheelContacts contacts;
    const auto rays = probe_rays(params, body);
    for (int i = 0; i < wheel_count; ++i) {
        const ProbeRay& r = rays[i];
        contacts[i] = {true, r.start + r.direction * (r.length - compression), {0.0, 0.0, 1.0}, 1.0};
    }
    return contacts;
}

} // namespace

TEST_CASE("When braking the front wheels compress deeper than the rear ones and the body pitches forward") {
    TestWorld world(sports_coupe(), 0.02, TestGround{}, kmh_100);
    Drive drive(world);
    drive.run(1.5);
    const AnimationData rolling = world.last_step().animation;
    CHECK(std::abs(rolling.pitch) < 0.02);

    InputPacket brake;
    brake.brake = 1.0;
    drive.run(1.0, brake);
    const AnimationData a = world.last_step().animation;

    CHECK(a.pitch > 0.01); // nose down is positive (ISO 8855)
    CHECK(a.suspension_travel[front_left] > rolling.suspension_travel[front_left] + 0.01);
    CHECK(a.suspension_travel[rear_left] < rolling.suspension_travel[rear_left] - 0.005);
    CHECK(a.suspension_travel[front_right] == doctest::Approx(a.suspension_travel[front_left]).epsilon(0.05));
    // The pitch is the incline of the longitudinal axis.
    const Vec3 nose = rotate(world.body().orientation, {1.0, 0.0, 0.0});
    CHECK(a.pitch == doctest::Approx(std::asin(-nose.z)));
}

TEST_CASE("In a left-hand corner the front wheels and the steering wheel point left, the body rolls outward") {
    const VehicleParams car = sports_coupe();
    TestWorld world(car, 0.02, TestGround{}, 20.0);
    Drive drive(world);
    drive.run(1.0);

    InputPacket left;
    left.steering = 0.5;
    left.throttle = 0.3;
    left.gear_request = 4;
    drive.run(3.0, left);
    const AnimationData a = world.last_step().animation;

    CHECK(a.steer_angle[front_left] == doctest::Approx(0.5 * car.steering.max_steer_angle));
    CHECK(a.steer_angle[front_right] == doctest::Approx(0.5 * car.steering.max_steer_angle));
    CHECK(a.steer_angle[rear_left] == 0.0);
    CHECK(a.steer_angle[rear_right] == 0.0);
    CHECK(a.steering_wheel == doctest::Approx(0.5 * 2.5 * pi)); // 225° at half lock
    CHECK(a.roll > 0.0);                                        // left up, right down
    CHECK(a.suspension_travel[front_right] > a.suspension_travel[front_left]);
    CHECK(a.engine_speed == doctest::Approx(world.last_step().state.engine_speed));
}

TEST_CASE("The wheel angle follows the wheel speed and stays in the range -π to π") {
    const VehicleParams params = sports_coupe();
    BodyMotion body;
    body.position = {0.0, 0.0, 0.5};
    body.velocity = {30.0, 0.0, 0.0};
    const WheelContacts contacts = compressed(params, body);
    State state = rolling_state(params, 30.0);

    const double dt = 1.0 / 120.0;
    int wraps = 0;
    double previous = 0.0;
    for (int tick = 0; tick < 240; ++tick) {
        const StepResult e = vehicle_step(state, params, {}, contacts, body, dt);
        const double angle = e.animation.wheel_angle[front_left];
        CHECK(angle >= -pi);
        CHECK(angle < pi);
        // Per tick the wheel turns by about ω·dt.
        const double step = wrap_angle(angle - previous);
        CHECK(step == doctest::Approx(e.state.wheel_speed[front_left] * dt).epsilon(0.02));
        if (angle < previous) ++wraps;
        previous = angle;
        state = e.state;
    }
    CHECK(wraps > 10);
}

TEST_CASE("The interpolation yields exactly the two ticks at fraction 0 and 1") {
    AnimationData a, b;
    a.wheel_angle = {0.1, 3.0, -2.0, 0.0};
    b.wheel_angle = {0.9, -3.0, -1.0, 0.5};
    a.suspension_travel = {0.03, 0.04, 0.05, 0.06};
    b.suspension_travel = {0.07, 0.08, 0.09, 0.1};
    a.steer_angle[front_left] = 0.1;
    b.steer_angle[front_left] = 0.3;
    a.pitch = 0.01;
    b.pitch = 0.03;
    a.roll = -0.02;
    b.roll = 0.02;
    a.steering_wheel = 1.0;
    b.steering_wheel = 2.0;
    a.engine_speed = 300.0;
    b.engine_speed = 400.0;

    const auto equal = [](const AnimationData& x, const AnimationData& y) {
        for (int i = 0; i < wheel_count; ++i) {
            CHECK(x.wheel_angle[i] == y.wheel_angle[i]);
            CHECK(x.steer_angle[i] == y.steer_angle[i]);
            CHECK(x.suspension_travel[i] == y.suspension_travel[i]);
        }
        CHECK(x.pitch == y.pitch);
        CHECK(x.roll == y.roll);
        CHECK(x.steering_wheel == y.steering_wheel);
        CHECK(x.engine_speed == y.engine_speed);
    };
    equal(interpolate(a, b, 0.0), a);
    equal(interpolate(a, b, 1.0), b);

    const AnimationData half = interpolate(a, b, 0.5);
    CHECK(half.suspension_travel[0] == doctest::Approx(0.05));
    CHECK(half.pitch == doctest::Approx(0.02));
    CHECK(half.engine_speed == doctest::Approx(350.0));
    CHECK(half.wheel_angle[0] == doctest::Approx(0.5));
}

TEST_CASE("Wheel angles do not jump in the interpolation at the wrap") {
    AnimationData a, b;
    a.wheel_angle[0] = pi - 0.1;
    b.wheel_angle[0] = -pi + 0.3; // the wheel has passed π and turns 0.4 further

    double previous = a.wheel_angle[0];
    for (int i = 1; i <= 40; ++i) {
        const double angle = interpolate(a, b, i / 40.0).wheel_angle[0];
        CHECK(angle >= -pi);
        CHECK(angle < pi);
        CHECK(std::abs(wrap_angle(angle - previous)) == doctest::Approx(0.4 / 40.0).epsilon(1e-6));
        previous = angle;
    }
}

TEST_CASE("The interpolation between real ticks is continuous") {
    TestWorld world(sports_coupe(), 0.02, TestGround{}, kmh_100);
    Drive drive(world);
    drive.run(1.0);
    InputPacket packet;
    packet.brake = 0.5;
    packet.steering = 0.2;
    drive.one_tick(packet);
    const AnimationData a = world.last_step().animation;
    drive.one_tick(packet);
    const AnimationData b = world.last_step().animation;

    AnimationData previous = a;
    for (int i = 1; i <= 100; ++i) {
        const AnimationData x = interpolate(a, b, i / 100.0);
        for (int r = 0; r < wheel_count; ++r) {
            CHECK(std::abs(wrap_angle(x.wheel_angle[r] - previous.wheel_angle[r])) < 0.1);
            CHECK(std::abs(x.suspension_travel[r] - previous.suspension_travel[r]) < 0.005);
        }
        previous = x;
    }
}

TEST_CASE("Without ground contact of a wheel no tire force arises at that wheel") {
    // The air acts without ground contact too; this is about the tires, so without aero.
    VehicleParams params = sports_coupe();
    // The plausibility check demands positive values; this one is small enough to neglect aero.
    constexpr double negligible_drag_area = 1e-9; // m²
    params.aero.drag_area = negligible_drag_area;
    params.front.downforce = 0.0;
    params.rear.downforce = 0.0;
    // A limited-slip differential would drag the wheel in the air along via its preload.
    params.drivetrain.differential = {};
    BodyMotion body;
    body.position = {0.0, 0.0, 0.5};
    body.velocity = {20.0, 3.0, 0.0};
    const WheelContacts contacts = compressed(params, body);
    const State state = rolling_state(params, 15.0); // wheels slide: strong slip
    InputPacket input;
    input.brake = 1.0;

    // All wheels in the air: no force, no torque.
    WheelContacts air = contacts;
    for (WheelContact& c : air) c.contact = false;
    const StepResult airborne = vehicle_step(state, params, input, air, body, 1.0 / 120.0);
    CHECK(std::abs(airborne.forces.force.x) < 1e-6);
    CHECK(std::abs(airborne.forces.force.y) < 1e-6);
    CHECK(std::abs(airborne.forces.force.z) < 1e-6);
    CHECK(std::abs(airborne.forces.torque.x) < 1e-6);
    CHECK(std::abs(airborne.forces.torque.y) < 1e-6);
    CHECK(std::abs(airborne.forces.torque.z) < 1e-6);
    for (int i = 0; i < wheel_count; ++i) {
        CHECK(airborne.animation.suspension_travel[i] == 0.0);
        CHECK(airborne.telemetry.wheel_load[i] == 0.0);
    }

    // Only one wheel in the air: without the brake it keeps its speed, while the
    // wheel in contact is accelerated by the tire.
    input.brake = 0.0;
    WheelContacts three = contacts;
    three[rear_left].contact = false;
    const StepResult e3 = vehicle_step(state, params, input, three, body, 1.0 / 120.0);
    const StepResult e4 = vehicle_step(state, params, input, contacts, body, 1.0 / 120.0);
    CHECK(e3.state.wheel_speed[rear_left] == state.wheel_speed[rear_left]);
    CHECK(e4.state.wheel_speed[rear_left] != state.wheel_speed[rear_left]);
    CHECK(e3.telemetry.wheel_load[rear_left] == 0.0);
    CHECK(e3.forces.force.x > e4.forces.force.x); // the sliding wheel no longer helps braking
}

TEST_CASE("A vehicle step only changes its result and has no hidden state") {
    const VehicleParams params = sports_coupe();
    BodyMotion body;
    body.position = {0.0, 0.0, 0.5};
    body.velocity = {12.0, 1.0, -0.3};
    const WheelContacts contacts = compressed(params, body);
    const State state = rolling_state(params, 11.0);
    InputPacket input;
    input.steering = 0.3;
    input.throttle = 0.5;
    input.gear_request = 1;

    const std::uint64_t sum_before = checksum(state, body);
    const WheelContacts contacts_before = contacts;

    const StepResult first = vehicle_step(state, params, input, contacts, body, 1.0 / 120.0);
    CHECK(checksum(state, body) == sum_before);
    for (int i = 0; i < wheel_count; ++i) {
        CHECK(contacts[i].contact == contacts_before[i].contact);
        CHECK(contacts[i].point.z == contacts_before[i].point.z);
    }

    // Another step in between (other car, other state, other input)
    // does not change the following step with the same inputs.
    VehicleParams other = params;
    other.mass *= 1.5;
    BodyMotion fast = body;
    fast.velocity = {40.0, -4.0, 0.0};
    InputPacket brake;
    brake.brake = 1.0;
    (void)vehicle_step(rolling_state(other, 40.0), other, brake, compressed(other, fast, 0.08), fast, 1.0 / 60.0);

    const StepResult second = vehicle_step(state, params, input, contacts, body, 1.0 / 120.0);
    CHECK(checksum(first.state, body) == checksum(second.state, body));
    CHECK(first.forces.force.x == second.forces.force.x);
    CHECK(first.forces.torque.z == second.forces.torque.z);
    for (int i = 0; i < wheel_count; ++i) {
        CHECK(first.animation.wheel_angle[i] == second.animation.wheel_angle[i]);
        CHECK(first.animation.suspension_travel[i] == second.animation.suspension_travel[i]);
    }
    CHECK(first.animation.pitch == second.animation.pitch);
    CHECK(first.animation.steering_wheel == second.animation.steering_wheel);
}
