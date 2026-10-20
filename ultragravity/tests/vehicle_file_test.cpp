#include "doctest/doctest.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <numbers>
#include <sstream>
#include <string>

#include "nlohmann/json.hpp"
#include "ultragravity/vehicle_file.h"

using namespace ultragravity;

namespace {

const char* const sports_coupe_path = UG_VEHICLES_DIR "/sports_coupe.json";

// The sports coupe as a JSON tree, so a test can bend exactly one field.
nlohmann::json sports_coupe() {
    std::ifstream file(sports_coupe_path);
    return nlohmann::json::parse(file);
}

LoadResult parse(const nlohmann::json& j) { return parse_vehicle_file(j.dump()); }

} // namespace

TEST_CASE("The sports coupe loads from its vehicle file") {
    const LoadResult result = load_vehicle_file(sports_coupe_path);

    REQUIRE_FALSE(result.error.has_value());
    const VehicleParams& car = result.params;
    CHECK(car.name == "Sports Coupe");
    CHECK(car.mass == doctest::Approx(1350.0));
    CHECK(car.front.x - car.rear.x == doctest::Approx(2.6));
    CHECK(car.rear.suspension.damper_rebound > car.rear.suspension.damper_bump);
}

TEST_CASE("The sports coupe has 300 hp and six gears") {
    const LoadResult result = load_vehicle_file(sports_coupe_path);

    REQUIRE_FALSE(result.error.has_value());
    const Drivetrain& drivetrain = result.params.drivetrain;
    CHECK(drivetrain.gearbox.ratios.size() == 6);
    // The vehicle file states engine speeds in rpm, the parameters in rad/s.
    constexpr double rpm = 2.0 * std::numbers::pi / 60.0;
    CHECK(drivetrain.engine.idle_speed == doctest::Approx(900.0 * rpm));
    double peak_power = 0.0;
    for (const TorquePoint& p : drivetrain.engine.torque_curve) {
        peak_power = std::max(peak_power, p.speed * p.torque);
    }
    constexpr double hp = 735.49875; // W
    CHECK(peak_power / hp == doctest::Approx(300.0).epsilon(0.01));
}

TEST_CASE("Broken JSON reports line and column") {
    const LoadResult result = parse_vehicle_file("{\n  \"name\": \"Coupé\",\n  \"mass\": 13 50\n}");

    REQUIRE(result.error.has_value());
    CHECK(result.error->field == "");
    CHECK(result.error->message.find("line 3, column 15") != std::string::npos);
}

TEST_CASE("The column of a syntax error counts characters, not bytes") {
    // The "é" takes two bytes; the faulty x is the 19th character.
    const LoadResult result = parse_vehicle_file("{\"name\": \"Coupé\", x}");

    REQUIRE(result.error.has_value());
    CHECK(result.error->message.find("line 1, column 19") != std::string::npos);
}

TEST_CASE("A missing field is reported with its path") {
    nlohmann::json j = sports_coupe();
    j["axles"]["rear"]["suspension"].erase("travel");

    const LoadResult result = parse(j);

    REQUIRE(result.error.has_value());
    CHECK(result.error->field == "axles.rear.suspension.travel");
    CHECK(result.error->message == "is missing");
}

TEST_CASE("A field of the wrong type is reported with its path") {
    nlohmann::json j = sports_coupe();
    j["center_of_mass"]["z"] = "0.48";

    const LoadResult result = parse(j);

    REQUIRE(result.error.has_value());
    CHECK(result.error->field == "center_of_mass.z");
    CHECK(result.error->message == "is not a number");
}

TEST_CASE("A negative mass is rejected") {
    nlohmann::json j = sports_coupe();
    j["mass"] = -1350.0;

    const LoadResult result = parse(j);

    REQUIRE(result.error.has_value());
    CHECK(result.error->field == "mass");
    CHECK(result.error->message == "must be greater than 0");
}

TEST_CASE("A center of mass outside the wheelbase is rejected") {
    nlohmann::json j = sports_coupe();
    j["center_of_mass"]["x"] = 0.3; // ahead of the front axle at x = 0

    const LoadResult result = parse(j);

    REQUIRE(result.error.has_value());
    CHECK(result.error->field == "center_of_mass.x");
    CHECK(result.error->message == "must be between rear axle (-2.6) and front axle (0)");
}

TEST_CASE("A front axle behind the rear axle is rejected") {
    nlohmann::json j = sports_coupe();
    j["axles"]["front"]["x"] = -3.0;

    const LoadResult result = parse(j);

    REQUIRE(result.error.has_value());
    CHECK(result.error->field == "axles.front.x");
    CHECK(result.error->message == "must be ahead of the rear axle (-2.6)");
}

TEST_CASE("Tires without grip are rejected") {
    nlohmann::json j = sports_coupe();
    j["axles"]["rear"]["tire"]["peak_friction"] = 0.0;

    const LoadResult result = parse(j);

    REQUIRE(result.error.has_value());
    CHECK(result.error->field == "axles.rear.tire.peak_friction");
    CHECK(result.error->message == "must be greater than 0");
}

TEST_CASE("Parameters without physical sense are rejected with the field path") {
    struct Case {
        nlohmann::json::json_pointer field;
        double value;
        const char* path;
        const char* message;
    };
    using ptr = nlohmann::json::json_pointer;
    const Case cases[] = {
        {ptr("/inertia/y"), 0.0, "inertia.y", "must be greater than 0"},
        {ptr("/center_of_mass/z"), -0.1, "center_of_mass.z", "must be greater than 0"},
        {ptr("/center_of_mass/y"), 0.9, "center_of_mass.y",
         "must be between right wheel (-0.78) and left wheel (0.78)"},
        {ptr("/axles/front/track_width"), 0.0, "axles.front.track_width", "must be greater than 0"},
        {ptr("/axles/rear/wheel_radius"), -0.33, "axles.rear.wheel_radius", "must be greater than 0"},
        {ptr("/axles/front/mount_height"), 0.0, "axles.front.mount_height", "must be greater than 0"},
        {ptr("/axles/front/suspension/spring_rate"), 0.0, "axles.front.suspension.spring_rate",
         "must be greater than 0"},
        {ptr("/axles/rear/suspension/travel"), 0.0, "axles.rear.suspension.travel", "must be greater than 0"},
        {ptr("/axles/front/suspension/damper_bump"), -1.0, "axles.front.suspension.damper_bump",
         "must not be negative"},
        {ptr("/axles/rear/suspension/damper_rebound"), -1.0, "axles.rear.suspension.damper_rebound",
         "must not be negative"},
        {ptr("/axles/front/suspension/bump_stop_rate"), -1.0, "axles.front.suspension.bump_stop_rate",
         "must not be negative"},
        {ptr("/axles/front/tire/nominal_load"), 0.0, "axles.front.tire.nominal_load", "must be greater than 0"},
        {ptr("/axles/rear/tire/load_sensitivity"), -0.1, "axles.rear.tire.load_sensitivity",
         "must be between 0 and 1"},
        {ptr("/axles/front/tire/inertia"), 0.0, "axles.front.tire.inertia", "must be greater than 0"},
        {ptr("/axles/front/tire/longitudinal/stiffness"), 0.0, "axles.front.tire.longitudinal.stiffness",
         "must be greater than 0"},
        {ptr("/axles/rear/tire/longitudinal/shape"), 2.5, "axles.rear.tire.longitudinal.shape",
         "must be at most 2"},
        {ptr("/axles/front/tire/longitudinal/curvature"), 1.2, "axles.front.tire.longitudinal.curvature",
         "must be at most 1"},
        {ptr("/axles/front/suspension/anti_roll_bar"), -1.0, "axles.front.suspension.anti_roll_bar",
         "must not be negative"},
        {ptr("/axles/rear/tire/relaxation_length"), 0.0, "axles.rear.tire.relaxation_length",
         "must be greater than 0"},
        {ptr("/axles/front/tire/lateral/stiffness"), 0.0, "axles.front.tire.lateral.stiffness",
         "must be greater than 0"},
        {ptr("/axles/rear/tire/lateral/shape"), 2.5, "axles.rear.tire.lateral.shape", "must be at most 2"},
        {ptr("/axles/front/tire/lateral/curvature"), 1.2, "axles.front.tire.lateral.curvature",
         "must be at most 1"},
        {ptr("/axles/front/tire/combined/longitudinal"), -1.0, "axles.front.tire.combined.longitudinal",
         "must not be negative"},
        {ptr("/axles/rear/tire/combined/lateral"), -1.0, "axles.rear.tire.combined.lateral",
         "must not be negative"},
        {ptr("/steering/max_steer_angle_deg"), 0.0, "steering.max_steer_angle_deg", "must be greater than 0"},
        {ptr("/steering/max_steer_angle_deg"), 75.0, "steering.max_steer_angle_deg", "must be at most 60"},
        {ptr("/brakes/torque"), -1.0, "brakes.torque", "must not be negative"},
        {ptr("/brakes/balance"), 1.2, "brakes.balance", "must be between 0 and 1"},
        {ptr("/drivetrain/engine/torque_curve/0/rpm"), 0.0, "drivetrain.engine.torque_curve[0].rpm",
         "must be greater than 0"},
        {ptr("/drivetrain/engine/torque_curve/2/rpm"), 1500.0, "drivetrain.engine.torque_curve[2].rpm",
         "must be greater than at the previous point"},
        {ptr("/drivetrain/engine/torque_curve/3/torque"), -10.0, "drivetrain.engine.torque_curve[3].torque",
         "must not be negative"},
        {ptr("/drivetrain/engine/idle_rpm"), 0.0, "drivetrain.engine.idle_rpm", "must be greater than 0"},
        {ptr("/drivetrain/engine/rev_limit_rpm"), 800.0, "drivetrain.engine.rev_limit_rpm",
         "must be above the idle speed"},
        {ptr("/drivetrain/engine/rev_limit_rpm"), 9000.0, "drivetrain.engine.rev_limit_rpm",
         "must not be above the last point of the torque curve"},
        {ptr("/drivetrain/engine/inertia"), 0.0, "drivetrain.engine.inertia", "must be greater than 0"},
        {ptr("/drivetrain/engine/drag_torque"), -1.0, "drivetrain.engine.drag_torque", "must not be negative"},
        {ptr("/drivetrain/clutch/torque"), 0.0, "drivetrain.clutch.torque", "must be greater than 0"},
        {ptr("/drivetrain/gearbox/ratios/0"), 0.0, "drivetrain.gearbox.ratios[0]", "must be greater than 0"},
        {ptr("/drivetrain/gearbox/ratios/3"), 2.0, "drivetrain.gearbox.ratios[3]",
         "must be smaller than in the previous gear"},
        {ptr("/drivetrain/gearbox/final_drive"), 0.0, "drivetrain.gearbox.final_drive", "must be greater than 0"},
        {ptr("/drivetrain/gearbox/shift_time"), -0.1, "drivetrain.gearbox.shift_time", "must not be negative"},
    };

    for (const Case& c : cases) {
        const std::string path = c.path;
        CAPTURE(path);
        nlohmann::json j = sports_coupe();
        j[c.field] = c.value;

        const LoadResult result = parse(j);

        REQUIRE(result.error.has_value());
        CHECK(result.error->field == c.path);
        CHECK(result.error->message == c.message);
    }
}

TEST_CASE("An engine without a torque curve and a gearbox without gears are rejected") {
    for (const char* field : {"/drivetrain/engine/torque_curve", "/drivetrain/gearbox/ratios"}) {
        const std::string pointer = field;
        CAPTURE(pointer);
        nlohmann::json j = sports_coupe();
        j[nlohmann::json::json_pointer(pointer)] = nlohmann::json::array();

        const LoadResult result = parse(j);

        REQUIRE(result.error.has_value());
        CHECK(result.error->message == "must not be empty");
    }
}

TEST_CASE("The gearbox needs a positive ratio for the reverse gear") {
    const LoadResult coupe = parse(sports_coupe());
    REQUIRE_FALSE(coupe.error.has_value());
    CHECK(coupe.params.drivetrain.gearbox.reverse_ratio == doctest::Approx(3.4));

    nlohmann::json missing = sports_coupe();
    missing["drivetrain"]["gearbox"].erase("reverse_ratio");
    const LoadResult without = parse(missing);
    REQUIRE(without.error.has_value());
    CHECK(without.error->field == "drivetrain.gearbox.reverse_ratio");
    CHECK(without.error->message == "is missing");

    for (const double value : {0.0, -3.4}) {
        CAPTURE(value);
        nlohmann::json j = sports_coupe();
        j["drivetrain"]["gearbox"]["reverse_ratio"] = value;
        const LoadResult result = parse(j);
        REQUIRE(result.error.has_value());
        CHECK(result.error->field == "drivetrain.gearbox.reverse_ratio");
        CHECK(result.error->message == "must be greater than 0");
    }
}

TEST_CASE("A torque curve point that is not an object is reported with its path") {
    nlohmann::json j = sports_coupe();
    j["drivetrain"]["engine"]["torque_curve"][1] = 42.0;

    const LoadResult result = parse(j);

    REQUIRE(result.error.has_value());
    CHECK(result.error->field == "drivetrain.engine.torque_curve[1]");
    CHECK(result.error->message == "is not an object");
}

TEST_CASE("Unusable input yields an error instead of crashing") {
    const char* const inputs[] = {"", "   ", "42", "null", "{", "{\"mass\": \"\xff\"}", "{} {}"};

    for (const char* input : inputs) {
        const std::string text = input;
        CAPTURE(text);

        const LoadResult result = parse_vehicle_file(text);

        REQUIRE(result.error.has_value());
        CHECK_FALSE(result.error->message.empty());
    }
}

TEST_CASE("A vehicle file whose root is not an object is reported as such") {
    const LoadResult result = parse_vehicle_file("[1, 2]");

    REQUIRE(result.error.has_value());
    CHECK(result.error->field == "");
    CHECK(result.error->message == "is not an object");
}

TEST_CASE("The sports coupe carries aero and rolling resistance values") {
    const LoadResult result = load_vehicle_file(sports_coupe_path);

    REQUIRE_FALSE(result.error.has_value());
    const VehicleParams& car = result.params;
    CHECK(car.aero.drag_area == doctest::Approx(0.72));
    CHECK(car.front.downforce > 0.0);
    CHECK(car.rear.downforce > car.front.downforce);
    CHECK(car.rear.tire.rolling_resistance == doctest::Approx(0.012));
}

TEST_CASE("Missing or implausible aero and rolling resistance values are reported with their path") {
    nlohmann::json j = sports_coupe();
    std::string expected_field;

    SUBCASE("no aero object") {
        j.erase("aero");
        expected_field = "aero";
    }
    SUBCASE("negative downforce") {
        j["axles"]["front"]["downforce"] = -0.1;
        expected_field = "axles.front.downforce";
    }
    SUBCASE("drag area zero") {
        j["aero"]["drag_area"] = 0.0;
        expected_field = "aero.drag_area";
    }
    SUBCASE("absurd rolling resistance") {
        j["axles"]["rear"]["tire"]["rolling_resistance"] = 1.2;
        expected_field = "axles.rear.tire.rolling_resistance";
    }

    const LoadResult result = parse(j);
    REQUIRE(result.error.has_value());
    CHECK(result.error->field == expected_field);
}

TEST_CASE("The sports coupe has rear-wheel drive, a manual gearbox and a limited-slip differential") {
    const LoadResult result = load_vehicle_file(sports_coupe_path);

    REQUIRE_FALSE(result.error.has_value());
    const Drivetrain& drivetrain = result.params.drivetrain;
    CHECK(drivetrain.layout == DriveLayout::rear);
    CHECK(drivetrain.gearbox.mode == GearboxMode::manual);
    CHECK(drivetrain.differential.limited_slip);
    CHECK(drivetrain.differential.preload > 0.0);
}

namespace {

nlohmann::json automatic_block() {
    return {{"upshift_rpm", 6500.0},       {"downshift_rpm", 2500.0}, {"brake_downshift_rpm", 4000.0},
            {"hold_time", 0.8},            {"engage_range_rpm", 1500.0},
            {"engage_time", 0.15},         {"drift_angle_deg", 10.0}};
}

} // namespace

TEST_CASE("Front-wheel drive, all-wheel drive and the automatic load when the file describes them completely") {
    nlohmann::json j = sports_coupe();
    j["drivetrain"]["layout"] = "all_wheel";
    j["drivetrain"]["front_torque_split"] = 0.4;
    j["drivetrain"]["differential"] = {{"type", "open"}};
    j["drivetrain"]["gearbox"]["mode"] = "automatic";
    j["drivetrain"]["gearbox"]["automatic"] = automatic_block();

    const LoadResult result = parse(j);

    REQUIRE_FALSE(result.error.has_value());
    CHECK(result.params.drivetrain.layout == DriveLayout::all_wheel);
    CHECK(result.params.drivetrain.front_torque_split == doctest::Approx(0.4));
    CHECK_FALSE(result.params.drivetrain.differential.limited_slip);
    CHECK(result.params.drivetrain.gearbox.mode == GearboxMode::automatic);

    j["drivetrain"]["layout"] = "front";
    j["drivetrain"].erase("front_torque_split");
    CHECK_FALSE(parse(j).error.has_value());
}

TEST_CASE("Invalid combinations of drive layout, differential and gearbox mode are rejected understandably") {
    using nlohmann::json;
    struct Case {
        const char* description;
        std::function<void(json&)> change;
        const char* field;
        const char* message;
    };
    const Case cases[] = {
        {"unknown drive layout", [](json& j) { j["drivetrain"]["layout"] = "mid_engine"; }, "drivetrain.layout",
         "must be \"rear\", \"front\" or \"all_wheel\""},
        {"all-wheel drive without torque split", [](json& j) { j["drivetrain"]["layout"] = "all_wheel"; },
         "drivetrain.front_torque_split", "is missing"},
        {"all-wheel drive with torque split 1",
         [](json& j) {
             j["drivetrain"]["layout"] = "all_wheel";
             j["drivetrain"]["front_torque_split"] = 1.0;
         },
         "drivetrain.front_torque_split",
         "must be between no torque at the front (0) and no torque at the rear (1)"},
        {"torque split without all-wheel drive", [](json& j) { j["drivetrain"]["front_torque_split"] = 0.5; },
         "drivetrain.front_torque_split", "only exists with all-wheel drive"},
        {"unknown differential", [](json& j) { j["drivetrain"]["differential"]["type"] = "torsen"; },
         "drivetrain.differential.type", "must be \"open\" or \"limited_slip\""},
        {"limited slip without effect",
         [](json& j) {
             j["drivetrain"]["differential"]["preload"] = 0.0;
             j["drivetrain"]["differential"]["locking_factor"] = 0.0;
         },
         "drivetrain.differential.type",
         "a limited-slip differential needs preload or locking_factor, otherwise it is open"},
        {"open differential with preload",
         [](json& j) { j["drivetrain"]["differential"] = {{"type", "open"}, {"preload", 50.0}}; },
         "drivetrain.differential.preload", "only exists with a limited-slip differential"},
        {"unknown gearbox mode", [](json& j) { j["drivetrain"]["gearbox"]["mode"] = "dual_clutch"; },
         "drivetrain.gearbox.mode", "must be \"manual\" or \"automatic\""},
        {"automatic without shift points", [](json& j) { j["drivetrain"]["gearbox"]["mode"] = "automatic"; },
         "drivetrain.gearbox.automatic", "is missing"},
        {"shift points without automatic",
         [](json& j) { j["drivetrain"]["gearbox"]["automatic"] = automatic_block(); },
         "drivetrain.gearbox.automatic", "only exists in gearbox mode automatic"},
        {"automatic that would hunt",
         [](json& j) {
             j["drivetrain"]["gearbox"]["mode"] = "automatic";
             j["drivetrain"]["gearbox"]["automatic"] = automatic_block();
             j["drivetrain"]["gearbox"]["automatic"]["downshift_rpm"] = 5000.0;
             j["drivetrain"]["gearbox"]["automatic"]["brake_downshift_rpm"] = 5500.0;
         },
         "drivetrain.gearbox.automatic.downshift_rpm",
         "must be below the engine speed an upshift drops to (4468.75), otherwise the automatic hunts"},
    };

    for (const Case& c : cases) {
        CAPTURE(c.description);
        json j = sports_coupe();
        c.change(j);

        const LoadResult result = parse(j);

        REQUIRE(result.error.has_value());
        CHECK(result.error->field == c.field);
        CHECK(result.error->message == c.message);
    }
}

TEST_CASE("A vehicle file with the old German field names is rejected with the field path") {
    using nlohmann::json;
    // There is deliberately no support for the old names: every one of them counts as unknown,
    // and the English field it replaced is reported as missing.
    struct Case {
        const char* description;
        std::function<void(json&)> change;
        const char* field;
        const char* message;
    };
    const auto rename = [](json& parent, const char* from, const char* to) {
        parent[to] = parent[from];
        parent.erase(from);
    };
    const Case cases[] = {
        {"whole file of the German era", [](json& j) { j = json::parse(R"({"name": "Sportcoupé", "masse": 1350.0})"); },
         "mass", "is missing"},
        {"top level", [&](json& j) { rename(j, "mass", "masse"); }, "mass", "is missing"},
        {"nested", [&](json& j) { rename(j["axles"]["rear"]["suspension"], "travel", "federweg"); },
         "axles.rear.suspension.travel", "is missing"},
        {"German choice", [](json& j) { j["drivetrain"]["gearbox"]["mode"] = "manuell"; }, "drivetrain.gearbox.mode",
         "must be \"manual\" or \"automatic\""},
    };

    for (const Case& c : cases) {
        CAPTURE(c.description);
        json j = sports_coupe();
        c.change(j);

        const LoadResult result = parse(j);

        REQUIRE(result.error.has_value());
        CHECK(result.error->field == c.field);
        CHECK(result.error->message == c.message);
    }
}
