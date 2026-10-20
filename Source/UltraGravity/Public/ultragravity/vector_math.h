#pragma once

#include <cmath>

// Small vector and quaternion math. SI units, axes per ISO 8855
// (X forward, Y left, Z up, right-handed), see ADR-0004.
namespace ultragravity {

struct Vec3 {
    double x = 0.0; // unit of the quantity
    double y = 0.0; // unit of the quantity
    double z = 0.0; // unit of the quantity
};

constexpr Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
constexpr Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
constexpr Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
constexpr Vec3 operator*(Vec3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
constexpr Vec3 operator*(double s, Vec3 a) { return a * s; }
constexpr Vec3& operator+=(Vec3& a, Vec3 b) { return a = a + b; }

constexpr double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double length(Vec3 a) { return std::sqrt(dot(a, a)); }

// Unit quaternion for a rotation, w is the real part.
struct Quat {
    double w = 1.0; // 1
    double x = 0.0; // 1
    double y = 0.0; // 1
    double z = 0.0; // 1
};

constexpr Quat operator*(Quat a, Quat b) {
    return {
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
    };
}

inline Quat normalized(Quat q) {
    const double n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z); // 1
    return {q.w / n, q.x / n, q.y / n, q.z / n};
}

// Rotates v from the vehicle frame into the world frame.
constexpr Vec3 rotate(Quat q, Vec3 v) {
    const Vec3 u{q.x, q.y, q.z};
    const Vec3 t = 2.0 * cross(u, v);
    return v + q.w * t + cross(u, t);
}

// Rotates v from the world frame into the vehicle frame.
constexpr Vec3 rotate_inverse(Quat q, Vec3 v) { return rotate({q.w, -q.x, -q.y, -q.z}, v); }

} // namespace ultragravity
