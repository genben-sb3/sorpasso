# 20: Testmanöver Kreisdrift und Achterdrift

**What to build:** Das Abstimm-Werkzeug bekommt zwei Testmanöver, die der Driftregler aus 19 fährt. **Kreisdrift:** Der Driftwinkel wird um einen festen Mittelpunkt gesteigert, solange der Radius in der Toleranz bleibt. Ergebnis ist der höchste Driftwinkel, der sich 10 s lang halten lässt. **Achterdrift:** Der Drift wechselt von einer Seite auf die andere. Beide Ergebnisse haben Zielwerte in der Fahrzeugdatei und erscheinen in der Tabelle, fließen aber nicht in den PP-Wert ein.

**Blocked by:** 10, 19

**Status:** ready-for-agent

- [ ] Der Kreisdrift hält den Radius in einer Toleranz; das Ergebnis ist der höchste Driftwinkel, der 10 s gehalten wird.
- [ ] Der Achterdrift erkennt einen gelungenen Seitenwechsel und meldet Dreher oder Verlassen der Bahn als „gescheitert“ mit Grund.
- [ ] Beide erscheinen in der Tabelle mit Zielwert und Abweichung; fehlende Zielwerte werden als „kein Ziel“ angezeigt.
- [ ] Der PP-Wert ändert sich nicht, wenn sich nur die Drift-Ergebnisse ändern.
- [ ] Ein Auto mit offenem Differenzial erreicht im Kreisdrift einen kleineren Driftwinkel als dasselbe Auto mit Sperrdifferenzial.

## Comments

- Aus dem Grilling vom 2026-10-20: Der PP-Wert misst Leistungsfähigkeit (ADR-0003). Ein Auto, das gut driftet, ist nicht schneller, deshalb zählen die Drift-Ergebnisse nicht für den PP-Wert.
- Beide Manöver laufen auf der Ebene des Testgeländes; Strecken mit Kurvenverlauf bleiben Out of Scope.
