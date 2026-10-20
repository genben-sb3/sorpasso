# 03: Geradeaus bremsen

**What to build:** Ein rollendes Auto kann geradeaus bremsen, und der Bremsweg stimmt. Dafür kommen Reifen in Längsrichtung (Magic Formula mit Lastempfindlichkeit, geglätteter Schlupfnenner bei niedriger Geschwindigkeit), Raddrehung, Bremsen als begrenzter Impuls Richtung Stillstand mit Bremsbalance, Antrieb vorerst als direktes Radmoment sowie Belagzonen mit eigenem Reibwert auf dem Testgelände. Getestet wird über eine fest vorgegebene Folge von Eingabe-Paketen.

**Blocked by:** 01

**Status:** done

- [x] Bremsweg aus 100 km/h auf trockenem Belag liegt nahe v²/(2·μ·g) (Toleranz).
- [x] Auf einer nassen Belagzone ist der Bremsweg entsprechend länger.
- [x] Beim Bremsen nickt die Karosserie nach vorn, die Vorderachse trägt mehr Last.
- [x] Bei Vollbremsung blockieren die Räder sauber, ohne dass die Raddrehzahl um null schwingt.
- [x] Ein direktes Radmoment beschleunigt das Auto aus dem Stand, ohne Zittern beim Anfahren.

## Comments

- Aus dem Review von 01: In 01 wirkt nur die Radlast entlang der Bodennormalen, der Reibwert wird noch nicht gelesen. „Kein Wegrutschen“ aus 01 ist deshalb erst mit Reifenkräften wirklich geprüft. Den Stand-Test aus `tests/standstill_test.cpp` hier erneut grün bekommen. Ebenso kommen hier die 8 Zwischenschritte und die über den Schritt gemittelten Kräfte (Spec, Naht 2) dazu.
- Aus 02: Die Plausibilitätsprüfung (`check` in `Source/UltraGravity/Private/vehicle_file.cpp`) kennt noch keine Reifen. Sobald die Reifen-Koeffizienten in die Fahrzeugdatei kommen, dort „Reifen ohne Grip“ (Spitzenreibwert ≤ 0) und unsinnige Koeffizienten ablehnen und in `tests/vehicle_file_test.cpp` mittesten.
- Umgesetzt: Reifen pro Achse in der Fahrzeugdatei (`tire` mit Spitzenreibwert, Nennlast, Lastempfindlichkeit, Drehträgheit und `longitudinal` B/C/E), `brakes` (Moment, Balance) und vorläufig `antrieb.radmoment` an der Hinterachse. Der Fahrzeug-Schritt rechnet 8 Zwischenschritte. Die Raddrehung ist halbimplizit (die Reifenkraft wird um die neue Drehzahl linearisiert), sonst zittert das Rad bei kleinem Schlupfnenner; per Mutation geprüft, Stand- und Anfahr-Test fallen dann. Die Bremse ist ein begrenzter Impuls Richtung ω = 0. Schlupfnenner √(v² + (1 m/s)²). Belagzonen sind Rechtecke im Testgelände.
- Bremsweg trocken: 41,4 m statt ideal 37,5 m (+10 %), weil blockierte Räder bei κ = −1 nur etwa 0,915 des Spitzenwerts liefern. Das passt zu 11, wo ABS kürzer bremsen soll.
- Bewusste Abweichungen: Die Tests starten mit 100 km/h über `TestWorld(…, speed)` und `rolling_state`, nicht durch Beschleunigen über Pakete (dafür fehlt der Antriebsstrang). Last hängt nur am Spitzenreibwert D, B/C/E bleiben fest (nicht voll Pac94). In den Zwischenschritten wird nur die lineare Karosseriegeschwindigkeit mit den Reifenkräften mitgeführt; Radlast und Drehung bleiben je Tick fest.
- Aus dem Review, offen: Die an die Karosserie gegebene Reifenkraft gehört zu der Drehzahl vor dem Bremsimpuls (ein Zwischenschritt Verzug, beim Blockieren praktisch null). `vehicle_step` wird lang; spätestens mit dem Querreifen in 04 Reifen- und Radrechnung herausziehen. Das Bremsweg-Band 0,98…1,15 ist bewusst weit; in 09/11 mit den Testmanövern schärfen.
