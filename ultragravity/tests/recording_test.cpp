#include "doctest/doctest.h"

#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>

#include "drive.h"
#include "ultragravity/recording.h"
#include "ultragravity/replay.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

constexpr int maneuver_ticks = 400;

TestWorld new_world() { return TestWorld(sports_coupe(), 0.02, TestGround{}, 20.0); }

InputPacket maneuver_packet(int i) {
    const bool throttle_phase = i < maneuver_ticks / 2;
    InputPacket p = drive_input(throttle_phase ? 0.8 : 0.0, throttle_phase ? 0.0 : 0.5, i % 40 < 20 ? 0.3 : -0.3);
    p.tick = static_cast<std::uint32_t>(i);
    p.driving_aids.abs = i % 3 == 0;
    p.driving_aids.traction_control = i % 6;
    p.driving_aids.stability_control = i % 4 == 0;
    return p;
}

struct Take {
    Recording recording;
    std::uint64_t checksum = 0;
};

Take record() {
    TestWorld world = new_world();
    Recorder recorder;
    for (int i = 0; i < maneuver_ticks; ++i) {
        const InputPacket p = maneuver_packet(i);
        world.tick(p);
        recorder.add(p, world.last_step(), world.body());
    }
    return {recorder.recording(), world.checksum()};
}

} // namespace

TEST_CASE("The recording holds every packet and a state every 60 ticks") {
    const Recording r = record().recording;
    CHECK(r.inputs.size() == maneuver_ticks);
    REQUIRE(r.snapshots.size() == (maneuver_ticks + 59) / 60);
    for (std::size_t i = 0; i < r.snapshots.size(); ++i) CHECK(r.snapshots[i].tick == 60 * i);
}

TEST_CASE("Writing and reading back yields exactly the same recording") {
    const Recording r = record().recording;
    const ReadResult read = deserialize(serialize(r));
    REQUIRE_FALSE(read.error.has_value());
    CHECK(read.recording == r);
    CHECK(serialize(read.recording) == serialize(r));

    const std::string path = std::string(UG_TESTDATA_DIR) + "/recording_test.ugrc";
    REQUIRE_FALSE(save_recording(path, r).has_value());
    const ReadResult from_file = load_recording(path);
    std::remove(path.c_str());
    REQUIRE_FALSE(from_file.error.has_value());
    CHECK(from_file.recording == r);
}

TEST_CASE("An empty recording survives the round trip") {
    const ReadResult read = deserialize(serialize(Recording{}));
    REQUIRE_FALSE(read.error.has_value());
    CHECK(read.recording == Recording{});
}

TEST_CASE("A foreign identifier, a too new version and damaged files are rejected understandably") {
    const std::vector<std::uint8_t> good = serialize(record().recording);

    auto foreign = good;
    foreign[0] = 'X';
    const ReadResult f = deserialize(foreign);
    REQUIRE(f.error.has_value());
    CHECK(f.error->find("identifier") != std::string::npos);

    auto newer = good;
    newer[4] = static_cast<std::uint8_t>(recording_version + 1);
    const ReadResult n = deserialize(newer);
    REQUIRE(n.error.has_value());
    CHECK(n.error->find("version") != std::string::npos);

    auto cut = good;
    cut.resize(cut.size() - 5);
    const ReadResult c = deserialize(cut);
    REQUIRE(c.error.has_value());
    CHECK(c.error->find("damaged") != std::string::npos);

    CHECK(deserialize({}).error.has_value());
    CHECK(deserialize({'U', 'G'}).error.has_value());
    CHECK(load_recording("/does/not/exist.ugrc").error.has_value());
}

TEST_CASE("A replay yields a bit-identical drive") {
    const Take take = record();
    CHECK(take.checksum != 0);

    TestWorld world = new_world();
    const ReplayResult replay = play_replay(take.recording, world);
    CHECK(replay.checksum == take.checksum);
    CHECK(replay.snapshots_match);

    // A replay of a recording read back yields the same.
    TestWorld world2 = new_world();
    const ReadResult read = deserialize(serialize(take.recording));
    REQUIRE_FALSE(read.error.has_value());
    CHECK(play_replay(read.recording, world2).checksum == take.checksum);
}

TEST_CASE("A replay with a changed input deviates") {
    Take take = record();
    take.recording.inputs[10].steering += 0.5;
    TestWorld world = new_world();
    const ReplayResult replay = play_replay(take.recording, world);
    CHECK(replay.checksum != take.checksum);
    CHECK_FALSE(replay.snapshots_match);
}

TEST_CASE("A ghost delivers animation data at any time without simulating") {
    const Recording r = record().recording;
    const Ghost ghost(r, TestWorld::time_step);
    REQUIRE_FALSE(ghost.empty());

    // At the stored states it returns exactly these.
    for (const Snapshot& s : r.snapshots) {
        const Ghost::Pose pose = ghost.pose_at(s.tick);
        CHECK(pose.animation.pitch == s.animation.pitch);
        CHECK(pose.animation.suspension_travel == s.animation.suspension_travel);
        CHECK(pose.body.position.x == s.body.position.x);
    }

    // In between the position lies between the two neighbors, the car drives forward.
    const double x0 = r.snapshots[1].body.position.x; // m
    const double x1 = r.snapshots[2].body.position.x; // m
    const double middle = ghost.pose_at(90.0).body.position.x; // m
    CHECK(middle == doctest::Approx(0.5 * (x0 + x1)));
    CHECK(x1 > x0);

    // Before the start and after the end the first and last state apply; never NaN.
    CHECK(ghost.pose_at(-5.0).body.position.x == r.snapshots.front().body.position.x);
    CHECK(ghost.pose_at(1e6).body.position.x == r.snapshots.back().body.position.x);
    for (double t = 0.0; t < maneuver_ticks; t += 7.3) {
        const Ghost::Pose pose = ghost.pose_at(t);
        CHECK(std::isfinite(pose.animation.engine_speed));
        for (double w : pose.animation.wheel_angle) {
            CHECK(w >= -std::numbers::pi);
            CHECK(w < std::numbers::pi);
        }
    }
}

TEST_CASE("In the ghost the wheel keeps turning in the right direction between snapshots") {
    const Recording r = record().recording;
    const Ghost ghost(r, TestWorld::time_step);
    // One tick after the snapshot the angle is close to the state of the real tick.
    const double real = r.snapshots[0].animation.wheel_angle[front_left];     // rad
    const double after = ghost.pose_at(1.0).animation.wheel_angle[front_left]; // rad
    const double step = wrap_angle(after - real);                              // rad
    const double expected = r.snapshots[0].state.wheel_speed[front_left] * TestWorld::time_step; // rad
    CHECK(step > 0.0);
    CHECK(step == doctest::Approx(expected));
}

TEST_CASE("An empty ghost delivers a zero pose") {
    const Ghost ghost(Recording{}, TestWorld::time_step);
    CHECK(ghost.empty());
    CHECK(ghost.pose_at(3.0).animation.engine_speed == 0.0);
}
