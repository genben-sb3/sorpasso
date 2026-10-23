#include "ultragravity/test_maneuver.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <ostream>
#include <utility>
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
constexpr double slide_min_speed = 1.0;         // m/s, below this there is no drift angle to judge

constexpr double circle_radius = 100.0;          // m
constexpr double circle_tolerance = 5.0;         // m off the circle the test driver must stay within
constexpr double circle_start_speed = 20.0;      // m/s
constexpr double circle_gear_speed = 50.0;       // m/s at which the chosen gear still stays below the shift speed
constexpr double circle_speed_gain = 0.25;       // m/s², rise of the target speed
constexpr double circle_engage = 0.4;            // s clutch pressed
constexpr double circle_clutch_release = 0.5;    // s for letting the clutch out
constexpr double circle_window = 1.0;            // s over which the lateral acceleration is averaged
constexpr double circle_abort = 1.0;             // s outside the tolerance end the skidpad
constexpr double circle_at_most = 400.0;         // s
constexpr double circle_throttle_p = 0.3;        // 1 per m/s of speed error
constexpr double circle_throttle_i = 0.1;        // 1/s, share of the speed error integrated per second
constexpr double circle_integral_low = -0.5;     // 1, lower bound of the integral part
constexpr double circle_integral_high = 1.0;     // 1, upper bound of the integral part
constexpr double circle_throttle_base = 0.1;     // 1, throttle that carries the rolling resistance
constexpr double lane_tire_margin = 0.2;         // m, body width over the track width
constexpr double lane_run_up = 30.0;             // m straight ahead before the lane
constexpr double lane_run_out = 10.0;            // m behind the lane
constexpr double lane_lookahead = 0.2;           // s lookahead of the driver
constexpr double lane_lookahead_min = 3.0;       // m
constexpr double lane_start = 40.0 * kmh;        // m/s, lowest entry speed
constexpr double lane_at_most = 250.0 * kmh;     // m/s
constexpr double lane_step = 5.0 * kmh;          // m/s, step of the search
constexpr double lane_accuracy = 1.0 * kmh;      // m/s

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

    // The car moves sideways to its longitudinal axis (spin in a corner): direction of travel and
    // longitudinal axis are far apart.
    bool slid_sideways() const {
        const BodyMotion& b = world_.body();
        const Vec3 v = rotate_inverse(b.orientation, b.velocity); // m/s, vehicle frame
        if (std::hypot(v.x, v.y) < slide_min_speed) return false;
        return drift_angle_magnitude(v) > spin_angle;
    }

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

Measurement brake(Measurement m, const VehicleParams& params, bool with_abs, std::ostream* telemetry) {
    constexpr double from = 100.0 * kmh; // m/s
    Run run(params, braking_run_up, telemetry);
    InputPacket rolling;
    rolling.clutch = 1.0;
    run.rest(braking_roll, rolling);
    if (run.speed() < from) return fail(m, "rolls slower than 100 km/h already before braking");

    InputPacket braking = rolling;
    braking.brake = 1.0;
    braking.driving_aids.abs = with_abs;
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

// The skidpad: the test driver holds a 100 m radius and raises the speed slowly until the car
// no longer holds the circle. The result is the highest lateral acceleration it held within
// the tolerance, averaged over a window.
Measurement run_skidpad(Measurement m, const VehicleParams& params, std::ostream* telemetry) {
    Run run(params, circle_start_speed, telemetry);
    SteeringController steer(params);

    // The gear: the lowest in which the car stays below the shift speed at circle_gear_speed.
    const Gearbox& gearbox = params.drivetrain.gearbox;
    const double wheel_radius = params.rear.wheel_radius; // m
    const int gears = static_cast<int>(gearbox.ratios.size());
    int gear = gears;
    for (int g = 1; g <= gears; ++g) {
        if (circle_gear_speed / wheel_radius * overall_ratio(gearbox, g) <= TestDriver::shift_speed) {
            gear = g;
            break;
        }
    }

    const std::size_t window = static_cast<std::size_t>(std::lround(circle_window / TestWorld::time_step));
    std::vector<double> lateral; // m/s² per tick; 0 when the tick lay outside the tolerance
    double target_speed = circle_start_speed; // m/s
    double integral = 0.0;                    // 1, integral part of the throttle controller
    double highest = 0.0;                     // m/s², highest window mean
    double outside_for = 0.0;                 // s
    while (run.time() < circle_at_most) {
        const BodyMotion& b = run.world().body();
        const double speed = std::hypot(b.velocity.x, b.velocity.y); // m/s

        InputPacket p;
        p.gear_request = static_cast<std::int8_t>(gear);
        p.steering = steer.circle(b, circle_radius);
        if (run.time() < circle_engage) {
            p.clutch = 1.0;
        } else {
            p.clutch = std::max(1.0 - (run.time() - circle_engage) / circle_clutch_release, 0.0);
            if (p.clutch == 0.0) target_speed += circle_speed_gain * TestWorld::time_step;
            const double error = target_speed - speed; // m/s
            integral = std::clamp(integral + error * TestWorld::time_step * circle_throttle_i, circle_integral_low,
                                  circle_integral_high);
            const double control = circle_throttle_p * error + integral + circle_throttle_base; // 1
            p.throttle = std::clamp(control, 0.0, 1.0);
            p.brake = std::clamp(-control, 0.0, 1.0);
        }
        run.tick(p);

        const BodyMotion& n = run.world().body();
        const bool inside = std::abs(SteeringController::circle_error(n, circle_radius)) <= circle_tolerance;
        const double a = std::hypot(n.velocity.x, n.velocity.y) * n.angular_velocity.z; // m/s²
        lateral.push_back(inside ? a : 0.0);
        outside_for = inside ? 0.0 : outside_for + TestWorld::time_step;
        if (run.slid_sideways()) return fail(m, "the car has spun");
        if (lateral.size() >= window) {
            double sum = 0.0; // m/s²
            for (std::size_t i = lateral.size() - window; i < lateral.size(); ++i) sum += lateral[i];
            highest = std::max(highest, sum / static_cast<double>(window));
        }
        if (outside_for > circle_abort) {
            if (highest <= 0.0) return fail(m, "never holds the radius within the tolerance");
            m.result = highest / TestWorld::gravity;
            return m;
        }
    }
    return fail(m, "holds the radius up to " + number(target_speed / kmh, 0) + " km/h, the grip is not used up");
}

// The lane after ISO 3888-2 for a car of width `width`; x runs from the entry of the lane.
struct Lane {
    double width; // m

    static constexpr double offset = 1.0;                                         // m between the lane centers
    static constexpr std::array<double, 5> section{12.0, 13.5, 11.0, 12.5, 12.0}; // m length
    static constexpr double length = 61.0;                                        // m, sum of section

    // Target path of the driver (center): smooth transition into the offset lane and back.
    static double center(double x) {
        const auto smooth = [](double t) {
            t = std::clamp(t, 0.0, 1.0);
            return t * t * (3.0 - 2.0 * t);
        };
        const double x1 = section[0], x2 = x1 + section[1], x3 = x2 + section[2]; // m
        if (x < x3) return offset * smooth((x - x1) / section[1]);
        return offset * (1.0 - smooth((x - x3) / section[3]));
    }

    // Lane center and allowed distance of the car's center from it in the section at x (both m);
    // nullopt: free (transition).
    std::optional<std::pair<double, double>> allowed(double x) const {
        const double x1 = section[0], x2 = x1 + section[1], x3 = x2 + section[2], x4 = x3 + section[3]; // m
        if (x >= 0.0 && x <= x1) return std::pair{0.0, (0.1 * width + 0.25) / 2.0};
        if (x >= x2 && x <= x3) return std::pair{offset, 0.5}; // lane width w + 1.0
        if (x >= x4 && x <= length) return std::pair{0.0, (0.3 * width + 0.25) / 2.0};
        return std::nullopt;
    }
};

// One pass of the lane change with entry speed `speed` in m/s. Empty result: succeeded,
// otherwise the reason.
std::optional<std::string> run_lane_change(const VehicleParams& params, double speed, bool with_esc,
                                           std::ostream* telemetry) {
    const Lane lane{std::max(params.front.track_width, params.rear.track_width) + lane_tire_margin};
    Run run(params, speed, telemetry);
    SteeringController steer(params);
    InputPacket p;
    p.clutch = 1.0; // clutch in, throttle off, as in the moose test
    p.driving_aids.stability_control = with_esc;
    const auto path = [](double x) { return Lane::center(x - lane_run_up); };
    while (run.position() < lane_run_up + Lane::length + lane_run_out) {
        p.steering = steer.path(run.world().body(), path, std::max(lane_lookahead_min, lane_lookahead * speed));
        run.tick(p);
        if (run.spun()) return "the car has spun";
        const double x = run.position() - lane_run_up; // m
        if (const auto allowed = lane.allowed(x)) {
            const double distance = std::abs(run.world().body().position.y - allowed->first); // m
            if (distance > allowed->second) return "leaves the lane at " + number(x, 0) + " m";
        }
        if (run.speed() < standstill) return "comes to a stop";
    }
    return std::nullopt;
}

Measurement find_lane_change(Measurement m, const VehicleParams& params, bool with_esc, std::ostream* telemetry) {
    double last = 0.0; // m/s, highest successful entry speed of the staircase
    std::string reason;
    double speed = lane_start; // m/s
    for (; speed <= lane_at_most; speed += lane_step) {
        if (const auto failure = run_lane_change(params, speed, with_esc, nullptr)) {
            reason = *failure;
            break;
        }
        last = speed;
    }
    if (last <= 0.0) return fail(m, "already at " + number(lane_start / kmh, 0) + " km/h: " + reason);
    double good = last, bad = speed; // m/s
    if (!reason.empty()) {
        while (bad - good > lane_accuracy) {
            const double mid = 0.5 * (good + bad); // m/s
            (run_lane_change(params, mid, with_esc, nullptr) ? bad : good) = mid;
        }
    }
    if (telemetry) run_lane_change(params, good, with_esc, telemetry);
    m.result = good / kmh;
    return m;
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
    out << pad(name, 50) << pad(target, 14) << pad(result, 14) << deviation << '\n';
}

int decimals(const Measurement& m) { return m.unit == "m" || m.unit == "km/h" ? 1 : 2; }

} // namespace

std::string format_table(const std::string& car_name, const std::vector<Measurement>& measurements) {
    std::ostringstream out;
    out << "Measurement: " << car_name << "\n\n";
    write_row(out, "Test maneuver", "Target", "Result", "Deviation");
    out << std::string(88, '-') << '\n';
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
    case Maneuver::braking_100_0_abs: return "braking_100_0_abs";
    case Maneuver::top_speed: return "top_speed";
    case Maneuver::skidpad: return "skidpad";
    case Maneuver::lane_change: return "lane_change";
    case Maneuver::lane_change_esc: return "lane_change_esc";
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
        return brake(begin(maneuver, "Braking 100–0 km/h without ABS", "m", t.braking_100_0), params, false, telemetry);
    case Maneuver::braking_100_0_abs:
        return brake(begin(maneuver, "Braking 100–0 km/h with ABS", "m", std::nullopt), params, true, telemetry);
    case Maneuver::top_speed: {
        std::optional<double> target;
        if (t.top_speed) target = *t.top_speed / kmh;
        return find_top_speed(begin(maneuver, "Top speed", "km/h", target), params, telemetry);
    }
    case Maneuver::skidpad:
        return run_skidpad(begin(maneuver, "Skidpad 100 m radius", "g", t.skidpad), params, telemetry);
    case Maneuver::lane_change: {
        std::optional<double> target;
        if (t.lane_change) target = *t.lane_change / kmh;
        return find_lane_change(begin(maneuver, "Lane change (moose test)", "km/h", target), params, false, telemetry);
    }
    case Maneuver::lane_change_esc:
        return find_lane_change(begin(maneuver, "Lane change (moose test) with stability control", "km/h", std::nullopt),
                                params, true, telemetry);
    }
    return {};
}

std::vector<Measurement> measure(const VehicleParams& params) {
    std::vector<Measurement> measurements;
    for (Maneuver m : all_maneuvers) measurements.push_back(run_test_maneuver(m, params));
    return measurements;
}

} // namespace ultragravity
