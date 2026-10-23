#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>

#include "drive.h"
#include "ultragravity/test_world.h"

using namespace ultragravity;

namespace {

VehicleParams sports_coupe() {
    LoadResult result = load_vehicle_file(UG_VEHICLES_DIR "/sports_coupe.json");
    REQUIRE_FALSE(result.error.has_value());
    return result.params;
}

// Drives without control commands, only the tick field of the input packets advances.
void stand(TestWorld& world, std::uint32_t& tick, double seconds) {
    const int ticks = static_cast<int>(std::lround(seconds / TestWorld::time_step));
    for (int i = 0; i < ticks; ++i) {
        InputPacket packet;
        packet.tick = tick++;
        world.tick(packet);
    }
}

} // namespace

TEST_CASE("A car set down compresses its suspension and comes to rest") {
    TestWorld world(sports_coupe(), 0.10);
    std::uint32_t tick = 0;
    const double height_when_set_down = world.body().position.z;

    stand(world, tick, 4.0);

    CHECK(world.body().position.z < height_when_set_down - 0.10); // measured: 0.16 m lower, band: > 0.10 m
    for (double travel : world.last_step().animation.suspension_travel) {
        CHECK(travel > 0.03); // measured: 0.063 to 0.064 m, band: > 0.03 m
        CHECK(travel < 0.10); // measured: 0.063 to 0.064 m, band: < 0.10 m
    }

    // Afterwards it practically does not move for several seconds.
    const Vec3 rest_position = world.body().position;
    double largest_deviation = 0.0;
    double largest_velocity = 0.0;
    double largest_angular_velocity = 0.0;
    for (int second = 0; second < 5; ++second) {
        for (int i = 0; i < 120; ++i) {
            stand(world, tick, TestWorld::time_step);
            const BodyMotion& b = world.body();
            largest_deviation = std::max(largest_deviation, length(b.position - rest_position));
            largest_velocity = std::max(largest_velocity, length(b.velocity));
            largest_angular_velocity =
                std::max(largest_angular_velocity, length(b.angular_velocity));
        }
    }
    CHECK(largest_deviation < 1e-4); // measured: 9.1e-5 m, band: < 1e-4 m
    CHECK(largest_velocity < 1e-4); // measured: 2.4e-5 m/s, band: < 1e-4 m/s
    CHECK(largest_angular_velocity < 1e-4); // measured: 6.8e-10 rad/s, band: < 1e-4 rad/s
}

TEST_CASE("At standstill the axle loads match the static distribution") {
    const VehicleParams car = sports_coupe();
    TestWorld world(car, 0.10);
    std::uint32_t tick = 0;
    stand(world, tick, 4.0);

    // Beckman: Lf = G·b/L and Lr = G·a/L, with a and b as the distance of the
    // center of mass to the front and rear axle and L as the wheelbase.
    const double weight = car.mass * TestWorld::gravity;
    const double wheelbase = car.front.x - car.rear.x;
    const double a = car.front.x - car.center_of_mass.x;
    const double b = car.center_of_mass.x - car.rear.x;

    const auto& wheel_load = world.last_step().telemetry.wheel_load;
    // measured: -0.013 %, band: ±1 %
    CHECK(wheel_load[front_left] + wheel_load[front_right] ==
          doctest::Approx(weight * b / wheelbase).epsilon(0.01));
    // measured: +0.014 %, band: ±1 %
    CHECK(wheel_load[rear_left] + wheel_load[rear_right] ==
          doctest::Approx(weight * a / wheelbase).epsilon(0.01));
    // measured: equal to 6 digits, band: ±1 %
    CHECK(wheel_load[front_left] == doctest::Approx(wheel_load[front_right]).epsilon(0.01));
    // measured: equal to 6 digits, band: ±1 %
    CHECK(wheel_load[rear_left] == doctest::Approx(wheel_load[rear_right]).epsilon(0.01));
}

TEST_CASE("A car pushed sideways at standstill comes to rest without swinging back") {
    // The worry of ADR 0002: with the relaxation lag the tire acts like a spring at standstill,
    // and the shoved car slides back towards where it stood. Watched at the ground below the
    // center of mass, so the body rolling back upright on its springs does not count.
    TestWorld world(sports_coupe(), 0.02);
    std::uint32_t tick = 0;
    stand(world, tick, 3.0);
    const double height = world.body().position.z; // m, center of mass above the ground
    world.push({0.0, 0.5, 0.0}); // m/s to the left

    double lowest_side_speed = 0.0; // m/s at the ground, negative back towards where it stood
    for (int i = 0; i < 2 * 120; ++i) {
        stand(world, tick, TestWorld::time_step);
        const BodyMotion& b = world.body();
        const Vec3 ground = b.velocity + cross(b.angular_velocity, rotate(b.orientation, {0.0, 0.0, -height}));
        lowest_side_speed = std::min(lowest_side_speed, ground.y);
    }
    double largest_rest_speed = 0.0; // m/s
    for (int i = 0; i < 120; ++i) {
        stand(world, tick, TestWorld::time_step);
        largest_rest_speed = std::max(largest_rest_speed, length(world.body().velocity));
    }

    // The little backward speed comes from the body rolling back upright on its dampers,
    // with the tire yielding as a damper; a tire that springs back reaches -0.23 m/s.
    CHECK(lowest_side_speed > -0.02); // measured: -0.0102 m/s, band: > -2 cm/s
    CHECK(largest_rest_speed < 1e-3); // measured: 1.5e-5 m/s, band: < 1 mm/s (wide; the swing-back check above catches the springing tire)
}

TEST_CASE("A car turned at standstill stops turning without its yaw rate flipping from tick to tick") {
    // At standstill the tire is a stiff damper (ADR 0002). Against a yaw rate fixed over
    // the whole tick it would overshoot and flip the yaw rate every tick.
    TestWorld world(sports_coupe(), 0.02);
    std::uint32_t tick = 0;
    stand(world, tick, 3.0);
    world.push({}, {0.0, 0.0, 0.05}); // rad/s

    test::OscillationDetector yaw_rate(1e-6); // rad/s per tick
    for (int i = 0; i < 120; ++i) {
        stand(world, tick, TestWorld::time_step);
        yaw_rate.add(world.body().angular_velocity.z);
    }
    CAPTURE(world.body().angular_velocity.z);

    CHECK(yaw_rate.longest_run() < 3); // measured: 0 (118 with the yaw rate fixed over the tick), band: < 3
    // A wide band: the flipping yaw rate keeps 4.9e-3 rad/s after the second.
    CHECK(std::abs(world.body().angular_velocity.z) < 1e-4); // measured: 2e-12 rad/s, band: < 1e-4 rad/s
}

TEST_CASE("Two runs with the same input packets yield the same checksum") {
    TestWorld first_run(sports_coupe(), 0.10);
    TestWorld second_run(sports_coupe(), 0.10);
    TestWorld other_run(sports_coupe(), 0.12);
    std::uint32_t tick_first = 0, tick_second = 0, tick_other = 0;

    stand(first_run, tick_first, 2.0);
    stand(second_run, tick_second, 2.0);
    stand(other_run, tick_other, 2.0);

    CHECK(first_run.checksum() == second_run.checksum());
    CHECK(first_run.checksum() != other_run.checksum());

    stand(first_run, tick_first, TestWorld::time_step);
    CHECK(first_run.checksum() != second_run.checksum());
}
