#include "ultragravity/pruefsumme.h"

#include <bit>

namespace ultragravity {
namespace {

// FNV-1a über 64 Bit, Byte für Byte.
class Fnv1a {
public:
    void nimm(double wert) {
        std::uint64_t bits = std::bit_cast<std::uint64_t>(wert);
        for (int i = 0; i < 8; ++i) {
            summe_ ^= bits & 0xffu;
            summe_ *= 0x100000001b3u;
            bits >>= 8;
        }
    }
    void nimm(Vec3 v) {
        nimm(v.x);
        nimm(v.y);
        nimm(v.z);
    }
    void nimm(Quat q) {
        nimm(q.w);
        nimm(q.x);
        nimm(q.y);
        nimm(q.z);
    }
    std::uint64_t summe() const { return summe_; }

private:
    std::uint64_t summe_ = 0xcbf29ce484222325u;
};

} // namespace

std::uint64_t pruefsumme(const Zustand& zustand, const Karosseriebewegung& karosserie) {
    Fnv1a f;
    f.nimm(karosserie.position);
    f.nimm(karosserie.ausrichtung);
    f.nimm(karosserie.geschwindigkeit);
    f.nimm(karosserie.drehgeschwindigkeit);
    for (double w : zustand.raddrehzahl) f.nimm(w);
    return f.summe();
}

} // namespace ultragravity
