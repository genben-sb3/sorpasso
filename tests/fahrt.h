#pragma once

#include <cmath>
#include <cstdint>

#include "doctest/doctest.h"
#include "ultragravity/testwelt.h"

namespace ultragravity::test {

inline constexpr double hundert_kmh = 100.0 / 3.6; // m/s

inline Kennwerte sportcoupe() {
    LadeErgebnis ergebnis = lade_fahrzeugdatei(UG_FAHRZEUGE_DIR "/sportcoupe.json");
    REQUIRE_FALSE(ergebnis.fehler.has_value());
    return ergebnis.kennwerte;
}

// Liefert für jeden Tick der Fahrt ein Eingabe-Paket mit laufender Tick-Nummer.
class Fahrt {
public:
    explicit Fahrt(Testwelt& welt) : welt_(welt) {}

    void fahre(double sekunden, EingabePaket paket = {}) {
        for (int i = 0; i < ticks(sekunden); ++i) einen_tick(paket);
    }

    void einen_tick(EingabePaket paket) {
        paket.tick = tick_++;
        welt_.tick(paket);
    }

    static int ticks(double sekunden) { return static_cast<int>(std::lround(sekunden / Testwelt::zeitschritt)); }

private:
    Testwelt& welt_;
    std::uint32_t tick_ = 0;
};

inline double laengsgeschwindigkeit(const Testwelt& welt) {
    const Karosseriebewegung& k = welt.karosserie();
    return skalar(k.geschwindigkeit, drehe(k.ausrichtung, {1.0, 0.0, 0.0}));
}

} // namespace ultragravity::test
