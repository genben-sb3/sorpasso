# 🏁 SORPASSO

Ein Racing-Spiel im Stil von Gran Turismo, gebaut mit **Unreal Engine 5.8** und **C++**.
Das Herzstück ist ein **eigenes Fahrphysik-Modell**: Unreal liefert nur Starrkörper und
Kollision, Reifen, Federung und Fahrverhalten rechnet SORPASSO selbst.

> 🚧 Frühe Entwicklungsphase. Ein Auto fährt auf einer Teststrecke mit Rundenzeit.
> Ziele und Entscheidungen stehen in [`DESIGN.md`](DESIGN.md).

## Was schon da ist

- **Reifenmodell** nach der Pacejka Magic Formula
- **Federung und Gewichtsverlagerung** pro Rad
- **Fester Zeitschritt mit Substepping**, die Simulation läuft im Async-Physics-Tick von Unreal
- **Eingabe-Pakete** (Lenkung, Gas, Bremse): gleiche Schnittstelle für Spieler, später auch für KI, Replays und Ghosts
- **Teststrecke** (~2,4 km) als Procedural Mesh mit Curbs, Start-Ziel-Linie und **Rundenzeit**
- **HUD** mit aktueller, letzter und bester Runde und Grip-Telemetrie pro Rad

## Aufbau

| Ordner | Inhalt |
|---|---|
| `Source/GTOSim` | Sim-Kern: Reifen, Federung, Starrkörper, Eingabe-Queue. Hängt nur von `Core` ab, rechnet in SI-Einheiten |
| `Source/GTO` | Unreal-Seite: Fahrzeug-Pawn, Teststrecke, Rundenzeit, HUD |
| `Scripts/` | Python-Skript, das Testmap und Assets im Editor erzeugt |
| `docs/adr/` | Architektur-Entscheidungen |
| `research/` | Recherche zu Fahrphysik und Asset-Lizenzen |

Simulation und Darstellung sind getrennt: Der Sim-Kern weiss nichts von Unreal-Actors.
Darum lässt er sich ohne Grafik im **Headless-Harness** über viele Ticks testen.

## Tests

Unreal Automation Tests (`GTO.`, Sim-Kern `GTO.Sim.`): Reifenmodell, Federung, Substepping,
Eingabe-Queue, Fahrverhalten, Streckenlayout und Rundenzeit. Befehle zum Bauen und Testen
stehen in [`CLAUDE.md`](CLAUDE.md).

## Starten

Voraussetzung: Windows, Unreal Engine 5.8, Visual Studio mit C++ und **Git LFS**.

```
git lfs install
git clone https://github.com/genben-sb3/sorpasso.git
```

Dann `GTO.uproject` öffnen (die C++-Module werden beim ersten Start gebaut).

## Credits

Fremde Assets mit Quelle und Lizenz stehen in [`CREDITS.md`](CREDITS.md).
