---
status: accepted
---

# Die Testwelt ersetzt Chaos an genau der Naht, an der in Unreal der Adapter sitzt

In Unreal bewegt Chaos die Karosserie und liefert Raycasts; im Terminal gibt es beides nicht. Die Testwelt übernimmt diese Rolle außerhalb des Physikpakets: Sie integriert die Karosserie als starren Körper im Chaos-Takt (Startwert 120 Hz) und tastet eine aus Daten beschriebene Strecke (Flächen mit Höhe und Reibwert) pro Rad ab. Das Physikpaket sieht nur Radkontakte und Karosseriebewegung als Eingang und gibt Kräfte zurück, so wie gegenüber dem Unreal-Adapter. Damit prüfen die Terminal-Tests dieselbe Schnittstelle, die in Unreal gilt. Die Testwelt gehört nur zum CMake-Projekt unter `ultragravity/` und kommt nicht ins Unreal-Modul.
