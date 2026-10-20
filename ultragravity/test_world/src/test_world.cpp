#include "ultragravity/test_world.h"

#include <algorithm>
#include <limits>

#include "ultragravity/checksum.h"

namespace ultragravity {

WheelContact TestGround::probe(const ProbeRay& ray) const {
    if (ray.direction.z >= 0.0) return {};
    // If the mount point is already below the ground, the ray counts from its start.
    const double distance = std::max(ray.start.z / -ray.direction.z, 0.0); // m
    if (distance > ray.length) return {};
    const Vec3 point = ray.start + ray.direction * distance;
    double friction_here = friction; // 1
    for (const SurfaceZone& zone : surface_zones) {
        if (point.x >= zone.x_from && point.x < zone.x_to && point.y >= zone.y_from && point.y < zone.y_to) {
            friction_here = zone.friction;
            break;
        }
    }
    return {true, point, {0.0, 0.0, 1.0}, friction_here};
}

TestWorld::TestWorld(VehicleParams params, double drop_height, TestGround ground, double speed)
    : params_(std::move(params)), ground_(std::move(ground)) {
    double lowest_point = std::numeric_limits<double>::infinity(); // m
    for (const ProbeRay& r : probe_rays(params_, body_)) {
        lowest_point = std::min(lowest_point, (r.start + r.direction * r.length).z);
    }
    body_.position.z = drop_height - lowest_point;
    body_.velocity.x = speed;
    state_ = rolling_state(params_, speed);
}

void TestWorld::tick(const InputPacket& input) {
    const auto rays = probe_rays(params_, body_);
    WheelContacts contacts;
    for (int i = 0; i < wheel_count; ++i) contacts[i] = ground_.probe(rays[i]);

    last_step_ = vehicle_step(state_, params_, input, contacts, body_, time_step);
    state_ = last_step_.state;

    // Rigid body, semi-implicit Euler method.
    const double dt = time_step;   // s
    const double m = params_.mass; // kg
    const Vec3 force = last_step_.forces.force + Vec3{0.0, 0.0, -m * gravity};
    body_.velocity += force * (dt / m);
    body_.position += body_.velocity * dt;

    const Quat q = body_.orientation;
    const Vec3 I = params_.inertia;
    const Vec3 torque = rotate_inverse(q, last_step_.forces.torque);
    Vec3 w_vehicle = rotate_inverse(q, body_.angular_velocity);
    const Vec3& w = w_vehicle;
    const Vec3 gyro = cross(w, {I.x * w.x, I.y * w.y, I.z * w.z});
    w_vehicle += Vec3{(torque.x - gyro.x) / I.x, (torque.y - gyro.y) / I.y,
                      (torque.z - gyro.z) / I.z} * dt;
    body_.angular_velocity = rotate(q, w_vehicle);

    const Vec3 w_world = body_.angular_velocity;
    const Quat dq = Quat{0.0, w_world.x, w_world.y, w_world.z} * q;
    body_.orientation = normalized(
        {q.w + 0.5 * dt * dq.w, q.x + 0.5 * dt * dq.x, q.y + 0.5 * dt * dq.y, q.z + 0.5 * dt * dq.z});
}

std::uint64_t TestWorld::checksum() const { return ultragravity::checksum(state_, body_); }

} // namespace ultragravity
