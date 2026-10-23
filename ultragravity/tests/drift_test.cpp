#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "drive.h"
#include "ultragravity/test_driver.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

constexpr double deg = std::numbers::pi / 180.0; // rad per degree
constexpr int drive_gear = 4;                    // 1, gear of the drift drives unless stated otherwise
constexpr double corner_speed = 26.0;            // m/s, speed of the cornering before initiating
constexpr double corner_steering = 0.25;         // 1, steering of the cornering before initiating
constexpr double handover = 8.0 * deg;           // rad, from this drift angle the drift controller takes over

double drift_angle(const TestWorld& world) { return DriftController::drift_angle(world.body()); }
double speed(const TestWorld& world) { return length(world.body().velocity); }

// The input of the cornering: fixed steering, the throttle holds the speed.
InputPacket cornering(const TestWorld& world, int gear = drive_gear) {
    InputPacket p;
    p.steering = corner_steering;
    p.gear_request = static_cast<std::int8_t>(gear);
    p.throttle = std::clamp(0.5 + 2.0 * (corner_speed - speed(world)), 0.0, 1.0);
    return p;
}

enum class Initiation { power_over, lift_off, handbrake, clutch_kick };

// Smallest and largest drift angle over a stretch of the drive.
struct Range {
    double low = 1e9;   // rad
    double high = -1e9; // rad
    void add(double angle) {
        low = std::min(low, angle);
        high = std::max(high, angle);
    }
};

// A drive from cornering into a drift that the drift controller holds.
struct DriftDrive {
    VehicleParams car = sports_coupe();
    TestWorld world{car, 0.02, TestGround{}, corner_speed};
    Drive drive{world};
    DriftController controller{car};
    double time = 0.0; // s since initiating
    int gear;          // 1, engaged gear

    explicit DriftDrive(int start_gear = drive_gear) : gear(start_gear) {
        InputPacket engage;
        engage.gear_request = static_cast<std::int8_t>(gear);
        drive.run(0.5, engage);
        for (int i = 0; i < Drive::ticks(3.0); ++i) drive.one_tick(cornering(world, gear));
    }

    void tick(InputPacket p) {
        drive.one_tick(p);
        time += TestWorld::time_step;
    }

    // The drift controller at the wheel, gear fixed.
    InputPacket controller_packet(double target) {
        const DriftController::Output out = controller.regulate(world.body(), target);
        InputPacket p;
        p.gear_request = static_cast<std::int8_t>(gear);
        p.steering = out.steering;
        p.throttle = out.throttle;
        return p;
    }

    // Initiates until the rear is out so far that the driver takes over. Returns whether that worked.
    bool initiate(Initiation how) {
        if (how == Initiation::clutch_kick) {
            // Rev the engine up with the clutch pressed.
            for (int i = 0; i < Drive::ticks(0.6); ++i) {
                InputPacket p = cornering(world, gear);
                p.throttle = 1.0;
                p.clutch = 1.0;
                tick(p);
            }
        }
        time = 0.0;
        while (time < 1.5) {
            if (drift_angle(world) > handover) return true;
            InputPacket p = cornering(world, gear);
            switch (how) {
            case Initiation::power_over:
                p.throttle = time < 1.2 ? 1.0 : p.throttle;
                break;
            case Initiation::lift_off:
                // Throttle off and counter-steer, then turn in hard.
                if (time < 0.6) {
                    p.throttle = 0.0;
                    p.steering = time < 0.3 ? -0.4 : 0.5;
                }
                break;
            case Initiation::handbrake:
                p.handbrake = time < 0.35 ? 1.0 : 0.0;
                break;
            case Initiation::clutch_kick:
                // The clutch snaps shut at full engine speed.
                p.throttle = time < 0.35 ? 1.0 : p.throttle;
                break;
            }
            tick(p);
        }
        return drift_angle(world) > handover;
    }

    // Shifts in the drift like the test driver: clutch in, throttle off, change gear, after the
    // shift time let the clutch out briskly. The drift controller keeps steering meanwhile.
    Range shift(int new_gear, double target) {
        constexpr double release = 0.15;                                 // s for letting the clutch out
        const double pause = car.drivetrain.gearbox.shift_time + 0.05; // s
        gear = new_gear;
        Range r;
        for (int i = 0; i < Drive::ticks(pause + release); ++i) {
            const double t = i * TestWorld::time_step; // s since shifting
            InputPacket p = controller_packet(target);
            if (t < pause) {
                p.throttle = 0.0;
                p.clutch = 1.0;
            } else {
                p.clutch = 1.0 - (t - pause) / release;
            }
            tick(p);
            r.add(drift_angle(world));
        }
        return r;
    }

    // Holds the drift angle and returns smallest and largest value over the duration.
    Range hold(double target, double duration) {
        Range r;
        for (int i = 0; i < Drive::ticks(duration); ++i) {
            tick(controller_packet(target));
            r.add(drift_angle(world));
        }
        return r;
    }
};

// The drive in the held drift: handbrake to initiate, then the drift controller holds 25 degrees.
constexpr double drift_target = 25.0 * deg; // rad

void into_the_drift(DriftDrive& d) {
    REQUIRE(d.initiate(Initiation::handbrake));
    d.hold(drift_target, 3.5);
}

// Drives `duration` s with the frozen packet of the held drift, changed by `change`, and returns
// the drift angle at the end in rad. `change` gets the packet and the time since the start.
template <class Change>
double drive_frozen(DriftDrive& d, double duration, Change change, Range* range = nullptr) {
    const InputPacket frozen = d.controller_packet(drift_target);
    for (int i = 0; i < Drive::ticks(duration); ++i) {
        InputPacket p = frozen;
        change(p, i * TestWorld::time_step);
        d.tick(p);
        if (range) range->add(drift_angle(d.world));
    }
    return drift_angle(d.world);
}

// Counts reversals of the direction in which the drift angle moves, above the noise.
class ReversalCounter {
public:
    explicit ReversalCounter(double start) : last_(start) {}

    void add(double angle) {
        if (std::abs(angle - last_) <= noise) return;
        const double direction = angle > last_ ? 1.0 : -1.0; // 1
        if (direction_ != 0.0 && direction != direction_) ++reversals_;
        direction_ = direction;
        last_ = angle;
    }

    int reversals() const { return reversals_; }

private:
    static constexpr double noise = 0.3 * deg; // rad
    double last_;             // rad
    double direction_ = 0.0;  // 1
    int reversals_ = 0;       // 1
};

// A fast corner without a drift (35 m/s, fixed steering near the grip limit); from 5 s on `lift`
// releases the throttle. Returns the largest drift angle (rad) and largest yaw rate (rad/s) after that.
struct CornerPeaks {
    double drift_angle; // rad
    double yaw_rate;    // rad/s
};

CornerPeaks fast_corner(bool lift) {
    constexpr double v0 = 35.0;      // m/s
    constexpr double steering = 0.11; // 1
    const VehicleParams car = sports_coupe();
    TestWorld world{car, 0.02, TestGround{}, v0};
    Drive drive{world};
    InputPacket engage;
    engage.gear_request = 5;
    drive.run(0.5, engage);
    CornerPeaks peaks{0.0, 0.0};
    for (int i = 0; i < Drive::ticks(8.0); ++i) {
        const double t = i * TestWorld::time_step; // s
        InputPacket p;
        p.gear_request = 5;
        p.steering = steering;
        p.throttle = (lift && t >= 5.0) ? 0.0 : std::clamp(0.5 + 2.0 * (v0 - speed(world)), 0.0, 1.0);
        drive.one_tick(p);
        if (t >= 5.0) {
            peaks.drift_angle = std::max(peaks.drift_angle, std::abs(drift_angle(world)));
            peaks.yaw_rate = std::max(peaks.yaw_rate, std::abs(world.body().angular_velocity.z));
        }
    }
    return peaks;
}

} // namespace

// Intent broken since the import (#36): with peak friction 1.25 (7a7efaa) instead of 1.05 a power-over
// in fourth gear at 26 m/s only reaches 5.3 degrees of drift angle, below the handover at 8 degrees (in
// third gear 107 degrees). Kept unchanged and marked as failing instead of loosened; follow-up in #28.
TEST_CASE("A power-over takes the car from cornering into a drift" * doctest::should_fail()) {
    DriftDrive d;
    REQUIRE(d.initiate(Initiation::power_over));
    d.hold(drift_target, 3.0);
    CHECK(drift_angle(d.world) > 20.0 * deg); // measured: not reached, the initiation fails (see above)
    CHECK(speed(d.world) > 20.0); // m/s; measured: not reached, the initiation fails (see above)
}

TEST_CASE("Lift-off, handbrake and clutch kick each take the car from cornering into a drift") {
    for (Initiation how : {Initiation::lift_off, Initiation::handbrake, Initiation::clutch_kick}) {
        CAPTURE(static_cast<int>(how));
        DriftDrive d;
        REQUIRE(d.initiate(how));
        const Range r = d.hold(drift_target, 3.0);
        CAPTURE(r.low / deg);
        CAPTURE(r.high / deg);
        CAPTURE(drift_angle(d.world) / deg);
        // The drift holds afterwards: the car is still sideways and still moving.
        CHECK(drift_angle(d.world) > 20.0 * deg); // measured: 24.2 to 24.4 degrees, band: ×0.82
        CHECK(speed(d.world) > 20.0);             // m/s; measured: 26.7 to 28.8 m/s, band: ×0.75
    }
}

TEST_CASE("The drift controller holds the drift angle within the tolerance over several seconds") {
    for (double target_deg : {25.0, 30.0, 35.0}) {
        CAPTURE(target_deg);
        DriftDrive d;
        REQUIRE(d.initiate(Initiation::handbrake));
        d.hold(target_deg * deg, 2.0); // settle
        const Range r = d.hold(target_deg * deg, 5.0);
        CAPTURE(r.low / deg);
        CAPTURE(r.high / deg);
        // measured: 22.8 to 24.6 degrees at 25 degrees target (28.5 to 29.8 at 30, 33.6 to 34.8 at 35),
        // band: target -2.5 to +2.5 degrees
        CHECK(r.low > (target_deg - 2.5) * deg);
        CHECK(r.high < (target_deg + 2.5) * deg); // measured: see above, band: as above
    }
}

TEST_CASE("More throttle makes the drift angle grow") {
    DriftDrive less, more;
    into_the_drift(less);
    into_the_drift(more);
    const double small = drive_frozen(less, 0.9, [](InputPacket& p, double) { p.throttle = 0.5; });
    const double large = drive_frozen(more, 0.9, [](InputPacket& p, double) { p.throttle = 1.0; });
    CAPTURE(small / deg);
    CAPTURE(large / deg);
    CHECK(large > small + 2.0 * deg); // measured: 3.1 degrees difference (56.9 against 59.9), band: ×0.65
}

TEST_CASE("More counter-steering reduces the drift angle until the car is caught") {
    DriftDrive held, little, much;
    into_the_drift(held);
    into_the_drift(little);
    into_the_drift(much);
    const double unchanged = drive_frozen(held, 0.3, [](InputPacket&, double) {});
    const double some = drive_frozen(little, 0.3, [](InputPacket& p, double) { p.steering -= 0.05; });
    Range range;
    drive_frozen(much, 0.9, [](InputPacket& p, double) { p.steering -= 0.15; }, &range);
    CAPTURE(unchanged / deg);
    CAPTURE(some / deg);
    CAPTURE(range.low / deg);
    CHECK(some < unchanged - 1.5 * deg); // measured: 3.6 degrees less, band: ×0.42
    // Caught: the body points in the direction of travel again.
    CHECK(range.low < 3.0 * deg); // measured: -57.0 degrees (past zero), band: change of sign
}

TEST_CASE("Less counter-steering leads to a spin") {
    DriftDrive d;
    into_the_drift(d);
    const double angle = drive_frozen(d, 1.2, [](InputPacket& p, double) { p.steering += 0.1; });
    CAPTURE(angle / deg);
    CHECK(angle > 90.0 * deg); // the rear has overtaken the front; measured: 158 degrees, band: ×0.57
}

TEST_CASE("Full throttle at a large drift angle leads to a spin") {
    DriftDrive d;
    REQUIRE(d.initiate(Initiation::handbrake));
    d.hold(45.0 * deg, 1.5);
    double angle = 0.0; // rad
    for (int i = 0; i < Drive::ticks(3.0); ++i) {
        InputPacket p = d.controller_packet(45.0 * deg);
        p.throttle = 1.0;
        d.tick(p);
        angle = std::max(angle, drift_angle(d.world));
    }
    CAPTURE(angle / deg);
    CHECK(angle > 90.0 * deg); // measured: 180 degrees, band: ×0.5
}

TEST_CASE("Lifting off in the drift: the rear grips and the car swings back") {
    DriftDrive d;
    into_the_drift(d);
    Range range;
    drive_frozen(d, 1.6, [](InputPacket& p, double) { p.throttle = 0.0; }, &range);
    CAPTURE(range.low / deg);
    CHECK(range.low < -30.0 * deg); // the rear has swung to the other side; measured: -66.1 degrees, band: ×0.45
}

TEST_CASE("Lifting off from a large drift angle throws the car to the other side") {
    DriftDrive d;
    REQUIRE(d.initiate(Initiation::handbrake));
    d.hold(45.0 * deg, 2.0);
    Range range;
    for (int i = 0; i < Drive::ticks(1.5); ++i) {
        InputPacket p = d.controller_packet(45.0 * deg);
        p.throttle = 0.0;
        d.tick(p);
        range.add(drift_angle(d.world));
    }
    CAPTURE(range.low / deg);
    CHECK(range.low < -30.0 * deg); // measured: -179.2 degrees (spin to the other side), band: ×0.17
}

TEST_CASE("Lifting off in a fast corner without a drift lets the rear come around") {
    const CornerPeaks with_throttle = fast_corner(false);
    const CornerPeaks lifted = fast_corner(true);
    CAPTURE(with_throttle.drift_angle / deg);
    CAPTURE(lifted.drift_angle / deg);
    CAPTURE(with_throttle.yaw_rate);
    CAPTURE(lifted.yaw_rate);
    CHECK(with_throttle.drift_angle < 8.0 * deg); // without lifting no drift; measured: 3.3 degrees, band: < 8
    CHECK(lifted.drift_angle > with_throttle.drift_angle + 0.1 * deg); // measured: +0.43 degrees, band: ×0.23
    CHECK(lifted.yaw_rate > 1.08 * with_throttle.yaw_rate); // the rear turns in; measured: ×1.17, band: ×1.08
}

TEST_CASE("After lifting off in the drift the swinging back dies out without intervention, without jitter") {
    DriftDrive d;
    into_the_drift(d);
    OscillationDetector jitter(0.0003);
    Range end;
    ReversalCounter counter(drift_angle(d.world));
    for (int i = 0; i < Drive::ticks(8.0); ++i) {
        InputPacket p = d.controller_packet(0.0); // the driver only steers straight ahead, no throttle, no further input
        p.throttle = 0.0;
        d.tick(p);
        const double w = drift_angle(d.world); // rad
        jitter.add(w);
        counter.add(w);
        if (i * TestWorld::time_step >= 4.0) end.add(w);
    }
    CAPTURE(counter.reversals());
    CAPTURE(end.low / deg);
    CAPTURE(end.high / deg);
    CHECK(counter.reversals() <= 4); // measured: 3, band: ≤ 4
    CHECK(end.high - end.low < 1.0 * deg); // died out; measured: 0.003 degrees, band: < 1 degree
    CHECK(jitter.longest_run() < 3);
}

TEST_CASE("The brake in the drift changes the drift angle") {
    DriftDrive without_brake, with_brake;
    into_the_drift(without_brake);
    into_the_drift(with_brake);
    const double without_angle = drive_frozen(without_brake, 0.5, [](InputPacket&, double) {});
    const double with_angle = drive_frozen(with_brake, 0.5, [](InputPacket& p, double) { p.brake = 0.5; });
    CAPTURE(without_angle / deg);
    CAPTURE(with_angle / deg);
    CHECK(with_angle > without_angle + 8.0 * deg); // measured: 14.8 degrees difference, band: ×0.54
}

TEST_CASE("The swinging after turning in dies out in a few oscillations, without jitter") {
    constexpr double v0 = 30.0; // m/s
    const VehicleParams car = sports_coupe();
    TestWorld world{car, 0.02, TestGround{}, v0};
    Drive drive{world};
    InputPacket engage;
    engage.gear_request = 5;
    drive.run(0.5, engage);

    OscillationDetector jitter(0.0003);
    Range start, end;
    ReversalCounter counter(drift_angle(world));
    for (int i = 0; i < Drive::ticks(9.0); ++i) {
        InputPacket p;
        p.gear_request = 5;
        p.steering = 0.17; // turning in excites the swinging, afterwards nobody intervenes
        p.throttle = std::clamp(0.5 + 2.0 * (v0 - speed(world)), 0.0, 1.0);
        drive.one_tick(p);
        const double w = drift_angle(world); // rad
        jitter.add(w);
        counter.add(w);
        const double t = i * TestWorld::time_step; // s
        if (t < 3.0) start.add(w);
        if (t >= 6.0) end.add(w);
    }
    CAPTURE(counter.reversals());
    CAPTURE((start.high - start.low) / deg);
    CAPTURE((end.high - end.low) / deg);
    CHECK(counter.reversals() <= 6); // measured: 3, band: ≤ 6
    CHECK(end.high - end.low < 0.2 * (start.high - start.low)); // measured: 0.0015 of the start, band: < 0.2
    CHECK(jitter.longest_run() < 3);
}

// Intent broken since the import (#36): with peak friction 1.25 the drift angle drops by 3.2 instead of
// 1.5 degrees on the upshift and is still at 21.1 degrees when the hold starts, below the band of the
// package (target -2.5 degrees). Kept unchanged and marked as failing instead of loosened; follow-up in #28.
TEST_CASE("Shifting up in the drift makes the drift angle drop briefly, afterwards the drift holds on" *
          doctest::should_fail()) {
    DriftDrive d;
    into_the_drift(d);
    const double before = drift_angle(d.world); // rad
    const Range during = d.shift(drive_gear + 1, drift_target);
    const Range after = d.hold(drift_target, 3.0);
    CAPTURE(before / deg);
    CAPTURE(during.low / deg);
    CAPTURE(after.low / deg);
    CAPTURE(after.high / deg);
    CHECK(during.low < before - 0.8 * deg); // dropped briefly; measured: 3.2 degrees, band: ×0.25
    // measured: 21.1 to 24.6 degrees (fails, see above), band: target -2.5 to +2.5 degrees
    CHECK(after.low > drift_target - 2.5 * deg); // held afterwards
    CHECK(after.high < drift_target + 2.5 * deg); // measured: see above, band: as above
}

// Intent broken since the import (#36), upshift half only: with peak friction 1.25 an upshift lowers the
// drift angle by only 0.25 degrees against the same gear (package: 1.1), below the band of 0.4 degrees.
// Kept unchanged and marked as failing instead of loosened; follow-up in #28.
TEST_CASE("Shifting down in the drift makes the drift angle grow, shifting up makes it shrink" * doctest::should_fail()) {
    // Frozen input, only the gear changes: down, same gear, up.
    const auto after_shift = [](int new_gear) {
        DriftDrive d(5);
        into_the_drift(d);
        const double pause = d.car.drivetrain.gearbox.shift_time + 0.05; // s
        return drive_frozen(d, 0.7, [&](InputPacket& p, double t) {
            p.gear_request = static_cast<std::int8_t>(new_gear);
            if (t < pause) {
                p.throttle = 0.0;
                p.clutch = 1.0;
            } else if (t < pause + 0.15) {
                p.clutch = 1.0 - (t - pause) / 0.15;
            }
        });
    };
    const double down = after_shift(4);
    const double same = after_shift(5);
    const double up = after_shift(6);
    CAPTURE(down / deg);
    CAPTURE(same / deg);
    CAPTURE(up / deg);
    CHECK(down > same + 0.4 * deg); // measured: +0.65 degrees, band: ×0.62
    CHECK(up < same - 0.4 * deg);   // measured: -0.25 degrees (fails, see above), band: 0.4 degrees
}
