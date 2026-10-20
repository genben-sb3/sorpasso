#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>

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

    CHECK(world.body().position.z < height_when_set_down - 0.10);
    for (double travel : world.last_step().animation.suspension_travel) {
        CHECK(travel > 0.03);
        CHECK(travel < 0.10);
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
    CHECK(largest_deviation < 1e-4);
    CHECK(largest_velocity < 1e-4);
    CHECK(largest_angular_velocity < 1e-4);
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
    CHECK(wheel_load[front_left] + wheel_load[front_right] ==
          doctest::Approx(weight * b / wheelbase).epsilon(0.01));
    CHECK(wheel_load[rear_left] + wheel_load[rear_right] ==
          doctest::Approx(weight * a / wheelbase).epsilon(0.01));
    CHECK(wheel_load[front_left] == doctest::Approx(wheel_load[front_right]).epsilon(0.01));
    CHECK(wheel_load[rear_left] == doctest::Approx(wheel_load[rear_right]).epsilon(0.01));
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
