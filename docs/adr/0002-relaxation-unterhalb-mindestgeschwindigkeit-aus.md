---
status: accepted
---

# Relaxationslänge wirkt erst oberhalb einer Mindestgeschwindigkeit

Der Schräglaufwinkel baut sich über die Relaxationslänge auf, aber nur oberhalb von `RelaxationMinSpeed` (Startwert 5 m/s). Darunter wirkt der kinematische Schräglaufwinkel sofort, bis `RelaxationFullSpeed` (10 m/s) wird die Verzögerung eingeblendet. Die Spec (#1) sah stattdessen nur eine Untergrenze für die Geschwindigkeit in der Verzögerungsgleichung vor.

Grund: Mit Verzögerung verhält sich der Reifen bei niedriger Geschwindigkeit wie eine Feder in Reihe mit einem Dämpfer. Mit den Startwerten (Schräglaufsteifigkeit ≈ 80 kN/rad, σ = 0,4 m, ein Viertel der Fahrzeugmasse pro Rad) liegt der Dämpfungsgrad dieser Schwingung im Stand unter 0,1, und eine Untergrenze von wenigen m/s ändert daran kaum etwas: Erst ab etwa 80 m/s Untergrenze wäre sie ausreichend gedämpft. Das geparkte oder rangierende Auto würde seitlich schaukeln. Ohne Verzögerung ist der Reifen dort ein reiner Dämpfer, der bei 960 Hz stabil rechnet. Oberhalb von 10 m/s ist die Schwingung ausreichend gedämpft (Dämpfungsgrad ≈ 0,5 und mehr), dort wirkt die volle Relaxation.

Den Längsschlupf verzögern wir gar nicht: Rad und Reifen ergäben dort eine kaum gedämpfte Schwingung um 25 Hz. Stattdessen wird die Raddrehung implizit gegen die Längssteifigkeit des Reifens integriert.

## Konsequenzen

- Unter etwa 36 km/h reagiert die Lenkung ohne Reifenverzögerung. Das verzögerte, progressive Einlenken (Story 8) kommt dort aus der Eingabe-Glättung (#11).
- `GTO.Sim.Driving` prüft das: Seitlich angestoßen kommt das stehende Auto zur Ruhe, ohne zurückzuschwingen.
- Fühlt sich das Einlenken bei mittlerer Geschwindigkeit im Playtest zu direkt an, lassen sich beide Schwellen als Daten absenken. Darunter bräuchte es eine eigene Dämpfung im Reifen.
