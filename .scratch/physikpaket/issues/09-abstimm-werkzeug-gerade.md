# 09: Testfahrer und Abstimm-Werkzeug: gerade Testmanöver

**What to build:** Ein Kommandozeilenprogramm vermisst ein Auto. Es lädt eine Fahrzeugdatei samt Zielwerten, lässt den Testfahrer die geraden Testmanöver fahren (0–100 km/h, 0–200 km/h, Bremsweg 100–0 km/h ohne ABS, Höchstgeschwindigkeit) und gibt eine Tabelle mit Zielwert, Ergebnis und Abweichung aus. Der Testfahrer steuert nur über Eingabe-Pakete. Ein gescheitertes Testmanöver wird als gescheitert gemeldet, nicht mit einer falschen Zahl.

**Blocked by:** 06, 08

**Status:** ready-for-agent

- [ ] Ein Befehl vermisst das Sportcoupé und zeigt die Tabelle im Terminal (Umlaute korrekt).
- [ ] Zielwerte stehen in der Fahrzeugdatei; fehlende Zielwerte werden als „kein Ziel“ angezeigt.
- [ ] Jedes Ergebnis stimmt mit einer unabhängigen Messung am Verlauf der Fahrt überein.
- [ ] Ein Testmanöver, dessen Ziel nicht erreicht wird (zum Beispiel 200 km/h bei zu wenig Leistung), meldet „gescheitert“ mit Grund.
- [ ] Der Testfahrer erzeugt ausschließlich Eingabe-Pakete.
