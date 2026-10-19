#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "ultragravity/vektor.h"

namespace ultragravity {

struct Fahrwerk {
    double federrate = 0.0;      // N/m
    double daempfer_druck = 0.0; // N·s/m beim Einfedern
    double daempfer_zug = 0.0;   // N·s/m beim Ausfedern
    double federweg = 0.0;       // m vom voll ausgefederten Rad bis zum Anschlag
    double anschlagrate = 0.0;   // N/m zusätzlich hinter dem Anschlag
};

// Form der Magic-Formula-Kurve über einem Schlupf.
struct Kurvenform {
    double steifigkeit = 0.0; // B
    double form = 0.0;        // C
    double kruemmung = 0.0;   // E
};

// Reifen nach der Magic Formula. Der Spitzenreibwert gilt bei Nennlast auf
// einem Belag mit Reibwert 1 und sinkt mit steigender Radlast.
struct Reifen {
    double spitzenreibwert = 0.0;     // D bei Nennlast
    double nennlast = 0.0;            // N
    double lastempfindlichkeit = 0.0; // Verlust an Reibwert (relativ) je Nennlast Mehrlast
    double traegheit = 0.0;           // kg·m², Rad und Reifen um die Drehachse
    Kurvenform laengs;                // über dem Längsschlupf κ (dimensionslos)
};

// Eine Achse mit zwei gleichen Rädern links und rechts.
struct Achse {
    double x = 0.0;           // m, Lage der Achse in Längsrichtung
    double spurweite = 0.0;   // m
    double anlenkhoehe = 0.0; // m, Höhe des oberen Federbein-Anlenkpunkts
    double radradius = 0.0;   // m
    Fahrwerk fahrwerk;
    Reifen reifen;
};

struct Bremsen {
    double moment = 0.0;  // N·m an allen vier Rädern zusammen bei voll getretener Bremse
    double balance = 0.0; // Anteil der Vorderachse, 0 … 1
};

// Vorläufiger Antrieb bis zum Antriebsstrang: ein direktes Moment an den
// Hinterrädern, verteilt zu gleichen Teilen.
struct Antrieb {
    double radmoment = 0.0; // N·m an der Hinterachse zusammen bei Vollgas
};

// Alle Kennwerte eines Autos. Alle Positionen liegen im selben Fahrzeugsystem
// (ISO 8855); dessen Ursprung ist frei, üblich ist die Mitte der Vorderachse
// auf Bodenhöhe in Konstruktionslage.
struct Kennwerte {
    std::string name;
    double masse = 0.0;  // kg
    Vec3 traegheit;      // kg·m², Hauptträgheitsmomente um den Schwerpunkt
    Vec3 schwerpunkt;    // m
    Achse vorn;
    Achse hinten;
    Bremsen bremsen;
    Antrieb antrieb;
};

struct LadeFehler {
    std::string feld;    // Pfad des betroffenen Felds, z. B. "achsen.vorn.spurweite"
    std::string meldung;
};

struct LadeErgebnis {
    Kennwerte kennwerte;
    std::optional<LadeFehler> fehler;
};

// Liest eine Fahrzeugdatei aus JSON-Text. Wirft keine Exceptions.
LadeErgebnis lies_fahrzeugdatei(std::string_view json);

// Lädt eine Fahrzeugdatei von der Platte. Wirft keine Exceptions.
LadeErgebnis lade_fahrzeugdatei(const std::string& pfad);

} // namespace ultragravity
