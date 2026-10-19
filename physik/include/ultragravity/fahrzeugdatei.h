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

// Eine Achse mit zwei gleichen Rädern links und rechts.
struct Achse {
    double x = 0.0;           // m, Lage der Achse in Längsrichtung
    double spurweite = 0.0;   // m
    double anlenkhoehe = 0.0; // m, Höhe des oberen Federbein-Anlenkpunkts
    double radradius = 0.0;   // m
    Fahrwerk fahrwerk;
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
