#include "doctest/doctest.h"

#include <algorithm>
#include <cmath>

#include "fahrt.h"
#include "ultragravity/pruefsumme.h"

using namespace ultragravity;
using namespace ultragravity::test;

namespace {

struct Vollgasfahrt {
    double spitzenschlupf = 0.0;   // 1, größter Schlupf der Hinterräder nach dem Anfahren
    double mittlerer_schlupf = 0.0; // 1
    double geschwindigkeit = 0.0;  // m/s am Ende
    int richtungswechsel = 0;      // des Gasfaktors, ab Rauschen
    std::uint64_t pruefsumme = 0;
};

// Vollgas aus dem Stand mit der Automatik, mit der Traktionskontrolle auf der Stufe.
Vollgasfahrt vollgas(int stufe, double sekunden = 4.0) {
    const Kennwerte auto_ = mit_automatik();
    Testwelt welt(auto_, 0.02);
    Fahrt fahrt(welt);
    fahrt.fahre(1.5);
    EingabePaket paket = fahren(1.0);
    paket.fahrhilfen.traktionskontrolle = stufe;

    Vollgasfahrt f;
    int n = 0;
    double faktor_vorher = 1.0;
    int richtung = 0;
    const int ticks = Fahrt::ticks(sekunden);
    for (int tick = 0; tick < ticks; ++tick) {
        fahrt.einen_tick(paket);
        const auto& z = welt.letzter_schritt().zustand;
        const double v = laengsgeschwindigkeit(welt);
        const double schlupf = std::max((z.raddrehzahl[hinten_links] - v / auto_.hinten.radradius),
                                        (z.raddrehzahl[hinten_rechts] - v / auto_.hinten.radradius)) *
                               auto_.hinten.radradius / std::sqrt(v * v + 1.0);
        // Die ersten 0,5 s kuppelt die Automatik erst ein.
        if (tick > Fahrt::ticks(0.5)) {
            f.spitzenschlupf = std::max(f.spitzenschlupf, schlupf);
            f.mittlerer_schlupf += schlupf;
            ++n;
        }
        const double d = z.traktionsfaktor - faktor_vorher;
        if (std::abs(d) > 1e-6) {
            const int neu = d > 0.0 ? 1 : -1;
            if (richtung != 0 && neu != richtung) ++f.richtungswechsel;
            richtung = neu;
        }
        faktor_vorher = z.traktionsfaktor;
    }
    f.mittlerer_schlupf /= std::max(n, 1);
    f.geschwindigkeit = laengsgeschwindigkeit(welt);
    f.pruefsumme = pruefsumme(welt.letzter_schritt().zustand, welt.karosserie());
    return f;
}

} // namespace

TEST_CASE("Bei Vollgas aus dem Stand ist der Schlupf umso kleiner, je höher die Stufe") {
    double vorher = 1e9;
    for (int stufe = 0; stufe <= 5; ++stufe) {
        const Vollgasfahrt f = vollgas(stufe);
        CAPTURE(stufe);
        CAPTURE(f.mittlerer_schlupf);
        CHECK(f.mittlerer_schlupf < vorher);
        vorher = f.mittlerer_schlupf;
    }
}

TEST_CASE("Stufe 0 verhält sich genau wie ohne Traktionskontrolle") {
    const Kennwerte auto_ = mit_automatik();
    Testwelt aus(auto_, 0.02);
    Testwelt stufe0(auto_, 0.02);
    Fahrt fahrt_aus(aus);
    Fahrt fahrt_stufe0(stufe0);
    EingabePaket mit_stufe0 = fahren(1.0);
    mit_stufe0.fahrhilfen.traktionskontrolle = 0;
    fahrt_aus.fahre(1.0, fahren(1.0));
    fahrt_stufe0.fahre(1.0, mit_stufe0);
    CHECK(pruefsumme(aus.letzter_schritt().zustand, aus.karosserie()) ==
          pruefsumme(stufe0.letzter_schritt().zustand, stufe0.karosserie()));
    // Und sie dreht ohne Hilfe merklich durch, sonst bewiese der Vergleich nichts.
    CHECK(vollgas(0).spitzenschlupf > 1.0); // gemessen: etwa 7
}

TEST_CASE("Die Traktionskontrolle pendelt nicht") {
    for (int stufe = 1; stufe <= 5; ++stufe) {
        const Vollgasfahrt f = vollgas(stufe);
        CAPTURE(stufe);
        CAPTURE(f.richtungswechsel);
        // Gemessen: höchstens 5 Wechsel in 4 s (Abregeln, Halten, Aufregeln); ein
        // Schalten an und aus je Tick läge bei über 100.
        CHECK(f.richtungswechsel <= 6);
        // Sie bremst nicht ins Leere: Das Auto kommt mit ihr voran.
        CHECK(f.geschwindigkeit > 6.0);
    }
}
