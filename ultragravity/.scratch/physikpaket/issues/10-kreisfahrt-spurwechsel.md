# 10: Testmanöver Kreisfahrt und Spurwechsel

**What to build:** Der Testfahrer bekommt einen Lenkregler, und das Abstimm-Werkzeug zwei weitere Testmanöver: Kreisfahrt auf 100 m Radius (Ergebnis: maximale Querbeschleunigung in g) und Spurwechsel nach Art des Elchtests (Ergebnis: höchste Einfahrgeschwindigkeit ohne Dreher).

**Blocked by:** 04, 09

**Status:** ready-for-agent

- [ ] Die Kreisfahrt hält den Radius in einer Toleranz und steigert die Geschwindigkeit, bis der Grip erschöpft ist; das Ergebnis ist die höchste stationäre Querbeschleunigung.
- [ ] Der Spurwechsel erkennt Dreher und Verlassen der Gasse und sucht die höchste Einfahrgeschwindigkeit, die noch gelingt.
- [ ] Beide erscheinen in der Tabelle mit Zielwert und Abweichung.
- [ ] Ein Auto mit mehr Reifen-Grip erreicht in der Kreisfahrt mehr g.
