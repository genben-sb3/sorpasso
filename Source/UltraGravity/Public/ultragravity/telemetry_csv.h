#pragma once

#include <iosfwd>

#include "ultragravity/export.h"
#include "ultragravity/input.h"
#include "ultragravity/vehicle_step.h"

namespace ultragravity {

// Telemetry as CSV: a header row with units in square brackets, followed by
// exactly one row per tick. Numbers use a decimal point and are written so
// they read back losslessly.
UG_API void write_telemetry_header(std::ostream& out);

// The row for the tick that `result` computed from `input`.
UG_API void write_telemetry_row(std::ostream& out, const InputPacket& input, const StepResult& result);

} // namespace ultragravity
