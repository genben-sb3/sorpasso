#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

// Ein Punkt der Drehmomentkurve bei Vollgas. Dazwischen wird linear interpoliert.
struct Stuetzpunkt {
    double drehzahl = 0.0; // rad/s
    double moment = 0.0;   // N·m
};

struct Motor {
    std::vector<Stuetzpunkt> drehmoment; // nach Drehzahl aufsteigend
    double leerlaufdrehzahl = 0.0;       // rad/s
    double hoechstdrehzahl = 0.0;        // rad/s, darüber nimmt der Begrenzer das Gas weg
    double traegheit = 0.0;              // kg·m², Motor samt Schwungrad
    double schleppmoment = 0.0;          // N·m ohne Gas bei Höchstdrehzahl, linear mit der Drehzahl
};

struct Kupplung {
    double moment = 0.0; // N·m, das sie eingekuppelt höchstens überträgt
};

struct Getriebe {
    std::vector<double> uebersetzungen; // 1, vom ersten Gang an absteigend
    double achsuebersetzung = 0.0;      // 1
    double schaltzeit = 0.0;            // s ohne Kraftschluss je Gangwechsel
};

// Heckantrieb mit offenem Differenzial.
struct Antrieb {
    Motor motor;
    Kupplung kupplung;
    Getriebe getriebe;
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
