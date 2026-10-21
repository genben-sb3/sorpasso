#include "doctest/doctest.h"

#include <bit>
#include <cmath>
#include <numbers>
#include <vector>

#include "drive.h"
#include "ultragravity/vehicle_step.h"

using namespace ultragravity;
using ultragravity::test::sports_coupe;

namespace {

bool bit_equal(double a, double b) { return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b); }
bool bit_equal(Vec3 a, Vec3 b) { return bit_equal(a.x, b.x) && bit_equal(a.y, b.y) && bit_equal(a.z, b.z); }

// Every wheel stands on level ground compressed by 4 cm.
WheelContacts compressed(const VehicleParams& params, const BodyMotion& body) {
    WheelContacts contacts;
    const auto rays = probe_rays(params, body);
    for (int i = 0; i < wheel_count; ++i) {
        const ProbeRay& r = rays[i];
        contacts[i] = {true, r.start + r.direction * (r.length - 0.04), {0.0, 0.0, 1.0}, 1.0};
    }
    return contacts;
}

// Keeps rolling tick by tick with a fixed body motion and returns the
// result of every tick. The body is a thousand times as heavy for this, so the
// substeps do not noticeably slow it down.
std::vector<StepResult> roll(VehicleParams params, const BodyMotion& body, int ticks,
                             const InputPacket& input = {}) {
    params.mass *= 1000.0;
    params.inertia = params.inertia * 1000.0;
    const WheelContacts contacts = compressed(params, body);
    State state = rolling_state(params, body.velocity.x);
    std::vector<StepResult> results;
    for (int i = 0; i < ticks; ++i) {
        results.push_back(vehicle_step(state, params, input, contacts, body, 1.0 / 120.0));
        state = results.back().state;
    }
    return results;
}

// Lateral force on the body after every tick.
std::vector<double> lateral_forces(const VehicleParams& params, const BodyMotion& body, int ticks) {
    std::vector<double> forces; // N
    for (const StepResult& e : roll(params, body, ticks)) forces.push_back(e.forces.force.y);
    return forces;
}

} // namespace

TEST_CASE("The vehicle step yields the same output for the same input") {
    const VehicleParams params = sports_coupe();

    BodyMotion body;
    body.position = {0.0, 0.0, 0.5};
    body.velocity = {12.0, 0.0, -0.3};
    body.angular_velocity = {0.05, -0.02, 0.0};

    const WheelContacts contacts = compressed(params, body);

    // The wheels turn slightly too slowly, the brake is half on: tires and brake are working.
    const State state = rolling_state(params, 11.0);
    InputPacket input;
    input.brake = 0.5;

    const StepResult first = vehicle_step(state, params, input, contacts, body, 1.0 / 120.0);
    const StepResult second = vehicle_step(state, params, input, contacts, body, 1.0 / 120.0);

    CHECK(first.forces.force.z > 0.0);
    CHECK(first.forces.force.x < 0.0);
    CHECK(bit_equal(first.forces.force, second.forces.force));
    CHECK(bit_equal(first.forces.torque, second.forces.torque));
    for (int i = 0; i < wheel_count; ++i) {
        CHECK(bit_equal(first.telemetry.wheel_load[i], second.telemetry.wheel_load[i]));
        CHECK(bit_equal(first.animation.suspension_travel[i], second.animation.suspension_travel[i]));
        CHECK(bit_equal(first.state.wheel_speed[i], second.state.wheel_speed[i]));
    }
}

namespace {

// Tires with a long relaxation length: the lag then spans many ticks even at a speed
// at which it acts fully (ADR 0002).
VehicleParams with_long_relaxation() {
    VehicleParams params = sports_coupe();
    params.front.tire.relaxation_length = 1.5;
    params.rear.tire.relaxation_length = 1.5;
    return params;
}

} // namespace

TEST_CASE("The lateral force builds up over the relaxation length") {
    const VehicleParams params = with_long_relaxation();
    const double sigma = params.front.tire.relaxation_length; // m

    // The body slides at 1° slip angle, just fast enough to roll one relaxation
    // length in 15 ticks. At 1° the tire is almost linear.
    const double ticks_per_sigma = 15.0; // ticks per relaxation length
    const double vx = sigma * 120.0 / ticks_per_sigma; // m/s
    REQUIRE(vx > 10.0);
    BodyMotion body;
    body.position = {0.0, 0.0, 0.5};
    body.velocity = {vx, -vx * std::tan(std::numbers::pi / 180.0), 0.0};

    const std::vector<double> force = lateral_forces(params, body, 240); // N
    const double steady = force.back(); // N
    CAPTURE(steady);
    REQUIRE(steady > 0.0);

    // First-order lag over the rolled distance: 1 − e^(−s/σ).
    CHECK(force[0] < 0.2 * steady); // measured: 0.039·steady, band: < 0.2·steady
    CHECK(force[14] / steady == doctest::Approx(1.0 - std::exp(-1.0)).epsilon(0.05)); // measured: +1.07 %, band: ±5 %
    CHECK(force[44] / steady == doctest::Approx(1.0 - std::exp(-3.0)).epsilon(0.02)); // measured: +0.29 %, band: ±2 %
}

TEST_CASE("At standstill the slip angle acts at once, without the relaxation lag") {
    // ADR 0002: with the lag the tire would be a hardly damped spring at standstill.
    const VehicleParams params = sports_coupe();
    BodyMotion body;
    body.position = {0.0, 0.0, 0.5};
    body.velocity = {0.0, -0.02, 0.0};

    const std::vector<double> force = lateral_forces(params, body, 240); // N
    const double steady = force.back(); // N
    CAPTURE(steady);
    REQUIRE(steady > 0.0);

    CHECK(force[0] == doctest::Approx(steady).epsilon(0.01)); // measured: equal to 6 digits, band: ±1 %
}

TEST_CASE("At a large slip angle the full sliding speed counts for the relaxation") {
    const VehicleParams params = with_long_relaxation();
    const double sigma = params.front.tire.relaxation_length; // m

    // The body slides at 30°, fast enough to cover one relaxation length in 15
    // ticks. Longitudinally alone it would be only 0.87 of that.
    const double speed = sigma * 120.0 / 15.0;               // m/s
    const double angle = 30.0 * std::numbers::pi / 180.0;    // rad
    REQUIRE(speed * std::cos(angle) > 10.0);
    BodyMotion body;
    body.position = {0.0, 0.0, 0.5};
    body.velocity = {speed * std::cos(angle), -speed * std::sin(angle), 0.0};

    const std::vector<StepResult> ticks = roll(params, body, 240);
    const double steady = ticks.back().state.slip_angle[rear_left]; // rad
    CAPTURE(steady);
    REQUIRE(steady < -0.3); // measured: -0.48 rad, band: < -0.3 rad

    // Over the sliding distance 1 − e^(−s/σ); over the longitudinal distance it would be
    // only 1 − e^(−0.87) = 0.58 after one relaxation length.
    // measured: -0.00006 %, band: ±3 %
    CHECK(ticks[14].state.slip_angle[rear_left] / steady ==
          doctest::Approx(1.0 - std::exp(-1.0)).epsilon(0.03));
}

TEST_CASE("Without ground contact the tire forgets its slip angle") {
    const VehicleParams params = sports_coupe();
    BodyMotion body;
    body.position = {0.0, 0.0, 0.5};
    body.velocity = {10.0, -1.0, 0.0};

    State state = roll(params, body, 60).back().state;
    REQUIRE(state.slip_angle[front_left] < -0.05); // measured: -0.099 rad, band: < -0.05 rad
    const WheelContacts in_the_air{};
    state = vehicle_step(state, params, {}, in_the_air, body, 1.0 / 120.0).state;

    for (int i = 0; i < wheel_count; ++i) CHECK(state.slip_angle[i] == 0.0);
}

TEST_CASE("Slip angle weakens the longitudinal force of a locked wheel (combined slip)") {
    const VehicleParams params = sports_coupe();
    REQUIRE(params.front.tire.combined.longitudinal == 8.0);
    REQUIRE(params.rear.tire.combined.longitudinal == 8.0);
    InputPacket full_brake;
    full_brake.brake = 1.0;

    // Longitudinal force when the wheels slide locked over the ground and the tire
    // has settled to the slip angle.
    const auto brake_force = [&](double slip_angle) {
        BodyMotion body;
        body.position = {0.0, 0.0, 0.5};
        body.velocity = {10.0, -10.0 * std::tan(slip_angle), 0.0};
        return roll(params, body, 120, full_brake).back().forces.force.x; // N
    };

    const double straight = brake_force(0.0);                            // N
    const double angled = brake_force(5.0 * std::numbers::pi / 180.0);   // N
    CAPTURE(straight);
    CAPTURE(angled);
    REQUIRE(straight < 0.0);
    // Weighting 1/√(1 + (B·α)²) with B = 8 and α ≈ 5° (the slip denominator turns
    // that into 4.97°): 0.82.
    CHECK(angled / straight == doctest::Approx(0.82).epsilon(0.01)); // measured: +0.53 %, band: ±1 %
}

TEST_CASE("The anti-roll bar only shifts load between wheels that are on the ground") {
    const VehicleParams with = sports_coupe();
    VehicleParams without = with;
    without.front.suspension.anti_roll_bar = 0.0;
    without.rear.suspension.anti_roll_bar = 0.0;
    BodyMotion body;
    body.position = {0.0, 0.0, 0.5};

    // Right side compressed 2 cm deeper than the left.
    WheelContacts contacts = compressed(with, body);
    for (int i : {front_right, rear_right}) contacts[i].point.z += 0.02;
    const auto wheel_loads = [&](const VehicleParams& k, const WheelContacts& c) {
        return vehicle_step(rolling_state(k, 0.0), k, {}, c, body, 1.0 / 120.0).telemetry.wheel_load;
    };

    // Both wheels down: the right one carries more, the sum of the axle stays.
    const auto with_load = wheel_loads(with, contacts);       // N
    const auto without_load = wheel_loads(without, contacts); // N
    CHECK(with_load[front_right] > without_load[front_right] + 100.0); // measured: +400 N, band: > +100 N
    CHECK(with_load[front_left] + with_load[front_right] ==
          doctest::Approx(without_load[front_left] + without_load[front_right]));

    // Left side in the air: the anti-roll bar has nothing to push against.
    contacts[front_left].contact = false;
    contacts[rear_left].contact = false;
    const auto lifted_with = wheel_loads(with, contacts);       // N
    const auto lifted_without = wheel_loads(without, contacts); // N
    CHECK(lifted_with[front_right] == doctest::Approx(lifted_without[front_right]));
    CHECK(lifted_with[rear_right] == doctest::Approx(lifted_without[rear_right]));
}
