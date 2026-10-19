# 04: Lenken und Kurven

**What to build:** Das Auto lässt sich lenken und fährt plausible Kurven. Dafür kommen Lenkung, Reifen in Querrichtung, kombinierter Schlupf über Gewichtungsfunktionen (Reibkreis), Relaxationslänge mit Untergrenze für die Geschwindigkeit und Stabilisatoren pro Achse. Getestet wird über eine fest vorgegebene Folge von Eingabe-Paketen.

**Blocked by:** 03

**Status:** ready-for-agent

- [ ] Bei konstanter Geschwindigkeit und konstanter Lenkung stellt sich eine stationäre Kreisfahrt ein; die Querbeschleunigung stimmt mit v²/r des gefahrenen Radius.
- [ ] Die maximale Querbeschleunigung liegt im Bereich des Reifen-Grips (Toleranz).
- [ ] In der Kurve wankt die Karosserie nach außen, die kurvenäußeren Räder tragen mehr Last.
- [ ] Bremsen in der Kurve verringert die Seitenführung (kombinierter Schlupf).
- [ ] Langsames Rangieren mit Lenkeinschlag bleibt stabil, ohne Zittern.
- [ ] Bei hoher Geschwindigkeit schwingt das Auto nach einem Lenkimpuls nicht auf.
