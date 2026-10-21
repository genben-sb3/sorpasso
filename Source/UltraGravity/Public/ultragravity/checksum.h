#pragma once

#include <cstdint>

#include "ultragravity/export.h"
#include "ultragravity/vehicle_step.h"

namespace ultragravity {

// Checksum over the bit patterns of the entire state. The same drive in the same
// program yields the same checksum (ADR 0007).
UG_API std::uint64_t checksum(const State& state, const BodyMotion& body);

} // namespace ultragravity
