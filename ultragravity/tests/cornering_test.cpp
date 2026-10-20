#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "drive.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

// Upper limit for the force of all tires together, as a multiple of μ·g. Slightly
// above 1 is possible: unloaded wheels have a higher friction thanks to load
// sensitivity.
constexpr double friction_circle_limit = 1.05; // 1

// Steers firmly and holds the speed with a little throttle in the given gear.
InputPacket circle(const TestWorld& world, double steering, double speed, int gear) {
    InputPacket packet;
    packet.steering = steering;
    packet.gear_request = static_cast<std::int8_t>(gear);
    packet.throttle = std::clamp(2.0 * (speed - length(world.body().velocity)), 0.0, 1.0);
    return packet;
}

// Radius of the circle through three points in the plane.
double circumradius(Vec3 a, Vec3 b, Vec3 c) {
    a.z = b.z = c.z = 0.0;
    const double ab = length(b - a), bc = length(c - b), ca = length(a - c); // m
    return ab * bc * ca / (2.0 * length(cross(b - a, c - a)));
}

// Lateral acceleration from the forces of the last step: the component across
// the direction of travel in the plane.
double lateral_acceleration(const TestWorld& world, double mass) {
    const Vec3 v = world.body().velocity;
    const Vec3 lateral = cross({0.0, 0.0, 1.0}, Vec3{v.x, v.y, 0.0}) * (1.0 / std::hypot(v.x, v.y));
    return dot(world.last_step().forces.force, lateral) / mass;
}

struct Circle {
    double radius;               // m, from the driven path
    double speed;                // m/s
    double lateral_acceleration; // m/s², from the forces
};

// Measures over one second: radius from three path points, speed and
// lateral acceleration averaged.
Circle measure_circle(TestWorld& world, Drive& drive, double steering, double speed, int gear, double mass) {
    const int ticks = Drive::ticks(1.0);
    Vec3 points[3]; // m
    double sum_speed = 0.0, sum_lateral = 0.0; // m/s and m/s², summed over the ticks
    for (int i = 0; i <= ticks; ++i) {
        if (i == 0) points[0] = world.body().position;
        if (i == ticks / 2) points[1] = world.body().position;
        if (i == ticks) {
            points[2] = world.body().position;
            break;
        }
        drive.one_tick(circle(world, steering, speed, gear));
        sum_speed += length(world.body().velocity);
        sum_lateral += lateral_acceleration(world, mass);
    }
    return {circumradius(points[0], points[1], points[2]), sum_speed / ticks, sum_lateral / ticks};
}

} // namespace

TEST_CASE("At low speed the car drives the radius the steer angle dictates") {
    const VehicleParams car = sports_coupe();
    const double speed = 4.0;     // m/s
    const double steering = 0.2;  // 1, fraction of full lock
    TestWorld world(car, 0.02, TestGround{}, speed);
    Drive drive(world);
    for (int i = 0; i < Drive::ticks(6.0); ++i) drive.one_tick(circle(world, steering, speed, 1));

    const Circle c = measure_circle(world, drive, steering, speed, 1, car.mass);

    // Without slip angle the rear axle rolls around the point the steered
    // front wheel points at: R = L / tan δ. At 0.4 m/s² lateral acceleration the
    // slip angle is only tenths of a degree.
    const double wheelbase = car.front.x - car.rear.x;                // m
    const double steer_angle = steering * car.steering.max_steer_angle; // rad
    const double rear = wheelbase / std::tan(steer_angle);            // m, radius of the rear axle
    const double b = car.center_of_mass.x - car.rear.x;               // m, center of mass ahead of the rear axle
    CAPTURE(c.radius);
    CHECK(c.radius == doctest::Approx(std::hypot(rear, b)).epsilon(0.05)); // measured: +2.7 %, band: ±5 %
}

TEST_CASE("At constant speed and fixed steering a steady-state circle develops") {
    const VehicleParams car = sports_coupe();
    const double speed = 15.0;    // m/s
    const double steering = 0.08; // 1
    TestWorld world(car, 0.02, TestGround{}, speed);
    Drive drive(world);
    for (int i = 0; i < Drive::ticks(6.0); ++i) drive.one_tick(circle(world, steering, speed, 2));

    const Circle first = measure_circle(world, drive, steering, speed, 2, car.mass);
    const Circle second = measure_circle(world, drive, steering, speed, 2, car.mass);
    CAPTURE(first.radius);
    CAPTURE(second.radius);
    CAPTURE(second.lateral_acceleration);

    // Steady state: radius and speed barely change from second to second.
    CHECK(second.radius == doctest::Approx(first.radius).epsilon(0.01)); // measured: equal to 6 digits, band: ±1 %
    CHECK(second.speed == doctest::Approx(first.speed).epsilon(0.01)); // measured: equal to 6 digits, band: ±1 %
    CHECK(second.lateral_acceleration > 3.0); // measured: 3.83 m/s², band: > 3.0 m/s²
    // The tire forces keep the car exactly on the driven circle. On a
    // steady-state circle that follows from kinematics; the comparison checks that
    // path and forces fit together, the statement about the tire model lies in
    // the steady state and in the radius at low speed.
    // measured: below 0.001 %, band: ±2 %
    CHECK(second.lateral_acceleration ==
          doctest::Approx(second.speed * second.speed / second.radius).epsilon(0.02));
}

TEST_CASE("The highest lateral acceleration lies at the grip of the tires") {
    const VehicleParams car = sports_coupe();
    const double steering = 0.1; // 1
    TestWorld world(car, 0.02, TestGround{}, 15.0);
    Drive drive(world);
    for (int i = 0; i < Drive::ticks(4.0); ++i) drive.one_tick(circle(world, steering, 15.0, 3));

    // With fixed steering slowly faster until the tires no longer hold.
    // Averaged over half a second, so short peaks do not count.
    const int window = Drive::ticks(0.5); // ticks
    double highest = 0.0; // m/s²
    double sum = 0.0;     // m/s², summed over the window
    for (int i = 0; i < Drive::ticks(30.0); ++i) {
        const double speed = 15.0 + 0.5 * i * TestWorld::time_step; // m/s
        drive.one_tick(circle(world, steering, speed, 3));
        sum += lateral_acceleration(world, car.mass);
        if ((i + 1) % window == 0) {
            highest = std::max(highest, std::abs(sum) / window);
            sum = 0.0;
        }
    }

    // Without weight transfer the car would hold μ·g. Load sensitivity
    // costs some of it while rolling.
    const double grip = car.front.tire.peak_friction * TestWorld::gravity; // m/s²
    CAPTURE(highest);
    CAPTURE(grip);
    CHECK(highest > 0.85 * grip); // measured: 0.91·grip, band: > 0.85·grip
    CHECK(highest < friction_circle_limit * grip); // measured: 0.91·grip, band: < 1.05·grip
}

namespace {

struct Roll {
    double roll_angle;           // rad, positive to the right (outside of the corner in a left-hand corner)
    double lateral_acceleration; // m/s²
    double transfer_front;       // N, load right minus left at the front axle
    double transfer_rear;        // N
    double height;               // m, center of mass above the ground
};

// Drives a steady left-hand corner at about 6 m/s² and measures at the end.
Roll left_corner(const VehicleParams& car) {
    TestWorld world(car, 0.02, TestGround{}, 15.0);
    Drive drive(world);
    for (int i = 0; i < Drive::ticks(6.0); ++i) drive.one_tick(circle(world, 0.11, 15.0, 2));
    const auto& wheel_load = world.last_step().telemetry.wheel_load;
    const Vec3 left = rotate(world.body().orientation, {0.0, 1.0, 0.0});
    return {std::asin(left.z), lateral_acceleration(world, car.mass),
            wheel_load[front_right] - wheel_load[front_left], wheel_load[rear_right] - wheel_load[rear_left],
            world.body().position.z};
}

} // namespace

TEST_CASE("In a corner the body rolls outward and the outer wheels carry more load") {
    const VehicleParams car = sports_coupe();
    const Roll r = left_corner(car);
    CAPTURE(r.lateral_acceleration);
    CAPTURE(r.roll_angle);
    REQUIRE(r.lateral_acceleration > 5.0); // measured: 5.26 m/s², band: > 5.0 m/s²

    CHECK(r.roll_angle > 0.0);
    CHECK(r.transfer_front > 0.0);
    CHECK(r.transfer_rear > 0.0);
    // Moment balance about the longitudinal axis: the load differences times half
    // the track (N·m) balance the centrifugal force at the height of the center of mass,
    // plus the weight, because the center of mass moves outward by h·sin φ when rolling.
    const double expected = car.mass * r.height *
                            (r.lateral_acceleration + TestWorld::gravity * std::sin(r.roll_angle));
    const double measured =
        r.transfer_front * car.front.track_width / 2.0 + r.transfer_rear * car.rear.track_width / 2.0;
    CHECK(measured == doctest::Approx(expected).epsilon(0.02)); // measured: -0.02 %, band: ±2 %
}

TEST_CASE("Anti-roll bars reduce the roll and shift the load transfer to their axle") {
    const VehicleParams car = sports_coupe();
    VehicleParams without = car;
    without.front.suspension.anti_roll_bar = 0.0;
    without.rear.suspension.anti_roll_bar = 0.0;
    VehicleParams stiff_front = car;
    stiff_front.front.suspension.anti_roll_bar *= 3.0;

    const Roll with_bars = left_corner(car);
    const Roll without_bars = left_corner(without);
    const Roll stiff = left_corner(stiff_front);
    CAPTURE(with_bars.roll_angle);
    CAPTURE(without_bars.roll_angle);

    REQUIRE(car.front.suspension.anti_roll_bar > 0.0);
    REQUIRE(car.rear.suspension.anti_roll_bar > 0.0);
    CHECK(with_bars.roll_angle < 0.8 * without_bars.roll_angle); // measured: 0.57·without, band: < 0.8·without
    const auto front_share = [](const Roll& r) {
        return r.transfer_front / (r.transfer_front + r.transfer_rear);
    };
    CHECK(front_share(stiff) > front_share(with_bars) + 0.05); // measured: +0.15, band: > +0.05
}

TEST_CASE("Braking in a corner costs cornering force, the tires stay within the friction circle") {
    const VehicleParams car = sports_coupe();
    const double steering = 0.11; // 1
    TestWorld world(car, 0.02, TestGround{}, 15.0);
    Drive drive(world);
    for (int i = 0; i < Drive::ticks(6.0); ++i) drive.one_tick(circle(world, steering, 15.0, 2));
    const double before = lateral_acceleration(world, car.mass); // m/s²
    REQUIRE(before > 5.0); // measured: 5.26 m/s², band: > 5.0 m/s²

    // Full braking with the same steering. The wheels lock and push almost
    // only longitudinally.
    InputPacket braking;
    braking.steering = steering;
    braking.brake = 1.0;
    double highest_total = 0.0; // m/s²
    double sum_lateral = 0.0;   // m/s², summed over the ticks
    const int ticks = Drive::ticks(0.25);
    for (int i = 0; i < ticks; ++i) {
        drive.one_tick(braking);
        const Vec3 force = world.last_step().forces.force; // N
        highest_total = std::max(highest_total, std::hypot(force.x, force.y) / car.mass);
        sum_lateral += lateral_acceleration(world, car.mass);
    }

    const double grip = car.front.tire.peak_friction * TestWorld::gravity; // m/s²
    CAPTURE(before);
    CAPTURE(sum_lateral / ticks);
    CAPTURE(highest_total);
    // With weaker coupling (B = 4 instead of 10) 34 % would remain.
    CHECK(sum_lateral / ticks < 0.25 * before); // measured: 0.12·before, band: < 0.25·before
    // Without combined slip it would be about 1.13 μ·g.
    CHECK(highest_total < friction_circle_limit * grip); // measured: 0.97·grip, band: < 1.05·grip
}

TEST_CASE("Slow maneuvering at full lock stays calm until the car stands") {
    const VehicleParams car = sports_coupe();
    TestWorld world(car, 0.02, TestGround{}, 2.0);
    Drive drive(world);
    InputPacket full_lock;
    full_lock.steering = 1.0;
    drive.run(0.5, full_lock); // let the suspension settle

    // Coast in neutral; the scrubbing of the tires at full lock brakes
    // the car to a standstill, then it stands for a few more seconds.
    OscillationDetector yaw_rate(0.001); // rad/s per tick
    OscillationDetector lateral(0.001);  // m/s per tick
    for (int i = 0; i < Drive::ticks(15.0); ++i) {
        drive.one_tick(full_lock);
        const BodyMotion& b = world.body();
        yaw_rate.add(b.angular_velocity.z);
        lateral.add(rotate_inverse(b.orientation, b.velocity).y);
    }

    CAPTURE(length(world.body().velocity));
    CHECK(yaw_rate.longest_run() < 3); // measured: 1, band: < 3
    CHECK(lateral.longest_run() < 3); // measured: 1, band: < 3
    CHECK(length(world.body().velocity) < 1e-3); // measured: 5.2e-12 m/s, band: < 1e-3 m/s
    CHECK(std::abs(world.body().angular_velocity.z) < 1e-3); // measured: 1.2e-12 rad/s, band: < 1e-3 rad/s
}

TEST_CASE("At high speed the car does not oscillate after a steering impulse") {
    const VehicleParams car = sports_coupe();
    const double speed = 45.0; // m/s, about 160 km/h
    TestWorld world(car, 0.02, TestGround{}, speed);
    Drive drive(world);
    for (int i = 0; i < Drive::ticks(3.0); ++i) drive.one_tick(circle(world, 0.0, speed, 5));

    // Short steering impulse to the left, then straight again.
    for (int i = 0; i < Drive::ticks(0.2); ++i) drive.one_tick(circle(world, 0.04, speed, 5));

    // The yaw rate decays without swinging through to the other side, and after three
    // seconds practically nothing is left.
    double largest = 0.0;    // rad/s, to the left
    double other_side = 0.0; // rad/s, largest swing to the right
    double yaw_rate = 0.0;   // rad/s
    for (int i = 0; i < Drive::ticks(3.0); ++i) {
        drive.one_tick(circle(world, 0.0, speed, 5));
        yaw_rate = world.body().angular_velocity.z;
        largest = std::max(largest, yaw_rate);
        other_side = std::max(other_side, -yaw_rate);
    }

    CAPTURE(largest);
    CAPTURE(other_side);
    REQUIRE(largest > 0.05); // measured: 0.23 rad/s, band: > 0.05 rad/s
    CHECK(other_side < 0.1 * largest); // measured: 0.0057·largest, band: < 0.1·largest
    CHECK(std::abs(yaw_rate) < 0.02 * largest); // measured: 2.9e-8·largest, band: < 0.02·largest
}

TEST_CASE("Two corner drives with the same input packets yield the same checksum") {
    const auto drive_corner = [](double steering) {
        TestWorld world(sports_coupe(), 0.02, TestGround{}, 15.0);
        Drive drive(world);
        for (int i = 0; i < Drive::ticks(3.0); ++i) drive.one_tick(circle(world, steering, 15.0, 2));
        return world.checksum();
    };

    CHECK(drive_corner(0.1) == drive_corner(0.1));
    CHECK(drive_corner(0.1) != drive_corner(0.11));
}
