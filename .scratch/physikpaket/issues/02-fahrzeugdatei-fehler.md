# 02: Fahrzeugdatei: Fehler und Plausibilität

**What to build:** Wer eine Fahrzeugdatei falsch schreibt, bekommt eine verständliche Fehlermeldung statt eines kaputten Autos. Kaputtes JSON, fehlende Felder und falsche Typen werden mit dem Pfad des betroffenen Felds gemeldet; unplausible Werte (zum Beispiel negative Masse, Reifen ohne Grip, Schwerpunkt außerhalb des Radstands) werden abgelehnt. Fehler kommen als Rückgabewert, ohne Exceptions.

**Blocked by:** 01

**Status:** done

- [x] Eine gültige Fahrzeugdatei lädt ohne Fehler.
- [x] Kaputtes JSON liefert einen Fehler mit Position.
- [x] Ein fehlendes oder falsch getyptes Feld liefert einen Fehler mit Feldpfad.
- [x] Unplausible Werte werden mit Feldpfad und Begründung abgelehnt. (Ohne „Reifen ohne Grip“: kommt mit den Reifen in 03.)
- [x] Der Ladecode wirft keine Exceptions.

## Comments

- Umgesetzt: Syntaxfehler nennen Zeile, Spalte und Token; die Wurzel muss ein Objekt sein; Plausibilität prüft Masse, Trägheit, Schwerpunkt (zwischen den Achsen, innerhalb der schmaleren Spur, über dem Boden), Achsreihenfolge, Spurweite, Anlenkhöhe, Radradius und Fahrwerkswerte. Es wird der erste Verstoß gemeldet.
- „Reifen ohne Grip“ ist noch offen, weil die Reifen erst mit 03 in die Fahrzeugdatei kommen; Hinweis steht in 03.
- Aus dem Review: Doppelte und unbekannte Schlüssel werden nicht gemeldet (der letzte gewinnt, Unbekanntes wird ignoriert). Für die heutigen Pflichtfelder reicht das; sobald optionale Felder dazukommen, sollten Tippfehler in Feldnamen gemeldet werden.
