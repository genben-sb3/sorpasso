#pragma once

#include <numbers>

// Conversion factors to SI units. Every conversion lives only here.
namespace ultragravity {

inline constexpr double kmh = 1.0 / 3.6;                       // API, m/s per km/h
inline constexpr double rpm = 2.0 * std::numbers::pi / 60.0;   // API, rad/s per rpm

} // namespace ultragravity
