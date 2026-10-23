#include "ultragravity/test_maneuver.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <ostream>
#include <sstream>

#include "ultragravity/telemetry_csv.h"
#include "ultragravity/test_driver.h"
#include "ultragravity/test_world.h"
#include "ultragravity/units.h"

namespace ultragravity {
namespace {

constexpr double drop_height = 0.02;            // m, height of the wheels above the ground when set down
constexpr double settle = 1.5;                  // s at rest before pulling away, so the car settles on its springs
constexpr double accelerate_at_most = 60.0;     // s, after that an acceleration maneuver counts as failed
constexpr double top_speed_at_most = 300.0;     // s until the top speed has settled
constexpr double settle_window = 5.0;           // s over which the speed may hardly change
constexpr double settle_threshold = 0.05;       // m/s gained in the settle window, below that it counts as reached
constexpr double braking_run_up = 110.0 * kmh;  // m/s starting speed, so 100 km/h is surely reached
constexpr double braking_roll = 1.0;            // s rolling with released pedals before braking
constexpr double braking_at_most = 30.0;        // s, after that braking counts as failed
constexpr double standstill = 0.05;             // m/s, below this the car stands
constexpr double spin_angle = 60.0 * std::numbers::pi / 180.0; // rad off the initial direction

std::string number(double value, int digits) {
    std::ostringstream s;
    s.imbue(std::locale::classic());
    s.precision(digits);
    s << std::fixed << value;
    return s.str();
}

// The test world together with a tick counter and optional telemetry. The tool controls
// the car only through `tick(InputPacket)`.
class Run {
public:
    Run(const VehicleParams& params, double speed, std::ostream* telemetry)
        : world_(params, drop_height, TestGround{}, speed), telemetry_(telemetry) {
        if (telemetry_) write_telemetry_header(*telemetry_);
    }

    void tick(InputPacket packet) {
        packet.tick = tick_++;
        world_.tick(packet);
        if (telemetry_) write_telemetry_row(*telemetry_, packet, world_.last_step());
    }

    void rest(double seconds, InputPacket packet = {}) {
        for (int i = 0; i < static_cast<int>(std::lround(seconds / TestWorld::time_step)); ++i) tick(packet);
    }

    const TestWorld& world() const { return world_; }

    // Speed along the vehicle's longitudinal axis in m/s.
    double speed() const { return world_.last_step().telemetry.speed; }
    double position() const { return world_.body().position.x; } // m along the initial direction (X)
    double time() const { return tick_ * TestWorld::time_step; } // s

    // The rear has come around: the heading is far off X.
    bool spun() const {
        const Vec3 forward = rotate(world_.body().orientation, {1.0, 0.0, 0.0});
        return std::acos(std::clamp(forward.x, -1.0, 1.0)) > spin_angle;
    }

private:
    TestWorld world_;
    std::ostream* telemetry_;
    std::uint32_t tick_ = 0;
};

Measurement begin(Maneuver maneuver, const char* name, const char* unit, std::optional<double> target) {
    Measurement m;
    m.maneuver = maneuver;
    m.name = name;
    m.unit = unit;
    m.target = target;
    return m;
}

Measurement fail(Measurement m, std::string reason) {
    m.result.reset();
    m.reason = std::move(reason);
    return m;
}

// The test driver accelerates from standstill. `target_speed` in m/s.
Measurement accelerate(Measurement m, const VehicleParams& params, double target_speed, std::ostream* telemetry) {
    Run run(params, 0.0, telemetry);
    run.rest(settle);
    TestDriver driver(params);
    const double throttle_from = run.time() + TestDriver::engage; // s, time of the first throttle input

    double fastest = 0.0; // m/s
    while (run.time() - throttle_from < accelerate_at_most) {
        const double before = run.speed(); // m/s
        run.tick(driver.packet(run.world()));
        const double now = run.speed(); // m/s
        fastest = std::max(fastest, now);
        if (run.spun()) return fail(m, "the car has spun");
        if (now >= target_speed) {
            const double fraction = (target_speed - before) / (now - before); // 1, position of the limit within the tick
            m.result = run.time() - TestWorld::time_step * (1.0 - fraction) - throttle_from;
            return m;
        }
    }
    return fail(m, "after " + number(accelerate_at_most, 0) + " s only " + number(fastest / kmh, 0) +
                       " km/h reached, " + number(target_speed / kmh, 0) + " km/h not");
}

Measurement brake(Measurement m, const VehicleParams& params, std::ostream* telemetry) {
    constexpr double from = 100.0 * kmh; // m/s
    Run run(params, braking_run_up, telemetry);
    InputPacket rolling;
    rolling.clutch = 1.0;
    run.rest(braking_roll, rolling);
    if (run.speed() < from) return fail(m, "rolls slower than 100 km/h already before braking");

    InputPacket braking = rolling;
    braking.brake = 1.0;
    braking.driving_aids.abs = false;
    const double brake_start = run.time(); // s
    std::optional<double> start_position;  // m, where the speed drops below 100 km/h
    while (run.time() - brake_start < braking_at_most) {
        const double position_before = run.position(); // m
        const double before = run.speed();             // m/s
        run.tick(braking);
        const double now = run.speed(); // m/s
        if (run.spun()) return fail(m, "the car has spun");
        if (!start_position && now <= from) {
            const double fraction = (before - from) / (before - now); // 1
            start_position = position_before + fraction * (run.position() - position_before);
        }
        if (start_position && now <= standstill) {
            m.result = run.position() - *start_position;
            return m;
        }
    }
    return fail(m, "not stopped after " + number(braking_at_most, 0) + " s of braking");
}

Measurement find_top_speed(Measurement m, const VehicleParams& params, std::ostream* telemetry) {
    Run run(params, 0.0, telemetry);
    run.rest(settle);
    TestDriver driver(params);
    const int last_gear = static_cast<int>(params.drivetrain.gearbox.ratios.size());
    const std::size_t window = static_cast<std::size_t>(std::lround(settle_window / TestWorld::time_step));

    std::vector<double> trace; // m/s per tick
    while (static_cast<double>(trace.size()) * TestWorld::time_step < top_speed_at_most) {
        run.tick(driver.packet(run.world()));
        trace.push_back(run.speed());
        if (run.spun()) return fail(m, "the car has spun");
        if (trace.size() > window && driver.clutch_engaged() &&
            trace.back() - trace[trace.size() - 1 - window] < settle_threshold) {
            if (driver.gear() < last_gear) {
                return fail(m, "settles in gear " + std::to_string(driver.gear()) + " at " +
                                   number(trace.back() / kmh, 0) + " km/h without shifting into the last one");
            }
            m.result = trace.back() / kmh;
            return m;
        }
    }
    return fail(m, "not settled after " + number(top_speed_at_most, 0) + " s, last " + number(trace.back() / kmh, 0) +
                       " km/h");
}

std::string with_sign(double value, int digits) { return (value >= 0.0 ? "+" : "") + number(value, digits); }

// Characters, not bytes: dashes and umlauts take several bytes.
std::string pad(const std::string& text, std::size_t width) {
    std::size_t characters = 0;
    for (unsigned char c : text) characters += (c & 0xC0) != 0x80 ? 1 : 0;
    return text + std::string(width > characters ? width - characters : 0, ' ');
}

void write_row(std::ostream& out, const std::string& name, const std::string& target, const std::string& result,
               const std::string& deviation) {
    out << pad(name, 36) << pad(target, 14) << pad(result, 14) << deviation << '\n';
}

int decimals(const Measurement& m) { return m.unit == "m" || m.unit == "km/h" ? 1 : 2; }

} // namespace

std::string format_table(const std::string& car_name, const std::vector<Measurement>& measurements) {
    std::ostringstream out;
    out << "Measurement: " << car_name << "\n\n";
    write_row(out, "Test maneuver", "Target", "Result", "Deviation");
    out << std::string(74, '-') << '\n';
    std::string reasons;
    for (const Measurement& m : measurements) {
        const int n = decimals(m);
        const std::string target = m.target ? number(*m.target, n) + " " + m.unit : "no target";
        if (m.failed()) {
            write_row(out, m.name, target, "failed", "");
            reasons += "  " + m.name + ": " + m.reason + '\n';
            continue;
        }
        const std::string deviation = m.deviation() ? with_sign(*m.deviation(), n) + " " + m.unit : "";
        write_row(out, m.name, target, number(*m.result, n) + " " + m.unit, deviation);
    }
    if (!reasons.empty()) out << "\nFailed:\n" << reasons;
    return out.str();
}

const char* maneuver_id(Maneuver maneuver) {
    switch (maneuver) {
    case Maneuver::acceleration_0_100: return "acceleration_0_100";
    case Maneuver::acceleration_0_200: return "acceleration_0_200";
    case Maneuver::braking_100_0: return "braking_100_0";
    case Maneuver::top_speed: return "top_speed";
    }
    return "";
}

Measurement run_test_maneuver(Maneuver maneuver, const VehicleParams& params, std::ostream* telemetry) {
    const TargetValues& t = params.target_values;
    switch (maneuver) {
    case Maneuver::acceleration_0_100:
        return accelerate(begin(maneuver, "Acceleration 0–100 km/h", "s", t.acceleration_0_100), params, 100.0 * kmh,
                          telemetry);
    case Maneuver::acceleration_0_200:
        return accelerate(begin(maneuver, "Acceleration 0–200 km/h", "s", t.acceleration_0_200), params, 200.0 * kmh,
                          telemetry);
    case Maneuver::braking_100_0:
        return brake(begin(maneuver, "Braking 100–0 km/h without ABS", "m", t.braking_100_0), params, telemetry);
    case Maneuver::top_speed: {
        std::optional<double> target;
        if (t.top_speed) target = *t.top_speed / kmh;
        return find_top_speed(begin(maneuver, "Top speed", "km/h", target), params, telemetry);
    }
    }
    return {};
}

std::vector<Measurement> measure(const VehicleParams& params) {
    std::vector<Measurement> measurements;
    for (Maneuver m : all_maneuvers) measurements.push_back(run_test_maneuver(m, params));
    return measurements;
}

} // namespace ultragravity
