#pragma once

#include <cstdint>

#include "ultragravity/recording.h"
#include "ultragravity/test_world.h"

namespace ultragravity {

struct ReplayResult {
    std::uint64_t checksum = 0;  // 1, checksum of the world after the last tick
    bool snapshots_match = true; // all stored states match the new simulation bit for bit
};

// Simulates the drive again from its input packets, in a world set up like the one of the
// recording (same parameters as when recording).
ReplayResult play_replay(const Recording& recording, TestWorld& world);

} // namespace ultragravity
