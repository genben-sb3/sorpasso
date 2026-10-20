#include "doctest/doctest.h"

#include <sstream>
#include <string>
#include <vector>

#include "fahrt.h"
#include "ultragravity/telemetrie_csv.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

std::vector<std::string> getrennt(const std::string& text, char trenner) {
    std::vector<std::string> aus;
    std::stringstream ss(text);
    for (std::string f; std::getline(ss, f, trenner);) aus.push_back(f);
    return aus;
}

struct Lauf {
    std::string csv;
    std::vector<SchrittErgebnis> schritte;
    std::vector<EingabePaket> eingaben;
    std::uint64_t pruefsumme = 0;
};

// Fährt an, lenkt hin und her und bremst dann; schreibt wahlweise die Telemetrie mit.
Lauf fahre_manoever(bool mit_telemetrie, int ticks) {
    Testwelt welt(sportcoupe(), 0.02, Testgelaende{}, 20.0);
    std::ostringstream aus;
    if (mit_telemetrie) schreibe_telemetrie_kopfzeile(aus);
    Lauf lauf;
    for (int i = 0; i < ticks; ++i) {
        const bool gasphase = i < ticks / 2;
        EingabePaket p = fahren(gasphase ? 0.8 : 0.0, gasphase ? 0.0 : 0.5, i % 40 < 20 ? 0.3 : -0.3);
        p.tick = static_cast<std::uint32_t>(i);
        welt.tick(p);
        if (mit_telemetrie) schreibe_telemetrie_zeile(aus, p, welt.letzter_schritt());
        lauf.schritte.push_back(welt.letzter_schritt());
        lauf.eingaben.push_back(p);
    }
    lauf.csv = aus.str();
    lauf.pruefsumme = welt.pruefsumme();
    return lauf;
}

} // namespace

TEST_CASE("Die CSV hat eine Kopfzeile mit Einheiten und genau eine Zeile pro Tick") {
    const Lauf lauf = fahre_manoever(true, 240);
    const auto z = getrennt(lauf.csv, '\n');
    REQUIRE(z.size() == 241);

    const auto kopf = getrennt(z[0], ',');
    CHECK(kopf.size() == 9 + 5 * anzahl_raeder);
    for (const std::string& name : kopf) {
        CAPTURE(name);
        CHECK(name.find('[') != std::string::npos);
        CHECK(name.back() == ']');
    }
    for (std::size_t i = 1; i < z.size(); ++i) CHECK(getrennt(z[i], ',').size() == kopf.size());
}

TEST_CASE("Die Werte einer Zeile stimmen mit dem Zustand des Ticks überein") {
    const Lauf lauf = fahre_manoever(true, 240);
    const auto z = getrennt(lauf.csv, '\n');
    const auto kopf = getrennt(z[0], ',');
    const auto spalte = [&](const std::string& name) {
        for (std::size_t i = 0; i < kopf.size(); ++i) {
            if (kopf[i] == name) return i;
        }
        FAIL("Spalte fehlt: " << name);
        return std::size_t{0};
    };

    const std::size_t v = spalte("geschwindigkeit[m/s]"), n = spalte("motordrehzahl[rad/s]");
    const std::size_t g = spalte("gang[1]"), t = spalte("tick[1]"), gas = spalte("gas[1]");
    const std::size_t lenk = spalte("lenkung[1]"), brems = spalte("bremse[1]");
    const std::size_t last = spalte("radlast_hr[N]"), schlupf = spalte("schlupf_vl[1]");
    const std::size_t fx = spalte("kraft_laengs_vr[N]"), fy = spalte("kraft_quer_hl[N]");
    const std::size_t alpha = spalte("schraeglauf_vr[rad]");

    bool kraefte_gesehen = false;
    for (std::size_t i = 0; i < lauf.schritte.size(); ++i) {
        const auto f = getrennt(z[i + 1], ',');
        const SchrittErgebnis& e = lauf.schritte[i];
        const EingabePaket& p = lauf.eingaben[i];
        CAPTURE(i);
        CHECK(std::stoull(f[t]) == p.tick);
        CHECK(std::stod(f[v]) == e.telemetrie.geschwindigkeit);
        CHECK(std::stod(f[n]) == e.zustand.motordrehzahl);
        CHECK(std::stoi(f[g]) == e.zustand.gang);
        CHECK(std::stod(f[gas]) == p.gas);
        CHECK(std::stod(f[lenk]) == p.lenkung);
        CHECK(std::stod(f[brems]) == p.bremse);
        CHECK(std::stod(f[last]) == e.telemetrie.radlast[hinten_rechts]);
        CHECK(std::stod(f[schlupf]) == e.telemetrie.schlupf[vorn_links]);
        CHECK(std::stod(f[fx]) == e.telemetrie.kraft_laengs[vorn_rechts]);
        CHECK(std::stod(f[fy]) == e.telemetrie.kraft_quer[hinten_links]);
        CHECK(std::stod(f[alpha]) == e.zustand.schraeglauf[vorn_rechts]);
        if (e.telemetrie.kraft_quer[hinten_links] != 0.0 && e.telemetrie.kraft_laengs[vorn_rechts] != 0.0) {
            kraefte_gesehen = true;
        }
    }
    CHECK(kraefte_gesehen);
    // Das Manöver fährt wirklich: Das Auto bewegt sich und hat Last auf den Rädern.
    CHECK(lauf.schritte.back().telemetrie.geschwindigkeit > 5.0);
    CHECK(lauf.schritte.back().telemetrie.radlast[hinten_rechts] > 1000.0);
}

TEST_CASE("Die Telemetrie verändert die Fahrt nicht") {
    const Lauf mit = fahre_manoever(true, 240);
    const Lauf ohne = fahre_manoever(false, 240);
    CHECK(mit.pruefsumme == ohne.pruefsumme);
    CHECK(mit.pruefsumme != 0);
}
