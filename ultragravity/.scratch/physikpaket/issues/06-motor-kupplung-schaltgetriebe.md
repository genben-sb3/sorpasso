# 06: Motor, Kupplung, Schaltgetriebe

**What to build:** Das direkte Radmoment wird durch einen echten Antriebsstrang ersetzt: Motor mit Drehmomentkurve aus der Fahrzeugdatei, Motorträgheit, Leerlauf und Motorbremse; Kupplung, die haftet oder rutscht (als begrenzter Impuls); manuelles Getriebe mit Übersetzungen und Schaltzeit; offenes Differenzial; Heckantrieb. Das Sportcoupé bekommt seine Antriebswerte (300 PS, sechs Gänge). Getestet wird über eine fest vorgegebene Folge von Eingabe-Paketen.

**Blocked by:** 03

**Status:** done

- [x] Das Auto fährt aus dem Stand an und schaltet durch; 0–100 km/h liegt in plausibler Größenordnung.
- [x] Gangwechsel und Kupplungsvorgänge erzeugen kein Schwingen der Motor- oder Raddrehzahl.
- [x] Gas weg im Gang bremst das Auto über die Motorbremse.
- [x] Im Leerlauf und bei getretener Kupplung hält der Motor seine Leerlaufdrehzahl.
- [x] Mit zu viel Gas beim Anfahren drehen die Hinterräder durch.
- [x] Die Drehzahl in den Animationsdaten ist jetzt die Motordrehzahl.

## Comments

- Aus 03: `antrieb.radmoment` in der Fahrzeugdatei ist das vorläufige direkte Radmoment (Hinterachse, halb und halb). Es wird hier ersetzt; `Antrieb` in `fahrzeugdatei.h` und der Test „Ein direktes Radmoment beschleunigt…“ in `tests/geradeaus_test.cpp` ziehen dann um. Die Kupplung kann denselben begrenzten Impuls nutzen wie die Bremse in `fahrzeugschritt.cpp`.
- Umgesetzt: `antrieb` in der Fahrzeugdatei mit `motor` (Drehmomentkurve als Stützpunkte in U/min, Leerlauf, Höchstdrehzahl, Trägheit, Schleppmoment), `kupplung.moment` und `getriebe` (Übersetzungen, Achsübersetzung, Schaltzeit). Die Datei nennt Drehzahlen in U/min, die Kennwerte rechnen in rad/s (ADR-0004). `antrieb.radmoment` und der Test „Ein direktes Radmoment…“ sind entfernt. Der Zustand hat jetzt Motordrehzahl, Gang und Schaltrest; die Prüfsumme nimmt sie mit. `Animationsdaten::drehzahl` ist schon hier angelegt (Motordrehzahl in rad/s), weil 05 noch offen ist.
- Modell je Zwischenschritt: Motor explizit (Leerlaufregler über 100 U/min unter Leerlauf, Begrenzer, Schleppmoment linear mit der Drehzahl), dann die Räder halbimplizit wie in 03, danach die Kupplung als begrenzter Impuls zwischen Motor und Getriebeeingang (Mittel der Hinterräder, offenes Differenzial: gleicher Impuls je Seite), zuletzt die Bremse. Ein Gangwechsel trennt den Kraftschluss für die Schaltzeit.
- Tests in `tests/antrieb_test.cpp` (Naht 1) und `tests/fahrzeugdatei_test.cpp` (Naht 3). Jeder Antriebstest ist per Mutation geprüft: ohne Leerlaufregler, ohne Schleppmoment, mit schwacher Kupplung, mit gesperrtem Differenzial und mit einer überschießenden Kupplung pro Tick fallen die jeweiligen Tests.
- 0–100 km/h: 6,0 s mit dem Testfahrer (Teilgas 0,75/0,95 im ersten und zweiten Gang). Untere Grenze im Test aus der Traktion der Hinterachse (≈ 4,5 s), obere 7,0 s. Das Band ist bewusst weit; schärfen in 09/18 (dort notiert). Der Anfahrvorgang des Testfahrers ist grob (der Motor sackt auf ~800 U/min ab), mit Vollgas drehen die Räder stark durch (6,7 s).
- Bewusste Abweichungen: Kein Rückwärtsgang (`gangwunsch = -1` wirkt wie Leerlauf); das braucht noch ein Ticket. Teilgas skaliert das Vollgasmoment linear, ein echtes Drosselkennfeld fehlt.
- Für das Review: Schwingen innerhalb eines Ticks (zwischen den Zwischenschritten) sieht Naht 1 grundsätzlich nicht. Eine Kupplung, die pro Zwischenschritt um den Faktor 1,9 oder 2,05 überschießt, fällt nicht auf, weil der Impuls sie begrenzt und 8 Zwischenschritte das Vorzeichen wieder gleich machen. Die Reifenlinearisierung kennt nur die Radträgheit, nicht die über die haftende Kupplung angekoppelte Motorträgheit. `fahrzeugschritt` ist noch länger geworden, der Antriebsstrang sollte herausgezogen werden. Die Fahrt-Helfer liegen jetzt in `tests/fahrt.h`; `stand_test.cpp` nutzt noch seine eigene Kopie.
