#include "ultragravity/checksum.h"

#include <bit>

namespace ultragravity {
namespace {

// FNV-1a over 64 bits, byte by byte.
class Fnv1a {
public:
    void add(double value) {
        std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
        for (int i = 0; i < 8; ++i) {
            sum_ ^= bits & 0xffu;
            sum_ *= 0x100000001b3u;
            bits >>= 8;
        }
    }
    void add(Vec3 v) {
        add(v.x);
        add(v.y);
        add(v.z);
    }
    void add(Quat q) {
        add(q.w);
        add(q.x);
        add(q.y);
        add(q.z);
    }
    std::uint64_t sum() const { return sum_; }

private:
    std::uint64_t sum_ = 0xcbf29ce484222325u;
};

} // namespace

std::uint64_t checksum(const State& state, const BodyMotion& body) {
    Fnv1a f;
    f.add(body.position);
    f.add(body.orientation);
    f.add(body.velocity);
    f.add(body.angular_velocity);
    for (double w : state.wheel_speed) f.add(w);
    for (double w : state.wheel_angle) f.add(w);
    for (double a : state.slip_angle) f.add(a);
    f.add(state.engine_speed);
    f.add(static_cast<double>(state.gear));
    f.add(state.shift_remaining);
    f.add(state.gear_age);
    f.add(state.traction_factor);
    return f.sum();
}

} // namespace ultragravity
