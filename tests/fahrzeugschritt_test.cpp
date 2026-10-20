#include "doctest/doctest.h"

#include <bit>
#include <cmath>
#include <numbers>
#include <vector>

#include "fahrt.h"
#include "ultragravity/fahrzeugschritt.h"

using namespace ultragravity;
using ultragravity::test::sportcoupe;

namespace {

bool bitgleich(double a, double b) { return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b); }
bool bitgleich(Vec3 a, Vec3 b) { return bitgleich(a.x, b.x) && bitgleich(a.y, b.y) && bitgleich(a.z, b.z); }

// Jedes Rad steht 4 cm eingefedert auf ebenem Boden.
Radkontakte eingefedert(const Kennwerte& kennwerte, const Karosseriebewegung& karosserie) {
    Radkontakte kontakte;
    const auto strahlen = abtaststrahlen(kennwerte, karosserie);
    for (int i = 0; i < anzahl_raeder; ++i) {
        const Abtaststrahl& s = strahlen[i];
        kontakte[i] = {true, s.start + s.richtung * (s.laenge - 0.04), {0.0, 0.0, 1.0}, 1.0};
    }
    return kontakte;
}

// Rollt mit fester Karosseriebewegung Tick für Tick weiter und liefert das
// Ergebnis jedes Ticks. Die Karosserie ist dafür tausendmal so schwer, damit die
// Zwischenschritte sie nicht merklich abbremsen.
std::vector<SchrittErgebnis> rolle(Kennwerte kennwerte, const Karosseriebewegung& karosserie, int ticks,
                                   const EingabePaket& eingabe = {}) {
    kennwerte.masse *= 1000.0;
    kennwerte.traegheit = kennwerte.traegheit * 1000.0;
    const Radkontakte kontakte = eingefedert(kennwerte, karosserie);
    Zustand zustand = rollender_zustand(kennwerte, karosserie.geschwindigkeit.x);
    std::vector<SchrittErgebnis> ergebnisse;
    for (int i = 0; i < ticks; ++i) {
        ergebnisse.push_back(fahrzeugschritt(zustand, kennwerte, eingabe, kontakte, karosserie, 1.0 / 120.0));
        zustand = ergebnisse.back().zustand;
    }
    return ergebnisse;
}

// Seitenkraft auf die Karosserie nach jedem Tick.
std::vector<double> seitenkraefte(const Kennwerte& kennwerte, const Karosseriebewegung& karosserie, int ticks) {
    std::vector<double> kraefte; // N
    for (const SchrittErgebnis& e : rolle(kennwerte, karosserie, ticks)) kraefte.push_back(e.kraefte.kraft.y);
    return kraefte;
}

} // namespace

TEST_CASE("Der Fahrzeug-Schritt liefert bei gleichem Eingang den gleichen Ausgang") {
    const Kennwerte kennwerte = sportcoupe();

    Karosseriebewegung karosserie;
    karosserie.position = {0.0, 0.0, 0.5};
    karosserie.geschwindigkeit = {12.0, 0.0, -0.3};
    karosserie.drehgeschwindigkeit = {0.05, -0.02, 0.0};

    const Radkontakte kontakte = eingefedert(kennwerte, karosserie);

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

TEST_CASE("Die Seitenkraft baut sich über die Relaxationslänge auf") {
    const Kennwerte kennwerte = sportcoupe();
    const double sigma = kennwerte.vorn.reifen.relaxationslaenge; // m
    REQUIRE(kennwerte.hinten.reifen.relaxationslaenge == sigma);

    // Die Karosserie schiebt mit 1° Schräglauf, gerade so schnell, dass sie in
    // 15 Ticks eine Relaxationslänge rollt. Bei 1° ist der Reifen fast linear.
    const double ticks_je_sigma = 15.0; // Ticks je Relaxationslänge
    const double vx = sigma * 120.0 / ticks_je_sigma; // m/s
    Karosseriebewegung karosserie;
    karosserie.position = {0.0, 0.0, 0.5};
    karosserie.geschwindigkeit = {vx, -vx * std::tan(std::numbers::pi / 180.0), 0.0};

    const std::vector<double> kraft = seitenkraefte(kennwerte, karosserie, 240); // N
    const double stationaer = kraft.back(); // N
    CAPTURE(stationaer);
    REQUIRE(stationaer > 0.0);

    // Verzögerungsglied erster Ordnung über der Rollstrecke: 1 − e^(−s/σ).
    CHECK(kraft[0] < 0.2 * stationaer);
    CHECK(kraft[14] / stationaer == doctest::Approx(1.0 - std::exp(-1.0)).epsilon(0.05));
    CHECK(kraft[44] / stationaer == doctest::Approx(1.0 - std::exp(-3.0)).epsilon(0.02));
}

TEST_CASE("Auch im Stand baut sich die Seitenkraft auf, über eine Untergrenze der Geschwindigkeit") {
    const Kennwerte kennwerte = sportcoupe();
    Karosseriebewegung karosserie;
    karosserie.position = {0.0, 0.0, 0.5};
    karosserie.geschwindigkeit = {0.0, -0.02, 0.0};

    const std::vector<double> kraft = seitenkraefte(kennwerte, karosserie, 240); // N
    const double stationaer = kraft.back(); // N
    CAPTURE(stationaer);
    REQUIRE(stationaer > 0.0);

    // Ohne Untergrenze bliebe die Kraft im Stand bei null stehen; mit ihr baut sie
    // sich verzögert, aber vollständig auf.
    CHECK(kraft[0] < 0.5 * stationaer);
    CHECK(kraft[119] > 0.95 * stationaer);
}

TEST_CASE("Bei großem Schräglauf zählt für die Relaxation die ganze Rutschgeschwindigkeit") {
    const Kennwerte kennwerte = sportcoupe();
    const double sigma = kennwerte.vorn.reifen.relaxationslaenge; // m

    // Die Karosserie rutscht mit 30° schräg, so schnell, dass sie in 15 Ticks
    // eine Relaxationslänge zurücklegt. Längs allein wären es nur 0,87 davon.
    const double tempo = sigma * 120.0 / 15.0;                  // m/s
    const double winkel = 30.0 * std::numbers::pi / 180.0;      // rad
    Karosseriebewegung karosserie;
    karosserie.position = {0.0, 0.0, 0.5};
    karosserie.geschwindigkeit = {tempo * std::cos(winkel), -tempo * std::sin(winkel), 0.0};

    const std::vector<SchrittErgebnis> ticks = rolle(kennwerte, karosserie, 240);
    const double stationaer = ticks.back().zustand.schraeglauf[hinten_links]; // rad
    CAPTURE(stationaer);
    REQUIRE(stationaer < -0.3);

    // Über die Rutschstrecke 1 − e^(−s/σ); über die Längsstrecke wären es nach
    // einer Relaxationslänge nur 1 − e^(−0,87) = 0,58.
    CHECK(ticks[14].zustand.schraeglauf[hinten_links] / stationaer ==
          doctest::Approx(1.0 - std::exp(-1.0)).epsilon(0.03));
}

TEST_CASE("Ohne Bodenkontakt vergisst der Reifen seinen Schräglauf") {
    const Kennwerte kennwerte = sportcoupe();
    Karosseriebewegung karosserie;
    karosserie.position = {0.0, 0.0, 0.5};
    karosserie.geschwindigkeit = {10.0, -1.0, 0.0};

    Zustand zustand = rolle(kennwerte, karosserie, 60).back().zustand;
    REQUIRE(zustand.schraeglauf[vorn_links] < -0.05);
    const Radkontakte in_der_luft{};
    zustand = fahrzeugschritt(zustand, kennwerte, {}, in_der_luft, karosserie, 1.0 / 120.0).zustand;

    for (int i = 0; i < anzahl_raeder; ++i) CHECK(zustand.schraeglauf[i] == 0.0);
}

TEST_CASE("Schräglauf schwächt die Längskraft eines blockierten Rads (kombinierter Schlupf)") {
    const Kennwerte kennwerte = sportcoupe();
    REQUIRE(kennwerte.vorn.reifen.kombiniert.laengs == 8.0);
    REQUIRE(kennwerte.hinten.reifen.kombiniert.laengs == 8.0);
    EingabePaket vollbremsung;
    vollbremsung.bremse = 1.0;

    // Längskraft, wenn die Räder blockiert über den Boden schieben und der Reifen
    // sich auf den Schräglauf eingestellt hat.
    const auto bremskraft = [&](double schraeglauf) {
        Karosseriebewegung karosserie;
        karosserie.position = {0.0, 0.0, 0.5};
        karosserie.geschwindigkeit = {10.0, -10.0 * std::tan(schraeglauf), 0.0};
        return rolle(kennwerte, karosserie, 120, vollbremsung).back().kraefte.kraft.x; // N
    };

    const double gerade = bremskraft(0.0);                               // N
    const double schraeg = bremskraft(5.0 * std::numbers::pi / 180.0);   // N
    CAPTURE(gerade);
    CAPTURE(schraeg);
    REQUIRE(gerade < 0.0);
    // Gewichtung 1/√(1 + (B·α)²) mit B = 8 und α ≈ 5° (der Schlupfnenner macht
    // daraus 4,97°): 0,82.
    CHECK(schraeg / gerade == doctest::Approx(0.82).epsilon(0.01));
}

TEST_CASE("Der Stabilisator verschiebt Last nur zwischen Rädern, die auf dem Boden stehen") {
    const Kennwerte mit = sportcoupe();
    Kennwerte ohne = mit;
    ohne.vorn.fahrwerk.stabilisator = 0.0;
    ohne.hinten.fahrwerk.stabilisator = 0.0;
    Karosseriebewegung karosserie;
    karosserie.position = {0.0, 0.0, 0.5};

    // Rechts 2 cm tiefer eingefedert als links.
    Radkontakte kontakte = eingefedert(mit, karosserie);
    for (int i : {vorn_rechts, hinten_rechts}) kontakte[i].punkt.z += 0.02;
    const auto radlasten = [&](const Kennwerte& k, const Radkontakte& r) {
        return fahrzeugschritt(rollender_zustand(k, 0.0), k, {}, r, karosserie, 1.0 / 120.0).telemetrie.radlast;
    };

    // Beide Räder unten: Rechts trägt mehr, die Summe der Achse bleibt.
    const auto mit_last = radlasten(mit, kontakte);   // N
    const auto ohne_last = radlasten(ohne, kontakte); // N
    CHECK(mit_last[vorn_rechts] > ohne_last[vorn_rechts] + 100.0);
    CHECK(mit_last[vorn_links] + mit_last[vorn_rechts] ==
          doctest::Approx(ohne_last[vorn_links] + ohne_last[vorn_rechts]));

    // Links in der Luft: Der Stabilisator hat nichts, gegen das er sich abstützt.
    kontakte[vorn_links].kontakt = false;
    kontakte[hinten_links].kontakt = false;
    const auto abgehoben_mit = radlasten(mit, kontakte);   // N
    const auto abgehoben_ohne = radlasten(ohne, kontakte); // N
    CHECK(abgehoben_mit[vorn_rechts] == doctest::Approx(abgehoben_ohne[vorn_rechts]));
    CHECK(abgehoben_mit[hinten_rechts] == doctest::Approx(abgehoben_ohne[hinten_rechts]));
}
