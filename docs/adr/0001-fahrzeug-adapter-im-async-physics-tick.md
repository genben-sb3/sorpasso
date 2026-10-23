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

**Nachtrag aus #4: Gemittelte Kräfte genügen.** Der Adapter gibt die über 8 Substeps gemittelten Kräfte einmal pro Chaos-Schritt auf den Körper; Impulse pro Substep sind nicht nötig. Im Headless-Harness (`GTO.Sim.Substepping`, gleicher Ablauf wie im Adapter) kommt das Auto ohne Wippen zur Ruhe, trägt die statischen Achslasten auf 0,5 % genau und weicht beim Einfedern höchstens 2 mm von einem durchgehend mit 960 Hz gerechneten Lauf ab; auch den Fall aus 30 cm fangen die Bump-Stops stabil. In Unreal mit echtem Chaos (`GTO.Smoke.CarStandsStill`) steht das Auto nach 3 s mit Restgeschwindigkeiten um 1e-8 m/s, Achslasten 45,0 : 55,0.

**Nachtrag aus #4: Schnappschüsse tragen die Chaos-Pose.** Der Schnappschuss eines Schritts entsteht erst zu Beginn des nächsten Async-Ticks: Radzustände aus dem Sim-Kern, Chassis so, wie Chaos es tatsächlich bewegt hat (Kollisionen eingeschlossen), nicht die lokal vorhergesagte Pose der Substeps. Die Darstellung liegt dadurch einen Schritt (8,3 ms) weiter zurück. Startet der Rad-Cast bereits im Boden (harte Landung), gilt das als Kontakt weit jenseits des Bump-Stops statt als Rad in der Luft.

**Nachtrag aus der ersten Probefahrt: Darstellung nach Zeit, nicht nach Ankünften.** Bild und Auto ruckelten beim Fahren. Die Darstellung zählte die ankommenden Schnappschüsse gegen die Framezeit und schnitt den Rest hart auf einen Schritt ab; kamen die Schritte nicht genau im Takt der Frames an (ein Frame ohne, der nächste mit doppelt so vielen), sprang das Auto um bis zu einen Schritt (bei 180 km/h gut 40 cm). Jetzt folgt die gezeigte Zeit der Framezeit und wird nur sanft (0,5 s) auf zwei Schritte hinter den neuesten Schnappschuss gezogen; dafür hält der Interpolator sechs Schnappschüsse. Das kostet gut einen Schritt (8,3 ms) mehr Verzug und fängt Schritte ab, die einen 60-fps-Frame zu spät kommen (`GTO.Vehicle.Interpolation`). Ob sie in Unreal tatsächlich ungleichmäßig ankommen, zeigt `Log LogGTOVehicleSim VeryVerbose` (Schritte pro Frame).

## Konsequenzen

- **Projekteinstellungen:** `bTickPhysicsAsync=True`, `AsyncFixedTimeStepSize=1/120`, `MaxPhysicsDeltaTime=0.1`. Unter 10 FPS läuft das Spiel bewusst langsamer statt in eine Nachhol-Spirale zu geraten, weil Nachhol-Schritte ebenfalls Game-Thread-Zeit kosten.
- **Ungeklärte Anomalie ohne FPS-Limit:** Ungebremst (≈ 4450 FPS headless) lief die Simulation 2,2-mal zu schnell; mit Limits zwischen 60 und 2000 FPS war sie korrekt. Die Ursache ist offen, ein Spiel mit V-Sync oder FPS-Limit erreicht solche Bildraten aber nicht. Falls es doch auftaucht, ist das die erste Spur.
- **Schlafen am Fahrzeugkörper abschalten:** Ist die Geschwindigkeitsänderung pro Schritt klein, legt Chaos den Körper schlafen. Schon 0,1 m/s² bei 1/120 s (≈ 0,8 mm/s pro Schritt) reichten dafür; ebenso 1 m/s² bei Schritten ab 1/500 s. Kräfte über das Async-Handle wecken ihn nicht, auch `SetObjectState(Dynamic)` pro Tick nicht. Nur eine eigene Sleep Family mit Schwellen-Multiplikator 0 hilft. Ein stehendes Auto, das sanft anfährt, wäre sonst festgefroren.
- **Komponente muss aktiv sein:** Eine Komponente mit Async-Tick braucht `bAutoActivate = true`. Sonst stürzt Chaos mit `check(Component->IsActive())` ab.
- **`UPhysicsThreadLibrary::AddForce` ist in 5.8 nicht aufrufbar** (die Klasse hat kein `public:`). Kräfte und Momente gehen direkt über das Handle aus `GetBodyInstanceAsyncPhysicsTickHandle()`.
