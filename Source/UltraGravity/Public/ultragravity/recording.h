#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ultragravity/animation_data.h"
#include "ultragravity/export.h"
#include "ultragravity/input.h"
#include "ultragravity/vehicle_step.h"

namespace ultragravity {

inline constexpr std::uint32_t recording_version = 1; // API, 1, current format version
inline constexpr std::uint32_t snapshot_interval = 60; // API, 1, ticks between two complete states

// Complete state at the end of a tick.
struct Snapshot {
    std::uint32_t tick = 0; // 1, tick after which the state applies
    State state;
    BodyMotion body;
    AnimationData animation;

    bool operator==(const Snapshot&) const = default;
};

// A drive: all input packets and a snapshot every snapshot_interval ticks.
struct Recording {
    std::vector<InputPacket> inputs;
    std::vector<Snapshot> snapshots;

    bool operator==(const Recording&) const = default;
};

// Collects a drive: call once per tick, with the packet and the result of that tick.
class Recorder {
public:
    UG_API void add(const InputPacket& input, const StepResult& result, const BodyMotion& body);
    const Recording& recording() const { return recording_; }

private:
    Recording recording_;
};

// Binary format: identifier "UGRC", version number, then the data. All numbers
// little-endian, floating point numbers as bit patterns, so reading returns exactly what was written.
UG_API std::vector<std::uint8_t> serialize(const Recording& recording);

struct ReadResult {
    Recording recording;
    std::optional<std::string> error; // understandable message when the data cannot be read
};

UG_API ReadResult deserialize(const std::vector<std::uint8_t>& data);

// Writes and reads a recording file with the standard library. An adapter that has its own file
// access (Unreal) hands the bytes of serialize and deserialize to it instead.
UG_API std::optional<std::string> save_recording(const std::string& path, const Recording& recording);
UG_API ReadResult load_recording(const std::string& path);

// Only plays back stored states, without simulating. Between two snapshots it interpolates,
// before the first and after the last that one applies.
class Ghost {
public:
    struct Pose {
        BodyMotion body;
        AnimationData animation;
    };

    // tick_duration: length of a tick, so the wheels keep turning correctly between snapshots.
    UG_API Ghost(const Recording& recording, double tick_duration); // tick_duration in s

    bool empty() const { return snapshots_.empty(); }
    // tick may lie between whole ticks (frame rate ≠ tick rate). An empty ghost yields a zero pose.
    UG_API Pose pose_at(double tick) const;

private:
    std::vector<Snapshot> snapshots_;
    double tick_duration_ = 0.0; // s
};

} // namespace ultragravity
