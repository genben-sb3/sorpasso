#include "ultragravity/vehicle_step.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "magic_formula.h"

namespace ultragravity {
namespace {

struct WheelMount {
    const Axle* axle;
    Vec3 mount_point; // in the vehicle frame, relative to the center of mass
};

std::array<WheelMount, wheel_count> wheel_mounts(const VehicleParams& k) {
    const auto mount_point = [&](const Axle& a, double side) {
        return Vec3{a.x, side * a.track_width / 2.0, a.mount_height} - k.center_of_mass;
    };
    return {{
        {&k.front, mount_point(k.front, 1.0)},
        {&k.front, mount_point(k.front, -1.0)},
        {&k.rear, mount_point(k.rear, 1.0)},
        {&k.rear, mount_point(k.rear, -1.0)},
    }};
}

// Density of the air from which drag and downforce draw their force.
constexpr double air_density = 1.2; // kg/m³

// Air: the drag acts against the direction of travel at the center of mass, the
// downforce per axle perpendicular to the body, downward at the axle position.
void air_forces(const VehicleParams& params, const BodyMotion& body, const Vec3& forward, Forces& forces) {
    const double dynamic_pressure_factor = 0.5 * air_density; // kg/m³
    forces.force +=
        body.velocity * (-dynamic_pressure_factor * params.aero.drag_area * length(body.velocity));
    const double airspeed = std::max(dot(body.velocity, forward), 0.0); // m/s
    for (const Axle* axle : {&params.front, &params.rear}) {
        const Vec3 downforce = rotate(body.orientation, {0.0, 0.0, -1.0}) *
                               (dynamic_pressure_factor * axle->downforce * airspeed * airspeed); // N
        const Vec3 lever = rotate(body.orientation, Vec3{axle->x, 0.0, 0.0} - params.center_of_mass);
        forces.force += downforce;
        forces.torque += cross(lever, downforce);
    }
}

bool front_wheel(int wheel) { return wheel < rear_left; }

// The other wheel of the same axle.
int opposite(int wheel) { return wheel ^ 1; }

// Force of spring, bump stop and damper on a wheel.
double spring_force(const Suspension& s, double compression, double compression_speed) {
    double force = s.spring_rate * compression; // N
    if (compression > s.travel) force += s.bump_stop_rate * (compression - s.travel);
    const double damping = compression_speed > 0.0 ? s.damper_bump : s.damper_rebound; // N·s/m
    force += damping * compression_speed;
    // The ground can only push, not pull.
    return std::max(force, 0.0);
}

// Load the anti-roll bar adds to the wheel and takes from its opposite:
// more to the one compressed further. It can only shift what the unloaded wheel
// carries; if it lifts off, the bar has nothing left to push against.
double anti_roll_bar_load(const Suspension& s, double compression, double compression_opposite, double load,
                          double load_opposite) {
    return std::clamp(s.anti_roll_bar * (compression - compression_opposite), -load, load_opposite);
}

// Substeps for wheels and tires per vehicle step (ADR-0006).
constexpr int substeps = 8;

// Rotation of the steering wheel at full lock, per side (450°).
constexpr double steering_wheel_lock = 2.5 * std::numbers::pi; // rad

// Below this speed the slip denominator blends smoothly into a fixed
// value, so the slip does not run toward infinity at standstill.
constexpr double slip_denominator_min = 1.0; // m/s

// Lower bound of the wheel speed for the relaxation. Without it the
// slip angle would freeze at standstill, because the tire only deforms while rolling.
constexpr double relaxation_min = 1.0; // m/s

double slip_denominator(double v) { return std::sqrt(v * v + slip_denominator_min * slip_denominator_min); }

// Slip ratio of a wheel with angular speed omega (rad/s) and wheel radius r (m) at the
// longitudinal speed v (m/s) at the contact point. Dimensionless.
double slip_ratio(double omega, double r, double v) { return (omega * r - v) / slip_denominator(v); }

// Weighting function for combined slip: 1 without the other slip,
// drops toward zero with it.
double weighting(double b, double slip) { return 1.0 / std::sqrt(1.0 + b * b * slip * slip); }

// Friction between tire and surface. More wheel load brings less than
// proportionally more force (load sensitivity).
double friction(const Tire& t, double wheel_load, double surface) {
    const double extra_load = (wheel_load - t.nominal_load) / t.nominal_load; // 1
    return std::max(surface * t.peak_friction * (1.0 - t.load_sensitivity * extra_load), 0.0);
}

// Below the idle speed the idle controller opens from zero to full throttle
// across this range.
constexpr double idle_control_range = 100.0 * 2.0 * std::numbers::pi / 60.0; // rad/s

// Torque at full throttle, linear between the points, outside the curve
// its end value.
double full_throttle_torque(const Engine& e, double speed) {
    const auto& curve = e.torque_curve;
    if (speed <= curve.front().speed) return curve.front().torque;
    for (std::size_t i = 1; i < curve.size(); ++i) {
        if (speed <= curve[i].speed) {
            const TorquePoint& a = curve[i - 1];
            const TorquePoint& b = curve[i];
            return a.torque + (b.torque - a.torque) * (speed - a.speed) / (b.speed - a.speed);
        }
    }
    return curve.back().torque;
}

// Engine torque at the crankshaft: the idle controller opens the throttle below
// the idle speed, the limiter closes it above the rev limit, and without
// throttle the engine drags (engine braking).
double engine_torque(const Engine& e, double speed, double throttle) {
    const double controller = std::clamp((e.idle_speed - speed) / idle_control_range, 0.0, 1.0); // 1
    const double effective = speed >= e.rev_limit ? 0.0 : std::max(throttle, controller);         // 1
    const double drag = e.drag_torque * std::max(speed, 0.0) / e.rev_limit;                      // N·m
    return effective * full_throttle_torque(e, speed) - (1.0 - effective) * drag;
}

// What stays the same for a wheel over all substeps of a step.
struct WheelSetup {
    const Axle* axle = nullptr;
    bool contact = false;
    Vec3 lever;                  // m, from the center of mass to the contact point, world frame
    Vec3 longitudinal;           // 1, rolling direction in the ground plane
    Vec3 lateral;                // 1, in the ground plane to the left, across the rolling direction
    Vec3 contact_velocity;       // m/s, velocity of the body at the contact point
    double wheel_load = 0.0;     // N
    double grip = 0.0;           // N, wheel load times friction
    double brake_torque = 0.0;   // N·m
};

// How far a wheel in contact is compressed and how fast it keeps compressing.
struct Compression {
    double compression = 0.0; // m
    double speed = 0.0;       // m/s, positive when compressing
};

Compression compression(const ProbeRay& ray, const WheelContact& contact, const BodyMotion& body) {
    const double distance = dot(contact.point - ray.start, ray.direction); // m
    // How fast the mount point approaches the ground along the ray.
    const Vec3 mount_lever = ray.start - body.position;
    const Vec3 mount_velocity = body.velocity + cross(body.angular_velocity, mount_lever);
    const double incline = dot(-ray.direction, contact.normal); // 1
    return {std::max(ray.length - distance, 0.0), -dot(mount_velocity, contact.normal) / incline};
}

// Rolling and lateral direction of a wheel in the ground plane: they follow
// the body, rotated by the steer angle.
void align(WheelSetup& wheel, Vec3 normal, Vec3 forward, double steer_angle) {
    const Vec3 lateral = cross(normal, forward);
    const Vec3 longitudinal = cross(lateral, normal) * (1.0 / length(lateral));
    const Vec3 left = cross(normal, longitudinal);
    wheel.longitudinal = longitudinal * std::cos(steer_angle) + left * std::sin(steer_angle);
    wheel.lateral = cross(normal, wheel.longitudinal);
}

// Forces of a tire in one substep.
struct TireForce {
    double longitudinal = 0.0; // N
    double lateral = 0.0;      // N
    double slope = 0.0;        // N·s/rad, derivative of the longitudinal force with respect to wheel speed
    double limit = 0.0;        // N, what the slip angle leaves of longitudinal force
    double slip = 0.0;         // 1, slip ratio
};

// Tire forces of a wheel in contact at velocity v at the contact point.
// The slip angle the tire feels follows the kinematic one over the
// relaxation length: dα/ds = (α_kin − α)/σ with s the distance the wheel covers
// over the ground, exact over the substep.
TireForce tire_force(const WheelSetup& wheel, Vec3 v, double omega, double h, double& slip_angle) {
    const Tire& tire = wheel.axle->tire;
    const double r = wheel.axle->wheel_radius;             // m
    const double vx = dot(v, wheel.longitudinal);          // m/s
    const double vy = dot(v, wheel.lateral);               // m/s
    const double denominator = slip_denominator(vx);       // m/s
    const double kinematic = std::atan(vy / denominator);  // rad
    const double distance = std::max(std::hypot(vx, vy), relaxation_min) * h; // m
    slip_angle += (kinematic - slip_angle) * (1.0 - std::exp(-distance / tire.relaxation_length));

    const double slip = slip_ratio(omega, r, vx); // 1
    const CurvePoint curve = magic_formula(tire.longitudinal, slip);
    TireForce k;
    k.slip = slip;
    k.limit = wheel.grip * weighting(tire.combined.longitudinal, slip_angle);
    k.longitudinal = k.limit * curve.value;
    k.slope = k.limit * std::max(curve.slope, 0.0) * r / denominator;
    k.lateral = -wheel.grip * weighting(tire.combined.lateral, slip) * magic_formula(tire.lateral, slip_angle).value;
    return k;
}

// A driven axle together with its share of the drive torque.
struct DrivenAxle {
    int left;
    int right;
    const Axle* axle;
    double share; // 1
};

struct DrivenAxles {
    std::array<DrivenAxle, 2> axles{};
    int count = 0;
};

DrivenAxles driven_axles(const VehicleParams& k) {
    const DrivenAxle front{front_left, front_right, &k.front, 1.0};
    const DrivenAxle rear{rear_left, rear_right, &k.rear, 1.0};
    const Drivetrain& d = k.drivetrain;
    switch (d.layout) {
    case DriveLayout::rear: return {{rear, {}}, 1};
    case DriveLayout::front: return {{front, {}}, 1};
    case DriveLayout::all_wheel: break;
    }
    DrivenAxle f = front;
    DrivenAxle r = rear;
    f.share = d.front_torque_split;
    r.share = 1.0 - d.front_torque_split;
    return {{f, r}, 2};
}

// Mean wheel radius of the driven axles, weighted by their share.
double drive_radius(const DrivenAxles& driven) {
    double radius = 0.0; // m
    for (int i = 0; i < driven.count; ++i) {
        radius += driven.axles[i].share * driven.axles[i].axle->wheel_radius;
    }
    return radius;
}

// Weighted mean wheel speed of the driven axles.
double input_shaft_speed(const DrivenAxles& driven, const std::array<double, wheel_count>& speed) {
    double sum = 0.0; // rad/s
    for (int i = 0; i < driven.count; ++i) {
        const DrivenAxle& a = driven.axles[i];
        sum += a.share * (speed[a.left] + speed[a.right]) / 2.0;
    }
    return sum;
}

// An impulse at the gearbox input (geared) is distributed to the wheels of the driven axles.
void distribute_impulse(const DrivenAxles& driven, double impulse, // N·m·s at the gearbox input
                        double ratio, std::array<double, wheel_count>& speed) {
    for (int i = 0; i < driven.count; ++i) {
        const DrivenAxle& a = driven.axles[i];
        const double dw = impulse * ratio * a.share / (2.0 * a.axle->tire.inertia); // rad/s
        speed[a.left] += dw;
        speed[a.right] += dw;
    }
}

// Limited impulse that equalizes the difference of two speeds: it is the
// full equalization, but at most +-limit.
double equalizing_impulse(double difference, // rad/s
                          double inertia,    // kg·m², effective inertia of the equalization
                          double limit) {    // N·m·s
    return std::clamp(difference * inertia, -limit, limit);
}

// Overall ratio from the engine to the wheels; negative in reverse, so the
// engine turns the wheels backward.
double overall_ratio(const Drivetrain& d, int gear) {
    if (gear > 0) return d.gearbox.ratios[gear - 1] * d.gearbox.final_drive;
    if (gear < 0) return -d.gearbox.reverse_ratio * d.gearbox.final_drive;
    return 0.0;
}

// Reverse only engages below this forward speed, never out of forward travel.
constexpr double reverse_engage_speed = 0.5; // m/s

// The gear the automatic picks next. It works with the engine speed that
// belongs to the speed over ground: spinning wheels do not push it into the
// next gear. In a drift it holds the gear.
int automatic_gear(const State& state, const VehicleParams& k, double brake, Vec3 vehicle_velocity,
                   double radius) {
    const Drivetrain& d = k.drivetrain;
    const Automatic& au = d.gearbox.automatic;
    const int gears = static_cast<int>(d.gearbox.ratios.size());
    // From neutral or reverse the automatic pulls away in first gear.
    if (state.gear <= 0) return 1;
    const int gear = state.gear;
    if (state.gear_age < au.hold_time) return gear;

    constexpr double brake_threshold = 0.05; // 1, brake input from which the automatic counts as braking
    const bool braking = brake >= brake_threshold;
    const double vx = vehicle_velocity.x; // m/s
    const double vy = vehicle_velocity.y; // m/s
    constexpr double drift_speed = 2.0;   // m/s, below this there is no drift angle
    if (std::hypot(vx, vy) > drift_speed && std::atan2(std::abs(vy), std::abs(vx)) > au.drift_angle) {
        return gear;
    }

    const auto ground_speed = [&](int g) { return std::abs(vx) / radius * overall_ratio(d, g); }; // rad/s
    if (gear < gears && !braking && ground_speed(gear) > au.upshift_speed) return gear + 1;
    const double down = braking ? au.brake_downshift_speed : au.downshift_speed; // rad/s
    if (gear > 1 && ground_speed(gear) < down && ground_speed(gear - 1) < au.upshift_speed) {
        return gear - 1;
    }
    return gear;
}

// Traction control: it takes throttle away from the driver while the slip of the
// driven wheels is above the target of the level, and only gives it back once it is
// clearly below. In between it holds the factor (hysteresis), so it does
// not switch on and off. Level 0 never intervenes.
constexpr double tc_target_level0 = 0.30;      // 1, target slip the levels subtract from
constexpr int    tc_max_level = 5;             // highest level, larger ones are clamped
constexpr double tc_target_min = 0.01;         // 1, lower bound of the target slip
constexpr double tc_target_per_level = 0.05;   // 1, less target slip per level
constexpr double tc_lower_threshold = 0.8;     // 1, fraction of the target below which throttle returns
constexpr double tc_cut_rate = 2.0;            // 1/s, throttle factor removed per second
constexpr double tc_restore_rate = 0.5;        // 1/s, throttle factor restored per second

double traction_factor(double factor, int level, const DrivenAxles& driven,
                       const std::array<double, wheel_count>& wheel_speed, double v_longitudinal, double dt) {
    if (level <= 0) return 1.0;
    const double target = std::max(tc_target_level0 - tc_target_per_level * std::min(level, tc_max_level), tc_target_min); // 1
    double slip = 0.0; // 1, largest drive slip
    for (int i = 0; i < driven.count; ++i) {
        const DrivenAxle& a = driven.axles[i];
        for (int wheel : {a.left, a.right}) {
            slip = std::max(slip, slip_ratio(wheel_speed[wheel], a.axle->wheel_radius, v_longitudinal));
        }
    }
    if (slip > target) return std::max(factor - tc_cut_rate * dt, 0.0);
    if (slip < tc_lower_threshold * target) return std::min(factor + tc_restore_rate * dt, 1.0);
    return factor;
}

} // namespace

State rolling_state(const VehicleParams& params, double speed) {
    State s;
    const auto mounts = wheel_mounts(params);
    for (int i = 0; i < wheel_count; ++i) s.wheel_speed[i] = speed / mounts[i].axle->wheel_radius;
    s.engine_speed = params.drivetrain.engine.idle_speed;
    return s;
}

std::array<ProbeRay, wheel_count> probe_rays(const VehicleParams& params, const BodyMotion& body) {
    const Vec3 down = rotate(body.orientation, {0.0, 0.0, -1.0});
    std::array<ProbeRay, wheel_count> rays;
    const auto mounts = wheel_mounts(params);
    for (int i = 0; i < wheel_count; ++i) {
        const WheelMount& m = mounts[i];
        rays[i] = {body.position + rotate(body.orientation, m.mount_point), down,
                   m.axle->suspension.travel + m.axle->wheel_radius};
    }
    return rays;
}

StepResult vehicle_step(const State& state, const VehicleParams& params,
                        const InputPacket& input, const WheelContacts& contacts,
                        const BodyMotion& body, double dt) {
    StepResult result;
    result.state = state;

    const auto rays = probe_rays(params, body);
    const auto mounts = wheel_mounts(params);
    const Vec3 forward = rotate(body.orientation, {1.0, 0.0, 0.0});
    const double front_steer_angle = std::clamp(input.steering, -1.0, 1.0) * params.steering.max_steer_angle; // rad

    // Springs and dampers first for all wheels, because the anti-roll bar needs both
    // wheels of an axle. Without contact a wheel hangs fully extended and unloaded.
    std::array<Compression, wheel_count> compressions{};
    std::array<double, wheel_count> spring_load{}; // N
    for (int i = 0; i < wheel_count; ++i) {
        if (!contacts[i].contact) continue;
        compressions[i] = compression(rays[i], contacts[i], body);
        spring_load[i] =
            spring_force(mounts[i].axle->suspension, compressions[i].compression, compressions[i].speed);
    }

    std::array<WheelSetup, wheel_count> wheels;
    for (int i = 0; i < wheel_count; ++i) {
        WheelSetup& wheel = wheels[i];
        wheel.axle = mounts[i].axle;
        const double axle_share = front_wheel(i) ? params.brakes.balance : 1.0 - params.brakes.balance; // 1
        wheel.brake_torque = input.brake * params.brakes.torque * axle_share / 2.0;

        const WheelContact& contact = contacts[i];
        if (!contact.contact) {
            result.state.slip_angle[i] = 0.0;
            continue;
        }

        // The wheel load acts along the ground normal at the contact point.
        const int o = opposite(i);
        const double wheel_load = // N
            spring_load[i] + anti_roll_bar_load(wheel.axle->suspension, compressions[i].compression,
                                                compressions[o].compression, spring_load[i], spring_load[o]);
        const Vec3 force = contact.normal * wheel_load;
        wheel.contact = true;
        wheel.lever = contact.point - body.position;
        result.forces.force += force;
        result.forces.torque += cross(wheel.lever, force);

        align(wheel, contact.normal, forward, front_wheel(i) ? front_steer_angle : 0.0);
        wheel.contact_velocity = body.velocity + cross(body.angular_velocity, wheel.lever);
        wheel.wheel_load = wheel_load;
        wheel.grip = wheel_load * friction(wheel.axle->tire, wheel_load, contact.friction);

        // Rolling resistance: proportional to the wheel load, against the rolling direction. The
        // smooth sign transition keeps it at zero at standstill.
        const double vx = dot(wheel.contact_velocity, wheel.longitudinal); // m/s
        const Vec3 rolling_force =
            wheel.longitudinal * (-wheel.axle->tire.rolling_resistance * wheel_load * vx / slip_denominator(vx));
        result.forces.force += rolling_force;
        result.forces.torque += cross(wheel.lever, rolling_force);

        result.animation.suspension_travel[i] = compressions[i].compression;
        result.telemetry.wheel_load[i] = wheel_load;
    }

    air_forces(params, body, forward, result.forces);

    // Wheels and tires in substeps. Wheel load, contact point and rotation of the
    // body stay fixed meanwhile. Deliberate approximation: only the velocity of the
    // body is carried along with the tire forces, so the stiff tire does not work
    // against an outdated velocity over the whole tick at a small slip denominator
    // (research/fahrphysik.md, 9.2).
    const Drivetrain& drivetrain = params.drivetrain;
    const DrivenAxles driven = driven_axles(params);
    const bool automatic = drivetrain.gearbox.mode == GearboxMode::automatic;
    const int gears = static_cast<int>(drivetrain.gearbox.ratios.size());
    result.state.gear_age += dt;
    const double v_longitudinal = dot(body.velocity, forward); // m/s
    int gear_request = std::clamp(static_cast<int>(input.gear_request), -1, gears);
    if (automatic && gear_request > 0) {
        // The automatic only knows reverse (-1), neutral (0) and drive (anything above);
        // it picks the forward gear itself and never shifts in reverse.
        const Vec3 vehicle_velocity{v_longitudinal, dot(body.velocity, rotate(body.orientation, {0.0, 1.0, 0.0})),
                                    0.0};
        gear_request = automatic_gear(state, params, input.brake, vehicle_velocity, drive_radius(driven));
    }
    // Out of forward travel the reverse gear does not engage; the car stays in neutral
    // until it has (nearly) stopped.
    if (gear_request < 0 && state.gear >= 0 && v_longitudinal > reverse_engage_speed) gear_request = 0;
    // Gear change: the requested gear only engages after the shift time, until then
    // there is no drive.
    if (gear_request != result.state.gear) {
        result.state.gear = gear_request;
        result.state.shift_remaining = drivetrain.gearbox.shift_time;
        result.state.gear_age = 0.0;
    }
    const double ratio = overall_ratio(drivetrain, result.state.gear); // 1
    double& engine_speed = result.state.engine_speed;
    result.state.traction_factor =
        traction_factor(state.traction_factor, input.driving_aids.traction_control, driven,
                        state.wheel_speed, v_longitudinal, dt);
    // The automatic operates the clutch itself: it engages with the engine speed above idle
    // and again within the engage time after a shift.
    const Automatic& au = drivetrain.gearbox.automatic;
    const double after_shift = // 1
        automatic ? std::clamp((result.state.gear_age - drivetrain.gearbox.shift_time) / std::max(au.engage_time, dt), 0.0, 1.0)
                  : 1.0;
    double wheel_inertia_inverse = 0.0; // 1/(kg·m²), wheels as seen by an impulse at the gearbox input
    for (int i = 0; i < driven.count; ++i) {
        const DrivenAxle& a = driven.axles[i];
        wheel_inertia_inverse += a.share * a.share / (2.0 * a.axle->tire.inertia);
    }

    const double h = dt / substeps;                // s
    const double substep_weight = 1.0 / substeps;  // 1
    Vec3 body_dv; // m/s
    for (int step = 0; step < substeps; ++step) {
        // During an automatic shift the automatic closes the throttle.
        double throttle = automatic && result.state.shift_remaining > 0.0 ? 0.0 : input.throttle; // 1
        throttle *= result.state.traction_factor;
        engine_speed += h * engine_torque(drivetrain.engine, engine_speed, throttle) / drivetrain.engine.inertia;
        engine_speed = std::max(engine_speed, 0.0);
        for (int i = 0; i < wheel_count; ++i) result.state.wheel_angle[i] += h * result.state.wheel_speed[i];

        Vec3 total_tire_force; // N
        std::array<double, wheel_count> free_speed{}; // rad/s after tire and drive, before the brake
        for (int i = 0; i < wheel_count; ++i) {
            const WheelSetup& wheel = wheels[i];
            const double inertia = wheel.axle->tire.inertia; // kg·m²
            const double r = wheel.axle->wheel_radius;       // m
            const double omega = result.state.wheel_speed[i]; // rad/s
            const TireForce k = wheel.contact ? tire_force(wheel, wheel.contact_velocity + body_dv, omega, h,
                                                           result.state.slip_angle[i])
                                              : TireForce{};

            // Semi-implicit: the tire force is linearized about the new wheel speed,
            // otherwise the wheel oscillates at a small slip denominator.
            free_speed[i] = omega - h * k.longitudinal * r / inertia / (1.0 + h * r * k.slope / inertia);
            const double fx = std::clamp(k.longitudinal + k.slope * (free_speed[i] - omega), -k.limit, k.limit); // N

            if (!wheel.contact) continue;
            const Vec3 force = wheel.longitudinal * fx + wheel.lateral * k.lateral; // N
            total_tire_force += force;
            result.telemetry.slip[i] += k.slip * substep_weight;
            result.telemetry.force_longitudinal[i] += fx * substep_weight;
            result.telemetry.force_lateral[i] += k.lateral * substep_weight;
            result.forces.force += force * substep_weight;
            result.forces.torque += cross(wheel.lever, force) * substep_weight;
        }
        body_dv += total_tire_force * (h / params.mass);

        // The clutch is a limited impulse that brings the engine and the gearbox input
        // to the same speed: if its torque suffices it sticks, otherwise it slips.
        result.state.shift_remaining = std::max(result.state.shift_remaining - h, 0.0);
        double drive_impulse = 0.0; // N·m·s at the gearbox input
        if (ratio != 0.0 && result.state.shift_remaining == 0.0) {
            const double clutch_engagement = // 1
                automatic ? std::clamp((engine_speed - drivetrain.engine.idle_speed) / au.engage_range, 0.0, 1.0) *
                                after_shift
                          : 1.0 - input.clutch;
            const double input_speed = input_shaft_speed(driven, free_speed) * ratio; // rad/s
            const double inverse_inertia = // 1/(kg·m²)
                1.0 / drivetrain.engine.inertia + ratio * ratio * wheel_inertia_inverse;
            const double limit = drivetrain.clutch.torque * clutch_engagement * h; // N·m·s
            drive_impulse = std::clamp((engine_speed - input_speed) / inverse_inertia, -limit, limit);
            engine_speed -= drive_impulse / drivetrain.engine.inertia;
            distribute_impulse(driven, drive_impulse, ratio, free_speed);
        }

        // The limited-slip differential transfers torque from the faster to the slower
        // wheel, at most up to the locking torque. Like the clutch it is a limited
        // impulse that equalizes the speeds.
        if (drivetrain.differential.limited_slip) {
            for (int i = 0; i < driven.count; ++i) {
                const DrivenAxle& a = driven.axles[i];
                const double axle_torque = std::abs(drive_impulse * ratio) / h * a.share; // N·m
                const double locking_torque = // N·m
                    drivetrain.differential.preload + drivetrain.differential.locking_factor * axle_torque;
                const double inertia = a.axle->tire.inertia;  // kg·m²
                const double limit = locking_torque * h;      // N·m·s
                const double difference = free_speed[a.left] - free_speed[a.right];          // rad/s
                const double impulse = equalizing_impulse(difference, inertia / 2.0, limit);  // N·m·s
                free_speed[a.left] -= impulse / inertia;
                free_speed[a.right] += impulse / inertia;
            }
        }

        // The brake is a limited impulse toward standstill: if it suffices,
        // the wheel stops, otherwise it removes its full amount.
        for (int i = 0; i < wheel_count; ++i) {
            const double brake_impulse = wheels[i].brake_torque * h / wheels[i].axle->tire.inertia; // rad/s
            const double unbraked = free_speed[i]; // rad/s
            result.state.wheel_speed[i] =
                std::abs(unbraked) <= brake_impulse ? 0.0 : unbraked - std::copysign(brake_impulse, unbraked);
        }
    }
    AnimationData& anim = result.animation;
    for (int i = 0; i < wheel_count; ++i) {
        result.state.wheel_angle[i] = wrap_angle(result.state.wheel_angle[i]);
        anim.wheel_angle[i] = result.state.wheel_angle[i];
        anim.steer_angle[i] = front_wheel(i) ? front_steer_angle : 0.0;
    }
    // From the orientation of the body: pitch from the incline of the longitudinal axis,
    // roll from the incline of the lateral axis.
    anim.pitch = std::asin(std::clamp(-forward.z, -1.0, 1.0));
    anim.roll = std::asin(std::clamp(rotate(body.orientation, {0.0, 1.0, 0.0}).z, -1.0, 1.0));
    anim.steering_wheel = std::clamp(input.steering, -1.0, 1.0) * steering_wheel_lock;
    anim.engine_speed = result.state.engine_speed;
    result.telemetry.slip_angle = result.state.slip_angle;
    result.telemetry.speed = dot(body.velocity, forward);
    return result;
}

} // namespace ultragravity
