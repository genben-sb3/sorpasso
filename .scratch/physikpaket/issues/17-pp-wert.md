# 17: PP-Wert

**What to build:** Das Abstimm-Werkzeug berechnet aus den gewichteten Ergebnissen der Testmanöver (Beschleunigung, Querbeschleunigung, Bremsweg, Höchstgeschwindigkeit) einen PP-Wert und schreibt ihn zusammen mit einer Kennung des Physikstands in die Fahrzeugdatei (ADR-0003). Passt die gespeicherte Kennung nicht mehr zum aktuellen Physikstand, warnt das Werkzeug.

**Blocked by:** 10, 11

**Status:** ready-for-agent

- [ ] Ein Auto, das in allen Testmanövern besser ist als ein anderes, bekommt mehr PP.
- [ ] Der PP-Wert und die Kennung des Physikstands stehen nach dem Lauf in der Fahrzeugdatei; der übrige Inhalt bleibt unverändert.
- [ ] Bei veraltetem PP-Wert erscheint eine Warnung.
- [ ] Ein gescheitertes Testmanöver führt zu einer klaren Meldung statt zu einem PP-Wert.
