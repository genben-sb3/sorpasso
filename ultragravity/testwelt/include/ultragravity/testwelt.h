#pragma once

#include <cstdint>
#include <vector>

#include "ultragravity/eingabe.h"
#include "ultragravity/fahrzeugdatei.h"
#include "ultragravity/fahrzeugschritt.h"

namespace ultragravity {

// Ein rechteckiger Bereich des Testgeländes mit eigenem Reibwert.
struct Belagzone {
    double x_von = 0.0, x_bis = 0.0; // m
    double y_von = 0.0, y_bis = 0.0; // m
    double reibwert = 0.0;
};

// Die Strecke der Testwelt: eine unbegrenzte Ebene in Höhe null. Außerhalb der
// Belagzonen gilt der allgemeine Reibwert; überlappen sich Zonen, zählt die erste.
struct Testgelaende {
    double reibwert = 1.0;
    std::vector<Belagzone> belagzonen;

    Radkontakt taste_ab(const Abtaststrahl& strahl) const;
};

// Ersatz für Unreal im Terminal: bewegt die Karosserie als starren Körper und
// ruft den Fahrzeug-Schritt so auf, wie es später der Unreal-Adapter tut.
class Testwelt {
public:
    static constexpr double zeitschritt = 1.0 / 120.0; // s
    static constexpr double erdbeschleunigung = 9.81;  // m/s²

    // Setzt das Auto waagerecht ab, die Räder hängen absetzhoehe über dem Boden.
    // Mit einer Geschwindigkeit rollt es dabei geradeaus in Richtung X.
    Testwelt(Kennwerte kennwerte, double absetzhoehe, Testgelaende gelaende = {}, double geschwindigkeit = 0.0);

    void tick(const EingabePaket& eingabe);

    const Karosseriebewegung& karosserie() const { return karosserie_; }
    const SchrittErgebnis& letzter_schritt() const { return letzter_schritt_; }
    std::uint64_t pruefsumme() const;

private:
    Kennwerte kennwerte_;
    Testgelaende gelaende_;
    Karosseriebewegung karosserie_;
    Zustand zustand_;
    SchrittErgebnis letzter_schritt_;
};

} // namespace ultragravity
