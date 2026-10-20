#pragma once

#include <cmath>

#include "ultragravity/fahrzeugdatei.h"

namespace ultragravity {

// Magic Formula über einem Schlupf, normiert auf den Spitzenwert 1, samt ihrer
// Ableitung nach dem Schlupf.
struct Kurvenpunkt {
    double wert;
    double steigung;
};

inline Kurvenpunkt magic_formula(const Kurvenform& k, double schlupf) {
    const double b = k.steifigkeit;
    const double bx = b * schlupf;
    const double phi = bx - k.kruemmung * (bx - std::atan(bx));
    const double dphi = b - k.kruemmung * (b - b / (1.0 + bx * bx));
    const double winkel = k.form * std::atan(phi);
    return {std::sin(winkel), std::cos(winkel) * k.form / (1.0 + phi * phi) * dphi};
}

// Schlupf, bei dem die Kurve ihren Spitzenwert erreicht (Steigung null), für
// eine Kurve mit Spitze unterhalb von grenze.
inline double spitzenschlupf(const Kurvenform& k, double grenze) {
    double unten = 0.0, oben = grenze; // gleiche Einheit wie der Schlupf
    for (int i = 0; i < 60; ++i) {
        const double mitte = 0.5 * (unten + oben); // wie der Schlupf
        (magic_formula(k, mitte).steigung > 0.0 ? unten : oben) = mitte;
    }
    return 0.5 * (unten + oben);
}

} // namespace ultragravity
