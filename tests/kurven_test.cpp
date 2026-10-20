#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "fahrt.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

// Obergrenze für die Kraft aller Reifen zusammen, als Vielfaches von μ·g. Etwas
// über 1 ist möglich: Entlastete Räder haben dank Lastempfindlichkeit einen
// höheren Reibwert.
constexpr double reibkreis_grenze = 1.05; // 1

// Lenkt fest und hält mit etwas Gas im gegebenen Gang die Geschwindigkeit.
EingabePaket kreisfahrt(const Testwelt& welt, double lenkung, double tempo, int gang) {
    EingabePaket paket;
    paket.lenkung = lenkung;
    paket.gangwunsch = static_cast<std::int8_t>(gang);
    paket.gas = std::clamp(2.0 * (tempo - betrag(welt.karosserie().geschwindigkeit)), 0.0, 1.0);
    return paket;
}

// Radius des Kreises durch drei Punkte in der Ebene.
double umkreisradius(Vec3 a, Vec3 b, Vec3 c) {
    a.z = b.z = c.z = 0.0;
    const double ab = betrag(b - a), bc = betrag(c - b), ca = betrag(a - c); // m
    return ab * bc * ca / (2.0 * betrag(kreuz(b - a, c - a)));
}

// Querbeschleunigung aus den Kräften des letzten Schritts: der Anteil quer zur
// Fahrtrichtung in der Ebene.
double querbeschleunigung(const Testwelt& welt, double masse) {
    const Vec3 v = welt.karosserie().geschwindigkeit;
    const Vec3 quer = kreuz({0.0, 0.0, 1.0}, Vec3{v.x, v.y, 0.0}) * (1.0 / std::hypot(v.x, v.y));
    return skalar(welt.letzter_schritt().kraefte.kraft, quer) / masse;
}

struct Kreis {
    double radius;            // m, aus der gefahrenen Bahn
    double tempo;             // m/s
    double querbeschleunigung; // m/s², aus den Kräften
};

// Misst über eine Sekunde: Radius aus drei Bahnpunkten, Tempo und
// Querbeschleunigung gemittelt.
Kreis miss_kreis(Testwelt& welt, Fahrt& fahrt, double lenkung, double tempo, int gang, double masse) {
    const int ticks = Fahrt::ticks(1.0);
    Vec3 punkte[3]; // m
    double summe_tempo = 0.0, summe_quer = 0.0; // m/s und m/s², über die Ticks aufsummiert
    for (int i = 0; i <= ticks; ++i) {
        if (i == 0) punkte[0] = welt.karosserie().position;
        if (i == ticks / 2) punkte[1] = welt.karosserie().position;
        if (i == ticks) {
            punkte[2] = welt.karosserie().position;
            break;
        }
        fahrt.einen_tick(kreisfahrt(welt, lenkung, tempo, gang));
        summe_tempo += betrag(welt.karosserie().geschwindigkeit);
        summe_quer += querbeschleunigung(welt, masse);
    }
    return {umkreisradius(punkte[0], punkte[1], punkte[2]), summe_tempo / ticks, summe_quer / ticks};
}

} // namespace

TEST_CASE("Langsam fährt das Auto den Radius, den der Lenkwinkel vorgibt") {
    const Kennwerte auto_ = sportcoupe();
    const double tempo = 4.0;    // m/s
    const double lenkung = 0.2;  // 1, Anteil am vollen Einschlag
    Testwelt welt(auto_, 0.02, Testgelaende{}, tempo);
    Fahrt fahrt(welt);
    for (int i = 0; i < Fahrt::ticks(6.0); ++i) fahrt.einen_tick(kreisfahrt(welt, lenkung, tempo, 1));

    const Kreis kreis = miss_kreis(welt, fahrt, lenkung, tempo, 1, auto_.masse);

    // Ohne Schräglauf rollt die Hinterachse um den Punkt, auf den das gelenkte
    // Vorderrad zeigt: R = L / tan δ. Bei 0,4 m/s² Querbeschleunigung liegt der
    // Schräglauf nur bei Zehnteln eines Grads.
    const double radstand = auto_.vorn.x - auto_.hinten.x;    // m
    const double lenkwinkel = lenkung * auto_.lenkung.max_lenkwinkel; // rad
    const double hinten = radstand / std::tan(lenkwinkel);    // m, Radius der Hinterachse
    const double b = auto_.schwerpunkt.x - auto_.hinten.x;    // m, Schwerpunkt vor der Hinterachse
    CAPTURE(kreis.radius);
    CHECK(kreis.radius == doctest::Approx(std::hypot(hinten, b)).epsilon(0.05));
}

TEST_CASE("Bei konstantem Tempo und fester Lenkung stellt sich eine stationäre Kreisfahrt ein") {
    const Kennwerte auto_ = sportcoupe();
    const double tempo = 15.0;    // m/s
    const double lenkung = 0.08;  // 1
    Testwelt welt(auto_, 0.02, Testgelaende{}, tempo);
    Fahrt fahrt(welt);
    for (int i = 0; i < Fahrt::ticks(6.0); ++i) fahrt.einen_tick(kreisfahrt(welt, lenkung, tempo, 2));

    const Kreis erster = miss_kreis(welt, fahrt, lenkung, tempo, 2, auto_.masse);
    const Kreis zweiter = miss_kreis(welt, fahrt, lenkung, tempo, 2, auto_.masse);
    CAPTURE(erster.radius);
    CAPTURE(zweiter.radius);
    CAPTURE(zweiter.querbeschleunigung);

    // Stationär: Radius und Tempo ändern sich von Sekunde zu Sekunde kaum.
    CHECK(zweiter.radius == doctest::Approx(erster.radius).epsilon(0.01));
    CHECK(zweiter.tempo == doctest::Approx(erster.tempo).epsilon(0.01));
    CHECK(zweiter.querbeschleunigung > 3.0);
    // Die Reifenkräfte halten das Auto genau auf dem gefahrenen Kreis. Auf einem
    // stationären Kreis folgt das aus der Kinematik; der Vergleich prüft, dass
    // Bahn und Kräfte zusammenpassen, die Aussage über das Reifenmodell steckt in
    // der Stationarität und im Radius bei langsamer Fahrt.
    CHECK(zweiter.querbeschleunigung ==
          doctest::Approx(zweiter.tempo * zweiter.tempo / zweiter.radius).epsilon(0.02));
}

TEST_CASE("Die höchste Querbeschleunigung liegt beim Grip der Reifen") {
    const Kennwerte auto_ = sportcoupe();
    const double lenkung = 0.1; // 1
    Testwelt welt(auto_, 0.02, Testgelaende{}, 15.0);
    Fahrt fahrt(welt);
    for (int i = 0; i < Fahrt::ticks(4.0); ++i) fahrt.einen_tick(kreisfahrt(welt, lenkung, 15.0, 3));

    // Bei fester Lenkung langsam schneller, bis die Reifen nicht mehr halten.
    // Gemittelt über eine halbe Sekunde, damit kurze Spitzen nicht zählen.
    const int fenster = Fahrt::ticks(0.5); // Ticks
    double hoechste = 0.0; // m/s²
    double summe = 0.0;    // m/s², über das Fenster aufsummiert
    for (int i = 0; i < Fahrt::ticks(30.0); ++i) {
        const double tempo = 15.0 + 0.5 * i * Testwelt::zeitschritt; // m/s
        fahrt.einen_tick(kreisfahrt(welt, lenkung, tempo, 3));
        summe += querbeschleunigung(welt, auto_.masse);
        if ((i + 1) % fenster == 0) {
            hoechste = std::max(hoechste, std::abs(summe) / fenster);
            summe = 0.0;
        }
    }

    // Ohne Gewichtsverlagerung hielte das Auto μ·g. Die Lastempfindlichkeit
    // kostet beim Wanken etwas davon.
    const double grip = auto_.vorn.reifen.spitzenreibwert * Testwelt::erdbeschleunigung; // m/s²
    CAPTURE(hoechste);
    CAPTURE(grip);
    CHECK(hoechste > 0.85 * grip);
    CHECK(hoechste < reibkreis_grenze * grip);
}

namespace {

struct Wanken {
    double wankwinkel;          // rad, positiv nach rechts (kurvenaußen in einer Linkskurve)
    double querbeschleunigung;  // m/s²
    double verlagerung_vorn;    // N, Last rechts minus links an der Vorderachse
    double verlagerung_hinten;  // N
    double hoehe;               // m, Schwerpunkt über dem Boden
};

// Fährt eine stationäre Linkskurve mit rund 6 m/s² und misst am Ende.
Wanken linkskurve(const Kennwerte& auto_) {
    Testwelt welt(auto_, 0.02, Testgelaende{}, 15.0);
    Fahrt fahrt(welt);
    for (int i = 0; i < Fahrt::ticks(6.0); ++i) fahrt.einen_tick(kreisfahrt(welt, 0.11, 15.0, 2));
    const auto& radlast = welt.letzter_schritt().telemetrie.radlast;
    const Vec3 links = drehe(welt.karosserie().ausrichtung, {0.0, 1.0, 0.0});
    return {std::asin(links.z), querbeschleunigung(welt, auto_.masse),
            radlast[vorn_rechts] - radlast[vorn_links], radlast[hinten_rechts] - radlast[hinten_links],
            welt.karosserie().position.z};
}

} // namespace

TEST_CASE("In der Kurve wankt die Karosserie nach außen und die äußeren Räder tragen mehr Last") {
    const Kennwerte auto_ = sportcoupe();
    const Wanken w = linkskurve(auto_);
    CAPTURE(w.querbeschleunigung);
    CAPTURE(w.wankwinkel);
    REQUIRE(w.querbeschleunigung > 5.0);

    CHECK(w.wankwinkel > 0.0);
    CHECK(w.verlagerung_vorn > 0.0);
    CHECK(w.verlagerung_hinten > 0.0);
    // Momentengleichgewicht um die Längsachse: Die Lastunterschiede mal halber
    // Spur (N·m) halten der Fliehkraft in Schwerpunkthöhe über dem Boden die Waage,
    // dazu dem Gewicht, weil der Schwerpunkt beim Wanken um h·sin φ nach außen wandert.
    const double erwartet = auto_.masse * w.hoehe *
                            (w.querbeschleunigung + Testwelt::erdbeschleunigung * std::sin(w.wankwinkel));
    const double gemessen =
        w.verlagerung_vorn * auto_.vorn.spurweite / 2.0 + w.verlagerung_hinten * auto_.hinten.spurweite / 2.0;
    CHECK(gemessen == doctest::Approx(erwartet).epsilon(0.02));
}

TEST_CASE("Stabilisatoren verringern das Wanken und verteilen die Lastverlagerung auf ihre Achse") {
    const Kennwerte auto_ = sportcoupe();
    Kennwerte ohne = auto_;
    ohne.vorn.fahrwerk.stabilisator = 0.0;
    ohne.hinten.fahrwerk.stabilisator = 0.0;
    Kennwerte vorn_hart = auto_;
    vorn_hart.vorn.fahrwerk.stabilisator *= 3.0;

    const Wanken mit_stabi = linkskurve(auto_);
    const Wanken ohne_stabi = linkskurve(ohne);
    const Wanken hart = linkskurve(vorn_hart);
    CAPTURE(mit_stabi.wankwinkel);
    CAPTURE(ohne_stabi.wankwinkel);

    REQUIRE(auto_.vorn.fahrwerk.stabilisator > 0.0);
    REQUIRE(auto_.hinten.fahrwerk.stabilisator > 0.0);
    CHECK(mit_stabi.wankwinkel < 0.8 * ohne_stabi.wankwinkel);
    const auto anteil_vorn = [](const Wanken& w) {
        return w.verlagerung_vorn / (w.verlagerung_vorn + w.verlagerung_hinten);
    };
    CHECK(anteil_vorn(hart) > anteil_vorn(mit_stabi) + 0.05);
}

TEST_CASE("Bremsen in der Kurve kostet Seitenführung, die Reifen bleiben im Reibkreis") {
    const Kennwerte auto_ = sportcoupe();
    const double lenkung = 0.11; // 1
    Testwelt welt(auto_, 0.02, Testgelaende{}, 15.0);
    Fahrt fahrt(welt);
    for (int i = 0; i < Fahrt::ticks(6.0); ++i) fahrt.einen_tick(kreisfahrt(welt, lenkung, 15.0, 2));
    const double vorher = querbeschleunigung(welt, auto_.masse); // m/s²
    REQUIRE(vorher > 5.0);

    // Vollbremsung bei gleicher Lenkung. Die Räder blockieren und schieben fast
    // nur noch längs.
    EingabePaket bremsen;
    bremsen.lenkung = lenkung;
    bremsen.bremse = 1.0;
    double hoechste_gesamt = 0.0; // m/s²
    double summe_quer = 0.0;      // m/s², über die Ticks aufsummiert
    const int ticks = Fahrt::ticks(0.25);
    for (int i = 0; i < ticks; ++i) {
        fahrt.einen_tick(bremsen);
        const Vec3 kraft = welt.letzter_schritt().kraefte.kraft; // N
        hoechste_gesamt = std::max(hoechste_gesamt, std::hypot(kraft.x, kraft.y) / auto_.masse);
        summe_quer += querbeschleunigung(welt, auto_.masse);
    }

    const double grip = auto_.vorn.reifen.spitzenreibwert * Testwelt::erdbeschleunigung; // m/s²
    CAPTURE(vorher);
    CAPTURE(summe_quer / ticks);
    CAPTURE(hoechste_gesamt);
    // Mit schwächerer Kopplung (B = 4 statt 10) blieben 34 %.
    CHECK(summe_quer / ticks < 0.25 * vorher);
    // Ohne kombinierten Schlupf wären es rund 1,13 μ·g.
    CHECK(hoechste_gesamt < reibkreis_grenze * grip);
}

TEST_CASE("Langsames Rangieren mit vollem Lenkeinschlag bleibt ruhig, bis das Auto steht") {
    const Kennwerte auto_ = sportcoupe();
    Testwelt welt(auto_, 0.02, Testgelaende{}, 2.0);
    Fahrt fahrt(welt);
    EingabePaket eingeschlagen;
    eingeschlagen.lenkung = 1.0;
    fahrt.fahre(0.5, eingeschlagen); // einfedern lassen

    // Im Leerlauf ausrollen; das Radieren der Reifen bei vollem Einschlag bremst
    // das Auto bis zum Stillstand, dann steht es noch ein paar Sekunden.
    Schwingungsmelder gierrate(0.001); // rad/s je Tick
    Schwingungsmelder quer(0.001);     // m/s je Tick
    for (int i = 0; i < Fahrt::ticks(15.0); ++i) {
        fahrt.einen_tick(eingeschlagen);
        const Karosseriebewegung& k = welt.karosserie();
        gierrate.nimm(k.drehgeschwindigkeit.z);
        quer.nimm(drehe_zurueck(k.ausrichtung, k.geschwindigkeit).y);
    }

    CAPTURE(betrag(welt.karosserie().geschwindigkeit));
    CHECK(gierrate.laengste_folge() < 3);
    CHECK(quer.laengste_folge() < 3);
    CHECK(betrag(welt.karosserie().geschwindigkeit) < 1e-3);
    CHECK(std::abs(welt.karosserie().drehgeschwindigkeit.z) < 1e-3);
}

TEST_CASE("Bei hoher Geschwindigkeit schwingt das Auto nach einem Lenkimpuls nicht auf") {
    const Kennwerte auto_ = sportcoupe();
    const double tempo = 45.0; // m/s, rund 160 km/h
    Testwelt welt(auto_, 0.02, Testgelaende{}, tempo);
    Fahrt fahrt(welt);
    for (int i = 0; i < Fahrt::ticks(3.0); ++i) fahrt.einen_tick(kreisfahrt(welt, 0.0, tempo, 5));

    // Kurzer Lenkimpuls nach links, dann wieder geradeaus.
    for (int i = 0; i < Fahrt::ticks(0.2); ++i) fahrt.einen_tick(kreisfahrt(welt, 0.04, tempo, 5));

    // Die Gierrate klingt ab, ohne zur Gegenseite durchzuschwingen, und nach drei
    // Sekunden ist praktisch nichts mehr übrig.
    double groesste = 0.0;  // rad/s, nach links
    double gegenseite = 0.0; // rad/s, größter Ausschlag nach rechts
    double gierrate = 0.0;  // rad/s
    for (int i = 0; i < Fahrt::ticks(3.0); ++i) {
        fahrt.einen_tick(kreisfahrt(welt, 0.0, tempo, 5));
        gierrate = welt.karosserie().drehgeschwindigkeit.z;
        groesste = std::max(groesste, gierrate);
        gegenseite = std::max(gegenseite, -gierrate);
    }

    CAPTURE(groesste);
    CAPTURE(gegenseite);
    REQUIRE(groesste > 0.05);
    CHECK(gegenseite < 0.1 * groesste);
    CHECK(std::abs(gierrate) < 0.02 * groesste);
}

TEST_CASE("Zwei Kurvenfahrten mit denselben Eingabe-Paketen ergeben dieselbe Prüfsumme") {
    const auto fahre_kurve = [](double lenkung) {
        Testwelt welt(sportcoupe(), 0.02, Testgelaende{}, 15.0);
        Fahrt fahrt(welt);
        for (int i = 0; i < Fahrt::ticks(3.0); ++i) fahrt.einen_tick(kreisfahrt(welt, lenkung, 15.0, 2));
        return welt.pruefsumme();
    };

    CHECK(fahre_kurve(0.1) == fahre_kurve(0.1));
    CHECK(fahre_kurve(0.1) != fahre_kurve(0.11));
}
