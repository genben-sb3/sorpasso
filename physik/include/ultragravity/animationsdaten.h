#pragma once

#include <array>

namespace ultragravity {

inline constexpr int anzahl_raeder = 4;

// Der sichtbare Zustand eines Autos pro Tick. Die Darstellung liest ihn nur.
// Winkel sind ISO 8855 (ADR-0004): Nicken positiv mit der Nase nach unten,
// Wanken positiv mit der linken Seite nach oben, Lenkung positiv nach links.
struct Animationsdaten {
    std::array<double, anzahl_raeder> raddrehwinkel{}; // rad, positiv beim Vorwärtsrollen, in [-π, π)
    std::array<double, anzahl_raeder> lenkwinkel{};    // rad, je Rad (hinten null)
    std::array<double, anzahl_raeder> federweg{};      // m Einfederung ab voll ausgefedert
    double nicken = 0.0;                               // rad
    double wanken = 0.0;                               // rad
    double lenkrad = 0.0;                              // rad, Drehung des Lenkrads
    double drehzahl = 0.0;                             // rad/s des Motors
};

// Wickelt einen Winkel in [-π, π).
double wickle_winkel(double winkel);

// Zwischenwert zweier aufeinanderfolgender Ticks für beliebige Bildraten.
// anteil 0 liefert genau a, anteil 1 genau b; dazwischen wird linear gemischt.
// Der Raddrehwinkel nimmt den kurzen Weg über den Überlauf bei ±π (ein Rad
// dreht pro Tick weniger als eine halbe Umdrehung).
Animationsdaten interpoliere(const Animationsdaten& a, const Animationsdaten& b, double anteil);

} // namespace ultragravity
