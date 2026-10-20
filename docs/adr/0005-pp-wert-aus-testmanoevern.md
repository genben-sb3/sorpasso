---
status: accepted
---

# Der PP-Wert entsteht aus gefahrenen Testmanövern, nicht aus einer Formel über die Kennwerte

Eine Formel über Leistung, Gewicht, Grip und Abtrieb wäre einfacher und bräuchte kein Fahren, bewertet aber das Datenblatt statt des Fahrverhaltens: Ein Auto mit guten Kennwerten, das schlecht fährt, bekäme zu viele PP. Deshalb rechnet das Abstimm-Werkzeug den PP-Wert im Terminal aus gewichteten Ergebnissen der Testmanöver (Beschleunigung, Querbeschleunigung, Bremsweg, Höchstgeschwindigkeit) und schreibt ihn in die Fahrzeugdatei. Das Spiel liest nur die fertige Zahl; ändert sich die Physik, muss der PP-Wert neu berechnet werden.
