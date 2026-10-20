#pragma once

#include <iosfwd>

#include "ultragravity/eingabe.h"
#include "ultragravity/fahrzeugschritt.h"

namespace ultragravity {

// Telemetrie als CSV: eine Kopfzeile mit Einheiten in eckigen Klammern, danach
// genau eine Zeile pro Tick. Zahlen stehen mit Punkt und so, dass sie sich
// verlustfrei zurücklesen lassen.
void schreibe_telemetrie_kopfzeile(std::ostream& aus);

// Die Zeile für den Tick, den `ergebnis` aus `eingabe` berechnet hat.
void schreibe_telemetrie_zeile(std::ostream& aus, const EingabePaket& eingabe, const SchrittErgebnis& ergebnis);

} // namespace ultragravity
