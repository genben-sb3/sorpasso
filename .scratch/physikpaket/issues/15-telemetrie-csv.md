# 15: Telemetrie als CSV

**What to build:** Die Telemetrie einer Fahrt lässt sich als CSV mit Kopfzeile und einer Zeile pro Tick ausgeben: Schlupf, Radlast und Kräfte je Rad, Geschwindigkeit, Motordrehzahl, Gang, Eingaben. Das Abstimm-Werkzeug kann die Telemetrie eines Testmanövers auf Wunsch in eine Datei schreiben.

**Blocked by:** 06

**Status:** ready-for-agent

- [ ] Die CSV hat eine Kopfzeile mit Einheiten und genau eine Zeile pro Tick.
- [ ] Die Werte einer Zeile stimmen mit dem Zustand des jeweiligen Ticks überein.
- [ ] Die Telemetrie verändert die Fahrt nicht (gleiche Prüfsumme mit und ohne).
