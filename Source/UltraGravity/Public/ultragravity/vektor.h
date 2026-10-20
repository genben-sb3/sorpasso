#pragma once

#include <cmath>

// Kleine Vektor- und Quaternion-Mathematik. SI-Einheiten, Achsen nach ISO 8855
// (X vorn, Y links, Z oben, rechtshändig), siehe ADR-0004.
namespace ultragravity {

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

constexpr Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
constexpr Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
constexpr Vec3 operator-(Vec3 a) { return {-a.x, -a.y, -a.z}; }
constexpr Vec3 operator*(Vec3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
constexpr Vec3 operator*(double s, Vec3 a) { return a * s; }
constexpr Vec3& operator+=(Vec3& a, Vec3 b) { return a = a + b; }

constexpr double skalar(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr Vec3 kreuz(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline double betrag(Vec3 a) { return std::sqrt(skalar(a, a)); }

// Einheitsquaternion für eine Drehung, w ist der Realteil.
struct Quat {
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

constexpr Quat operator*(Quat a, Quat b) {
    return {
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
    };
}

inline Quat normiert(Quat q) {
    const double n = std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    return {q.w / n, q.x / n, q.y / n, q.z / n};
}

// Dreht v aus dem Fahrzeugsystem ins Weltsystem.
constexpr Vec3 drehe(Quat q, Vec3 v) {
    const Vec3 u{q.x, q.y, q.z};
    const Vec3 t = 2.0 * kreuz(u, v);
    return v + q.w * t + kreuz(u, t);
}

// Dreht v aus dem Weltsystem ins Fahrzeugsystem.
constexpr Vec3 drehe_zurueck(Quat q, Vec3 v) { return drehe({q.w, -q.x, -q.y, -q.z}, v); }

} // namespace ultragravity
