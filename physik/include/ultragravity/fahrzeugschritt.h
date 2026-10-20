#pragma once

#include <array>

#include "ultragravity/animationsdaten.h"
#include "ultragravity/eingabe.h"
#include "ultragravity/fahrzeugdatei.h"
#include "ultragravity/vektor.h"

namespace ultragravity {

// Dichte der Luft, aus der Luftwiderstand und Abtrieb ihre Kraft ziehen.
inline constexpr double luftdichte = 1.2; // kg/m³

// Reihenfolge der Räder in allen Feldern pro Rad.
enum Rad { vorn_links = 0, vorn_rechts = 1, hinten_links = 2, hinten_rechts = 3 };

// Bewegung der Karosserie im Weltsystem, so wie sie die Testwelt oder später
// Chaos liefert. Die Position ist die des Schwerpunkts.
struct Karosseriebewegung {
    Vec3 position;
    Quat ausrichtung;
    Vec3 geschwindigkeit;
    Vec3 drehgeschwindigkeit;
};

// Der Strahl, mit dem die Umgebung den Boden unter einem Rad abtastet:
// vom Federbein-Anlenkpunkt nach unten, so lang wie Federweg plus Radradius.
struct Abtaststrahl {
    Vec3 start;
    Vec3 richtung; // Einheitsvektor
    double laenge = 0.0;
};

struct Radkontakt {
    bool kontakt = false;
    Vec3 punkt;
    Vec3 normale;
    double reibwert = 0.0; // der Belagzone
};

using Radkontakte = std::array<Radkontakt, anzahl_raeder>;

// Kraft und Moment um den Schwerpunkt im Weltsystem, über den Schritt gemittelt.
struct Kraefte {
    Vec3 kraft;
    Vec3 moment;
};

// Vollständiger, kopierbarer Zustand des Fahrzeugmodells.
struct Zustand {
    std::array<double, anzahl_raeder> raddrehzahl{}; // rad/s, positiv beim Vorwärtsrollen
    std::array<double, anzahl_raeder> raddrehwinkel{}; // rad, in [-π, π)
    std::array<double, anzahl_raeder> schraeglauf{}; // rad, dem Reifen über die Relaxationslänge nachgeführt
    double motordrehzahl = 0.0;                      // rad/s
    int gang = 0;                                    // 0 Leerlauf, 1 … n Vorwärtsgänge
    double schaltrest = 0.0;                         // s, bis der Gang greift
    double gangalter = 1000.0;                       // s seit dem letzten Gangwechsel
    double traktionsfaktor = 1.0;                    // 1, Anteil des Gaspedals, den die Traktionskontrolle durchlässt
};


// Messwerte eines Ticks zur Analyse. Sie sind reine Ausgabe und wirken nicht
// auf die Fahrt zurück. Schlupf und Reifenkräfte sind über die Zwischenschritte
// des Ticks gemittelt; die Kräfte wirken entlang Roll- und Querrichtung des Rads.
struct Telemetrie {
    std::array<double, anzahl_raeder> radlast{};      // N
    std::array<double, anzahl_raeder> schlupf{};      // 1, Längsschlupf
    std::array<double, anzahl_raeder> schraeglauf{};  // rad, am Ende des Ticks
    std::array<double, anzahl_raeder> kraft_laengs{}; // N, Reifenkraft in Rollrichtung
    std::array<double, anzahl_raeder> kraft_quer{};   // N, Reifenkraft quer zur Rollrichtung, positiv nach links
    double geschwindigkeit = 0.0;                     // m/s, Karosserie in Fahrzeug-Längsrichtung
};

struct SchrittErgebnis {
    Kraefte kraefte;
    Zustand zustand;
    Animationsdaten animationsdaten;
    Telemetrie telemetrie;
};

std::array<Abtaststrahl, anzahl_raeder> abtaststrahlen(const Kennwerte& kennwerte,
                                                       const Karosseriebewegung& karosserie);

// Zustand eines Autos, das mit der gegebenen Geschwindigkeit im Leerlauf
// geradeaus rollt.
Zustand rollender_zustand(const Kennwerte& kennwerte, double geschwindigkeit);

// Der Fahrzeug-Schritt: eine reine Funktion ohne globalen Zustand.
SchrittErgebnis fahrzeugschritt(const Zustand& zustand, const Kennwerte& kennwerte,
                                const EingabePaket& eingabe, const Radkontakte& kontakte,
                                const Karosseriebewegung& karosserie, double dt);

} // namespace ultragravity
