#include "ultragravity/vehicle_file.h"

#include <algorithm>
#include <fstream>
#include <locale>
#include <numbers>
#include <sstream>

#include "nlohmann/json.hpp"

namespace ultragravity {
namespace {

using nlohmann::json;

std::string child(const std::string& path, const std::string& name) {
    return path.empty() ? name : path + "." + name;
}

// Reads fields with their path. After the first error it only returns
// zero values, so the loading code can read straight through without exceptions.
class Reader {
public:
    std::optional<LoadError> error;

    const json& object(const json& parent, const std::string& path, const std::string& name) {
        const json* field = find(parent, path, name);
        if (field && !field->is_object()) {
            report(child(path, name), "is not an object");
            return empty_;
        }
        return field ? *field : empty_;
    }

    double number(const json& parent, const std::string& path, const std::string& name) {
        const json* field = find(parent, path, name);
        if (field && !field->is_number()) {
            report(child(path, name), "is not a number");
            return 0.0;
        }
        return field ? field->get<double>() : 0.0;
    }

    std::string text(const json& parent, const std::string& path, const std::string& name) {
        const json* field = find(parent, path, name);
        if (field && !field->is_string()) {
            report(child(path, name), "is not a string");
            return {};
        }
        return field ? field->get<std::string>() : std::string{};
    }

    // An array with its path. Its elements are called path[0], path[1], …
    const json& array(const json& parent, const std::string& path, const std::string& name) {
        const json* field = find(parent, path, name);
        if (field && !field->is_array()) {
            report(child(path, name), "is not an array");
            return empty_array_;
        }
        return field ? *field : empty_array_;
    }

    const json& object(const json& element, const std::string& path) {
        if (error) return empty_;
        if (!element.is_object()) {
            report(path, "is not an object");
            return empty_;
        }
        return element;
    }

    double number(const json& element, const std::string& path) {
        if (error) return 0.0;
        if (!element.is_number()) {
            report(path, "is not a number");
            return 0.0;
        }
        return element.get<double>();
    }

    Vec3 vector(const json& parent, const std::string& path, const std::string& name) {
        const json& o = object(parent, path, name);
        const std::string p = child(path, name);
        return {number(o, p, "x"), number(o, p, "y"), number(o, p, "z")};
    }

    void report(std::string field, std::string message) {
        if (!error) error = LoadError{std::move(field), std::move(message)};
    }

private:
    const json* find(const json& parent, const std::string& path, const std::string& name) {
        if (error) return nullptr;
        const auto it = parent.find(name);
        if (it == parent.end()) {
            report(child(path, name), "is missing");
            return nullptr;
        }
        return &*it;
    }

    const json empty_ = json::object();
    const json empty_array_ = json::array();
};

Suspension read_suspension(Reader& reader, const json& o, const std::string& path) {
    Suspension s;
    s.spring_rate = reader.number(o, path, "spring_rate");
    s.damper_bump = reader.number(o, path, "damper_bump");
    s.damper_rebound = reader.number(o, path, "damper_rebound");
    s.travel = reader.number(o, path, "travel");
    s.bump_stop_rate = reader.number(o, path, "bump_stop_rate");
    s.anti_roll_bar = reader.number(o, path, "anti_roll_bar");
    return s;
}

CurveShape read_curve_shape(Reader& reader, const json& o, const std::string& path) {
    CurveShape k;
    k.stiffness = reader.number(o, path, "stiffness");
    k.shape = reader.number(o, path, "shape");
    k.curvature = reader.number(o, path, "curvature");
    return k;
}

Tire read_tire(Reader& reader, const json& o, const std::string& path) {
    Tire t;
    t.peak_friction = reader.number(o, path, "peak_friction");
    t.nominal_load = reader.number(o, path, "nominal_load");
    t.load_sensitivity = reader.number(o, path, "load_sensitivity");
    t.inertia = reader.number(o, path, "inertia");
    t.rolling_resistance = reader.number(o, path, "rolling_resistance");
    t.relaxation_length = reader.number(o, path, "relaxation_length");
    t.longitudinal =
        read_curve_shape(reader, reader.object(o, path, "longitudinal"), child(path, "longitudinal"));
    t.lateral = read_curve_shape(reader, reader.object(o, path, "lateral"), child(path, "lateral"));
    const std::string combined = child(path, "combined");
    const json& g = reader.object(o, path, "combined");
    t.combined.longitudinal = reader.number(g, combined, "longitudinal");
    t.combined.lateral = reader.number(g, combined, "lateral");
    return t;
}

Axle read_axle(Reader& reader, const json& axles, const std::string& name) {
    const std::string path = child("axles", name);
    const json& o = reader.object(axles, "axles", name);
    Axle a;
    a.x = reader.number(o, path, "x");
    a.track_width = reader.number(o, path, "track_width");
    a.mount_height = reader.number(o, path, "mount_height");
    a.wheel_radius = reader.number(o, path, "wheel_radius");
    a.downforce = reader.number(o, path, "downforce");
    a.suspension = read_suspension(reader, reader.object(o, path, "suspension"), child(path, "suspension"));
    a.tire = read_tire(reader, reader.object(o, path, "tire"), child(path, "tire"));
    return a;
}

std::string element(const std::string& path, std::size_t index) { return path + "[" + std::to_string(index) + "]"; }

constexpr double rpm = 2.0 * std::numbers::pi / 60.0; // rad/s per rpm
constexpr double deg = std::numbers::pi / 180.0;      // rad per degree

Drivetrain read_drivetrain(Reader& reader, const json& root) {
    Drivetrain d;
    const json& o = reader.object(root, "", "drivetrain");

    const json& e = reader.object(o, "drivetrain", "engine");
    const std::string engine = "drivetrain.engine";
    const json& curve = reader.array(e, engine, "torque_curve");
    for (std::size_t i = 0; i < curve.size(); ++i) {
        const std::string path = element(child(engine, "torque_curve"), i);
        const json& point = reader.object(curve[i], path);
        d.engine.torque_curve.push_back({reader.number(point, path, "rpm") * rpm, reader.number(point, path, "torque")});
    }
    d.engine.idle_speed = reader.number(e, engine, "idle_rpm") * rpm;
    d.engine.rev_limit = reader.number(e, engine, "rev_limit_rpm") * rpm;
    d.engine.inertia = reader.number(e, engine, "inertia");
    d.engine.drag_torque = reader.number(e, engine, "drag_torque");

    d.clutch.torque = reader.number(reader.object(o, "drivetrain", "clutch"), "drivetrain.clutch", "torque");

    const json& g = reader.object(o, "drivetrain", "gearbox");
    const std::string gearbox = "drivetrain.gearbox";
    const json& gears = reader.array(g, gearbox, "ratios");
    for (std::size_t i = 0; i < gears.size(); ++i) {
        d.gearbox.ratios.push_back(reader.number(gears[i], element(child(gearbox, "ratios"), i)));
    }
    d.gearbox.final_drive = reader.number(g, gearbox, "final_drive");
    d.gearbox.shift_time = reader.number(g, gearbox, "shift_time");

    // Optional blocks are only read when the choice asks for them; if one is in
    // the file without that choice, the plausibility check catches it.
    const std::string mode = reader.text(g, gearbox, "mode");
    if (mode == "manual") {
        d.gearbox.mode = GearboxMode::manual;
    } else if (mode == "automatic") {
        d.gearbox.mode = GearboxMode::automatic;
        const std::string path = child(gearbox, "automatic");
        const json& au = reader.object(g, gearbox, "automatic");
        Automatic& m = d.gearbox.automatic;
        m.upshift_speed = reader.number(au, path, "upshift_rpm") * rpm;
        m.downshift_speed = reader.number(au, path, "downshift_rpm") * rpm;
        m.brake_downshift_speed = reader.number(au, path, "brake_downshift_rpm") * rpm;
        m.hold_time = reader.number(au, path, "hold_time");
        m.engage_range = reader.number(au, path, "engage_range_rpm") * rpm;
        m.engage_time = reader.number(au, path, "engage_time");
        m.drift_angle = reader.number(au, path, "drift_angle_deg") * deg;
    } else if (!reader.error) {
        reader.report(child(gearbox, "mode"), "must be \"manual\" or \"automatic\"");
    }

    const std::string layout = reader.text(o, "drivetrain", "layout");
    if (layout == "rear") {
        d.layout = DriveLayout::rear;
    } else if (layout == "front") {
        d.layout = DriveLayout::front;
    } else if (layout == "all_wheel") {
        d.layout = DriveLayout::all_wheel;
        d.front_torque_split = reader.number(o, "drivetrain", "front_torque_split");
    } else if (!reader.error) {
        reader.report("drivetrain.layout", "must be \"rear\", \"front\" or \"all_wheel\"");
    }

    const std::string differential = "drivetrain.differential";
    const json& diff = reader.object(o, "drivetrain", "differential");
    const std::string type = reader.text(diff, differential, "type");
    if (type == "open") {
        d.differential.limited_slip = false;
    } else if (type == "limited_slip") {
        d.differential.limited_slip = true;
        d.differential.preload = reader.number(diff, differential, "preload");
        d.differential.locking_factor = reader.number(diff, differential, "locking_factor");
    } else if (!reader.error) {
        reader.report(child(differential, "type"), "must be \"open\" or \"limited_slip\"");
    }
    return d;
}

std::string number_as_text(double value) {
    std::ostringstream s;
    s.imbue(std::locale::classic()); // decimal point, whatever the host has set
    s << value;
    return s.str();
}

// A named limit, so the message says what the value fails against.
struct Limit {
    double value; // unit of the checked field
    std::string name;
};

// Rejects values that do not make a drivable car. Keeps the first violation.
class Plausibility {
public:
    std::optional<LoadError> error;

    void positive(double value, const std::string& field) {
        if (!(value > 0.0)) report(field, "must be greater than 0");
    }

    void non_negative(double value, const std::string& field) {
        if (!(value >= 0.0)) report(field, "must not be negative");
    }

    void at_most(double value, double limit, const std::string& field) {
        if (!(value <= limit)) report(field, "must be at most " + number_as_text(limit));
    }

    // Like between, but with inclusive limits that have no names of their own.
    void range(double value, double low, double high, const std::string& field) {
        if (!(value >= low && value <= high)) {
            report(field, "must be between " + number_as_text(low) + " and " + number_as_text(high));
        }
    }

    void between(double value, const Limit& low, const Limit& high, const std::string& field) {
        if (!(value > low.value && value < high.value)) {
            report(field, "must be between " + low.name + " (" + number_as_text(low.value) + ") and " + high.name +
                              " (" + number_as_text(high.value) + ")");
        }
    }

    void not_empty(std::size_t count, const std::string& field) {
        if (count == 0) report(field, "must not be empty");
    }

    void report(const std::string& field, std::string message) {
        if (!error) error = LoadError{field, std::move(message)};
    }
};

// With C above 2 or E above 1 the curve loses its shape: it tips into the
// negative behind the peak or has several peaks.
void check_curve_shape(Plausibility& p, const CurveShape& k, const std::string& path) {
    p.positive(k.stiffness, child(path, "stiffness"));
    p.positive(k.shape, child(path, "shape"));
    p.at_most(k.shape, 2.0, child(path, "shape"));
    p.at_most(k.curvature, 1.0, child(path, "curvature"));
}

void check_tire(Plausibility& p, const Tire& t, const std::string& path) {
    p.positive(t.peak_friction, child(path, "peak_friction"));
    p.positive(t.nominal_load, child(path, "nominal_load"));
    p.range(t.load_sensitivity, 0.0, 1.0, child(path, "load_sensitivity"));
    p.positive(t.inertia, child(path, "inertia"));
    // Real tires roll at 0.005 to 0.03; anything above is a typo.
    p.range(t.rolling_resistance, 0.0, 0.1, child(path, "rolling_resistance"));
    p.positive(t.relaxation_length, child(path, "relaxation_length"));
    check_curve_shape(p, t.longitudinal, child(path, "longitudinal"));
    check_curve_shape(p, t.lateral, child(path, "lateral"));
    p.non_negative(t.combined.longitudinal, child(path, "combined.longitudinal"));
    p.non_negative(t.combined.lateral, child(path, "combined.lateral"));
}

void check_axle(Plausibility& p, const Axle& a, const std::string& path) {
    p.positive(a.track_width, child(path, "track_width"));
    p.positive(a.mount_height, child(path, "mount_height"));
    p.positive(a.wheel_radius, child(path, "wheel_radius"));
    p.non_negative(a.downforce, child(path, "downforce"));
    const std::string s = child(path, "suspension");
    p.positive(a.suspension.spring_rate, child(s, "spring_rate"));
    p.non_negative(a.suspension.damper_bump, child(s, "damper_bump"));
    p.non_negative(a.suspension.damper_rebound, child(s, "damper_rebound"));
    p.positive(a.suspension.travel, child(s, "travel"));
    p.non_negative(a.suspension.bump_stop_rate, child(s, "bump_stop_rate"));
    p.non_negative(a.suspension.anti_roll_bar, child(s, "anti_roll_bar"));
    check_tire(p, a.tire, child(path, "tire"));
}

void check_engine(Plausibility& p, const Engine& e, const std::string& path) {
    const std::string curve = child(path, "torque_curve");
    p.not_empty(e.torque_curve.size(), curve);
    for (std::size_t i = 0; i < e.torque_curve.size(); ++i) {
        const std::string point = element(curve, i);
        if (i == 0) {
            p.positive(e.torque_curve[i].speed, child(point, "rpm"));
        } else if (!(e.torque_curve[i].speed > e.torque_curve[i - 1].speed)) {
            p.report(child(point, "rpm"), "must be greater than at the previous point");
        }
        p.non_negative(e.torque_curve[i].torque, child(point, "torque"));
    }
    p.positive(e.idle_speed, child(path, "idle_rpm"));
    if (!(e.rev_limit > e.idle_speed)) {
        p.report(child(path, "rev_limit_rpm"), "must be above the idle speed");
    }
    if (!e.torque_curve.empty() && !(e.rev_limit <= e.torque_curve.back().speed)) {
        p.report(child(path, "rev_limit_rpm"), "must not be above the last point of the torque curve");
    }
    p.positive(e.inertia, child(path, "inertia"));
    p.non_negative(e.drag_torque, child(path, "drag_torque"));
}

void check_gearbox(Plausibility& p, const Gearbox& g, const std::string& path) {
    const std::string gears = child(path, "ratios");
    p.not_empty(g.ratios.size(), gears);
    for (std::size_t i = 0; i < g.ratios.size(); ++i) {
        p.positive(g.ratios[i], element(gears, i));
        if (i > 0 && !(g.ratios[i] < g.ratios[i - 1])) {
            p.report(element(gears, i), "must be smaller than in the previous gear");
        }
    }
    p.positive(g.final_drive, child(path, "final_drive"));
    p.non_negative(g.shift_time, child(path, "shift_time"));
}

// Consecutive gears: smallest ratio of a gear's ratio to that of its
// predecessor, i.e. the largest drop in engine speed on an upshift.
double largest_step(const Gearbox& g) {
    double step = 1.0; // 1
    for (std::size_t i = 1; i < g.ratios.size(); ++i) {
        step = std::min(step, g.ratios[i] / g.ratios[i - 1]);
    }
    return step;
}

void check_automatic(Plausibility& p, const Drivetrain& d, const std::string& path) {
    const Automatic& m = d.gearbox.automatic;
    const Engine& engine = d.engine;
    p.between(m.upshift_speed / rpm, {engine.idle_speed / rpm, "idle speed"}, {engine.rev_limit / rpm, "rev limit"},
              child(path, "upshift_rpm"));
    p.positive(m.downshift_speed, child(path, "downshift_rpm"));
    // After an upshift the engine speed must stay above the downshift threshold,
    // otherwise the automatic shifts straight back and hunts.
    const double speed_after_upshift = m.upshift_speed * largest_step(d.gearbox); // rad/s
    if (!(m.downshift_speed < speed_after_upshift)) {
        p.report(child(path, "downshift_rpm"),
                 "must be below the engine speed an upshift drops to (" +
                     number_as_text(speed_after_upshift / rpm) + "), otherwise the automatic hunts");
    }
    if (!(m.brake_downshift_speed >= m.downshift_speed && m.brake_downshift_speed < m.upshift_speed)) {
        p.report(child(path, "brake_downshift_rpm"), "must be between downshift_rpm and upshift_rpm");
    }
    p.positive(m.hold_time, child(path, "hold_time"));
    p.positive(m.engage_range, child(path, "engage_range_rpm"));
    p.non_negative(m.engage_time, child(path, "engage_time"));
    p.positive(m.drift_angle, child(path, "drift_angle_deg"));
    p.at_most(m.drift_angle / deg, 90.0, child(path, "drift_angle_deg"));
}

void check_drive_layout(Plausibility& p, const Drivetrain& d) {
    if (d.layout == DriveLayout::all_wheel) {
        p.between(d.front_torque_split, {0.0, "no torque at the front"}, {1.0, "no torque at the rear"},
                  "drivetrain.front_torque_split");
    }
    if (d.differential.limited_slip) {
        p.non_negative(d.differential.preload, "drivetrain.differential.preload");
        p.non_negative(d.differential.locking_factor, "drivetrain.differential.locking_factor");
        if (!(d.differential.preload > 0.0 || d.differential.locking_factor > 0.0)) {
            p.report("drivetrain.differential.type",
                     "a limited-slip differential needs preload or locking_factor, otherwise it is open");
        }
    }
    if (d.gearbox.mode == GearboxMode::automatic) check_automatic(p, d, "drivetrain.gearbox.automatic");
}

std::optional<LoadError> check(const VehicleParams& k) {
    Plausibility p;
    p.positive(k.mass, "mass");
    p.positive(k.inertia.x, "inertia.x");
    p.positive(k.inertia.y, "inertia.y");
    p.positive(k.inertia.z, "inertia.z");
    check_axle(p, k.front, "axles.front");
    check_axle(p, k.rear, "axles.rear");
    if (!(k.front.x > k.rear.x)) {
        p.report("axles.front.x", "must be ahead of the rear axle (" + number_as_text(k.rear.x) + ")");
    }
    p.between(k.center_of_mass.x, {k.rear.x, "rear axle"}, {k.front.x, "front axle"}, "center_of_mass.x");
    // ISO 8855: y points to the left. The narrower track decides.
    const double half_track = std::min(k.front.track_width, k.rear.track_width) / 2.0; // m
    p.between(k.center_of_mass.y, {-half_track, "right wheel"}, {half_track, "left wheel"}, "center_of_mass.y");
    p.positive(k.center_of_mass.z, "center_of_mass.z");
    p.positive(k.aero.drag_area, "aero.drag_area");
    // Up to 60° the lock stays far away from turning the wheel sideways.
    p.positive(k.steering.max_steer_angle, "steering.max_steer_angle_deg");
    p.at_most(k.steering.max_steer_angle / deg, 60.0, "steering.max_steer_angle_deg");
    p.non_negative(k.brakes.torque, "brakes.torque");
    p.range(k.brakes.balance, 0.0, 1.0, "brakes.balance");
    check_engine(p, k.drivetrain.engine, "drivetrain.engine");
    p.positive(k.drivetrain.clutch.torque, "drivetrain.clutch.torque");
    check_gearbox(p, k.drivetrain.gearbox, "drivetrain.gearbox");
    check_drive_layout(p, k.drivetrain);
    return p.error;
}

// Blocks that do not fit the chosen drive layout, differential or gearbox mode
// are rejected instead of silently ignored.
std::optional<LoadError> foreign_blocks(const json& drivetrain) {
    const auto text = [](const json& o, const char* name) { return o.at(name).get<std::string>(); };
    if (drivetrain.contains("front_torque_split") && text(drivetrain, "layout") != "all_wheel") {
        return LoadError{"drivetrain.front_torque_split", "only exists with all-wheel drive"};
    }
    const json& differential = drivetrain.at("differential");
    for (const char* name : {"preload", "locking_factor"}) {
        if (differential.contains(name) && text(differential, "type") != "limited_slip") {
            return LoadError{child("drivetrain.differential", name), "only exists with a limited-slip differential"};
        }
    }
    const json& gearbox = drivetrain.at("gearbox");
    if (gearbox.contains("automatic") && text(gearbox, "mode") != "automatic") {
        return LoadError{"drivetrain.gearbox.automatic", "only exists in gearbox mode automatic"};
    }
    return std::nullopt;
}

// Only checks the JSON syntax and remembers where it breaks. In no-exception
// mode the DOM parser only says that something is broken, not where.
struct SyntaxChecker {
    std::size_t consumed = 0; // bytes up to and including the position where the error showed

    bool null() { return true; }
    bool boolean(bool) { return true; }
    bool number_integer(json::number_integer_t) { return true; }
    bool number_unsigned(json::number_unsigned_t) { return true; }
    bool number_float(json::number_float_t, const json::string_t&) { return true; }
    bool string(json::string_t&) { return true; }
    bool binary(json::binary_t&) { return true; }
    bool start_object(std::size_t) { return true; }
    bool key(json::string_t&) { return true; }
    bool end_object() { return true; }
    bool start_array(std::size_t) { return true; }
    bool end_array() { return true; }
    bool parse_error(std::size_t position, const std::string&, const nlohmann::detail::exception&) {
        consumed = position;
        return false;
    }
};

// Line and column (from 1) of the last byte read. Columns count UTF-8
// characters, so they match what an editor shows.
LoadError syntax_error(std::string_view text, std::size_t consumed) {
    const std::size_t position = std::min(text.size(), consumed > 0 ? consumed - 1 : 0);
    std::size_t line = 1;
    std::size_t column = 1;
    for (std::size_t i = 0; i < position; ++i) {
        const auto byte = static_cast<unsigned char>(text[i]);
        if (byte == '\n') {
            ++line;
            column = 1;
        } else if ((byte & 0xC0) != 0x80) { // continuation bytes belong to the character before
            ++column;
        }
    }
    return {"", "invalid JSON at line " + std::to_string(line) + ", column " + std::to_string(column)};
}

} // namespace

LoadResult parse_vehicle_file(std::string_view text) {
    SyntaxChecker checker;
    if (!json::sax_parse(text, &checker)) return {{}, syntax_error(text, checker.consumed)};
    const json root = json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (!root.is_object()) return {{}, LoadError{"", "is not an object"}};

    Reader reader;
    VehicleParams k;
    k.name = reader.text(root, "", "name");
    k.mass = reader.number(root, "", "mass");
    k.inertia = reader.vector(root, "", "inertia");
    k.center_of_mass = reader.vector(root, "", "center_of_mass");
    const json& axles = reader.object(root, "", "axles");
    k.front = read_axle(reader, axles, "front");
    k.rear = read_axle(reader, axles, "rear");
    k.aero.drag_area = reader.number(reader.object(root, "", "aero"), "aero", "drag_area");
    k.steering.max_steer_angle =
        reader.number(reader.object(root, "", "steering"), "steering", "max_steer_angle_deg") * deg;
    const json& brakes = reader.object(root, "", "brakes");
    k.brakes.torque = reader.number(brakes, "brakes", "torque");
    k.brakes.balance = reader.number(brakes, "brakes", "balance");
    k.drivetrain = read_drivetrain(reader, root);

    if (reader.error) return {{}, reader.error};
    if (auto violation = foreign_blocks(root.at("drivetrain"))) return {{}, std::move(violation)};
    if (auto violation = check(k)) return {{}, std::move(violation)};
    return {std::move(k), std::nullopt};
}

LoadResult load_vehicle_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return {{}, LoadError{"", "cannot open file: " + path}};
    std::ostringstream content;
    content << file.rdbuf();
    return parse_vehicle_file(content.str());
}

} // namespace ultragravity
