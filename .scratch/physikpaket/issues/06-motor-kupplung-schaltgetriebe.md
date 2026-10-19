# 06: Motor, Kupplung, Schaltgetriebe

**What to build:** Das direkte Radmoment wird durch einen echten Antriebsstrang ersetzt: Motor mit Drehmomentkurve aus der Fahrzeugdatei, Motorträgheit, Leerlauf und Motorbremse; Kupplung, die haftet oder rutscht (als begrenzter Impuls); manuelles Getriebe mit Übersetzungen und Schaltzeit; offenes Differenzial; Heckantrieb. Das Sportcoupé bekommt seine Antriebswerte (300 PS, sechs Gänge). Getestet wird über eine fest vorgegebene Folge von Eingabe-Paketen.

**Blocked by:** 03

**Status:** ready-for-agent

- [ ] Das Auto fährt aus dem Stand an und schaltet durch; 0–100 km/h liegt in plausibler Größenordnung.
- [ ] Gangwechsel und Kupplungsvorgänge erzeugen kein Schwingen der Motor- oder Raddrehzahl.
- [ ] Gas weg im Gang bremst das Auto über die Motorbremse.
- [ ] Im Leerlauf und bei getretener Kupplung hält der Motor seine Leerlaufdrehzahl.
- [ ] Mit zu viel Gas beim Anfahren drehen die Hinterräder durch.
- [ ] Die Drehzahl in den Animationsdaten ist jetzt die Motordrehzahl.
