# 13: Stabilitätskontrolle

**What to build:** Stabilitätskontrolle als Fahrhilfe, an- und abschaltbar über das Eingabe-Paket. Sie vergleicht Soll- und Ist-Gierrate und bremst einzelne Räder (bei Übersteuern kurvenäußeres Vorderrad, bei Untersteuern kurveninneres Hinterrad), dazu Gasreduktion.

**Blocked by:** 10

**Status:** ready-for-agent

- [ ] Im Spurwechsel erreicht das Auto mit Stabilitätskontrolle eine höhere Einfahrgeschwindigkeit ohne Dreher als ohne.
- [ ] Bei ruhiger Geradeausfahrt und stationärer Kreisfahrt fern vom Grenzbereich greift sie nicht ein.
- [ ] Ausgeschaltet verhält sich das Auto genau wie ohne Stabilitätskontrolle.
