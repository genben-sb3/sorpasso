---
status: accepted
---

# Fahrzeug-Adapter läuft im Async-Physics-Tick der Komponente

Der Unreal-Adapter des Fahrzeugs rechnet seine Substeps in `UActorComponent::AsyncPhysicsTickComponent`, nicht in einem eigenen `TSimCallbackObject` wie Chaos Vehicles. Chaos tickt asynchron mit festem Schritt 1/120 s. Der Spike (#2, Branch `prototype/async-physics-spike`, gemessen mit UE 5.8 am 2026-10-15) hat gezeigt, dass dieser Tick auf dem eingefrorenen Game Thread läuft. Dort sind World-Sweeps und UObject-Zugriffe erlaubt, und Kräfte über das Async-Handle wirken exakt. Das ist für M1 und M2 einfacher als ein Callback auf dem Physik-Thread. Der Preis: Jeder Physikschritt kostet Game-Thread-Zeit. Wird das bei 11 Autos zu teuer, zieht nur der Adapter auf das Callback-Muster um; der Sim-Kern bleibt unberührt.

## Messergebnisse

Headless (`-game -nullrhi`), ein Körper mit 1000 kg ohne Schwerkraft, konstante Beschleunigung über das Async-Handle, ein Kugel-Sweep pro Schritt. Werte aus dem eingeschwungenen 1-s-Fenster.

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
3. **Slowdown bei niedriger Bildrate:** Bestätigt. Pro Frame holt Chaos höchstens `MaxPhysicsDeltaTime` nach, mit dem Standard 1/30 s läuft die Simulation bei 16 FPS nur mit 54 % Geschwindigkeit. Mit 0,1 s bleibt sie bis 10 FPS in Echtzeit.

## Konsequenzen

- **Projekteinstellungen:** `bTickPhysicsAsync=True`, `AsyncFixedTimeStepSize=1/120`, `MaxPhysicsDeltaTime=0.1`. Unter 10 FPS läuft das Spiel bewusst langsamer statt in eine Nachhol-Spirale zu geraten, weil Nachhol-Schritte ebenfalls Game-Thread-Zeit kosten.
- **Bildrate begrenzen:** Ohne Limit (≈ 4450 FPS headless) lief die Simulation 2,2-mal zu schnell, ab 2000 FPS abwärts korrekt. Das Spiel braucht ein FPS-Limit (V-Sync oder `t.MaxFPS`).
- **Schlafen am Fahrzeugkörper abschalten:** Ist die Geschwindigkeitsänderung pro Schritt klein (schon 0,1 m/s² bei 1/120 s), legt Chaos den Körper schlafen. Kräfte über das Async-Handle wecken ihn nicht, auch `SetObjectState(Dynamic)` pro Tick nicht. Nur eine eigene Sleep Family mit Schwellen-Multiplikator 0 hilft. Ein stehendes Auto, das sanft anfährt, wäre sonst festgefroren.
- **Komponente muss aktiv sein:** Eine Komponente mit Async-Tick braucht `bAutoActivate = true`. Sonst stürzt Chaos mit `check(Component->IsActive())` ab.
- **`UPhysicsThreadLibrary::AddForce` ist in 5.8 nicht aufrufbar** (die Klasse hat kein `public:`). Kräfte und Momente gehen direkt über das Handle aus `GetBodyInstanceAsyncPhysicsTickHandle()`.
