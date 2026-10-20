#pragma once

#include <cstdint>

namespace ultragravity {

struct Fahrhilfen {
    bool abs = false;
    int traktionskontrolle = 0; // Stufe 0 (aus) bis 5
    bool stabilitaetskontrolle = false;
};

// Die Steuerbefehle für einen Sim-Tick. Einziger Weg, ein Auto zu steuern:
// gleich für Spieler, KI, Replay und Ghost.
struct EingabePaket {
    std::uint32_t tick = 0;
    double lenkung = 0.0;    // -1 (rechts) … 1 (links)
    double gas = 0.0;        // 0 … 1
    double bremse = 0.0;     // 0 … 1
    double kupplung = 0.0;   // 0 (eingekuppelt) … 1 (getreten)
    double handbremse = 0.0; // 0 … 1
    std::int8_t gangwunsch = 0; // -1 Rückwärtsgang, 0 Leerlauf, 1 … n Vorwärtsgänge
    Fahrhilfen fahrhilfen;
};

} // namespace ultragravity
