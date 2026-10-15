---
status: accepted
---

# Fahrzeug-Adapter läuft im Async-Physics-Tick der Komponente

Der Unreal-Adapter des Fahrzeugs rechnet seine Substeps in `UActorComponent::AsyncPhysicsTickComponent` und nicht in einem eigenen Callback auf dem Physik-Thread, wie ihn Chaos Vehicles nutzt. Chaos tickt asynchron mit festem Schritt 1/120 s. Grund: Laut Spike (#2) läuft dieser Tick auf dem eingefrorenen Game Thread, wo World-Sweeps und UObject-Zugriffe erlaubt sind; das ist für M1 und M2 deutlich einfacher. Der Preis: Jeder Physikschritt kostet Game-Thread-Zeit, und Async-Physik ist in Unreal offiziell experimentell. Wird eins davon zum Problem, zieht nur der Adapter auf einen Physik-Thread-Callback um; der Sim-Kern bleibt unberührt.

## Messergebnisse

Gemessen mit UE 5.8 am 2026-10-15, Prototyp-Commit `5909f36` (Branch `prototype/async-physics-spike`, Messskript `RunSpike.ps1`). Alle Läufe headless (`-game -nullrhi`): ein Körper mit 1000 kg ohne Schwerkraft, konstante Beschleunigung 1 m/s² über das Async-Handle, Schlafen nur im Schlaf-Test abgeschaltet, ein Kugel-Sweep pro Schritt. In jedem Lauf trafen alle Sweeps den Boden an der erwarteten Stelle, und die gemessene Geschwindigkeit stimmte mit a·t überein (außer im Schlaf-Fall unten). Die Bildrate wurde mit `t.MaxFPS` begrenzt, das unter `-nullrhi` nur grob greift (Limit 60 → 46 FPS). Werte aus dem eingeschwungenen 1-s-Fenster.

| Fall | Game-Thread-FPS | Physikschritte/s | Sim-Zeit / Wanduhr |
|---|---|---|---|
| 1/120 s, FPS-Limit 60 | 46 | 120 | 0,995 |
| 1/120 s, FPS-Limit 240 / 500 / 1000 / 2000 | wie Limit | 120 | 0,991 |
| 1/120 s, **ohne FPS-Limit** | ≈ 4450 | 267 | **2,22** |
| 1/120 s, FPS-Limit 20, `MaxPhysicsDeltaTime` 1/30 (Standard) | 16 | 66 | **0,54** |
| 1/120 s, FPS-Limit 20, `MaxPhysicsDeltaTime` 0,1 | 16 | 124 | 1,02 |
| 1/120 s, FPS-Limit 8, `MaxPhysicsDeltaTime` 0,1 | 8 | 93 | 0,77 |
| 1/240 s bis 1/10000 s, FPS-Limit 60 | ≈ 47 | 240 … 9881 | ≈ 0,99 |

Die drei offenen Fragen aus der Spec:

1. **Thread:** Der Async-Tick lief in allen Läufen zu 100 % auf dem Game Thread (`IsInGameThread()`, gleiche Thread-ID). Der Quellcode bestätigt es: Der Tick ist ein `TSimCallbackObject` mit `RunOnFrozenGameThread`. World-Sweeps funktionieren dort und treffen exakt.
2. **Minimum des Festschritts:** Der Code erzwingt keins; der Wert geht unverändert an den Solver. Gemessen stabil bis 1/10000 s. 1/120 s läuft stabil.
3. **Slowdown bei niedriger Bildrate:** Bestätigt. Pro Frame holt Chaos höchstens `MaxPhysicsDeltaTime` nach, mit dem Standard 1/30 s läuft die Simulation bei 16 FPS nur mit 54 % Geschwindigkeit. Mit 0,1 s läuft sie bei 16 FPS in Echtzeit; rechnerisch hält das bis 10 FPS (gemessen bei 8 FPS: 77 %).

Nicht geklärt hat der Spike, ob über 8 Substeps gemittelte Kräfte das Chassis ausreichend genau treiben (Spec #1). Das zeigt sich erst mit echter Federung und gehört zu #4.

## Konsequenzen

- **Projekteinstellungen:** `bTickPhysicsAsync=True`, `AsyncFixedTimeStepSize=1/120`, `MaxPhysicsDeltaTime=0.1`. Unter 10 FPS läuft das Spiel bewusst langsamer statt in eine Nachhol-Spirale zu geraten, weil Nachhol-Schritte ebenfalls Game-Thread-Zeit kosten.
- **Ungeklärte Anomalie ohne FPS-Limit:** Ungebremst (≈ 4450 FPS headless) lief die Simulation 2,2-mal zu schnell; mit Limits zwischen 60 und 2000 FPS war sie korrekt. Die Ursache ist offen, ein Spiel mit V-Sync oder FPS-Limit erreicht solche Bildraten aber nicht. Falls es doch auftaucht, ist das die erste Spur.
- **Schlafen am Fahrzeugkörper abschalten:** Ist die Geschwindigkeitsänderung pro Schritt klein, legt Chaos den Körper schlafen. Schon 0,1 m/s² bei 1/120 s (≈ 0,8 mm/s pro Schritt) reichten dafür; ebenso 1 m/s² bei Schritten ab 1/500 s. Kräfte über das Async-Handle wecken ihn nicht, auch `SetObjectState(Dynamic)` pro Tick nicht. Nur eine eigene Sleep Family mit Schwellen-Multiplikator 0 hilft. Ein stehendes Auto, das sanft anfährt, wäre sonst festgefroren.
- **Komponente muss aktiv sein:** Eine Komponente mit Async-Tick braucht `bAutoActivate = true`. Sonst stürzt Chaos mit `check(Component->IsActive())` ab.
- **`UPhysicsThreadLibrary::AddForce` ist in 5.8 nicht aufrufbar** (die Klasse hat kein `public:`). Kräfte und Momente gehen direkt über das Handle aus `GetBodyInstanceAsyncPhysicsTickHandle()`.
