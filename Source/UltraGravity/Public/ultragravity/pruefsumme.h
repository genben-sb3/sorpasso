#pragma once

#include <cstdint>

#include "ultragravity/fahrzeugschritt.h"

namespace ultragravity {

// Prüfsumme über die Bitmuster des gesamten Zustands. Gleiche Fahrt im selben
// Programm ergibt dieselbe Prüfsumme (ADR-0005).
std::uint64_t pruefsumme(const Zustand& zustand, const Karosseriebewegung& karosserie);

} // namespace ultragravity
