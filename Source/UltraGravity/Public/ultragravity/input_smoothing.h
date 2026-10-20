#pragma once

#include <cstdint>

#include "ultragravity/input.h"
#include "ultragravity/vehicle_file.h"
#include "ultragravity/vehicle_step.h"

namespace ultragravity {

// Raw signals from keyboard or gamepad. Keys deliver -1, 0 or 1.
struct RawInput {
    double steering = 0.0;    // -1 (right) … 1 (left)
    double throttle = 0.0;    // 0 … 1
    double brake = 0.0;       // 0 … 1
    double clutch = 0.0;      // 0 … 1, passed through
    double handbrake = 0.0;   // 0 … 1, passed through
    std::int8_t gear_request = 0; // passed through
    DrivingAids driving_aids;     // passed through
};

// How fast the smoothing builds up signals and how it limits the steering.
struct SmoothingParams {
    double steer_rise_time = 0.30;    // s from center to full lock
    double steer_fall_time = 0.15;    // s from full lock back to center
    double throttle_rise_time = 0.25; // s from 0 to 1
    double throttle_fall_time = 0.10; // s from 1 to 0
    double brake_rise_time = 0.20;    // s from 0 to 1
    double brake_fall_time = 0.10;    // s from 1 to 0
    double countersteer_gain = 4.0;      // 1, steering per rad of drift angle
    double countersteer_deadzone = 0.05; // rad, drift angle below which the aid does not steer
    double countersteer_min_speed = 3.0; // m/s, below this there is no aid
    double peak_fraction = 0.8;          // 1, fraction of the peak slip angle the steering limit uses
    double steer_key_deadzone = 0.05;    // 1, raw steering below this counts as released
};

// The state of the smoothing between two ticks: the last output values.
struct SmoothingState {
    double steering = 0.0; // -1 … 1
    double throttle = 0.0; // 0 … 1
    double brake = 0.0;    // 0 … 1
};

struct SmoothingResult {
    InputPacket packet;
    SmoothingState state;
};

// Largest steering input (0 … 1) a car can sensibly use at this speed:
// the front tire should not exceed the peak slip angle (scaled by
// peak_fraction), plus the steer angle the corner demands kinematically at full
// lateral acceleration.
double steering_limit(const VehicleParams& params, double speed, double peak_fraction);

// Turns raw signals into an input packet. A pure function: reads the
// last state of the body and writes nothing back. Test drivers and AI
// do not call it.
SmoothingResult smooth(const SmoothingState& state, const SmoothingParams& params,
                       const VehicleParams& vehicle, const BodyMotion& body,
                       const RawInput& raw, std::uint32_t tick, double dt);

} // namespace ultragravity
