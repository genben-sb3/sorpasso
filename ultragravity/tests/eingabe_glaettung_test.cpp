#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>

#include "fahrt.h"
#include "ultragravity/eingabe_glaettung.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

constexpr double dt = Testwelt::zeitschritt; // s

Karosseriebewegung geradeaus(double tempo) {
    Karosseriebewegung k;
    k.geschwindigkeit = {tempo, 0.0, 0.0};
    return k;
}

// Läuft n Sekunden mit gleichem Rohsignal und liefert den letzten Zustand.
GlaettungsErgebnis laufe(const Kennwerte& auto_, const Karosseriebewegung& k, const RohEingabe& roh, double sekunden,
                         GlaettungsZustand start = {}) {
    GlaettungsErgebnis e;
    e.zustand = start;
    for (int i = 0; i < Fahrt::ticks(sekunden); ++i) e = glaette(e.zustand, GlaettungsParameter{}, auto_, k, roh, 0, dt);
    return e;
}

} // namespace

TEST_CASE("Ein voller Tastendruck baut die Lenkung über die eingestellte Zeit auf") {
    const Kennwerte auto_ = sportcoupe();
    const GlaettungsParameter p;
    RohEingabe roh;
    roh.lenkung = 1.0;
    const Karosseriebewegung langsam = geradeaus(2.0); // Grenze liegt dort bei 1

    CHECK(laufe(auto_, langsam, roh, p.lenk_aufbauzeit / 2.0).paket.lenkung == doctest::Approx(0.5).epsilon(0.02));
    CHECK(laufe(auto_, langsam, roh, p.lenk_aufbauzeit * 1.05).paket.lenkung == doctest::Approx(1.0));
}

TEST_CASE("Loslassen stellt die Lenkung schneller zurück, als sie aufgebaut wurde") {
    const Kennwerte auto_ = sportcoupe();
    const GlaettungsParameter p;
    const Karosseriebewegung langsam = geradeaus(2.0);
    RohEingabe los;

    const GlaettungsErgebnis e = laufe(auto_, langsam, los, p.lenk_abbauzeit / 2.0, {1.0, 0.0, 0.0});
    CHECK(e.paket.lenkung == doctest::Approx(0.5).epsilon(0.02));
    CHECK(p.lenk_abbauzeit < p.lenk_aufbauzeit);
    CHECK(laufe(auto_, langsam, los, p.lenk_abbauzeit * 1.05, {1.0, 0.0, 0.0}).paket.lenkung == doctest::Approx(0.0));
}

TEST_CASE("Gas und Bremse steigen bei digitaler Eingabe weich an") {
    const Kennwerte auto_ = sportcoupe();
    const GlaettungsParameter p;
    RohEingabe roh;
    roh.gas = 1.0;
    roh.bremse = 1.0;

    const GlaettungsErgebnis erster = glaette({}, p, auto_, geradeaus(10.0), roh, 0, dt);
    CHECK(erster.paket.gas > 0.0);
    CHECK(erster.paket.gas < 0.1);
    CHECK(erster.paket.bremse > 0.0);
    CHECK(erster.paket.bremse < 0.1);

    const GlaettungsErgebnis halb = laufe(auto_, geradeaus(10.0), roh, p.gas_aufbauzeit / 2.0);
    CHECK(halb.paket.gas == doctest::Approx(0.5).epsilon(0.02));
    CHECK(laufe(auto_, geradeaus(10.0), roh, 1.0).paket.gas == doctest::Approx(1.0));

    RohEingabe los;
    CHECK(laufe(auto_, geradeaus(10.0), los, p.gas_abbauzeit * 1.05, {0.0, 1.0, 1.0}).paket.gas == doctest::Approx(0.0));
}

TEST_CASE("Die Lenkgrenze sinkt mit der Geschwindigkeit bis zum Peak-Schräglaufwinkel") {
    const Kennwerte auto_ = sportcoupe();
    CHECK(lenkgrenze(auto_, 2.0, 1.0) == doctest::Approx(1.0));
    CHECK(lenkgrenze(auto_, 15.0, 1.0) < lenkgrenze(auto_, 8.0, 1.0));
    CHECK(lenkgrenze(auto_, 40.0, 1.0) < lenkgrenze(auto_, 15.0, 1.0));
    // Quer-Kurve des Coupés: B = 14, C = 1,5, E = 0,3, Peak bei etwa 0,12 rad.
    const double winkel = lenkgrenze(auto_, 100.0, 1.0) * auto_.lenkung.max_lenkwinkel; // rad
    CHECK(winkel > 0.10);
    CHECK(winkel < 0.17);
}

TEST_CASE("Bei hoher Geschwindigkeit lenkt ein voller Tastendruck nicht über den Grip hinaus") {
    const Kennwerte auto_ = sportcoupe();
    const double tempo = 150.0 / 3.6; // m/s
    Testwelt welt(auto_, 0.02, Testgelaende{}, tempo);
    Fahrt fahrt(welt);
    RohEingabe roh;
    roh.lenkung = 1.0;
    roh.gangwunsch = 5;

    GlaettungsZustand zustand;
    double hoechste_quer = 0.0;  // m/s²
    double hoechster_drift = 0.0; // rad
    for (int i = 0; i < Fahrt::ticks(4.0); ++i) {
        const Karosseriebewegung& k = welt.karosserie();
        roh.gas = 0.12; // etwa Rollen auf der Geraden, damit kein Gas das Heck antreibt
        const double grenze = lenkgrenze(auto_, std::hypot(k.geschwindigkeit.x, k.geschwindigkeit.y), GlaettungsParameter{}.peak_anteil);
        const GlaettungsErgebnis e = glaette(zustand, GlaettungsParameter{}, auto_, k, roh, 0, dt);
        zustand = e.zustand;
        fahrt.einen_tick(e.paket);
        const Vec3 v = drehe_zurueck(welt.karosserie().ausrichtung, welt.karosserie().geschwindigkeit);
        const Vec3 kraft = drehe_zurueck(welt.karosserie().ausrichtung, welt.letzter_schritt().kraefte.kraft);
        hoechste_quer = std::max(hoechste_quer, std::abs(kraft.y) / auto_.masse);
        hoechster_drift = std::max(hoechster_drift, std::abs(std::atan2(v.y, v.x)));
        CHECK(std::abs(e.paket.lenkung) <= grenze + 1e-3);
    }
    CAPTURE(hoechste_quer);
    CAPTURE(hoechster_drift);
    CHECK(lenkgrenze(auto_, tempo, GlaettungsParameter{}.peak_anteil) < 0.3);
    // Reibkreis: ein wenig über μ·g sind dank Abtrieb und Lastempfindlichkeit möglich.
    // Der Einlenkstoß überschwingt kurz (Trägheit, Relaxation), daher 1,3 statt 1,05.
    CHECK(hoechste_quer < 1.3 * 1.05 * Testwelt::erdbeschleunigung);
    // Ein Dreher geht gegen 90° und mehr; hier fängt sich das Auto.
    CHECK(hoechster_drift < 0.5);
}

TEST_CASE("Rutscht das Heck und ist die Lenktaste los, lenkt die Gegenlenkhilfe in Fahrtrichtung") {
    const Kennwerte auto_ = sportcoupe();
    RohEingabe los;

    // Fährt nach links aus der Längsachse (vy > 0): Heck ist nach rechts ausgebrochen.
    Karosseriebewegung links = geradeaus(20.0);
    links.geschwindigkeit = {20.0, 5.0, 0.0};
    CHECK(laufe(auto_, links, los, 0.3).paket.lenkung > 0.1);

    Karosseriebewegung rechts = geradeaus(20.0);
    rechts.geschwindigkeit = {20.0, -5.0, 0.0};
    CHECK(laufe(auto_, rechts, los, 0.3).paket.lenkung < -0.1);

    // Hält der Fahrer die Taste, gilt sein Wunsch, nicht die Hilfe.
    RohEingabe rechts_taste;
    rechts_taste.lenkung = -1.0;
    CHECK(laufe(auto_, links, rechts_taste, 0.3).paket.lenkung < 0.0);

    // Ohne Driftwinkel greift die Hilfe nicht.
    CHECK(laufe(auto_, geradeaus(20.0), los, 0.3).paket.lenkung == doctest::Approx(0.0));
}

TEST_CASE("Gleiche Rohsignale und gleicher Zustand ergeben dasselbe Eingabe-Paket") {
    const Kennwerte auto_ = sportcoupe();
    RohEingabe roh;
    roh.lenkung = 0.7;
    roh.gas = 0.6;
    roh.gangwunsch = 3;
    Karosseriebewegung k = geradeaus(30.0);
    k.geschwindigkeit.y = 2.0;
    const GlaettungsZustand z{0.2, 0.3, 0.1};

    const GlaettungsErgebnis a = glaette(z, GlaettungsParameter{}, auto_, k, roh, 7, dt);
    const GlaettungsErgebnis b = glaette(z, GlaettungsParameter{}, auto_, k, roh, 7, dt);
    CHECK(a.paket.lenkung == b.paket.lenkung);
    CHECK(a.paket.gas == b.paket.gas);
    CHECK(a.paket.bremse == b.paket.bremse);
    CHECK(a.paket.tick == 7);
    CHECK(a.paket.gangwunsch == 3);
}
