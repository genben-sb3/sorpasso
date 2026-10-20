# 04: Lenken und Kurven

**What to build:** Das Auto lässt sich lenken und fährt plausible Kurven. Dafür kommen Lenkung, Reifen in Querrichtung, kombinierter Schlupf über Gewichtungsfunktionen (Reibkreis), Relaxationslänge mit Untergrenze für die Geschwindigkeit und Stabilisatoren pro Achse. Getestet wird über eine fest vorgegebene Folge von Eingabe-Paketen.

**Blocked by:** 03

**Status:** done

- [x] Bei konstanter Geschwindigkeit und konstanter Lenkung stellt sich eine stationäre Kreisfahrt ein; die Querbeschleunigung stimmt mit v²/r des gefahrenen Radius.
- [x] Die maximale Querbeschleunigung liegt im Bereich des Reifen-Grips (Toleranz).
- [x] In der Kurve wankt die Karosserie nach außen, die kurvenäußeren Räder tragen mehr Last.
- [x] Bremsen in der Kurve verringert die Seitenführung (kombinierter Schlupf).
- [x] Langsames Rangieren mit Lenkeinschlag bleibt stabil, ohne Zittern.
- [x] Bei hoher Geschwindigkeit schwingt das Auto nach einem Lenkimpuls nicht auf.

## Comments

- Aus 03: `Kurvenform` (B/C/E) gibt es schon für `reifen.laengs`; der Querreifen bekommt eine zweite `Kurvenform`. `fahrzeugschritt` ist lang geworden: beim Einbau von Quer- und kombiniertem Schlupf die Reifen- und Radrechnung in eigene Funktionen ziehen. Die Längsrichtung der Räder kommt heute aus der Karosserie-X-Achse; die Vorderräder brauchen dann den Lenkwinkel.
- Umgesetzt: `lenkung.max_lenkwinkel_grad` (Parallellenkung, kein Ackermann; Plausibilität 0 < δ ≤ 60°, damit das Rad nie querstellt), je Reifen `quer` (zweite `Kurvenform` über α in rad), `relaxationslaenge` und `kombiniert` (B-Faktoren der Gewichtung 1/√(1+(B·x)²): `laengs` schwächt Fx über α, `quer` schwächt Fy über κ), je Achse `fahrwerk.stabilisator` (N/m). Der Zustand führt den relaxierten Schräglauf je Rad; die Prüfsumme nimmt ihn mit.
- Modell: Der Schräglauf folgt dem kinematischen über die Relaxationslänge, exakt je Zwischenschritt über die Strecke, die das Rad über den Boden zurücklegt (|v|, Untergrenze 1 m/s); ohne Bodenkontakt fällt er auf 0. Der Stabilisator verschiebt Last zwischen den Rädern einer Achse, höchstens so viel, wie das entlastete Rad trägt. In den Zwischenschritten wird weiter nur die Geschwindigkeit der Karosserie mitgeführt, nicht ihre Drehung (ausprobiert, kein Test braucht es). `fahrzeugschritt.cpp` ist aufgeteilt in `federung`, `stabilisatorlast`, `richte_aus` und `reifenkraft`.
- Tests: `tests/kurven_test.cpp` (Testwelt mit Eingabe-Paketen: Radius bei 4 m/s, stationärer Kreis, höchste Querbeschleunigung 0,90 μ·g, Wanken mit Momentengleichgewicht auf 2 %, Stabilisatoren, Bremsen in der Kurve, Rangieren, Lenkimpuls bei 45 m/s, Prüfsumme), dazu in `tests/fahrzeugschritt_test.cpp` direkt am Fahrzeug-Schritt: Relaxation über die Strecke, im Stand, bei 30° Rutschwinkel, Vergessen ohne Kontakt, Fx-Gewichtung über α (0,82 bei 5°), Stabilisator bei abgehobenem Rad. Per Mutation geprüft: Fx-Gewichtung, |vx| statt |v|, ungebremster Stabilisator, Haftung 0,8, kleiner Schlupfnenner, B_quer = 4 (Bremstest).
- Für später: Der Lenkimpuls-Test schlägt auch ohne Relaxation nicht an; das Auto ist bei 45 m/s sehr stabil (Gegenausschlag ≈ 0). Ein schärferer Test gehört zu 10 (Spurwechsel). Der Antriebsstrang steckt noch in `fahrzeugschritt` (Kommentar aus 06). `Gewichtung` hält nur die B-Faktoren; umbenennen, falls 19 weitere Kopplungsgrößen braucht.
