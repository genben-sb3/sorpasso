#include "doctest/doctest.h"

#include "ultragravity/fahrzeugdatei.h"

using namespace ultragravity;

TEST_CASE("Das Sportcoupé lädt aus seiner Fahrzeugdatei") {
    const LadeErgebnis ergebnis = lade_fahrzeugdatei(UG_FAHRZEUGE_DIR "/sportcoupe.json");

    REQUIRE_FALSE(ergebnis.fehler.has_value());
    const Kennwerte& auto_ = ergebnis.kennwerte;
    CHECK(auto_.name == "Sportcoupé");
    CHECK(auto_.masse == doctest::Approx(1350.0));
    CHECK(auto_.vorn.x - auto_.hinten.x == doctest::Approx(2.6));
    CHECK(auto_.hinten.fahrwerk.daempfer_zug > auto_.hinten.fahrwerk.daempfer_druck);
}
