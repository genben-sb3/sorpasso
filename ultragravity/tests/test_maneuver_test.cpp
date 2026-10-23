#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include "drive.h"
#include "ultragravity/test_driver.h"
#include "ultragravity/test_maneuver.h"
#include "ultragravity/units.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

// The drive of a test maneuver, read back from the telemetry CSV. Every
// measurement below only computes from these rows and knows nothing of the tool.
struct Trace {
    std::vector<double> speed;    // m/s at the end of the tick
    std::vector<double> throttle; // 1
    std::vector<double> brake;    // 1
};

Trace read_trace(const std::string& csv) {
    std::stringstream ss(csv);
    std::string line;
    std::getline(ss, line); // header
    Trace t;
    while (std::getline(ss, line)) {
        std::vector<double> fields;
        std::stringstream ls(line);
        for (std::string f; std::getline(ls, f, ',');) fields.push_back(std::stod(f));
        t.speed.push_back(fields.at(1));
        t.throttle.push_back(fields.at(5));
        t.brake.push_back(fields.at(6));
    }
    return t;
}

// Seconds from the first throttle input until the speed first reaches the limit.
double time_to(const Trace& t, double limit) {
    std::size_t start = 0;
    while (t.throttle.at(start) == 0.0) ++start;
    for (std::size_t i = start; i < t.speed.size(); ++i) {
        if (t.speed[i] >= limit) {
            const double before = t.speed[i - 1];                      // m/s
            const double fraction = (limit - before) / (t.speed[i] - before); // 1
            return (static_cast<double>(i - start) + fraction) * TestWorld::time_step;
        }
    }
    FAIL("limit never reached");
    return 0.0;
}

} // namespace

TEST_CASE("The time 0–100 km/h matches a measurement on the trace of the drive") {
    std::ostringstream csv;

    const Measurement m = run_test_maneuver(Maneuver::acceleration_0_100, sports_coupe(), &csv);

    REQUIRE_FALSE(m.failed());
    CHECK(m.unit == "s");
    // measured: 6.113 s, deviation below 0.01 %, band: ±0.5 % (tick resolution of the interpolation)
    CHECK(*m.result == doctest::Approx(time_to(read_trace(csv.str()), kmh_100)).epsilon(0.005));
}

TEST_CASE("The time 0–200 km/h matches a measurement on the trace of the drive") {
    std::ostringstream csv;

    const Measurement m = run_test_maneuver(Maneuver::acceleration_0_200, sports_coupe(), &csv);

    REQUIRE_FALSE(m.failed());
    const Trace trace = read_trace(csv.str());
    // measured: 17.84 s, deviation below 0.01 %, band: ±0.5 % (tick resolution of the interpolation)
    CHECK(*m.result == doctest::Approx(time_to(trace, 200.0 * kmh)).epsilon(0.005));
    CHECK(*m.result > time_to(trace, kmh_100));
}

TEST_CASE("The braking distance 100–0 km/h is the area under the speed curve of the drive") {
    std::ostringstream csv;

    const Measurement m = run_test_maneuver(Maneuver::braking_100_0, sports_coupe(), &csv);

    REQUIRE_FALSE(m.failed());
    CHECK(m.unit == "m");
    const Trace t = read_trace(csv.str());
    std::size_t i = 0;
    while (t.brake.at(i) == 0.0 || t.speed[i] > kmh_100) ++i; // first tick below 100 km/h while braking
    double distance = 0.0; // m, trapezoid under the curve
    for (; t.speed[i] > 0.05; ++i) distance += 0.5 * (t.speed[i] + t.speed[i + 1]) * TestWorld::time_step;
    // The start of the measurement lies at most one tick (about 0.2 m) from the crossing of 100 km/h in the trace.
    // measured: 33.73 m against 33.93 m (-0.58 %), band: ±1 % (start of the measurement up to one tick beside the crossing)
    CHECK(*m.result == doctest::Approx(distance).epsilon(0.01));
}

TEST_CASE("The top speed is the speed the drive has settled at in the end") {
    std::ostringstream csv;

    const Measurement m = run_test_maneuver(Maneuver::top_speed, sports_coupe(), &csv);

    REQUIRE_FALSE(m.failed());
    CHECK(m.unit == "km/h");
    const Trace t = read_trace(csv.str());
    const double fastest = *std::max_element(t.speed.begin(), t.speed.end()) / kmh; // km/h
    // measured: 279.3 km/h, deviation below 0.01 %, band: ±0.5 % (final value against the maximum of the trace)
    CHECK(*m.result == doctest::Approx(fastest).epsilon(0.005));
}

TEST_CASE("A test maneuver whose goal the car does not reach counts as failed and gives the reason") {
    VehicleParams weak = sports_coupe();
    for (TorquePoint& p : weak.drivetrain.engine.torque_curve) p.torque *= 0.3;

    const Measurement m = run_test_maneuver(Maneuver::acceleration_0_200, weak);

    CHECK(m.failed());
    CHECK_FALSE(m.result.has_value());
    CHECK(m.reason.find("200 km/h not") != std::string::npos);
}

TEST_CASE("A braking distance in which the car does not come to a stop counts as failed and gives the reason") {
    VehicleParams without_brakes = sports_coupe();
    without_brakes.brakes.torque *= 0.001;

    const Measurement m = run_test_maneuver(Maneuver::braking_100_0, without_brakes);

    CHECK(m.failed());
    CHECK_FALSE(m.result.has_value());
    CHECK(m.reason.find("not stopped") != std::string::npos);
}

TEST_CASE("A top speed that does not settle counts as failed and gives the reason") {
    VehicleParams weak = sports_coupe();
    for (TorquePoint& p : weak.drivetrain.engine.torque_curve) p.torque *= 0.05;

    const Measurement m = run_test_maneuver(Maneuver::top_speed, weak);

    CHECK(m.failed());
    CHECK_FALSE(m.result.has_value());
    CHECK_FALSE(m.reason.empty());
}

TEST_CASE("Target values and deviation come from the vehicle file; without a target there is no deviation") {
    VehicleParams car = sports_coupe();
    car.target_values = {};
    car.target_values.acceleration_0_100 = 5.0;

    const std::vector<Measurement> ms = measure(car);

    REQUIRE(ms.size() == all_maneuvers.size());
    CHECK(ms[0].target.value() == doctest::Approx(5.0));
    CHECK(ms[0].deviation().value() == doctest::Approx(*ms[0].result - 5.0));
    CHECK_FALSE(ms[1].target.has_value());
    CHECK_FALSE(ms[1].deviation().has_value());
    CHECK_FALSE(ms[3].target.has_value());
}

TEST_CASE("The table shows target, result and deviation, missing targets as \"no target\" and failed ones with their reason") {
    Measurement succeeded;
    succeeded.name = "Acceleration 0–100 km/h";
    succeeded.unit = "s";
    succeeded.result = 6.25;
    succeeded.target = 5.5;
    Measurement without_target = succeeded;
    without_target.name = "Braking 100–0 km/h without ABS";
    without_target.target.reset();
    Measurement failed;
    failed.name = "Top speed";
    failed.unit = "km/h";
    failed.reason = "does not settle";

    const std::string table = format_table("Sports Coupé", {succeeded, without_target, failed});

    CHECK(table.find("Measurement: Sports Coupé") != std::string::npos);
    CHECK(table.find("5.50 s") != std::string::npos);
    CHECK(table.find("6.25 s") != std::string::npos);
    CHECK(table.find("+0.75 s") != std::string::npos);
    CHECK(table.find("no target") != std::string::npos);
    CHECK(table.find("failed") != std::string::npos);
    CHECK(table.find("Top speed: does not settle") != std::string::npos);
}

TEST_CASE("The table pads by characters, not bytes, so multi-byte names keep the columns aligned") {
    Measurement dash;
    dash.name = "Acceleration 0–100 km/h"; // the dash takes three bytes
    dash.unit = "s";
    dash.result = 6.0;
    Measurement plain = dash;
    plain.name = "Acceleration 0-100 km/h";

    const std::string table = format_table("Car", {dash, plain});

    std::istringstream lines(table);
    std::vector<std::size_t> columns; // character position of the result in each row
    for (std::string line; std::getline(lines, line);) {
        const std::size_t at = line.find("6.00 s");
        if (at == std::string::npos) continue;
        std::size_t characters = 0;
        for (std::size_t i = 0; i < at; ++i) characters += (static_cast<unsigned char>(line[i]) & 0xC0) != 0x80 ? 1 : 0;
        columns.push_back(characters);
    }
    REQUIRE(columns.size() == 2);
    CHECK(columns[0] == columns[1]);
}

TEST_CASE("The sports coupe carries target values for all straight test maneuvers") {
    const TargetValues& t = sports_coupe().target_values;

    CHECK(t.acceleration_0_100.has_value());
    CHECK(t.acceleration_0_200.has_value());
    CHECK(t.braking_100_0.has_value());
    CHECK(t.top_speed.has_value());
}

namespace {

VehicleParams with_grip(double factor) {
    VehicleParams car = sports_coupe();
    car.front.tire.peak_friction *= factor;
    car.rear.tire.peak_friction *= factor;
    return car;
}

} // namespace

TEST_CASE("The skidpad yields the lateral acceleration from speed and radius of the drive") {
    std::ostringstream csv;

    const Measurement m = run_test_maneuver(Maneuver::skidpad, sports_coupe(), &csv);

    REQUIRE_FALSE(m.failed());
    CHECK(m.unit == "g");
    const Trace t = read_trace(csv.str());
    const double fastest = *std::max_element(t.speed.begin(), t.speed.end()); // m/s
    const double highest = fastest * fastest / 100.0 / TestWorld::gravity;    // g
    // The result is a one-second mean with the radius inside the tolerance, so just below the peak.
    // measured: 1.137 g against 1.234 g peak (×0.92), band: 0.9 to 1.02
    CHECK(*m.result <= highest * 1.02);
    CHECK(*m.result >= highest * 0.9); // measured: see above
}

TEST_CASE("The skidpad raises the speed up to the limit, not until the time runs out") {
    std::ostringstream csv;

    run_test_maneuver(Maneuver::skidpad, sports_coupe(), &csv);

    const Trace t = read_trace(csv.str());
    // measured: 34.8 m/s at the end against 27.4 m/s at half time (×1.27), band: at least 1.1
    CHECK(t.speed.back() > t.speed[t.speed.size() / 2] * 1.1);
    // measured: 61.5 s, band: < 77 s (×1.25)
    CHECK(static_cast<double>(t.speed.size()) * TestWorld::time_step < 77.0);
}

TEST_CASE("A car with more tire grip reaches more g on the skidpad") {
    const Measurement normal = run_test_maneuver(Maneuver::skidpad, sports_coupe());
    const Measurement sticky = run_test_maneuver(Maneuver::skidpad, with_grip(1.2));

    REQUIRE_FALSE(normal.failed());
    REQUIRE_FALSE(sticky.failed());
    // measured: 1.328 g against 1.137 g (+17 %), band: at least +10 %
    CHECK(*sticky.result > *normal.result * 1.1);
}

TEST_CASE("The steering controller measures the circle error from the center of the circle") {
    BodyMotion b;
    b.position = {0.0, 0.0, 0.0};
    CHECK(SteeringController::circle_error(b, 100.0) == doctest::Approx(0.0));
    b.position = {100.0, 100.0, 0.0};
    CHECK(SteeringController::circle_error(b, 100.0) == doctest::Approx(0.0));
    b.position = {0.0, 210.0, 0.0};
    CHECK(SteeringController::circle_error(b, 100.0) == doctest::Approx(10.0)); // outside
    b.position = {0.0, 20.0, 0.0};
    CHECK(SteeringController::circle_error(b, 100.0) == doctest::Approx(-20.0)); // inside
}

TEST_CASE("The steering controller follows a path to the left when it lies left and to the right when it lies right") {
    SteeringController steer(sports_coupe());
    BodyMotion b;
    b.orientation = {1.0, 0.0, 0.0, 0.0};
    b.velocity = {20.0, 0.0, 0.0};

    const double left = steer.path(b, [](double) { return 1.0; }, 10.0);
    const double right = steer.path(b, [](double) { return -1.0; }, 10.0);
    const double straight = steer.path(b, [](double) { return 0.0; }, 10.0);

    CHECK(left > 0.0); // measured: positive, band: sign
    CHECK(right == doctest::Approx(-left));
    CHECK(straight == doctest::Approx(0.0));
}

TEST_CASE("The lane change finds an entry speed a human calls sensible") {
    const Measurement m = run_test_maneuver(Maneuver::lane_change, sports_coupe());

    REQUIRE_FALSE(m.failed());
    CHECK(m.unit == "km/h");
    // measured: 105.0 km/h, band: 70 to 130 km/h (moose test results lie at 80 to 110 km/h)
    CHECK(*m.result > 70.0);
    CHECK(*m.result < 130.0); // measured: see above
}

TEST_CASE("A car with less tire grip passes the lane change only slower") {
    const Measurement normal = run_test_maneuver(Maneuver::lane_change, sports_coupe());
    const Measurement slippery = run_test_maneuver(Maneuver::lane_change, with_grip(0.7));

    REQUIRE_FALSE(normal.failed());
    REQUIRE_FALSE(slippery.failed());
    // measured: 86.9 km/h against 105.0 km/h (×0.83), band: at most 0.9
    CHECK(*slippery.result < *normal.result * 0.9);
}

TEST_CASE("A lane change the car fails already at the lowest speed gives the reason") {
    // Peak friction 0.15 (0.12 × 1.25): the friction of the package test, which ran a car with 1.05.
    const Measurement m = run_test_maneuver(Maneuver::lane_change, with_grip(0.12));

    // measured: fails at 40 km/h, the car leaves the lane after 32 m
    CHECK(m.failed());
    CHECK(m.reason.find("already at 40 km/h") != std::string::npos);
}

TEST_CASE("The vehicle file carries target values for skidpad and lane change") {
    const TargetValues& t = sports_coupe().target_values;

    CHECK(t.skidpad.has_value());
    CHECK(t.lane_change.has_value());
}
