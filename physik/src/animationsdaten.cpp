#include "ultragravity/animationsdaten.h"

#include <cmath>
#include <numbers>

namespace ultragravity {
namespace {

double mische(double a, double b, double anteil) { return anteil == 1.0 ? b : a + (b - a) * anteil; }

} // namespace

double wickle_winkel(double winkel) {
    constexpr double pi = std::numbers::pi;
    if (winkel >= -pi && winkel < pi) return winkel;
    const double gewickelt = winkel - 2.0 * pi * std::floor((winkel + pi) / (2.0 * pi));
    // Rundung kann genau π liefern.
    return gewickelt >= pi ? -pi : gewickelt;
}

Animationsdaten interpoliere(const Animationsdaten& a, const Animationsdaten& b, double anteil) {
    Animationsdaten r;
    for (int i = 0; i < anzahl_raeder; ++i) {
        // Kürzester Weg von a nach b über den Überlauf.
        const double schritt = wickle_winkel(b.raddrehwinkel[i] - a.raddrehwinkel[i]);
        r.raddrehwinkel[i] = anteil == 1.0 ? b.raddrehwinkel[i] : wickle_winkel(a.raddrehwinkel[i] + schritt * anteil);
        r.lenkwinkel[i] = mische(a.lenkwinkel[i], b.lenkwinkel[i], anteil);
        r.federweg[i] = mische(a.federweg[i], b.federweg[i], anteil);
    }
    r.nicken = mische(a.nicken, b.nicken, anteil);
    r.wanken = mische(a.wanken, b.wanken, anteil);
    r.lenkrad = mische(a.lenkrad, b.lenkrad, anteil);
    r.drehzahl = mische(a.drehzahl, b.drehzahl, anteil);
    return r;
}

} // namespace ultragravity
