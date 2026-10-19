#pragma once

#include <array>

#include "ultragravity/eingabe.h"
#include "ultragravity/fahrzeugdatei.h"
#include "ultragravity/vektor.h"

namespace ultragravity {

// Reihenfolge der Räder in allen Feldern pro Rad.
enum Rad { vorn_links = 0, vorn_rechts = 1, hinten_links = 2, hinten_rechts = 3 };
inline constexpr int anzahl_raeder = 4;

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

// Vollständiger, kopierbarer Zustand des Fahrzeugmodells. Relaxations- und
// Antriebszustände kommen mit Querreifen und Antriebsstrang dazu.
struct Zustand {
    std::array<double, anzahl_raeder> raddrehzahl{}; // rad/s, positiv beim Vorwärtsrollen
};


struct Animationsdaten {
    std::array<double, anzahl_raeder> federweg{}; // m Einfederung ab voll ausgefedert
};

struct Telemetrie {
    std::array<double, anzahl_raeder> radlast{}; // N
};

struct SchrittErgebnis {
    Kraefte kraefte;
    Zustand zustand;
    Animationsdaten animationsdaten;
    Telemetrie telemetrie;
};

std::array<Abtaststrahl, anzahl_raeder> abtaststrahlen(const Kennwerte& kennwerte,
                                                       const Karosseriebewegung& karosserie);

// Zustand eines Autos, das mit der gegebenen Geschwindigkeit geradeaus rollt.
Zustand rollender_zustand(const Kennwerte& kennwerte, double geschwindigkeit);

// Der Fahrzeug-Schritt: eine reine Funktion ohne globalen Zustand.
SchrittErgebnis fahrzeugschritt(const Zustand& zustand, const Kennwerte& kennwerte,
                                const EingabePaket& eingabe, const Radkontakte& kontakte,
                                const Karosseriebewegung& karosserie, double dt);

} // namespace ultragravity
