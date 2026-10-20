# SORPASSO (GTO)

## Build

Engine: Launcher-Build UE 5.8 unter `C:\Program Files\Epic Games\UE_5.8`. Bauen (PowerShell, im Repo-Root):

```
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" GTOEditor Win64 Development "-Project=$PWD\GTO.uproject" -WaitMutex
```

Der Editor muss dafür geschlossen sein (Live Coding sperrt den Build). Vorher fragen, bevor der Editor geschlossen wird.

## Tests

Unreal Automation Tests im Namensraum `GTO.` (Sim-Kern: `GTO.Sim.`), headless nach dem Build (PowerShell, im Repo-Root):

```
& "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$PWD\GTO.uproject" -ExecCmds="Automation RunTests GTO.;Quit" -unattended -nullrhi -nosplash -nosound -nopause -stdout -FullStdOutLogOutput
```

Exit-Code 0 heißt alles grün, sonst 255. Pro Test steht im Log eine Zeile `Test Completed. Result={Success|Fail}`, Fehlschläge zusätzlich als `LogAutomationController: Error:`. Für einen Teil der Tests den Filter enger fassen, z. B. `GTO.Sim.Suspension`.

`GTO.Smoke.` spielt die Testmap `/Game/Maps/FlatTest` per PIE und prüft das Zusammenspiel von Adapter und Chaos. Weitere Konsolenbefehle vor dem Testlauf mit Komma trennen (`-ExecCmds="t.MaxFPS 30,Automation RunTests GTO.;Quit"`); das Semikolon gehört zum Automation-Befehl.

Der Sim-Kern (`Source/GTOSim`) rechnet in SI-Einheiten mit Unreal-Achsen (X vorn, Y rechts, Z oben) und hängt nur von `Core` ab. Szenarien über viele Ticks laufen im Headless-Harness (`GTOHeadlessHarness.h`). Er wird durch das Physikpaket abgelöst (ADR 0009) und fällt weg, sobald das Auto damit in Unreal fährt.

## Physikpaket

Die Fahrphysik liegt im Unreal-Modul `Source/UltraGravity` (C++20, nur Standardbibliothek, SI-Einheiten mit ISO-8855-Achsen: X vorn, Y links, Z oben). Das CMake-Projekt `ultragravity/` baut dieselben Quellen ohne Unreal, dazu Testwelt und doctest-Tests; dafür braucht es keine Unreal-Installation. Bauen und alle Tests (im Repo-Root):

```
cd ultragravity && cmake --workflow --preset check
```

`check` baut mit Sanitizern und `-Werror` in `ultragravity/build-check/`; schneller ohne Sanitizer ist `--preset standard` (Build in `ultragravity/build/`, einzelne Tests mit `ctest --test-dir build -R <Name>`). Neue `.cpp` im Modul in `ultragravity/physics/CMakeLists.txt` eintragen (UBT findet sie von selbst, CMake nicht), neue Testdateien in `ultragravity/tests/CMakeLists.txt`.

Der Pre-Commit-Hook `.githooks/pre-commit` prüft Einheitenkommentare und läuft `check`, aber nur, wenn der Commit Dateien unter `Source/UltraGravity` oder `ultragravity` enthält. Einmal pro Klon aktivieren (im Repo-Root):

```
ln -s ../../.githooks/pre-commit "$(git rev-parse --git-common-dir)/hooks/pre-commit"
```

`core.hooksPath` bleibt unverändert, damit die Git-LFS-Hooks in `.git/hooks` weiterlaufen.

Für Modul und CMake-Projekt gelten die Coding Standards in `CODING_STANDARDS.md`. Code, Dateinamen und Feldnamen der Fahrzeugdatei sind Englisch; Doku, Issues und Commit-Nachrichten Deutsch (ADR 0011).

## Binäre Assets

Testmap und `DA_SportsCar` erzeugt `Scripts/create_flat_test_content.py` (Aufruf steht im Skript). Nach Änderungen am Skript neu ausführen statt die Assets von Hand zu bearbeiten. Fremde Assets kommen mit Quelle und Lizenz in `CREDITS.md`.

## Agent skills

### Issue tracker

Issues live in this repo's GitHub Issues, managed via the `gh` CLI. See `docs/agents/issue-tracker.md`.

### Triage labels

Default vocabulary: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `CONTEXT.md` + `docs/adr/` at the repo root, created lazily. See `docs/agents/domain.md`.
