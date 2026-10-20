#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "fahrt.h"
#include "ultragravity/animationsdaten.h"
#include "ultragravity/pruefsumme.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

constexpr double pi = std::numbers::pi;

Radkontakte eingefedert(const Kennwerte& kennwerte, const Karosseriebewegung& karosserie, double einfederung = 0.04) {
    Radkontakte kontakte;
    const auto strahlen = abtaststrahlen(kennwerte, karosserie);
    for (int i = 0; i < anzahl_raeder; ++i) {
        const Abtaststrahl& s = strahlen[i];
        kontakte[i] = {true, s.start + s.richtung * (s.laenge - einfederung), {0.0, 0.0, 1.0}, 1.0};
    }
    return kontakte;
}

} // namespace

TEST_CASE("Beim Bremsen federn die Vorderräder tiefer ein als die hinteren und die Karosserie nickt nach vorn") {
    Testwelt welt(sportcoupe(), 0.02, Testgelaende{}, hundert_kmh);
    Fahrt fahrt(welt);
    fahrt.fahre(1.5);
    const Animationsdaten rollend = welt.letzter_schritt().animationsdaten;
    CHECK(std::abs(rollend.nicken) < 0.02);

    EingabePaket bremse;
    bremse.bremse = 1.0;
    fahrt.fahre(1.0, bremse);
    const Animationsdaten a = welt.letzter_schritt().animationsdaten;

    CHECK(a.nicken > 0.01); // Nase nach unten ist positiv (ISO 8855)
    CHECK(a.federweg[vorn_links] > rollend.federweg[vorn_links] + 0.01);
    CHECK(a.federweg[hinten_links] < rollend.federweg[hinten_links] - 0.005);
    CHECK(a.federweg[vorn_rechts] == doctest::Approx(a.federweg[vorn_links]).epsilon(0.05));
    // Das Nicken ist die Neigung der Längsachse.
    const Vec3 nase = drehe(welt.karosserie().ausrichtung, {1.0, 0.0, 0.0});
    CHECK(a.nicken == doctest::Approx(std::asin(-nase.z)));
}

TEST_CASE("In der Linkskurve stehen die Vorderräder und das Lenkrad nach links, die Karosserie wankt nach außen") {
    const Kennwerte auto_ = sportcoupe();
    Testwelt welt(auto_, 0.02, Testgelaende{}, 20.0);
    Fahrt fahrt(welt);
    fahrt.fahre(1.0);

    EingabePaket links;
    links.lenkung = 0.5;
    links.gas = 0.3;
    links.gangwunsch = 4;
    fahrt.fahre(3.0, links);
    const Animationsdaten a = welt.letzter_schritt().animationsdaten;

    CHECK(a.lenkwinkel[vorn_links] == doctest::Approx(0.5 * auto_.lenkung.max_lenkwinkel));
    CHECK(a.lenkwinkel[vorn_rechts] == doctest::Approx(0.5 * auto_.lenkung.max_lenkwinkel));
    CHECK(a.lenkwinkel[hinten_links] == 0.0);
    CHECK(a.lenkwinkel[hinten_rechts] == 0.0);
    CHECK(a.lenkrad == doctest::Approx(0.5 * 2.5 * pi)); // 225° bei halbem Ausschlag
    CHECK(a.wanken > 0.0);                               // links oben, rechts unten
    CHECK(a.federweg[vorn_rechts] > a.federweg[vorn_links]);
    CHECK(a.drehzahl == doctest::Approx(welt.letzter_schritt().zustand.motordrehzahl));
}

TEST_CASE("Der Raddrehwinkel folgt der Raddrehzahl und bleibt im Bereich -π bis π") {
    const Kennwerte kennwerte = sportcoupe();
    Karosseriebewegung karosserie;
    karosserie.position = {0.0, 0.0, 0.5};
    karosserie.geschwindigkeit = {30.0, 0.0, 0.0};
    const Radkontakte kontakte = eingefedert(kennwerte, karosserie);
    Zustand zustand = rollender_zustand(kennwerte, 30.0);

    const double dt = 1.0 / 120.0;
    int ueberlaeufe = 0;
    double vorher = 0.0;
    for (int tick = 0; tick < 240; ++tick) {
        const SchrittErgebnis e = fahrzeugschritt(zustand, kennwerte, {}, kontakte, karosserie, dt);
        const double winkel = e.animationsdaten.raddrehwinkel[vorn_links];
        CHECK(winkel >= -pi);
        CHECK(winkel < pi);
        // Pro Tick dreht sich das Rad um etwa ω·dt.
        const double schritt = wickle_winkel(winkel - vorher);
        CHECK(schritt == doctest::Approx(e.zustand.raddrehzahl[vorn_links] * dt).epsilon(0.02));
        if (winkel < vorher) ++ueberlaeufe;
        vorher = winkel;
        zustand = e.zustand;
    }
    CHECK(ueberlaeufe > 10);
}

TEST_CASE("Die Interpolation liefert bei Anteil 0 und 1 genau die beiden Ticks") {
    Animationsdaten a, b;
    a.raddrehwinkel = {0.1, 3.0, -2.0, 0.0};
    b.raddrehwinkel = {0.9, -3.0, -1.0, 0.5};
    a.federweg = {0.03, 0.04, 0.05, 0.06};
    b.federweg = {0.07, 0.08, 0.09, 0.1};
    a.lenkwinkel[vorn_links] = 0.1;
    b.lenkwinkel[vorn_links] = 0.3;
    a.nicken = 0.01;
    b.nicken = 0.03;
    a.wanken = -0.02;
    b.wanken = 0.02;
    a.lenkrad = 1.0;
    b.lenkrad = 2.0;
    a.drehzahl = 300.0;
    b.drehzahl = 400.0;

    const auto gleich = [](const Animationsdaten& x, const Animationsdaten& y) {
        for (int i = 0; i < anzahl_raeder; ++i) {
            CHECK(x.raddrehwinkel[i] == y.raddrehwinkel[i]);
            CHECK(x.lenkwinkel[i] == y.lenkwinkel[i]);
            CHECK(x.federweg[i] == y.federweg[i]);
        }
        CHECK(x.nicken == y.nicken);
        CHECK(x.wanken == y.wanken);
        CHECK(x.lenkrad == y.lenkrad);
        CHECK(x.drehzahl == y.drehzahl);
    };
    gleich(interpoliere(a, b, 0.0), a);
    gleich(interpoliere(a, b, 1.0), b);

    const Animationsdaten halb = interpoliere(a, b, 0.5);
    CHECK(halb.federweg[0] == doctest::Approx(0.05));
    CHECK(halb.nicken == doctest::Approx(0.02));
    CHECK(halb.drehzahl == doctest::Approx(350.0));
    CHECK(halb.raddrehwinkel[0] == doctest::Approx(0.5));
}

TEST_CASE("Raddrehwinkel springen in der Interpolation nicht beim Überlauf") {
    Animationsdaten a, b;
    a.raddrehwinkel[0] = pi - 0.1;
    b.raddrehwinkel[0] = -pi + 0.3; // das Rad hat π überschritten und dreht 0,4 weiter

    double vorher = a.raddrehwinkel[0];
    for (int i = 1; i <= 40; ++i) {
        const double winkel = interpoliere(a, b, i / 40.0).raddrehwinkel[0];
        CHECK(winkel >= -pi);
        CHECK(winkel < pi);
        CHECK(std::abs(wickle_winkel(winkel - vorher)) == doctest::Approx(0.4 / 40.0).epsilon(1e-6));
        vorher = winkel;
    }
}

TEST_CASE("Die Interpolation zwischen echten Ticks ist stetig") {
    Testwelt welt(sportcoupe(), 0.02, Testgelaende{}, hundert_kmh);
    Fahrt fahrt(welt);
    fahrt.fahre(1.0);
    EingabePaket paket;
    paket.bremse = 0.5;
    paket.lenkung = 0.2;
    fahrt.einen_tick(paket);
    const Animationsdaten a = welt.letzter_schritt().animationsdaten;
    fahrt.einen_tick(paket);
    const Animationsdaten b = welt.letzter_schritt().animationsdaten;

    Animationsdaten vorher = a;
    for (int i = 1; i <= 100; ++i) {
        const Animationsdaten x = interpoliere(a, b, i / 100.0);
        for (int r = 0; r < anzahl_raeder; ++r) {
            CHECK(std::abs(wickle_winkel(x.raddrehwinkel[r] - vorher.raddrehwinkel[r])) < 0.1);
            CHECK(std::abs(x.federweg[r] - vorher.federweg[r]) < 0.005);
        }
        vorher = x;
    }
}

TEST_CASE("Ohne Bodenkontakt eines Rads entsteht an diesem Rad keine Reifenkraft") {
    const Kennwerte kennwerte = sportcoupe();
    Karosseriebewegung karosserie;
    karosserie.position = {0.0, 0.0, 0.5};
    karosserie.geschwindigkeit = {20.0, 3.0, 0.0};
    const Radkontakte kontakte = eingefedert(kennwerte, karosserie);
    const Zustand zustand = rollender_zustand(kennwerte, 15.0); // Räder rutschen: starker Schlupf
    EingabePaket eingabe;
    eingabe.bremse = 1.0;

    // Alle Räder in der Luft: keine Kraft, kein Moment.
    Radkontakte luft = kontakte;
    for (Radkontakt& k : luft) k.kontakt = false;
    const SchrittErgebnis frei = fahrzeugschritt(zustand, kennwerte, eingabe, luft, karosserie, 1.0 / 120.0);
    CHECK(frei.kraefte.kraft.x == 0.0);
    CHECK(frei.kraefte.kraft.y == 0.0);
    CHECK(frei.kraefte.kraft.z == 0.0);
    CHECK(frei.kraefte.moment.x == 0.0);
    CHECK(frei.kraefte.moment.y == 0.0);
    CHECK(frei.kraefte.moment.z == 0.0);
    for (int i = 0; i < anzahl_raeder; ++i) {
        CHECK(frei.animationsdaten.federweg[i] == 0.0);
        CHECK(frei.telemetrie.radlast[i] == 0.0);
    }

    // Nur ein Rad in der Luft: Ohne Bremse behält es seine Drehzahl, während das
    // Rad mit Kontakt vom Reifen beschleunigt wird.
    eingabe.bremse = 0.0;
    Radkontakte drei = kontakte;
    drei[hinten_links].kontakt = false;
    const SchrittErgebnis e3 = fahrzeugschritt(zustand, kennwerte, eingabe, drei, karosserie, 1.0 / 120.0);
    const SchrittErgebnis e4 = fahrzeugschritt(zustand, kennwerte, eingabe, kontakte, karosserie, 1.0 / 120.0);
    CHECK(e3.zustand.raddrehzahl[hinten_links] == zustand.raddrehzahl[hinten_links]);
    CHECK(e4.zustand.raddrehzahl[hinten_links] != zustand.raddrehzahl[hinten_links]);
    CHECK(e3.telemetrie.radlast[hinten_links] == 0.0);
    CHECK(e3.kraefte.kraft.x > e4.kraefte.kraft.x); // Das rutschende Rad bremst nicht mehr mit
}

TEST_CASE("Ein Fahrzeug-Schritt verändert nur sein Ergebnis und hat keinen verborgenen Zustand") {
    const Kennwerte kennwerte = sportcoupe();
    Karosseriebewegung karosserie;
    karosserie.position = {0.0, 0.0, 0.5};
    karosserie.geschwindigkeit = {12.0, 1.0, -0.3};
    const Radkontakte kontakte = eingefedert(kennwerte, karosserie);
    const Zustand zustand = rollender_zustand(kennwerte, 11.0);
    EingabePaket eingabe;
    eingabe.lenkung = 0.3;
    eingabe.gas = 0.5;
    eingabe.gangwunsch = 1;

    const std::uint64_t summe_vorher = pruefsumme(zustand, karosserie);
    const Radkontakte kontakte_vorher = kontakte;

    const SchrittErgebnis erster = fahrzeugschritt(zustand, kennwerte, eingabe, kontakte, karosserie, 1.0 / 120.0);
    CHECK(pruefsumme(zustand, karosserie) == summe_vorher);
    for (int i = 0; i < anzahl_raeder; ++i) {
        CHECK(kontakte[i].kontakt == kontakte_vorher[i].kontakt);
        CHECK(kontakte[i].punkt.z == kontakte_vorher[i].punkt.z);
    }

    // Ein anderer Schritt dazwischen (anderes Auto, anderer Zustand, andere Eingabe)
    // ändert den Folgeschritt mit denselben Eingängen nicht.
    Kennwerte anderes = kennwerte;
    anderes.masse *= 1.5;
    Karosseriebewegung schnell = karosserie;
    schnell.geschwindigkeit = {40.0, -4.0, 0.0};
    EingabePaket bremse;
    bremse.bremse = 1.0;
    (void)fahrzeugschritt(rollender_zustand(anderes, 40.0), anderes, bremse, eingefedert(anderes, schnell, 0.08),
                          schnell, 1.0 / 60.0);

    const SchrittErgebnis zweiter = fahrzeugschritt(zustand, kennwerte, eingabe, kontakte, karosserie, 1.0 / 120.0);
    CHECK(pruefsumme(erster.zustand, karosserie) == pruefsumme(zweiter.zustand, karosserie));
    CHECK(erster.kraefte.kraft.x == zweiter.kraefte.kraft.x);
    CHECK(erster.kraefte.moment.z == zweiter.kraefte.moment.z);
    for (int i = 0; i < anzahl_raeder; ++i) {
        CHECK(erster.animationsdaten.raddrehwinkel[i] == zweiter.animationsdaten.raddrehwinkel[i]);
        CHECK(erster.animationsdaten.federweg[i] == zweiter.animationsdaten.federweg[i]);
    }
    CHECK(erster.animationsdaten.nicken == zweiter.animationsdaten.nicken);
    CHECK(erster.animationsdaten.lenkrad == zweiter.animationsdaten.lenkrad);
}
