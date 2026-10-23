#pragma once

#include <array>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "ultragravity/vehicle_file.h"

namespace ultragravity {

// The test maneuvers. The braking distance is driven without and with ABS (full braking, ABS
// switched on through the input packet; the vehicle file only names a target without ABS). The skidpad (100 m radius,
// result: highest steady lateral acceleration in g) and the lane change in the style of the
// moose test (ISO 3888-2, result: highest entry speed without a spin or leaving the lane)
// need the steering controller of the test driver.
enum class Maneuver {
    acceleration_0_100,
    acceleration_0_200,
    braking_100_0,
    braking_100_0_abs,
    top_speed,
    skidpad,
    lane_change
};

inline constexpr std::array<Maneuver, 7> all_maneuvers{
    Maneuver::acceleration_0_100, Maneuver::acceleration_0_200, Maneuver::braking_100_0, Maneuver::braking_100_0_abs,
    Maneuver::top_speed,          Maneuver::skidpad,            Maneuver::lane_change};

// Result of a test maneuver. Result and target are in the unit `unit` (s, m, km/h or g).
// A failed maneuver has no result, but a reason.
struct Measurement {
    Maneuver maneuver = Maneuver::acceleration_0_100;
    std::string name;
    std::string unit;
    std::optional<double> result; // in `unit`
    std::string reason;           // only for a failed maneuver
    std::optional<double> target; // in `unit`

    bool failed() const { return !result.has_value(); }
    // Result minus target; only with both present.
    std::optional<double> deviation() const {
        if (!result || !target) return std::nullopt;
        return *result - *target;
    }
};

// Drives a test maneuver with the test driver in the test world. With `telemetry`
// it writes the telemetry of every tick as CSV into the stream.
Measurement run_test_maneuver(Maneuver maneuver, const VehicleParams& params, std::ostream* telemetry = nullptr);

// All test maneuvers in the order of `all_maneuvers`.
std::vector<Measurement> measure(const VehicleParams& params);

// The table for the terminal (UTF-8): target, result and deviation per test maneuver.
// Without a target it says "no target", for a failed maneuver "failed"; the reasons follow below.
std::string format_table(const std::string& car_name, const std::vector<Measurement>& measurements);

// Short name usable as a file name, e.g. "acceleration_0_100".
const char* maneuver_id(Maneuver maneuver);

} // namespace ultragravity
