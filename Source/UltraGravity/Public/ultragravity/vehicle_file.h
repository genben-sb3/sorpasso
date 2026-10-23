#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ultragravity/export.h"
#include "ultragravity/vector_math.h"

namespace ultragravity {

struct Suspension {
    double spring_rate = 0.0;    // N/m
    double damper_bump = 0.0;    // N·s/m in compression
    double damper_rebound = 0.0; // N·s/m in extension
    double travel = 0.0;         // m from the fully extended wheel to the bump stop
    double bump_stop_rate = 0.0; // N/m added beyond the bump stop
    double anti_roll_bar = 0.0;  // N/m, force per meter of compression difference between left and right
};

// Shape of the Magic Formula curve over a slip.
struct CurveShape {
    double stiffness = 0.0; // B
    double shape = 0.0;     // C
    double curvature = 0.0; // E
};

// Combined slip: each force is weighted with G = cos(atan(B·x)) over the
// respective other slip, so longitudinal and lateral force share the grip.
struct CombinedWeighting {
    double longitudinal = 0.0; // B, weakens the longitudinal force over the slip angle α in rad
    double lateral = 0.0;      // B, weakens the lateral force over the slip ratio κ
};

// Tire per the Magic Formula. The peak friction applies at nominal load on
// a surface with friction 1 and drops with rising wheel load.
struct Tire {
    double peak_friction = 0.0;      // D at nominal load
    double nominal_load = 0.0;       // N
    double load_sensitivity = 0.0;   // relative loss of friction per nominal load of extra load
    double inertia = 0.0;            // kg·m², wheel and tire about the spin axis
    double rolling_resistance = 0.0; // 1, rolling resistance force per newton of wheel load
    double relaxation_length = 0.0;  // m rolled until the lateral force reaches 63 %
    CurveShape longitudinal;         // over the slip ratio κ (dimensionless)
    CurveShape lateral;              // over the slip angle α in rad
    CombinedWeighting combined;
};

// An axle with two identical wheels left and right.
struct Axle {
    double x = 0.0;            // m, longitudinal position of the axle
    double track_width = 0.0;  // m
    double mount_height = 0.0; // m, height of the upper strut mount
    double wheel_radius = 0.0; // m
    double downforce = 0.0;    // m², lift coefficient times area (cL·A) of the axle, positive pushes down
    Suspension suspension;
    Tire tire;
};

struct Steering {
    double max_steer_angle = 0.0; // rad at both front wheels at full lock
};

struct Brakes {
    double torque = 0.0;  // N·m at all four wheels together with the brake fully applied
    double balance = 0.0; // front axle share, 0 … 1
};

// A point of the full-throttle torque curve. Linear interpolation in between.
struct TorquePoint {
    double speed = 0.0;  // rad/s
    double torque = 0.0; // N·m
};

struct Engine {
    std::vector<TorquePoint> torque_curve; // ascending by speed
    double idle_speed = 0.0;               // rad/s
    double rev_limit = 0.0;                // rad/s, above this the limiter cuts the throttle
    double inertia = 0.0;                  // kg·m², engine including flywheel
    double drag_torque = 0.0;              // N·m off throttle at the rev limit, linear with speed
};

struct Clutch {
    double torque = 0.0; // N·m it transmits at most when engaged
};

enum class GearboxMode { manual, automatic };

// Shift points of the automatic. It works with the engine speed that belongs
// to the speed over ground, not with the measured one: spinning wheels do not
// push the engine into the next gear.
struct Automatic {
    double upshift_speed = 0.0;         // rad/s, above this it shifts up
    double downshift_speed = 0.0;       // rad/s, below this it shifts down without braking
    double brake_downshift_speed = 0.0; // rad/s, below this it shifts down while braking
    double hold_time = 0.0;             // s a gear is held at least
    double engage_range = 0.0;          // rad/s above idle until the clutch fully engages
    double engage_time = 0.0;           // s in which the clutch re-engages after a shift
    double drift_angle = 0.0;           // rad from which it holds the gear
};

struct Gearbox {
    std::vector<double> ratios; // 1, descending from first gear
    double reverse_ratio = 0.0; // 1, magnitude; the reverse gear turns the wheels backward
    double final_drive = 0.0;   // 1
    double shift_time = 0.0;    // s without drive per gear change
    GearboxMode mode = GearboxMode::manual;
    Automatic automatic;        // only read in mode automatic
};

enum class DriveLayout { rear, front, all_wheel };

// Clutch-pack limited-slip differential: the locking keeps the wheels of an
// axle together until the locking torque is used up. It grows with the
// drive torque of the axle.
struct Differential {
    bool limited_slip = false;    // false: open differential
    double preload = 0.0;         // N·m of locking torque without load
    double locking_factor = 0.0;  // 1, extra locking torque per drive torque of the axle
};

struct Drivetrain {
    DriveLayout layout = DriveLayout::rear;
    Differential differential;          // at every driven axle
    double front_torque_split = 0.0;    // 1, front axle share of the drive torque, all-wheel drive only
    Engine engine;
    Clutch clutch;
    Gearbox gearbox;
};

struct Aero {
    double drag_area = 0.0; // m², drag coefficient times frontal area (cD·A)
};

// Desired results of the test maneuvers of the tuning tool. What the vehicle file does
// not name stays empty: there is no target for it.
struct TargetValues {
    std::optional<double> acceleration_0_100; // s
    std::optional<double> acceleration_0_200; // s
    std::optional<double> braking_100_0;      // m, without ABS
    std::optional<double> top_speed;          // m/s
};

// All parameters of a car. All positions are in the same vehicle frame
// (ISO 8855); its origin is free, usually the center of the front axle at
// ground level in design position.
struct VehicleParams {
    std::string name;
    double mass = 0.0;    // kg
    Vec3 inertia;         // kg·m², principal moments of inertia about the center of mass
    Vec3 center_of_mass;  // m
    Axle front;
    Axle rear;
    Aero aero;
    Steering steering;
    Brakes brakes;
    Drivetrain drivetrain;
    TargetValues target_values; // optional, only read by the tuning tool
};

struct LoadError {
    std::string field;   // path of the affected field, e.g. "axles.front.track_width"
    std::string message;
};

struct LoadResult {
    VehicleParams params;
    std::optional<LoadError> error;
};

// Loads a vehicle file from its JSON text, for an adapter that reads the file
// itself (the Unreal adapter uses Unreal's file access). Same checks, field
// paths and messages as load_vehicle_file. Invalid JSON yields an error with
// line and column. Throws no exceptions.
UG_API LoadResult parse_vehicle_file(std::string_view json);

// Loads a vehicle file from disk; reads the file and hands the text to
// parse_vehicle_file. Throws no exceptions.
UG_API LoadResult load_vehicle_file(const std::string& path);

} // namespace ultragravity
