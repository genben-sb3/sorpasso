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
    double stabilisator = 0.0;   // N/m, Kraft je Meter Unterschied der Einfederung links und rechts
};

// Form der Magic-Formula-Kurve über einem Schlupf.
struct Kurvenform {
    double steifigkeit = 0.0; // B
    double form = 0.0;        // C
    double kruemmung = 0.0;   // E
};

// Kombinierter Schlupf: Jede Kraft wird mit G = cos(atan(B·x)) über dem
// jeweils anderen Schlupf gewichtet, so teilen sich Längs- und Seitenkraft den Grip.
struct Gewichtung {
    double laengs = 0.0; // B, schwächt die Längskraft über dem Schräglaufwinkel α in rad
    double quer = 0.0;   // B, schwächt die Seitenkraft über dem Längsschlupf κ
};

// Reifen nach der Magic Formula. Der Spitzenreibwert gilt bei Nennlast auf
// einem Belag mit Reibwert 1 und sinkt mit steigender Radlast.
struct Reifen {
    double spitzenreibwert = 0.0;     // D bei Nennlast
    double nennlast = 0.0;            // N
    double lastempfindlichkeit = 0.0; // Verlust an Reibwert (relativ) je Nennlast Mehrlast
    double traegheit = 0.0;           // kg·m², Rad und Reifen um die Drehachse
    double rollwiderstand = 0.0;      // 1, Rollwiderstandskraft je Newton Radlast
    double relaxationslaenge = 0.0;   // m Rollstrecke, bis die Seitenkraft 63 % erreicht
    Kurvenform laengs;                // über dem Längsschlupf κ (dimensionslos)
    Kurvenform quer;                  // über dem Schräglaufwinkel α in rad
    Gewichtung kombiniert;
};

// Eine Achse mit zwei gleichen Rädern links und rechts.
struct Achse {
    double x = 0.0;           // m, Lage der Achse in Längsrichtung
    double spurweite = 0.0;   // m
    double anlenkhoehe = 0.0; // m, Höhe des oberen Federbein-Anlenkpunkts
    double radradius = 0.0;   // m
    double abtrieb = 0.0;     // m², Auftriebsbeiwert mal Fläche (cL·A) der Achse, positiv drückt nach unten
    Fahrwerk fahrwerk;
    Reifen reifen;
};

struct Lenkung {
    double max_lenkwinkel = 0.0; // rad an beiden Vorderrädern bei vollem Ausschlag
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

enum class Getriebemodus { manuell, automatik };

// Schaltpunkte der Automatik. Sie rechnet mit der Motordrehzahl, die zur
// Geschwindigkeit über Grund gehört, nicht mit der gemessenen: Durchdrehende
// Räder treiben den Motor nicht in den nächsten Gang.
struct Automatik {
    double hochschaltdrehzahl = 0.0;      // rad/s, darüber schaltet sie hoch
    double runterschaltdrehzahl = 0.0;    // rad/s, darunter schaltet sie ohne Bremse herunter
    double bremsschaltdrehzahl = 0.0;     // rad/s, darunter schaltet sie bei Bremse herunter
    double haltezeit = 0.0;               // s, die ein Gang mindestens gehalten wird
    double einkuppelbereich = 0.0;        // rad/s über Leerlauf, bis die Kupplung voll greift
    double einkuppelzeit = 0.0;           // s, in denen die Kupplung nach dem Schalten wieder greift
    double driftwinkel = 0.0;             // rad, ab dem sie den Gang hält
};

struct Getriebe {
    std::vector<double> uebersetzungen; // 1, vom ersten Gang an absteigend
    double achsuebersetzung = 0.0;      // 1
    double schaltzeit = 0.0;            // s ohne Kraftschluss je Gangwechsel
    Getriebemodus modus = Getriebemodus::manuell;
    Automatik automatik;                // nur bei Modus automatik gelesen
};

enum class Antriebsart { heck, front, allrad };

// Lamellen-Sperrdifferenzial: Die Sperrwirkung hält die Räder einer Achse
// zusammen, bis das Sperrmoment ausgeschöpft ist. Es wächst mit dem
// Antriebsmoment der Achse.
struct Differenzial {
    bool sperrt = false;       // false: offenes Differenzial
    double vorspannung = 0.0;  // N·m Sperrmoment ohne Last
    double sperrwirkung = 0.0; // 1, zusätzliches Sperrmoment je Antriebsmoment der Achse
};

struct Antrieb {
    Antriebsart art = Antriebsart::heck;
    Differenzial differenzial;        // an jeder angetriebenen Achse
    double momentverteilung_vorn = 0.0; // 1, Anteil der Vorderachse am Antriebsmoment, nur beim Allrad
    Motor motor;
    Kupplung kupplung;
    Getriebe getriebe;
};

struct Aero {
    double luftwiderstand = 0.0; // m², Widerstandsbeiwert mal Stirnfläche (cW·A)
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
    Aero aero;
    Lenkung lenkung;
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
