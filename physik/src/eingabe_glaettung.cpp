#include "ultragravity/eingabe_glaettung.h"

#include <algorithm>
#include <cmath>

#include "magic_formula.h"

namespace ultragravity {
namespace {

constexpr double erdbeschleunigung = 9.81; // m/s²
constexpr double schlupf_suchgrenze = 1.0; // rad, darunter liegt der Peak jeder brauchbaren Kurve
constexpr double tempo_minimum = 1.0;      // m/s, gegen die Division im Quadrat

// Nähert wert dem ziel mit konstanter Rate (Einheit je Sekunde).
double naehere(double wert, double ziel, double rate, double dt) {
    const double schritt = rate * dt;
    return wert + std::clamp(ziel - wert, -schritt, schritt);
}

// Lenkung: vom Wert weg in Richtung Ausschlag baut sie langsam auf, zurück
// zur Mitte und über die Mitte hinweg stellt sie schneller zurück.
double lenkrate(double wert, double ziel, const GlaettungsParameter& p) {
    const bool baut_auf = std::abs(ziel) > std::abs(wert) && (wert == 0.0 || (ziel > 0.0) == (wert > 0.0));
    return 1.0 / (baut_auf ? p.lenk_aufbauzeit : p.lenk_abbauzeit);
}

double gleite(double wert, double ziel, double aufbauzeit, double abbauzeit, double dt) {
    return naehere(wert, ziel, 1.0 / (ziel > wert ? aufbauzeit : abbauzeit), dt);
}

} // namespace

double lenkgrenze(const Kennwerte& kennwerte, double geschwindigkeit, double peak_anteil) {
    const Reifen& reifen = kennwerte.vorn.reifen;
    const double peak = peak_anteil * spitzenschlupf(reifen.quer, schlupf_suchgrenze); // rad
    const double radstand = kennwerte.vorn.x - kennwerte.hinten.x;       // m
    const double v = std::max(std::abs(geschwindigkeit), tempo_minimum); // m/s
    const double kinematisch = radstand * reifen.spitzenreibwert * erdbeschleunigung / (v * v); // rad
    return std::clamp((peak + kinematisch) / kennwerte.lenkung.max_lenkwinkel, 0.0, 1.0);
}

GlaettungsErgebnis glaette(const GlaettungsZustand& zustand, const GlaettungsParameter& p,
                           const Kennwerte& kennwerte, const Karosseriebewegung& karosserie,
                           const RohEingabe& roh, std::uint32_t tick, double dt) {
    const Vec3 v = drehe_zurueck(karosserie.ausrichtung, karosserie.geschwindigkeit); // m/s, Fahrzeugsystem
    const double tempo = std::hypot(v.x, v.y);                                       // m/s
    const double driftwinkel = std::atan2(v.y, std::max(v.x, 0.0));                  // rad, links positiv

    const double roh_lenkung = std::clamp(roh.lenkung, -1.0, 1.0);
    double ziel = roh_lenkung * lenkgrenze(kennwerte, tempo, p.peak_anteil);
    if (std::abs(roh_lenkung) < p.lenktasten_totzone && tempo > p.gegenlenk_mindesttempo &&
        std::abs(driftwinkel) > p.gegenlenk_totzone) {
        // Gegenlenken in Fahrtrichtung: Fährt das Auto nach links aus der Längsachse (vy > 0), lenkt die Hilfe links.
        const double ueber = std::abs(driftwinkel) - p.gegenlenk_totzone; // rad
        ziel = std::copysign(std::min(ueber * p.gegenlenk_verstaerkung, 1.0), driftwinkel);
    }

    GlaettungsErgebnis e;
    e.zustand.lenkung = naehere(zustand.lenkung, ziel, lenkrate(zustand.lenkung, ziel, p), dt);
    e.zustand.gas = gleite(zustand.gas, std::clamp(roh.gas, 0.0, 1.0), p.gas_aufbauzeit, p.gas_abbauzeit, dt);
    e.zustand.bremse =
        gleite(zustand.bremse, std::clamp(roh.bremse, 0.0, 1.0), p.bremse_aufbauzeit, p.bremse_abbauzeit, dt);

    e.paket.tick = tick;
    e.paket.lenkung = e.zustand.lenkung;
    e.paket.gas = e.zustand.gas;
    e.paket.bremse = e.zustand.bremse;
    e.paket.kupplung = roh.kupplung;
    e.paket.handbremse = roh.handbremse;
    e.paket.gangwunsch = roh.gangwunsch;
    e.paket.fahrhilfen = roh.fahrhilfen;
    return e;
}

} // namespace ultragravity
