#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>

#include "ultragravity/testwelt.h"

using namespace ultragravity;

namespace {

constexpr double hundert_kmh = 100.0 / 3.6;

Kennwerte sportcoupe() {
    LadeErgebnis ergebnis = lade_fahrzeugdatei(UG_FAHRZEUGE_DIR "/sportcoupe.json");
    REQUIRE_FALSE(ergebnis.fehler.has_value());
    return ergebnis.kennwerte;
}

// Liefert für jeden Tick der Fahrt ein Eingabe-Paket mit laufender Tick-Nummer.
class Fahrt {
public:
    explicit Fahrt(Testwelt& welt) : welt_(welt) {}

    void fahre(double sekunden, EingabePaket paket = {}) {
        const int ticks = static_cast<int>(std::lround(sekunden / Testwelt::zeitschritt));
        for (int i = 0; i < ticks; ++i) einen_tick(paket);
    }

    void einen_tick(EingabePaket paket) {
        paket.tick = tick_++;
        welt_.tick(paket);
    }

private:
    Testwelt& welt_;
    std::uint32_t tick_ = 0;
};

EingabePaket vollbremsung() {
    EingabePaket paket;
    paket.bremse = 1.0;
    return paket;
}

double laengsgeschwindigkeit(const Testwelt& welt) {
    const Karosseriebewegung& k = welt.karosserie();
    return skalar(k.geschwindigkeit, drehe(k.ausrichtung, {1.0, 0.0, 0.0}));
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

TEST_CASE("Ein direktes Radmoment beschleunigt das Auto aus dem Stand, ohne Zittern beim Anfahren") {
    const Kennwerte auto_ = sportcoupe();
    Testwelt welt(auto_, 0.02);
    Fahrt fahrt(welt);
    fahrt.fahre(2.0);
    REQUIRE(std::abs(laengsgeschwindigkeit(welt)) < 1e-6);

    EingabePaket halbgas;
    halbgas.gas = 0.5;
    double v = laengsgeschwindigkeit(welt);
    std::array<double, anzahl_raeder> omega{};
    for (int tick = 0; tick < 3 * 120; ++tick) {
        fahrt.einen_tick(halbgas);
        const double v_neu = laengsgeschwindigkeit(welt);
        CHECK(v_neu > v);
        v = v_neu;
        for (int i = 0; i < anzahl_raeder; ++i) {
            const double omega_neu = welt.letzter_schritt().zustand.raddrehzahl[i];
            CHECK(omega_neu > omega[i]);
            omega[i] = omega_neu;
        }
    }

    // Ohne Fahrwiderstände treibt die Antriebskraft T/r die Masse des Autos samt
    // der Drehträgheit seiner Räder (I/r² je Rad) gleichmäßig an.
    const double r = auto_.hinten.radradius;
    const double kraft = 0.5 * auto_.antrieb.radmoment / r;
    const double masse = auto_.masse + 2.0 * auto_.vorn.reifen.traegheit / (auto_.vorn.radradius * auto_.vorn.radradius) +
                         2.0 * auto_.hinten.reifen.traegheit / (r * r);
    CHECK(v == doctest::Approx(kraft / masse * 3.0).epsilon(0.02));
}
