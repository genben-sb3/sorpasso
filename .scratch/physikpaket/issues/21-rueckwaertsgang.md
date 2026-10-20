# 21: Rückwärtsgang

**What to build:** `gangwunsch = -1` im Eingabe-Paket legt den Rückwärtsgang ein, statt wie bisher wie Leerlauf zu wirken. Die Fahrzeugdatei bekommt eine Übersetzung für den Rückwärtsgang. Eingelegt wird er nur nahe am Stillstand, im manuellen Getriebe und in der Automatik gleich.

**Blocked by:** 07

**Status:** ready-for-agent

- [ ] Mit eingelegtem Rückwärtsgang und Gas fährt das Auto aus dem Stand rückwärts an, ohne Zittern.
- [ ] Oberhalb einer kleinen Vorwärtsgeschwindigkeit wird der Rückwärtsgang nicht eingelegt; das Auto bleibt im Leerlauf.
- [ ] Die Automatik schaltet im Rückwärtsgang nicht selbstständig.
- [ ] Die Fahrzeugdatei verlangt eine Übersetzung für den Rückwärtsgang und lehnt unplausible Werte mit Feldpfad ab.
- [ ] Die Raddrehung in den Animationsdaten läuft beim Rückwärtsfahren rückwärts.

## Comments

- Aus 06: Dort ist der Rückwärtsgang bewusst weggelassen; `gangwunsch = -1` wirkt bisher wie Leerlauf.
