#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>

#include "ultragravity/testwelt.h"

using namespace ultragravity;

namespace {

Kennwerte sportcoupe() {
    LadeErgebnis ergebnis = lade_fahrzeugdatei(UG_FAHRZEUGE_DIR "/sportcoupe.json");
    REQUIRE_FALSE(ergebnis.fehler.has_value());
    return ergebnis.kennwerte;
}

// Fährt ohne Steuerbefehle, nur das Tick-Feld der Eingabe-Pakete läuft mit.
void stehe(Testwelt& welt, std::uint32_t& tick, double sekunden) {
    const int ticks = static_cast<int>(std::lround(sekunden / Testwelt::zeitschritt));
    for (int i = 0; i < ticks; ++i) {
        EingabePaket paket;
        paket.tick = tick++;
        welt.tick(paket);
    }
}

} // namespace

TEST_CASE("Ein abgesetztes Auto federt ein und kommt zur Ruhe") {
    Testwelt welt(sportcoupe(), 0.10);
    std::uint32_t tick = 0;
    const double hoehe_beim_absetzen = welt.karosserie().position.z;

    stehe(welt, tick, 4.0);

    CHECK(welt.karosserie().position.z < hoehe_beim_absetzen - 0.10);
    for (double federweg : welt.letzter_schritt().animationsdaten.federweg) {
        CHECK(federweg > 0.03);
        CHECK(federweg < 0.10);
    }

    // Danach bewegt es sich über mehrere Sekunden praktisch nicht.
    const Vec3 ruhelage = welt.karosserie().position;
    double groesste_abweichung = 0.0;
    double groesste_geschwindigkeit = 0.0;
    double groesste_drehgeschwindigkeit = 0.0;
    for (int sekunde = 0; sekunde < 5; ++sekunde) {
        for (int i = 0; i < 120; ++i) {
            stehe(welt, tick, Testwelt::zeitschritt);
            const Karosseriebewegung& k = welt.karosserie();
            groesste_abweichung = std::max(groesste_abweichung, betrag(k.position - ruhelage));
            groesste_geschwindigkeit = std::max(groesste_geschwindigkeit, betrag(k.geschwindigkeit));
            groesste_drehgeschwindigkeit =
                std::max(groesste_drehgeschwindigkeit, betrag(k.drehgeschwindigkeit));
        }
    }
    CHECK(groesste_abweichung < 1e-4);
    CHECK(groesste_geschwindigkeit < 1e-4);
    CHECK(groesste_drehgeschwindigkeit < 1e-4);
}

TEST_CASE("Im Stand stimmen die Achslasten mit der statischen Verteilung überein") {
    const Kennwerte auto_ = sportcoupe();
    Testwelt welt(auto_, 0.10);
    std::uint32_t tick = 0;
    stehe(welt, tick, 4.0);

    // Beckman: Lv = G·b/L und Lh = G·a/L, mit a und b als Abstand des
    // Schwerpunkts zur Vorder- und Hinterachse und L als Radstand.
    const double gewicht = auto_.masse * Testwelt::erdbeschleunigung;
    const double radstand = auto_.vorn.x - auto_.hinten.x;
    const double a = auto_.vorn.x - auto_.schwerpunkt.x;
    const double b = auto_.schwerpunkt.x - auto_.hinten.x;

    const auto& radlast = welt.letzter_schritt().telemetrie.radlast;
    CHECK(radlast[vorn_links] + radlast[vorn_rechts] ==
          doctest::Approx(gewicht * b / radstand).epsilon(0.01));
    CHECK(radlast[hinten_links] + radlast[hinten_rechts] ==
          doctest::Approx(gewicht * a / radstand).epsilon(0.01));
    CHECK(radlast[vorn_links] == doctest::Approx(radlast[vorn_rechts]).epsilon(0.01));
    CHECK(radlast[hinten_links] == doctest::Approx(radlast[hinten_rechts]).epsilon(0.01));
}

TEST_CASE("Zwei Läufe mit denselben Eingabe-Paketen ergeben dieselbe Prüfsumme") {
    Testwelt erster_lauf(sportcoupe(), 0.10);
    Testwelt zweiter_lauf(sportcoupe(), 0.10);
    Testwelt anderer_lauf(sportcoupe(), 0.12);
    std::uint32_t tick_erster = 0, tick_zweiter = 0, tick_anderer = 0;

    stehe(erster_lauf, tick_erster, 2.0);
    stehe(zweiter_lauf, tick_zweiter, 2.0);
    stehe(anderer_lauf, tick_anderer, 2.0);

    CHECK(erster_lauf.pruefsumme() == zweiter_lauf.pruefsumme());
    CHECK(erster_lauf.pruefsumme() != anderer_lauf.pruefsumme());

    stehe(erster_lauf, tick_erster, Testwelt::zeitschritt);
    CHECK(erster_lauf.pruefsumme() != zweiter_lauf.pruefsumme());
}
