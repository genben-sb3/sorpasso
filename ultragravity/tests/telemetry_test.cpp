#include "doctest/doctest.h"

#include <sstream>
#include <string>
#include <vector>

#include "drive.h"
#include "ultragravity/telemetry_csv.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> out;
    std::stringstream ss(text);
    for (std::string f; std::getline(ss, f, separator);) out.push_back(f);
    return out;
}

struct Run {
    std::string csv;
    std::vector<StepResult> steps;
    std::vector<InputPacket> inputs;
    std::uint64_t checksum = 0;
};

// Pulls away, steers back and forth and then brakes; optionally records the telemetry.
Run drive_maneuver(bool with_telemetry, int ticks) {
    TestWorld world(sports_coupe(), 0.02, TestGround{}, 20.0);
    std::ostringstream out;
    if (with_telemetry) write_telemetry_header(out);
    Run run;
    for (int i = 0; i < ticks; ++i) {
        const bool throttle_phase = i < ticks / 2;
        InputPacket p = drive_input(throttle_phase ? 0.8 : 0.0, throttle_phase ? 0.0 : 0.5, i % 40 < 20 ? 0.3 : -0.3);
        p.tick = static_cast<std::uint32_t>(i);
        world.tick(p);
        if (with_telemetry) write_telemetry_row(out, p, world.last_step());
        run.steps.push_back(world.last_step());
        run.inputs.push_back(p);
    }
    run.csv = out.str();
    run.checksum = world.checksum();
    return run;
}

} // namespace

TEST_CASE("The CSV has a header row with units and exactly one row per tick") {
    const Run run = drive_maneuver(true, 240);
    const auto rows = split(run.csv, '\n');
    REQUIRE(rows.size() == 241);

    const auto header = split(rows[0], ',');
    CHECK(header.size() == 9 + 5 * wheel_count);
    for (const std::string& name : header) {
        CAPTURE(name);
        CHECK(name.find('[') != std::string::npos);
        CHECK(name.back() == ']');
    }
    for (std::size_t i = 1; i < rows.size(); ++i) CHECK(split(rows[i], ',').size() == header.size());
}

TEST_CASE("The values of a row match the state of the tick") {
    const Run run = drive_maneuver(true, 240);
    const auto rows = split(run.csv, '\n');
    const auto header = split(rows[0], ',');
    const auto column = [&](const std::string& name) {
        for (std::size_t i = 0; i < header.size(); ++i) {
            if (header[i] == name) return i;
        }
        FAIL("column missing: " << name);
        return std::size_t{0};
    };

    const std::size_t v = column("speed[m/s]"), n = column("engine_speed[rad/s]");
    const std::size_t g = column("gear[1]"), t = column("tick[1]"), throttle = column("throttle[1]");
    const std::size_t steer = column("steering[1]"), brake = column("brake[1]");
    const std::size_t load = column("wheel_load_rr[N]"), slip = column("slip_fl[1]");
    const std::size_t fx = column("force_longitudinal_fr[N]"), fy = column("force_lateral_rl[N]");
    const std::size_t alpha = column("slip_angle_fr[rad]");

    bool forces_seen = false;
    for (std::size_t i = 0; i < run.steps.size(); ++i) {
        const auto f = split(rows[i + 1], ',');
        const StepResult& e = run.steps[i];
        const InputPacket& p = run.inputs[i];
        CAPTURE(i);
        CHECK(std::stoull(f[t]) == p.tick);
        CHECK(std::stod(f[v]) == e.telemetry.speed);
        CHECK(std::stod(f[n]) == e.state.engine_speed);
        CHECK(std::stoi(f[g]) == e.state.gear);
        CHECK(std::stod(f[throttle]) == p.throttle);
        CHECK(std::stod(f[steer]) == p.steering);
        CHECK(std::stod(f[brake]) == p.brake);
        CHECK(std::stod(f[load]) == e.telemetry.wheel_load[rear_right]);
        CHECK(std::stod(f[slip]) == e.telemetry.slip[front_left]);
        CHECK(std::stod(f[fx]) == e.telemetry.force_longitudinal[front_right]);
        CHECK(std::stod(f[fy]) == e.telemetry.force_lateral[rear_left]);
        CHECK(std::stod(f[alpha]) == e.state.slip_angle[front_right]);
        if (e.telemetry.force_lateral[rear_left] != 0.0 && e.telemetry.force_longitudinal[front_right] != 0.0) {
            forces_seen = true;
        }
    }
    CHECK(forces_seen);
    // The maneuver really drives: the car moves and has load on the wheels.
    CHECK(run.steps.back().telemetry.speed > 5.0); // measured: 13.6 m/s, band: > 5.0 m/s
    CHECK(run.steps.back().telemetry.wheel_load[rear_right] > 1000.0); // measured: 2339 N, band: > 1000 N
}

TEST_CASE("The telemetry does not change the drive") {
    const Run with = drive_maneuver(true, 240);
    const Run without = drive_maneuver(false, 240);
    CHECK(with.checksum == without.checksum);
    CHECK(with.checksum != 0);
}
