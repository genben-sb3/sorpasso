#include "doctest/doctest.h"

#include <bit>

#include "ultragravity/fahrzeugschritt.h"

using namespace ultragravity;

namespace {

bool bitgleich(double a, double b) { return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b); }
bool bitgleich(Vec3 a, Vec3 b) { return bitgleich(a.x, b.x) && bitgleich(a.y, b.y) && bitgleich(a.z, b.z); }

} // namespace

TEST_CASE("Der Fahrzeug-Schritt liefert bei gleichem Eingang den gleichen Ausgang") {
    const LadeErgebnis geladen = lade_fahrzeugdatei(UG_FAHRZEUGE_DIR "/sportcoupe.json");
    REQUIRE_FALSE(geladen.fehler.has_value());
    const Kennwerte& kennwerte = geladen.kennwerte;

    Karosseriebewegung karosserie;
    karosserie.position = {0.0, 0.0, 0.5};
    karosserie.geschwindigkeit = {12.0, 0.0, -0.3};
    karosserie.drehgeschwindigkeit = {0.05, -0.02, 0.0};

    // Jedes Rad steht 4 cm eingefedert auf ebenem Boden.
    Radkontakte kontakte;
    const auto strahlen = abtaststrahlen(kennwerte, karosserie);
    for (int i = 0; i < anzahl_raeder; ++i) {
        const Abtaststrahl& s = strahlen[i];
        kontakte[i] = {true, s.start + s.richtung * (s.laenge - 0.04), {0.0, 0.0, 1.0}, 1.0};
    }

    // Die Räder drehen etwas zu langsam, gebremst wird halb: Reifen und Bremse arbeiten.
    const Zustand zustand = rollender_zustand(kennwerte, 11.0);
    EingabePaket eingabe;
    eingabe.bremse = 0.5;

    const SchrittErgebnis erster = fahrzeugschritt(zustand, kennwerte, eingabe, kontakte, karosserie, 1.0 / 120.0);
    const SchrittErgebnis zweiter = fahrzeugschritt(zustand, kennwerte, eingabe, kontakte, karosserie, 1.0 / 120.0);

    CHECK(erster.kraefte.kraft.z > 0.0);
    CHECK(erster.kraefte.kraft.x < 0.0);
    CHECK(bitgleich(erster.kraefte.kraft, zweiter.kraefte.kraft));
    CHECK(bitgleich(erster.kraefte.moment, zweiter.kraefte.moment));
    for (int i = 0; i < anzahl_raeder; ++i) {
        CHECK(bitgleich(erster.telemetrie.radlast[i], zweiter.telemetrie.radlast[i]));
        CHECK(bitgleich(erster.animationsdaten.federweg[i], zweiter.animationsdaten.federweg[i]));
        CHECK(bitgleich(erster.zustand.raddrehzahl[i], zweiter.zustand.raddrehzahl[i]));
    }
}
