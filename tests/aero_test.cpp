#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>

#include "fahrt.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

constexpr double erdbeschleunigung = Testwelt::erdbeschleunigung; // m/s²

double abtriebskraft(double flaeche, double v) { return 0.5 * luftdichte * flaeche * v * v; } // N

double gesamtabtrieb(const Kennwerte& k, double v) { return abtriebskraft(k.vorn.abtrieb + k.hinten.abtrieb, v); } // N

// Fahrwiderstand im Geradeauslauf bei der Geschwindigkeit v: Luftwiderstand
// plus Rollwiderstand auf der Radlast samt Abtrieb.
double fahrwiderstand(const Kennwerte& k, double v) {
    const double luft = 0.5 * luftdichte * k.aero.luftwiderstand * v * v;
    const double rollen = k.hinten.reifen.rollwiderstand * (k.masse * erdbeschleunigung + gesamtabtrieb(k, v));
    return luft + rollen;
}

// Antriebskraft am Boden bei Vollgas im höchsten Gang, ohne Verluste.
double antriebskraft(const Kennwerte& k, double v) {
    const Motor& m = k.antrieb.motor;
    const double gesamt = k.antrieb.getriebe.uebersetzungen.back() * k.antrieb.getriebe.achsuebersetzung;
    const double drehzahl = v / k.hinten.radradius * gesamt;
    if (drehzahl >= m.hoechstdrehzahl) return 0.0;
    double moment = m.drehmoment.back().moment;
    for (std::size_t i = 1; i < m.drehmoment.size(); ++i) {
        if (drehzahl <= m.drehmoment[i].drehzahl) {
            const Stuetzpunkt& a = m.drehmoment[i - 1];
            const Stuetzpunkt& b = m.drehmoment[i];
            moment = a.moment + (b.moment - a.moment) * (drehzahl - a.drehzahl) / (b.drehzahl - a.drehzahl);
            break;
        }
    }
    return moment * gesamt / k.hinten.radradius;
}

} // namespace

TEST_CASE("Die Höchstgeschwindigkeit liegt dort, wo Antriebskraft und Fahrwiderstand im Gleichgewicht sind") {
    const Kennwerte auto_ = sportcoupe();

    // Gleichgewicht der Kräfte im sechsten Gang per Intervallhalbierung.
    double unten = 20.0; // m/s
    double oben = 100.0; // m/s
    for (int i = 0; i < 60; ++i) {
        const double mitte = (unten + oben) / 2.0;
        (antriebskraft(auto_, mitte) > fahrwiderstand(auto_, mitte) ? unten : oben) = mitte;
    }
    const double erwartet = unten;
    REQUIRE(erwartet * 3.6 > 250.0); // ein Sportcoupé mit 300 PS
    REQUIRE(erwartet * 3.6 < 300.0);

    Testwelt welt(auto_, 0.02);
    Fahrt fahrt(welt);
    fahrt.fahre(1.5);
    Testfahrer fahrer(auto_);
    double zeit = 0.0;
    while (fahrer.gang() < 6 || !fahrer.eingekuppelt()) {
        REQUIRE(zeit < 60.0);
        fahrt.einen_tick(fahrer.paket(welt));
        zeit += Testwelt::zeitschritt;
    }
    EingabePaket vollgas = fahrer.paket(welt);
    REQUIRE(vollgas.gas == 1.0);
    // Das Auto läuft nur asymptotisch auf die Höchstgeschwindigkeit zu.
    fahrt.fahre(120.0, vollgas);

    const double v = laengsgeschwindigkeit(welt);
    CAPTURE(v * 3.6);
    CAPTURE(erwartet * 3.6);
    // Die Reifen verlieren auf der Strecke etwas Schlupf an die Antriebskraft; 2 % reichen dafür.
    CHECK(v == doctest::Approx(erwartet).epsilon(0.02));
    // Ohne mehr Leistung wird es nicht schneller: die Beschleunigung ist weg.
    const double v_vorher = v;
    fahrt.fahre(5.0, vollgas);
    CHECK(laengsgeschwindigkeit(welt) - v_vorher < 0.3);
}

TEST_CASE("Ohne Gas und Bremse rollt das Auto im Leerlauf nach dem Fahrwiderstand aus") {
    const Kennwerte auto_ = sportcoupe();
    Testwelt welt(auto_, 0.02, Testgelaende{}, hundert_kmh);
    Fahrt fahrt(welt);
    fahrt.fahre(1.0); // einfedern lassen

    // Erwartung: dv/dt = −F_Widerstand(v)/m, einfach integriert.
    double v = laengsgeschwindigkeit(welt);
    double v_vorher = v;
    double erwartet = v;
    constexpr double dt = 0.005;
    const auto integriere = [&](double sekunden) {
        for (int i = 0; i < static_cast<int>(sekunden / dt); ++i) erwartet -= fahrwiderstand(auto_, erwartet) / auto_.masse * dt;
    };

    const double anfangsverzoegerung = fahrwiderstand(auto_, v) / auto_.masse;
    fahrt.fahre(1.0);
    CHECK(v_vorher - laengsgeschwindigkeit(welt) == doctest::Approx(anfangsverzoegerung).epsilon(0.1));

    // Über 40 s verzögert er immer sanfter, und die Geschwindigkeit folgt dem Modell.
    integriere(2.0);
    double letzte_abnahme = 1e9;
    for (int abschnitt = 0; abschnitt < 4; ++abschnitt) {
        v = laengsgeschwindigkeit(welt);
        fahrt.fahre(10.0);
        const double abnahme = v - laengsgeschwindigkeit(welt);
        CAPTURE(abschnitt);
        CHECK(abnahme > 0.0);
        CHECK(abnahme < letzte_abnahme);
        letzte_abnahme = abnahme;
        integriere(10.0);
    }
    CAPTURE(laengsgeschwindigkeit(welt));
    CAPTURE(erwartet);
    CHECK(laengsgeschwindigkeit(welt) == doctest::Approx(erwartet).epsilon(0.03));
    // Weder bleibt er sofort stehen noch rollt er ungebremst weiter.
    CHECK(laengsgeschwindigkeit(welt) > 0.5 * hundert_kmh);
    CHECK(laengsgeschwindigkeit(welt) < 0.9 * hundert_kmh);
}

TEST_CASE("Im Stand und bei langsamer Fahrt gibt es keinen Luftwiderstand und der Rollwiderstand schiebt nicht an") {
    const Kennwerte auto_ = sportcoupe();
    Testwelt welt(auto_, 0.02);
    Fahrt fahrt(welt);
    fahrt.fahre(3.0);
    CHECK(std::abs(laengsgeschwindigkeit(welt)) < 1e-3);
}

TEST_CASE("Der Abtrieb drückt die Achse nach unten, an der er angreift") {
    const Kennwerte ohne = [] {
        Kennwerte k = sportcoupe();
        k.vorn.abtrieb = 0.0;
        k.hinten.abtrieb = 0.0;
        return k;
    }();
    constexpr double v = 200.0 / 3.6; // m/s

    const auto achslasten = [&](const Kennwerte& k) {
        Testwelt welt(k, 0.02, Testgelaende{}, v);
        Fahrt fahrt(welt);
        fahrt.fahre(3.0);
        const auto& last = welt.letzter_schritt().telemetrie.radlast;
        return std::array<double, 2>{last[vorn_links] + last[vorn_rechts], last[hinten_links] + last[hinten_rechts]};
    };
    const auto basis = achslasten(ohne);

    SUBCASE("nur hinten") {
        Kennwerte k = ohne;
        k.hinten.abtrieb = 0.8;
        const auto last = achslasten(k);
        CHECK(last[1] - basis[1] == doctest::Approx(abtriebskraft(0.8, v)).epsilon(0.1));
        CHECK(last[0] - basis[0] == doctest::Approx(0.0).epsilon(0.1).scale(abtriebskraft(0.8, v)));
    }

    SUBCASE("nur vorn") {
        Kennwerte k = ohne;
        k.vorn.abtrieb = 0.8;
        const auto last = achslasten(k);
        CHECK(last[0] - basis[0] == doctest::Approx(abtriebskraft(0.8, v)).epsilon(0.1));
        CHECK(last[1] - basis[1] == doctest::Approx(0.0).epsilon(0.1).scale(abtriebskraft(0.8, v)));
    }

    SUBCASE("er wächst mit dem Quadrat der Geschwindigkeit") {
        Kennwerte k = ohne;
        k.hinten.abtrieb = 0.8;
        const double bei_v = achslasten(k)[1] - basis[1];
        CHECK(bei_v > 0.0);
        Testwelt langsam(k, 0.02, Testgelaende{}, v / 2.0);
        Testwelt langsam_ohne(ohne, 0.02, Testgelaende{}, v / 2.0);
        Fahrt f1(langsam);
        Fahrt f2(langsam_ohne);
        f1.fahre(3.0);
        f2.fahre(3.0);
        const auto summe = [](const Testwelt& w) {
            const auto& l = w.letzter_schritt().telemetrie.radlast;
            return l[hinten_links] + l[hinten_rechts];
        };
        CHECK(summe(langsam) - summe(langsam_ohne) == doctest::Approx(bei_v / 4.0).epsilon(0.15));
    }
}
