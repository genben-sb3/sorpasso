#include "doctest/doctest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

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
    Kennwerte auto_ = sportcoupe();
    auto_.antrieb.differenzial = {};
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

namespace {

Kennwerte mit_automatik() {
    Kennwerte auto_ = sportcoupe();
    auto_.antrieb.getriebe.modus = Getriebemodus::automatik;
    auto_.antrieb.getriebe.automatik = {.hochschaltdrehzahl = 6500.0 * u_min,
                                        .runterschaltdrehzahl = 2500.0 * u_min,
                                        .bremsschaltdrehzahl = 4000.0 * u_min,
                                        .haltezeit = 0.8,
                                        .einkuppelbereich = 1500.0 * u_min,
                                        .einkuppelzeit = 0.15,
                                        .driftwinkel = 10.0 * std::numbers::pi / 180.0};
    return auto_;
}

int gang(const Testwelt& welt) { return welt.letzter_schritt().zustand.gang; }

EingabePaket fahren(double gas, double bremse = 0.0, double lenkung = 0.0) {
    EingabePaket p;
    p.gangwunsch = 1; // in der Automatik: Fahren
    p.gas = gas;
    p.bremse = bremse;
    p.lenkung = lenkung;
    return p;
}

// Die Gänge, die ein Lauf nacheinander einlegt, samt der Zeit, die jeder Gang hielt.
struct Gangfolge {
    std::vector<int> gaenge;
    std::vector<double> haltezeiten; // s, der letzte Gang ist nicht dabei

    void nimm(int g, double zeit) {
        if (gaenge.empty() || g != gaenge.back()) {
            if (!gaenge.empty()) haltezeiten.push_back(zeit - seit_);
            gaenge.push_back(g);
            seit_ = zeit;
        }
    }

private:
    double seit_ = 0.0; // s
};

} // namespace

TEST_CASE("Die Automatik fährt an, schaltet bei Vollgas hoch und pendelt dabei nicht") {
    const Kennwerte auto_ = mit_automatik();
    Testwelt welt(auto_, 0.02);
    Fahrt fahrt(welt);
    fahrt.fahre(1.5);

    Gangfolge folge;
    const int ticks = Fahrt::ticks(20.0);
    for (int tick = 0; tick < ticks; ++tick) {
        fahrt.einen_tick(fahren(1.0));
        folge.nimm(gang(welt), tick * Testwelt::zeitschritt);
    }

    CAPTURE(folge.gaenge.size());
    CHECK(laengsgeschwindigkeit(welt) > 40.0);
    REQUIRE(folge.gaenge.size() >= 5);
    // Nur aufwärts, ein Gang nach dem anderen, und jeder Gang wird gehalten.
    for (std::size_t i = 1; i < folge.gaenge.size(); ++i) CHECK(folge.gaenge[i] == folge.gaenge[i - 1] + 1);
    for (double zeit : folge.haltezeiten) CHECK(zeit >= auto_.antrieb.getriebe.automatik.haltezeit);
}

TEST_CASE("Die Automatik schaltet beim Bremsen herunter und schaltet dabei nicht wieder hoch") {
    const Kennwerte auto_ = mit_automatik();
    Testwelt welt(auto_, 0.02);
    Fahrt fahrt(welt);
    fahrt.fahre(1.5);
    fahrt.fahre(14.0, fahren(1.0));
    const int hoch = gang(welt);
    REQUIRE(hoch >= 4);

    Gangfolge folge;
    folge.nimm(hoch, 0.0);
    const int ticks = Fahrt::ticks(8.0);
    for (int tick = 0; tick < ticks && laengsgeschwindigkeit(welt) > 1.0; ++tick) {
        fahrt.einen_tick(fahren(0.0, 0.5));
        folge.nimm(gang(welt), tick * Testwelt::zeitschritt);
    }

    CHECK(laengsgeschwindigkeit(welt) < 1.0);
    CHECK(gang(welt) < hoch);
    for (std::size_t i = 1; i < folge.gaenge.size(); ++i) CHECK(folge.gaenge[i] == folge.gaenge[i - 1] - 1);
}

TEST_CASE("Durchdrehende Räder lösen in der Automatik kein Hochschalten aus") {
    const Kennwerte auto_ = mit_automatik();
    Testgelaende glatt;
    glatt.reibwert = 0.15;
    Testwelt welt(auto_, 0.02, glatt);
    Fahrt fahrt(welt);
    fahrt.fahre(1.5);

    double hoechste_drehzahl = 0.0; // rad/s
    for (int tick = 0; tick < Fahrt::ticks(5.0); ++tick) {
        fahrt.einen_tick(fahren(1.0));
        hoechste_drehzahl = std::max(hoechste_drehzahl, drehzahl(welt));
        REQUIRE(gang(welt) == 1);
    }
    // Die Räder drehen durch und ziehen den Motor weit über die Schaltdrehzahl,
    // bei einem Auto, das noch lange nicht schnell genug für den zweiten Gang ist.
    CHECK(hoechste_drehzahl > auto_.antrieb.getriebe.automatik.hochschaltdrehzahl);
    CHECK(laengsgeschwindigkeit(welt) < 10.0);
}

TEST_CASE("Im Drift hält die Automatik den Gang") {
    const Kennwerte auto_ = mit_automatik();
    Testgelaende glatt;
    glatt.reibwert = 0.4;
    Testwelt welt(auto_, 0.02, glatt, 80.0 / 3.6);
    Fahrt fahrt(welt);
    fahrt.fahre(1.0, fahren(0.3)); // Gang einlegen und mitrollen
    const double driftwinkel = auto_.antrieb.getriebe.automatik.driftwinkel;

    int im_drift = 0;
    int gang_im_drift = 0;
    for (int tick = 0; tick < Fahrt::ticks(4.0); ++tick) {
        fahrt.einen_tick(fahren(1.0, 0.0, 1.0));
        const Karosseriebewegung& k = welt.karosserie();
        const double vx = skalar(k.geschwindigkeit, drehe(k.ausrichtung, {1.0, 0.0, 0.0}));
        const double vy = skalar(k.geschwindigkeit, drehe(k.ausrichtung, {0.0, 1.0, 0.0}));
        if (betrag(k.geschwindigkeit) > 2.0 && std::atan2(std::abs(vy), std::abs(vx)) > driftwinkel) {
            if (im_drift == 0) gang_im_drift = gang(welt);
            ++im_drift;
            CHECK(gang(welt) == gang_im_drift);
        }
    }
    CAPTURE(im_drift);
    CHECK(im_drift > Fahrt::ticks(1.0)); // der Test hat wirklich einen Drift erzeugt
}

namespace {

// Beschleunigt aus einer Kurve: erst fest eingeschlagen mit gleichmäßigem Tempo,
// dann Vollgas. Liefert den mittleren Drehzahlunterschied der Hinterräder
// während der Beschleunigung.
double drehzahlunterschied_aus_der_kurve(const Kennwerte& auto_) {
    Testwelt welt(auto_, 0.02, Testgelaende{}, 40.0 / 3.6);
    Fahrt fahrt(welt);
    EingabePaket zweiter;
    zweiter.gangwunsch = 2;
    zweiter.lenkung = 0.5;
    zweiter.kupplung = 1.0;
    fahrt.fahre(0.5, zweiter);
    zweiter.kupplung = 0.0;
    zweiter.gas = 0.3;
    fahrt.fahre(2.0, zweiter);
    zweiter.gas = 1.0;
    double summe = 0.0; // rad/s
    const int ticks = Fahrt::ticks(1.0);
    for (int tick = 0; tick < ticks; ++tick) {
        fahrt.einen_tick(zweiter);
        const auto& omega = welt.letzter_schritt().zustand.raddrehzahl;
        summe += std::abs(omega[hinten_links] - omega[hinten_rechts]);
    }
    return summe / ticks;
}

} // namespace

TEST_CASE("Mit Sperrdifferenzial ist der Drehzahlunterschied der Hinterräder aus der Kurve kleiner als mit offenem") {
    Kennwerte offen = sportcoupe();
    offen.antrieb.differenzial = {};
    const Kennwerte gesperrt = sportcoupe();
    REQUIRE(gesperrt.antrieb.differenzial.sperrt);

    const double unterschied_offen = drehzahlunterschied_aus_der_kurve(offen);
    const double unterschied_gesperrt = drehzahlunterschied_aus_der_kurve(gesperrt);

    CAPTURE(unterschied_offen);
    CAPTURE(unterschied_gesperrt);
    REQUIRE(unterschied_offen > 0.5);
    CHECK(unterschied_gesperrt < 0.7 * unterschied_offen);
}

TEST_CASE("Das Sperrdifferenzial bringt auf einseitig glattem Belag mehr Vortrieb als das offene") {
    Kennwerte offen = sportcoupe();
    offen.antrieb.differenzial = {};
    const Kennwerte gesperrt = sportcoupe();
    Testgelaende glatt_rechts;
    glatt_rechts.belagzonen.push_back(
        {.x_von = -100.0, .x_bis = 100.0, .y_von = -10.0, .y_bis = 0.0, .reibwert = 0.2});

    const auto tempo_nach_anfahren = [&](const Kennwerte& k) {
        Testwelt welt(k, 0.02, glatt_rechts);
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
        fahrt.fahre(1.5, erster);
        return laengsgeschwindigkeit(welt);
    };

    CHECK(tempo_nach_anfahren(gesperrt) > 1.5 * tempo_nach_anfahren(offen));
}

TEST_CASE("Front- und Allradantrieb fahren an, beschleunigen und bremsen stabil") {
    struct Fall {
        const char* name;
        Antriebsart art;
        double vorn; // Anteil der Vorderachse
    };
    for (const Fall& fall : {Fall{"front", Antriebsart::front, 0.0}, Fall{"allrad", Antriebsart::allrad, 0.4}}) {
        const std::string name = fall.name;
        CAPTURE(name);
        Kennwerte auto_ = sportcoupe();
        auto_.antrieb.art = fall.art;
        auto_.antrieb.momentverteilung_vorn = fall.vorn;
        Testwelt welt(auto_, 0.02);
        Fahrt fahrt(welt);
        fahrt.fahre(1.5);
        Testfahrer fahrer(auto_);

        Schwingungsmelder motor;
        double hoechste_gierrate = 0.0; // rad/s
        // Der Testfahrer gibt im ersten Gang mehr Gas, als ein Fronttriebler übertragen
        // kann; er schaltet dann früh, und das Auto kommt später auf Tempo.
        for (int tick = 0; tick < Fahrt::ticks(12.0); ++tick) {
            REQUIRE(welt.letzter_schritt().zustand.motordrehzahl >= 0.0);
            fahrt.einen_tick(fahrer.paket(welt));
            motor.nimm(drehzahl(welt));
            hoechste_gierrate = std::max(hoechste_gierrate, std::abs(welt.karosserie().drehgeschwindigkeit.z));
        }
        const double v = laengsgeschwindigkeit(welt);
        CAPTURE(v);
        CHECK(v > 15.0);
        CHECK(motor.laengste_folge() < 3);
        CHECK(hoechste_gierrate < 0.05);
        CHECK(std::abs(welt.karosserie().position.y) < 0.3);

        // Bremsen mit getretener Kupplung bis zum Stand.
        EingabePaket bremsen;
        bremsen.gangwunsch = static_cast<std::int8_t>(fahrer.gang());
        bremsen.kupplung = 1.0;
        bremsen.bremse = 0.4;
        for (int tick = 0; tick < Fahrt::ticks(10.0) && laengsgeschwindigkeit(welt) > 0.5; ++tick) {
            fahrt.einen_tick(bremsen);
            hoechste_gierrate = std::max(hoechste_gierrate, std::abs(welt.karosserie().drehgeschwindigkeit.z));
        }
        CHECK(laengsgeschwindigkeit(welt) < 0.5);
        CHECK(hoechste_gierrate < 0.05);
        CHECK(std::abs(welt.karosserie().position.y) < 0.5);
    }
}
