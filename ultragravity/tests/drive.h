#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>

#include "doctest/doctest.h"
#include "ultragravity/test_world.h"

namespace ultragravity::test {

inline constexpr double kmh_100 = 100.0 / 3.6; // m/s

inline constexpr double rpm = 2.0 * std::numbers::pi / 60.0; // rad/s per rpm

inline double engine_speed(const TestWorld& world) { return world.last_step().animation.engine_speed; }

// The sports coupe as its vehicle file describes it, with the automatic the game drives.
inline VehicleParams with_automatic() {
    LoadResult result = load_vehicle_file(UG_VEHICLES_DIR "/sports_coupe.json");
    REQUIRE_FALSE(result.error.has_value());
    return result.params;
}

// The sports coupe with a manual gearbox: the gear request selects the gear itself. Most tests drive it
// that way, so a gear change happens exactly when the test asks for it.
inline VehicleParams sports_coupe() {
    VehicleParams car = with_automatic();
    car.drivetrain.gearbox.mode = GearboxMode::manual;
    return car;
}

inline int gear(const TestWorld& world) { return world.last_step().state.gear; }

inline InputPacket drive_input(double throttle, double brake = 0.0, double steering = 0.0) {
    InputPacket p;
    p.gear_request = 1; // in the automatic: drive
    p.throttle = throttle;
    p.brake = brake;
    p.steering = steering;
    return p;
}

// Delivers an input packet with a running tick number for every tick of the drive.
class Drive {
public:
    explicit Drive(TestWorld& world) : world_(world) {}

    void run(double seconds, InputPacket packet = {}) {
        for (int i = 0; i < ticks(seconds); ++i) one_tick(packet);
    }

    void one_tick(InputPacket packet) {
        packet.tick = tick_++;
        world_.tick(packet);
    }

    static int ticks(double seconds) { return static_cast<int>(std::lround(seconds / TestWorld::time_step)); }

private:
    TestWorld& world_;
    std::uint32_t tick_ = 0;
};

inline double forward_speed(const TestWorld& world) {
    const BodyMotion& b = world.body();
    return dot(b.velocity, rotate(b.orientation, {1.0, 0.0, 0.0}));
}

// Detects oscillation of a quantity: if its change from tick to tick flips sign
// three times in a row, it jitters instead of following a course.
// Changes up to the noise do not count.
class OscillationDetector {
public:
    explicit OscillationDetector(double noise = 0.01) : noise_(noise) {}

    void add(double value) {
        if (previous_) {
            const double change = value - *previous_; // unit of the quantity
            if (std::abs(change) > noise_) {
                const int direction = change > 0.0 ? 1 : -1;
                flips_in_a_row_ = direction == -direction_ ? flips_in_a_row_ + 1 : 0;
                direction_ = direction;
                longest_run_ = std::max(longest_run_, flips_in_a_row_);
            }
        }
        previous_ = value;
    }

    int longest_run() const { return longest_run_; }

private:
    double noise_; // unit of the quantity per tick
    std::optional<double> previous_;
    int direction_ = 0;
    int flips_in_a_row_ = 0;
    int longest_run_ = 0;
};

// Pulls away and shifts up just before the limiter: clutch in, throttle off,
// next gear, after the shift time let the clutch out briskly. In the
// lower gears it only gives as much throttle as keeps the rear wheels from spinning much.
class TestDriver {
public:
    static constexpr double engage = 0.3;         // s clutch pressed before pulling away
    static constexpr double clutch_launch = 0.5;  // s for letting the clutch out when pulling away
    static constexpr double clutch_shift = 0.15;  // s for letting the clutch out after a shift
    static constexpr double shift_speed = 6800.0 * rpm; // rad/s
    static constexpr std::array<double, 3> throttle_low_gears{0.75, 0.95, 1.0}; // 1

    explicit TestDriver(const VehicleParams& k)
        : gears_(static_cast<int>(k.drivetrain.gearbox.ratios.size())),
          shift_pause_(k.drivetrain.gearbox.shift_time + 0.05) {}

    InputPacket packet(const TestWorld& world) {
        InputPacket p;
        p.gear_request = static_cast<std::int8_t>(gear_);
        if (time_ < engage) {
            p.clutch = 1.0;
        } else if (time_ < engage + clutch_launch) {
            p.throttle = throttle();
            p.clutch = 1.0 - (time_ - engage) / clutch_launch;
        } else {
            const double since_shift = time_ - shifted_; // s
            if (gear_ < gears_ && since_shift > shift_pause_ + clutch_shift &&
                engine_speed(world) > shift_speed) {
                ++gear_;
                shifted_ = time_;
                p.gear_request = static_cast<std::int8_t>(gear_);
                p.clutch = 1.0;
            } else if (since_shift < shift_pause_) {
                p.clutch = 1.0;
            } else {
                p.throttle = throttle();
                p.clutch = std::max(1.0 - (since_shift - shift_pause_) / clutch_shift, 0.0);
            }
        }
        time_ += TestWorld::time_step;
        return p;
    }

    int gear() const { return gear_; }
    bool clutch_engaged() const { return time_ - shifted_ > shift_pause_ + clutch_shift; }

private:
    double throttle() const { return gear_ <= 3 ? throttle_low_gears[gear_ - 1] : 1.0; }

    int gears_;
    double shift_pause_; // s
    int gear_ = 1;
    double time_ = 0.0;     // s since the start of pulling away
    double shifted_ = -1e9; // s, time of the last gear change
};

} // namespace ultragravity::test
