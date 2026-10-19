#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>

#include "fahrt.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

EingabePaket vollbremsung() {
    EingabePaket paket;
    paket.bremse = 1.0;
    return paket;
}

// Bremst voll, bis das Auto praktisch steht, und liefert den Weg ab Bremsbeginn.
double bremsweg(Testwelt& welt, Fahrt& fahrt) {
    const Vec3 start = welt.karosserie().position;
    for (int tick = 0; laengsgeschwindigkeit(welt) > 0.01; ++tick) {
        REQUIRE(tick < 20 * 120);
        fahrt.einen_tick(vollbremsung());
    }
    return betrag(welt.karosserie().position - start);
}

// Bremsweg aus μ·g: s = v²/(2·μ·g). Blockierte Räder gleiten unterhalb des
// Spitzenreibwerts, deshalb darf der Weg etwas länger sein, aber nie kürzer.
void pruefe_bremsweg(double weg, double v, double reibwert) {
    const double ideal = v * v / (2.0 * reibwert * Testwelt::erdbeschleunigung);
    CAPTURE(ideal);
    CHECK(weg > 0.98 * ideal);
    CHECK(weg < 1.15 * ideal);
}

} // namespace

TEST_CASE("Der Bremsweg aus 100 km/h auf trockenem Belag folgt aus μ·g") {
    const Kennwerte auto_ = sportcoupe();
    Testwelt welt(auto_, 0.02, Testgelaende{}, hundert_kmh);
    Fahrt fahrt(welt);
    fahrt.fahre(1.5); // einfedern lassen

    const double v = laengsgeschwindigkeit(welt);
    REQUIRE(v == doctest::Approx(hundert_kmh).epsilon(0.01));
    const double weg = bremsweg(welt, fahrt);

    CAPTURE(weg);
    pruefe_bremsweg(weg, v, auto_.vorn.reifen.spitzenreibwert * Testgelaende{}.reibwert);
}

TEST_CASE("Auf einer nassen Belagzone ist der Bremsweg entsprechend länger") {
    const Kennwerte auto_ = sportcoupe();
    const double trocken = Testgelaende{}.reibwert;
    const double nass = 0.7;
    // Das Auto rollt beim Einfedern rund 40 m weit und bremst dann ganz auf der Zone.
    Testgelaende gelaende;
    gelaende.belagzonen.push_back({.x_von = 20.0, .x_bis = 500.0, .y_von = -10.0, .y_bis = 10.0, .reibwert = nass});

    Testwelt welt_trocken(auto_, 0.02, Testgelaende{}, hundert_kmh);
    Testwelt welt_nass(auto_, 0.02, gelaende, hundert_kmh);
    Fahrt fahrt_trocken(welt_trocken);
    Fahrt fahrt_nass(welt_nass);
    fahrt_trocken.fahre(1.5);
    fahrt_nass.fahre(1.5);
    REQUIRE(welt_nass.karosserie().position.x - 2.0 > 20.0);

    const double v = laengsgeschwindigkeit(welt_nass);
    const double weg_trocken = bremsweg(welt_trocken, fahrt_trocken);
    const double weg_nass = bremsweg(welt_nass, fahrt_nass);

    CAPTURE(weg_trocken);
    CAPTURE(weg_nass);
    pruefe_bremsweg(weg_nass, v, auto_.vorn.reifen.spitzenreibwert * nass);
    // s = v²/(2·μ·g): Bei gleicher Anfangsgeschwindigkeit verhalten sich die Wege umgekehrt wie die Reibwerte.
    CHECK(weg_nass / weg_trocken == doctest::Approx(trocken / nass).epsilon(0.03));
}

TEST_CASE("Beim Bremsen nickt die Karosserie nach vorn und die Vorderachse trägt mehr Last") {
    const Kennwerte auto_ = sportcoupe();
    Testwelt welt(auto_, 0.02, Testgelaende{}, hundert_kmh);
    Fahrt fahrt(welt);
    fahrt.fahre(1.5);

    // Nach einer Sekunde Vollbremsung ist das Nicken eingeschwungen.
    fahrt.fahre(1.0, vollbremsung());
    const double v_vorher = laengsgeschwindigkeit(welt);
    fahrt.einen_tick(vollbremsung());
    const double verzoegerung = (v_vorher - laengsgeschwindigkeit(welt)) / Testwelt::zeitschritt;
    REQUIRE(verzoegerung > 5.0);

    const Vec3 nase = drehe(welt.karosserie().ausrichtung, {1.0, 0.0, 0.0});
    CHECK(nase.z < 0.0);

    // Beckman: Lv = G·b/L + m·a·h/L mit h als Höhe des Schwerpunkts über dem Boden.
    const double gewicht = auto_.masse * Testwelt::erdbeschleunigung;
    const double radstand = auto_.vorn.x - auto_.hinten.x;
    const double b = auto_.schwerpunkt.x - auto_.hinten.x;
    const double h = welt.karosserie().position.z;
    const double erwartet = gewicht * b / radstand + auto_.masse * verzoegerung * h / radstand;

    const auto& radlast = welt.letzter_schritt().telemetrie.radlast;
    const double vorderachse = radlast[vorn_links] + radlast[vorn_rechts];
    CHECK(vorderachse > 1.2 * gewicht * b / radstand);
    CHECK(vorderachse == doctest::Approx(erwartet).epsilon(0.03));
}

TEST_CASE("Bei Vollbremsung blockieren die Räder, ohne dass die Raddrehzahl um null schwingt") {
    Testwelt welt(sportcoupe(), 0.02, Testgelaende{}, hundert_kmh);
    Fahrt fahrt(welt);
    fahrt.fahre(1.5);

    // Bis zum Stillstand und eine Sekunde darüber hinaus.
    std::array<bool, anzahl_raeder> blockiert{};
    double kleinste_drehzahl = 0.0;
    int nachlaufende_ticks = 0;
    for (int tick = 0; tick < 10 * 120 && nachlaufende_ticks < 120; ++tick) {
        fahrt.einen_tick(vollbremsung());
        if (laengsgeschwindigkeit(welt) < 0.01) ++nachlaufende_ticks;
        for (int i = 0; i < anzahl_raeder; ++i) {
            const double omega = welt.letzter_schritt().zustand.raddrehzahl[i];
            kleinste_drehzahl = std::min(kleinste_drehzahl, omega);
            // Einmal blockiert, bleibt das Rad genau stehen.
            if (blockiert[i]) CHECK(omega == 0.0);
            blockiert[i] = blockiert[i] || omega == 0.0;
        }
    }

    REQUIRE(nachlaufende_ticks == 120);
    CHECK(kleinste_drehzahl == 0.0);
    for (bool b : blockiert) CHECK(b);
    // Das stehende Auto rührt sich nicht mehr.
    CHECK(betrag(welt.karosserie().geschwindigkeit) < 1e-3);
}
