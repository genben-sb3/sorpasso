---
status: accepted
---

# Bitgenauer Determinismus nur innerhalb desselben Programms, nicht zwischen Plattformen

Gleiche Eingabe-Pakete ergeben im selben Programm auf demselben Rechner bitgenau dieselbe Fahrt; ein Test prüft das über eine Prüfsumme des Zustands. Bitgleichheit zwischen Linux (g++) und Windows (MSVC) sichern wir bewusst nicht zu: Sie würde eigene Mathematikfunktionen oder Festkomma und strenge Compiler-Schalter in jedem Modul verlangen. Replays und Ghosts brauchen sie nicht, weil die Aufzeichnung regelmäßig vollständige Zustände enthält.
