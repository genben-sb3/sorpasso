#include "doctest/doctest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

#include "drive.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

double rear_axle_speed(const TestWorld& world) {
    const auto& omega = world.last_step().state.wheel_speed;
    return (omega[rear_left] + omega[rear_right]) / 2.0;
}

} // namespace

TEST_CASE("In neutral and with the clutch pressed the engine holds its idle speed") {
    const VehicleParams car = sports_coupe();
    const double idle = car.drivetrain.engine.idle_speed;

    SUBCASE("at standstill without a gear") {
        TestWorld world(car, 0.02);
        Drive drive(world);
        for (int tick = 0; tick < Drive::ticks(3.0); ++tick) {
            drive.one_tick({});
            // measured: 93.94 to 94.04 rad/s (-0.3 %), band: ±2 %
            CHECK(engine_speed(world) == doctest::Approx(idle).epsilon(0.02));
        }
    }

    SUBCASE("rolling in third gear with the clutch pressed") {
        TestWorld world(car, 0.02, TestGround{}, kmh_100);
        Drive drive(world);
        InputPacket pressed;
        pressed.gear_request = 3;
        pressed.clutch = 1.0;
        for (int tick = 0; tick < Drive::ticks(3.0); ++tick) {
            drive.one_tick(pressed);
            // measured: 93.94 to 94.04 rad/s (-0.3 %), band: ±2 %
            CHECK(engine_speed(world) == doctest::Approx(idle).epsilon(0.02));
        }
        // Without drive the engine does not slow the car; only the driving resistances
        // cost about 1 m/s in 3 s (see aero_test.cpp).
        CHECK(forward_speed(world) < kmh_100); // measured: 26.74 m/s, band: < 27.78 m/s
        CHECK(forward_speed(world) > kmh_100 - 1.5); // measured: 26.74 m/s, band: > 26.28 m/s
    }
}

TEST_CASE("The car pulls away from standstill, shifts up to sixth gear and does 0–100 km/h in a plausible time") {
    const VehicleParams car = sports_coupe();
    TestWorld world(car, 0.02);
    Drive drive(world);
    drive.run(1.5); // let the suspension settle
    TestDriver driver(car);

    double time = -TestDriver::engage; // s from the first throttle input
    double zero_to_hundred = 0.0;
    while (driver.gear() < 6 || !driver.clutch_engaged()) {
        REQUIRE(time < 60.0);
        drive.one_tick(driver.packet(world));
        time += TestWorld::time_step;
        if (zero_to_hundred == 0.0 && forward_speed(world) >= kmh_100) zero_to_hundred = time;
    }

    // Lower limit: the rear axle transmits at most μ times its dynamic
    // load, a = μ·g·l_f / (L − μ·h). A real car with 300 hp and 1350 kg
    // needs about 5 s. The upper band lies just above the value of the test maneuver
    // (tuning tool); the target value is tuned in package ticket 18.
    const double mu = car.rear.tire.peak_friction;
    const double wheelbase = car.front.x - car.rear.x;
    const double l_f = car.front.x - car.center_of_mass.x;
    const double h = car.center_of_mass.z;
    const double max_acceleration = mu * TestWorld::gravity * l_f / (wheelbase - mu * h);
    CAPTURE(zero_to_hundred);
    CHECK(zero_to_hundred > kmh_100 / max_acceleration); // measured: 5.85 s, band: > 3.63 s
    CHECK(zero_to_hundred < 6.2); // measured: 5.85 s, band: ×1.06

    // In sixth gear the clutch sticks: engine and rear axle turn in the ratio of the gearing.
    drive.run(0.5, driver.packet(world));
    const double overall = car.drivetrain.gearbox.ratios[5] * car.drivetrain.gearbox.final_drive;
    // measured: 584.98 rad/s (deviation below 1e-5), band: ±1 %
    CHECK(engine_speed(world) == doctest::Approx(rear_axle_speed(world) * overall).epsilon(0.01));
}

TEST_CASE("Pulling away and gear changes set neither engine nor wheel speed oscillating") {
    const VehicleParams car = sports_coupe();
    TestWorld world(car, 0.02);
    Drive drive(world);
    drive.run(1.5);
    TestDriver driver(car);

    OscillationDetector engine;
    std::array<OscillationDetector, wheel_count> wheels;
    while (driver.gear() < 4 || !driver.clutch_engaged()) {
        REQUIRE(world.last_step().state.engine_speed >= 0.0);
        drive.one_tick(driver.packet(world));
        engine.add(engine_speed(world));
        for (int i = 0; i < wheel_count; ++i) wheels[i].add(world.last_step().state.wheel_speed[i]);
    }

    CHECK(engine.longest_run() < 3); // measured: 1, band: < 3
    for (const OscillationDetector& wheel : wheels) CHECK(wheel.longest_run() < 3); // measured: at most 2, band: < 3
}

TEST_CASE("Lifting off in gear slows the car through engine braking") {
    const VehicleParams car = sports_coupe();
    TestWorld in_gear(car, 0.02, TestGround{}, kmh_100);
    TestWorld in_neutral(car, 0.02, TestGround{}, kmh_100);
    Drive drive_in_gear(in_gear);
    Drive drive_in_neutral(in_neutral);

    // Engage third gear with the clutch pressed and then let it out.
    InputPacket third;
    third.gear_request = 3;
    third.clutch = 1.0;
    drive_in_gear.run(0.5, third);
    for (int tick = 0; tick < Drive::ticks(0.5); ++tick) {
        third.clutch = 1.0 - static_cast<double>(tick) / Drive::ticks(0.5);
        drive_in_gear.one_tick(third);
    }
    third.clutch = 0.0;
    drive_in_neutral.run(1.0);

    for (int tick = 0; tick < Drive::ticks(2.0); ++tick) {
        const double v_before = forward_speed(in_gear);
        drive_in_gear.one_tick(third);
        drive_in_neutral.one_tick({});
        CHECK(forward_speed(in_gear) < v_before);
    }

    // About 670 N of drag force at 4600 rpm decelerates by roughly 0.45 m/s² more than
    // the driving resistances in neutral.
    CHECK(forward_speed(in_neutral) - forward_speed(in_gear) > 0.5); // measured: 1.82 m/s, band: > 0.5 m/s
}

TEST_CASE("With too much throttle when pulling away the rear wheels spin") {
    const VehicleParams car = sports_coupe();
    TestWorld world(car, 0.02);
    Drive drive(world);
    drive.run(1.5);

    // Engage first gear, rev up and let the clutch snap.
    InputPacket first;
    first.gear_request = 1;
    first.clutch = 1.0;
    drive.run(0.3, first);
    first.throttle = 1.0;
    drive.run(0.3, first);
    first.clutch = 0.0;
    drive.run(1.0, first);

    const double v = forward_speed(world);
    const auto& omega = world.last_step().state.wheel_speed;
    CAPTURE(v);
    REQUIRE(v > 1.0); // measured: 3.32 m/s, band: > 1.0 m/s
    // measured: 6.5·v, band: > 1.5·v
    for (int i : {rear_left, rear_right}) CHECK(omega[i] * car.rear.wheel_radius > 1.5 * v);
    // measured: -0.1 %, band: ±2 %
    for (int i : {front_left, front_right}) CHECK(omega[i] * car.front.wheel_radius == doctest::Approx(v).epsilon(0.02));
}

TEST_CASE("The open differential lets the rear wheel on a slippery surface spin, the other not") {
    VehicleParams car = sports_coupe();
    car.drivetrain.differential = {};
    // Right of the center line it is slippery; the car stands on it with its right wheels.
    TestGround slippery_right;
    slippery_right.surface_zones.push_back(
        {.x_from = -100.0, .x_to = 100.0, .y_from = -10.0, .y_to = 0.0, .friction = 0.2});

    // Half throttle in first gear, let the clutch out briskly.
    const auto pull_away = [&](TestWorld& world) {
        Drive drive(world);
        drive.run(1.5);
        InputPacket first;
        first.gear_request = 1;
        first.clutch = 1.0;
        drive.run(0.3, first);
        first.throttle = 0.5;
        for (int tick = 0; tick < Drive::ticks(0.3); ++tick) {
            first.clutch = 1.0 - static_cast<double>(tick) / Drive::ticks(0.3);
            drive.one_tick(first);
        }
        first.clutch = 0.0;
        drive.run(0.7, first);
    };
    TestWorld world(car, 0.02, slippery_right);
    TestWorld dry(car, 0.02);
    pull_away(world);
    pull_away(dry);

    const double v = forward_speed(world);
    const auto& omega = world.last_step().state.wheel_speed;
    const double r = car.rear.wheel_radius;
    CAPTURE(v);
    REQUIRE(v > 0.1); // measured: 0.80 m/s, band: > 0.1 m/s
    // The speeds drift apart freely: the right one spins, the left rolls almost without slip.
    CHECK(omega[rear_right] * r > v + 5.0); // measured: v + 34.4 m/s, band: > v + 5.0 m/s
    CHECK(omega[rear_left] * r < v + 0.5); // measured: v + 0.05 m/s, band: < v + 0.5 m/s
    // Both sides get the same torque, so no more than the slippery wheel
    // supports: the car gets away much worse than on a dry surface.
    CHECK(v < 0.3 * forward_speed(dry)); // measured: 0.21 of dry, band: < 0.3 of dry
}

namespace {

// The gears a run engages one after another, together with the time each gear was held.
struct GearSequence {
    std::vector<int> gears;
    std::vector<double> hold_times; // s, the last gear is not included

    void add(int g, double time) {
        if (gears.empty() || g != gears.back()) {
            if (!gears.empty()) hold_times.push_back(time - since_);
            gears.push_back(g);
            since_ = time;
        }
    }

private:
    double since_ = 0.0; // s
};

} // namespace

TEST_CASE("The automatic pulls away, shifts up at full throttle and does not hunt") {
    const VehicleParams car = with_automatic();
    TestWorld world(car, 0.02);
    Drive drive(world);
    drive.run(1.5);

    GearSequence sequence;
    const int ticks = Drive::ticks(20.0);
    for (int tick = 0; tick < ticks; ++tick) {
        drive.one_tick(drive_input(1.0));
        sequence.add(gear(world), tick * TestWorld::time_step);
    }

    CAPTURE(sequence.gears.size());
    CHECK(forward_speed(world) > 40.0); // measured: 60.3 m/s, band: > 40 m/s
    REQUIRE(sequence.gears.size() >= 5); // measured: 5 gears, band: >= 5
    // Only upward, one gear after the other, and every gear is held.
    for (std::size_t i = 1; i < sequence.gears.size(); ++i) CHECK(sequence.gears[i] == sequence.gears[i - 1] + 1);
    // measured: at least 2.0 s, band: >= 0.8 s (hold_time)
    for (double time : sequence.hold_times) CHECK(time >= car.drivetrain.gearbox.automatic.hold_time);
}

TEST_CASE("The automatic shifts down when braking and does not shift back up meanwhile") {
    const VehicleParams car = with_automatic();
    TestWorld world(car, 0.02);
    Drive drive(world);
    drive.run(1.5);
    drive.run(14.0, drive_input(1.0));
    const int top = gear(world);
    REQUIRE(top >= 4); // measured: 5, band: >= 4

    GearSequence sequence;
    sequence.add(top, 0.0);
    const int ticks = Drive::ticks(8.0);
    for (int tick = 0; tick < ticks && forward_speed(world) > 1.0; ++tick) {
        drive.one_tick(drive_input(0.0, 0.5));
        sequence.add(gear(world), tick * TestWorld::time_step);
    }

    CHECK(forward_speed(world) < 1.0); // measured: 0.95 m/s, band: < 1.0 m/s
    CHECK(gear(world) < top);
    for (std::size_t i = 1; i < sequence.gears.size(); ++i) CHECK(sequence.gears[i] == sequence.gears[i - 1] - 1);
}

TEST_CASE("Spinning wheels do not trigger an upshift in the automatic") {
    const VehicleParams car = with_automatic();
    TestGround slippery;
    slippery.friction = 0.15;
    TestWorld world(car, 0.02, slippery);
    Drive drive(world);
    drive.run(1.5);

    double highest_speed = 0.0; // rad/s
    for (int tick = 0; tick < Drive::ticks(5.0); ++tick) {
        drive.one_tick(drive_input(1.0));
        highest_speed = std::max(highest_speed, engine_speed(world));
        REQUIRE(gear(world) == 1);
    }
    // The wheels spin and pull the engine far above the shift speed,
    // in a car that is nowhere near fast enough for second gear.
    // measured: 755.3 rad/s, band: > 680.7 rad/s (upshift_speed)
    CHECK(highest_speed > car.drivetrain.gearbox.automatic.upshift_speed);
    CHECK(forward_speed(world) < 10.0); // measured: 1.59 m/s, band: < 10 m/s
}

TEST_CASE("In a drift the automatic holds the gear") {
    const VehicleParams car = with_automatic();
    TestGround slippery;
    slippery.friction = 0.4;
    TestWorld world(car, 0.02, slippery, 80.0 * kmh);
    Drive drive(world);
    drive.run(1.0, drive_input(0.3)); // engage a gear and roll along
    const double drift_angle = car.drivetrain.gearbox.automatic.drift_angle;

    int in_drift = 0;
    int gear_in_drift = 0;
    for (int tick = 0; tick < Drive::ticks(4.0); ++tick) {
        drive.one_tick(drive_input(1.0, 0.0, 1.0));
        const BodyMotion& b = world.body();
        const double vx = dot(b.velocity, rotate(b.orientation, {1.0, 0.0, 0.0}));
        const double vy = dot(b.velocity, rotate(b.orientation, {0.0, 1.0, 0.0}));
        if (length(b.velocity) > 2.0 && std::atan2(std::abs(vy), std::abs(vx)) > drift_angle) {
            if (in_drift == 0) gear_in_drift = gear(world);
            ++in_drift;
            CHECK(gear(world) == gear_in_drift);
        }
    }
    CAPTURE(in_drift);
    // measured: 319 ticks, band: > 120 ticks (1 s)
    CHECK(in_drift > Drive::ticks(1.0)); // the test really produced a drift
}

namespace {

// Accelerates out of a corner: first firmly steered at a steady speed,
// then full throttle. Returns the mean speed difference of the rear wheels
// during the acceleration.
double speed_difference_out_of_corner(const VehicleParams& car) {
    TestWorld world(car, 0.02, TestGround{}, 40.0 * kmh);
    Drive drive(world);
    InputPacket second;
    second.gear_request = 2;
    second.steering = 0.5;
    second.clutch = 1.0;
    drive.run(0.5, second);
    second.clutch = 0.0;
    second.throttle = 0.3;
    drive.run(2.0, second);
    second.throttle = 1.0;
    double sum = 0.0; // rad/s
    const int ticks = Drive::ticks(1.0);
    for (int tick = 0; tick < ticks; ++tick) {
        drive.one_tick(second);
        const auto& omega = world.last_step().state.wheel_speed;
        sum += std::abs(omega[rear_left] - omega[rear_right]);
    }
    return sum / ticks;
}

} // namespace

TEST_CASE("With a limited-slip differential the rear wheel speed difference out of a corner is smaller than with an open one") {
    VehicleParams open = sports_coupe();
    open.drivetrain.differential = {};
    const VehicleParams locked = sports_coupe();
    REQUIRE(locked.drivetrain.differential.limited_slip);

    const double difference_open = speed_difference_out_of_corner(open);
    const double difference_locked = speed_difference_out_of_corner(locked);

    CAPTURE(difference_open);
    CAPTURE(difference_locked);
    REQUIRE(difference_open > 0.5); // measured: 85.2 rad/s, band: > 0.5 rad/s
    CHECK(difference_locked < 0.7 * difference_open); // measured: 4.7e-16 rad/s (about 0·open), band: < 0.7·open
}

TEST_CASE("The limited-slip differential gives more traction than the open one on a surface slippery on one side") {
    VehicleParams open = sports_coupe();
    open.drivetrain.differential = {};
    const VehicleParams locked = sports_coupe();
    TestGround slippery_right;
    slippery_right.surface_zones.push_back(
        {.x_from = -100.0, .x_to = 100.0, .y_from = -10.0, .y_to = 0.0, .friction = 0.2});

    const auto speed_after_pulling_away = [&](const VehicleParams& k) {
        TestWorld world(k, 0.02, slippery_right);
        Drive drive(world);
        drive.run(1.5);
        InputPacket first;
        first.gear_request = 1;
        first.clutch = 1.0;
        drive.run(0.3, first);
        first.throttle = 0.5;
        for (int tick = 0; tick < Drive::ticks(0.3); ++tick) {
            first.clutch = 1.0 - static_cast<double>(tick) / Drive::ticks(0.3);
            drive.one_tick(first);
        }
        first.clutch = 0.0;
        drive.run(1.5, first);
        return forward_speed(world);
    };

    // measured: 1.71·open, band: > 1.5·open
    CHECK(speed_after_pulling_away(locked) > 1.5 * speed_after_pulling_away(open));
}

TEST_CASE("Front- and all-wheel drive pull away, accelerate and brake stably") {
    struct Case {
        const char* name;
        DriveLayout layout;
        double front; // front axle share
    };
    for (const Case& c : {Case{"front", DriveLayout::front, 0.0}, Case{"all_wheel", DriveLayout::all_wheel, 0.4}}) {
        const std::string name = c.name;
        CAPTURE(name);
        VehicleParams car = sports_coupe();
        car.drivetrain.layout = c.layout;
        car.drivetrain.front_torque_split = c.front;
        TestWorld world(car, 0.02);
        Drive drive(world);
        drive.run(1.5);
        TestDriver driver(car);

        OscillationDetector engine;
        double highest_yaw_rate = 0.0; // rad/s
        // In first gear the test driver gives more throttle than a front-wheel-drive car can
        // transmit; it then shifts early, and the car gets up to speed later.
        for (int tick = 0; tick < Drive::ticks(12.0); ++tick) {
            REQUIRE(world.last_step().state.engine_speed >= 0.0);
            drive.one_tick(driver.packet(world));
            engine.add(engine_speed(world));
            highest_yaw_rate = std::max(highest_yaw_rate, std::abs(world.body().angular_velocity.z));
        }
        const double v = forward_speed(world);
        CAPTURE(v);
        CHECK(v > 15.0); // measured: 42.6 m/s (front), 45.3 m/s (all-wheel), band: > 15 m/s
        CHECK(engine.longest_run() < 3); // measured: 1, band: < 3
        CHECK(highest_yaw_rate < 0.05); // measured: 0 rad/s, band: < 0.05 rad/s
        CHECK(std::abs(world.body().position.y) < 0.3); // measured: 0 m, band: < 0.3 m

        // Braking with the clutch pressed until standstill.
        InputPacket braking;
        braking.gear_request = static_cast<std::int8_t>(driver.gear());
        braking.clutch = 1.0;
        braking.brake = 0.4;
        for (int tick = 0; tick < Drive::ticks(10.0) && forward_speed(world) > 0.5; ++tick) {
            drive.one_tick(braking);
            highest_yaw_rate = std::max(highest_yaw_rate, std::abs(world.body().angular_velocity.z));
        }
        CHECK(forward_speed(world) < 0.5); // measured: 0.46 to 0.50 m/s, band: < 0.5 m/s
        CHECK(highest_yaw_rate < 0.05); // measured: 0 rad/s, band: < 0.05 rad/s
        CHECK(std::abs(world.body().position.y) < 0.5); // measured: 0 m, band: < 0.5 m
    }
}

TEST_CASE("In reverse with throttle the car pulls away backward and its wheels turn backward") {
    const VehicleParams car = sports_coupe();
    TestWorld world(car, 0.02);
    Drive drive(world);
    drive.run(1.5);

    InputPacket reverse;
    reverse.gear_request = -1;
    reverse.clutch = 1.0;
    drive.run(0.5, reverse);
    REQUIRE(gear(world) == -1);

    OscillationDetector engine;
    std::array<OscillationDetector, wheel_count> wheels;
    double previous_angle = world.last_step().animation.wheel_angle[rear_left]; // rad
    int backward_ticks = 0;
    reverse.throttle = 0.3;
    for (int tick = 0; tick < Drive::ticks(2.5); ++tick) {
        reverse.clutch = std::max(1.0 - static_cast<double>(tick) / Drive::ticks(0.5), 0.0);
        drive.one_tick(reverse);
        engine.add(engine_speed(world));
        for (int i = 0; i < wheel_count; ++i) wheels[i].add(world.last_step().state.wheel_speed[i]);
        const double angle = world.last_step().animation.wheel_angle[rear_left]; // rad
        if (wrap_angle(angle - previous_angle) < 0.0) ++backward_ticks;
        previous_angle = angle;
    }

    const double v = forward_speed(world);
    CAPTURE(v);
    CHECK(v < -2.0); // measured: -5.19 m/s, band: < -2.0 m/s
    CHECK(gear(world) == -1);
    for (int i = 0; i < wheel_count; ++i) CHECK(world.last_step().state.wheel_speed[i] < 0.0);
    // measured: 300 ticks (2.5 s), band: > 180 ticks (1.5 s)
    CHECK(backward_ticks > Drive::ticks(1.5)); // the animated wheel turns backward
    // Pulling away backward without jitter.
    CHECK(engine.longest_run() < 3); // measured: 1, band: < 3
    for (const OscillationDetector& wheel : wheels) CHECK(wheel.longest_run() < 3); // measured: at most 2, band: < 3
}

TEST_CASE("Reverse only engages near standstill, never out of forward travel") {
    for (const bool automatic : {false, true}) {
        CAPTURE(automatic);
        const VehicleParams car = automatic ? with_automatic() : sports_coupe();
        TestWorld world(car, 0.02, TestGround{}, 30.0 * kmh);
        Drive drive(world);
        drive.run(0.5);

        // Asking for reverse while rolling forward and braking: the gearbox stays in
        // neutral until the car has stopped, then reverse engages.
        InputPacket reverse;
        reverse.gear_request = -1;
        reverse.clutch = 1.0;
        reverse.brake = 0.3;
        for (int tick = 0; tick < Drive::ticks(6.0); ++tick) {
            const double v_before = forward_speed(world); // m/s
            drive.one_tick(reverse);
            if (v_before > 1.0) REQUIRE(gear(world) == 0);
        }
        CHECK(std::abs(forward_speed(world)) < 0.1); // measured: 3.3e-13 m/s, band: < 0.1 m/s
        CHECK(gear(world) == -1);
    }
}

TEST_CASE("The automatic pulls away in reverse and does not shift by itself") {
    const VehicleParams car = with_automatic();
    TestWorld world(car, 0.02);
    Drive drive(world);
    drive.run(1.5);

    InputPacket reverse;
    reverse.gear_request = -1;
    reverse.throttle = 1.0;
    for (int tick = 0; tick < Drive::ticks(6.0); ++tick) {
        drive.one_tick(reverse);
        REQUIRE(gear(world) == -1);
    }
    const double v = forward_speed(world);
    CAPTURE(v);
    // At full throttle the engine reaches its limiter in reverse; an automatic that
    // shifted would carry the car further.
    CHECK(v < -10.0); // measured: -20.2 m/s, band: < -10 m/s
    // measured: 754.8 rad/s, band: > 680.7 rad/s (upshift_speed)
    CHECK(engine_speed(world) > car.drivetrain.gearbox.automatic.upshift_speed);

    // Selecting drive again pulls away forward in first gear once stopped.
    for (int tick = 0; tick < Drive::ticks(8.0); ++tick) drive.one_tick(drive_input(0.0, 1.0));
    REQUIRE(std::abs(forward_speed(world)) < 0.1); // measured: 1.8e-18 m/s, band: < 0.1 m/s
    drive.run(3.0, drive_input(0.5));
    CHECK(forward_speed(world) > 2.0); // measured: 8.94 m/s, band: > 2.0 m/s
    CHECK(gear(world) >= 1);
}

TEST_CASE("Two reverse drives with the same input packets yield the same checksum, and reverse counts in it") {
    const auto drive_with = [](std::int8_t gear_request, double throttle) {
        TestWorld world(sports_coupe(), 0.02);
        Drive drive(world);
        drive.run(1.0);
        InputPacket packet;
        packet.gear_request = gear_request;
        packet.clutch = 1.0;
        drive.run(0.5, packet);
        packet.clutch = 0.0;
        packet.throttle = throttle;
        drive.run(1.0, packet);
        return world.checksum();
    };

    CHECK(drive_with(-1, 0.3) == drive_with(-1, 0.3));
    CHECK(drive_with(-1, 0.3) != drive_with(-1, 0.31));
    // The gear is part of the state: reverse at idle differs from neutral.
    CHECK(drive_with(-1, 0.0) != drive_with(0, 0.0));
}
