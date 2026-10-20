#pragma once

#include <cstdint>

#include "ultragravity/eingabe.h"
#include "ultragravity/fahrzeugdatei.h"
#include "ultragravity/fahrzeugschritt.h"

namespace ultragravity {

// Rohe Signale von Tastatur oder Gamepad. Tasten liefern -1, 0 oder 1.
struct RohEingabe {
    double lenkung = 0.0;       // -1 (rechts) … 1 (links)
    double gas = 0.0;           // 0 … 1
    double bremse = 0.0;        // 0 … 1
    double kupplung = 0.0;      // 0 … 1, wird durchgereicht
    double handbremse = 0.0;    // 0 … 1, wird durchgereicht
    std::int8_t gangwunsch = 0; // wird durchgereicht
    Fahrhilfen fahrhilfen;      // wird durchgereicht
};

// Wie schnell die Glättung Signale aufbaut und die Lenkung begrenzt.
struct GlaettungsParameter {
    double lenk_aufbauzeit = 0.30;  // s von Mitte bis voller Ausschlag
    double lenk_abbauzeit = 0.15;   // s von vollem Ausschlag zurück zur Mitte
    double gas_aufbauzeit = 0.25;   // s von 0 bis 1
    double gas_abbauzeit = 0.10;    // s von 1 bis 0
    double bremse_aufbauzeit = 0.20; // s von 0 bis 1
    double bremse_abbauzeit = 0.10;  // s von 1 bis 0
    double gegenlenk_verstaerkung = 4.0; // 1, Lenkung je rad Driftwinkel
    double gegenlenk_totzone = 0.05;     // rad, Driftwinkel darunter lenkt die Hilfe nicht
    double gegenlenk_mindesttempo = 3.0; // m/s, darunter gibt es keine Hilfe
    double peak_anteil = 0.8;            // 1, Anteil des Peak-Schräglaufwinkels, den die Lenkgrenze ausnutzt
    double lenktasten_totzone = 0.05;    // 1, Roh-Lenkung darunter gilt als losgelassen
};

// Der Zustand der Glättung zwischen zwei Ticks: die zuletzt ausgegebenen Werte.
struct GlaettungsZustand {
    double lenkung = 0.0; // -1 … 1
    double gas = 0.0;     // 0 … 1
    double bremse = 0.0;  // 0 … 1
};

struct GlaettungsErgebnis {
    EingabePaket paket;
    GlaettungsZustand zustand;
};

// Größter Lenkausschlag (0 … 1), den ein Auto bei dieser Geschwindigkeit
// sinnvoll nutzen kann: Der Vorderreifen soll den Peak-Schräglaufwinkel (zu
// peak_anteil) nicht überschreiten, zuzüglich dem Lenkwinkel, den die Kurve bei voller
// Querbeschleunigung kinematisch verlangt.
double lenkgrenze(const Kennwerte& kennwerte, double geschwindigkeit, double peak_anteil);

// Macht aus Rohsignalen ein Eingabe-Paket. Eine reine Funktion: liest den
// letzten Zustand der Karosserie und schreibt nichts zurück. Testfahrer und KI
// rufen sie nicht auf.
GlaettungsErgebnis glaette(const GlaettungsZustand& zustand, const GlaettungsParameter& parameter,
                           const Kennwerte& kennwerte, const Karosseriebewegung& karosserie,
                           const RohEingabe& roh, std::uint32_t tick, double dt);

} // namespace ultragravity
