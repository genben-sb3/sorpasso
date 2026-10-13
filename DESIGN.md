# APEXLINE – Design-Entscheidungen

> Arbeitstitel: **APEXLINE** (interner Projektname „GTO“). Markenprüfung vor Veröffentlichung.
> Stand: 2026-10-11, Ergebnis eines Grilling-Interviews. Dieses Dokument ist die Grundlage für `/to-spec`.

## Vision

Ein Gran-Turismo-6-ähnliches Racing-Spiel. Hobbyprojekt, das andere **kostenlos** spielen können.
Ziel: **super viel Spaß** und **fotorealistischer Look**.

Kern des Spiels (Priorität):
1. **Das Fahren selbst**: Physik auf GT6-Niveau, Strecken lernen, Rundenzeiten, Zweikämpfe
2. **Autos sammeln**: Credits verdienen, Garage füllen

## Plattform & Technik

| Thema | Entscheidung |
|---|---|
| Engine | **Unreal Engine 5.8** (festgepinnt) |
| Zielplattform | **Nur Windows** |
| Dev-Hardware | Windows-PC: AMD Ryzen 5 7500F, AMD Radeon RX 7600 (8 GB) |
| Arbeitsweise | Claude Code läuft **auf dem Windows-PC** im Projekt; optional `/remote-control` |
| Versionskontrolle | Git + **Git LFS**, privates GitHub-Repo |
| Editor-Anbindung | Epics **Unreal MCP** (Plugin `ModelContextProtocol` + `AllToolsets`, experimentell, lokal auf `127.0.0.1:8000/mcp`) für Claude Code |
| Code | **Kernlogik in C++** (Physik, KI, Regeln, Speichern); **Blueprints nur für Feintuning und UI** (Blueprints sind binär, Claude kann sie nicht bearbeiten) |

## Fahrphysik

- **Eigenes Fahrzeugmodell in C++**, nicht Chaos Vehicles. Unreal liefert nur Rigid Body + Kollision.
- Aufbau in Stufen: **Reifen (Pacejka Magic Formula) + Fahrwerk/Gewichtsverlagerung** → **Antriebsstrang** (Motor, Getriebe, Differenzial) → **Aerodynamik**.
- **Fester Zeitschritt** (hohe Rechenfrequenz, Substepping).
- **Ausschließlich über Eingabe-Pakete gesteuert** (Lenkung, Gas, Bremse, Gang …) – gleiche Schnittstelle für Spieler, KI, Replays und Ghosts.
- Simulation und Darstellung sind sauber getrennt (Vorbereitung Multiplayer).
- Physik ist für **alle Autos gleich**, die KI bekommt keine Abkürzungen.

## Steuerung

- **Tastatur und Gamepad**, beide vollwertig. Lenkrad ist kein Ziel.
- Zwischen Eingabe und Physik: **Eingabe-Glättung** (weicher Lenkaufbau, geschwindigkeitsabhängige Lenkung).
- **Fahrhilfen zuschaltbar wie in GT6**: ABS, Traktionskontrolle, Stabilitätskontrolle. Die Physik bleibt dabei unverändert.

## Look & Assets

- **Fotorealistisch**: Lumen, PBR, Megascans, gutes Post-Processing.
- **Fiktive Autos**, erkennbar von echten inspiriert (keine echten Marken- oder Modellnamen).
- **Fiktive Strecken im GT-Stil** (vgl. Trial Mountain, Deep Forest).
- Assets nur **kostenlos** und nur mit Lizenz **CC0, CC-BY oder Fab-Standardlizenz** (kein NC, damit ein späterer Verkauf möglich bleibt).
- **`CREDITS.md`** mit Quelle und Lizenz jedes Assets wird laufend gepflegt.
- Lieber wenige, sehr schöne Autos als viele mittelmäßige.
- **Sonniss-GDC-Soundpakete erlaubt** (eigene Royalty-free-Lizenz), in `CREDITS.md` vermerken.
- **Kostenpflichtige Fab-Autos vorerst nein**, Entscheidung nach M2.
- **Kein engine-sim** für Motorsounds, solange die Rechte am gerenderten Audio ungeklärt sind.
- Details zu Lizenzen und Kandidaten: `research/assets.md`.

## Strecken & Umgebung

- Start: **ein fiktiver Rundkurs, 3–4 km**, mit Kurven aller Art.
- **Feste Tageszeit pro Strecke**. Dynamische Tageszeit und Wetter sind vorerst kein Thema.

## KI-Gegner

- **Ca. 10 Gegner** (11 Autos im Feld).
- **Gleiche Physik wie der Spieler**, die KI steuert über Eingabe-Pakete.
- **Fahrerprofile** (Aggressivität, Konstanz, Fehlerquote) → gemischtes Feld: echte Zweikämpfer neben sauberen Linienfahrern.
- **Kein Gummiband-Effekt.** Stärke ist einstellbar.

## Sammeln & Progression

- **Credits** durch Rennplatzierungen, dazu gelegentlich ein **Gewinnwagen**.
- Credits dienen in v1.0 **nur zum Autokauf** (Autohändler).
- **Leistungsklassen über PP-Werte**: Events haben PP-Grenzen.
- Umfang: Meilenstein-Version mit **3 Autos**, v1.0 mit **10–12 Autos**.
- **Upgrades und Tuning erst nach v1.0.**

## Kameras

- Verfolger, Motorhaube, Stoßstange für **alle** Autos.
- **Cockpit nur bei Autos mit Interieur.**

## Schaden

- **Nur optisch**: Kratzer, Schmutz, Abrieb als Material-Ebene an der Aufprallstelle, skaliert mit der Aufprallstärke. Keine Verformung, kein mechanischer Schaden. → **M4**

## Sound & Musik

- **Motorsound**: aufgenommene Samples, über **MetaSounds** nach Drehzahl überblendet. Ein Sample-Set pro Motortyp (R4, V6, V8, Boxer …), das sich mehrere Autos teilen. Quelle z. B. Freesound (CC0/CC-BY).
- Eigene Ebenen für Reifenquietschen, Wind, Getriebe.
- **Musik**: Lounge und Jazz im Menü wie bei GT, im Rennen standardmäßig aus. CC0/CC-BY. → **M4**

## UI

- **GT-Stil**: clean, elegant, große Autofotos.
- Schlankes HUD: Tacho, Gang, Position, Runde, Rundenzeit.
- **UMG + CommonUI**, komplett mit Gamepad und Tastatur bedienbar.
- Sprachen: **Englisch (Basis) + Deutsch**, über Unreals Lokalisierungssystem.

## Speichern

- **Ein lokales Profil mit Autosave** nach jedem Event und jedem Autokauf.
- **Bestzeiten + Ghost pro Strecke und Auto.**

## Multiplayer

- **Später.** Jetzt nur Architektur-Vorbereitung (fester Zeitschritt, Eingabe-Pakete, Trennung von Simulation und Darstellung). **Kein** Netzwerk-Code und keine Replication.
- Auf derselben Grundlage entstehen **Replays und Ghosts** über aufgezeichnete Eingaben.

## Meilensteine

1. **M1 – Fahrgefühl**: 1 Auto, 1 Teststrecke, kein Menü. Einzige Frage: *Macht Runden drehen Spaß?* Erst wenn die Antwort Ja ist, geht es weiter.
2. **M2 – Rennen**: Hauptstrecke (3–4 km), 3 Autos, KI-Feld mit ca. 10 Gegnern, Start, Ziel, Platzierung, Ghost und Replay.
3. **M3 – Spiel**: Garage, Credits, Autohändler, Events mit PP-Grenzen, Speichern, Menüs.
4. **M4 – Politur**: optischer Schaden, Musik, Fotomodus, mehr Autos und Strecken, Sound-Politur.

## Bewusst ausgeschlossen (vorerst)

Echte Automarken und Strecken · Lenkrad-Support · Wetter und Tageszeitwechsel · mechanischer Schaden · Upgrades und Tuning (bis nach v1.0) · Multiplayer-Code · Linux- und Mac-Builds

## Nächste Schritte

1. Auf dem Windows-PC installieren: Unreal Engine 5, Visual Studio 2022 („Game development with C++“), Git, Git LFS, Claude Code.
2. Leeres **C++-Projekt** anlegen, `git init` + `git lfs install`, Unreal-`.gitignore` und `.gitattributes` (LFS für `*.uasset`, `*.umap`, Texturen und Audio), diese `DESIGN.md` hineinkopieren.
3. In Claude Code (Windows): `/setup-matt-pocock-skills` → `/to-spec` für **M1** → `/to-tickets` → `/implement` für jedes Ticket.
