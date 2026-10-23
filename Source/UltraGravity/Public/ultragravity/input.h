#pragma once

#include <cstdint>

namespace ultragravity {

inline constexpr int traction_control_max = 5; // API, highest level of the traction control; larger ones count as this

struct DrivingAids {
    bool abs = false;
    int traction_control = 0; // level 0 (off) to traction_control_max
    bool stability_control = false;

    bool operator==(const DrivingAids&) const = default;
};

// The control commands for one sim tick. The only way to control a car:
// the same for player, AI, replay and ghost.
struct InputPacket {
    std::uint32_t tick = 0;
    double steering = 0.0;  // -1 (right) … 1 (left)
    double throttle = 0.0;  // 0 … 1
    double brake = 0.0;     // 0 … 1
    double clutch = 0.0;    // 0 (engaged) … 1 (pressed)
    double handbrake = 0.0; // 0 … 1
    std::int8_t gear_request = 0; // -1 reverse, 0 neutral, 1 … n forward gears
    DrivingAids driving_aids;

    bool operator==(const InputPacket&) const = default;
};

} // namespace ultragravity
