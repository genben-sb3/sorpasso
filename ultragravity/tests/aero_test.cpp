#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>

#include "drive.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

// Independent oracle value, deliberately not taken from the physics package.
constexpr double air_density = 1.2; // kg/m³

constexpr double gravity = TestWorld::gravity; // m/s²

double downforce(double area, double v) { return 0.5 * air_density * area * v * v; } // N

double total_downforce(const VehicleParams& k, double v) { return downforce(k.front.downforce + k.rear.downforce, v); } // N

// Driving resistance in straight-line running at speed v: aerodynamic drag
// plus rolling resistance on the wheel load including downforce.
double driving_resistance(const VehicleParams& k, double v) {
    const double air = 0.5 * air_density * k.aero.drag_area * v * v;
    const double rolling = k.rear.tire.rolling_resistance * (k.mass * gravity + total_downforce(k, v));
    return air + rolling;
}

// Drive force at the ground at full throttle in top gear, without losses.
double drive_force(const VehicleParams& k, double v) {
    const Engine& e = k.drivetrain.engine;
    const double overall = k.drivetrain.gearbox.ratios.back() * k.drivetrain.gearbox.final_drive;
    const double speed = v / k.rear.wheel_radius * overall;
    if (speed >= e.rev_limit) return 0.0;
    double torque = e.torque_curve.back().torque;
    for (std::size_t i = 1; i < e.torque_curve.size(); ++i) {
        if (speed <= e.torque_curve[i].speed) {
            const TorquePoint& a = e.torque_curve[i - 1];
            const TorquePoint& b = e.torque_curve[i];
            torque = a.torque + (b.torque - a.torque) * (speed - a.speed) / (b.speed - a.speed);
            break;
        }
    }
    return torque * overall / k.rear.wheel_radius;
}

} // namespace

TEST_CASE("The top speed lies where drive force and driving resistance are in balance") {
    const VehicleParams car = sports_coupe();

    // Balance of forces in sixth gear by bisection.
    double low = 20.0;   // m/s
    double high = 100.0; // m/s
    for (int i = 0; i < 60; ++i) {
        const double mid = (low + high) / 2.0;
        (drive_force(car, mid) > driving_resistance(car, mid) ? low : high) = mid;
    }
    const double expected = low;
    REQUIRE(expected * 3.6 > 250.0); // a sports coupe with 300 hp
    REQUIRE(expected * 3.6 < 300.0);

    TestWorld world(car, 0.02);
    Drive drive(world);
    drive.run(1.5);
    TestDriver driver(car);
    double time = 0.0;
    while (driver.gear() < 6 || !driver.clutch_engaged()) {
        REQUIRE(time < 60.0);
        drive.one_tick(driver.packet(world));
        time += TestWorld::time_step;
    }
    InputPacket full_throttle = driver.packet(world);
    REQUIRE(full_throttle.throttle == 1.0);
    // The car only approaches the top speed asymptotically.
    drive.run(120.0, full_throttle);

    const double v = forward_speed(world);
    CAPTURE(v * 3.6);
    CAPTURE(expected * 3.6);
    // The tires lose some slip to the drive force along the way; 2 % covers that.
    CHECK(v == doctest::Approx(expected).epsilon(0.02));
    // Without more power it gets no faster: the acceleration is gone.
    const double v_before = v;
    drive.run(5.0, full_throttle);
    CHECK(forward_speed(world) - v_before < 0.3);
}

TEST_CASE("Without throttle and brake the car coasts down in neutral according to the driving resistance") {
    const VehicleParams car = sports_coupe();
    TestWorld world(car, 0.02, TestGround{}, kmh_100);
    Drive drive(world);
    drive.run(1.0); // let the suspension settle

    // Expectation: dv/dt = −F_resistance(v)/m, simply integrated.
    double v = forward_speed(world);
    double v_before = v;
    double expected = v;
    constexpr double dt = 0.005;
    const auto integrate = [&](double seconds) {
        for (int i = 0; i < static_cast<int>(seconds / dt); ++i) expected -= driving_resistance(car, expected) / car.mass * dt;
    };

    const double initial_deceleration = driving_resistance(car, v) / car.mass;
    drive.run(1.0);
    CHECK(v_before - forward_speed(world) == doctest::Approx(initial_deceleration).epsilon(0.1));

    // Over 40 s it decelerates ever more gently, and the speed follows the model.
    integrate(2.0);
    double last_decrease = 1e9;
    for (int section = 0; section < 4; ++section) {
        v = forward_speed(world);
        drive.run(10.0);
        const double decrease = v - forward_speed(world);
        CAPTURE(section);
        CHECK(decrease > 0.0);
        CHECK(decrease < last_decrease);
        last_decrease = decrease;
        integrate(10.0);
    }
    CAPTURE(forward_speed(world));
    CAPTURE(expected);
    CHECK(forward_speed(world) == doctest::Approx(expected).epsilon(0.03));
    // It neither stops at once nor rolls on unbraked.
    CHECK(forward_speed(world) > 0.5 * kmh_100);
    CHECK(forward_speed(world) < 0.9 * kmh_100);
}

TEST_CASE("At standstill and at low speed there is no aerodynamic drag and the rolling resistance does not push") {
    const VehicleParams car = sports_coupe();
    TestWorld world(car, 0.02);
    Drive drive(world);
    drive.run(3.0);
    CHECK(std::abs(forward_speed(world)) < 1e-3);
}

TEST_CASE("The downforce pushes down the axle it acts on") {
    const VehicleParams without = [] {
        VehicleParams k = sports_coupe();
        k.front.downforce = 0.0;
        k.rear.downforce = 0.0;
        return k;
    }();
    constexpr double v = 200.0 / 3.6; // m/s

    const auto axle_loads = [&](const VehicleParams& k) {
        TestWorld world(k, 0.02, TestGround{}, v);
        Drive drive(world);
        drive.run(3.0);
        const auto& load = world.last_step().telemetry.wheel_load;
        return std::array<double, 2>{load[front_left] + load[front_right], load[rear_left] + load[rear_right]};
    };
    const auto base = axle_loads(without);

    SUBCASE("rear only") {
        VehicleParams k = without;
        k.rear.downforce = 0.8;
        const auto load = axle_loads(k);
        CHECK(load[1] - base[1] == doctest::Approx(downforce(0.8, v)).epsilon(0.1));
        CHECK(load[0] - base[0] == doctest::Approx(0.0).epsilon(0.1).scale(downforce(0.8, v)));
    }

    SUBCASE("front only") {
        VehicleParams k = without;
        k.front.downforce = 0.8;
        const auto load = axle_loads(k);
        CHECK(load[0] - base[0] == doctest::Approx(downforce(0.8, v)).epsilon(0.1));
        CHECK(load[1] - base[1] == doctest::Approx(0.0).epsilon(0.1).scale(downforce(0.8, v)));
    }

    SUBCASE("it grows with the square of the speed") {
        VehicleParams k = without;
        k.rear.downforce = 0.8;
        const double at_v = axle_loads(k)[1] - base[1];
        CHECK(at_v > 0.0);
        TestWorld slow(k, 0.02, TestGround{}, v / 2.0);
        TestWorld slow_without(without, 0.02, TestGround{}, v / 2.0);
        Drive d1(slow);
        Drive d2(slow_without);
        d1.run(3.0);
        d2.run(3.0);
        const auto sum = [](const TestWorld& w) {
            const auto& l = w.last_step().telemetry.wheel_load;
            return l[rear_left] + l[rear_right];
        };
        CHECK(sum(slow) - sum(slow_without) == doctest::Approx(at_v / 4.0).epsilon(0.15));
    }
}
