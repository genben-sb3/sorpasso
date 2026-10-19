# 03: Geradeaus bremsen

**What to build:** Ein rollendes Auto kann geradeaus bremsen, und der Bremsweg stimmt. Dafür kommen Reifen in Längsrichtung (Magic Formula mit Lastempfindlichkeit, geglätteter Schlupfnenner bei niedriger Geschwindigkeit), Raddrehung, Bremsen als begrenzter Impuls Richtung Stillstand mit Bremsbalance, Antrieb vorerst als direktes Radmoment sowie Belagzonen mit eigenem Reibwert auf dem Testgelände. Getestet wird über eine fest vorgegebene Folge von Eingabe-Paketen.

**Blocked by:** 01

**Status:** ready-for-agent

- [ ] Bremsweg aus 100 km/h auf trockenem Belag liegt nahe v²/(2·μ·g) (Toleranz).
- [ ] Auf einer nassen Belagzone ist der Bremsweg entsprechend länger.
- [ ] Beim Bremsen nickt die Karosserie nach vorn, die Vorderachse trägt mehr Last.
- [ ] Bei Vollbremsung blockieren die Räder sauber, ohne dass die Raddrehzahl um null schwingt.
- [ ] Ein direktes Radmoment beschleunigt das Auto aus dem Stand, ohne Zittern beim Anfahren.
