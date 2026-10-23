#include "ultragravity/replay.h"

namespace ultragravity {

ReplayResult play_replay(const Recording& recording, TestWorld& world) {
    ReplayResult result;
    std::size_t next = 0;
    for (const InputPacket& packet : recording.inputs) {
        world.tick(packet);
        if (next < recording.snapshots.size() && recording.snapshots[next].tick == packet.tick) {
            const Snapshot& expected = recording.snapshots[next++];
            const Snapshot actual{packet.tick, world.last_step().state, world.body(), world.last_step().animation};
            if (!(actual == expected)) result.snapshots_match = false;
        }
    }
    result.checksum = world.checksum();
    return result;
}

} // namespace ultragravity
