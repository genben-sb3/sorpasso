# 12: Traktionskontrolle, Stufen 0–5

**What to build:** Traktionskontrolle als Fahrhilfe mit den Stufen 0 (aus) bis 5, einstellbar über das Eingabe-Paket. Sie reduziert das Gas mit Hysterese; die Stufe bestimmt den Ziel-Schlupf. Getestet wird über eine fest vorgegebene Folge von Eingabe-Paketen (Vollgas aus dem Stand).

**Blocked by:** 06

**Status:** done
- [ ] Bei Vollgas aus dem Stand ist der Schlupf der Antriebsräder umso kleiner, je höher die Stufe.
- [ ] Stufe 0 verhält sich genau wie ohne Traktionskontrolle.
- [ ] Die Regelung pendelt nicht (kein ständiges An und Aus).
