# 16: Aufzeichnung und Ghost

**What to build:** Eine Fahrt lässt sich als Aufzeichnung in einem eigenen Binärformat speichern und wieder laden: Kennung, Versionsnummer, alle Eingabe-Pakete und alle 60 Ticks ein vollständiger Zustand. Aus einer Aufzeichnung lässt sich ein Ghost abspielen, der nur gespeicherte Zustände wiedergibt (mit Interpolation), und ein Replay, das die Fahrt neu simuliert.

**Blocked by:** 07

**Status:** ready-for-agent

- [ ] Schreiben und wieder Lesen ergibt genau dieselbe Aufzeichnung.
- [ ] Eine Datei mit fremder Kennung oder zu neuer Version wird verständlich abgelehnt.
- [ ] Ein Replay ergibt im selben Programm bitgenau dieselbe Fahrt (gleiche Prüfsumme).
- [ ] Ein Ghost liefert zu jedem Zeitpunkt Animationsdaten, ohne zu simulieren.
