#include "ultragravity/recording.h"

#include <algorithm>
#include <array>
#include <bit>
#include <fstream>
#include <iterator>

namespace ultragravity {
namespace {

constexpr std::array<char, 4> identifier{'U', 'G', 'R', 'C'};

class ByteWriter {
public:
    void u8(std::uint8_t w) { data_.push_back(w); }
    void u32(std::uint32_t w) {
        for (int i = 0; i < 4; ++i) u8(static_cast<std::uint8_t>(w >> (8 * i)));
    }
    void f64(double w) {
        const std::uint64_t bits = std::bit_cast<std::uint64_t>(w);
        for (int i = 0; i < 8; ++i) u8(static_cast<std::uint8_t>(bits >> (8 * i)));
    }
    void vec(Vec3 v) {
        f64(v.x);
        f64(v.y);
        f64(v.z);
    }
    void quat(Quat q) {
        f64(q.w);
        f64(q.x);
        f64(q.y);
        f64(q.z);
    }
    template <std::size_t N>
    void array(const std::array<double, N>& a) {
        for (double w : a) f64(w);
    }
    std::vector<std::uint8_t> take() { return std::move(data_); }

private:
    std::vector<std::uint8_t> data_;
};

class ByteReader {
public:
    explicit ByteReader(const std::vector<std::uint8_t>& data) : data_(data) {}
    bool ok() const { return !past_end_; }
    std::size_t remaining() const { return data_.size() - pos_; }

    std::uint8_t u8() {
        if (pos_ >= data_.size()) {
            past_end_ = true;
            return 0;
        }
        return data_[pos_++];
    }
    std::uint32_t u32() {
        std::uint32_t w = 0;
        for (int i = 0; i < 4; ++i) w |= static_cast<std::uint32_t>(u8()) << (8 * i);
        return w;
    }
    double f64() {
        std::uint64_t bits = 0;
        for (int i = 0; i < 8; ++i) bits |= static_cast<std::uint64_t>(u8()) << (8 * i);
        return std::bit_cast<double>(bits);
    }
    Vec3 vec() {
        Vec3 v;
        v.x = f64();
        v.y = f64();
        v.z = f64();
        return v;
    }
    Quat quat() {
        Quat q;
        q.w = f64();
        q.x = f64();
        q.y = f64();
        q.z = f64();
        return q;
    }
    template <std::size_t N>
    void array(std::array<double, N>& a) {
        for (double& w : a) w = f64();
    }

private:
    const std::vector<std::uint8_t>& data_;
    std::size_t pos_ = 0;
    bool past_end_ = false;
};

// Sizes on disk; reading checks the counts against the bytes present.
constexpr std::size_t input_bytes = 4 + 5 * 8 + 4;                   // tick, 5 doubles, gear, ABS, TC, ESC
constexpr std::size_t state_bytes = (3 * wheel_count + 5) * 8;       // fields per wheel plus 5 single values
constexpr std::size_t body_bytes = (3 + 4 + 3 + 3) * 8;              // position, orientation, 2 velocities
constexpr std::size_t animation_bytes = (3 * wheel_count + 4) * 8;   // fields per wheel plus 4 single values
constexpr std::size_t snapshot_bytes = 4 + state_bytes + body_bytes + animation_bytes;

void write(ByteWriter& w, const InputPacket& p) {
    w.u32(p.tick);
    w.f64(p.steering);
    w.f64(p.throttle);
    w.f64(p.brake);
    w.f64(p.clutch);
    w.f64(p.handbrake);
    w.u8(static_cast<std::uint8_t>(p.gear_request));
    w.u8(p.driving_aids.abs ? 1 : 0);
    w.u8(static_cast<std::uint8_t>(p.driving_aids.traction_control));
    w.u8(p.driving_aids.stability_control ? 1 : 0);
}

InputPacket read_input(ByteReader& r) {
    InputPacket p;
    p.tick = r.u32();
    p.steering = r.f64();
    p.throttle = r.f64();
    p.brake = r.f64();
    p.clutch = r.f64();
    p.handbrake = r.f64();
    p.gear_request = static_cast<std::int8_t>(r.u8());
    p.driving_aids.abs = r.u8() != 0;
    p.driving_aids.traction_control = static_cast<std::int8_t>(r.u8());
    p.driving_aids.stability_control = r.u8() != 0;
    return p;
}

void write(ByteWriter& w, const Snapshot& s) {
    w.u32(s.tick);
    w.array(s.state.wheel_speed);
    w.array(s.state.wheel_angle);
    w.array(s.state.slip_angle);
    w.f64(s.state.engine_speed);
    w.f64(static_cast<double>(s.state.gear));
    w.f64(s.state.shift_remaining);
    w.f64(s.state.gear_age);
    w.f64(s.state.traction_factor);
    w.vec(s.body.position);
    w.quat(s.body.orientation);
    w.vec(s.body.velocity);
    w.vec(s.body.angular_velocity);
    w.array(s.animation.wheel_angle);
    w.array(s.animation.steer_angle);
    w.array(s.animation.suspension_travel);
    w.f64(s.animation.pitch);
    w.f64(s.animation.roll);
    w.f64(s.animation.steering_wheel);
    w.f64(s.animation.engine_speed);
}

Snapshot read_snapshot(ByteReader& r) {
    Snapshot s;
    s.tick = r.u32();
    r.array(s.state.wheel_speed);
    r.array(s.state.wheel_angle);
    r.array(s.state.slip_angle);
    s.state.engine_speed = r.f64();
    s.state.gear = static_cast<int>(r.f64());
    s.state.shift_remaining = r.f64();
    s.state.gear_age = r.f64();
    s.state.traction_factor = r.f64();
    s.body.position = r.vec();
    s.body.orientation = r.quat();
    s.body.velocity = r.vec();
    s.body.angular_velocity = r.vec();
    r.array(s.animation.wheel_angle);
    r.array(s.animation.steer_angle);
    r.array(s.animation.suspension_travel);
    s.animation.pitch = r.f64();
    s.animation.roll = r.f64();
    s.animation.steering_wheel = r.f64();
    s.animation.engine_speed = r.f64();
    return s;
}

ReadResult failure(std::string text) {
    ReadResult e;
    e.error = std::move(text);
    return e;
}

double blend(double a, double b, double t) { return a + (b - a) * t; }

Quat blend(Quat a, Quat b, double t) {
    // Short way: q and -q are the same rotation.
    const double sign = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z < 0.0 ? -1.0 : 1.0; // 1
    return normalized({blend(a.w, sign * b.w, t), blend(a.x, sign * b.x, t), blend(a.y, sign * b.y, t),
                       blend(a.z, sign * b.z, t)});
}

} // namespace

void Recorder::add(const InputPacket& input, const StepResult& result, const BodyMotion& body) {
    if (recording_.inputs.size() % snapshot_interval == 0) {
        recording_.snapshots.push_back({input.tick, result.state, body, result.animation});
    }
    recording_.inputs.push_back(input);
}

std::vector<std::uint8_t> serialize(const Recording& recording) {
    ByteWriter w;
    for (char c : identifier) w.u8(static_cast<std::uint8_t>(c));
    w.u32(recording_version);
    w.u32(static_cast<std::uint32_t>(recording.inputs.size()));
    w.u32(static_cast<std::uint32_t>(recording.snapshots.size()));
    for (const InputPacket& p : recording.inputs) write(w, p);
    for (const Snapshot& s : recording.snapshots) write(w, s);
    return w.take();
}

ReadResult deserialize(const std::vector<std::uint8_t>& data) {
    ByteReader r(data);
    for (char c : identifier) {
        if (r.u8() != static_cast<std::uint8_t>(c) || !r.ok()) {
            return failure("not a recording: the data does not start with the identifier UGRC");
        }
    }
    const std::uint32_t version = r.u32();
    if (!r.ok()) return failure("recording cut off: the version number is missing");
    if (version == 0 || version > recording_version) {
        return failure("recording has version " + std::to_string(version) + ", this program reads up to version " +
                       std::to_string(recording_version));
    }
    const std::uint32_t input_count = r.u32();
    const std::uint32_t snapshot_count = r.u32();
    if (!r.ok()) return failure("recording cut off: the header is incomplete");
    if (static_cast<std::uint64_t>(input_count) * input_bytes +
            static_cast<std::uint64_t>(snapshot_count) * snapshot_bytes !=
        r.remaining()) {
        return failure("recording damaged: the length does not match the stated counts");
    }
    ReadResult e;
    e.recording.inputs.reserve(input_count);
    for (std::uint32_t i = 0; i < input_count; ++i) e.recording.inputs.push_back(read_input(r));
    e.recording.snapshots.reserve(snapshot_count);
    for (std::uint32_t i = 0; i < snapshot_count; ++i) e.recording.snapshots.push_back(read_snapshot(r));
    return e;
}

std::optional<std::string> save_recording(const std::string& path, const Recording& recording) {
    const std::vector<std::uint8_t> data = serialize(recording);
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!out) return "cannot write recording to " + path;
    return std::nullopt;
}

ReadResult load_recording(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return failure("cannot open recording: " + path);
    const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return deserialize(data);
}

Ghost::Ghost(const Recording& recording, double tick_duration)
    : snapshots_(recording.snapshots), tick_duration_(tick_duration) {}

Ghost::Pose Ghost::pose_at(double tick) const {
    if (snapshots_.empty()) return {};
    const auto next = std::lower_bound(snapshots_.begin(), snapshots_.end(), tick,
                                       [](const Snapshot& s, double t) { return s.tick < t; });
    if (next == snapshots_.begin() || next == snapshots_.end()) {
        const Snapshot& s = next == snapshots_.end() ? snapshots_.back() : *next;
        return {s.body, s.animation};
    }
    const Snapshot& a = *(next - 1);
    const Snapshot& b = *next;
    const double fraction = (tick - a.tick) / static_cast<double>(b.tick - a.tick); // 1
    Pose pose;
    pose.body = {a.body.position + (b.body.position - a.body.position) * fraction,
                 blend(a.body.orientation, b.body.orientation, fraction),
                 a.body.velocity + (b.body.velocity - a.body.velocity) * fraction,
                 a.body.angular_velocity + (b.body.angular_velocity - a.body.angular_velocity) * fraction};
    pose.animation = interpolate(a.animation, b.animation, fraction);
    // Between two snapshots a wheel turns more than half a revolution; the short way of
    // interpolate() would be wrong. The stored wheel speed gives the direction.
    const double dt = (tick - a.tick) * tick_duration_; // s
    for (int w = 0; w < wheel_count; ++w) {
        pose.animation.wheel_angle[w] = wrap_angle(a.animation.wheel_angle[w] + a.state.wheel_speed[w] * dt);
    }
    return pose;
}

} // namespace ultragravity
