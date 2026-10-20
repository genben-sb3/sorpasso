#include "doctest/doctest.h"

#include <algorithm>
#include <fstream>
#include <numbers>
#include <sstream>
#include <string>

#include "nlohmann/json.hpp"
#include "ultragravity/fahrzeugdatei.h"

using namespace ultragravity;

namespace {

const char* const sportcoupe_pfad = UG_FAHRZEUGE_DIR "/sportcoupe.json";

// Das Sportcoupé als JSON-Baum, damit ein Test genau ein Feld verbiegen kann.
nlohmann::json sportcoupe() {
    std::ifstream datei(sportcoupe_pfad);
    return nlohmann::json::parse(datei);
}

LadeErgebnis lies(const nlohmann::json& j) { return lies_fahrzeugdatei(j.dump()); }

} // namespace

TEST_CASE("Das Sportcoupé lädt aus seiner Fahrzeugdatei") {
    const LadeErgebnis ergebnis = lade_fahrzeugdatei(sportcoupe_pfad);

    REQUIRE_FALSE(ergebnis.fehler.has_value());
    const Kennwerte& auto_ = ergebnis.kennwerte;
    CHECK(auto_.name == "Sportcoupé");
    CHECK(auto_.masse == doctest::Approx(1350.0));
    CHECK(auto_.vorn.x - auto_.hinten.x == doctest::Approx(2.6));
    CHECK(auto_.hinten.fahrwerk.daempfer_zug > auto_.hinten.fahrwerk.daempfer_druck);
}

TEST_CASE("Das Sportcoupé hat 300 PS und sechs Gänge") {
    const LadeErgebnis ergebnis = lade_fahrzeugdatei(sportcoupe_pfad);

    REQUIRE_FALSE(ergebnis.fehler.has_value());
    const Antrieb& antrieb = ergebnis.kennwerte.antrieb;
    CHECK(antrieb.getriebe.uebersetzungen.size() == 6);
    // Die Fahrzeugdatei nennt Drehzahlen in U/min, die Kennwerte in rad/s.
    constexpr double u_min = 2.0 * std::numbers::pi / 60.0;
    CHECK(antrieb.motor.leerlaufdrehzahl == doctest::Approx(900.0 * u_min));
    double spitzenleistung = 0.0;
    for (const Stuetzpunkt& p : antrieb.motor.drehmoment) {
        spitzenleistung = std::max(spitzenleistung, p.drehzahl * p.moment);
    }
    constexpr double ps = 735.49875; // W
    CHECK(spitzenleistung / ps == doctest::Approx(300.0).epsilon(0.01));
}

TEST_CASE("Kaputtes JSON meldet Zeile und Spalte") {
    const LadeErgebnis ergebnis = lies_fahrzeugdatei("{\n  \"name\": \"Coupé\",\n  \"masse\": 13 50\n}");

    REQUIRE(ergebnis.fehler.has_value());
    CHECK(ergebnis.fehler->feld == "");
    CHECK(ergebnis.fehler->meldung.find("Zeile 3, Spalte 16") != std::string::npos);
}

TEST_CASE("Die Spalte eines Syntaxfehlers zählt Zeichen, nicht Bytes") {
    // Das „é“ belegt zwei Bytes; das fehlerhafte x steht im 19. Zeichen.
    const LadeErgebnis ergebnis = lies_fahrzeugdatei("{\"name\": \"Coupé\", x}");

    REQUIRE(ergebnis.fehler.has_value());
    CHECK(ergebnis.fehler->meldung.find("Zeile 1, Spalte 19") != std::string::npos);
}

TEST_CASE("Ein fehlendes Feld wird mit seinem Pfad gemeldet") {
    nlohmann::json j = sportcoupe();
    j["achsen"]["hinten"]["fahrwerk"].erase("federweg");

    const LadeErgebnis ergebnis = lies(j);

    REQUIRE(ergebnis.fehler.has_value());
    CHECK(ergebnis.fehler->feld == "achsen.hinten.fahrwerk.federweg");
    CHECK(ergebnis.fehler->meldung == "fehlt");
}

TEST_CASE("Ein falsch getyptes Feld wird mit seinem Pfad gemeldet") {
    nlohmann::json j = sportcoupe();
    j["schwerpunkt"]["z"] = "0.48";

    const LadeErgebnis ergebnis = lies(j);

    REQUIRE(ergebnis.fehler.has_value());
    CHECK(ergebnis.fehler->feld == "schwerpunkt.z");
    CHECK(ergebnis.fehler->meldung == "ist keine Zahl");
}

TEST_CASE("Eine negative Masse wird abgelehnt") {
    nlohmann::json j = sportcoupe();
    j["masse"] = -1350.0;

    const LadeErgebnis ergebnis = lies(j);

    REQUIRE(ergebnis.fehler.has_value());
    CHECK(ergebnis.fehler->feld == "masse");
    CHECK(ergebnis.fehler->meldung == "muss größer als 0 sein");
}

TEST_CASE("Ein Schwerpunkt außerhalb des Radstands wird abgelehnt") {
    nlohmann::json j = sportcoupe();
    j["schwerpunkt"]["x"] = 0.3; // vor der Vorderachse bei x = 0

    const LadeErgebnis ergebnis = lies(j);

    REQUIRE(ergebnis.fehler.has_value());
    CHECK(ergebnis.fehler->feld == "schwerpunkt.x");
    CHECK(ergebnis.fehler->meldung == "muss zwischen Hinterachse (-2.6) und Vorderachse (0) liegen");
}

TEST_CASE("Eine Vorderachse hinter der Hinterachse wird abgelehnt") {
    nlohmann::json j = sportcoupe();
    j["achsen"]["vorn"]["x"] = -3.0;

    const LadeErgebnis ergebnis = lies(j);

    REQUIRE(ergebnis.fehler.has_value());
    CHECK(ergebnis.fehler->feld == "achsen.vorn.x");
    CHECK(ergebnis.fehler->meldung == "muss vor der Hinterachse (-2.6) liegen");
}

TEST_CASE("Reifen ohne Grip werden abgelehnt") {
    nlohmann::json j = sportcoupe();
    j["achsen"]["hinten"]["reifen"]["spitzenreibwert"] = 0.0;

    const LadeErgebnis ergebnis = lies(j);

    REQUIRE(ergebnis.fehler.has_value());
    CHECK(ergebnis.fehler->feld == "achsen.hinten.reifen.spitzenreibwert");
    CHECK(ergebnis.fehler->meldung == "muss größer als 0 sein");
}

TEST_CASE("Kennwerte ohne physikalischen Sinn werden mit Feldpfad abgelehnt") {
    struct Fall {
        nlohmann::json::json_pointer feld;
        double wert;
        const char* pfad;
        const char* meldung;
    };
    using ptr = nlohmann::json::json_pointer;
    const Fall faelle[] = {
        {ptr("/traegheit/y"), 0.0, "traegheit.y", "muss größer als 0 sein"},
        {ptr("/schwerpunkt/z"), -0.1, "schwerpunkt.z", "muss größer als 0 sein"},
        {ptr("/schwerpunkt/y"), 0.9, "schwerpunkt.y",
         "muss zwischen rechtem Rad (-0.78) und linkem Rad (0.78) liegen"},
        {ptr("/achsen/vorn/spurweite"), 0.0, "achsen.vorn.spurweite", "muss größer als 0 sein"},
        {ptr("/achsen/hinten/radradius"), -0.33, "achsen.hinten.radradius", "muss größer als 0 sein"},
        {ptr("/achsen/vorn/anlenkhoehe"), 0.0, "achsen.vorn.anlenkhoehe", "muss größer als 0 sein"},
        {ptr("/achsen/vorn/fahrwerk/federrate"), 0.0, "achsen.vorn.fahrwerk.federrate", "muss größer als 0 sein"},
        {ptr("/achsen/hinten/fahrwerk/federweg"), 0.0, "achsen.hinten.fahrwerk.federweg", "muss größer als 0 sein"},
        {ptr("/achsen/vorn/fahrwerk/daempfer_druck"), -1.0, "achsen.vorn.fahrwerk.daempfer_druck",
         "darf nicht negativ sein"},
        {ptr("/achsen/hinten/fahrwerk/daempfer_zug"), -1.0, "achsen.hinten.fahrwerk.daempfer_zug",
         "darf nicht negativ sein"},
        {ptr("/achsen/vorn/fahrwerk/anschlagrate"), -1.0, "achsen.vorn.fahrwerk.anschlagrate",
         "darf nicht negativ sein"},
        {ptr("/achsen/vorn/reifen/nennlast"), 0.0, "achsen.vorn.reifen.nennlast", "muss größer als 0 sein"},
        {ptr("/achsen/hinten/reifen/lastempfindlichkeit"), -0.1, "achsen.hinten.reifen.lastempfindlichkeit",
         "muss zwischen 0 und 1 liegen"},
        {ptr("/achsen/vorn/reifen/traegheit"), 0.0, "achsen.vorn.reifen.traegheit", "muss größer als 0 sein"},
        {ptr("/achsen/vorn/reifen/laengs/steifigkeit"), 0.0, "achsen.vorn.reifen.laengs.steifigkeit",
         "muss größer als 0 sein"},
        {ptr("/achsen/hinten/reifen/laengs/form"), 2.5, "achsen.hinten.reifen.laengs.form",
         "darf höchstens 2 sein"},
        {ptr("/achsen/vorn/reifen/laengs/kruemmung"), 1.2, "achsen.vorn.reifen.laengs.kruemmung",
         "darf höchstens 1 sein"},
        {ptr("/achsen/vorn/fahrwerk/stabilisator"), -1.0, "achsen.vorn.fahrwerk.stabilisator",
         "darf nicht negativ sein"},
        {ptr("/achsen/hinten/reifen/relaxationslaenge"), 0.0, "achsen.hinten.reifen.relaxationslaenge",
         "muss größer als 0 sein"},
        {ptr("/achsen/vorn/reifen/quer/steifigkeit"), 0.0, "achsen.vorn.reifen.quer.steifigkeit",
         "muss größer als 0 sein"},
        {ptr("/achsen/hinten/reifen/quer/form"), 2.5, "achsen.hinten.reifen.quer.form", "darf höchstens 2 sein"},
        {ptr("/achsen/vorn/reifen/quer/kruemmung"), 1.2, "achsen.vorn.reifen.quer.kruemmung",
         "darf höchstens 1 sein"},
        {ptr("/achsen/vorn/reifen/kombiniert/laengs"), -1.0, "achsen.vorn.reifen.kombiniert.laengs",
         "darf nicht negativ sein"},
        {ptr("/achsen/hinten/reifen/kombiniert/quer"), -1.0, "achsen.hinten.reifen.kombiniert.quer",
         "darf nicht negativ sein"},
        {ptr("/lenkung/max_lenkwinkel_grad"), 0.0, "lenkung.max_lenkwinkel_grad", "muss größer als 0 sein"},
        {ptr("/lenkung/max_lenkwinkel_grad"), 75.0, "lenkung.max_lenkwinkel_grad", "darf höchstens 60 sein"},
        {ptr("/bremsen/moment"), -1.0, "bremsen.moment", "darf nicht negativ sein"},
        {ptr("/bremsen/balance"), 1.2, "bremsen.balance", "muss zwischen 0 und 1 liegen"},
        {ptr("/antrieb/motor/drehmoment/0/u_min"), 0.0, "antrieb.motor.drehmoment[0].u_min",
         "muss größer als 0 sein"},
        {ptr("/antrieb/motor/drehmoment/2/u_min"), 1500.0, "antrieb.motor.drehmoment[2].u_min",
         "muss größer als beim vorigen Stützpunkt sein"},
        {ptr("/antrieb/motor/drehmoment/3/moment"), -10.0, "antrieb.motor.drehmoment[3].moment",
         "darf nicht negativ sein"},
        {ptr("/antrieb/motor/leerlauf_u_min"), 0.0, "antrieb.motor.leerlauf_u_min", "muss größer als 0 sein"},
        {ptr("/antrieb/motor/hoechst_u_min"), 800.0, "antrieb.motor.hoechst_u_min",
         "muss über der Leerlaufdrehzahl liegen"},
        {ptr("/antrieb/motor/hoechst_u_min"), 9000.0, "antrieb.motor.hoechst_u_min",
         "darf nicht über dem letzten Stützpunkt der Drehmomentkurve liegen"},
        {ptr("/antrieb/motor/traegheit"), 0.0, "antrieb.motor.traegheit", "muss größer als 0 sein"},
        {ptr("/antrieb/motor/schleppmoment"), -1.0, "antrieb.motor.schleppmoment", "darf nicht negativ sein"},
        {ptr("/antrieb/kupplung/moment"), 0.0, "antrieb.kupplung.moment", "muss größer als 0 sein"},
        {ptr("/antrieb/getriebe/uebersetzungen/0"), 0.0, "antrieb.getriebe.uebersetzungen[0]",
         "muss größer als 0 sein"},
        {ptr("/antrieb/getriebe/uebersetzungen/3"), 2.0, "antrieb.getriebe.uebersetzungen[3]",
         "muss kleiner als im vorigen Gang sein"},
        {ptr("/antrieb/getriebe/achsuebersetzung"), 0.0, "antrieb.getriebe.achsuebersetzung",
         "muss größer als 0 sein"},
        {ptr("/antrieb/getriebe/schaltzeit"), -0.1, "antrieb.getriebe.schaltzeit", "darf nicht negativ sein"},
    };

    for (const Fall& fall : faelle) {
        const std::string pfad = fall.pfad;
        CAPTURE(pfad);
        nlohmann::json j = sportcoupe();
        j[fall.feld] = fall.wert;

        const LadeErgebnis ergebnis = lies(j);

        REQUIRE(ergebnis.fehler.has_value());
        CHECK(ergebnis.fehler->feld == fall.pfad);
        CHECK(ergebnis.fehler->meldung == fall.meldung);
    }
}

TEST_CASE("Ein Motor ohne Drehmomentkurve und ein Getriebe ohne Gänge werden abgelehnt") {
    for (const char* feld : {"/antrieb/motor/drehmoment", "/antrieb/getriebe/uebersetzungen"}) {
        const std::string zeiger = feld;
        CAPTURE(zeiger);
        nlohmann::json j = sportcoupe();
        j[nlohmann::json::json_pointer(zeiger)] = nlohmann::json::array();

        const LadeErgebnis ergebnis = lies(j);

        REQUIRE(ergebnis.fehler.has_value());
        CHECK(ergebnis.fehler->meldung == "darf nicht leer sein");
    }
}

TEST_CASE("Ein Stützpunkt der Drehmomentkurve, der kein Objekt ist, wird mit seinem Pfad gemeldet") {
    nlohmann::json j = sportcoupe();
    j["antrieb"]["motor"]["drehmoment"][1] = 42.0;

    const LadeErgebnis ergebnis = lies(j);

    REQUIRE(ergebnis.fehler.has_value());
    CHECK(ergebnis.fehler->feld == "antrieb.motor.drehmoment[1]");
    CHECK(ergebnis.fehler->meldung == "ist kein Objekt");
}

TEST_CASE("Unbrauchbare Eingaben liefern einen Fehler statt abzustürzen") {
    const char* const eingaben[] = {"", "   ", "42", "null", "{", "{\"masse\": \"\xff\"}", "{} {}"};

    for (const char* eingabe : eingaben) {
        const std::string text = eingabe;
        CAPTURE(text);

        const LadeErgebnis ergebnis = lies_fahrzeugdatei(text);

        REQUIRE(ergebnis.fehler.has_value());
        CHECK_FALSE(ergebnis.fehler->meldung.empty());
    }
}

TEST_CASE("Eine Fahrzeugdatei, deren Wurzel kein Objekt ist, wird als solche gemeldet") {
    const LadeErgebnis ergebnis = lies_fahrzeugdatei("[1, 2]");

    REQUIRE(ergebnis.fehler.has_value());
    CHECK(ergebnis.fehler->feld == "");
    CHECK(ergebnis.fehler->meldung == "ist kein Objekt");
}

TEST_CASE("Das Sportcoupé trägt Aero- und Rollwiderstandswerte") {
    const LadeErgebnis ergebnis = lade_fahrzeugdatei(sportcoupe_pfad);

    REQUIRE_FALSE(ergebnis.fehler.has_value());
    const Kennwerte& auto_ = ergebnis.kennwerte;
    CHECK(auto_.aero.luftwiderstand == doctest::Approx(0.72));
    CHECK(auto_.vorn.abtrieb > 0.0);
    CHECK(auto_.hinten.abtrieb > auto_.vorn.abtrieb);
    CHECK(auto_.hinten.reifen.rollwiderstand == doctest::Approx(0.012));
}

TEST_CASE("Fehlende oder unplausible Aero- und Rollwiderstandswerte werden mit ihrem Pfad gemeldet") {
    nlohmann::json j = sportcoupe();
    std::string erwartetes_feld;

    SUBCASE("kein aero-Objekt") {
        j.erase("aero");
        erwartetes_feld = "aero";
    }
    SUBCASE("negativer Abtrieb") {
        j["achsen"]["vorn"]["abtrieb"] = -0.1;
        erwartetes_feld = "achsen.vorn.abtrieb";
    }
    SUBCASE("Luftwiderstand null") {
        j["aero"]["luftwiderstand"] = 0.0;
        erwartetes_feld = "aero.luftwiderstand";
    }
    SUBCASE("absurder Rollwiderstand") {
        j["achsen"]["hinten"]["reifen"]["rollwiderstand"] = 1.2;
        erwartetes_feld = "achsen.hinten.reifen.rollwiderstand";
    }

    const LadeErgebnis ergebnis = lies(j);
    REQUIRE(ergebnis.fehler.has_value());
    CHECK(ergebnis.fehler->feld == erwartetes_feld);
}
