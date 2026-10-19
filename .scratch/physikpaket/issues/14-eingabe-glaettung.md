# 14: Eingabe-Glättung

**What to build:** Die Eingabe-Glättung macht aus rohen Tastatur- oder Gamepad-Signalen ein fahrbares Eingabe-Paket: getrennte Raten für Auf- und Abbau der Lenkung, weiches Gas und Bremsen, Lenkbegrenzung abhängig von der Geschwindigkeit bis zum Peak-Schräglaufwinkel und Gegenlenkhilfe über den Driftwinkel. Sie liest den letzten Zustand nur. Testfahrer und spätere KI umgehen sie.

**Blocked by:** 04

**Status:** ready-for-agent

- [ ] Ein voller Tastendruck baut die Lenkung über die eingestellte Zeit auf; Loslassen stellt schneller zurück.
- [ ] Gas und Bremse steigen bei digitaler Eingabe weich an.
- [ ] Bei hoher Geschwindigkeit ist der wirksame Lenkausschlag begrenzt; ein voller Tastendruck lenkt nicht über den Grip hinaus (in der Testwelt prüfbar).
- [ ] Rutscht das Heck und wird die Lenktaste losgelassen, lenkt die Gegenlenkhilfe in Richtung Fahrtrichtung.
- [ ] Gleiche Rohsignale und gleicher Zustand ergeben dasselbe Eingabe-Paket.
