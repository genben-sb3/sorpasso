#pragma once

#include <cstdint>
#include <vector>

#include "ultragravity/input.h"
#include "ultragravity/units.h"
#include "ultragravity/vehicle_file.h"
#include "ultragravity/vehicle_step.h"

namespace ultragravity {

// A rectangular area of the test ground with its own friction.
struct SurfaceZone {
    double x_from = 0.0, x_to = 0.0; // m
    double y_from = 0.0, y_to = 0.0; // m
    double friction = 0.0;           // 1
};

// The track of the test world: an unbounded plane at height zero. Outside the
// surface zones the general friction applies; if zones overlap, the first one counts.
struct TestGround {
    double friction = 1.0; // 1
    std::vector<SurfaceZone> surface_zones;

    WheelContact probe(const ProbeRay& ray) const;
};

// Stand-in for Unreal in the terminal: moves the body as a rigid body and
// calls the vehicle step the way the Unreal adapter will.
class TestWorld {
public:
    static constexpr double time_step = 1.0 / 120.0; // s
    static constexpr double gravity = ultragravity::gravity; // m/s²

    // Sets the car down level, with the wheels hanging drop_height above the ground.
    // With a speed it rolls straight ahead in the X direction.
    TestWorld(VehicleParams params, double drop_height, TestGround ground = {}, double speed = 0.0);

    void tick(const InputPacket& input);

    // A shove: changes the velocity and angular velocity of the body at once, as an impulse would.
    void push(Vec3 velocity_change, Vec3 angular_velocity_change = {}) {
        body_.velocity += velocity_change;
        body_.angular_velocity += angular_velocity_change;
    }

    const BodyMotion& body() const { return body_; }
    const StepResult& last_step() const { return last_step_; }
    std::uint64_t checksum() const;

private:
    VehicleParams params_;
    TestGround ground_;
    BodyMotion body_;
    State state_;
    StepResult last_step_;
};

} // namespace ultragravity
