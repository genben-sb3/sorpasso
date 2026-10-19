# 05: Animationsdaten und Schnittstellenvertrag

**What to build:** Der Fahrzeug-Schritt liefert pro Tick die Animationsdaten (Raddrehung, Lenkwinkel, Federweg je Rad, Nicken, Wanken, Lenkrad, Drehzahl), und das Physikpaket bietet eine Interpolation zwischen den letzten beiden Ticks für beliebige Bildraten. Dazu kommen die Vertragstests an Naht 2, die festhalten, was der spätere Unreal-Adapter voraussetzen darf. Die Drehzahl ist bis zum Antriebsstrang die Raddrehzahl-basierte Größe, die 06 später ersetzt.

**Blocked by:** 04

**Status:** ready-for-agent

- [ ] Die Animationsdaten passen zum Zustand (zum Beispiel Federweg und Nicken beim Bremsen, Lenkwinkel bei Lenkung).
- [ ] Die Interpolation liefert bei Anteil 0 und 1 genau die beiden Ticks und dazwischen stetige Werte; Raddrehwinkel springen nicht beim Überlauf.
- [ ] Ohne Bodenkontakt eines Rads entsteht an diesem Rad keine Reifenkraft.
- [ ] Ein Fahrzeug-Schritt verändert keinen anderen Zustand als seinen eigenen; gleicher Eingang ergibt gleichen Ausgang.
