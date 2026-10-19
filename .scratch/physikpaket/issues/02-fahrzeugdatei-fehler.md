# 02: Fahrzeugdatei: Fehler und Plausibilität

**What to build:** Wer eine Fahrzeugdatei falsch schreibt, bekommt eine verständliche Fehlermeldung statt eines kaputten Autos. Kaputtes JSON, fehlende Felder und falsche Typen werden mit dem Pfad des betroffenen Felds gemeldet; unplausible Werte (zum Beispiel negative Masse, Reifen ohne Grip, Schwerpunkt außerhalb des Radstands) werden abgelehnt. Fehler kommen als Rückgabewert, ohne Exceptions.

**Blocked by:** 01

**Status:** ready-for-agent

- [ ] Eine gültige Fahrzeugdatei lädt ohne Fehler.
- [ ] Kaputtes JSON liefert einen Fehler mit Position.
- [ ] Ein fehlendes oder falsch getyptes Feld liefert einen Fehler mit Feldpfad.
- [ ] Unplausible Werte werden mit Feldpfad und Begründung abgelehnt.
- [ ] Der Ladecode wirft keine Exceptions.
