---
status: accepted
---

# Das Physikpaket ersetzt GTOSim, seine Relaxation gilt

SORPASSO hatte zwei Fahrphysik-Kerne: den eigenen, einfachen Sim-Kern GTOSim und das getrennt entstandene Physikpaket „ultragravity“, das für dieses Spiel gebaut wurde und deutlich weiter ist (Antriebsstrang mit Automatik, Sperrdifferenzial, kombinierter Schlupf, Stabilisatoren, Aero, Fahrhilfen, Eingabe-Glättung, Animationsdaten, Telemetrie). Wir behalten nur das Physikpaket: Es kam als ein Commit ohne seine Historie ins Repo (Unreal-Modul `Source/UltraGravity`, CMake-Projekt `ultragravity/`), wird nur noch hier weiterentwickelt, und GTOSim fällt weg, sobald das Auto mit dem Paket in Unreal fährt. Zwei Kerne mit zwei Glossaren und zwei ADR-Reihen hätten jeden Beitrag Zeit gekostet; GTOSim auf den Stand des Pakets auszubauen hätte dessen Arbeit wiederholt.

Für den Reifen gilt das Modell des Pakets: Der Schräglaufwinkel folgt bei jeder Geschwindigkeit über die Relaxationslänge, mit einer Untergrenze der Geschwindigkeit in der Verzögerungsgleichung, damit er im Stand nicht einfriert. Das löst ADR 0002 ab, das die Relaxation unterhalb einer Mindestgeschwindigkeit abschaltete und erst darüber einblendete. (Nachtrag aus #23: Das hielt in Unreal nicht, das stehende Auto schaukelte nach einem seitlichen Stoß; ADR 0002 ist wieder in Kraft, siehe dort.)

## Konsequenzen

- Schaukelt das stehende Auto in Unreal nach einem seitlichen Stoß (die Sorge aus ADR 0002), übernimmt das Paket die Einblendung aus ADR 0002, statt dass der Adapter sie nachbaut. So kam es (#23): ADR 0002 ist wieder in Kraft, siehe dort.
- Physik prüfen die doctest-Tests des CMake-Projekts; Unreal-Automation prüft nur noch das Zusammenspiel von Adapter und Chaos (`GTO.Smoke.`). Die `GTO.Sim.`-Tests fallen mit GTOSim weg.
- ADR 0001 (Adapter im Async-Physics-Tick, gemittelte Kräfte über 8 Substeps) gilt unverändert für den neuen Adapter; das Paket rechnet wie Chaos mit 1/120 s und 8 Substeps.
