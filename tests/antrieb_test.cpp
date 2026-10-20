#include "doctest/doctest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "fahrt.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

constexpr double u_min = 2.0 * std::numbers::pi / 60.0; // rad/s je U/min

double drehzahl(const Testwelt& welt) { return welt.letzter_schritt().animationsdaten.drehzahl; }

double hinterachsdrehzahl(const Testwelt& welt) {
    const auto& omega = welt.letzter_schritt().zustand.raddrehzahl;
    return (omega[hinten_links] + omega[hinten_rechts]) / 2.0;
}

// Fährt an und schaltet kurz vor dem Begrenzer hoch: Kupplung treten, Gas weg,
// nächster Gang, nach der Schaltzeit die Kupplung zügig kommen lassen. In den
// unteren Gängen gibt er nur so viel Gas, dass die Hinterräder kaum durchdrehen.
class Testfahrer {
public:
    static constexpr double einlegen = 0.3;         // s Kupplung getreten vor dem Anfahren
    static constexpr double kupplung_anfahren = 0.5; // s zum Kommenlassen beim Anfahren
    static constexpr double kupplung_schalten = 0.15; // s zum Kommenlassen nach dem Schalten
    static constexpr double schaltdrehzahl = 6800.0 * u_min;
    static constexpr std::array<double, 3> gas_untere_gaenge{0.75, 0.95, 1.0}; // 1

    explicit Testfahrer(const Kennwerte& k)
        : gaenge_(static_cast<int>(k.antrieb.getriebe.uebersetzungen.size())),
          schaltpause_(k.antrieb.getriebe.schaltzeit + 0.05) {}

    EingabePaket paket(const Testwelt& welt) {
        EingabePaket p;
        p.gangwunsch = static_cast<std::int8_t>(gang_);
        if (zeit_ < einlegen) {
            p.kupplung = 1.0;
        } else if (zeit_ < einlegen + kupplung_anfahren) {
            p.gas = gas();
            p.kupplung = 1.0 - (zeit_ - einlegen) / kupplung_anfahren;
        } else {
            const double seit_schalten = zeit_ - geschaltet_;
            if (gang_ < gaenge_ && seit_schalten > schaltpause_ + kupplung_schalten &&
                drehzahl(welt) > schaltdrehzahl) {
                ++gang_;
                geschaltet_ = zeit_;
                p.gangwunsch = static_cast<std::int8_t>(gang_);
                p.kupplung = 1.0;
            } else if (seit_schalten < schaltpause_) {
                p.kupplung = 1.0;
            } else {
                p.gas = gas();
                p.kupplung = std::max(1.0 - (seit_schalten - schaltpause_) / kupplung_schalten, 0.0);
            }
        }
        zeit_ += Testwelt::zeitschritt;
        return p;
    }

    int gang() const { return gang_; }
    bool eingekuppelt() const { return zeit_ - geschaltet_ > schaltpause_ + kupplung_schalten; }

private:
    double gas() const { return gang_ <= 3 ? gas_untere_gaenge[gang_ - 1] : 1.0; }

    int gaenge_;
    double schaltpause_; // s
    int gang_ = 1;
    double zeit_ = 0.0;        // s seit Beginn des Anfahrens
    double geschaltet_ = -1e9; // s, Zeitpunkt des letzten Gangwechsels
};

} // namespace

TEST_CASE("Im Leerlauf und bei getretener Kupplung hält der Motor seine Leerlaufdrehzahl") {
    const Kennwerte auto_ = sportcoupe();
    const double leerlauf = auto_.antrieb.motor.leerlaufdrehzahl;

    SUBCASE("im Stand ohne Gang") {
        Testwelt welt(auto_, 0.02);
        Fahrt fahrt(welt);
        for (int tick = 0; tick < Fahrt::ticks(3.0); ++tick) {
            fahrt.einen_tick({});
            CHECK(drehzahl(welt) == doctest::Approx(leerlauf).epsilon(0.02));
        }
    }

    SUBCASE("rollend im dritten Gang mit getretener Kupplung") {
        Testwelt welt(auto_, 0.02, Testgelaende{}, hundert_kmh);
        Fahrt fahrt(welt);
        EingabePaket getreten;
        getreten.gangwunsch = 3;
        getreten.kupplung = 1.0;
        for (int tick = 0; tick < Fahrt::ticks(3.0); ++tick) {
            fahrt.einen_tick(getreten);
            CHECK(drehzahl(welt) == doctest::Approx(leerlauf).epsilon(0.02));
        }
        // Ohne Kraftschluss bremst der Motor das Auto nicht.
        CHECK(laengsgeschwindigkeit(welt) == doctest::Approx(hundert_kmh).epsilon(0.01));
    }
}

TEST_CASE("Das Auto fährt aus dem Stand an, schaltet bis in den sechsten Gang und schafft 0–100 km/h in plausibler Zeit") {
    const Kennwerte auto_ = sportcoupe();
    Testwelt welt(auto_, 0.02);
    Fahrt fahrt(welt);
    fahrt.fahre(1.5); // einfedern lassen
    Testfahrer fahrer(auto_);

    double zeit = -Testfahrer::einlegen; // s ab dem ersten Gasgeben
    double null_auf_hundert = 0.0;
    while (fahrer.gang() < 6 || !fahrer.eingekuppelt()) {
        REQUIRE(zeit < 60.0);
        fahrt.einen_tick(fahrer.paket(welt));
        zeit += Testwelt::zeitschritt;
        if (null_auf_hundert == 0.0 && laengsgeschwindigkeit(welt) >= hundert_kmh) null_auf_hundert = zeit;
    }

    // Untere Grenze: Die Hinterachse überträgt höchstens μ mal ihre dynamische
    // Last, a = μ·g·l_v / (L − μ·h). Ein echtes Auto mit 300 PS und 1350 kg
    // braucht rund 5 s. Das Band nach oben ist bewusst weit, weil der Testfahrer
    // grob anfährt und Fahrwiderstände noch fehlen; schärfer wird es mit dem
    // Testmanöver in 09 und den Zielwerten in 18.
    const double mu = auto_.hinten.reifen.spitzenreibwert;
    const double radstand = auto_.vorn.x - auto_.hinten.x;
    const double l_v = auto_.vorn.x - auto_.schwerpunkt.x;
    const double h = auto_.schwerpunkt.z;
    const double hoechste_beschleunigung = mu * Testwelt::erdbeschleunigung * l_v / (radstand - mu * h);
    CAPTURE(null_auf_hundert);
    CHECK(null_auf_hundert > hundert_kmh / hoechste_beschleunigung);
    CHECK(null_auf_hundert < 7.0);

    // Im sechsten Gang haftet die Kupplung: Motor und Hinterachse drehen im Verhältnis der Übersetzungen.
    fahrt.fahre(0.5, fahrer.paket(welt));
    const double gesamt = auto_.antrieb.getriebe.uebersetzungen[5] * auto_.antrieb.getriebe.achsuebersetzung;
    CHECK(drehzahl(welt) == doctest::Approx(hinterachsdrehzahl(welt) * gesamt).epsilon(0.01));
}

TEST_CASE("Anfahren und Gangwechsel bringen weder Motor- noch Raddrehzahl ins Schwingen") {
    const Kennwerte auto_ = sportcoupe();
    Testwelt welt(auto_, 0.02);
    Fahrt fahrt(welt);
    fahrt.fahre(1.5);
    Testfahrer fahrer(auto_);

    Schwingungsmelder motor;
    std::array<Schwingungsmelder, anzahl_raeder> raeder;
    while (fahrer.gang() < 4 || !fahrer.eingekuppelt()) {
        REQUIRE(welt.letzter_schritt().zustand.motordrehzahl >= 0.0);
        fahrt.einen_tick(fahrer.paket(welt));
        motor.nimm(drehzahl(welt));
        for (int i = 0; i < anzahl_raeder; ++i) raeder[i].nimm(welt.letzter_schritt().zustand.raddrehzahl[i]);
    }

    CHECK(motor.laengste_folge() < 3);
    for (const Schwingungsmelder& rad : raeder) CHECK(rad.laengste_folge() < 3);
}

TEST_CASE("Gas weg im Gang bremst das Auto über die Motorbremse") {
    const Kennwerte auto_ = sportcoupe();
    Testwelt im_gang(auto_, 0.02, Testgelaende{}, hundert_kmh);
    Testwelt im_leerlauf(auto_, 0.02, Testgelaende{}, hundert_kmh);
    Fahrt fahrt_im_gang(im_gang);
    Fahrt fahrt_im_leerlauf(im_leerlauf);

    // Den dritten Gang mit getretener Kupplung einlegen und sie dann kommen lassen.
    EingabePaket dritter;
    dritter.gangwunsch = 3;
    dritter.kupplung = 1.0;
    fahrt_im_gang.fahre(0.5, dritter);
    for (int tick = 0; tick < Fahrt::ticks(0.5); ++tick) {
        dritter.kupplung = 1.0 - static_cast<double>(tick) / Fahrt::ticks(0.5);
        fahrt_im_gang.einen_tick(dritter);
    }
    dritter.kupplung = 0.0;
    fahrt_im_leerlauf.fahre(1.0);

    const double v_im_gang = laengsgeschwindigkeit(im_gang);
    for (int tick = 0; tick < Fahrt::ticks(2.0); ++tick) {
        const double v_vorher = laengsgeschwindigkeit(im_gang);
        fahrt_im_gang.einen_tick(dritter);
        fahrt_im_leerlauf.einen_tick({});
        CHECK(laengsgeschwindigkeit(im_gang) < v_vorher);
    }

    // Ohne Fahrwiderstände rollt das Auto im Leerlauf ungebremst weiter.
    CHECK(laengsgeschwindigkeit(im_leerlauf) == doctest::Approx(hundert_kmh).epsilon(0.01));
    // Rund 670 N Schleppkraft bei 4600 U/min verzögern um etwa 0,45 m/s².
    CHECK(v_im_gang - laengsgeschwindigkeit(im_gang) > 0.5);
}

TEST_CASE("Mit zu viel Gas beim Anfahren drehen die Hinterräder durch") {
    const Kennwerte auto_ = sportcoupe();
    Testwelt welt(auto_, 0.02);
    Fahrt fahrt(welt);
    fahrt.fahre(1.5);

    // Ersten Gang einlegen, hochdrehen und die Kupplung schnappen lassen.
    EingabePaket erster;
    erster.gangwunsch = 1;
    erster.kupplung = 1.0;
    fahrt.fahre(0.3, erster);
    erster.gas = 1.0;
    fahrt.fahre(0.3, erster);
    erster.kupplung = 0.0;
    fahrt.fahre(1.0, erster);

    const double v = laengsgeschwindigkeit(welt);
    const auto& omega = welt.letzter_schritt().zustand.raddrehzahl;
    CAPTURE(v);
    REQUIRE(v > 1.0);
    for (int i : {hinten_links, hinten_rechts}) CHECK(omega[i] * auto_.hinten.radradius > 1.5 * v);
    for (int i : {vorn_links, vorn_rechts}) CHECK(omega[i] * auto_.vorn.radradius == doctest::Approx(v).epsilon(0.02));
}

TEST_CASE("Das offene Differenzial lässt das Hinterrad auf glattem Belag durchdrehen, das andere nicht") {
    const Kennwerte auto_ = sportcoupe();
    // Rechts von der Mittellinie ist es glatt; das Auto steht mit den rechten Rädern darauf.
    Testgelaende glatt_rechts;
    glatt_rechts.belagzonen.push_back(
        {.x_von = -100.0, .x_bis = 100.0, .y_von = -10.0, .y_bis = 0.0, .reibwert = 0.2});

    // Halbgas im ersten Gang, Kupplung zügig kommen lassen.
    const auto fahre_an = [&](Testwelt& welt) {
        Fahrt fahrt(welt);
        fahrt.fahre(1.5);
        EingabePaket erster;
        erster.gangwunsch = 1;
        erster.kupplung = 1.0;
        fahrt.fahre(0.3, erster);
        erster.gas = 0.5;
        for (int tick = 0; tick < Fahrt::ticks(0.3); ++tick) {
            erster.kupplung = 1.0 - static_cast<double>(tick) / Fahrt::ticks(0.3);
            fahrt.einen_tick(erster);
        }
        erster.kupplung = 0.0;
        fahrt.fahre(0.7, erster);
    };
    Testwelt welt(auto_, 0.02, glatt_rechts);
    Testwelt trocken(auto_, 0.02);
    fahre_an(welt);
    fahre_an(trocken);

    const double v = laengsgeschwindigkeit(welt);
    const auto& omega = welt.letzter_schritt().zustand.raddrehzahl;
    const double r = auto_.hinten.radradius;
    CAPTURE(v);
    REQUIRE(v > 0.1);
    // Die Drehzahlen laufen frei auseinander: Rechts dreht durch, links rollt fast ohne Schlupf.
    CHECK(omega[hinten_rechts] * r > v + 5.0);
    CHECK(omega[hinten_links] * r < v + 0.5);
    // Beide Seiten bekommen dasselbe Moment, also nicht mehr, als das glatte Rad
    // abstützt: Das Auto kommt viel schlechter weg als auf trockenem Belag.
    CHECK(v < 0.3 * laengsgeschwindigkeit(trocken));
}
