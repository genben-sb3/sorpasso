#pragma once

#include <numbers>

// Conversion factors to SI units and the gravity of the earth. Every conversion lives only here.
namespace ultragravity {

inline constexpr double kmh = 1.0 / 3.6;                       // API, m/s per km/h
inline constexpr double rpm = 2.0 * std::numbers::pi / 60.0;   // API, rad/s per rpm

inline constexpr double gravity = 9.81; // API, m/s²

// Largest lateral acceleration in m/s² a tire with the peak friction (1) can hold.
constexpr double lateral_acceleration_limit(double peak_friction) { return peak_friction * gravity; }

} // namespace ultragravity
