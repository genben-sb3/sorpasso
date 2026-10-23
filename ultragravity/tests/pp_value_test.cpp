#include "doctest/doctest.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "drive.h"
#include "nlohmann/json.hpp"
#include "ultragravity/pp_value.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

Measurement measurement(Maneuver m, double result) {
    Measurement x;
    x.maneuver = m;
    x.name = maneuver_id(m);
    x.result = result;
    return x;
}

// Acceleration 0–100, 0–200, braking distance, top speed, skidpad; a `factor` above 1 is better.
std::vector<Measurement> measurements(double factor) {
    return {measurement(Maneuver::acceleration_0_100, 6.0 / factor),
            measurement(Maneuver::acceleration_0_200, 20.0 / factor),
            measurement(Maneuver::braking_100_0, 40.0 / factor), measurement(Maneuver::top_speed, 250.0 * factor),
            measurement(Maneuver::skidpad, 1.0 * factor)};
}

std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

} // namespace

TEST_CASE("A car that is better in all test maneuvers gets more PP") {
    const PpResult worse = compute_pp(measurements(0.9));
    const PpResult better = compute_pp(measurements(1.1));
    REQUIRE(worse.value);
    REQUIRE(better.value);
    CHECK(*better.value > *worse.value); // measured: 110 against 90, band: exact (only greater)

    // A single better maneuver is enough as well.
    auto slightly_better = measurements(1.0);
    slightly_better[3].result = 275.0;
    CHECK(*compute_pp(slightly_better).value > *compute_pp(measurements(1.0)).value); // measured: 102 against 100, band: exact (only greater)
}

TEST_CASE("The reference results yield exactly 100 PP") {
    // Every result equals its reference value: every factor is 1, so the weighted geometric mean is 1.
    CHECK(compute_pp(measurements(1.0)).value == 100);
}

TEST_CASE("A failed test maneuver yields a message instead of a PP value") {
    auto m = measurements(1.0);
    m[4].result.reset();
    m[4].reason = "the car has spun";
    const PpResult pp = compute_pp(m);
    CHECK_FALSE(pp.value);
    CHECK(pp.error.find("the car has spun") != std::string::npos);
    CHECK(pp.error.find("No PP value") != std::string::npos);
}

TEST_CASE("A missing test maneuver yields a message instead of a PP value") {
    auto m = measurements(1.0);
    m.pop_back();
    const PpResult pp = compute_pp(m);
    CHECK_FALSE(pp.value);
    CHECK(pp.error.find("skidpad: not driven") != std::string::npos);
}

TEST_CASE("set_pp only changes the pp block and replaces an existing one") {
    const std::string original = read_file(UG_VEHICLES_DIR "/sports_coupe.json");
    const std::string first = set_pp(original, {123, "abc"});
    const auto a = nlohmann::json::parse(original);
    auto b = nlohmann::json::parse(first);
    REQUIRE(b.contains("pp"));
    CHECK(b["pp"]["value"] == 123);
    b.erase("pp");
    CHECK(a == b);
    // The rest of the text stays character for character.
    CHECK(first.starts_with(original.substr(0, original.rfind('}') - 1)));

    const std::string second = set_pp(first, {99, "def"});
    CHECK(read_pp(second)->value == 99);
    CHECK(read_pp(second)->physics_version == "def");
    CHECK(second.find("abc") == std::string::npos);
    CHECK(set_pp(second, {123, "abc"}) == first);

    // The vehicle file stays loadable with a pp block.
    CHECK_FALSE(parse_vehicle_file(first).error.has_value());
}

TEST_CASE("The physics version id is stable and depends on the car") {
    VehicleParams a = sports_coupe();
    VehicleParams b = sports_coupe();
    b.mass += 100.0;
    CHECK(physics_version_id(a) == physics_version_id(a));
    CHECK(physics_version_id(a) != physics_version_id(b));
    CHECK(physics_version_id(a).size() == 16);
}

TEST_CASE("An outdated PP value raises a warning") {
    const std::string current = physics_version_id(sports_coupe());
    CHECK(pp_warning(StoredPp{100, current}, current).empty());
    CHECK(pp_warning(std::nullopt, current).empty());
    const std::string w = pp_warning(StoredPp{100, "old"}, current);
    CHECK(w.find("outdated") != std::string::npos);
}
