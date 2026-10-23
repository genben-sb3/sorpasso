#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ultragravity/test_maneuver.h"
#include "ultragravity/vehicle_file.h"

namespace ultragravity {

// Result of the PP calculation: either a value or a clear message why there is none.
struct PpResult {
    std::optional<int> value; // PP, dimensionless
    std::string error;
};

// Computes the PP value from the weighted results of acceleration 0–100 and 0–200, skidpad,
// braking distance (without ABS) and top speed (ADR 0005). Each result is set against a fixed
// reference (better than the reference: factor above 1); the PP value is 100 times the weighted
// geometric mean of these factors. If one of these maneuvers failed or is missing, there is no
// PP value but a message.
PpResult compute_pp(const std::vector<Measurement>& measurements);

// Id of the physics version: checksum of a fixed reference drive with this car, 16 hex digits.
// If the physics or the car changes, the id changes.
std::string physics_version_id(const VehicleParams& params);

struct StoredPp {
    int value = 0; // PP, dimensionless
    std::string physics_version;
};

// Reads the block "pp" from vehicle file text; nullopt when it is missing or incomplete.
std::optional<StoredPp> read_pp(const std::string& json_text);

// Sets the block "pp" in the vehicle file text (replaces an existing one). The rest of the text
// stays character for character. Returns an empty text for invalid JSON.
std::string set_pp(const std::string& json_text, const StoredPp& pp);

// Warning when the stored id does not match the current one; empty when everything matches
// or no PP value is stored yet.
std::string pp_warning(const std::optional<StoredPp>& stored, const std::string& current_id);

} // namespace ultragravity
